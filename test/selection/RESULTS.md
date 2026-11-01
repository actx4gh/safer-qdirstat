# test/selection results

Evidence that the Shift-anchor regression tests fail on upstream `master` and
pass on `safer`. Regenerate with the commands at the end; keep this file in
sync when the matrix changes.

## Matrix

| Qt    | platform          | master                                   | safer      |
|-------|-------------------|------------------------------------------|------------|
| 6.4.2 | Ubuntu 24.04, offscreen | 6 FAIL (5 reproduction + hidden anchor), 6 PASS | 12 PASS |
| 6.8.x | not yet run       | expected: `*_filesOnly` pass (anchor re-sync), `*_shiftHeldDuringTreemapClick` FAIL | expected 12 PASS |
| 6.10+ | not yet run       | same as 6.8                              | expected 12 PASS |

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

## safer (with "DirTreeView: anchor Shift ranges on the visible current item"), Qt 6.4.2

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
PASS   : TestSelection::cleanupTestCase()
Totals: 12 passed, 0 failed, 0 skipped, 0 blacklisted, 689ms
********* Finished testing of TestSelection *********
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

The `master` run must show the reproduction tests failing; if it ever passes
on a Qt version where it is expected to fail, the test has stopped testing
the mechanism and needs attention before the fix is trusted.
