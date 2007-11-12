/*
 *
 * $Id$
 * Copyright (C) 2005 Sebastian Trueg <trueg@k3b.org>
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

#include "k3bdebuggingoutputdialog.h"

#include <k3bdevicemanager.h>
#include <k3bdevice.h>
#include <k3bdeviceglobals.h>
#include <k3bcore.h>
#include <k3bversion.h>
#include <k3bglobals.h>

#include <q3textedit.h>
#include <qcursor.h>
#include <qfile.h>
#include <qclipboard.h>

#include <klocale.h>
#include <kstdguiitem.h>
#include <kglobalsettings.h>
#include <kapplication.h>
#include <kfiledialog.h>
#include <kmessagebox.h>


K3bDebuggingOutputDialog::K3bDebuggingOutputDialog( QWidget* parent )
  : KDialogBase( parent, "debugViewDialog", true, i18n("Debugging Output"), Close|User1|User2, Close,
		 false,
		 KStdGuiItem::saveAs(),
		 KGuiItem( i18n("Copy"), "editcopy" ) )
{
  setButtonTip( User1, i18n("Save to file") );
  setButtonTip( User2, i18n("Copy to clipboard") );

  debugView = new Q3TextEdit( this );
  debugView->setReadOnly(true);
  debugView->setTextFormat( Q3TextEdit::PlainText );
  debugView->setCurrentFont( KGlobalSettings::fixedFont() );
  debugView->setWordWrap( Q3TextEdit::NoWrap );

  setMainWidget( debugView );

  resize( 600, 300 );
}


void K3bDebuggingOutputDialog::setOutput( const QString& data )
{
  // the following may take some time
  QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

  debugView->setText( data );

  QApplication::restoreOverrideCursor();
}


void K3bDebuggingOutputDialog::slotUser1()
{
  QString filename = KFileDialog::getSaveFileName();
  if( !filename.isEmpty() ) {
    QFile f( filename );
    if( !f.exists() || KMessageBox::warningContinueCancel( this,
						  i18n("Do you want to overwrite %1?").arg(filename),
						  i18n("File Exists"), i18n("Overwrite") )
	== KMessageBox::Continue ) {

      if( f.open( QIODevice::WriteOnly ) ) {
	Q3TextStream t( &f );
	t << debugView->text();
      }
      else {
	KMessageBox::error( this, i18n("Could not open file %1").arg(filename) );
      }
    }
  }
}


void K3bDebuggingOutputDialog::slotUser2()
{
  QApplication::clipboard()->setText( debugView->text(), QClipboard::Clipboard );
}

#include "k3bdebuggingoutputdialog.moc"
