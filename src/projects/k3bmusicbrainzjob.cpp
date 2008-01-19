/*
 *
 * Copyright (C) 2005-2008 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2008 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#include "k3bmusicbrainzjob.h"
#include "k3bmusicbrainztrackloopupjob.h"

#include <k3baudiotrack.h>
#include <k3baudiodatasource.h>
#include <k3bsimplejobhandler.h>

#include <kmessagebox.h>
#include <kinputdialog.h>
#include <klocale.h>


class K3bMusicBrainzJob::Private
{
public:
    Q3PtrList<K3bAudioTrack> tracks;
    bool canceled;
    K3bMusicBrainzTrackLookupJob* mbTrackLookupJob;
};


// cannot use this as parent for the K3bSimpleJobHandler since this has not been constructed yet
K3bMusicBrainzJob::K3bMusicBrainzJob( QWidget* parent )
    : K3bJob( new K3bSimpleJobHandler( 0 ), parent ),
      d( new Private() )
{
    d->canceled = false;
    d->mbTrackLookupJob = new K3bMusicBrainzTrackLookupJob( this, this );

    connect( d->mbTrackLookupJob, SIGNAL(percent(int)), this, SIGNAL(subPercent(int)) );
    connect( d->mbTrackLookupJob, SIGNAL(percent(int)), this, SLOT(slotTrmPercent(int)) );
    connect( d->mbTrackLookupJob, SIGNAL(finished(bool)), this, SLOT(slotMbJobFinished(bool)) );
    connect( d->mbTrackLookupJob, SIGNAL(infoMessage(const QString&, int)), this, SIGNAL(infoMessage(const QString&, int)) );
}


K3bMusicBrainzJob::~K3bMusicBrainzJob()
{
    delete jobHandler();
    delete d;
}


bool K3bMusicBrainzJob::hasBeenCanceled() const
{
    return d->canceled;
}


void K3bMusicBrainzJob::setTracks( const Q3PtrList<K3bAudioTrack>& tracks )
{
    d->tracks = tracks;
}


void K3bMusicBrainzJob::start()
{
    jobStarted();

    d->canceled = false;

    d->mbTrackLookupJob->setAudioTrack( d->tracks.first() );
    d->mbTrackLookupJob->start();
}


void K3bMusicBrainzJob::cancel()
{
    d->canceled = true;
    d->mbTrackLookupJob->cancel();
}


void K3bMusicBrainzJob::slotTrmPercent( int p )
{
    // the easy way (inaccurate)
    emit percent( (100*d->tracks.at() + p) / d->tracks.count() );
}


void K3bMusicBrainzJob::slotMbJobFinished( bool success )
{
    if( hasBeenCanceled() ) {
        emit canceled();
        jobFinished(false);
    }
    else {
        emit trackFinished( d->tracks.current(), success );

        if( success ) {
            // found entries
            QStringList resultStrings, resultStringsUnique;
            for( int i = 0; i < d->mbTrackLookupJob->results(); ++i )
                resultStrings.append( d->mbTrackLookupJob->artist(i) + " - " + d->mbTrackLookupJob->title(i) );

            // since we are only using the title and the artist a lot of entries are alike to us
            // so to not let the user have to choose between two equal entries we trim the list down
            for( QStringList::const_iterator it = resultStrings.begin();
                 it != resultStrings.end(); ++it )
                if( resultStringsUnique.find( *it ) == resultStringsUnique.end() )
                    resultStringsUnique.append( *it );

            QString s;
            bool ok = true;
            if( resultStringsUnique.count() > 1 )
                s = KInputDialog::getItem( i18n("MusicBrainz Query"),
                                           i18n("Found multiple matches for track %1 (%2). Please select one.")
                                           .arg(d->tracks.current()->trackNumber())
                                           .arg(d->tracks.current()->firstSource()->sourceComment()),
                                           resultStringsUnique,
                                           0,
                                           false,
                                           &ok,
                                           dynamic_cast<QWidget*>(parent()) );
            else
                s = resultStringsUnique.first();

            if( ok ) {
                int i = resultStrings.findIndex( s );
                d->tracks.current()->setTitle( d->mbTrackLookupJob->title(i) );
                d->tracks.current()->setArtist( d->mbTrackLookupJob->artist(i) );
            }
        }

        // query next track
        if( d->tracks.next() ) {
            d->mbTrackLookupJob->setAudioTrack( d->tracks.current() );
            d->mbTrackLookupJob->start();
        }
        else {
            jobFinished( true );
        }
    }
}

#include "k3bmusicbrainzjob.moc"
