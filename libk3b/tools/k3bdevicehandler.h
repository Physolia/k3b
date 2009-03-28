/*
 *
 * Copyright (C) 2003-2009 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2009 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */


#ifndef _K3B_DEVICE_HANDLER_H_
#define _K3B_DEVICE_HANDLER_H_

#include <k3bthreadjob.h>
#include "k3bdevice.h"
#include "k3bdiskinfo.h"
#include "k3bmsf.h"
#include "k3bcdtext.h"
#include "k3b_export.h"


namespace K3b {
    namespace Device
    {
        class Device;

        /**
         * The Device::Devicehandler is a threaded wrapper around Device::Device.
         * It allows async access to the time comsuming blocking Device::Device methods.
         * Since it's a Job it is very easy to handle. Just use one of the methods and
         * connect to the finished signal.
         * Be aware that all methods only return valid values if the corresponding info has
         * been successfuly requested.
         *
         * Be aware that multiple requests in a row (without waiting for the job to finish) will
         * only result in one finished() signal answering the last request.
         */
        class LIBK3B_EXPORT DeviceHandler : public ThreadJob
        {
            Q_OBJECT

        public:
            enum Command {
                NO_COMMAND = 0x0,

                /**
                 * Retrieve basic disk information.
                 * Always successful, even with an empty or no media at all!
                 *
                 * \sa diskInfo()
                 */
                DISKINFO = 0x1,

                /**
                 * Retrieve the Toc.
                 * Always successful, even with an empty or no media at all!
                 *
                 * \sa toc()
                 */
                TOC = 0x2,

                /**
                 * Retrieve the CD-Text.
                 *
                 * Successful if the media contains CD-Text.
                 *
                 * \sa cdText()
                 */
                CD_TEXT = 0x4,

                /**
                 * Retrieve the raw, undecoded CD-Text.
                 *
                 * Successful if the media contains CD-Text.
                 *
                 * \sa rawCdText()
                 */
                CD_TEXT_RAW = 0x8,

                /**
                 * Retrieve the size of the disk.
                 *
                 * Always successful, even with an empty or no media at all!
                 *
                 * \sa diskSize()
                 */
                DISKSIZE = 0x10,

                /**
                 * Retrieve the remaining size of the disk.
                 *
                 * Always successful, even with an empty or no media at all!
                 *
                 * \sa remainingSize()
                 */
                REMAININGSIZE = 0x20,

                /**
                 * Always successful, even with an empty or no media at all!
                 */
                TOCTYPE = 0x40,

                /**
                 * Always successful, even with an empty or no media at all!
                 */
                NUMSESSIONS = 0x80,

                /**
                 * Successful if the drive could be blocked.
                 */
                BLOCK = 0x100,

                /**
                 * Successful if the drive could be unblocked.
                 */
                UNBLOCK = 0x200,

                /**
                 * Successful if the media was ejected.
                 */
                EJECT = 0x400,

                /**
                 * Successful if the media was loaded
                 */
                LOAD = 0x800,

                RELOAD = EJECT|LOAD,

                /**
                 * Determine the device buffer state.
                 */
                BUFFER_CAPACITY = 0x1000,

                NEXT_WRITABLE_ADDRESS = 0x2000,

                /**
                 * Retrieves all medium information: DISKINFO, TOC, and CD_TEXT in case of an audio or mixed
                 * mode cd.
                 *
                 * Always successful, even with an empty or no media at all!
                 *
                 * \sa diskInfo(), toc(), cdText()
                 */
                MEDIAINFO = DISKINFO|TOC|CD_TEXT
            };
            Q_DECLARE_FLAGS( Commands, Command )

            DeviceHandler( Device*, QObject* parent = 0 );
            DeviceHandler( QObject* parent = 0 );

            /**
             * This constructor is used by the global "quick" methods and should not be used
             * otherwise except for the same usage.
             */
            DeviceHandler( Commands command, Device* );

            ~DeviceHandler();

            DiskInfo diskInfo() const;
            Toc toc() const;
            CdText cdText() const;
            QByteArray cdTextRaw() const;
            Msf diskSize() const;
            Msf remainingSize() const;
            int tocType() const;
            int numSessions() const;
            long long bufferCapacity() const;
            long long availableBufferCapacity() const;

            Msf nextWritableAddress() const;

            bool success() const;

        Q_SIGNALS:
            void finished( K3b::Device::DeviceHandler* );

        public Q_SLOTS:
            void setDevice( K3b::Device::Device* );
            void sendCommand( Commands command );

            void getToc();
            void getDiskInfo();
            void getDiskSize();
            void getRemainingSize();
            void getTocType();
            void getNumSessions();
            void block( bool );
            void eject();

        private:
            void jobFinished( bool success );
            bool run();

            class Private;
            Private* const d;
        };

        /**
         * Usage:
         * \code
         *  connect( Device::sendCommand( Device::DeviceHandler::DISKINFO, dev ),
         *           SIGNAL(finished(DeviceHandler*)),
         *           this, SLOT(someSlot(DeviceHandler*)) );
         *
         *  void someSlot( DeviceHandler* dh ) {
         *     if( dh->success() ) {
         * \endcode
         *
         * Be aware that the DeviceHandler will get destroyed once the signal has been
         * emitted.
         */
        LIBK3B_EXPORT DeviceHandler* sendCommand( DeviceHandler::Commands command, Device* );

        inline DeviceHandler* mediaInfo(Device* dev) {
            return sendCommand(DeviceHandler::MEDIAINFO,dev);
        }

        inline DeviceHandler* toc(Device* dev) {
            return sendCommand(DeviceHandler::TOC,dev);
        }

        inline DeviceHandler* diskSize(Device* dev) {
            return sendCommand(DeviceHandler::DISKSIZE,dev);
        }

        inline DeviceHandler* remainingSize(Device* dev) {
            return sendCommand(DeviceHandler::REMAININGSIZE,dev);
        }

        inline DeviceHandler* tocType(Device* dev) {
            return sendCommand(DeviceHandler::TOCTYPE,dev);
        }

        inline DeviceHandler* numSessions(Device* dev) {
            return sendCommand(DeviceHandler::NUMSESSIONS,dev);
        }

        inline DeviceHandler* block(Device* dev) {
            return sendCommand(DeviceHandler::BLOCK,dev);
        }

        inline DeviceHandler* unblock(Device* dev) {
            return sendCommand(DeviceHandler::UNBLOCK,dev);
        }

        inline DeviceHandler* eject(Device* dev) {
            return sendCommand(DeviceHandler::EJECT,dev);
        }

        inline DeviceHandler* reload(Device* dev) {
            return sendCommand(DeviceHandler::RELOAD,dev);
        }

        inline DeviceHandler* load(Device* dev) {
            return sendCommand(DeviceHandler::LOAD,dev);
        }
    }
}

Q_DECLARE_OPERATORS_FOR_FLAGS(K3b::Device::DeviceHandler::Commands)

LIBK3B_EXPORT QDebug operator<<( QDebug dbg, K3b::Device::DeviceHandler::Commands commands );

#endif
