// Manual unit tests for pr-ci.cjs using simulated GitHub responses; no credentials or API calls.
// Run locally with: node --test .github/scripts/pr-ci.test.cjs
'use strict';
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { approvedCandidate, validateCandidate, publishStatus } = require('./pr-ci.cjs');

const repo = { owner: 'owner', repo: 'sdk' };
const candidate = { pr: 7, head: 'a'.repeat(40), base: 'b'.repeat(40), merge: 'c'.repeat(40) };

function fixture() {
    const pr = {
        state: 'open', draft: false, mergeable: true, merge_commit_sha: candidate.merge,
        head: { sha: candidate.head },
        base: { ref: 'main', sha: candidate.base, repo: { full_name: 'owner/sdk' } }
    };
    const review = { headRefOid: candidate.head, reviewDecision: 'APPROVED' };
    const main = { object: { sha: candidate.base } };
    const merge = { parents: [{ sha: candidate.base }, { sha: candidate.head }] };
    const github = {
        rest: {
            pulls: { get: async () => ({ data: pr }) },
            git: {
                getRef: async () => ({ data: main }),
                getCommit: async () => ({ data: merge })
            }
        },
        graphql: async () => ({ repository: { pullRequest: review } })
    };
    return { github, pr, review, main, merge };
}

test('selects and validates the approved merge candidate', async () => {
    const { github } = fixture();
    assert.deepEqual(await approvedCandidate(github, repo, 7), candidate);
    assert.deepEqual(await validateCandidate(github, repo, candidate), candidate);
});

test('does not select unapproved or changed review state', async () => {
    const { github, review } = fixture();
    review.reviewDecision = 'REVIEW_REQUIRED';
    assert.equal(await approvedCandidate(github, repo, 7), null);
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
});

test('rejects a changed base or mismatched merge parents', async () => {
    const { github, main, merge } = fixture();
    main.object.sha = 'd'.repeat(40);
    assert.equal(await approvedCandidate(github, repo, 7), null);
    main.object.sha = candidate.base;
    merge.parents[1].sha = 'd'.repeat(40);
    assert.equal(await approvedCandidate(github, repo, 7), null);
});

test('refuses to replace the dispatched candidate with a different one', async () => {
    const { github } = fixture();
    await assert.rejects(validateCandidate(github, repo, { ...candidate, head: 'd'.repeat(40) }), /Candidate changed/);
    await assert.rejects(validateCandidate(github, repo, { ...candidate, merge: 'main' }), /Invalid candidate/);
    await assert.rejects(approvedCandidate(github, repo, -1), /Invalid PR/);
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
            assert.equal(options.ref, candidate.merge);
            return method();
        }
    };
    return { github, writes };
}

test('publishes pending on the exact merge candidate', async () => {
    const { github, writes } = statusFixture();
    await publishStatus(github, repo, candidate.merge, 'pending', 'https://example.test/run');
    assert.equal(writes.length, 1);
    assert.equal(writes[0].sha, candidate.merge);
    assert.equal(writes[0].context, 'approved-pr-tests');
    assert.equal(writes[0].state, 'pending');
});

test('publishes completion after pending or failed tests', async () => {
    for (const state of ['success', 'failure']) {
        const { github, writes } = statusFixture([{ context: 'approved-pr-tests', state: 'pending' }]);
        await publishStatus(github, repo, candidate.merge, state, 'https://example.test/run');
        assert.equal(writes[0].state, state);
    }
});

test('redundant runs cannot replace complete success with pending or failure', async () => {
    for (const state of ['pending', 'failure', 'success']) {
        const { github, writes } = statusFixture([{ context: 'approved-pr-tests', state: 'success' }]);
        await publishStatus(github, repo, candidate.merge, state, 'https://example.test/run');
        assert.equal(writes.length, 0);
    }
});

test('success in another status context does not satisfy the required tests', async () => {
    const { github, writes } = statusFixture([{ context: 'other-tests', state: 'success' }]);
    await publishStatus(github, repo, candidate.merge, 'failure', 'https://example.test/run');
    assert.equal(writes[0].state, 'failure');
});
