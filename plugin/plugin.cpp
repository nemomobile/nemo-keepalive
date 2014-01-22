/****************************************************************************************
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: Martin Jones <martin.jones@jollamobile.com>
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

#include "declarativebackgroundactivity.h"
#include "displayblanking.h"

#include <QtGlobal>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>

#define KEEPALIVE_URI "org.nemomobile.keepalive"

QML_DECLARE_TYPE(DisplayBlanking)
QML_DECLARE_TYPE(BackgroundActivity)

static QObject *display_blanking_api_factory(QQmlEngine *, QJSEngine *)
{
    return new DisplayBlanking;
}

static QObject *keepalive_api_factory(QQmlEngine *, QJSEngine *)
{
    return new DeclarativeKeepAlive;
}

class KeepalivePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID KEEPALIVE_URI)

public:
    void registerTypes(const char *uri)
    {
        Q_UNUSED(uri)
        Q_ASSERT(QLatin1String(uri) == QLatin1String(KEEPALIVE_URI));

        qmlRegisterSingletonType<DisplayBlanking>(KEEPALIVE_URI, 1, 0, "DisplayBlanking", display_blanking_api_factory);
        qmlRegisterSingletonType<DeclarativeKeepAlive>(KEEPALIVE_URI, 1, 0, "KeepAlive", keepalive_api_factory);
        qmlRegisterType<DeclarativeBackgroundJob>(KEEPALIVE_URI, 1, 0, "BackgroundJob");
    }
};

#include "plugin.moc"
