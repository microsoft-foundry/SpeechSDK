// Unit and workflow-wiring tests; no credentials or API calls.
// Run locally with: node --test .github/scripts/pr-ci.test.cjs
'use strict';
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { readFileSync } = require('node:fs');
const { approvedCandidate, validateCandidate, assessCandidate, hasSuccessfulTests, publishStatus } = require('./pr-ci.cjs');
const { runInNewContext } = require('node:vm');

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
    assert.match(testJob, /if: needs\.prepare\.outputs\.run == 'true'/);
    assert.equal((testJob.match(/if: steps\.candidate\.outputs\.eligible == 'true'/g) || []).length, 3);
    assert.match(workflow, /run: \$\{\{ steps\.candidate\.outputs\.run \}\}/);
    assert.match(workflow, /if: steps\.candidate\.outputs\.run == 'true'/);
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

// Execute the actual trusted github-script bodies with mocked GitHub/core bindings.
function workflowScripts() {
    const workflow = readFileSync(`${__dirname}/../workflows/pr-approved-tests.yml`, 'utf8');
    return [...workflow.matchAll(/          script: \|\r?\n((?:            [^\r\n]*\r?\n|\r?\n)+)/g)]
        .map(match => match[1].replace(/^            /gm, ''));
}
async function executeScript(script, github, result = 'success') {
    const outputs = {};
    const messages = [];
    const failures = [];
    await runInNewContext(`(async () => { ${script}\n })()`, {
        github, context: { repo, serverUrl: 'https://example.test', runId: 42 },
        process: { env: { CANDIDATE: JSON.stringify(candidate), TEST_RESULT: result } },
        require: () => require('./pr-ci.cjs'),
        core: {
            setOutput: (key, value) => { outputs[key] = value; },
            info: message => { messages.push(message); },
            setFailed: message => { failures.push(message); }
        }
    });
    return { outputs, messages, failures };
}

test('assessment classifies lifecycle changes without authorizing them', async () => {
    const { github, pr, review, main } = fixture();
    assert.equal((await assessCandidate(github, repo, candidate)).eligible, true);
    pr.state = 'closed';
    pr.merged = true;
    pr.mergeable = null;
    assert.equal((await assessCandidate(github, repo, candidate)).reason, 'merged');
    pr.merged = false;
    assert.equal((await assessCandidate(github, repo, candidate)).reason, 'closed');
    pr.state = 'open';
    pr.mergeable = true;
    pr.head.sha = 'd'.repeat(40);
    assert.equal((await assessCandidate(github, repo, candidate)).reason, 'head-changed');
    pr.head.sha = candidate.head;
    main.object.sha = 'd'.repeat(40);
    assert.equal((await assessCandidate(github, repo, candidate)).reason, 'base-changed');
    main.object.sha = candidate.base;
    review.reviewDecision = 'REVIEW_REQUIRED';
    assert.equal((await assessCandidate(github, repo, candidate)).reason, 'approval-missing');
});

test('assessment throws for malformed inputs, API errors and unexpected responses', async () => {
    const { github, pr } = fixture();
    await assert.rejects(assessCandidate(github, repo, { ...candidate, head: 'main' }), /Invalid candidate/);
    pr.state = 'unexpected';
    await assert.rejects(assessCandidate(github, repo, candidate), /Unexpected/);
    pr.state = 'open';
    pr.head.sha = 'main';
    await assert.rejects(assessCandidate(github, repo, candidate), /Unexpected/);
    pr.head.sha = candidate.head;
    github.rest.pulls.get = async () => { throw new Error('API unavailable'); };
    await assert.rejects(assessCandidate(github, repo, candidate), /API unavailable/);
});

test('completed-success lookup does not mistake pending, failure or another context for success', async () => {
    for (const state of ['pending', 'failure', 'error']) {
        const { github } = statusFixture([{ context: 'approved-pr-tests', state }]);
        assert.equal(await hasSuccessfulTests(github, repo, candidate.head), false);
    }
    assert.equal(await hasSuccessfulTests(statusFixture([{ context: 'other', state: 'success' }]).github, repo, candidate.head), false);
    assert.equal(await hasSuccessfulTests(statusFixture([{ context: 'approved-pr-tests', state: 'success' }]).github, repo, candidate.head), true);
});

test('actual preparation script skips obsolete and completed candidates; gates live candidates', async () => {
    const script = workflowScripts().find(s => s.includes("setOutput('run'"));
    assert.ok(script, 'preparation must expose an explicit run output');
    const { github, pr } = fixture();
    Object.assign(github.rest.repos, statusFixture().github.rest.repos);
    github.paginate = async method => method();
    assert.equal((await executeScript(script, github)).outputs.run, 'true');
    pr.state = 'closed';
    assert.notEqual((await executeScript(script, github)).outputs.run, 'true');
    pr.state = 'open';
    github.paginate = async () => [{ context: 'approved-pr-tests', state: 'success' }];
    assert.notEqual((await executeScript(script, github)).outputs.run, 'true');
});

test('actual platform preflight skips PR code after approval dismissal', async () => {
    const script = workflowScripts().find(s => s.includes("setOutput('eligible'"));
    assert.ok(script);
    const { github, review } = fixture();
    assert.equal((await executeScript(script, github)).outputs.eligible, 'true');
    review.reviewDecision = 'REVIEW_REQUIRED';
    const skipped = await executeScript(script, github);
    assert.equal(skipped.outputs.eligible, 'false');
    assert.equal(skipped.failures.length, 1, 'a skipped build cannot look like passed tests');
    github.graphql = async () => { throw new Error('API unavailable'); };
    await assert.rejects(executeScript(script, github), /API unavailable/);
});

test('actual reporter leaves status untouched after merge, close, head/base change or dismissed approval', async () => {
    const script = workflowScripts().find(s => s.includes('TEST_RESULT'));
    assert.ok(script);
    for (const change of ['merged', 'closed', 'head', 'base', 'approval']) {
        const { github, pr, main, review } = fixture();
        const status = statusFixture();
        Object.assign(github.rest.repos, status.github.rest.repos);
        github.paginate = status.github.paginate;
        if (change === 'merged' || change === 'closed') {
            pr.state = 'closed'; pr.merged = change === 'merged';
        }
        if (change === 'head') pr.head.sha = 'd'.repeat(40);
        if (change === 'base') main.object.sha = 'd'.repeat(40);
        if (change === 'approval') review.reviewDecision = 'REVIEW_REQUIRED';
        const outcome = await executeScript(script, github);
        assert.equal(status.writes.length, 0, change);
        assert.equal(outcome.failures.length, 0, change);
        assert.ok(outcome.messages.length, change);
    }
});

test('actual reporter reports eligible failures/cancellations and propagates API errors', async () => {
    const script = workflowScripts().find(s => s.includes('TEST_RESULT'));
    for (const result of ['success', 'failure', 'cancelled', 'skipped']) {
        const { github } = fixture();
        const status = statusFixture();
        Object.assign(github.rest.repos, status.github.rest.repos);
        github.paginate = status.github.paginate;
        const outcome = await executeScript(script, github, result);
        assert.equal(status.writes[0].state, result === 'success' ? 'success' : 'failure');
        assert.equal(outcome.failures.length, result === 'success' ? 0 : 1);
    }
    const { github } = fixture();
    github.rest.pulls.get = async () => { throw new Error('API unavailable'); };
    await assert.rejects(executeScript(script, github), /API unavailable/);
});

test('approval restored after a rejected platform cannot certify skipped tests', async () => {
    const scripts = workflowScripts();
    const { github, review } = fixture();
    const status = statusFixture();
    Object.assign(github.rest.repos, status.github.rest.repos);
    github.paginate = status.github.paginate;
    review.reviewDecision = 'REVIEW_REQUIRED';
    const platform = await executeScript(scripts.find(s => s.includes("setOutput('eligible'")), github);
    assert.equal(platform.failures.length, 1);
    review.reviewDecision = 'APPROVED';
    const report = await executeScript(scripts.find(s => s.includes('TEST_RESULT')), github, 'failure');
    assert.equal(status.writes[0].state, 'failure');
    assert.equal(report.failures.length, 1);
});

test('actual selector deduplicates completed success but not stale pending or failed runs', async () => {
    const script = workflowScripts()[0];
    for (const state of ['success', 'pending', 'failure', null]) {
        const { github } = fixture();
        let dispatched = 0;
        github.rest.actions = { createWorkflowDispatch: async () => { dispatched++; } };
        github.rest.repos.listCommitStatusesForRef = async () => state ? [{ context: 'approved-pr-tests', state }] : [];
        github.paginate = async method => method();
        await runInNewContext(`(async () => { ${script}\n })()`, {
            github, context: { repo, payload: { workflow_run: { pull_requests: [{ number: 7 }] } } },
            require: () => require('./pr-ci.cjs')
        });
        assert.equal(dispatched, state === 'success' ? 0 : 1);
    }
});
