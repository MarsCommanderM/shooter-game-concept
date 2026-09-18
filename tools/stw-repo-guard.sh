#!/usr/bin/env bash
set -Eeuo pipefail

readonly expected_repository="MarsCommanderM/shooter-game-concept"
readonly expected_repository_id="1296772090"
readonly production_branch="brauny/stw-game-production"
readonly mode="${1:-identity}"

fail() {
  printf 'STW_REPO_GUARD=FAIL reason=%s\n' "$*" >&2
  exit 1
}

command -v git >/dev/null 2>&1 || fail "git_missing"

case "${mode}" in
  identity | start | ci) ;;
  *) fail "usage_identity_start_ci" ;;
esac

repository_root="$(git rev-parse --show-toplevel 2>/dev/null)" ||
  fail "not_a_git_worktree"
cd "${repository_root}"

origin_url="$(git remote get-url origin 2>/dev/null || true)"
case "${origin_url}" in
  "https://github.com/${expected_repository}" |   "https://github.com/${expected_repository}.git" |   "git@github.com:${expected_repository}.git")
    ;;
  *)
    fail "wrong_origin"
    ;;
esac

if [[ -n "${GITHUB_REPOSITORY:-}" &&
      "${GITHUB_REPOSITORY}" != "${expected_repository}" ]]; then
  fail "wrong_github_repository"
fi

if [[ -n "${GITHUB_REPOSITORY_ID:-}" &&
      "${GITHUB_REPOSITORY_ID}" != "${expected_repository_id}" ]]; then
  fail "wrong_github_repository_id"
fi

required_paths=(
  ".stw-repository.json"
  "AGENTS.md"
  "stw-o3de/O3DE_VERSION.md"
  "stw-o3de/Gems/STWGameplay/gem.json"
  ".github/lightning-t4/task.sh"
  ".github/workflows/stw-lightning-t4.yml"
)

for required_path in "${required_paths[@]}"; do
  [[ -e "${repository_root}/${required_path}" ]] ||
    fail "missing_required_path:${required_path}"
done

[[ ! -e "${repository_root}/nova" ]] || fail "retired_nova_tree_present"

# Archived on 2026-09-18 (tag archive/legacy-web-prototype-20260918); must not return.
for archived_path in app components server.mjs stw-engine unity-starter; do
  [[ ! -e "${repository_root}/${archived_path}" ]] ||
    fail "archived_legacy_path_present:${archived_path}"
done

if [[ "${mode}" == "ci" ]]; then
  bash -n "${repository_root}/tools/stw-repo-guard.sh" ||
    fail "guard_syntax"
  git diff --check HEAD^ HEAD ||
    fail "diff_check"
  printf 'STW_REPO_GUARD=PASS mode=ci repository=%s sha=%s\n'     "${expected_repository}" "$(git rev-parse HEAD)"
  exit 0
fi

current_branch="$(git symbolic-ref --quiet --short HEAD 2>/dev/null || true)"
[[ -n "${current_branch}" ]] || fail "detached_head"
[[ "${current_branch}" != "main" ]] || fail "stale_main_forbidden"

git fetch --quiet --no-tags origin   "refs/heads/${production_branch}:refs/remotes/origin/${production_branch}" ||
  fail "production_fetch_failed"

production_sha="$(git rev-parse "refs/remotes/origin/${production_branch}" 2>/dev/null)" ||
  fail "production_ref_missing"
head_sha="$(git rev-parse HEAD)"

case "${current_branch}" in
  "${production_branch}")
    [[ "${head_sha}" == "${production_sha}" ]] ||
      fail "production_checkout_not_at_remote_head"
    ;;
  codex/stw-* | recovery/stw-*)
    git merge-base --is-ancestor "${production_sha}" HEAD ||
      fail "task_branch_missing_fresh_production_head"
    ;;
  *)
    fail "unauthorized_branch"
    ;;
esac

if [[ "${mode}" == "start" ]]; then
  [[ -z "$(git status --porcelain)" ]] || fail "dirty_task_start"
fi

printf 'STW_REPO_GUARD=PASS mode=%s repository=%s branch=%s head=%s production=%s\n'   "${mode}" "${expected_repository}" "${current_branch}"   "${head_sha}" "${production_sha}"
