#!/usr/bin/env bash
#
# sync-upstream.sh: bring the safer-qdirstat fork up to date with upstream.
#
#   master   byte-identical mirror of upstream/master, fast-forward only
#   safer    the fork's commits (README first, then one per fix), rebased
#            onto master
#
# In order: fetch upstream and origin; fast-forward master to upstream/master
# (refuses if master has commits of its own); rebase safer onto master; build
# the application and test/selection in a shadow directory and run the suite
# (must report 0 failed); then stop and print the push commands. With --push
# it pushes master first (the CI evidence job checks out master) and then
# safer with a lease on the origin/safer fetched at the start, so a push made
# elsewhere in the meantime is not overwritten.
#
# Usage, from anywhere inside the clone:
#
#   scripts/sync-upstream.sh               fetch, mirror, rebase, build, test, stop
#   scripts/sync-upstream.sh --push        the same, then push master and safer
#   scripts/sync-upstream.sh --no-build    skip the build and the suite; never pushes
#   scripts/sync-upstream.sh --build-dir D shadow build directory
#                                          (default: ../build-safer-sync next to the clone)
#
# Environment: QMAKE (default qmake6), JOBS (default: nproc).
#
# A clone without a local master branch gets one from origin/master. If
# upstream has not moved and safer is already on it, the script says so and
# exits 0 without building. On a rebase conflict it exits 1 with the
# rebase in progress: resolve and "git rebase --continue", or
# "git rebase --abort", then rerun. If the build or the suite fails, safer
# stays rebased and nothing is pushed; the previous safer head is printed
# for "git reset --hard".

set -euo pipefail

UPSTREAM_URL="https://github.com/shundhammer/qdirstat.git"
QMAKE="${QMAKE:-qmake6}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"

push=0
build=1
build_dir=""

while [ $# -gt 0 ]; do
    case "$1" in
        --push)      push=1 ;;
        --no-build)  build=0 ;;
        --build-dir) shift; build_dir="${1:?--build-dir needs a directory}" ;;
        -h|--help)   sed -n '2,33p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)           echo "unknown option: $1 (try --help)" >&2; exit 2 ;;
    esac
    shift
done

if [ $push -eq 1 ] && [ $build -eq 0 ]; then
    echo "refusing --push together with --no-build: the suite must pass before a push" >&2
    exit 2
fi

toplevel="$(git rev-parse --show-toplevel)"
cd "$toplevel"
[ -n "$build_dir" ] || build_dir="$toplevel/../build-safer-sync"

git show-ref --verify --quiet refs/heads/safer || { echo "no local branch 'safer'" >&2; exit 1; }

if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    echo "worktree has uncommitted changes; commit or stash first" >&2
    exit 1
fi

git remote get-url upstream >/dev/null 2>&1 || git remote add upstream "$UPSTREAM_URL"

echo "fetching upstream and origin"
git fetch -q upstream
git fetch -q origin

if ! git show-ref --verify --quiet refs/heads/master; then
    git branch -q --track master origin/master
    echo "created local master from origin/master ($(git rev-parse --short=7 master))"
fi

old_master="$(git rev-parse master)"
new_master="$(git rev-parse upstream/master)"
old_safer="$(git rev-parse safer)"
origin_safer="$(git rev-parse origin/safer)"

short() { git rev-parse --short=7 "$1"; }

if ! git merge-base --is-ancestor master upstream/master; then
    echo "local master has commits that are not in upstream/master; refusing" >&2
    echo "(master must stay a mirror; move any such commit to safer first)" >&2
    exit 1
fi

if [ "$old_safer" != "$origin_safer" ]; then
    echo "note: local safer ($(short safer)) differs from origin/safer ($(short origin/safer));"
    echo "      local commits are rebased along, the push lease is on origin/safer"
fi

if [ "$old_master" = "$new_master" ] && git merge-base --is-ancestor master safer; then
    echo "upstream unchanged at $(short upstream/master); safer is on it, nothing to do"
    exit 0
fi

git switch -q master
git merge -q --ff-only upstream/master
echo "master: $(short "$old_master") -> $(short master) ($(git log -1 --format=%s master))"

git switch -q safer
if ! git rebase -q master; then
    echo >&2
    echo "rebase stopped on a conflict. Resolve, then: git rebase --continue" >&2
    echo "or give up for now:                         git rebase --abort" >&2
    echo "previous safer head: $old_safer" >&2
    exit 1
fi
echo "safer rebased: $(short "$old_safer") -> $(short safer); commits above master:"
git log --oneline master..safer

if [ $build -eq 0 ]; then
    echo
    echo "not built (--no-build), not pushed. Build and run test/selection, then:"
    echo "  git push origin master"
    echo "  git push --force-with-lease=safer:$origin_safer origin safer"
    exit 0
fi

echo
echo "building in $build_dir (log: $build_dir/build.log)"
mkdir -p "$build_dir"
if ! ( cd "$build_dir" && "$QMAKE" "$toplevel/qdirstat.pro" && make -j"$JOBS" ) > "$build_dir/build.log" 2>&1; then
    tail -20 "$build_dir/build.log" >&2
    echo >&2
    echo "build failed; not pushed. safer is rebased; previous head: $old_safer" >&2
    exit 1
fi

echo "running test/selection (log: $build_dir/test-selection.log)"
( cd "$build_dir" && QT_QPA_PLATFORM=offscreen ./test/selection/test-selection ) \
    > "$build_dir/test-selection.log" 2>&1 || true
totals="$(grep -E '^Totals:' "$build_dir/test-selection.log" || true)"
echo "${totals:-no Totals line (binary did not run to the end)}"
grep -E '^FAIL!' "$build_dir/test-selection.log" || true

case "$totals" in
    *" 0 failed, "*) ;;
    *)
        echo >&2
        echo "the suite did not pass; not pushed. safer is rebased; previous head: $old_safer" >&2
        exit 1 ;;
esac

if [ $push -eq 1 ]; then
    echo
    git push origin master
    git push --force-with-lease="safer:$origin_safer" origin safer
    echo "pushed. origin/safer: $(short origin/safer); CI runs on it next."
else
    echo
    echo "not pushed. Review, then:"
    echo "  git push origin master"
    echo "  git push --force-with-lease=safer:$origin_safer origin safer"
fi
