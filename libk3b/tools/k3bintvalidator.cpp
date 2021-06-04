/*
    SPDX-FileCopyrightText: 1998-2007 Sebastian Trueg <trueg@k3b.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "k3bintvalidator.h"
#include "k3b_i18n.h"

#include <QDebug>
#include <QString>
#include <QWidget>


K3b::IntValidator::IntValidator ( QWidget * parent )
    : QValidator(parent)
{
    m_min = m_max = 0;
}


K3b::IntValidator::IntValidator ( int bottom, int top, QWidget * parent)
    : QValidator(parent)
{
    m_min = bottom;
    m_max = top;
}


K3b::IntValidator::~IntValidator ()
{
}


QValidator::State K3b::IntValidator::validate ( QString &str, int & ) const
{
    bool ok;
    int  val = 0;
    QString newStr;

    newStr = str.trimmed();
    newStr = newStr.toUpper();

    if( newStr.length() ) {
        // check for < 0
        bool minus = newStr.startsWith( '-' );
        if( minus )
            newStr.remove( 0, 1 );

        // check for hex
        bool hex = newStr.startsWith( "0X" );

        if( hex )
            newStr.remove( 0, 2 );

        // a special case
        if( newStr.isEmpty() ) {
            if( minus && m_min && m_min >= 0)
                ok = false;
            else
                return QValidator::Acceptable;
        }

        val = newStr.toInt( &ok, hex ? 16 : 10 );
        if( minus )
            val *= -1;
    }
    else {
        val = 0;
        ok = true;
    }

    if( !ok )
        return QValidator::Invalid;

    if( m_min && val > 0 && val < m_min )
        return QValidator::Acceptable;

    if( m_max && val < 0 && val > m_max )
        return QValidator::Acceptable;

    if( (m_max && val > m_max) || (m_min && val < m_min) )
        return QValidator::Invalid;

    return QValidator::Intermediate;
}


void K3b::IntValidator::fixup ( QString& ) const
{
    // TODO: remove preceding zeros
}


void K3b::IntValidator::setRange ( int bottom, int top )
{
    m_min = bottom;
    m_max = top;

    if( m_max < m_min )
        m_max = m_min;
}


int K3b::IntValidator::bottom () const
{
    return m_min;
}


int K3b::IntValidator::top () const
{
    return m_max;
}


int K3b::IntValidator::toInt( const QString& s, bool* ok )
{
    if( s.toLower().startsWith( "0x" ) )
        return s.right( s.length()-2 ).toInt( ok, 16 );
    else if( s.toLower().startsWith( "-0x" ) )
        return -1 * s.right( s.length()-3 ).toInt( ok, 16 );
    else
        return s.toInt( ok, 10 );
}
