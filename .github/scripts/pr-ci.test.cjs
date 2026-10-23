// Unit and workflow-wiring tests; no credentials or API calls.
// Run locally with: node --test .github/scripts/pr-ci.test.cjs
'use strict';
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { readFileSync } = require('node:fs');
const { approvedCandidate, validateCandidate, publishStatus } = require('./pr-ci.cjs');

const repo = { owner: 'owner', repo: 'sdk' };
const candidate = { pr: 7, head: 'a'.repeat(40), base: 'b'.repeat(40) };
function fixture() {
    const pr = {
        state: 'open', draft: false, mergeable: true, merge_commit_sha: 'c'.repeat(40),
        head: { sha: candidate.head },
        base: { ref: 'main', sha: candidate.base, repo: { full_name: 'owner/sdk' } }
    };
    const review = { headRefOid: candidate.head, reviewDecision: 'APPROVED' };
    const main = { object: { sha: candidate.base } };
    const comparison = { status: 'ahead', merge_base_commit: { sha: candidate.base } };
    const github = {
        rest: {
            pulls: { get: async () => ({ data: pr }) },
            git: { getRef: async () => ({ data: main }) },
            repos: { compareCommitsWithBasehead: async options => {
                assert.equal(options.basehead, `${pr.base.sha}...${pr.head.sha}`);
                return { data: comparison };
            } }
        },
        graphql: async () => ({ repository: { pullRequest: review } })
    };
    return { github, pr, review, main, comparison };
}

test('selects and validates the approved head candidate', async () => {
    const { github } = fixture();
    assert.deepEqual(await approvedCandidate(github, repo, 7), candidate);
    assert.deepEqual(await validateCandidate(github, repo, candidate), candidate);
});

test('merge SHA regeneration or absence does not change the candidate', async () => {
    const { github, pr } = fixture();
    for (const sha of ['d'.repeat(40), null]) {
        pr.merge_commit_sha = sha;
        assert.deepEqual(await validateCandidate(github, repo, candidate), candidate);
    }
});

test('does not select unapproved or mixed review/head state', async () => {
    const { github, review } = fixture();
    for (const decision of ['REVIEW_REQUIRED', 'CHANGES_REQUESTED', null]) {
        review.reviewDecision = decision;
        assert.equal(await approvedCandidate(github, repo, 7), null);
    }
    review.reviewDecision = 'APPROVED';
    review.headRefOid = 'd'.repeat(40);
    assert.equal(await approvedCandidate(github, repo, 7), null);
});

test('rejects a closed, draft, conflicting or wrong-target PR', async () => {
    for (const change of [{ state: 'closed' }, { draft: true }, { mergeable: false }]) {
        const { github, pr } = fixture();
        Object.assign(pr, change);
        assert.equal(await approvedCandidate(github, repo, 7), null);
    }
    const { github, pr } = fixture();
    pr.base.ref = 'other';
    assert.equal(await approvedCandidate(github, repo, 7), null);
    pr.base.ref = 'main';
    pr.base.repo.full_name = 'other/sdk';
    assert.equal(await approvedCandidate(github, repo, 7), null);
});

test('requires current main to be an ancestor of the PR head', async () => {
    const { github, main, comparison } = fixture();
    main.object.sha = 'd'.repeat(40);
    assert.equal(await approvedCandidate(github, repo, 7), null);
    main.object.sha = candidate.base;
    for (const status of ['behind', 'diverged']) {
        comparison.status = status;
        assert.equal(await approvedCandidate(github, repo, 7), null);
    }
    comparison.status = 'ahead';
    comparison.merge_base_commit.sha = 'd'.repeat(40);
    assert.equal(await approvedCandidate(github, repo, 7), null);
});

test('accepts an identical base and head', async () => {
    const { github, pr, review, comparison } = fixture();
    pr.head.sha = review.headRefOid = candidate.base;
    comparison.status = 'identical';
    assert.deepEqual(await approvedCandidate(github, repo, 7), { ...candidate, head: candidate.base });
});

test('rejects main advancing while ancestry is queried', async () => {
    const { github, main, comparison } = fixture();
    github.rest.repos.compareCommitsWithBasehead = async () => {
        main.object.sha = 'd'.repeat(40);
        return { data: comparison };
    };
    assert.equal(await approvedCandidate(github, repo, 7), null);
});

test('retries reject dismissed approval and changed heads or bases', async () => {
    const { github, pr, review, main, comparison } = fixture();
    review.reviewDecision = 'REVIEW_REQUIRED';
    await assert.rejects(validateCandidate(github, repo, candidate), /Candidate changed/);
    review.reviewDecision = 'APPROVED';
    pr.head.sha = review.headRefOid = 'd'.repeat(40);
    await assert.rejects(validateCandidate(github, repo, candidate), /Candidate changed/);
    pr.head.sha = review.headRefOid = candidate.head;
    pr.base.sha = main.object.sha = comparison.merge_base_commit.sha = 'e'.repeat(40);
    await assert.rejects(validateCandidate(github, repo, candidate), /Candidate changed/);
});

test('rejects malformed dispatched identities', async () => {
    const { github } = fixture();
    for (const expected of [null, {}, { ...candidate, head: 'main' }, { ...candidate, base: 'main' }]) {
        await assert.rejects(validateCandidate(github, repo, expected), /Invalid candidate/);
    }
    for (const number of [-1, 0, 1.5, '7']) {
        await assert.rejects(approvedCandidate(github, repo, number), /Invalid PR/);
    }
});

test('ancestry API failures fail closed', async () => {
    const { github } = fixture();
    github.rest.repos.compareCommitsWithBasehead = async () => { throw new Error('API unavailable'); };
    await assert.rejects(approvedCandidate(github, repo, 7), /API unavailable/);
});

function statusFixture(statuses = []) {
    const writes = [];
    const list = async () => statuses;
    const github = {
        rest: { repos: {
            listCommitStatusesForRef: list,
            createCommitStatus: async status => { writes.push(status); }
        } },
        paginate: async (method, options) => {
            assert.equal(method, list);
            assert.equal(options.ref, candidate.head);
            return method();
        }
    };
    return { github, writes };
}

test('publishes pending and completion on the exact head', async () => {
    for (const state of ['pending', 'success', 'failure']) {
        const { github, writes } = statusFixture();
        await publishStatus(github, repo, candidate.head, state, 'https://example.test/run');
        assert.equal(writes.length, 1);
        assert.equal(writes[0].sha, candidate.head);
        assert.equal(writes[0].context, 'approved-pr-tests');
        assert.equal(writes[0].state, state);
    }
});

test('publishes completion after pending or failed tests', async () => {
    for (const previous of ['pending', 'failure']) {
        for (const state of ['success', 'failure']) {
            const { github, writes } = statusFixture([{ context: 'approved-pr-tests', state: previous }]);
            await publishStatus(github, repo, candidate.head, state, 'https://example.test/run');
            assert.equal(writes[0].state, state);
        }
    }
});

test('redundant runs cannot replace complete success with pending or failure', async () => {
    for (const state of ['pending', 'failure', 'success']) {
        const { github, writes } = statusFixture([{ context: 'approved-pr-tests', state: 'success' }]);
        await publishStatus(github, repo, candidate.head, state, 'https://example.test/run');
        assert.equal(writes.length, 0);
    }
});

test('success in another context does not satisfy the required tests', async () => {
    const { github, writes } = statusFixture([{ context: 'other-tests', state: 'success' }]);
    await publishStatus(github, repo, candidate.head, 'failure', 'https://example.test/run');
    assert.equal(writes[0].state, 'failure');
});

test('workflow checks out and reports the head with isolated trusted reporting', () => {
    const workflow = readFileSync(`${__dirname}/../workflows/pr-approved-tests.yml`, 'utf8');
    assert.doesNotMatch(workflow, /candidate\)?\.merge/);
    assert.match(workflow, /ref: \$\{\{ fromJSON\(inputs\.candidate\)\.head \}\}/);
    assert.match(workflow, /EXPECTED_SHA: \$\{\{ fromJSON\(inputs\.candidate\)\.head \}\}/);
    assert.match(workflow, /publishStatus\(github, context\.repo, candidate\.head, 'pending', url\)/);
    assert.match(workflow, /publishStatus\(github, context\.repo, candidate\.head, passed \? 'success' : 'failure', url\)/);
    assert.equal((workflow.match(/group: approved-pr-status-\$\{\{ fromJSON\(inputs\.candidate\)\.head \}\}/g) || []).length, 2);
    const testJob = workflow.split('\n  test:')[1].split('\n  report:')[0];
    assert.doesNotMatch(testJob, /statuses: write/);
    assert.match(testJob, /uses: \.\/\.ci\/\.github\/actions\/sdk-ci/);
    const reportJob = workflow.split('\n  report:')[1];
    assert.match(reportJob, /statuses: write/);
    assert.doesNotMatch(reportJob, /secrets\.|download-artifact|path: work/);
    assert.match(reportJob, /TEST_RESULT: \$\{\{ needs\.test\.result \}\}/);
    // Failed, cancelled and skipped matrices must not establish a new success.
    assert.match(reportJob, /const passed = process\.env\.TEST_RESULT === 'success'/);
    assert.match(reportJob, /if: always\(\)/);
});

test('offline workflow explicitly checks out the PR head and runs these tests', () => {
    const workflow = readFileSync(`${__dirname}/../workflows/cmake-multi-platform.yml`, 'utf8');
    const offline = workflow.split('\n  offline:')[1].split('\n  main:')[0];
    assert.match(offline, /ref: \$\{\{ github\.event\.pull_request\.head\.sha \}\}/);
    assert.match(offline, /node --test \.github\/scripts\/pr-ci\.test\.cjs/);
    assert.doesNotMatch(offline, /secrets\.|environment:|statuses: write/);
});
