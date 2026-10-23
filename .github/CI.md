# CI implementation

## Components

| File | Responsibility |
|---|---|
| [workflows/cmake-multi-platform.yml](workflows/cmake-multi-platform.yml) | Offline PR checks and full main-branch CI |
| [workflows/pr-review-notify.yml](workflows/pr-review-notify.yml) | Notification of submitted, edited, or dismissed reviews |
| [workflows/pr-approved-tests.yml](workflows/pr-approved-tests.yml) | Candidate selection, automatic dispatch, approved execution, and commit-status reporting |
| [actions/sdk-ci/action.yml](actions/sdk-ci/action.yml) | Shared build, core tests, samples, artifacts, and failure handling |
| [scripts/pr-ci.cjs](scripts/pr-ci.cjs) | Assess candidate eligibility and identity, and preserve completed test statuses |

Builds use Release configuration on Linux x64, macOS arm64, and Windows x64.

## Execution paths

### PR opened, updated, or reopened

`cmake-multi-platform.yml` runs the offline matrix for PRs targeting main. It explicitly checks out the PR head SHA and uses the composite action from that checkout. The Linux job also runs the CI helper and workflow-wiring tests.

Each platform builds the SDK and runs `~[online]~[tls]~[.]`. It does not generate subscription configuration or run samples. The workflow supplies no service credentials or status-writing permission.

### Main push, daily schedule, or manual main run

The same workflow runs its main matrix using the `speech-ci` environment. Each platform builds once, runs the combined core selection `~[tls]~[.]`, then builds and runs the six recognition, synthesis, and translation sample scenarios. No PR approval is involved.

### Approved PR selection

`pr-approved-tests.yml` runs `select` on main pushes and after completion of the review notifier or ordinary CI. Completion is a wake-up signal, not evidence of approval or successful tests.

The selector uses PRs associated with the triggering workflow run when available; otherwise it lists open PRs targeting main. Thus a main update also causes approved PRs to be considered against the new base.

`approvedCandidate()` requires an open, non-draft, mergeable PR targeting this repository's main, GitHub's required-review decision `APPROVED`, and agreement between the review query and PR head SHA. It verifies that the base is the current main tip and uses an immutable-SHA commit comparison to require that this base is an ancestor of the head. It checks main again after the comparison to reject a base that advanced during that request.

Each eligible candidate is automatically dispatched to the same workflow as `{ pr, head, base }`. Maintainers do not perform a separate dispatch or environment approval. Selection skips heads that already have a complete `approved-pr-tests` success. Pending or failed results do not suppress dispatch: they may belong to interrupted runs. Concurrent in-flight candidates are not deduplicated, so repeated notifications can still start redundant runs.

### Approved candidate execution

Main-ref workflow_dispatch events enter the following path; the selector invokes this dispatch automatically:

```text
prepare -> test-Linux / test-macOS / test-Windows -> report
```

1. **prepare:** Assesses the candidate. Obsolete candidates and heads with an existing complete success are logged and skipped through an explicit job output; no test matrix or reporter is started. Otherwise it publishes a pending commit status named `approved-pr-tests` on the selected PR head SHA using its automatic `GITHUB_TOKEN`.
2. **test:** Each platform revalidates approval and candidate identity before checking out PR code, then verifies the checkout SHA. If preflight rejects the candidate, all PR-code steps are skipped and the platform job explicitly fails: skipped tests must not count as passed if eligibility returns before reporting. Trusted helper/action code comes from the workflow revision in `.ci`; the reviewed PR head is checked out into `work`. Each platform builds once and runs combined core tests and all six samples. These jobs receive service credentials, but no status-writing permission.
3. **report:** A separate runner assesses the candidate immediately before publication. For an eligible candidate it publishes success or failure for the same status context and PR head SHA, based on the matrix result. For an obsolete candidate it logs the reason and publishes nothing, without failing merely because the PR has merged, closed, changed head/base, or lost approval. It does not execute PR code or consume PR build artifacts. For an eligible candidate, a non-success matrix result also fails the reporting job.

## Updates, retries, and results

A PR-head update must satisfy the required review policy again and receives its own test results. If main advances beyond the PR branch, update the branch by merging or rebasing main before it is eligible for approved testing and merging. An in-flight run rejects changed head/base identities rather than certifying a new candidate. Preparation, every platform job, and reporting repeat candidate assessment, including on retries; none silently switch revisions. Preparation and reporting treat expected lifecycle changes as explicit no-op outcomes, never as test success. Platform preflight rejection remains a failure so that an unexecuted build cannot certify a subsequently eligible candidate. Malformed inputs, API errors, and unexpected responses remain visible failures, not lifecycle skips.

Failed-job retries use Actions dependencies and `needs.test.result`, not custom job-history aggregation. The platform matrix has fail-fast disabled, and an eligible candidate's reporter fails when the matrix is not successful so that it participates in failed-job retries.

Preparation and reporting serialize status writes for the same head SHA. A complete success for that immutable source is preserved during redundant runs. Strict up-to-date branch protection independently prevents merging a stale head; a branch update produces a new head requiring its own tests. Missing reporting cannot establish a new success; a pending status remains pending until a result is published.

Core tests and samples use continue-on-error to allow failure-log collection and artifact upload, followed by an explicit failing step if either failed. Logs are included only after test/sample failure; SDK binaries and headers are uploaded independently of that condition.

## Credentials and required configuration

- `speech-ci` provides `SPEECH_SERVICE_REGION` and `SPEECH_SUBSCRIPTION_KEY` to main CI and approved platform jobs.
- Restrict `speech-ci` to main workflow refs, without required environment reviewers. Keep equivalent credentials out of repository-level secrets or other paths available to unapproved PR workflows.
- Preparation/reporting jobs use their automatic `GITHUB_TOKEN` with `statuses: write`. Jobs executing PR code do not have that permission. Selection separately uses `actions: write` for automatic dispatch.
- Require the commit status `approved-pr-tests`, offline PR checks, and the existing maintainer PR approval. Enable stale-approval dismissal, require approval of the most recent reviewable push, and enable strict **Require branches to be up to date before merging**. This is mandatory: the tests run the head, not a synthetic merge with a newer base. The required result is the API-published commit status, not the dispatched workflow's native job check. The helper relies on GitHub's required-review decision rather than reconstructing a reviewer policy.

Maintainer approval authorizes PR test execution with service credentials. Branch protection/rulesets must enforce the required status.

## Validation

Run `node --test .github/scripts/pr-ci.test.cjs` for mocked helper tests, execution of the actual trusted `github-script` bodies with mocked bindings, and static workflow-wiring assertions. These do not prove GitHub's live approval, required-status, or merge behavior.

When changing CI workflows or repository protection settings, validate the following behavior using an external-fork PR with all required checks enforced:

1. Unapproved code cannot start credential-bearing tests. GitHub's fork-workflow approval is separate from the PR-review approval queried by this workflow.
2. Maintainer PR approval automatically starts tests of the exact head, without a separate dispatch or environment approval.
3. A new head cannot inherit results from the old head. Dismissed approval prevents a new eligible execution; it cannot revoke credentials already exposed to a running job.
4. Advancing main blocks a stale branch. Updating that branch and satisfying the review policy permits tests of the new head.
5. A failed or cancelled matrix cannot establish a new success. Failed-job retries can recover the same candidate.
6. Temporary merge SHA regeneration leaves the head result usable, and the PR can actually be merged with protection enforced.
7. A PR that merges or closes while redundant tests run causes reporting to log a skip without changing statuses. A platform rejected before execution cannot count as passed if eligibility returns before reporting.

Approved runs use trusted workflow code from main. Validate changes to that code after it reaches main; testing a PR checkout alone does not exercise the trusted execution path.

Approval deliberately authorizes contributor-controlled build/test code to use service credentials; masking and step-scoped environment variables do not prevent that code from exfiltrating them. Use a dedicated, limited test resource and keep privileged reporting on its separate runner.
