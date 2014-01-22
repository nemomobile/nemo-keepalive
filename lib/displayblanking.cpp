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

#include "displayblanking_p.h"
#include "common.h"

/* ========================================================================= *
 * class DisplayBlanking
 * ========================================================================= */

DisplayBlanking::DisplayBlanking(QObject *parent)
: QObject(parent)
{
    priv = new DisplayBlankingPrivate(this);
    connect(priv, SIGNAL(displayStatusChanged()), this, SIGNAL(statusChanged()));
}

DisplayBlanking::~DisplayBlanking(void)
{
}

DisplayBlanking::Status DisplayBlanking::status() const
{
    return priv->displayStatus();
}

bool DisplayBlanking::preventBlanking() const
{
    return priv->m_preventBlanking;
}

void DisplayBlanking::setPreventBlanking(bool prevent)
{
    TRACE
    if (prevent == priv->m_preventBlanking)
        return;

    priv->m_preventBlanking = prevent;
    if (priv->m_preventBlanking) {
        priv->startKeepalive();
    } else {
        priv->stopKeepalive();
    }
    emit preventBlankingChanged();
}
