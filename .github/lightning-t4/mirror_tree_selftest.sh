#!/usr/bin/env bash
# Static scratch self-test for the mirror_tree synchronization helper in task.sh.
#
# It extracts the live mirror_tree definition from task.sh (single source of
# truth, no drift) and exercises it ONLY against disposable scratch directories
# created under a fresh mktemp root. It never runs task.sh, never configures or
# builds O3DE, and never touches the real persistent T4/O3DE worktree or build
# tree. Safe to run anywhere.
#
#   bash .github/lightning-t4/mirror_tree_selftest.sh
set -Eeuo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
task_sh="${here}/task.sh"
[[ -f "${task_sh}" ]] || { echo "TASK_SH_MISSING=${task_sh}"; exit 1; }

work="$(mktemp -d "${TMPDIR:-/tmp}/mirror-tree-selftest.XXXXXX")"
trap 'rm -rf "${work}"' EXIT

fail(){ echo "SELFTEST_FAIL: $*"; exit 1; }

# --- extract mirror_tree from task.sh ----------------------------------------
awk '/^# >>> STW_SYNC_MIRROR_TREE/{f=1} f{print} /^# <<< STW_SYNC_MIRROR_TREE/{f=0}' \
  "${task_sh}" > "${work}/mirror_tree.sh"
grep -q '^mirror_tree()' "${work}/mirror_tree.sh" || fail "could not extract mirror_tree from task.sh"
# shellcheck source=/dev/null
source "${work}/mirror_tree.sh"

# Snapshot of everything under a root: path, type and content hash. Used to prove
# a rejected call mutated nothing at all, not merely that it returned non-zero.
tree_state(){
  local root="$1"
  [[ -e "${root}" ]] || { echo "ABSENT"; return 0; }
  find "${root}" -mindepth 0 -print0 \
    | sort -z \
    | while IFS= read -r -d '' path; do
        if [[ -L "${path}" ]]; then
          printf 'L %s -> %s\n' "${path#"${root}"}" "$(readlink "${path}")"
        elif [[ -d "${path}" ]]; then
          printf 'D %s\n' "${path#"${root}"}"
        else
          printf 'F %s %s\n' "${path#"${root}"}" "$(cksum < "${path}")"
        fi
      done
}

# --- Test 1: additions / modifications / deletions / outside-boundary --------
boundary="${work}/t1/worktree"
src="${work}/t1/checkout/src"
dst="${boundary}/Project/Assets"
mkdir -p "${src}" "${dst}"

printf 'new-content'   > "${src}/keep.txt"
printf 'new-file'      > "${src}/new.txt"
printf 'old-content'   > "${dst}/keep.txt"
printf 'stale-content' > "${dst}/stale.txt"
printf 'preserve-me'   > "${boundary}/Project/sibling-important.txt"

mirror_tree "${src}" "${dst}" "${boundary}" > "${work}/t1.log" 2>&1 \
  || fail "mirror_tree returned non-zero on a valid sync"

[[ -f "${dst}/keep.txt" && "$(cat "${dst}/keep.txt")" == "new-content" ]] \
  || fail "keep.txt not updated to source content"
[[ -f "${dst}/new.txt" && "$(cat "${dst}/new.txt")" == "new-file" ]] \
  || fail "new.txt not copied into destination"
[[ ! -e "${dst}/stale.txt" ]] \
  || fail "stale.txt still present after sync"
[[ -f "${boundary}/Project/sibling-important.txt" \
   && "$(cat "${boundary}/Project/sibling-important.txt")" == "preserve-me" ]] \
  || fail "file outside the destination root was disturbed"

echo "NORMAL_NEW_COPY=PASS"
echo "NORMAL_CHANGED_COPY=PASS"
echo "NORMAL_STALE_DELETE=PASS"
echo "OUTSIDE_BOUNDARY_FILE_PRESERVED=PASS"

# A first run with no destination at all must still work: the destination is
# canonicalised without being created, then created only after every guard.
fresh_dst="${boundary}/Project/FreshAssets/Nested"
mirror_tree "${src}" "${fresh_dst}" "${boundary}" > "${work}/t1b.log" 2>&1 \
  || fail "mirror_tree returned non-zero creating a fresh destination"
[[ -f "${fresh_dst}/new.txt" ]] || fail "fresh destination was not populated"
echo "FRESH_DESTINATION_CREATED=PASS"

# --- Test 2: guards. Every rejection must also mutate nothing ----------------
# `desc` names the token the caller is proving; `watch` is the tree that must be
# byte-identical afterwards.
guard_must_reject(){
  local token="$1" watch="$2"; shift 2
  local before after
  before="$(tree_state "${watch}")"
  if mirror_tree "$@" > "${work}/guard.log" 2>&1; then
    fail "unsafe call accepted: ${token}"
  fi
  after="$(tree_state "${watch}")"
  [[ "${before}" == "${after}" ]] \
    || fail "${token}: rejected call still mutated ${watch}"
  echo "${token}=PASS"
}

guard_must_reject "EMPTY_DESTINATION_REJECTED"    "${boundary}" "${src}" ""                  "${boundary}"
guard_must_reject "FILESYSTEM_ROOT_REJECTED"      "${boundary}" "${src}" "/"                 "${boundary}"
guard_must_reject "RELATIVE_DESTINATION_REJECTED" "${boundary}" "${src}" "relative/dst"      "${boundary}"
guard_must_reject "DOTDOT_ESCAPE_REJECTED"        "${boundary}" "${src}" "${boundary}/../x"  "${boundary}"
guard_must_reject "DEST_EQUALS_BOUNDARY_REJECTED" "${boundary}" "${src}" "${boundary}"       "${boundary}"
guard_must_reject "DEST_OUTSIDE_BOUNDARY_REJECTED" "${boundary}" "${src}" "${work}/t1/outside" "${boundary}"

# --- Test 3: source/destination overlap -------------------------------------
ov_boundary="${work}/t3"
ov_wt="${ov_boundary}/worktree"
ov_src="${ov_wt}/src"
mkdir -p "${ov_src}/sub"
printf 'payload' > "${ov_src}/sub/file.txt"

guard_must_reject "SOURCE_EQUALS_DESTINATION_REJECTED" "${ov_boundary}" \
  "${ov_src}" "${ov_src}" "${ov_boundary}"
guard_must_reject "SOURCE_CONTAINS_DESTINATION_REJECTED" "${ov_boundary}" \
  "${ov_src}" "${ov_src}/sub" "${ov_boundary}"
guard_must_reject "DESTINATION_CONTAINS_SOURCE_REJECTED" "${ov_boundary}" \
  "${ov_src}" "${ov_wt}" "${ov_boundary}"

# --- Test 4: descendant symlinks --------------------------------------------
sl_boundary="${work}/t4/worktree"
sl_src="${work}/t4/checkout/src"
sl_dst="${sl_boundary}/Assets"
mkdir -p "${sl_src}/sub" "${sl_dst}/sub" "${work}/t4/elsewhere"
printf 'outside-target' > "${work}/t4/elsewhere/target.txt"
printf 'real'           > "${sl_src}/real.txt"

ln -s "${work}/t4/elsewhere/target.txt" "${sl_src}/sub/link.txt"
guard_must_reject "SOURCE_DESCENDANT_SYMLINK_REJECTED" "${work}/t4" \
  "${sl_src}" "${sl_dst}" "${sl_boundary}"
rm -f "${sl_src}/sub/link.txt"

ln -s "${work}/t4/elsewhere" "${sl_dst}/sub/escape"
guard_must_reject "DESTINATION_DESCENDANT_SYMLINK_REJECTED" "${work}/t4" \
  "${sl_src}" "${sl_dst}" "${sl_boundary}"
rm -f "${sl_dst}/sub/escape"

# --- Test 5: file / directory type conflicts --------------------------------
tc_boundary="${work}/t5/worktree"
tc_src="${work}/t5/checkout/src"
tc_dst="${tc_boundary}/Assets"
mkdir -p "${tc_src}" "${tc_dst}"
printf 'a-content' > "${tc_src}/a.txt"
mkdir -p "${tc_dst}/a.txt"
printf 'held' > "${tc_dst}/a.txt/held-by-host.txt"
guard_must_reject "SOURCE_FILE_DEST_DIR_CONFLICT_REJECTED" "${work}/t5" \
  "${tc_src}" "${tc_dst}" "${tc_boundary}"
rm -rf "${tc_dst}/a.txt"

mkdir -p "${tc_src}/b"
printf 'b-content' > "${tc_src}/b/inner.txt"
printf 'host-file' > "${tc_dst}/b"
guard_must_reject "SOURCE_DIR_DEST_FILE_CONFLICT_REJECTED" "${work}/t5" \
  "${tc_src}" "${tc_dst}" "${tc_boundary}"
rm -f "${tc_dst}/b"

# The same trees sync cleanly once the conflicts are gone, so the guards above
# are proving a conflict rather than a broken fixture.
mirror_tree "${tc_src}" "${tc_dst}" "${tc_boundary}" > "${work}/t5.log" 2>&1 \
  || fail "type-conflict fixture failed to sync after the conflicts were removed"
[[ -f "${tc_dst}/a.txt" && -f "${tc_dst}/b/inner.txt" ]] \
  || fail "type-conflict fixture did not sync its files"
echo "TYPE_CONFLICT_FIXTURE_SYNCS_WHEN_CLEAN=PASS"

# --- Test 6: Block26E legacy arena deletion simulation -----------------------
# Reproduces the intended Block26E state: the pushed authoritative Project/Assets
# tree carries only the new arena kit, while a derived worktree still holds the
# retired Block26D arena meshes and .mtl sidecars from an earlier run.
b_wt="${work}/b26e/worktree/stw-o3de"
b_src="${work}/b26e/checkout/stw-o3de/Project/Assets"
b_dst="${b_wt}/Project/Assets"
arena_src="${b_src}/Environment/STW_ARENA_01"
arena_dst="${b_dst}/Environment/STW_ARENA_01"
mkdir -p "${arena_src}" "${arena_dst}"

# authoritative source (post-Block26E): new kit only
for f in STW_ARENA_ARCH_01.obj STW_ARENA_WALL_01.obj STW_ARENA_MARK_01.obj \
         STW_ARENA_MARK_01.material STW_ARENA_01.material; do
  printf 'kit:%s' "${f}" > "${arena_src}/${f}"
done

# retired Block26D files still sitting in the derived destination
legacy_stale=(
  Environment/STW_ARENA_01/STW_ARENA_01.obj
  Environment/STW_ARENA_01/STW_ARENA_COVER_01.obj
  Environment/STW_ARENA_01/STW_ARENA_LANDMARK_01.obj
  Environment/STW_ARENA_01/STW_ARENA_TRIM_01.obj
  Environment/STW_ARENA_01/STW_ARENA_01.mtl
  Environment/STW_ARENA_01/STW_ARENA_COVER_01.mtl
  Environment/STW_ARENA_01/STW_ARENA_LANDMARK_01.mtl
  Environment/STW_ARENA_01/STW_ARENA_TRIM_01.mtl
)
for rel in "${legacy_stale[@]}"; do
  mkdir -p "$(dirname "${b_dst}/${rel}")"
  printf 'legacy:%s' "${rel}" > "${b_dst}/${rel}"
done
printf 'outdated' > "${arena_dst}/STW_ARENA_01.material"   # present but stale content

mirror_tree "${b_src}" "${b_dst}" "${b_wt}" > "${work}/t6.log" 2>&1 \
  || fail "Block26E simulation mirror_tree returned non-zero"

sim_ok=1
for rel in "${legacy_stale[@]}"; do
  if [[ -e "${b_dst}/${rel}" ]]; then echo "  STILL PRESENT: ${rel}"; sim_ok=0; fi
done
[[ "${sim_ok}" -eq 1 ]] || fail "retired Block26D arena files survived the sync"
[[ "$(cat "${arena_dst}/STW_ARENA_01.material")" == "kit:STW_ARENA_01.material" ]] \
  || fail "current material was not refreshed from source"
[[ -f "${arena_dst}/STW_ARENA_ARCH_01.obj" ]] || fail "new kit mesh missing from destination"

echo "SIMULATED_STALE_PATHS_REMOVED:"
grep 'MIRROR_TREE_STALE_REMOVED=' "${work}/t6.log" | sed 's/^/  /'
echo "BLOCK26E_LEGACY_DELETION_SIMULATION=PASS"

echo "ALL_MIRROR_TREE_SELFTESTS=PASS"
