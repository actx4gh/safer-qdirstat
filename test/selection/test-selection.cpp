/*
 *   File name: test-selection.cpp
 *   Summary:	QTest regression tests for the selection-safety fixes
 *		of the safer-qdirstat fork (upstream issue #329)
 *   License:	GPL V2 - See file LICENSE for details.
 *
 * The tests drive the real MainWindow with the real DirTreeView, TreemapView
 * and SelectionModel on a small fixture directory, using QTest input
 * synthesis. Run with QT_QPA_PLATFORM=offscreen.
 *
 * The two tests named *_filesOnly and *_shiftHeldDuringTreemapClick are the
 * reproduction of upstream issue #329. They must FAIL on upstream master
 * (Qt < 6.8: both; Qt >= 6.8: the second one) and PASS on 'safer'.
 */

#include <QtTest>
#include <QApplication>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QMessageBox>
#include <QAbstractButton>

#include "MainWindow.h"
#include "QDirStatApp.h"
#include "DirTreeView.h"
#include "TreemapView.h"
#include "TreemapTile.h"
#include "SelectionModel.h"
#include "DirTree.h"
#include "DirTreeModel.h"
#include "FileInfo.h"
#include "FileInfoSet.h"
#include "Logger.h"
#include "CleanupCollection.h"
#include "Cleanup.h"

using namespace QDirStat;

static const QString ROOT = "<scan root>";


// QTest macros return from the calling function on failure; a helper that
// uses them must therefore be void, and its caller has to check for a
// failure and return as well.
#define CHECKED( call )					\
    do {						\
	call;						\
	if ( QTest::currentTestFailed() )		\
	    return;					\
    } while ( 0 )

// Compare the current selection with 'expected' and show both lists in full
// on failure (QCOMPARE on lists only reports the sizes).
#define COMPARE_SELECTION( expected )						\
    do {									\
	QStringList actual_   = selected();					\
	QStringList expected_ = ( expected );					\
	QVERIFY2( actual_ == expected_,						\
		  qPrintable( QString( "\n   selected: [%1]\n   expected: [%2]" )	\
			      .arg( actual_.join( ", " ), expected_.join( ", " ) ) ) ); \
    } while ( 0 )


/**
 * Close any popup (e.g. a context menu whose exec() would block the test)
 * as soon as it opens.
 **/
class PopupCloser: public QObject
{
    Q_OBJECT

public:
    PopupCloser()
    {
	connect( &_timer, &QTimer::timeout, this, &PopupCloser::closePopup );
	_timer.start( 20 );
    }

    int closed() const { return _closed; }

private slots:
    void closePopup()
    {
	QWidget * popup = QApplication::activePopupWidget();

	if ( popup )
	{
	    popup->close();
	    ++_closed;
	}
    }

private:
    QTimer _timer;
    int    _closed = 0;
};


/**
 * Record and dismiss modal QMessageBoxes (confirmation, refusal) as soon as
 * they open, so a blocking exec() in the code under test returns. 'answer'
 * is the standard button to press; NoButton means Escape / the escape button.
 **/
class DialogCatcher: public QObject
{
    Q_OBJECT

public:
    /** Answer every message box with 'key' (Escape by default). **/
    explicit DialogCatcher( Qt::Key key = Qt::Key_Escape ):
	_key( key ),
	_answer( QMessageBox::NoButton )
    {
	start();
    }

    /** Answer every message box by clicking its 'answer' button. **/
    explicit DialogCatcher( QMessageBox::StandardButton answer ):
	_key( Qt::Key_unknown ),
	_answer( answer )
    {
	start();
    }

    int	    count()	   const { return _count;	 }
    QString objectName()   const { return _objectName; }
    QString text()	   const { return _text;	 }
    QString detailedText() const { return _detailedText; }
    QMessageBox::Icon	        icon()	        const { return _icon;	      }
    QMessageBox::StandardButton defaultButton() const { return _defaultButton; }

private slots:
    void poll()
    {
	QMessageBox * box = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );

	if ( ! box )
	{
	    _last = 0;
	    return;
	}

	if ( box == _last )
	    return;

	_last	      = box;
	++_count;
	_objectName   = box->objectName();
	_text	      = box->text();
	_detailedText = box->detailedText();
	_icon	      = box->icon();

	QAbstractButton * def = box->defaultButton();
	_defaultButton = def ? box->standardButton( def ) : QMessageBox::NoButton;

	QAbstractButton * button = _answer != QMessageBox::NoButton ? box->button( _answer ) : 0;

	if ( button )
	    button->click();
	else
	    QTest::keyClick( box, _key );	// what the user would press
    }

private:
    void start()
    {
	connect( &_timer, &QTimer::timeout, this, &DialogCatcher::poll );
	_timer.start( 20 );
    }

    QTimer			_timer;
    Qt::Key			_key;
    QMessageBox::StandardButton _answer;
    QMessageBox *		_last = 0;
    int				_count = 0;
    QString			_objectName;
    QString			_text;
    QString			_detailedText;
    QMessageBox::Icon		_icon = QMessageBox::NoIcon;
    QMessageBox::StandardButton _defaultButton = QMessageBox::NoButton;
};


class TestSelection: public QObject
{
    Q_OBJECT

private slots:

    void initTestCase();
    void cleanupTestCase();
    void init();

    // Reproduction of upstream issue #329 (must fail on master)
    void shiftClickAfterTreemapClick_filesOnly();
    void shiftClickAfterTreemapClick_shiftHeldDuringTreemapClick();
    void shiftDragAfterTreemapClick_filesOnly();
    void shiftClickOnIndentationAfterTreemapClick_filesOnly();
    void shiftClickAfterProgrammaticCurrent_filesOnly();

    // Stock behaviour that must be preserved
    void shiftDownRepeated_growsFromAnchor();
    void shiftDrag_growsAndShrinksFromAnchor();
    void ctrlClickThenShiftClick_keepsCommittedRange();
    void shiftClickWithHiddenAnchor_isPlainClick();
    void shiftClickOnBranchIndicator_togglesExpansion();

    // Scan root guard (fix 2)
    void deleteCleanup_refusesToplevelInMultiSelection();
    void deleteCleanup_refusesToplevelAlone();
    void assumeDeletedCleanup_refusesToplevelAlone();
    void refreshThisCleanup_allowsToplevelAlone_refusesWithOthers();
    void noRefreshCleanup_allowsToplevelInMultiSelection();
    void deleteCleanup_runsWithoutToplevel();
    void moveToTrash_refusesToplevel();

    // Confirmation dialog (fix 3)
    void confirmation_listsAllDirectoriesAndDefaultsToNo();
    void confirmation_enterDoesNotConfirm();
    void confirmation_filesAreCutWithACount();
    void confirmation_longDirectoryListGoesToDetails();
    void confirmation_flagsNestedSelection();
    void confirmation_marksScanRoot();
    void confirmation_yesRunsCleanup();


private:

    // Fixture paths are relative to the scan root; "" is the scan root itself
    // on input, and ROOT is how it is reported in a selection.

    QString	  absPath( const QString & relPath ) const;
    QString	  relPath( const QString & absPath ) const;
    FileInfo *	  find( const QString & relPath ) const;
    QModelIndex	  idx( const QString & relPath, int col = 0 ) const;

    QStringList	  selected() const;		// sorted relative paths
    QStringList	  files( int from, int to ) const;	// "C_target/fNN" for from..to
    QStringList	  dirs( int from, int to ) const;	// "E_dirs/dNN" for from..to
    QStringList	  sorted( QStringList list ) const;

    void	  expandFixture();
    void	  treeClick( const QString & relPath, Qt::MouseButton button,
			     Qt::KeyboardModifiers mods = Qt::NoModifier );
    void	  treeShiftPress( const QString & relPath );
    void	  treeShiftMove( const QString & relPath );
    void	  treeShiftRelease( const QString & relPath );
    void	  treemapClick( const QString & relPath, Qt::KeyboardModifiers mods = Qt::NoModifier );
    void	  refreshSelectedAndWait();
    void	  scrollToAndRequireToplevelOffscreen( const QString & relPath );

    /**
     * Steps 1 to 3 of the issue: right-click the toplevel row, "Refresh
     * Selected", then make 'relPath' current via the treemap (optionally
     * with Shift held). Leaves the toplevel row current-then-replaced, i.e.
     * exactly the state in which the stale anchor bites.
     **/
    void	  arrangeStaleAnchor( const QString & treemapItem, Qt::KeyboardModifiers treemapMods = Qt::NoModifier );

    /**
     * Add a cleanup to the test collection whose command drops a marker
     * file named after the item into _markerDir, so "was it executed" is a
     * file existence check that does not touch the fixture.
     **/
    Cleanup *	  addMarkerCleanup( const QString & title, Cleanup::RefreshPolicy policy );
    QString	  marker( const QString & relPath ) const;	// marker file for a fixture item
    void	  select( const QStringList & relPaths );	// via SelectionModel::setSelectedItems()
    void	  waitForRefresh( QSignalSpy & finished );	// a cleanup's Refresher re-read

    QTemporaryDir   _tmp;
    QString	    _markerDir;
    CleanupCollection * _cleanups = 0;
    QString	    _root;
    MainWindow *    _win     = 0;
    DirTreeView *   _tree    = 0;
    TreemapView *   _treemap = 0;
};


//
// Fixture
//
// root/
//   A_dir/a           1200 KiB
//   B_dir/b           1000 KiB
//   C_target/f01..f12  130 KiB .. 20 KiB, 10 KiB steps (f01 largest)
//   E_dirs/d01..d30/x    4 KiB each (confirmation dialog tests only)
//   D_dir/g1, g2         8 KiB, 4 KiB
//
// QDirStat sorts by allocated size, descending, after every read
// (MainWindow::idleDisplay()), so the row order with C_target expanded is
//
//   root, A_dir, B_dir, C_target, f01 .. f12, E_dirs, D_dir
//
// i.e. two collapsed sibling directories between the toplevel and the
// clicked files, as in the issue. Sizes are multiples of 4 KiB apart so
// block rounding cannot create ties.
//

static bool writeFile( const QString & path, qint64 size )
{
    QFile file( path );

    if ( ! file.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
	return false;

    const QByteArray chunk( 4096, 'x' );

    while ( size > 0 )
    {
	qint64 n = qMin<qint64>( size, chunk.size() );

	if ( file.write( chunk.constData(), n ) != n )
	    return false;

	size -= n;
    }

    return true;
}


void TestSelection::initTestCase()
{
    QVERIFY( _tmp.isValid() );
    _root = QDir( _tmp.path() ).canonicalPath() + "/root";

    QDir dir;
    QVERIFY( dir.mkpath( _root + "/A_dir"    ) );
    QVERIFY( dir.mkpath( _root + "/B_dir"    ) );
    QVERIFY( dir.mkpath( _root + "/C_target" ) );
    QVERIFY( dir.mkpath( _root + "/D_dir"    ) );

    QVERIFY( writeFile( _root + "/A_dir/a",  1200 * 1024 ) );
    QVERIFY( writeFile( _root + "/B_dir/b",  1000 * 1024 ) );
    QVERIFY( writeFile( _root + "/D_dir/g1",    8 * 1024 ) );
    QVERIFY( writeFile( _root + "/D_dir/g2",    4 * 1024 ) );

    for ( int i = 1; i <= 12; ++i )
    {
	QString name = QString( "%1/C_target/f%2" ).arg( _root ).arg( i, 2, 10, QChar( '0' ) );
	QVERIFY( writeFile( name, ( 14 - i ) * 10 * 1024 ) );
    }

    // 30 small directories for the confirmation dialog tests; E_dirs stays
    // collapsed in the Shift tests. ~244 KiB allocated: sorts between
    // C_target and D_dir.
    for ( int i = 1; i <= 30; ++i )
    {
	QString d = QString( "%1/E_dirs/d%2" ).arg( _root ).arg( i, 2, 10, QChar( '0' ) );
	QVERIFY( dir.mkpath( d ) );
	QVERIFY( writeFile( d + "/x", 4 * 1024 ) );
    }

    _markerDir = QDir( _tmp.path() ).canonicalPath() + "/markers";
    QVERIFY( dir.mkpath( _markerDir ) );

    _win = new MainWindow();
    QVERIFY( _win );

    // A separate collection for the test cleanups so the application's own
    // menus stay as they are. Its cleanups are enabled/disabled from the
    // selection like the real ones (CleanupCollection::updateActions()).
    _cleanups = new CleanupCollection( app()->selectionModel(), _win );
    QVERIFY( _cleanups );

    _tree    = _win->findChild<DirTreeView *>( "dirTreeView" );
    _treemap = _win->findChild<TreemapView *>( "treemapView" );
    QVERIFY( _tree );
    QVERIFY( _treemap );

    // Small window: only a few tree rows visible, so the toplevel row is
    // scrolled out of view once the end of C_target is current (the fixture
    // has 18 rows with C_target expanded). The exact count is checked
    // below, after the tree is filled.
    _win->resize( 900, 440 );
    _win->show();
    QVERIFY( QTest::qWaitForWindowExposed( _win ) );

    QSignalSpy finished( app()->dirTree(), SIGNAL( finished() ) );
    _win->openDir( _root );
    QTRY_VERIFY_WITH_TIMEOUT( finished.count() >= 1, 15000 );

    QVERIFY( find( "" ) );
    QVERIFY( find( "C_target/f01" ) );
    QVERIFY( find( "C_target/f12" ) );
    QVERIFY( find( "D_dir/g1" ) );

    // Row order as documented above
    CHECKED( expandFixture() );
    QCOMPARE( _tree->indexBelow( idx( "" )        ), idx( "A_dir"    ) );
    QCOMPARE( _tree->indexBelow( idx( "A_dir" )   ), idx( "B_dir"    ) );
    QCOMPARE( _tree->indexBelow( idx( "B_dir" )   ), idx( "C_target" ) );
    QCOMPARE( _tree->indexBelow( idx( "C_target") ), idx( "C_target/f01" ) );
    QCOMPARE( _tree->indexBelow( idx( "C_target/f11" ) ), idx( "C_target/f12" ) );
    QCOMPARE( _tree->indexBelow( idx( "C_target/f12" ) ), idx( "E_dirs" ) );
    QCOMPARE( _tree->indexBelow( idx( "E_dirs" ) ), idx( "D_dir" ) );
    QVERIFY( find( "E_dirs/d30/x" ) );

    // Between 5 and 12 rows visible: enough for the drag tests (5 rows in
    // view at once), few enough that the toplevel row scrolls out of view
    // with f12 at the bottom (f12 is row 15 of 0..17).
    int rowHeight   = _tree->visualRect( idx( "" ) ).height();
    int visibleRows = rowHeight > 0 ? _tree->viewport()->height() / rowHeight : 0;
    QVERIFY2( visibleRows >= 5 && visibleRows <= 12,
	      qPrintable( QString( "%1 rows visible (viewport %2 px, row %3 px); adjust the window size" )
			  .arg( visibleRows ).arg( _tree->viewport()->height() ).arg( rowHeight ) ) );

    // Treemap built and the largest file in C_target has a clickable tile
    QTRY_VERIFY_WITH_TIMEOUT( _treemap->findTile( find( "C_target/f01" ) ) != 0, 5000 );
}


void TestSelection::cleanupTestCase()
{
    delete _win;
    _win = 0;
}


void TestSelection::init()
{
    // Nothing selected, no current item, toplevel and C_target expanded,
    // scrolled to the top.

    app()->selectionModel()->clearSelection();
    app()->selectionModel()->clearCurrentIndex();
    CHECKED( expandFixture() );
    _tree->scrollToTop();

    QVERIFY( selected().isEmpty() );
    QVERIFY( ! _tree->currentIndex().isValid() );
}


//
// Helpers
//

QString TestSelection::absPath( const QString & rel ) const
{
    return rel.isEmpty() ? _root : _root + "/" + rel;
}


QString TestSelection::relPath( const QString & abs ) const
{
    if ( abs == _root )
	return ROOT;

    if ( abs.startsWith( _root + "/" ) )
	return abs.mid( _root.size() + 1 );

    return abs;    // outside the fixture: report as is
}


FileInfo * TestSelection::find( const QString & rel ) const
{
    return app()->dirTree()->locate( absPath( rel ), true );
}


QModelIndex TestSelection::idx( const QString & rel, int col ) const
{
    FileInfo * item = find( rel );

    return item ? app()->dirTreeModel()->modelIndex( item, col ) : QModelIndex();
}


QStringList TestSelection::selected() const
{
    QStringList result;

    foreach ( FileInfo * item, app()->selectionModel()->selectedItems() )
	result << relPath( item->path() );

    return sorted( result );
}


QStringList TestSelection::files( int from, int to ) const
{
    QStringList result;

    for ( int i = from; i <= to; ++i )
	result << QString( "C_target/f%1" ).arg( i, 2, 10, QChar( '0' ) );

    return sorted( result );
}


QStringList TestSelection::dirs( int from, int to ) const
{
    QStringList result;

    for ( int i = from; i <= to; ++i )
	result << QString( "E_dirs/d%1" ).arg( i, 2, 10, QChar( '0' ) );

    return sorted( result );
}


QStringList TestSelection::sorted( QStringList list ) const
{
    list.sort();
    return list;
}


void TestSelection::expandFixture()
{
    // FileInfo objects are recreated by a refresh, so always look them up.
    _tree->collapseAll();
    QVERIFY( find( "" ) );
    QVERIFY( find( "C_target" ) );
    _tree->setExpanded( find( "" ), true );
    _tree->setExpanded( find( "C_target" ), true );
    QVERIFY( _tree->isExpanded( idx( "" ) ) );
    QVERIFY( _tree->isExpanded( idx( "C_target" ) ) );
    QVERIFY( ! _tree->isExpanded( idx( "D_dir" ) ) );
}


void TestSelection::treeClick( const QString & rel, Qt::MouseButton button, Qt::KeyboardModifiers mods )
{
    QModelIndex index = idx( rel );
    QVERIFY2( index.isValid(), qPrintable( rel ) );

    _tree->scrollTo( index );
    QRect rect = _tree->visualRect( index );
    QVERIFY2( ! rect.isEmpty(), qPrintable( rel ) );

    QTest::mouseClick( _tree->viewport(), button, mods, rect.center() );
}


void TestSelection::treeShiftPress( const QString & rel )
{
    QModelIndex index = idx( rel );
    QVERIFY2( index.isValid(), qPrintable( rel ) );
    _tree->scrollTo( index );	// no-op if already visible
    QRect rect = _tree->visualRect( index );
    QVERIFY( ! rect.isEmpty() );
    QTest::mousePress( _tree->viewport(), Qt::LeftButton, Qt::ShiftModifier, rect.center() );
}


void TestSelection::treeShiftMove( const QString & rel )
{
    // No scrollTo() here: the pointer moves over rows that are visible.
    QModelIndex index = idx( rel );
    QVERIFY2( index.isValid(), qPrintable( rel ) );
    QRect rect = _tree->visualRect( index );
    QVERIFY( ! rect.isEmpty() );
    QVERIFY2( _tree->viewport()->rect().contains( rect.center() ), qPrintable( rel ) );

    // QTest::mouseMove() has no modifier parameter; QTest::mouseEvent() does.
    QTest::mouseEvent( QTest::MouseMove, _tree->viewport(), Qt::NoButton, Qt::ShiftModifier, rect.center() );
}


void TestSelection::treeShiftRelease( const QString & rel )
{
    QModelIndex index = idx( rel );
    QVERIFY2( index.isValid(), qPrintable( rel ) );
    QRect rect = _tree->visualRect( index );
    QVERIFY( ! rect.isEmpty() );
    QTest::mouseRelease( _tree->viewport(), Qt::LeftButton, Qt::ShiftModifier, rect.center() );
}


void TestSelection::treemapClick( const QString & rel, Qt::KeyboardModifiers mods )
{
    FileInfo * item = find( rel );
    QVERIFY2( item, qPrintable( rel ) );

    // The treemap is rebuilt with a delay after each read.
    TreemapTile * tile = 0;
    QTRY_VERIFY_WITH_TIMEOUT( ( tile = _treemap->findTile( item ) ) != 0, 5000 );

    QPoint pos = _treemap->mapFromScene( tile->sceneBoundingRect().center() );
    QVERIFY2( _treemap->viewport()->rect().contains( pos ), qPrintable( rel ) );

    QTest::mouseClick( _treemap->viewport(), Qt::LeftButton, mods, pos );
}


void TestSelection::refreshSelectedAndWait()
{
    QAction * action = _win->findChild<QAction *>( "actionRefreshSelected" );
    QVERIFY( action );

    QSignalSpy finished( app()->dirTree(), SIGNAL( finished() ) );
    action->trigger();
    QTRY_VERIFY_WITH_TIMEOUT( finished.count() >= 1, 15000 );

    // MainWindow::applyFutureSelection() re-selects the refreshed item
    QTRY_COMPARE( selected(), QStringList() << ROOT );
    QCOMPARE( _tree->currentIndex().sibling( _tree->currentIndex().row(), 0 ), idx( "" ) );
}


void TestSelection::scrollToAndRequireToplevelOffscreen( const QString & rel )
{
    _tree->scrollTo( idx( rel ) );
    QRect toplevelRect = _tree->visualRect( idx( "" ) );

    // Expanded, so the rect is non-empty; above the viewport, so bottom < 0.
    QVERIFY( ! toplevelRect.isEmpty() );
    QVERIFY2( toplevelRect.bottom() < 0,
	      qPrintable( QString( "toplevel row still visible (bottom=%1); window too tall for this fixture" )
			  .arg( toplevelRect.bottom() ) ) );
}


void TestSelection::arrangeStaleAnchor( const QString & treemapItem, Qt::KeyboardModifiers treemapMods )
{
    // Step 1: right-click the toplevel row. QAbstractItemView::mousePressEvent()
    // sets its private Shift anchor to that row (no Shift, no Ctrl, so the
    // selection command has no 'Current' flag). A context menu, if one
    // opens, is closed immediately; it plays no role in the mechanism.
    {
	PopupCloser closer;
	CHECKED( treeClick( "", Qt::RightButton ) );
	COMPARE_SELECTION( QStringList() << ROOT );
    }

    // Step 2: Refresh Selected. The toplevel's own persistent index
    // survives the subtree refresh (DirTreeModel::invalidatePersistent()
    // skips it), so the private anchor stays valid, and
    // applyFutureSelection() makes the toplevel current and selected again.
    CHECKED( refreshSelectedAndWait() );
    CHECKED( expandFixture() );		// C_target is a new object after the refresh

    // Step 3: click a tile in the treemap. The current item moves through
    // SelectionModel::setCurrentItem(), which never updates the private
    // anchor.
    CHECKED( treemapClick( treemapItem, treemapMods ) );
    COMPARE_SELECTION( QStringList() << treemapItem );
    QCOMPARE( _tree->currentIndex().sibling( _tree->currentIndex().row(), 0 ), idx( treemapItem ) );
}


Cleanup * TestSelection::addMarkerCleanup( const QString & title, Cleanup::RefreshPolicy policy )
{
    // %p is expanded single-quoted by Cleanup::expandVariables(), so the
    // basename is taken in the shell.
    QString command = QString( "touch '%1/'$(basename %p)" ).arg( _markerDir );

    Cleanup * cleanup = new Cleanup( command, title, _cleanups );
    cleanup->setActive( true );
    cleanup->setWorksForDir( true );
    cleanup->setWorksForFile( true );
    cleanup->setWorksForDotEntry( false );
    cleanup->setAskForConfirmation( false );
    cleanup->setRefreshPolicy( policy );
    cleanup->setOutputWindowPolicy( Cleanup::ShowNever );
    cleanup->setOutputWindowAutoClose( true );
    _cleanups->add( cleanup );

    return cleanup;
}


QString TestSelection::marker( const QString & rel ) const
{
    QString name = rel.isEmpty() ? QString( "root" ) : rel.section( '/', -1 );

    return _markerDir + "/" + name;
}


void TestSelection::select( const QStringList & relPaths )
{
    FileInfoSet items;

    foreach ( const QString & rel, relPaths )
    {
	FileInfo * item = find( rel );
	QVERIFY2( item, qPrintable( rel ) );
	items << item;
    }

    app()->selectionModel()->setSelectedItems( items );

    QStringList expected = relPaths;

    for ( QString & rel : expected )
	if ( rel.isEmpty() )
	    rel = ROOT;

    COMPARE_SELECTION( sorted( expected ) );
}


void TestSelection::waitForRefresh( QSignalSpy & finished )
{
    // Cleanup -> QProcess -> OutputWindow::lastProcessFinished() ->
    // Refresher::refresh() -> DirTree re-read -> finished()
    QTRY_VERIFY_WITH_TIMEOUT( finished.count() >= 1, 15000 );
    QTRY_VERIFY( ! app()->dirTree()->isBusy() );
}


//
// Tests: reproduction of upstream issue #329
//

void TestSelection::shiftClickAfterTreemapClick_filesOnly()
{
    CHECKED( arrangeStaleAnchor( "C_target/f01" ) );

    // Step 4: Shift-click the last file in the same directory, with the
    // toplevel row scrolled out of view.
    CHECKED( scrollToAndRequireToplevelOffscreen( "C_target/f12" ) );
    CHECKED( treeClick( "C_target/f12", Qt::LeftButton, Qt::ShiftModifier ) );

    // Exactly f01 .. f12: no toplevel, no sibling directories, no C_target.
    COMPARE_SELECTION( files( 1, 12 ) );
}


void TestSelection::shiftClickAfterTreemapClick_shiftHeldDuringTreemapClick()
{
    // Same as above, but Shift is already held during the treemap click.
    // On Qt >= 6.8 QAbstractItemView::currentChanged() re-syncs its anchor
    // only if QGuiApplication::keyboardModifiers() reports no Shift; QTest
    // events are spontaneous, so their modifiers land in that global state.
    CHECKED( arrangeStaleAnchor( "C_target/f01", Qt::ShiftModifier ) );

    CHECKED( scrollToAndRequireToplevelOffscreen( "C_target/f12" ) );
    CHECKED( treeClick( "C_target/f12", Qt::LeftButton, Qt::ShiftModifier ) );

    COMPARE_SELECTION( files( 1, 12 ) );
}


void TestSelection::shiftDragAfterTreemapClick_filesOnly()
{
    // Shift + press, pointer motion, release. QAbstractItemView builds a
    // drag selection from its private press position, which only non-Shift
    // presses update: the same stale anchor in pixel form.
    CHECKED( arrangeStaleAnchor( "C_target/f01" ) );

    CHECKED( scrollToAndRequireToplevelOffscreen( "C_target/f12" ) );
    CHECKED( treeShiftPress( "C_target/f10" ) );
    CHECKED( treeShiftMove( "C_target/f11" ) );
    CHECKED( treeShiftMove( "C_target/f12" ) );
    CHECKED( treeShiftRelease( "C_target/f12" ) );

    COMPARE_SELECTION( files( 1, 12 ) );
}


void TestSelection::shiftClickOnIndentationAfterTreemapClick_filesOnly()
{
    // Shift-click in the indentation area of a file row, where a directory
    // row would have its branch indicator. Files have none, so this is a
    // selection click for QTreeView and must be one for the fix as well.
    CHECKED( arrangeStaleAnchor( "C_target/f01" ) );
    CHECKED( scrollToAndRequireToplevelOffscreen( "C_target/f12" ) );

    QRect  rect = _tree->visualRect( idx( "C_target/f12" ) );
    QPoint pos( rect.left() - _tree->indentation() / 2, rect.center().y() );
    QVERIFY( _tree->viewport()->rect().contains( pos ) );
    QCOMPARE( _tree->indexAt( pos ).sibling( _tree->indexAt( pos ).row(), 0 ), idx( "C_target/f12" ) );

    QTest::mouseClick( _tree->viewport(), Qt::LeftButton, Qt::ShiftModifier, pos );

    COMPARE_SELECTION( files( 1, 12 ) );
}


void TestSelection::shiftClickAfterProgrammaticCurrent_filesOnly()
{
    // Every other caller of SelectionModel::setCurrentItem() (locate
    // windows, breadcrumbs, future selection) has the same exposure as the
    // treemap. Right-click the toplevel to set the private anchor, then move
    // current programmatically.
    {
	PopupCloser closer;
	CHECKED( treeClick( "", Qt::RightButton ) );
    }

    app()->selectionModel()->setCurrentItem( find( "C_target/f02" ), true );
    COMPARE_SELECTION( QStringList() << "C_target/f02" );

    CHECKED( scrollToAndRequireToplevelOffscreen( "C_target/f12" ) );
    CHECKED( treeClick( "C_target/f12", Qt::LeftButton, Qt::ShiftModifier ) );

    COMPARE_SELECTION( files( 2, 12 ) );
}


//
// Tests: stock behaviour that must be preserved
//

void TestSelection::shiftDownRepeated_growsFromAnchor()
{
    CHECKED( treeClick( "C_target/f03", Qt::LeftButton ) );
    COMPARE_SELECTION( files( 3, 3 ) );

    for ( int i = 0; i < 3; ++i )
	QTest::keyClick( _tree, Qt::Key_Down, Qt::ShiftModifier );

    COMPARE_SELECTION( files( 3, 6 ) );

    // Shift+Up shrinks the same range
    QTest::keyClick( _tree, Qt::Key_Up, Qt::ShiftModifier );
    COMPARE_SELECTION( files( 3, 5 ) );
}


void TestSelection::shiftDrag_growsAndShrinksFromAnchor()
{
    CHECKED( treeClick( "C_target/f03", Qt::LeftButton ) );

    // The drag moves over f04 .. f07 without scrolling; make sure they are
    // all in the (about five rows tall) viewport.
    _tree->scrollTo( idx( "C_target/f05" ), QAbstractItemView::PositionAtCenter );
    QVERIFY( _tree->viewport()->rect().contains( _tree->visualRect( idx( "C_target/f03" ) ).center() ) );
    QVERIFY( _tree->viewport()->rect().contains( _tree->visualRect( idx( "C_target/f07" ) ).center() ) );

    CHECKED( treeShiftPress( "C_target/f05" ) );
    COMPARE_SELECTION( files( 3, 5 ) );

    CHECKED( treeShiftMove( "C_target/f06" ) );
    COMPARE_SELECTION( files( 3, 6 ) );

    CHECKED( treeShiftMove( "C_target/f07" ) );
    COMPARE_SELECTION( files( 3, 7 ) );

    CHECKED( treeShiftMove( "C_target/f04" ) );
    COMPARE_SELECTION( files( 3, 4 ) );

    CHECKED( treeShiftRelease( "C_target/f04" ) );
    COMPARE_SELECTION( files( 3, 4 ) );
}


void TestSelection::ctrlClickThenShiftClick_keepsCommittedRange()
{
    CHECKED( treeClick( "C_target/f01", Qt::LeftButton ) );
    CHECKED( treeClick( "C_target/f03", Qt::LeftButton, Qt::ShiftModifier ) );
    COMPARE_SELECTION( files( 1, 3 ) );

    CHECKED( treeClick( "C_target/f08", Qt::LeftButton, Qt::ControlModifier ) );
    COMPARE_SELECTION( sorted( files( 1, 3 ) + files( 8, 8 ) ) );

    CHECKED( treeClick( "C_target/f10", Qt::LeftButton, Qt::ShiftModifier ) );
    COMPARE_SELECTION( sorted( files( 1, 3 ) + files( 8, 10 ) ) );
}


void TestSelection::shiftClickWithHiddenAnchor_isPlainClick()
{
    // The anchor (and current item) f02 disappears under a collapsed
    // branch. A Shift-click must not range from a row that is not visible.
    CHECKED( treeClick( "C_target/f02", Qt::LeftButton ) );
    _tree->setExpanded( find( "C_target" ), false );
    QVERIFY( _tree->visualRect( idx( "C_target/f02" ) ).isEmpty() );

    CHECKED( treeClick( "D_dir", Qt::LeftButton, Qt::ShiftModifier ) );

    COMPARE_SELECTION( QStringList() << "D_dir" );
}


void TestSelection::shiftClickOnBranchIndicator_togglesExpansion()
{
    CHECKED( treeClick( "C_target/f02", Qt::LeftButton ) );
    QVERIFY( ! _tree->isExpanded( idx( "D_dir" ) ) );

    _tree->scrollTo( idx( "D_dir" ) );
    QRect  rect = _tree->visualRect( idx( "D_dir" ) );
    QPoint pos( rect.left() - _tree->indentation() / 2, rect.center().y() );
    QVERIFY( _tree->viewport()->rect().contains( pos ) );

    QTest::mouseClick( _tree->viewport(), Qt::LeftButton, Qt::ShiftModifier, pos );

    QVERIFY( _tree->isExpanded( idx( "D_dir" ) ) );
    COMPARE_SELECTION( files( 2, 2 ) );
}


//
// Tests: scan root guard (fix 2)
//
// All cleanups here drop a marker file per item; "nothing executed" is
// "no marker". Every refusal must show the refusal dialog (object name
// "toplevelRefusal"): a cleanup that silently did nothing, e.g. because its
// action was disabled, would otherwise pass these tests for the wrong reason.
//

void TestSelection::deleteCleanup_refusesToplevelInMultiSelection()
{
    Cleanup * cleanup = addMarkerCleanup( "Test Delete", Cleanup::RefreshParent );
    CHECKED( select( QStringList() << "" << "C_target/f01" ) );
    QVERIFY( cleanup->isEnabled() );

    QSignalSpy finished( app()->dirTree(), SIGNAL( finished() ) );
    DialogCatcher dialog;
    cleanup->trigger();

    QCOMPARE( dialog.count(), 1 );
    QCOMPARE( dialog.objectName(), QString( "toplevelRefusal" ) );
    QCOMPARE( dialog.icon(), QMessageBox::Critical );
    QVERIFY2( dialog.text().contains( "Test Delete" ), qPrintable( dialog.text() ) );
    QVERIFY2( dialog.text().contains( find( "" )->url().toHtmlEscaped() ), qPrintable( dialog.text() ) );
    QVERIFY2( dialog.text().contains( "2 items are selected" ), qPrintable( dialog.text() ) );

    QTest::qWait( 300 );
    QVERIFY( ! QFile::exists( marker( "" ) ) );
    QVERIFY( ! QFile::exists( marker( "C_target/f01" ) ) );
    QCOMPARE( finished.count(), 0 );	// no prepareRefresh() to the root, no re-read
    COMPARE_SELECTION( sorted( QStringList() << ROOT << "C_target/f01" ) );
}


void TestSelection::deleteCleanup_refusesToplevelAlone()
{
    Cleanup * cleanup = addMarkerCleanup( "Test Delete Alone", Cleanup::RefreshParent );
    CHECKED( select( QStringList() << "" ) );
    QVERIFY( cleanup->isEnabled() );

    QSignalSpy finished( app()->dirTree(), SIGNAL( finished() ) );
    DialogCatcher dialog;
    cleanup->trigger();

    QCOMPARE( dialog.count(), 1 );
    QCOMPARE( dialog.objectName(), QString( "toplevelRefusal" ) );
    QVERIFY2( dialog.text().contains( "Nothing was executed" ), qPrintable( dialog.text() ) );

    QTest::qWait( 300 );
    QVERIFY( ! QFile::exists( marker( "" ) ) );
    QCOMPARE( finished.count(), 0 );
}


void TestSelection::assumeDeletedCleanup_refusesToplevelAlone()
{
    // AssumeDeleted would call DirTree::deleteSubtree() on the scan root.
    Cleanup * cleanup = addMarkerCleanup( "Test Assume Deleted", Cleanup::AssumeDeleted );
    CHECKED( select( QStringList() << "" ) );
    QVERIFY( cleanup->isEnabled() );

    DialogCatcher dialog;
    cleanup->trigger();

    QCOMPARE( dialog.count(), 1 );
    QCOMPARE( dialog.objectName(), QString( "toplevelRefusal" ) );

    QTest::qWait( 300 );
    QVERIFY( ! QFile::exists( marker( "" ) ) );
    QVERIFY( find( "" ) );			// still in the tree
    QVERIFY( find( "C_target/f12" ) );
}


void TestSelection::refreshThisCleanup_allowsToplevelAlone_refusesWithOthers()
{
    // The one exception: an explicit single-item action on the scan root
    // (e.g. "Clear Directory Contents") runs; it is then refreshed.
    Cleanup * cleanup = addMarkerCleanup( "Test Refresh This", Cleanup::RefreshThis );
    CHECKED( select( QStringList() << "" ) );
    QVERIFY( cleanup->isEnabled() );

    {
	QSignalSpy finished( app()->dirTree(), SIGNAL( finished() ) );
	DialogCatcher dialog;
	cleanup->trigger();

	QTRY_VERIFY_WITH_TIMEOUT( QFile::exists( marker( "" ) ), 5000 );
	QCOMPARE( dialog.count(), 0 );
	CHECKED( waitForRefresh( finished ) );
	QVERIFY( QFile::remove( marker( "" ) ) );
    }

    // Same cleanup, scan root plus one more item: refused.
    CHECKED( expandFixture() );			// the refresh recreated the items
    CHECKED( select( QStringList() << "" << "C_target/f01" ) );
    QVERIFY( cleanup->isEnabled() );

    {
	QSignalSpy finished( app()->dirTree(), SIGNAL( finished() ) );
	DialogCatcher dialog;
	cleanup->trigger();

	QCOMPARE( dialog.count(), 1 );
	QCOMPARE( dialog.objectName(), QString( "toplevelRefusal" ) );

	QTest::qWait( 300 );
	QVERIFY( ! QFile::exists( marker( "" ) ) );
	QVERIFY( ! QFile::exists( marker( "C_target/f01" ) ) );
	QCOMPARE( finished.count(), 0 );
    }
}


void TestSelection::noRefreshCleanup_allowsToplevelInMultiSelection()
{
    // Non-modifying by declaration (open terminal, file manager, ...).
    Cleanup * cleanup = addMarkerCleanup( "Test No Refresh", Cleanup::NoRefresh );
    CHECKED( select( QStringList() << "" << "C_target/f01" ) );
    QVERIFY( cleanup->isEnabled() );

    DialogCatcher dialog;
    cleanup->trigger();

    QTRY_VERIFY_WITH_TIMEOUT( QFile::exists( marker( "" ) ), 5000 );
    QTRY_VERIFY_WITH_TIMEOUT( QFile::exists( marker( "C_target/f01" ) ), 5000 );
    QCOMPARE( dialog.count(), 0 );

    QVERIFY( QFile::remove( marker( "" ) ) );
    QVERIFY( QFile::remove( marker( "C_target/f01" ) ) );
}


void TestSelection::deleteCleanup_runsWithoutToplevel()
{
    // Control: the guard does not get in the way of an ordinary delete.
    Cleanup * cleanup = addMarkerCleanup( "Test Delete Files", Cleanup::RefreshParent );
    CHECKED( select( QStringList() << "C_target/f01" << "C_target/f02" ) );
    QVERIFY( cleanup->isEnabled() );

    QSignalSpy finished( app()->dirTree(), SIGNAL( finished() ) );
    DialogCatcher dialog;
    cleanup->trigger();

    QTRY_VERIFY_WITH_TIMEOUT( QFile::exists( marker( "C_target/f01" ) ), 5000 );
    QTRY_VERIFY_WITH_TIMEOUT( QFile::exists( marker( "C_target/f02" ) ), 5000 );
    QCOMPARE( dialog.count(), 0 );
    CHECKED( waitForRefresh( finished ) );	// RefreshParent re-reads C_target

    QVERIFY( QFile::remove( marker( "C_target/f01" ) ) );
    QVERIFY( QFile::remove( marker( "C_target/f02" ) ) );
}


void TestSelection::moveToTrash_refusesToplevel()
{
    // MainWindow::moveToTrash() is not a Cleanup, has no confirmation
    // dialog, and first checks that the active window is the main window.
    _win->activateWindow();
    QTRY_COMPARE( QApplication::activeWindow(), static_cast<QWidget *>( _win ) );

    CHECKED( select( QStringList() << "" << "C_target/f01" ) );

    DialogCatcher dialog;
    QVERIFY( QMetaObject::invokeMethod( _win, "moveToTrash", Qt::DirectConnection ) );

    QCOMPARE( dialog.count(), 1 );
    QCOMPARE( dialog.objectName(), QString( "toplevelRefusal" ) );
    QVERIFY2( dialog.text().contains( "Move to Trash" ), qPrintable( dialog.text() ) );
    QVERIFY2( dialog.text().contains( find( "" )->url().toHtmlEscaped() ), qPrintable( dialog.text() ) );

    QTest::qWait( 300 );
    QVERIFY( QFile::exists( absPath( "" ) ) );
    QVERIFY( QFile::exists( absPath( "C_target/f01" ) ) );
    QVERIFY( ! app()->dirTree()->isBusy() );
}


//
// Tests: confirmation dialog (fix 3)
//
// The confirmation box has object name "cleanupConfirmation". Every test
// here declines (Escape or Enter) except the last one, so the marker
// command must not run; the tests on the previous tree fail on the content
// assertions (truncated directories, no default button, no nested-selection
// note, no scan root marker) or, for Enter, on the cleanup actually running.
//

void TestSelection::confirmation_listsAllDirectoriesAndDefaultsToNo()
{
    Cleanup * cleanup = addMarkerCleanup( "Test Confirm Dirs", Cleanup::RefreshThis );
    cleanup->setAskForConfirmation( true );
    CHECKED( select( dirs( 1, 10 ) + files( 1, 3 ) ) );
    QVERIFY( cleanup->isEnabled() );

    DialogCatcher dialog;			// Escape
    cleanup->trigger();

    QCOMPARE( dialog.count(), 1 );
    QString text = dialog.text();

    // Every directory is listed (the old dialog cut them to 7)
    foreach ( const QString & rel, dirs( 1, 10 ) )
	QVERIFY2( text.contains( find( rel )->url().toHtmlEscaped() ), qPrintable( rel + " missing in:\n" + text ) );

    foreach ( const QString & rel, files( 1, 3 ) )
	QVERIFY2( text.contains( find( rel )->url().toHtmlEscaped() ), qPrintable( rel + " missing in:\n" + text ) );

    QVERIFY2( text.contains( "Test Confirm Dirs" ), qPrintable( text ) );
    QVERIFY2( text.contains( "10 directories and 3 files" ), qPrintable( text ) );
    QVERIFY2( ! text.contains( "inside that directory" ), qPrintable( text ) );
    QVERIFY( dialog.detailedText().isEmpty() );

    // No is the default (the old dialog had none set, so Enter meant Yes)
    QCOMPARE( dialog.defaultButton(), QMessageBox::No );
    QCOMPARE( dialog.icon(), QMessageBox::Warning );
    QCOMPARE( dialog.objectName(), QString( "cleanupConfirmation" ) );

    QTest::qWait( 300 );
    QVERIFY( ! QFile::exists( marker( "E_dirs/d01" ) ) );
    QVERIFY( ! QFile::exists( marker( "C_target/f01" ) ) );
}


void TestSelection::confirmation_enterDoesNotConfirm()
{
    Cleanup * cleanup = addMarkerCleanup( "Test Confirm Enter", Cleanup::RefreshThis );
    cleanup->setAskForConfirmation( true );
    CHECKED( select( files( 4, 5 ) ) );
    QVERIFY( cleanup->isEnabled() );

    DialogCatcher dialog( Qt::Key_Return );
    cleanup->trigger();

    QCOMPARE( dialog.count(), 1 );

    QTest::qWait( 500 );
    QVERIFY2( ! QFile::exists( marker( "C_target/f04" ) ), "Enter confirmed the cleanup" );
    QVERIFY2( ! QFile::exists( marker( "C_target/f05" ) ), "Enter confirmed the cleanup" );
    QCOMPARE( dialog.defaultButton(), QMessageBox::No );
}


void TestSelection::confirmation_filesAreCutWithACount()
{
    Cleanup * cleanup = addMarkerCleanup( "Test Confirm Files", Cleanup::RefreshThis );
    cleanup->setAskForConfirmation( true );
    CHECKED( select( files( 1, 10 ) ) );
    QVERIFY( cleanup->isEnabled() );

    DialogCatcher dialog;
    cleanup->trigger();

    QCOMPARE( dialog.count(), 1 );
    QString text = dialog.text();
    QVERIFY2( text.contains( "10 files" ), qPrintable( text ) );
    QVERIFY2( text.contains( find( "C_target/f07" )->url().toHtmlEscaped() ), qPrintable( text ) );
    QVERIFY2( ! text.contains( find( "C_target/f08" )->url().toHtmlEscaped() ), qPrintable( text ) );
    QVERIFY2( text.contains( "... 3 more files" ), qPrintable( text ) );

    QTest::qWait( 300 );
    QVERIFY( ! QFile::exists( marker( "C_target/f01" ) ) );
}


void TestSelection::confirmation_longDirectoryListGoesToDetails()
{
    Cleanup * cleanup = addMarkerCleanup( "Test Confirm Many Dirs", Cleanup::RefreshThis );
    cleanup->setAskForConfirmation( true );
    CHECKED( select( dirs( 1, 30 ) ) );
    QVERIFY( cleanup->isEnabled() );

    DialogCatcher dialog;
    cleanup->trigger();

    QCOMPARE( dialog.count(), 1 );
    QString text    = dialog.text();
    QString details = dialog.detailedText();

    QVERIFY2( text.contains( "30 directories" ), qPrintable( text ) );
    QVERIFY2( text.contains( "Show Details" ), qPrintable( text ) );
    QVERIFY2( text.contains( find( "E_dirs/d10" )->url().toHtmlEscaped() ), qPrintable( text ) );
    QVERIFY2( ! text.contains( find( "E_dirs/d11" )->url().toHtmlEscaped() ), qPrintable( text ) );

    foreach ( const QString & rel, dirs( 1, 30 ) )
	QVERIFY2( details.contains( find( rel )->url() ), qPrintable( rel + "\n" + details ) );

    QTest::qWait( 300 );
    QVERIFY( ! QFile::exists( marker( "E_dirs/d01" ) ) );
    QVERIFY( ! QFile::exists( marker( "E_dirs/d30" ) ) );
}


void TestSelection::confirmation_flagsNestedSelection()
{
    Cleanup * cleanup = addMarkerCleanup( "Test Confirm Nested", Cleanup::RefreshThis );
    cleanup->setAskForConfirmation( true );
    CHECKED( select( QStringList() << "C_target" << "C_target/f01" ) );
    QVERIFY( cleanup->isEnabled() );

    DialogCatcher dialog;
    cleanup->trigger();

    QCOMPARE( dialog.count(), 1 );
    QString text = dialog.text();
    QVERIFY2( text.contains( "1 directory and 1 file" ), qPrintable( text ) );
    QVERIFY2( text.contains( "inside that directory" ), qPrintable( text ) );

    QTest::qWait( 300 );
    QVERIFY( ! QFile::exists( marker( "C_target" ) ) );
    QVERIFY( ! QFile::exists( marker( "C_target/f01" ) ) );
}


void TestSelection::confirmation_marksScanRoot()
{
    // Only reachable as the single item of a RefreshThis cleanup (fix 2).
    Cleanup * cleanup = addMarkerCleanup( "Test Confirm Root", Cleanup::RefreshThis );
    cleanup->setAskForConfirmation( true );
    CHECKED( select( QStringList() << "" ) );
    QVERIFY( cleanup->isEnabled() );

    DialogCatcher dialog;
    cleanup->trigger();

    QCOMPARE( dialog.count(), 1 );
    QString text = dialog.text();
    QVERIFY2( text.contains( "SCAN ROOT" ), qPrintable( text ) );
    QVERIFY2( text.contains( find( "" )->url().toHtmlEscaped() ), qPrintable( text ) );
    QCOMPARE( dialog.defaultButton(), QMessageBox::No );
    QCOMPARE( dialog.objectName(), QString( "cleanupConfirmation" ) );

    QTest::qWait( 300 );
    QVERIFY( ! QFile::exists( marker( "" ) ) );
}


void TestSelection::confirmation_yesRunsCleanup()
{
    // Control: Yes still runs the cleanup.
    Cleanup * cleanup = addMarkerCleanup( "Test Confirm Yes", Cleanup::RefreshParent );
    cleanup->setAskForConfirmation( true );
    CHECKED( select( files( 6, 7 ) ) );
    QVERIFY( cleanup->isEnabled() );

    QSignalSpy finished( app()->dirTree(), SIGNAL( finished() ) );
    DialogCatcher dialog( QMessageBox::Yes );
    cleanup->trigger();

    QCOMPARE( dialog.count(), 1 );
    QTRY_VERIFY_WITH_TIMEOUT( QFile::exists( marker( "C_target/f06" ) ), 5000 );
    QTRY_VERIFY_WITH_TIMEOUT( QFile::exists( marker( "C_target/f07" ) ), 5000 );
    CHECKED( waitForRefresh( finished ) );

    QVERIFY( QFile::remove( marker( "C_target/f06" ) ) );
    QVERIFY( QFile::remove( marker( "C_target/f07" ) ) );
}


//
// main
//

int main( int argc, char * argv[] )
{
    // Keep the user's real QDirStat settings out of this.
    QTemporaryDir configDir;
    QStandardPaths::setTestModeEnabled( true );
    QSettings::setPath( QSettings::NativeFormat, QSettings::UserScope, configDir.path() );
    QSettings::setPath( QSettings::IniFormat,    QSettings::UserScope, configDir.path() );

    QCoreApplication::setOrganizationName( "QDirStat" );
    QCoreApplication::setApplicationName ( "QDirStat" );

    QApplication qtApp( argc, argv );
    qtApp.setAttribute( Qt::AA_Use96Dpi, true );

    Logger logger( configDir.path() + "/test-selection.log" );

    TestSelection test;
    QTEST_SET_MAIN_SOURCE_PATH

    return QTest::qExec( &test, argc, argv );
}

#include "test-selection.moc"
