#!/usr/bin/env bash
#
# Dispatch a workflow_dispatch workflow, find the run it created, and wait for it to finish.
# Used by release.yml to orchestrate the four publish workflows. Dispatch (not workflow_call)
# is load-bearing: npm and PyPI trusted publishing validate the *top-level* workflow filename
# from the OIDC claims, so publish-js.yml / publish-python.yml must run as their own top-level
# workflows or registry auth breaks (and npm allows only one trusted publisher per package).
#
# Run correlation: `gh workflow run` returns nothing, so the created run is found by listing
# runs of that workflow on that ref created after a pre-dispatch timestamp. release.yml's
# `release` concurrency group serializes orchestrators, and the bindings are dispatched on a
# fresh per-release tag, so the newest matching run is unambiguous.
#
# Waiting is a plain status poll, not `gh run watch` (hang report inside runners, cli/cli#8194,
# plus spinner noise in CI logs). The caller's job-level timeout-minutes is the time bound.
#
# Requires: GH_TOKEN with actions:write (GITHUB_TOKEN works — workflow_dispatch is an explicit
# exception to "GITHUB_TOKEN events don't trigger workflows"). Respects GH_REPO if set.
#
# Usage:
#   scripts/ci/dispatch_workflow.sh <workflow-file> <ref> [key=value ...]
# Extra key=value args are passed as -f inputs; pass only inputs the workflow declares
# (undeclared inputs are a hard 422 from the dispatch API).

set -euo pipefail

if [ $# -lt 2 ]; then
  echo "usage: dispatch_workflow.sh <workflow-file> <ref> [key=value ...]" >&2
  exit 2
fi

workflow="$1"
ref="$2"
shift 2

fields=()
for kv in "$@"; do
  fields+=(-f "$kv")
done

# Timestamp before dispatching, with a 60s skew buffer: only needs to exclude stale runs.
since="$(date -u -d '60 seconds ago' +%Y-%m-%dT%H:%M:%SZ)"

echo "Dispatching ${workflow} on ${ref}${*:+ with inputs: $*}"
dispatched=0
for attempt in 1 2 3; do
  if gh workflow run "$workflow" --ref "$ref" ${fields[@]+"${fields[@]}"}; then
    dispatched=1
    break
  fi
  echo "dispatch attempt ${attempt} failed; retrying in 5s" >&2
  sleep 5
done
if [ "$dispatched" -ne 1 ]; then
  echo "::error::dispatch_workflow.sh: failed to dispatch ${workflow} on ${ref}" >&2
  exit 1
fi

run_id=""
for _ in $(seq 1 12); do
  run_id="$(gh run list --workflow "$workflow" --branch "$ref" --event workflow_dispatch \
    --created ">=${since}" --limit 1 --json databaseId --jq '.[0].databaseId // empty' || true)"
  if [ -n "$run_id" ]; then
    break
  fi
  sleep 5
done
if [ -z "$run_id" ]; then
  echo "::error::dispatch_workflow.sh: dispatched ${workflow} on ${ref} but found no run for it" >&2
  exit 1
fi

url="$(gh run view "$run_id" --json url --jq .url)"
echo "Watching ${workflow} run ${run_id}: ${url}"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  echo "- \`${workflow}\` on \`${ref}\`: ${url}" >> "$GITHUB_STEP_SUMMARY"
fi

# Tolerate transient gh/API failures inside the loop; the job timeout is the bound.
while true; do
  status="$(gh run view "$run_id" --json status --jq .status || echo unknown)"
  if [ "$status" = "completed" ]; then
    break
  fi
  sleep 15
done

conclusion="$(gh run view "$run_id" --json conclusion --jq .conclusion)"
if [ "$conclusion" != "success" ]; then
  echo "::error::${workflow} run ${run_id} concluded: ${conclusion} (${url})" >&2
  gh run view "$run_id" --log-failed || true
  exit 1
fi
echo "${workflow} run ${run_id} succeeded"
