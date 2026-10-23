'use strict';

// Candidate checks shared by selection, preparation, platform preflight, and reporting.
// Loaded through actions/github-script from trusted workflow code on main, not the PR checkout.
// Stale approval dismissal and strict up-to-date branches must be enabled in PR protection.
const ineligible = reason => ({ eligible: false, reason });
const isSha = value => typeof value === 'string' && /^[0-9a-f]{40}$/.test(value);

// Expected lifecycle transitions are data; API and malformed-response errors propagate.
async function resolveCandidate(github, repo, number, expected) {
    if (!Number.isSafeInteger(number) || number < 1) {
        throw new Error('Invalid PR number');
    }

    // GitHub may still be computing the test merge commit when the notification arrives.
    // Selection returns null for an ineligible candidate instead of authorizing execution.
    let pr;
    for (let attempt = 0; attempt < 5; attempt++) {
        ({ data: pr } = await github.rest.pulls.get({ ...repo, pull_number: number }));
        if (!['open', 'closed'].includes(pr.state)) throw new Error('Unexpected PR state');
        if (pr.state === 'closed') return ineligible(pr.merged ? 'merged' : 'closed');
        if (!isSha(pr.head.sha) || !isSha(pr.base.sha) || typeof pr.draft !== 'boolean' ||
            ![true, false, null].includes(pr.mergeable)) throw new Error('Unexpected PR response');
        if (expected && pr.head.sha !== expected.head) return ineligible('head-changed');
        if (expected && pr.base.sha !== expected.base) return ineligible('base-changed');
        if (pr.mergeable !== null) break;
        await new Promise(resolve => setTimeout(resolve, 2000));
    }
    if (pr.state !== 'open' || pr.draft || pr.base.ref !== 'main' ||
        pr.base.repo.full_name !== `${repo.owner}/${repo.repo}` || !pr.mergeable) {
        return ineligible('not-ready');
    }

    // Use the repository's required-review decision, not a separate reviewer permission policy.
    // Match the head across queries so a concurrent PR update cannot select mixed identities.
    const { repository } = await github.graphql(`
        query($owner: String!, $repo: String!, $number: Int!) {
            repository(owner: $owner, name: $repo) {
                pullRequest(number: $number) { headRefOid reviewDecision }
            }
        }
    `, { ...repo, number });
    const review = repository.pullRequest;
    if (!review || !isSha(review.headRefOid) ||
        !['APPROVED', 'REVIEW_REQUIRED', 'CHANGES_REQUESTED', null].includes(review.reviewDecision)) {
        throw new Error('Unexpected review response');
    }
    if (review.headRefOid !== pr.head.sha) return ineligible('head-changed');
    if (review.reviewDecision !== 'APPROVED') return ineligible('approval-missing');

    // Test the reviewed head itself, only when it already contains current main.
    // GitHub can regenerate its temporary merge commit without changing source content.
    const { data: main } = await github.rest.git.getRef({ ...repo, ref: 'heads/main' });
    if (!isSha(main.object.sha)) throw new Error('Unexpected main ref');
    if (main.object.sha !== pr.base.sha) return ineligible('base-changed');
    const { data: comparison } = await github.rest.repos.compareCommitsWithBasehead({
        ...repo, basehead: `${pr.base.sha}...${pr.head.sha}`
    });
    if (!['ahead', 'identical', 'behind', 'diverged'].includes(comparison.status) ||
        !isSha(comparison.merge_base_commit.sha)) throw new Error('Unexpected comparison response');
    if (!['ahead', 'identical'].includes(comparison.status) ||
        comparison.merge_base_commit.sha !== pr.base.sha) {
        return ineligible('base-not-contained');
    }
    // Do not select a stale base if main advanced during the comparison request.
    const { data: latestMain } = await github.rest.git.getRef({ ...repo, ref: 'heads/main' });
    if (!isSha(latestMain.object.sha)) throw new Error('Unexpected main ref');
    if (latestMain.object.sha !== pr.base.sha) return ineligible('base-changed');
    return { eligible: true, candidate: { pr: number, head: pr.head.sha, base: pr.base.sha } };
}

async function approvedCandidate(github, repo, number) {
    const assessment = await resolveCandidate(github, repo, number);
    return assessment.eligible ? assessment.candidate : null;
}

async function assessCandidate(github, repo, expected) {
    if (!expected || !['head', 'base'].every(key => isSha(expected[key]))) {
        throw new Error('Invalid candidate SHAs');
    }
    return resolveCandidate(github, repo, expected.pr, expected);
}

// Strict interface for callers that require an eligible candidate rather than a skip.
// No caller may silently adopt newer head/base SHAs, including on retries.
async function validateCandidate(github, repo, expected) {
    const assessment = await assessCandidate(github, repo, expected);
    if (!assessment.eligible) {
        throw new Error(`Candidate changed or required approval is missing: ${assessment.reason}`);
    }
    return assessment.candidate;
}

async function hasSuccessfulTests(github, repo, sha) {
    const statuses = await github.paginate(github.rest.repos.listCommitStatusesForRef, {
        ...repo, ref: sha
    });
    return statuses.some(status => status.context === 'approved-pr-tests' && status.state === 'success');
}

// Called only in prepare/report, whose jobs serialize writes for the same head SHA.
// A complete success for that immutable source remains valid during redundant runs.
// Strict branch protection independently blocks a head that no longer contains main.
async function publishStatus(github, repo, sha, state, url) {
    const context = 'approved-pr-tests';
    if (await hasSuccessfulTests(github, repo, sha)) {
        return;
    }
    await github.rest.repos.createCommitStatus({
        ...repo, sha, context, state, target_url: url,
        description: state === 'success' ? 'All required tests passed' :
            state === 'pending' ? 'Approved candidate tests pending' : 'Required tests did not pass'
    });
}

module.exports = { approvedCandidate, assessCandidate, validateCandidate, hasSuccessfulTests, publishStatus };
