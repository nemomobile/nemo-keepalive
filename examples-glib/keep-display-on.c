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
#include <keepalive-glib/keepalive-displaykeepalive.h>

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

    /* Create display keepalive object */
    displaykeepalive_t *displaykeepalive = displaykeepalive_new();

    /* Schedule exit in 2 minutes */
    g_timeout_add_seconds(120, exit_timer_cb, 0);

    /* Block automatic display dimming / blanking
     *
     * Note: This does not turn on the display
     */
    printf("BLOCK DISPLAY BLANKING\n");
    displaykeepalive_start(displaykeepalive);

    /* Run mainloop (until timer expires)
     *
     * Whenever the device is in a state where display is
     * on and lockscreen is not shown, automatic display
     * dimming/blanking does not happen.
     */
    printf("ENTER MAINLOOP\n");
    g_main_loop_run(mainloop_handle);
    printf("LEAVE MAINLOOP\n");

    /* Allow automatic dimming / blanking to happen again */
    printf("ALLOW DISPLAY BLANKING\n");
    displaykeepalive_stop(displaykeepalive);

    /* Release display keepalive object */
    displaykeepalive_unref(displaykeepalive);

    disconnect_from_systembus();

    g_main_loop_unref(mainloop_handle);
    return 0;
}
