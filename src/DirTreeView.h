/*
 *   File name: DirTreeView.h
 *   Summary:	Tree view widget for directory tree
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#ifndef DirTreeView_h
#define DirTreeView_h


#include <QTreeView>
#include <QPersistentModelIndex>

class QAction;


namespace QDirStat
{
    class DirTreePercentBarDelegate;
    class SizeColDelegate;
    class HeaderTweaker;
    class SelectionModelProxy;
    class CleanupCollection;
    class FileInfo;


    /**
     * Tree view widget for the QDirStat directory tree.
     *
     * This is a thin wrapper around TreeView that takes care about basic setup
     * and configuration of the tree view and adds support for synchronizing
     * current / selected items between the DirTree, the DirTreeMap and this
     * DirTreeView.
     *
     * The Qt model / view abstraction is kept up as good as possible, but this
     * widget is really meant to be used with a QDirStat::DirTreeModel and not
     * just any random subclass of QAbstractItemModel.
     **/
    class DirTreeView: public QTreeView
    {
	Q_OBJECT

    public:

	/**
	 * Constructor
	 **/
	DirTreeView( QWidget * parent = 0 );

	/**
	 * Destructor
	 **/
	virtual ~DirTreeView();

	/**
	 * Set the cleanup collection. If set, all cleanup actions from that
	 * collection will be added to the item context menu.
	 **/
	void setCleanupCollection( CleanupCollection * collection )
	    { _cleanupCollection = collection; }

	/**
	 * Return the cleanup collection or 0 if it is not set.
	 **/
	CleanupCollection * cleanupCollection() const { return _cleanupCollection; }

	/**
	 * Return the list of items that are currently expanded.
	 *
	 * This is well-known in the base class, but due to poor design that
	 * information is not accessible in derived classes. Well, I got
	 * creative.
	 *
	 * This method being public is more to spite the "masterminds" who in
	 * their infinite wisdom considered this information not worthy of
	 * being exported to the public because "it doesn't kneed to
	 * know". WTF?
	 **/
	QModelIndexList expandedIndexes() const;

	/**
	 * Return this view's header tweaker.
	 **/
	HeaderTweaker * headerTweaker() const { return _headerTweaker; }

        /**
         * Expand or collapse an item based on a FileInfo pointer.
         **/
        void setExpanded( FileInfo * item, bool expanded = true );


    public slots:

	/**
	 * Close (collapse) all branches except the one that 'branch' is in.
	 **/
	void closeAllExcept( const QModelIndex & branch );


    protected slots:

	/**
	 * Post a context menu for the item at 'pos'.
	 **/
	void contextMenu( const QPoint & pos );


    protected:

	/**
	 * Post the common context menu with actions (cleanup and other) for
	 * 'item' at 'pos'.
	 **/
	void actionContextMenu( const QPoint & pos, FileInfo * item );

	/**
	 * Post the context menu for the size column for 'item' at 'pos'.
         *
         * This displays the exact byte size for that item (the column shows a
         * human-readable size like "4.31 MB").
	 **/
	void sizeContextMenu( const QPoint & pos, FileInfo * item );

	/**
	 * Change the current item. Overwritten from QTreeView to make sure
	 * the branch of the new current item is expanded and scrolled to
	 * the visible area.
         *
         * This also moves the anchor for Shift ranges (see below) unless
         * the change was caused by this view's own Shift extension.
	 **/
	virtual void currentChanged( const QModelIndex & current,
				     const QModelIndex & oldCurrent ) Q_DECL_OVERRIDE;

        /**
         * Keyboard event handler.
         *
         * Shift + Up / Down / PageUp / PageDown / Home / End / Left / Right
         * extend the selection from this view's own anchor (see below).
         * Everything else is left to QTreeView.
         *
         * Reimplemented from QTreeView.
         **/
        virtual void keyPressEvent( QKeyEvent * event ) Q_DECL_OVERRIDE;

        /**
         * Mouse button handler.
         *
         * Don't let QTreeView steal and misappropriate the mouse back /
         * forward buttons; we want consistent history buttons throughout the
         * application.
         *
         * A Shift + left click is handled here as well: it selects the row
         * range from this view's own anchor to the clicked row (see below).
         *
         * Reimplemented from QTreeView.
         **/
        virtual void mousePressEvent( QMouseEvent * event ) Q_DECL_OVERRIDE;

        /**
         * Mouse move handler: a Shift + left click that this view handled
         * keeps extending the same range while the button is held down.
         *
         * Reimplemented from QTreeView.
         **/
        virtual void mouseMoveEvent( QMouseEvent * event ) Q_DECL_OVERRIDE;

        /**
         * Mouse release handler: ends a Shift range gesture started in
         * mousePressEvent().
         *
         * Reimplemented from QTreeView.
         **/
        virtual void mouseReleaseEvent( QMouseEvent * event ) Q_DECL_OVERRIDE;


    private:

        //
        // Shift range anchor
        //
        // QAbstractItemView keeps a private anchor for Shift ranges that is
        // only updated by its own mouse and keyboard handling. QDirStat moves
        // the current item through the shared SelectionModel (treemap clicks,
        // refresh, "future selection", locate windows, breadcrumbs), which
        // never updates that anchor. Depending on the Qt version it is either
        // never re-synced (Qt < 6.8) or re-synced only when no Shift key is
        // reported at that instant (Qt >= 6.8). A Shift-click could then
        // extend from a row that was made current long ago and is scrolled out
        // of view, selecting every expanded row in between.
        //
        // So this view keeps its own anchor: the item that was last made
        // current by any means except this view's own Shift extension, i.e.
        // the item that shows the focus outline. All Shift gestures (click,
        // click-and-drag, navigation keys) build their range from it. Qt's
        // private anchor is never consulted for them.
        //
        // See https://github.com/shundhammer/qdirstat/issues/329
        //

        /**
         * Return the anchor for a Shift range: _rangeAnchor if it is still
         * valid and visible (not below a collapsed branch), otherwise the
         * current item if that is visible, otherwise an invalid index.
         **/
        QModelIndex rangeAnchor() const;

        /**
         * Make 'target' the current item and select the row range from the
         * anchor to it. Without a usable anchor this degrades to a plain
         * single selection of 'target'. Return 'false' if nothing was done.
         **/
        bool extendRangeTo( const QModelIndex & target );

        /**
         * Return 'true' if 'event' is a Shift + left button press on an item
         * that this view should handle as a range extension.
         **/
        bool isShiftRangePress( const QMouseEvent * event ) const;

        /**
         * Return 'true' if 'event' is a Shift + navigation key that this view
         * should handle as a range extension.
         **/
        bool isShiftRangeKey( const QKeyEvent * event ) const;

        /**
         * Return the logical index of the column that shows the tree
         * (indentation and branch indicators).
         **/
        int treeColumn() const;

        /**
         * Return 'true' if 'pos' is on the branch indicator of 'index', i.e.
         * in the area where a click expands or collapses the item. This
         * mirrors what QTreeView checks before it handles a click as a
         * selection click.
         **/
        bool isOnBranchIndicator( const QPoint & pos, const QModelIndex & index ) const;

        /**
         * Return 'true' if both indexes are in the same row (same parent,
         * same row number); the column is ignored.
         **/
        static bool sameRow( const QModelIndex & a, const QModelIndex & b );


    protected:

	// Data members

	DirTreePercentBarDelegate * _percentBarDelegate;
        SizeColDelegate *           _sizeColDelegate;
	HeaderTweaker *             _headerTweaker;
	CleanupCollection *         _cleanupCollection;

        QPersistentModelIndex       _rangeAnchor;       // see "Shift range anchor" above
        QPersistentModelIndex       _shiftPressIndex;   // item of the Shift + click being handled
        bool                        _extendingRange;    // true only inside extendRangeTo()
        bool                        _shiftRangePressed; // left button still down after a handled Shift + click

    };	// class DirTreeView

}	// namespace QDirStat


#endif	// DirTreeView_h
