'use strict';

// Candidate checks shared by selection, preparation, platform preflight, and reporting.
// Loaded through actions/github-script from trusted workflow code on main, not the PR checkout.
// Stale approval dismissal must be enabled in the repository's PR protection.
async function approvedCandidate(github, repo, number) {
    if (!Number.isSafeInteger(number) || number < 1) {
        throw new Error('Invalid PR number');
    }

    // GitHub may still be computing the test merge commit when the notification arrives.
    // Selection returns null for an ineligible candidate instead of authorizing execution.
    let pr;
    for (let attempt = 0; attempt < 5; attempt++) {
        ({ data: pr } = await github.rest.pulls.get({ ...repo, pull_number: number }));
        if (pr.mergeable !== null) break;
        await new Promise(resolve => setTimeout(resolve, 2000));
    }
    if (pr.state !== 'open' || pr.draft || pr.base.ref !== 'main' ||
        pr.base.repo.full_name !== `${repo.owner}/${repo.repo}` || !pr.mergeable) {
        return null;
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
    if (review.reviewDecision !== 'APPROVED' || review.headRefOid !== pr.head.sha) {
        return null;
    }

    // Bind tests to the current main tip and the merge of that base with the reviewed head.
    const { data: main } = await github.rest.git.getRef({ ...repo, ref: 'heads/main' });
    const { data: merge } = await github.rest.git.getCommit({ ...repo, commit_sha: pr.merge_commit_sha });
    if (main.object.sha !== pr.base.sha || merge.parents.length !== 2 ||
        merge.parents[0].sha !== pr.base.sha || merge.parents[1].sha !== pr.head.sha) {
        return null;
    }
    return { pr: number, head: pr.head.sha, base: pr.base.sha, merge: pr.merge_commit_sha };
}

// Execution and reporting must match the dispatched identities, including on job retries.
// Throw on a changed candidate instead of silently adopting newer head/base/merge SHAs.
async function validateCandidate(github, repo, expected) {
    if (!expected || !['head', 'base', 'merge'].every(key => /^[0-9a-f]{40}$/.test(expected[key] || ''))) {
        throw new Error('Invalid candidate SHAs');
    }
    const current = await approvedCandidate(github, repo, expected.pr);
    if (!current || ['head', 'base', 'merge'].some(key => current[key] !== expected[key])) {
        throw new Error('Candidate changed or required approval is missing');
    }
    return current;
}

// Called only in prepare/report, whose jobs serialize writes for the same merge SHA.
// A complete success remains valid when an unchanged candidate is run again.
async function publishStatus(github, repo, sha, state, url) {
    const context = 'approved-pr-tests';
    const statuses = await github.paginate(github.rest.repos.listCommitStatusesForRef, {
        ...repo, ref: sha
    });
    if (statuses.some(status => status.context === context && status.state === 'success')) {
        return;
    }
    await github.rest.repos.createCommitStatus({
        ...repo, sha, context, state, target_url: url,
        description: state === 'success' ? 'All required tests passed' :
            state === 'pending' ? 'Approved candidate tests pending' : 'Required tests did not pass'
    });
}

module.exports = { approvedCandidate, validateCandidate, publishStatus };
