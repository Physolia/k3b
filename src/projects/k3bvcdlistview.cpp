/*
*
* $Id$
* Copyright (C) 2003-2004 Christian Kvasny <chris@k3b.org>
*
* This file is part of the K3b project.
* Copyright (C) 1998-2007 Sebastian Trueg <trueg@k3b.org>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
* See the file "COPYING" for the exact licensing terms.
*/

#include <q3header.h>
#include <qtimer.h>
#include <q3dragobject.h>
#include <qpoint.h>
#include <q3ptrlist.h>
#include <qstringlist.h>
#include <qevent.h>
#include <qpainter.h>
#include <qfontmetrics.h>
//Added by qt3to4:
#include <QDropEvent>

#include <kiconloader.h>
#include <kurl.h>
#include <k3urldrag.h>
#include <klocale.h>
#include <kaction.h>
#include <kmenu.h>
#include <kdialog.h>
#include <kactioncollection.h>

// K3b Includes
#include "k3bvcdlistview.h"
#include "k3bvcdlistviewitem.h"
#include "k3bvcdtrack.h"
#include "k3bvcdtrackdialog.h"
#include "k3bvcddoc.h"
#include <k3bview.h>

K3bVcdListView::K3bVcdListView( K3bView* view, K3bVcdDoc* doc, QWidget *parent, const char *name )
        : K3bListView( parent ), m_doc( doc ), m_view( view )
{
    setAcceptDrops( true );
    setDropVisualizer( true );
    setAllColumnsShowFocus( true );
    setDragEnabled( true );
    setSelectionModeExt( K3ListView::Extended );
    setItemsMovable( false );

    setNoItemText( i18n( "Use drag'n'drop to add MPEG video files to the project." ) + "\n"
                   + i18n( "After that press the burn button to write the CD." ) );

    setSorting( 0 );

    setupActions();
    setupPopupMenu();

    setupColumns();
    header() ->setClickEnabled( false );

    connect( this, SIGNAL( dropped( K3ListView*, QDropEvent*, Q3ListViewItem* ) ),
             this, SLOT( slotDropped( K3ListView*, QDropEvent*, Q3ListViewItem* ) ) );
    connect( this, SIGNAL( contextMenu( K3ListView*, Q3ListViewItem*, const QPoint& ) ),
             this, SLOT( showPopupMenu( K3ListView*, Q3ListViewItem*, const QPoint& ) ) );
    connect( this, SIGNAL( doubleClicked( Q3ListViewItem*, const QPoint&, int ) ),
             this, SLOT( showPropertiesDialog() ) );

    connect( m_doc, SIGNAL( changed() ), this, SLOT( slotUpdateItems() ) );
    connect( m_doc, SIGNAL( trackRemoved( K3bVcdTrack* ) ), this, SLOT( slotTrackRemoved( K3bVcdTrack* ) ) );

    slotUpdateItems();
}

K3bVcdListView::~K3bVcdListView()
{}

void K3bVcdListView::setupColumns()
{
    addColumn( i18n( "No." ) );
    addColumn( i18n( "Title" ) );
    addColumn( i18n( "Type" ) );
    addColumn( i18n( "Resolution" ) );
    addColumn( i18n( "High Resolution" ) );
    addColumn( i18n( "Framerate" ) );
    addColumn( i18n( "Muxrate" ) );
    addColumn( i18n( "Duration" ) );
    addColumn( i18n( "File Size" ) );
    addColumn( i18n( "Filename" ) );
}


void K3bVcdListView::setupActions()
{
    m_actionCollection = new KActionCollection( this );
    m_actionProperties = new KAction( i18n( "Properties" ), "misc", 0, this, SLOT( showPropertiesDialog() ), actionCollection() );
    m_actionRemove = new KAction( i18n( "Remove" ), "editdelete", Qt::Key_Delete, this, SLOT( slotRemoveTracks() ), actionCollection() );

    // disabled by default
    m_actionRemove->setEnabled( false );
}


void K3bVcdListView::setupPopupMenu()
{
    m_popupMenu = new KMenu( this, "VcdViewPopupMenu" );
    m_actionRemove->plug( m_popupMenu );
    m_popupMenu->insertSeparator();
    m_actionProperties->plug( m_popupMenu );
    m_popupMenu->insertSeparator();
    m_view->actionCollection() ->action( "project_burn" ) ->plug( m_popupMenu );
}


bool K3bVcdListView::acceptDrag( QDropEvent* e ) const
{
    // the first is for built-in item moving, the second for dropping urls
    return ( K3ListView::acceptDrag( e ) || K3URLDrag::canDecode( e ) );
}


Q3DragObject* K3bVcdListView::dragObject()
{
    Q3PtrList<Q3ListViewItem> list = selectedItems();

    if ( list.isEmpty() )
        return 0;

    Q3PtrListIterator<Q3ListViewItem> it( list );
    KUrl::List urls;

    for ( ; it.current(); ++it )
        urls.append( KUrl( ( ( K3bVcdListViewItem* ) it.current() ) ->vcdTrack() ->absPath() ) );

    return K3URLDrag::newDrag( urls, viewport() );
}


void K3bVcdListView::slotDropped( K3ListView*, QDropEvent* e, Q3ListViewItem* after )
{
    if ( !e->isAccepted() )
        return ;

    int pos;
    if ( after == 0L )
        pos = 0;
    else
        pos = ( ( K3bVcdListViewItem* ) after ) ->vcdTrack() ->index() + 1;

    if ( e->source() == viewport() ) {
        Q3PtrList<Q3ListViewItem> sel = selectedItems();
        Q3PtrListIterator<Q3ListViewItem> it( sel );
        K3bVcdTrack* trackAfter = ( after ? ( ( K3bVcdListViewItem* ) after ) ->vcdTrack() : 0 );
        while ( it.current() ) {
            K3bVcdTrack * track = ( ( K3bVcdListViewItem* ) it.current() ) ->vcdTrack();
            m_doc->moveTrack( track, trackAfter );
            trackAfter = track;
            ++it;
        }
    } else {
        KUrl::List urls;
        K3URLDrag::decode( e, urls );

        m_doc->addTracks( urls, pos );
    }

  // now grab that focus
  setFocus();
}


void K3bVcdListView::insertItem( Q3ListViewItem* item )
{
    K3ListView::insertItem( item );

    // make sure at least one item is selected
    if ( selectedItems().isEmpty() ) {
        setSelected( firstChild(), true );
    }
}

void K3bVcdListView::showPopupMenu( K3ListView*, Q3ListViewItem* _item, const QPoint& _point )
{
    if ( _item ) {
        m_actionRemove->setEnabled( true );
    } else {
        m_actionRemove->setEnabled( false );
    }

    m_popupMenu->popup( _point );
}

void K3bVcdListView::showPropertiesDialog()
{
    Q3PtrList<K3bVcdTrack> selected = selectedTracks();
    if ( !selected.isEmpty() && selected.count() == 1 ) {
        Q3PtrList<K3bVcdTrack> tracks = *m_doc->tracks();
        K3bVcdTrackDialog d( m_doc, tracks, selected, this );
        if ( d.exec() ) {
            repaint();
        }
    } else {
      m_view->slotProperties();
    }
}

Q3PtrList<K3bVcdTrack> K3bVcdListView::selectedTracks()
{
    Q3PtrList<K3bVcdTrack> selectedTracks;
    Q3PtrList<Q3ListViewItem> selectedVI( selectedItems() );
    for ( Q3ListViewItem * item = selectedVI.first(); item != 0; item = selectedVI.next() ) {
        K3bVcdListViewItem * vcdItem = dynamic_cast<K3bVcdListViewItem*>( item );
        if ( vcdItem ) {
            selectedTracks.append( vcdItem->vcdTrack() );
        }
    }

    return selectedTracks;
}


void K3bVcdListView::slotRemoveTracks()
{
    Q3PtrList<K3bVcdTrack> selected = selectedTracks();
    if ( !selected.isEmpty() ) {

        for ( K3bVcdTrack * track = selected.first(); track != 0; track = selected.next() ) {
            m_doc->removeTrack( track );
        }
    }

    if ( m_doc->numOfTracks() == 0 ) {
        m_actionRemove->setEnabled( false );
    }
}


void K3bVcdListView::slotTrackRemoved( K3bVcdTrack* track )
{
    Q3ListViewItem * viewItem = m_itemMap[ track ];
    m_itemMap.remove( track );
    delete viewItem;
}


void K3bVcdListView::slotUpdateItems()
{
    // iterate through all doc-tracks and test if we have a listItem, if not, create one
    K3bVcdTrack * track = m_doc->first();
    K3bVcdTrack* lastTrack = 0;
    while ( track != 0 ) {
        if ( !m_itemMap.contains( track ) )
            m_itemMap.insert( track, new K3bVcdListViewItem( track, this, m_itemMap[ lastTrack ] ) );

        lastTrack = track;
        track = m_doc->next();
    }

    if ( m_doc->numOfTracks() > 0 ) {
        m_actionRemove->setEnabled( true );
    } else {
        m_actionRemove->setEnabled( false );
    }

    sort();  // This is so lame!

    header()->setShown( m_doc->numOfTracks() > 0 );
}

#include "k3bvcdlistview.moc"
