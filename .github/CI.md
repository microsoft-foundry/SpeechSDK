# CI implementation

## Components

| File | Responsibility |
|---|---|
| [workflows/cmake-multi-platform.yml](workflows/cmake-multi-platform.yml) | Offline PR checks and full main-branch CI |
| [workflows/pr-review-notify.yml](workflows/pr-review-notify.yml) | Notification of submitted, edited, or dismissed reviews |
| [workflows/pr-approved-tests.yml](workflows/pr-approved-tests.yml) | Candidate selection, automatic dispatch, approved execution, and commit-status reporting |
| [actions/sdk-ci/action.yml](actions/sdk-ci/action.yml) | Shared build, core tests, samples, artifacts, and failure handling |
| [scripts/pr-ci.cjs](scripts/pr-ci.cjs) | Resolve an approved candidate and validate its identity |

Builds use Release configuration on Linux x64, macOS arm64, and Windows x64.

## Execution paths

### PR opened, updated, or reopened

`cmake-multi-platform.yml` runs the offline matrix for PRs targeting main. It checks out the revision selected by the pull_request event and uses the composite action from that checkout.

Each platform builds the SDK and runs `~[online]~[tls]~[.]`. It does not generate subscription configuration or run samples. The workflow supplies no service credentials or status-writing permission.

### Main push, daily schedule, or manual main run

The same workflow runs its main matrix using the `speech-ci` environment. Each platform builds once, runs the combined core selection `~[tls]~[.]`, then builds and runs the six recognition, synthesis, and translation sample scenarios. No PR approval is involved.

### Approved PR selection

`pr-approved-tests.yml` runs `select` on main pushes and after completion of the review notifier or ordinary CI. Completion is a wake-up signal, not evidence of approval or successful tests.

The selector uses PRs associated with the triggering workflow run when available; otherwise it lists open PRs targeting main. Thus a main update also causes approved PRs to be considered against the new base.

`approvedCandidate()` requires an open, non-draft, mergeable PR targeting this repository's main, GitHub's required-review decision `APPROVED`, and agreement between the review query and PR head SHA. It verifies that the base is the current main tip and that the merge commit's parents match that base and head.

Each eligible candidate is automatically dispatched to the same workflow as `{ pr, head, base, merge }`. Maintainers do not perform a separate dispatch or environment approval. Selection does not deduplicate dispatches, so repeated notifications can start redundant runs.

### Approved candidate execution

Main-ref workflow_dispatch events enter the following path; the selector invokes this dispatch automatically:

```text
prepare -> test-Linux / test-macOS / test-Windows -> report
```

1. **prepare:** Revalidates the candidate and publishes a pending commit status named `approved-pr-tests` on the selected merge SHA using its automatic `GITHUB_TOKEN`. An existing complete success for that SHA is preserved.
2. **test:** Each platform revalidates approval and candidate identity before checking out PR code, then verifies the checkout SHA. Trusted helper/action code comes from the workflow revision in `.ci`; the merge candidate is checked out into `work`. Each platform builds once and runs combined core tests and all six samples. These jobs receive service credentials, but no status-writing permission.
3. **report:** A separate runner revalidates the candidate and publishes success or failure for the same status context and merge SHA, based on the matrix result. It does not execute PR code or consume PR build artifacts. A non-success matrix result also fails the reporting job.

## Updates, retries, and results

A PR-head update requires renewed approval. A base update requires tests for the new merge candidate while approval remains valid. Preparation, every platform job, and reporting repeat candidate validation, including on retries; they fail rather than silently switch revisions.

Failed-job retries use Actions dependencies and `needs.test.result`, not custom job-history aggregation. The platform matrix has fail-fast disabled, and the reporter fails when the matrix is not successful so that it participates in failed-job retries.

Preparation and reporting serialize status writes for the same merge SHA. A complete success for that candidate is preserved during redundant runs. Missing reporting cannot establish a new success; a pending status remains pending until a result is published.

Core tests and samples use continue-on-error to allow failure-log collection and artifact upload, followed by an explicit failing step if either failed. Logs are included only after test/sample failure; SDK binaries and headers are uploaded independently of that condition.

## Credentials and required configuration

- `speech-ci` provides `SPEECH_SERVICE_REGION` and `SPEECH_SUBSCRIPTION_KEY` to main CI and approved platform jobs.
- Restrict `speech-ci` to main workflow refs, without required environment reviewers. Keep equivalent credentials out of repository-level secrets or other paths available to unapproved PR workflows.
- Preparation/reporting jobs use their automatic `GITHUB_TOKEN` with `statuses: write`. Jobs executing PR code do not have that permission. Selection separately uses `actions: write` for automatic dispatch.
- Require the commit status `approved-pr-tests`, offline PR checks, and the existing maintainer PR approval. Enable stale-approval dismissal and require results for the current merge candidate. The required result is the API-published commit status, not the dispatched workflow's native job check. The helper relies on GitHub's required-review decision rather than reconstructing a reviewer policy.

Maintainer approval authorizes PR test execution with service credentials. Branch protection/rulesets must enforce the required status.
