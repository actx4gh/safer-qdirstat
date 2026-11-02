/*
 *   File name: CleanupCollection.cpp
 *   Summary:	QDirStat classes to reclaim disk space
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <algorithm>

#include <QMenu>
#include <QToolBar>
#include <QMessageBox>

#include "CleanupCollection.h"
#include "Cleanup.h"
#include "StdCleanup.h"
#include "DirTree.h"
#include "DirInfo.h"
#include "Settings.h"
#include "SettingsHelpers.h"
#include "SelectionModel.h"
#include "OutputWindow.h"
#include "Refresher.h"
#include "Logger.h"
#include "Exception.h"

#define MAX_URLS_IN_CONFIRMATION_POPUP 7	// files only; directories are never cut
#define MAX_DIRS_IN_CONFIRMATION_TEXT 25	// more than this: complete list in the details pane
#define DIRS_SHOWN_IF_MORE	       10	// ... and this many in the main text

using namespace QDirStat;


CleanupCollection::CleanupCollection( SelectionModel * selectionModel,
				      QObject	     * parent ):
    QObject( parent ),
    _selectionModel( selectionModel ),
    _listMover( _cleanupList )
{
    readSettings();

    // Just initialize to show the current status in the log;
    // the contents are cached anyway.

    (void) Cleanup::desktopSpecificApps();

    if ( _cleanupList.isEmpty() )
	addStdCleanups();

    updateActions();

    connect( selectionModel, SIGNAL( selectionChanged() ),
	     this,	     SLOT  ( updateActions()	) );
}


CleanupCollection::~CleanupCollection()
{
    writeSettings();
    clear();
}


void CleanupCollection::add( Cleanup * cleanup )
{
    CHECK_PTR( cleanup );
    _cleanupList << cleanup;

    connect( cleanup, SIGNAL( triggered() ),
	     this,    SLOT  ( execute()	  ) );

    updateMenusAndToolBars();
}


void CleanupCollection::remove( Cleanup * cleanup )
{
    int index = indexOf( cleanup );

    if ( index == -1 )
    {
	logError() << "No such cleanup: " << cleanup << endl;
	return;
    }

    _cleanupList.removeAt( index );
    delete cleanup;

    // No need for updateMenusAndToolBars() since QObject/QWidget will take care of
    // deleted actions all by itself.
}


void CleanupCollection::addStdCleanups()
{
    foreach ( Cleanup * cleanup, StdCleanup::stdCleanups( this ) )
    {
	add( cleanup );
    }
}


int CleanupCollection::indexOf( Cleanup * cleanup ) const
{
    int index = _cleanupList.indexOf( cleanup );

    if ( index == -1 )
	logError() << "Cleanup " << cleanup << " is not in this collection" << endl;

    return index;
}


Cleanup * CleanupCollection::at( int index ) const
{
    if ( index >= 0 && index < _cleanupList.size() )
	return _cleanupList.at( index );
    else
	return 0;
}


void CleanupCollection::clear()
{
    qDeleteAll( _cleanupList );
    _cleanupList.clear();

    // No need for updateMenusAndToolBars() since QObject/QWidget will take
    // care of deleted actions all by itself.
}


void CleanupCollection::updateMenusAndToolBars()
{
    updateMenus();
    updateToolBars();
}


void CleanupCollection::updateActions()
{
    FileInfoSet sel = _selectionModel->selectedItems();

    bool dirSelected	  = sel.containsDir();
    bool fileSelected	  = sel.containsFile();
    bool pkgSelected      = sel.containsPkg();
    bool dotEntrySelected = sel.containsDotEntry();
    bool busy		  = sel.containsBusyItem();
    bool treeBusy	  = sel.treeIsBusy();

    foreach ( Cleanup * cleanup, _cleanupList )
    {
	if ( ! cleanup->active() || sel.isEmpty() )
	    cleanup->setEnabled( false );
	else
	{
	    bool enabled = ! busy;

	    if ( treeBusy && cleanup->refreshPolicy() != Cleanup::NoRefresh )
		enabled = false;

	    if ( dirSelected && ! cleanup->worksForDir() )
		enabled = false;

	    if ( dotEntrySelected && ! cleanup->worksForDotEntry() )
		enabled = false;

	    if ( fileSelected && ! cleanup->worksForFile() )
		enabled = false;

            if ( pkgSelected )
                enabled = false;

	    cleanup->setEnabled( enabled );
	}
    }
}


void CleanupCollection::updateMenus()
{
    _menus.removeAll( nullptr ); // Remove QPointers that have become invalid

    foreach ( QMenu * menu, _menus )
    {
	if ( menu )
	{
	    // Remove all Cleanups from this menu

	    foreach ( QAction * action, menu->actions() )
	    {
		Cleanup * cleanup = dynamic_cast<Cleanup *>( action );

		if ( cleanup )
		    menu->removeAction( cleanup );
	    }

	    // Add the current cleanups in the current order
	    addToMenu( menu );
	}
    }
}


void CleanupCollection::updateToolBars()
{
    _toolBars.removeAll( nullptr ); // Remove QPointers that have become invalid

    foreach ( QToolBar * toolBar, _toolBars )
    {
	if ( toolBar )
	{
	    // Remove all Cleanups from this tool bar

	    foreach ( QAction * action, toolBar->actions() )
	    {
		Cleanup * cleanup = dynamic_cast<Cleanup *>( action );

		if ( cleanup )
		    toolBar->removeAction( cleanup );
	    }

	    // Add the current cleanups in the current order
	    addToToolBar( toolBar );
	}
    }
}


void CleanupCollection::execute()
{
    Cleanup * cleanup = qobject_cast<Cleanup *>( sender() );

    if ( ! cleanup )
    {
	logError() << "Wrong sender type: "
		   << sender()->metaObject()->className() << endl;
	return;
    }

    FileInfoSet selection = _selectionModel->selectedItems();

    if ( selection.isEmpty() )
    {
	logWarning() << "Nothing selected" << endl;
	return;
    }

    // Scan root guard (see CleanupCollection.h). Refuse the whole operation
    // rather than dropping the toplevel item and continuing: the selection
    // is not what the user believed it was, and executing the remainder
    // would hide that. Returning here also skips prepareRefresh() with the
    // tree's root in the refresh set and the complete re-read that follows.

    FileInfoSet toplevels = toplevelItems( selection );

    if ( ! toplevels.isEmpty() &&
	 ! allowsToplevel( cleanup->refreshPolicy(), selection.size() ) )
    {
	reportToplevelRefusal( cleanup->cleanTitle(), toplevels, selection.size() );
	return;
    }

    if ( cleanup->askForConfirmation() && ! confirmation( cleanup, selection ) )
    {
	logDebug() << "User declined confirmation" << endl;
	return;
    }

    emit startingCleanup( cleanup->cleanTitle() );

    OutputWindow * outputWindow = new OutputWindow( qApp->activeWindow() );
    CHECK_NEW( outputWindow );
    outputWindow->setAutoClose( cleanup->outputWindowAutoClose() );

    switch ( cleanup->outputWindowPolicy() )
    {
	case Cleanup::ShowAlways:
	    outputWindow->show();
	    break;

	case Cleanup::ShowAfterTimeout:
	    outputWindow->showAfterTimeout( cleanup->outputWindowTimeout() );
	    break;

	case Cleanup::ShowIfErrorOutput: // showOnStderr is default
	    break;

	case Cleanup::ShowNever:
	    outputWindow->setShowOnStderr( false );
	    break;
    }

    if ( cleanup->refreshPolicy() == Cleanup::RefreshThis ||
	 cleanup->refreshPolicy() == Cleanup::RefreshParent )
    {
	FileInfoSet refreshSet =
	    cleanup->refreshPolicy() == Cleanup::RefreshParent ?
	    Refresher::parents( selection ) : selection;

	_selectionModel->prepareRefresh( refreshSet );
	Refresher * refresher = new Refresher( refreshSet, this );
	CHECK_NEW( refresher );

	connect( outputWindow, SIGNAL( lastProcessFinished( int ) ),
		 refresher,    SLOT  ( refresh()		) );
    }

    connect( outputWindow, SIGNAL( lastProcessFinished( int ) ),
	     this,	   SIGNAL( cleanupFinished    ( int ) ) );


    // Intentionally not using the normalized FileInfoSet here: If a user
    // selects a file and one of its ancestors, he might be interested to
    // perform an action on each of them individually. We can't know if the
    // action on the ancestor affects any of its children.

    foreach ( FileInfo * item, selection )
    {
	if ( cleanup->worksFor( item ) )
	{
	    cleanup->execute( item, outputWindow );
	}
	else
	{
	    logWarning() << "Cleanup " << cleanup
			 << " does not work for " << item << endl;
	}
    }

    if ( cleanup->refreshPolicy() == Cleanup::AssumeDeleted )
    {
        // It is important to use the normalized FileInfoSet here to avoid a
        // segfault because we are iterating over items whose ancestors we just
        // deleted (thus invalidating pointers to it). Normalizing removes
        // items from the set that also have any ancestors in the set.

        foreach ( FileInfo * item, selection.invalidRemoved().normalized() )
        {
            DirTree * tree = item->tree();

            if ( tree->isBusy() )
                logWarning() << "Ignoring AssumeDeleted: DirTree is being read" << endl;
            else
                tree->deleteSubtree( item );
        }
    }

    outputWindow->noMoreProcesses();
}


FileInfoSet CleanupCollection::toplevelItems( const FileInfoSet & selection )
{
    FileInfoSet toplevels;

    foreach ( FileInfo * item, selection )
    {
	if ( isToplevelItem( item ) )
	    toplevels << item;
    }

    return toplevels;
}


bool CleanupCollection::allowsToplevel( Cleanup::RefreshPolicy policy, int selectionSize )
{
    switch ( policy )
    {
	case Cleanup::NoRefresh:
	    return true;

	case Cleanup::RefreshThis:
	    // An explicit action on just the scan root, e.g. "Clear Directory
	    // Contents"; the normal confirmation still applies. With anything
	    // else selected along with it, the selection is suspect.
	    return selectionSize == 1;

	case Cleanup::RefreshParent:
	case Cleanup::AssumeDeleted:
	    // Both declare that the item ceases to exist.
	    return false;
    }

    return false;
}


void CleanupCollection::reportToplevelRefusal( const QString     & actionTitle,
					       const FileInfoSet & toplevels,
					       int		   selectionSize )
{
    QStringList urls;

    foreach ( FileInfo * item, toplevels )
	urls << item->url();

    urls.sort();

    logError() << actionTitle << " refused: the selection of "
	       << selectionSize << " item(s) contains the toplevel item(s) "
	       << urls.join( ", " ) << "; nothing executed" << endl;

    QStringList lines;

    foreach ( FileInfo * item, toplevels )
    {
	QString label = item->isPkgInfo() ? tr( "Package" ) : tr( "Directory" );

	lines << tr( "<b>%1 <font color=red>%2</font></b>" )
	    .arg( label )
	    .arg( item->url().toHtmlEscaped() );
    }

    lines.sort();

    QString msg = "<html>";
    msg += tr( "<h3>%1 refused</h3>" ).arg( actionTitle.toHtmlEscaped() );
    msg += tr( "<p>The selection contains the top level of this directory tree:</p>" );
    msg += lines.join( "<br>" );

    if ( selectionSize > 1 )
    {
	msg += tr( "<p>%1 items are selected. Nothing was executed.<br>"
		   "Check the selection: it may include rows that are "
		   "scrolled out of view.</p>" ).arg( selectionSize );
    }
    else
    {
	msg += tr( "<p>Nothing was executed. QDirStat does not run an action "
		   "that removes or replaces the top level item.<br>"
		   "To work on it, open its parent directory instead.</p>" );
    }

    QMessageBox box( QMessageBox::Critical,
		     tr( "Not Executed" ),
		     msg,
		     QMessageBox::Ok,
		     qApp->activeWindow() );
    box.setObjectName( "toplevelRefusal" );
    box.exec();
}


bool CleanupCollection::confirmation( Cleanup * cleanup, const FileInfoSet & items )
{
    //
    // Rules (see https://github.com/shundhammer/qdirstat/issues/329):
    //
    // - Directories are listed first, each on its own line, never truncated.
    //   Only files are cut to MAX_URLS_IN_CONFIRMATION_POPUP, with a count of
    //   what was left out. A long directory list moves, complete, into the
    //   dialog's details pane; the main text keeps the count and the first
    //   few.
    // - The counts are stated up front ("3 directories and 2 files").
    // - A selection that contains a directory and items inside it is flagged:
    //   that is what an unintended Shift range looks like.
    // - The scan root is marked if it reaches this dialog (only possible as
    //   the single item of a RefreshThis cleanup, see the scan root guard).
    // - The default button and the Escape button are both "No". Enter never
    //   confirms.
    //
    // URLs are HTML-escaped in the main text (it is HTML) and raw in the
    // details pane (it is plain text).
    //

    QString title = cleanup->cleanTitle();
    QString msg   = "<html>" + tr( "<h3>%1</h3>" ).arg( title.toHtmlEscaped() );
    QString details;

    QList<FileInfo *> dirs;
    QList<FileInfo *> files;

    foreach ( FileInfo * item, items )
    {
	if ( item->isDirInfo() )
	    dirs << item;
	else
	    files << item;
    }

    std::sort( dirs.begin(),  dirs.end(),  urlLessThan );
    std::sort( files.begin(), files.end(), urlLessThan );

    if ( items.size() == 1 ) // The most common case
    {
	FileInfo * item = items.first();

	if ( item->isDirInfo() )
	    msg += tr( "for <b>directory</b> %1" ).arg( confirmationLine( item, false ) );
	else
	    msg += tr( "for file %1" ).arg( confirmationLine( item, false ) );

	msg += "<br>";
    }
    else // Multiple items selected
    {
	QString counts;

	if ( ! dirs.isEmpty() && ! files.isEmpty() )
	    counts = tr( "%1 and %2" ).arg( dirCount( dirs.size() ) ).arg( fileCount( files.size() ) );
	else if ( ! dirs.isEmpty() )
	    counts = dirCount( dirs.size() );
	else
	    counts = fileCount( files.size() );

	msg += tr( "<p>for %1:</p>" ).arg( counts );

	QStringList lines;

	// Directories: all of them, either here or in the details pane

	bool dirsInDetails = dirs.size() > MAX_DIRS_IN_CONFIRMATION_TEXT;
	int	 dirsShown     = dirsInDetails ? DIRS_SHOWN_IF_MORE : dirs.size();

	for ( int i = 0; i < dirsShown; ++i )
	    lines << confirmationLine( dirs.at( i ), true );

	if ( dirsInDetails )
	{
	    lines << tr( "<i>... %1 more; all %2 are listed under \"Show Details\"</i>" )
		.arg( dirs.size() - dirsShown )
		.arg( dirCount( dirs.size() ) );

	    details += dirCount( dirs.size() ) + ":\n";

	    foreach ( FileInfo * dir, dirs )
		details += "  " + dir->url() + ( isToplevelItem( dir ) ? tr( "   [SCAN ROOT]" ) : QString() ) + "\n";
	}

	// Files: the first few, then a count

	if ( ! files.isEmpty() )
	{
	    if ( ! dirs.isEmpty() )
		lines << "";

	    int filesShown = qMin( files.size(), (int) MAX_URLS_IN_CONFIRMATION_POPUP );

	    for ( int i = 0; i < filesShown; ++i )
		lines << confirmationLine( files.at( i ), false );

	    if ( filesShown < files.size() )
	    {
		int more = files.size() - filesShown;

		lines << ( more == 1 ? tr( "<i>... 1 more file</i>" )
				     : tr( "<i>... %1 more files</i>" ).arg( more ) );
	    }

	    if ( dirsInDetails )
	    {
		details += "\n" + fileCount( files.size() ) + ":\n";

		foreach ( FileInfo * file, files )
		    details += "  " + file->url() + "\n";
	    }
	}

	msg += lines.join( "<br>" );

	// Nested selection: a directory and items inside it

	if ( items.normalized().size() != items.size() )
	{
	    msg += tr( "<p><b>The selection contains a directory and items inside "
		       "that directory.</b><br>"
		       "Check that this is the intended selection: a Shift-click "
		       "range may include rows that are scrolled out of view.</p>" );
	}
    }

    QMessageBox box( QMessageBox::Warning,
		     tr( "Please Confirm" ),
		     msg,
		     QMessageBox::Yes | QMessageBox::No,
		     qApp->activeWindow() );

    box.setObjectName( "cleanupConfirmation" );
    box.setDefaultButton( QMessageBox::No );
    box.setEscapeButton ( QMessageBox::No );

    if ( ! details.isEmpty() )
	box.setDetailedText( details );

    return box.exec() == QMessageBox::Yes;
}


QString CleanupCollection::confirmationLine( FileInfo * item, bool dirLabel )
{
    QString line;

    if ( dirLabel )
	line = tr( "<b>Directory</b> " );

    line += item->url().toHtmlEscaped();

    if ( isToplevelItem( item ) )
	line += tr( " &nbsp;<b><font color=red>SCAN ROOT</font></b>" );

    return line;
}


bool CleanupCollection::isToplevelItem( const FileInfo * item )
{
    // Same test as DirTree::isToplevel(): a direct child of the tree's
    // internal root. Not going through item->tree() so this also works for
    // items whose tree pointer is not set.

    return item && item->parent() && ! item->parent()->parent();
}


bool CleanupCollection::urlLessThan( const FileInfo * a, const FileInfo * b )
{
    return a->url() < b->url();
}


QString CleanupCollection::dirCount( int n )
{
    return n == 1 ? tr( "1 directory" ) : tr( "%1 directories" ).arg( n );
}


QString CleanupCollection::fileCount( int n )
{
    return n == 1 ? tr( "1 file" ) : tr( "%1 files" ).arg( n );
}


void CleanupCollection::addToMenu( QMenu * menu, bool keepUpdated )
{
    CHECK_PTR( menu );

    foreach ( Cleanup * cleanup, _cleanupList )
    {
	if ( cleanup->active() )
	    menu->addAction( cleanup );
    }

    if ( keepUpdated && ! _menus.contains( menu ) )
	_menus << menu;
}


void CleanupCollection::addEnabledToMenu( QMenu * menu )
{
    CHECK_PTR( menu );

    foreach ( Cleanup * cleanup, _cleanupList )
    {
	if ( cleanup->active() && cleanup->isEnabled() )
	    menu->addAction( cleanup );
    }
}


void CleanupCollection::addToToolBar( QToolBar * toolBar, bool keepUpdated )
{
    CHECK_PTR( toolBar );

    foreach ( Cleanup * cleanup, _cleanupList )
    {
	if ( cleanup->active() &&
	     ! cleanup->icon().isNull() )
	{
	    // Add only cleanups that have an icon to avoid overcrowding the
	    // toolbar with actions that only have a text.

	    toolBar->addAction( cleanup );
	}
    }

    if ( keepUpdated && ! _toolBars.contains( toolBar ) )
	_toolBars << toolBar;
}


void CleanupCollection::moveUp( Cleanup * cleanup )
{
    _listMover.moveUp( cleanup );
    updateMenusAndToolBars();
}


void CleanupCollection::moveDown( Cleanup * cleanup )
{
    _listMover.moveDown( cleanup );
    updateMenusAndToolBars();
}


void CleanupCollection::moveToTop( Cleanup * cleanup )
{
    _listMover.moveToTop( cleanup );
    updateMenusAndToolBars();
}


void CleanupCollection::moveToBottom( Cleanup * cleanup )
{
    _listMover.moveToBottom( cleanup );
    updateMenusAndToolBars();
}


void CleanupCollection::readSettings()
{
    CleanupSettings settings;
    QStringList cleanupGroups = settings.findGroups( settings.groupPrefix() );

    if ( ! cleanupGroups.isEmpty() ) // Keep defaults (StdCleanups) if settings empty
    {
	clear();

	// Read all settings groups [Cleanup_xx] that were found

	foreach ( const QString & groupName, cleanupGroups )
	{
	    settings.beginGroup( groupName );

	    // Read one cleanup

	    QString command  = settings.value( "Command" ).toString();
	    QString title    = settings.value( "Title"	 ).toString();
	    QString iconName = settings.value( "Icon"	 ).toString();
	    QString hotkey   = settings.value( "Hotkey"	 ).toString();
	    QString shell    = settings.value( "Shell"	 ).toString();

	    bool active		       = settings.value( "Active"		, true	).toBool();
	    bool worksForDir	       = settings.value( "WorksForDir"		, true	).toBool();
	    bool worksForFile	       = settings.value( "WorksForFile"		, true	).toBool();
	    bool worksForDotEntry      = settings.value( "WorksForDotEntry"	, true	).toBool();
	    bool recurse	       = settings.value( "Recurse"		, false ).toBool();
	    bool askForConfirmation    = settings.value( "AskForConfirmation"	, false ).toBool();
	    bool outputWindowAutoClose = settings.value( "OutputWindowAutoClose", false ).toBool();
	    int	 outputWindowTimeout   = settings.value( "OutputWindowTimeout"	, 0	).toInt();

	    int refreshPolicy	    = readEnumEntry( settings, "RefreshPolicy",
						     Cleanup::NoRefresh,
						     Cleanup::refreshPolicyMapping() );

	    int outputWindowPolicy  = readEnumEntry( settings, "OutputWindowPolicy",
						     Cleanup::ShowAfterTimeout,
						     Cleanup::outputWindowPolicyMapping() );
	    if ( ! command.isEmpty() &&
		 ! title.isEmpty()     )
	    {
		Cleanup * cleanup = new Cleanup( command, title, this );
		CHECK_NEW( cleanup );
		add( cleanup );

		cleanup->setActive	    ( active	       );
		cleanup->setWorksForDir	    ( worksForDir      );
		cleanup->setWorksForFile    ( worksForFile     );
		cleanup->setWorksForDotEntry( worksForDotEntry );
		cleanup->setRecurse	    ( recurse	       );
		cleanup->setShell	    ( shell	       );
		cleanup->setAskForConfirmation	 ( askForConfirmation	 );
		cleanup->setOutputWindowAutoClose( outputWindowAutoClose );
		cleanup->setOutputWindowTimeout	 ( outputWindowTimeout	 );
		cleanup->setRefreshPolicy     ( static_cast<Cleanup::RefreshPolicy>( refreshPolicy ) );
		cleanup->setOutputWindowPolicy( static_cast<Cleanup::OutputWindowPolicy>( outputWindowPolicy ) );

		if ( ! iconName.isEmpty() )
		    cleanup->setIcon( iconName );

		if ( ! hotkey.isEmpty() )
                {
                    if ( hotkey == "Ctrl+F" )
                    {
                        // Crude workaround for the "Open File Manager Here" Ctrl+F shortcut
                        // clashing with Ctrl+F for "Find..." in the "Edit" menu.

                        // Yes, that's ugly; sorry. But Ctrl+F for "Find" has become an
                        // established standard.

                        hotkey = "Ctrl+G";
                        logError() << "The Ctrl+F hotkey for '" << title
                                   << "' is now taken by 'Edit' -> 'Find...'." << endl;
                        logError() << "Changing to " << hotkey << "." << endl;
                    }

		    cleanup->setShortcut( hotkey );
                }

		// if ( ! shell.isEmpty() )
		//    logDebug() << "Using custom shell " << shell << " for " << cleanup << endl;
	    }
	    else
	    {
		logError() << "Need at least Command and Title for a cleanup" << endl;
	    }

	    settings.endGroup(); // [Cleanup_01], [Cleanup_02], ...
	}
    }
}


void CleanupCollection::writeSettings()
{
    CleanupSettings settings;

    // Remove all leftover cleanup descriptions
    settings.removeGroups( settings.groupPrefix() );

    // Using a separate group for each cleanup for better readability in the
    // file.
    //
    // Settings arrays are hard to read and to edit if there are more than,
    // say, 2-3 entries for each array index. Plus, a user editing the file
    // would have to take care of the array count - which is very error prone.
    //
    // We are using [Cleanup_01], [Cleanup_02], ... here just because that's
    // easiest to generate automatically; upon reading, the numbers are
    // irrelevant. It's just important that each group name is
    // unique. readSettings() will happily pick up any group that starts with
    // "Cleanup_".

    for ( int i=0; i < _cleanupList.size(); ++i )
    {
	settings.beginGroup( "Cleanup", i+1 );

	Cleanup * cleanup = _cleanupList.at(i);

	settings.setValue( "Command"		  , cleanup->command()		     );
	settings.setValue( "Title"		  , cleanup->title()		     );
	settings.setValue( "Active"		  , cleanup->active()		     );
	settings.setValue( "WorksForDir"	  , cleanup->worksForDir()	     );
	settings.setValue( "WorksForFile"	  , cleanup->worksForFile()	     );
	settings.setValue( "WorksForDotEntry"	  , cleanup->worksForDotEntry()	     );
	settings.setValue( "Recurse"		  , cleanup->recurse()		     );
	settings.setValue( "AskForConfirmation"	  , cleanup->askForConfirmation()    );
	settings.setValue( "OutputWindowAutoClose", cleanup->outputWindowAutoClose() );

	if ( cleanup->outputWindowTimeout() > 0 )
	    settings.setValue( "OutputWindowTimeout"  , cleanup->outputWindowTimeout()	 );

	writeEnumEntry( settings, "RefreshPolicy",
			cleanup->refreshPolicy(),
			Cleanup::refreshPolicyMapping() );

	writeEnumEntry( settings, "OutputWindowPolicy",
			cleanup->outputWindowPolicy(),
			Cleanup::outputWindowPolicyMapping() );

	if ( ! cleanup->shell().isEmpty() )
	     settings.setValue( "Shell", cleanup->shell() );

	if ( ! cleanup->iconName().isEmpty() )
	    settings.setValue( "Icon", cleanup->iconName() );

	if ( ! cleanup->shortcut().isEmpty() )
	    settings.setValue( "Hotkey" , cleanup->shortcut().toString() );

	settings.endGroup(); // [Cleanup_01], [Cleanup_02], ...
    }
}

