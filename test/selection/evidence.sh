#!/bin/sh
#
# evidence.sh: run the selection tests against an UNFIXED tree and check each
# result against what the fixes are supposed to change.
#
# The fixes on 'safer' are only trustworthy if the tests that cover them
# fail without them. This script makes that check repeatable: build
# test/selection from 'safer' against the src/ of upstream master (or an
# intermediate commit), then run this script on the resulting binary.
#
#     evidence.sh <path/to/test-selection> [qt-version]
#
# Each test runs in its own process: without the fixes, some tests alter the
# tree (AssumeDeleted on the scan root) or the fixture (Move to Trash) and
# would break every later test in the same process. The Move to Trash tests
# take their items out of the trash again afterwards (TrashGuard in the
# test source), on every tree. Since upstream c0eefcf there is a second
# reason: upstream's restricted mode, once entered, lasts until the process
# ends.
#
# Each test process is limited to TEST_TIMEOUT seconds (default 60, needs
# timeout(1); a test takes about a second). A test that does not finish is
# reported as TIMEOUT and always counts as unexpected: a hang proves nothing
# about the fix, and without the limit one blocked modal dialog would hold
# the run until the CI job's own time limit. With the default, a run in
# which every test hangs still ends with a complete table inside that limit.
#
# Expectations per test, on a tree WITHOUT the fixes:
#
#   FAIL   must fail on every Qt version
#   FAIL<6.8  must fail below Qt 6.8; from 6.8 on QAbstractItemView re-syncs
#          its anchor in currentChanged() when no Shift is reported, so the
#          plain-click reproductions pass there (recorded, not asserted;
#          verified: FAIL on 6.4.2, PASS on 6.12.0)
#   PASS   control: must pass on every tree, fixed or not
#
# The table describes upstream master from c0eefcf on ("restricted mode":
# when an item and one of its ancestors are selected together, upstream
# shows a warning dialog, clears the selection and restricts the rest of the
# program run). The test binary closes that dialog itself
# (UpstreamRestrictedModeDialogCloser in the test source). Consequences for
# this table: the reproductions of the stale Shift anchor still fail, now
# with an empty selection where the range reached the scan root and with the
# over-long range where it did not; and every test that selects the scan
# root together with an item inside it fails there, including the former
# control noRefreshCleanup_allowsToplevelInMultiSelection, because that
# selection no longer survives.
#
# The Shift-held and Shift-drag reproductions are FAIL on every version: the
# re-sync is skipped while Shift is reported, and the drag selection is built
# from the private press position, which the re-sync never touches
# (verified: FAIL on 6.4.2 and 6.12.0).
#
# The Qt version is taken from the first test's output unless given.
#
# Exit status: 0 if every expectation holds, 1 otherwise, 2 on usage error.
# Prints one line per test and a summary. Set VERBOSE=1 for full test output
# of every unexpected result.

BIN=$1
QTVER=$2

if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
    echo "usage: $0 <path/to/test-selection> [qt-version]" >&2
    exit 2
fi

export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen}

TEST_TIMEOUT=${TEST_TIMEOUT:-60}

if command -v timeout >/dev/null 2>&1; then
    run_test() { timeout "$TEST_TIMEOUT" "$@"; }
else
    echo "warning: timeout(1) not found, tests run without a time limit" >&2
    run_test() { "$@"; }
fi

# name expectation (one per line; '#' comments allowed)
TABLE='
# fix 1: Shift range anchor
shiftClickAfterTreemapClick_filesOnly                      FAIL<6.8
shiftClickAfterTreemapClick_shiftHeldDuringTreemapClick    FAIL
shiftDragAfterTreemapClick_filesOnly                       FAIL
shiftClickOnIndentationAfterTreemapClick_filesOnly         FAIL<6.8
shiftClickAfterProgrammaticCurrent_filesOnly               FAIL<6.8
shiftClickAfterTreeAndTreemapClick_fromCurrentOnly         FAIL<6.8
shiftClickAfterTreeAndShiftTreemapClick_fromCurrentOnly    FAIL
shiftDownRepeated_growsFromAnchor                          PASS
shiftDrag_growsAndShrinksFromAnchor                        PASS
ctrlClickThenShiftClick_keepsCommittedRange                PASS
shiftClickWithHiddenAnchor_isPlainClick                    FAIL
shiftClickOnBranchIndicator_togglesExpansion               PASS
# fix 2: scan root guard
deleteCleanup_refusesToplevelInMultiSelection              FAIL
deleteCleanup_refusesToplevelAlone                         FAIL
assumeDeletedCleanup_refusesToplevelAlone                  FAIL
refreshThisCleanup_allowsToplevelAlone_refusesWithOthers   FAIL
noRefreshCleanup_allowsToplevelInMultiSelection            FAIL
deleteCleanup_runsWithoutToplevel                          PASS
moveToTrash_refusesToplevel                                FAIL
# fix 3: confirmation dialog
confirmation_listsAllDirectoriesAndDefaultsToNo            FAIL
confirmation_enterDoesNotConfirm                           FAIL
confirmation_filesAreCutWithACount                         FAIL
confirmation_longDirectoryListGoesToDetails                FAIL
confirmation_flagsNestedSelection                          FAIL
confirmation_marksScanRoot                                 FAIL
confirmation_yesRunsCleanup                                PASS
# fix 4: Move to Trash confirmation
moveToTrash_asksForConfirmationAndDefaultsToNo             FAIL
moveToTrash_enterDoesNotConfirm                            FAIL
moveToTrash_yesMovesToTrash                                PASS
# fix 5: no restricted mode
ctrlClickDirectoryAndChild_staysSelectedAndUnrestricted    FAIL
'

TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

violations=0
total=0
qt_num=0     # major*100 + minor, 0 = unknown

qt_number()
{
    # "6.4.2" -> 604
    major=${1%%.*}
    rest=${1#*.}
    minor=${rest%%.*}
    echo $(( major * 100 + minor ))
}

if [ -n "$QTVER" ]; then
    qt_num=$(qt_number "$QTVER")
fi

printf '%-58s %-9s %-7s %s\n' "test" "expected" "result" "verdict"
printf '%-58s %-9s %-7s %s\n' "----" "--------" "------" "-------"

# A here-document instead of a pipe: the loop must run in this shell so the
# counters and the detected Qt version survive it.
while read -r name expect; do
    case "$name" in
        ''|'#'*) continue ;;
    esac

    run_test "$BIN" "$name" >"$TMP" 2>&1
    rc=$?

    if [ "$rc" -eq 0 ]; then
        result=PASS
    elif [ "$rc" -eq 124 ]; then
        result=TIMEOUT
    else
        result=FAIL
    fi

    if [ -z "$QTVER" ]; then
        QTVER=$(sed -n 's/.*Using QtTest library \([0-9][0-9.]*\).*/\1/p' "$TMP" | head -n 1)
        [ -n "$QTVER" ] && qt_num=$(qt_number "$QTVER")
    fi

    want=$expect

    if [ "$expect" = "FAIL<6.8" ]; then
        if [ "$qt_num" -gt 0 ] && [ "$qt_num" -lt 608 ]; then
            want=FAIL
        else
            want=ANY
        fi
    fi

    if grep -q 'Function not found' "$TMP"; then
        verdict="MISSING (test not in this binary)"
        violation=1
    elif [ "$result" = TIMEOUT ]; then
        verdict="UNEXPECTED (no result within ${TEST_TIMEOUT}s)"
        violation=1
    elif [ "$want" = ANY ]; then
        verdict="recorded"
        violation=0
    elif [ "$want" = "$result" ]; then
        verdict="ok"
        violation=0
    else
        verdict="UNEXPECTED"
        violation=1
    fi

    printf '%-58s %-9s %-7s %s\n' "$name" "$expect" "$result" "$verdict"

    if [ "$violation" -eq 1 ] && [ "${VERBOSE:-0}" = 1 ]; then
        sed 's/^/    | /' "$TMP"
    fi

    total=$(( total + 1 ))
    violations=$(( violations + violation ))
done <<EOF
$TABLE
EOF

echo
echo "Qt ${QTVER:-unknown}: $total tests, $violations unexpected"

if [ "$violations" -ne 0 ]; then
    echo "FAILED: at least one test did not behave as expected on an unfixed tree." >&2
    exit 1
fi

echo "OK: every test behaves as expected on an unfixed tree."
exit 0
