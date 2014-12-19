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

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <keepalive-glib/keepalive-cpukeepalive.h>

#include <assert.h>

#define failure(FMT, ARGS...) do {\
    fprintf(stderr, "%s: "FMT"\n", __FUNCTION__, ## ARGS);\
    exit(EXIT_FAILURE); \
} while(0)

static DBusConnection *system_bus = 0;
static GMainLoop *mainloop_handle = 0;

static void disconnect_from_systembus(void)
{
    if( system_bus )
        dbus_connection_unref(system_bus), system_bus = 0;
}

static void connect_to_system_bus(void)
{
    DBusError err = DBUS_ERROR_INIT;
    system_bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if( !system_bus )
        failure("%s: %s", err.name, err.message);
    dbus_connection_setup_with_g_main(system_bus, 0);
    dbus_error_free(&err);
}

static gboolean exit_timer_cb(gpointer aptr)
{
    g_main_loop_quit(mainloop_handle);
    return FALSE;
}

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    mainloop_handle = g_main_loop_new(0, 0);

    connect_to_system_bus();

    /* Create cpu keepalive object */
    cpukeepalive_t *cpukeepalive = cpukeepalive_new();

    /* Schedule exit in 20 seconds */
    g_timeout_add_seconds(20, exit_timer_cb, 0);

    /* Start blocking suspend */
    printf("BLOCK SUSPEND\n");
    cpukeepalive_start(cpukeepalive);

    /* Run mainloop (until timer expires) */
    printf("ENTER MAINLOOP\n");
    g_main_loop_run(mainloop_handle);
    printf("LEAVE MAINLOOP\n");

    /* Allow device to suspend again*/
    printf("ALLOW SUSPEND\n");
    cpukeepalive_stop(cpukeepalive);

    /* Release cpu keepalive object */
    cpukeepalive_unref(cpukeepalive);

    disconnect_from_systembus();

    g_main_loop_unref(mainloop_handle);
    return 0;
}
