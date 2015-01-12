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

#ifndef KEEPALIVE_GLIB_XDBUS_H_
# define KEEPALIVE_GLIB_XDBUS_H_

#include <stdbool.h>
#include <dbus/dbus.h>

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

/* Internal to libkeepalive-glib - documented at source code
 *
 * These functions are not exported and the header must not
 * be included in the devel package
 */

bool             xdbus_connection_is_valid (DBusConnection *con);
DBusPendingCall *xdbus_method_call_va      (DBusConnection *con, const char *service, const char *object, const char *interface, const char *method, DBusPendingCallNotifyFunction notify_cb, void *data, DBusFreeFunction free_cb, int arg_type, va_list va);
DBusPendingCall *xdbus_method_call         (DBusConnection *con, const char *service, const char *object, const char *interface, const char *method, DBusPendingCallNotifyFunction notify_cb, void *data, DBusFreeFunction free_cb, int arg_type, ...);
void             xdbus_simple_call         (DBusConnection *con, const char *service, const char *object, const char *interface, const char *method, int arg_type, ...);

# ifdef __cplusplus
};
# endif

#endif // KEEPALIVE_GLIB_XDBUS_H_
