/*
 *   File name: DirTreeView.cpp
 *   Summary:	Tree view widget for directory tree
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QMenu>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QHeaderView>
#include <QStyle>
#include <QStyleOption>

#include "DirTreeView.h"
#include "DirTreeModel.h"
#include "SelectionModel.h"
#include "ActionManager.h"
#include "CleanupCollection.h"
#include "PercentBar.h"
#include "SizeColDelegate.h"
#include "HeaderTweaker.h"
#include "DirTree.h"
#include "FormatUtil.h"
#include "Exception.h"
#include "Logger.h"

using namespace QDirStat;


DirTreeView::DirTreeView( QWidget * parent ):
    QTreeView( parent ),
    _cleanupCollection(0),
    _extendingRange( false ),
    _shiftRangePressed( false )
{
    _percentBarDelegate = new DirTreePercentBarDelegate( this, PercentBarCol );
    CHECK_NEW( _percentBarDelegate );
    setItemDelegateForColumn( PercentBarCol, _percentBarDelegate );

    _sizeColDelegate = new SizeColDelegate( this );
    CHECK_NEW( _sizeColDelegate );
    setItemDelegateForColumn( SizeCol, _sizeColDelegate );

    setRootIsDecorated( true );
    setSortingEnabled( true );
    setSelectionMode( ExtendedSelection );
    setContextMenuPolicy( Qt::CustomContextMenu );
    setTextElideMode( Qt::ElideMiddle );
    setUniformRowHeights( true ); // Important for very large directories

    _headerTweaker = new HeaderTweaker( header(), this );
    CHECK_NEW( _headerTweaker );

    connect( this , SIGNAL( customContextMenuRequested( const QPoint & ) ),
	     this,  SLOT  ( contextMenu		      ( const QPoint & ) ) );
}


DirTreeView::~DirTreeView()
{
    delete _headerTweaker;
}


void DirTreeView::currentChanged( const QModelIndex & current,
				  const QModelIndex & oldCurrent )
{
    // logDebug() << "Setting new current to " << current << endl;
    QTreeView::currentChanged( current, oldCurrent );

    // Every current change that is not this view's own Shift extension moves
    // the Shift anchor: a tree click, a treemap click, a refresh, a locate
    // window, a breadcrumb. This replaces QAbstractItemView's private anchor
    // which is updated by none of the programmatic ones.

    if ( ! _extendingRange )
        _rangeAnchor = current;

    scrollTo( current );
}


void DirTreeView::contextMenu( const QPoint & pos )
{
    QModelIndex index = indexAt( pos );

    if ( ! index.isValid() )
    {
	// logDebug() << "No item at this position" << endl;
	return;
    }

    DataColumn col  = DataColumns::fromViewCol( index.column() );
    FileInfo * item = static_cast<FileInfo *>( index.internalPointer() );
    CHECK_MAGIC( item );

    if ( col == SizeCol )
	sizeContextMenu( pos, item );
    else
	actionContextMenu( pos, item );
}


void DirTreeView::actionContextMenu( const QPoint & pos, FileInfo * item )
{
    QMenu menu;
    QStringList actions;

    // The first action should not be a destructive one like "move to trash":
    // It's just too easy to select and execute the first action accidentially,
    // especially on a laptop touchpad.

    actions << "actionGoUp"
            << "actionGoToToplevel"
            << "---"
            << "actionMoveToTrash"
        ;

    // Intentionally adding unconditionally, even if disabled
    ActionManager::instance()->addActions( &menu, actions );


    // User-defined cleanups

    if ( _cleanupCollection )
	_cleanupCollection->addEnabledToMenu( &menu );

    // Less commonly used menu options
    actions.clear();
    actions << "---"
	    << "actionRefreshSelected"
	    << "actionReadExcludedDirectory"
	    << "actionContinueReadingAtMountPoint"
        ;

    ActionManager::instance()->addEnabledActions( &menu, actions );

    // Submenu for the auxiliary views to keep the context menu short.
    //
    // Those actions are strictly speaking irrelevant in most cases, and so
    // they should be omitted from a context menu. But here this serves for
    // discoverability: Most users don't even know that it is an option to
    // start any of those views from a subdirectory in the tree. As a
    // compromise to keep the context menu short, those auxiliary views go to a
    // submenu of the context menu. Submenus in context menus are generally
    // also discouraged, but here discoverability of these features is more
    // important.

    if ( item->isDirInfo() )    // Not for files, symlinks etc.
    {
        QMenu * subMenu = menu.addMenu( tr( "View in" ) );

        actions.clear();
        actions << "actionFileSizeStats"
                << "actionFileTypeStats"
                << "actionFileAgeStats"
            ;

        ActionManager::instance()->addActions( subMenu, actions );
    }

    menu.exec( mapToGlobal( pos ) );
}


void DirTreeView::sizeContextMenu( const QPoint & pos, FileInfo * item )
{
    if ( item->totalSize() >= 1024 ||
         item->totalAllocatedSize() > item->totalSize() )
    {
        QString text = DirTreeModel::sizeText( item, formatByteSize );

        if ( text.isEmpty() )
        {
            text = item->sizePrefix() + formatByteSize( item->totalSize() );

            if ( item->allocatedSize() > item->totalSize() )
            {
                text += tr( " (allocated: %1)" )
                    .arg( formatByteSize( item->totalAllocatedSize() ) );
            }
        }

        QMenu menu;
        menu.addAction( text );
        menu.exec( mapToGlobal( pos ) );
    }
}


QModelIndexList DirTreeView::expandedIndexes() const
{
    QModelIndexList expandedList;

    if ( ! model() )
	return QModelIndexList();

    DirTreeModel * dirTreeModel = dynamic_cast<DirTreeModel *>( model() );

    if ( ! dirTreeModel )
    {
	logError() << "Wrong model type to get this information" << endl;
	return QModelIndexList();
    }

    foreach ( const QModelIndex & index, dirTreeModel->persistentIndexList() )
    {
	if ( isExpanded( index ) )
	    expandedList << index;
    }

    return expandedList;
}


void DirTreeView::closeAllExcept( const QModelIndex & branch )
{
    QModelIndexList branchesToClose = expandedIndexes();

    // Remove all ancestors of 'branch' from branchesToClose

    QModelIndex index = branch;

    while ( index.isValid() )
    {
	// logDebug() << "Not closing " << index << endl;
	branchesToClose.removeAll( index );
	index = index.parent();
    }

    // Close all items in branchesToClose

    foreach ( index, branchesToClose )
    {
	// logDebug() << "Closing " << index << endl;
	collapse( index );
    }

    scrollTo( currentIndex(), QAbstractItemView::PositionAtCenter );
}


void DirTreeView::setExpanded( FileInfo * item, bool expanded )
{
    DirTreeModel * dirTreeModel = dynamic_cast<DirTreeModel *>( model() );

    if ( ! dirTreeModel )
    {
	logError() << "Wrong model type" << endl;
        return;
    }

    QModelIndex index = dirTreeModel->modelIndex( item );

    if ( index.isValid() )
        QTreeView::setExpanded( index, expanded );
}


void DirTreeView::mousePressEvent( QMouseEvent * event )
{
    if ( ! event )
        return;

    _shiftRangePressed = false;

    switch ( event->button() )
    {
        // Leave the the back / forward buttons on the mouse to act like the
        // history back / forward buttons in the tool bar.
        //
        // By default, the QTreeView parent class uses them to act as
        // cursor up / cursor down in the tree which defeats the idea of
        // using them as history consistently throughout the application,
        // making those mouse buttons pretty much unusable.
        //
        // So this makes sure those events are immediately propagated up to
        // the parent widget.

        case Qt::BackButton:
        case Qt::ForwardButton:
            event->ignore();
            return;

        case Qt::LeftButton:
            if ( isShiftRangePress( event ) )
            {
                QModelIndex target = indexAt( event->position().toPoint() );

                if ( extendRangeTo( target ) )
                {
                    // Not calling QTreeView::mousePressEvent(): it would
                    // build the range from Qt's private anchor.

                    _shiftRangePressed = true;
                    _shiftPressIndex   = target;
                    event->accept();
                    return;
                }
            }
            break;

        default:
            break;
    }

    QTreeView::mousePressEvent( event );
}


void DirTreeView::mouseMoveEvent( QMouseEvent * event )
{
    if ( ! event )
        return;

    if ( _shiftRangePressed )
    {
        if ( event->buttons() & Qt::LeftButton )
        {
            // Continuation of a Shift + click that this view handled: keep
            // extending the same range to the row under the pointer.
            //
            // QTreeView must not see this event: it would start a drag
            // selection from its own private press position, which is only
            // updated by non-Shift presses, i.e. the same stale anchor in
            // pixel form. Auto-scrolling at the viewport edges is not
            // available during this gesture.

            QModelIndex target = indexAt( event->position().toPoint() );

            if ( target.isValid() && ! sameRow( target, currentIndex() ) )
                extendRangeTo( target );

            event->accept();
            return;
        }

        _shiftRangePressed = false;     // button already up: gesture is over
    }

    QTreeView::mouseMoveEvent( event );
}


void DirTreeView::mouseReleaseEvent( QMouseEvent * event )
{
    if ( ! event )
        return;

    if ( _shiftRangePressed && event->button() == Qt::LeftButton )
    {
        _shiftRangePressed = false;

        // QAbstractItemView emits clicked() on release over the pressed
        // item; do the same so observers (e.g. MainWindow::itemClicked())
        // see no difference.

        QModelIndex index = indexAt( event->position().toPoint() );

        if ( index.isValid() && index == _shiftPressIndex )
            emit clicked( index );

        _shiftPressIndex = QPersistentModelIndex();
        event->accept();
        return;
    }

    QTreeView::mouseReleaseEvent( event );
}


void DirTreeView::keyPressEvent( QKeyEvent * event )
{
    if ( ! event )
        return;

    switch ( event->key() )
    {
        case Qt::Key_Asterisk:
            // By default, this opens all tree branches which completely
            // kills our performance, negating all our lazy sorting in
            // each branch in the DirTreeModel / DirInfo classes.
            //
            // So let's just ignore this key; we have better alternatives
            // with "Tree" -> "Expand to Level" -> "Level 0" .. "Level 5".
            return;

        default:
            break;
    }

    if ( isShiftRangeKey( event ) )
    {
        CursorAction action = MoveDown;

        switch ( event->key() )
        {
            case Qt::Key_Up:       action = MoveUp;       break;
            case Qt::Key_Down:     action = MoveDown;     break;
            case Qt::Key_PageUp:   action = MovePageUp;   break;
            case Qt::Key_PageDown: action = MovePageDown; break;
            case Qt::Key_Home:     action = MoveHome;     break;
            case Qt::Key_End:      action = MoveEnd;      break;
            case Qt::Key_Left:     action = MoveLeft;     break;
            case Qt::Key_Right:    action = MoveRight;    break;
            default:                                      break;
        }

        // QTreeView::moveCursor() has side effects for MoveLeft / MoveRight
        // (collapse / expand the current item); in that case it returns the
        // unchanged current index, and like QAbstractItemView::keyPressEvent()
        // this then changes no selection.

        QModelIndex oldCurrent = currentIndex();
        QModelIndex target     = moveCursor( action, event->modifiers() );

        if ( target.isValid() && target != oldCurrent )
            extendRangeTo( target );

        event->accept();
        return;
    }

    QTreeView::keyPressEvent( event );
}


QModelIndex DirTreeView::rangeAnchor() const
{
    // The anchor becomes invalid after a model reset or when its row was
    // removed, and visualRect() is empty when an ancestor of it was collapsed
    // after it became current. In both cases fall back to the current item,
    // which is what shows the focus outline.
    //
    // Visibility is judged in the tree column so that a hidden column of the
    // stored index is not mistaken for a collapsed branch.

    const int   col    = treeColumn();
    QModelIndex anchor = _rangeAnchor;

    if ( anchor.isValid() )
        anchor = anchor.sibling( anchor.row(), col );

    if ( ! anchor.isValid() || visualRect( anchor ).isEmpty() )
    {
        anchor = currentIndex();

        if ( anchor.isValid() )
            anchor = anchor.sibling( anchor.row(), col );
    }

    if ( ! anchor.isValid() || visualRect( anchor ).isEmpty() )
        return QModelIndex();   // current item not visible either

    return anchor;
}


bool DirTreeView::extendRangeTo( const QModelIndex & target )
{
    if ( ! selectionModel() || ! target.isValid() )
        return false;

    QModelIndex anchor = rangeAnchor();

    _extendingRange = true;     // keep currentChanged() from moving the anchor

    if ( anchor.isValid() )
    {
        selectionModel()->setCurrentIndex( target, QItemSelectionModel::NoUpdate );

        // The rect only needs to span the two rows; take both points from the
        // target's column so a hidden column of the anchor cannot interfere.
        // QTreeView::setSelection() maps points above or below the viewport
        // to the correct rows in both scroll modes.

        QModelIndex anchorRow = anchor.sibling( anchor.row(), target.column() );

        if ( ! anchorRow.isValid() )
            anchorRow = anchor;

        QRect anchorRect = visualRect( anchorRow );
        QRect targetRect = visualRect( target );
        QRect rect( QPoint( targetRect.center().x(), anchorRect.center().y() ),
                    targetRect.center() );

        // SelectCurrent replaces the range that is being extended and keeps
        // ranges committed earlier (e.g. by Ctrl + clicks), just like
        // QAbstractItemView does with its own anchor.

        setSelection( rect, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows );
    }
    else
    {
        // No usable anchor: a plain click. Never fall back to QTreeView
        // here; it would use Qt's private anchor.

        selectionModel()->setCurrentIndex( target,
                                           QItemSelectionModel::ClearAndSelect
                                           | QItemSelectionModel::Rows );
    }

    _extendingRange = false;

    return true;
}


bool DirTreeView::isShiftRangePress( const QMouseEvent * event ) const
{
    if ( selectionMode() != ExtendedSelection )
        return false;

    if ( event->button() != Qt::LeftButton ||
         ! ( event->modifiers() & Qt::ShiftModifier ) )
    {
        return false;
    }

    QPoint      pos   = event->position().toPoint();
    QModelIndex index = indexAt( pos );

    if ( ! index.isValid() )
        return false;   // Shift + click on empty space: QTreeView ignores it

    if ( isOnBranchIndicator( pos, index ) )
        return false;   // keep QTreeView's expand / collapse on the indicator

    return true;
}


bool DirTreeView::isShiftRangeKey( const QKeyEvent * event ) const
{
    if ( selectionMode() != ExtendedSelection )
        return false;

    Qt::KeyboardModifiers mods = event->modifiers();

    if ( ! ( mods & Qt::ShiftModifier ) || ( mods & Qt::ControlModifier ) )
    {
        // Ctrl + Shift + navigation: QTreeView moves the current item
        // without changing the selection; currentChanged() then moves the
        // anchor along, which is also what Qt does with its own anchor.

        return false;
    }

    switch ( event->key() )
    {
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_Left:
        case Qt::Key_Right:
            return true;

        default:
            return false;
    }
}


int DirTreeView::treeColumn() const
{
    // The tree (indentation, branch indicators) is drawn in the column
    // treePosition() names; -1 means the first visible column, whatever its
    // logical index (the user can reorder columns).

    return treePosition() >= 0 ? treePosition() : header()->logicalIndex( 0 );
}


bool DirTreeView::isOnBranchIndicator( const QPoint & pos, const QModelIndex & index ) const
{
    // This mirrors QTreeViewPrivate::itemDecorationAt(): the indicator is an
    // indentation()-wide strip directly before the tree column's item rect,
    // only for items that have children (and not for toplevel items if
    // rootIsDecorated() is off), possibly adjusted by the style.

    int col = treeColumn();

    if ( header()->logicalIndexAt( pos.x() ) != col )
        return false;

    QModelIndex treeIndex = index.sibling( index.row(), col );

    if ( ! treeIndex.isValid() || ! model()->hasChildren( treeIndex ) )
        return false;

    if ( ! rootIsDecorated() && ! treeIndex.parent().isValid() )
        return false;

    QRect itemRect = visualRect( treeIndex );

    if ( itemRect.isEmpty() )
        return false;

    QRect strip;

    if ( isRightToLeft() )
        strip = QRect( itemRect.right() + 1, itemRect.top(), indentation(), itemRect.height() );
    else
        strip = QRect( itemRect.left() - indentation(), itemRect.top(), indentation(), itemRect.height() );

    QStyleOption opt;
    opt.initFrom( this );
    opt.rect = strip;
    strip = style()->subElementRect( QStyle::SE_TreeViewDisclosureItem, &opt, this );

    return strip.contains( pos );
}


bool DirTreeView::sameRow( const QModelIndex & a, const QModelIndex & b )
{
    return a.isValid() && b.isValid() &&
        a.row() == b.row() && a.parent() == b.parent();
}
