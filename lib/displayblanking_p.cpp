/****************************************************************************************
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: Simo Piiroinen <simo.piiroinen@jollamobile.com>
** All rights reserved.
**
** This file is part of nemo keepalive package.
**
** You may use this file under the terms of the GNU Lesser General
** Public License version 2.1 as published by the Free Software Foundation
** and appearing in the file license.lgpl included in the packaging
** of this file.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file license.lgpl included in the packaging
** of this file.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Lesser General Public License for more details.
**
****************************************************************************************/

#include <QTimer>
#include "displayblanking_p.h"

#include <mce/dbus-names.h>
#include <mce/mode-names.h>

/* ========================================================================= *
 * class DisplayBlankingPrivate
 * ========================================================================= */

DisplayBlankingPrivate::DisplayBlankingPrivate(DisplayBlanking *parent)
    : QObject(parent), m_preventBlanking(false), m_renew_timer(0), m_displayStatus(DisplayBlanking::Unknown)
{
    const int hardcoded_mce_limit = 60 * 1000; // [ms]
    const int safety_margin       = 10 * 1000; // [ms]

    // Default to: safe enough renew period
    m_renew_period = hardcoded_mce_limit - safety_margin; // [ms]

    m_mce_req_iface = new ComNokiaMceRequestInterface(MCE_SERVICE,
                                                      MCE_REQUEST_PATH,
                                                      QDBusConnection::systemBus(),
                                                      this);

    QDBusPendingReply<QString> reply = m_mce_req_iface->get_display_status();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(getDisplayStatusComplete(QDBusPendingCallWatcher*)));

    m_mce_signal_iface = new ComNokiaMceSignalInterface(MCE_SERVICE,
                                                        MCE_SIGNAL_PATH,
                                                        QDBusConnection::systemBus(),
                                                        this);

    connect(m_mce_signal_iface, SIGNAL(display_status_ind(const QString&)),
            this, SLOT(updateDisplayStatus(QString)));
}

DisplayBlanking::Status DisplayBlankingPrivate::displayStatus() const
{
    return m_displayStatus;
}

QTimer *DisplayBlankingPrivate::keepaliveTimer(void)
{
    if( !m_renew_timer ) {
        m_renew_timer = new QTimer(this);
        connect(m_renew_timer, SIGNAL(timeout()), this, SLOT(renewKeepalive()));
    }
    return m_renew_timer;
}

void DisplayBlankingPrivate::startKeepalive(void)
{
    m_mce_req_iface->req_display_blanking_pause();
    keepaliveTimer()->setInterval(m_renew_period);
    keepaliveTimer()->start();
}

void DisplayBlankingPrivate::renewKeepalive(void)
{
    m_mce_req_iface->req_display_blanking_pause();
}

void DisplayBlankingPrivate::stopKeepalive(void)
{
    keepaliveTimer()->stop();
    m_mce_req_iface->req_display_cancel_blanking_pause();
}

void DisplayBlankingPrivate::updateDisplayStatus(const QString &status)
{
    DisplayBlanking::Status newStatus = DisplayBlanking::Unknown;

    if (status == MCE_DISPLAY_OFF_STRING) {
        newStatus = DisplayBlanking::Off;
    } else if (status == MCE_DISPLAY_ON_STRING) {
        newStatus = DisplayBlanking::On;
    } else if (status == MCE_DISPLAY_DIM_STRING) {
        newStatus = DisplayBlanking::Dimmed;
    }

    if (newStatus != m_displayStatus) {
        m_displayStatus = newStatus;
        emit displayStatusChanged();
    }
}

void DisplayBlankingPrivate::getDisplayStatusComplete(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QString> reply = *call;
    if (!reply.isError()) {
        updateDisplayStatus(reply.value());
    }

    call->deleteLater();
}
