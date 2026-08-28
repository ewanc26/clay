#!/usr/bin/env bash
# Enforce the repo's MANDATORY scoped conventional-commit format.
#
#   feat(render): fill_circle now rasters from scratch
#   fix(core):    arena frame returned the wrong offset
#   docs(readme): document the reaction-rule schema
#
# Valid scopes map to modules: core, engine, ecs, render, systems, input,
# action, command, replay, event, demo, test, docs, build, ci, scripts.
#
# Usage:
#   ./scripts/lint_commits.sh              # $GITHUB_BASE_REF..HEAD, else HEAD~10
#   ./scripts/lint_commits.sh RANGE        # e.g. origin/main..HEAD
#
# Exits nonzero when any commit in the range violates the format. Merge
# commits are implementation-annotated (--no-ff) and skipped.
set -euo pipefail

PATTERN='^(feat|fix|docs|test|refactor|perf|chore|build|ci|revert)(\([a-z0-9_-]+\))?(!)?: .+'
RANGE="${1:-}"

if [[ -z "${RANGE}" ]]; then
  if [[ -n "${GITHUB_BASE_REF:-}" ]]; then
    RANGE="origin/${GITHUB_BASE_REF}..HEAD"
  else
    RANGE="HEAD~10..HEAD"
  fi
fi

fail=0
while IFS= read -r line; do
  [[ -z "${line}" ]] && continue
  if [[ "${line}" =~ ^Merge\ (branch|pull) ]]; then
    continue
  fi
  printf '  checking: %s\n' "${line}"
  if [[ ! "${line}" =~ ${PATTERN} ]]; then
    printf '  VIOLATION: %s\n' "${line}"
    fail=1
  fi
done < <(git log --format=%s "${RANGE}")

if (( fail )); then
  printf 'error: commit messages must be scoped conventional commits.\n'
  printf 'pattern: <type>(<scope>): <summary>\n'
  printf 'e.g.     feat(render): rasterize circles in the software pipeline\n'
  printf 'fix the violating commits (git commit --amend / rebase) and push again.\n'
  exit 1
fi

printf 'ok: %s\n' "${RANGE}"