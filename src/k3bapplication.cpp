/*
 *
 * $Id$
 * Copyright (C) 2003 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2003 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */


#include "k3bapplication.h"
#include "k3b.h"
#include "k3binterface.h"

#include <k3bcore.h>
#include <device/k3bdevicemanager.h>
#include <k3bexternalbinmanager.h>
#include <k3bdefaultexternalprograms.h>
#include <k3bglobals.h>
#include <k3bversion.h>
#include <songdb/k3bsongmanager.h>
#include <k3bdoc.h>
#include <k3bsystemproblemdialog.h>
#include <k3bthread.h>

#include <ktip.h>
#include <klocale.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <dcopclient.h>
#include <kstandarddirs.h>


K3bApplication* K3bApplication::s_k3bApp = 0;



K3bApplication::K3bApplication()
  : KApplication(),
    m_interface(0),
    m_mainWindow(0)
{
  m_core = new K3bCore( aboutData()->version(), config(), this );

  m_songManager = K3bSongManager::instance();  // this is bad stuff!!!

  connect( m_core, SIGNAL(initializationInfo(const QString&)),
	   SIGNAL(initializationInfo(const QString&)) );
  s_k3bApp = this;

  connect( this, SIGNAL(shutDown()), SLOT(slotShutDown()) );
}


K3bApplication::~K3bApplication()
{
  // we must not delete m_mainWindow here, QApplication takes care of it
  delete m_interface;
  delete m_songManager;  // this is bad stuff!!!
}


K3bMainWindow* K3bApplication::k3bMainWindow() const
{
  return m_mainWindow;
}


void K3bApplication::init()
{
  m_core->init();

  emit initializationInfo( i18n("Reading local Song database...") );
  config()->setGroup( "General Options" );

  QString filename = config()->readPathEntry( "songlistPath", locateLocal("data", "k3b") + "/songlist.xml" );
  K3bSongManager::instance()->load( filename );

  emit initializationInfo( i18n("Creating GUI...") );

  m_mainWindow = new K3bMainWindow();
  setMainWidget( m_mainWindow );

  if( dcopClient()->attach() ) {
    dcopClient()->registerAs( "k3b" );
    m_interface = new K3bInterface( m_mainWindow );
    dcopClient()->setDefaultObject( m_interface->objId() );
  }
  else {
    kdDebug() << "(K3bApplication) unable to attach to dcopserver!" << endl;
  }

  m_mainWindow->show();

  emit initializationInfo( i18n("Ready.") );

  config()->setGroup( "General Options" );

  if( config()->readBoolEntry( "check system config", true ) ) {
    emit initializationInfo( i18n("Checking System") );
    K3bSystemProblemDialog::checkSystem();
  }

  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

  if( args->isSet( "datacd" ) ) {
    // create new data project and add all arguments
    K3bDoc* doc = m_mainWindow->slotNewDataDoc();
    for( int i = 0; i < args->count(); i++ ) {
      doc->addUrl( args->url(i) );
    }
  }
  else if( args->isSet( "audiocd" ) ) {
    // create new audio project and add all arguments
    K3bDoc* doc = m_mainWindow->slotNewAudioDoc();
    for( int i = 0; i < args->count(); i++ ) {
      doc->addUrl( args->url(i) );
    }
  }
  else if( args->isSet( "mixedcd" ) ) {
    // create new audio project and add all arguments
    K3bDoc* doc = m_mainWindow->slotNewMixedDoc();
    for( int i = 0; i < args->count(); i++ ) {
      doc->addUrl( args->url(i) );
    }
  }
  else if( args->isSet( "videocd" ) ) {
    // create new audio project and add all arguments
    K3bDoc* doc = m_mainWindow->slotNewVcdDoc();
    for( int i = 0; i < args->count(); i++ ) {
      doc->addUrl( args->url(i) );
    }
  }
  else if( args->isSet( "emovixcd" ) ) {
    // create new audio project and add all arguments
    K3bDoc* doc = m_mainWindow->slotNewMovixDoc();
    for( int i = 0; i < args->count(); i++ ) {
      doc->addUrl( args->url(i) );
    }
  }
  else if( args->isSet( "datadvd" ) ) {
    // create new audio project and add all arguments
    K3bDoc* doc = m_mainWindow->slotNewDvdDoc();
    for( int i = 0; i < args->count(); i++ ) {
      doc->addUrl( args->url(i) );
    }
  }
  else if( args->isSet( "emovixdvd" ) ) {
    // create new audio project and add all arguments
    K3bDoc* doc = m_mainWindow->slotNewMovixDvdDoc();
    for( int i = 0; i < args->count(); i++ ) {
      doc->addUrl( args->url(i) );
    }
  }
  else if( args->isSet( "isoimage" ) ) {
    if ( args->count() == 1 )
      m_mainWindow->slotWriteIsoImage( args->url(0) );
    else
      m_mainWindow->slotWriteCdIsoImage();
  }
  else if( args->isSet( "binimage" ) ) {
    if ( args->count() == 1 )
      m_mainWindow->slotWriteBinImage( args->url(0) );
    else
      m_mainWindow->slotWriteBinImage();
  }
  else if(args->count()) {
    for( int i = 0; i < args->count(); i++ ) {
      m_mainWindow->openDocument( args->url(i) );
    }
  }

  if( args->isSet("copycd") )
    m_mainWindow->slotCdCopy();
  else if( args->isSet("clonecd") )
    m_mainWindow->slotCdClone();
  else if( args->isSet("erasecd") )
    m_mainWindow->slotBlankCdrw();
  else if( args->isSet("formatdvd") )
    m_mainWindow->slotFormatDvd();

  args->clear();

  KTipDialog::showTip( m_mainWindow );
}


void K3bApplication::slotShutDown()
{
  m_core->saveConfig();
  songManager()->save();

  K3bThread::waitUntilFinished();
}

#include "k3bapplication.moc"
