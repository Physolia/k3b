/*
    SPDX-FileCopyrightText: 1998-2007 Sebastian Trueg <trueg@k3b.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _K3B_VIDEODVD_TIME_H_
#define _K3B_VIDEODVD_TIME_H_

#include "k3b_export.h"

#include <QString>

namespace K3b {
    namespace VideoDVD
    {
        /**
         * This should not be confused with Msf
         */
        class LIBK3B_EXPORT Time
        {
        public:
            Time();
            Time( unsigned short hour,
                  unsigned short min,
                  unsigned short sec,
                  unsigned short frame,
                  double frameRate );

            unsigned short hour() const { return m_hour; }
            unsigned short minute() const { return m_minute; }
            unsigned short second() const { return m_second; }
            unsigned short frame() const { return m_frame; }
            double frameRate() const { return m_frameRate; }

            double totalSeconds() const;
            unsigned int totalFrames() const;

            QString toString( bool includeFrames = true ) const;

        private:
            unsigned short m_hour;
            unsigned short m_minute;
            unsigned short m_second;
            unsigned short m_frame;
            double m_frameRate;
        };
    }
}

#endif
