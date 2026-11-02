# test/selection results

Evidence that the Shift-anchor regression tests fail on upstream `master` and
pass on `safer`. Regenerate with the commands at the end; keep this file in
sync when the matrix changes.

## Matrix

| Qt    | platform          | master                                   | safer      |
|-------|-------------------|------------------------------------------|------------|
| 6.4.2 | Ubuntu 24.04, offscreen | fix 1 tests on master: 6 FAIL, 6 PASS; fix 2 tests on the fix-1-only tree: 5 FAIL, 2 PASS; fix 3 tests on the fix-2 tree: 6 FAIL, 1 PASS | 26 PASS |
| 6.12.0 | Linux, GCC 16, offscreen (local build) | `evidence.sh` on master: 24 tests, 0 unexpected. Plain-click reproductions (`shiftClickAfterTreemapClick_filesOnly`, `shiftClickOnIndentation...`, `shiftClickAfterProgrammaticCurrent...`) PASS on master (anchor re-sync); `shiftHeldDuringTreemapClick` and `shiftDrag...filesOnly` FAIL; hidden anchor, all fix 2 and fix 3 tests FAIL; controls PASS. Fix 3 tests on the fix-2 tree: 6 FAIL, 1 PASS | 26 PASS |
| 6.8.x, 6.10.x | not run locally; CI runs 6.10.0 | expected as 6.12 | expected 26 PASS |

The tests named `*_filesOnly` and `*_shiftHeldDuringTreemapClick` are the
reproduction of shundhammer/qdirstat#329. On `master` with Qt 6.4.2 every one
of them selects 16 rows after a Shift-click inside `C_target`: the scan root,
the two collapsed sibling directories, `C_target` itself and the 12 files. Only
the 12 files are visible as selected inside the clicked directory; the rest is
scrolled out of view.

## master (unpatched tree), Qt 6.4.2

```
********* Start testing of TestSelection *********
Config: Using QtTest library 6.4.2, Qt 6.4.2 (x86_64-little_endian-lp64 shared (dynamic) release build; by GCC 13.2.0), ubuntu 24.04
PASS   : TestSelection::initTestCase()
FAIL!  : TestSelection::shiftClickAfterTreemapClick_filesOnly() 'actual_ == expected_' returned FALSE. (
   selected: [<scan root>, A_dir, B_dir, C_target, C_target/f01, C_target/f02, C_target/f03, C_target/f04, C_target/f05, C_target/f06, C_target/f07, C_target/f08, C_target/f09, C_target/f10, C_target/f11, C_target/f12]
   expected: [C_target/f01, C_target/f02, C_target/f03, C_target/f04, C_target/f05, C_target/f06, C_target/f07, C_target/f08, C_target/f09, C_target/f10, C_target/f11, C_target/f12])
   Loc: [../work-master/test/selection/test-selection.cpp(513)]
FAIL!  : TestSelection::shiftClickAfterTreemapClick_shiftHeldDuringTreemapClick() 'actual_ == expected_' returned FALSE. (
   selected: [<scan root>, A_dir, B_dir, C_target, C_target/f01, C_target/f02, C_target/f03, C_target/f04, C_target/f05, C_target/f06, C_target/f07, C_target/f08, C_target/f09, C_target/f10, C_target/f11, C_target/f12]
   expected: [C_target/f01, C_target/f02, C_target/f03, C_target/f04, C_target/f05, C_target/f06, C_target/f07, C_target/f08, C_target/f09, C_target/f10, C_target/f11, C_target/f12])
   Loc: [../work-master/test/selection/test-selection.cpp(528)]
FAIL!  : TestSelection::shiftDragAfterTreemapClick_filesOnly() 'actual_ == expected_' returned FALSE. (
   selected: [<scan root>, A_dir, B_dir, C_target, C_target/f01, C_target/f02, C_target/f03, C_target/f04, C_target/f05, C_target/f06, C_target/f07, C_target/f08, C_target/f09, C_target/f10, C_target/f11, C_target/f12]
   expected: [C_target/f01, C_target/f02, C_target/f03, C_target/f04, C_target/f05, C_target/f06, C_target/f07, C_target/f08, C_target/f09, C_target/f10, C_target/f11, C_target/f12])
   Loc: [../work-master/test/selection/test-selection.cpp(545)]
FAIL!  : TestSelection::shiftClickOnIndentationAfterTreemapClick_filesOnly() 'actual_ == expected_' returned FALSE. (
   selected: [<scan root>, A_dir, B_dir, C_target, C_target/f01, C_target/f02, C_target/f03, C_target/f04, C_target/f05, C_target/f06, C_target/f07, C_target/f08, C_target/f09, C_target/f10, C_target/f11, C_target/f12]
   expected: [C_target/f01, C_target/f02, C_target/f03, C_target/f04, C_target/f05, C_target/f06, C_target/f07, C_target/f08, C_target/f09, C_target/f10, C_target/f11, C_target/f12])
   Loc: [../work-master/test/selection/test-selection.cpp(564)]
FAIL!  : TestSelection::shiftClickAfterProgrammaticCurrent_filesOnly() 'actual_ == expected_' returned FALSE. (
   selected: [<scan root>, A_dir, B_dir, C_target, C_target/f01, C_target/f02, C_target/f03, C_target/f04, C_target/f05, C_target/f06, C_target/f07, C_target/f08, C_target/f09, C_target/f10, C_target/f11, C_target/f12]
   expected: [C_target/f02, C_target/f03, C_target/f04, C_target/f05, C_target/f06, C_target/f07, C_target/f08, C_target/f09, C_target/f10, C_target/f11, C_target/f12])
   Loc: [../work-master/test/selection/test-selection.cpp(585)]
PASS   : TestSelection::shiftDownRepeated_growsFromAnchor()
PASS   : TestSelection::shiftDrag_growsAndShrinksFromAnchor()
PASS   : TestSelection::ctrlClickThenShiftClick_keepsCommittedRange()
FAIL!  : TestSelection::shiftClickWithHiddenAnchor_isPlainClick() 'actual_ == expected_' returned FALSE. (
   selected: [<scan root>, A_dir, B_dir, C_target, D_dir]
   expected: [D_dir])
   Loc: [../work-master/test/selection/test-selection.cpp(660)]
PASS   : TestSelection::shiftClickOnBranchIndicator_togglesExpansion()
PASS   : TestSelection::cleanupTestCase()
Totals: 6 passed, 6 failed, 0 skipped, 0 blacklisted, 674ms
********* Finished testing of TestSelection *********
```

## safer (fix 1 + fix 2 + fix 3), Qt 6.4.2

```
********* Start testing of TestSelection *********
Config: Using QtTest library 6.4.2, Qt 6.4.2 (x86_64-little_endian-lp64 shared (dynamic) release build; by GCC 13.2.0), ubuntu 24.04
PASS   : TestSelection::initTestCase()
PASS   : TestSelection::shiftClickAfterTreemapClick_filesOnly()
PASS   : TestSelection::shiftClickAfterTreemapClick_shiftHeldDuringTreemapClick()
PASS   : TestSelection::shiftDragAfterTreemapClick_filesOnly()
PASS   : TestSelection::shiftClickOnIndentationAfterTreemapClick_filesOnly()
PASS   : TestSelection::shiftClickAfterProgrammaticCurrent_filesOnly()
PASS   : TestSelection::shiftDownRepeated_growsFromAnchor()
PASS   : TestSelection::shiftDrag_growsAndShrinksFromAnchor()
PASS   : TestSelection::ctrlClickThenShiftClick_keepsCommittedRange()
PASS   : TestSelection::shiftClickWithHiddenAnchor_isPlainClick()
PASS   : TestSelection::shiftClickOnBranchIndicator_togglesExpansion()
PASS   : TestSelection::deleteCleanup_refusesToplevelInMultiSelection()
PASS   : TestSelection::deleteCleanup_refusesToplevelAlone()
PASS   : TestSelection::assumeDeletedCleanup_refusesToplevelAlone()
PASS   : TestSelection::refreshThisCleanup_allowsToplevelAlone_refusesWithOthers()
PASS   : TestSelection::noRefreshCleanup_allowsToplevelInMultiSelection()
PASS   : TestSelection::deleteCleanup_runsWithoutToplevel()
PASS   : TestSelection::moveToTrash_refusesToplevel()
PASS   : TestSelection::confirmation_listsAllDirectoriesAndDefaultsToNo()
PASS   : TestSelection::confirmation_enterDoesNotConfirm()
PASS   : TestSelection::confirmation_filesAreCutWithACount()
  /tmp/QDirStat-ZajIQX/root/E_dirs/d01
  /tmp/QDirStat-ZajIQX/root/E_dirs/d02
  /tmp/QDirStat-ZajIQX/root/E_dirs/d03
  /tmp/QDirStat-ZajIQX/root/E_dirs/d04
  /tmp/QDirStat-ZajIQX/root/E_dirs/d05
  /tmp/QDirStat-ZajIQX/root/E_dirs/d06
  /tmp/QDirStat-ZajIQX/root/E_dirs/d07
  /tmp/QDirStat-ZajIQX/root/E_dirs/d08
  /tmp/QDirStat-ZajIQX/root/E_dirs/d09
  /tmp/QDirStat-ZajIQX/root/E_dirs/d10
  /tmp/QDirStat-ZajIQX/root/E_dirs/d11
  /tmp/QDirStat-ZajIQX/root/E_dirs/d12
  /tmp/QDirStat-ZajIQX/root/E_dirs/d13
  /tmp/QDirStat-ZajIQX/root/E_dirs/d14
  /tmp/QDirStat-ZajIQX/root/E_dirs/d15
  /tmp/QDirStat-ZajIQX/root/E_dirs/d16
  /tmp/QDirStat-ZajIQX/root/E_dirs/d17
  /tmp/QDirStat-ZajIQX/root/E_dirs/d18
  /tmp/QDirStat-ZajIQX/root/E_dirs/d19
  /tmp/QDirStat-ZajIQX/root/E_dirs/d20
  /tmp/QDirStat-ZajIQX/root/E_dirs/d21
  /tmp/QDirStat-ZajIQX/root/E_dirs/d22
  /tmp/QDirStat-ZajIQX/root/E_dirs/d23
  /tmp/QDirStat-ZajIQX/root/E_dirs/d24
  /tmp/QDirStat-ZajIQX/root/E_dirs/d25
  /tmp/QDirStat-ZajIQX/root/E_dirs/d26
  /tmp/QDirStat-ZajIQX/root/E_dirs/d27
  /tmp/QDirStat-ZajIQX/root/E_dirs/d28
  /tmp/QDirStat-ZajIQX/root/E_dirs/d29
  /tmp/QDirStat-ZajIQX/root/E_dirs/d30

PASS   : TestSelection::confirmation_longDirectoryListGoesToDetails()
PASS   : TestSelection::confirmation_flagsNestedSelection()
PASS   : TestSelection::confirmation_marksScanRoot()
PASS   : TestSelection::confirmation_yesRunsCleanup()
PASS   : TestSelection::cleanupTestCase()
Totals: 26 passed, 0 failed, 0 skipped, 0 blacklisted, 4760ms
********* Finished testing of TestSelection *********
```

## Qt 6.12.0 (local build: Linux, GCC 16, offscreen)

safer: 26 passed, 0 failed, 4725 ms. Fix 3 tests against the fix-2 tree:
6 FAIL / 1 PASS, same assertions as on 6.4.2.

master, `evidence.sh`, upstream `ed3478e`:

```
test                                                       expected  result  verdict
----                                                       --------  ------  -------
shiftClickAfterTreemapClick_filesOnly                      FAIL<6.8  PASS    recorded
shiftClickAfterTreemapClick_shiftHeldDuringTreemapClick    FAIL      FAIL    ok
shiftDragAfterTreemapClick_filesOnly                       FAIL<6.8  FAIL    recorded
shiftClickOnIndentationAfterTreemapClick_filesOnly         FAIL<6.8  PASS    recorded
shiftClickAfterProgrammaticCurrent_filesOnly               FAIL<6.8  PASS    recorded
shiftDownRepeated_growsFromAnchor                          PASS      PASS    ok
shiftDrag_growsAndShrinksFromAnchor                        PASS      PASS    ok
ctrlClickThenShiftClick_keepsCommittedRange                PASS      PASS    ok
shiftClickWithHiddenAnchor_isPlainClick                    FAIL      FAIL    ok
shiftClickOnBranchIndicator_togglesExpansion               PASS      PASS    ok
deleteCleanup_refusesToplevelInMultiSelection              FAIL      FAIL    ok
deleteCleanup_refusesToplevelAlone                         FAIL      FAIL    ok
assumeDeletedCleanup_refusesToplevelAlone                  FAIL      FAIL    ok
refreshThisCleanup_allowsToplevelAlone_refusesWithOthers   FAIL      FAIL    ok
noRefreshCleanup_allowsToplevelInMultiSelection            PASS      PASS    ok
deleteCleanup_runsWithoutToplevel                          PASS      PASS    ok
moveToTrash_refusesToplevel                                FAIL      FAIL    ok
confirmation_listsAllDirectoriesAndDefaultsToNo            FAIL      FAIL    ok
confirmation_enterDoesNotConfirm                           FAIL      FAIL    ok
confirmation_filesAreCutWithACount                         FAIL      FAIL    ok
confirmation_longDirectoryListGoesToDetails                FAIL      FAIL    ok
confirmation_flagsNestedSelection                          FAIL      FAIL    ok
confirmation_marksScanRoot                                 FAIL      FAIL    ok
confirmation_yesRunsCleanup                                PASS      PASS    ok

Qt 6.12.0: 24 tests, 0 unexpected
```

What this settles: from Qt 6.8 on, QAbstractItemView re-syncs its private
anchor in currentChanged() when no Shift is reported, so a plain treemap
click (or any programmatic current change made without Shift held) followed
by a Shift-click behaves on master. Two paths stay broken on every Qt: the
re-sync is skipped while Shift is reported (shiftHeld...), and a Shift+click
with pointer motion before release selects from the private press position,
which the re-sync never updates (shiftDrag...). After this run the drag test
is `FAIL` unconditionally in the expectation table; this table was produced
while it was still `FAIL<6.8`, which is why its row says "recorded".

## Fix 2: scan root guard

Tests `deleteCleanup_*`, `assumeDeletedCleanup_*`, `refreshThisCleanup_*`,
`noRefreshCleanup_*`, `moveToTrash_*`. Each refusal must show the refusal
dialog (object name `toplevelRefusal`); the two `*_allows*` / `*_runs*`
controls must pass on every tree.

Against the tree with fix 1 only (commit "DirTreeView: anchor Shift ranges
on the visible current item"), Qt 6.4.2. The tests were run one per process:
without the guard, `assumeDeletedCleanup_refusesToplevelAlone` removes the
scan root from the tree and `moveToTrash_refusesToplevel` moves the fixture
directory to the trash, which breaks every later test in the same process.
Running them against an unguarded tree therefore leaves a small fixture
directory in the trash; empty it afterwards.

```
===== safer with fix 1 only (no scan root guard), Qt 6.4.2, each fix-2 test run on its own =====
FAIL!  : TestSelection::deleteCleanup_refusesToplevelInMultiSelection() Compared values are not the same
   Actual   (dialog.count()): 0
   Expected (1)             : 1
   Loc: [../work-fix1/test/selection/test-selection.cpp(864)]
FAIL!  : TestSelection::deleteCleanup_refusesToplevelAlone() Compared values are not the same
   Actual   (dialog.count()): 0
   Expected (1)             : 1
   Loc: [../work-fix1/test/selection/test-selection.cpp(889)]
FAIL!  : TestSelection::assumeDeletedCleanup_refusesToplevelAlone() Compared values are not the same
   Actual   (dialog.count()): 0
   Expected (1)             : 1
   Loc: [../work-fix1/test/selection/test-selection.cpp(909)]
FAIL!  : TestSelection::refreshThisCleanup_allowsToplevelAlone_refusesWithOthers() Compared values are not the same
   Actual   (dialog.count()): 0
   Expected (1)             : 1
   Loc: [../work-fix1/test/selection/test-selection.cpp(948)]
PASS   : TestSelection::noRefreshCleanup_allowsToplevelInMultiSelection()
PASS   : TestSelection::deleteCleanup_runsWithoutToplevel()
FAIL!  : TestSelection::moveToTrash_refusesToplevel() Compared values are not the same
   Actual   (dialog.count()): 0
   Expected (1)             : 1
   Loc: [../work-fix1/test/selection/test-selection.cpp(1011)]
```

## Fix 3: confirmation dialog

Tests `confirmation_*`. Against the tree with fixes 1 and 2 only (commit
"CleanupCollection, MainWindow: refuse destructive actions on the scan
root"), Qt 6.4.2, one test per process. Six fail on the behaviour: directories
missing from the dialog, Enter confirming the cleanup, no counts, no details
pane, no nested-selection note, no scan root marker. The Yes control passes.

```
===== safer with fix 1 + fix 2 only (old confirmation dialog), Qt 6.4.2, fix-3 tests one per process =====
FAIL!  : TestSelection::confirmation_listsAllDirectoriesAndDefaultsToNo() 'text.contains( find( rel )->url().toHtmlEscaped() )' returned FALSE. (E_dirs/d01 missing in:
   Loc: [../work-fix2/test/selection/test-selection.cpp(1098)]
FAIL!  : TestSelection::confirmation_enterDoesNotConfirm() '! QFile::exists( marker( "C_target/f04" ) )' returned FALSE. (Enter confirmed the cleanup)
   Loc: [../work-fix2/test/selection/test-selection.cpp(1132)]
FAIL!  : TestSelection::confirmation_filesAreCutWithACount() 'text.contains( "10 files" )' returned FALSE. (<html><h3>Test Confirm Files</h3> for:<br>
   Loc: [../work-fix2/test/selection/test-selection.cpp(1150)]
FAIL!  : TestSelection::confirmation_longDirectoryListGoesToDetails() 'text.contains( "30 directories" )' returned FALSE. (<html><h3>Test Confirm Many Dirs</h3> for:<br>
   Loc: [../work-fix2/test/selection/test-selection.cpp(1174)]
FAIL!  : TestSelection::confirmation_flagsNestedSelection() 'text.contains( "1 directory and 1 file" )' returned FALSE. (<html><h3>Test Confirm Nested</h3> for:<br>
   Loc: [../work-fix2/test/selection/test-selection.cpp(1200)]
FAIL!  : TestSelection::confirmation_marksScanRoot() 'text.contains( "SCAN ROOT" )' returned FALSE. (<html><h3>Test Confirm Root</h3>for <b>directory</b> /tmp/QDirStat-NJVJcv/root<br>)
   Loc: [../work-fix2/test/selection/test-selection.cpp(1222)]
PASS   : TestSelection::confirmation_yesRunsCleanup()
```

## How to regenerate

`evidence.sh` in this directory runs every test in its own process against
a binary built from an UNFIXED tree and checks each result against the
expectation table inside it (FAIL for the tests that cover a fix, PASS for
the controls, recorded-only for the plain-click reproductions on Qt >= 6.8).
It exits non-zero on any unexpected result. CI runs it against `master` on
every push (job `evidence-on-master`).

From a clone with both branches:

```bash
# safer: everything must pass
qmake6 && make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen ./test/selection/test-selection

# master: same test sources, upstream src/, expectations checked by evidence.sh
git worktree add ../qdirstat-master master
cp -a test/selection ../qdirstat-master/test/
mkdir -p ../build-master-test && cd ../build-master-test
qmake6 ../qdirstat-master/test/selection/selection.pro && make -j"$(nproc)"
sh ../qdirstat-master/test/selection/evidence.sh ./test-selection
```

For an intermediate commit (e.g. the tree with fix 1 only), check that
commit out instead of `master`; tests for fixes already present in it will
then show as UNEXPECTED, which is correct for that tree and only means the
table applies to `master`.

The `master` run must show the reproduction tests failing; if it ever
passes on a Qt version where it is expected to fail, the test has stopped
testing the mechanism and needs attention before the fix is trusted.
