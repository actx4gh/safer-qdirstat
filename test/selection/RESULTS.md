# test/selection results

Evidence that the Shift-anchor regression tests fail on upstream `master` and
pass on `safer`. Regenerate with the commands at the end; keep this file in
sync when the matrix changes.

## Matrix

| Qt    | platform          | master                                   | safer      |
|-------|-------------------|------------------------------------------|------------|
| 6.4.2 | Ubuntu 24.04, offscreen | fix 1 tests: 6 FAIL (5 reproduction + hidden anchor), 6 PASS; fix 2 tests on the fix-1-only tree: 5 FAIL, 2 PASS | 19 PASS |
| 6.8.x | not yet run       | expected: `*_filesOnly` pass (anchor re-sync), `*_shiftHeldDuringTreemapClick` FAIL; fix 2 tests FAIL as on 6.4 | expected 19 PASS |
| 6.10+ | not yet run       | same as 6.8                              | expected 19 PASS |

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

## safer (fix 1 + fix 2), Qt 6.4.2

```
===== safer (fix 1 + fix 2) =====
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
PASS   : TestSelection::cleanupTestCase()
Totals: 19 passed, 0 failed, 0 skipped, 0 blacklisted, 2493ms
********* Finished testing of TestSelection *********
```

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

## How to regenerate

From a clone with both branches:

```bash
# safer
qmake6 && make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen ./test/selection/test-selection

# master: same test sources, upstream src/
git worktree add ../qdirstat-master master
cp -r test/selection ../qdirstat-master/test/
mkdir -p ../build-master-test && cd ../build-master-test
qmake6 ../qdirstat-master/test/selection/selection.pro && make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen ./test-selection
```

For the fix 2 evidence, check out the fix-1 commit instead of `master`
(`git worktree add ../qdirstat-fix1 <hash of "DirTreeView: anchor Shift
ranges...">`), build the same way, and run the fix 2 tests one per process:

```bash
for t in deleteCleanup_refusesToplevelInMultiSelection deleteCleanup_refusesToplevelAlone \
         assumeDeletedCleanup_refusesToplevelAlone refreshThisCleanup_allowsToplevelAlone_refusesWithOthers \
         noRefreshCleanup_allowsToplevelInMultiSelection deleteCleanup_runsWithoutToplevel \
         moveToTrash_refusesToplevel; do
    QT_QPA_PLATFORM=offscreen ./test-selection $t 2>&1 | grep -E '^(PASS|FAIL)' | grep -v -e initTestCase -e cleanupTestCase
done
```

The `master` run must show the reproduction tests failing; if it ever passes
on a Qt version where it is expected to fail, the test has stopped testing
the mechanism and needs attention before the fix is trusted.
