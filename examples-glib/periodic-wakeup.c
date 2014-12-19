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
#include <string.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <keepalive-glib/keepalive-backgroundactivity.h>

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

static gboolean continue_cb(gpointer aptr)
{
    printf("%s()\n", __FUNCTION__);

    background_activity_t *activity = aptr;

    /* Suspend is unblocked, next iphb wakeup is scheduled */
    background_activity_wait(activity);

    return FALSE;
}

static gboolean stop_cb(gpointer aptr)
{
    printf("%s()\n", __FUNCTION__);

    background_activity_t *activity = aptr;

    /* Suspend is unblocked, no iphb wakeup is scheduled */
    background_activity_stop(activity);

    printf("EXIT MAINLOOP\n");
    g_main_loop_quit(mainloop_handle);
    return FALSE;
}

static void RUNNING(background_activity_t *activity, void *user_data)
{
    /* Suspend is blocked before entry to this function.
     *
     * In order for it not to stay blocked forever, ALL
     * EXECTION PATHS THAT CAN BE TAKEN FROM HERE MUST
     * END UP CALLING EITHER background_activity_wait()
     * OR background_activity_stop().
     */
    static int count = 0;

    printf("%s(%s) #%d\n", __FUNCTION__, (char *)user_data, ++count);

    if( count < 3 ) {
        /* After delay make background_activity_wait() call */
        g_timeout_add_seconds(10, continue_cb, activity);
    }
    else {
        /* After delay make background_activity_stop() call */
        g_timeout_add_seconds(10, stop_cb, activity);
    }

}

static void WAITING(background_activity_t *activity, void *user_data)
{
    /* Not needed, just illustrates when state transitions take place */
    printf("%s(%s)\n", __FUNCTION__, (char *)user_data);
}

static void STOPPED(background_activity_t *activity, void *user_data)
{
    /* Not needed, just illustrates when state transitions take place */
    printf("%s(%s)\n", __FUNCTION__, (char *)user_data);
}

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    background_activity_t *activity = 0;

    mainloop_handle = g_main_loop_new(0, 0);

    connect_to_system_bus();

    /* Create background activity object */
    activity = background_activity_new();

    /* Use dynamically allocated string as user data. It will be
     * released when background activity object is destroyed. */
    background_activity_set_user_data(activity, strdup("hello"), free);

    /* Setting up waiting and stopped callbacks is optional, but
     * can be helpful for example while debugging something */
    background_activity_set_waiting_callback(activity, WAITING);
    background_activity_set_stopped_callback(activity, STOPPED);

    /* Running callback must be set up or background activity
     * object will automatically move to stopped state immediately
     * after reaching running state  */
    background_activity_set_running_callback(activity, RUNNING);

    /* Schedule wakeups to occur on 30 second global wakeup slots */
    background_activity_frequency_t slot =
        BACKGROUND_ACTIVITY_FREQUENCY_THIRTY_SECONDS;
    background_activity_set_wakeup_slot(activity, slot);

    /* Start waiting for iphb wakeup */
    background_activity_wait(activity);

    /* Run mainloop; the 1st timer callback invocation can happen
     * in 0 to 30 seconds, but the subsequent ones 30 seconds from
     * the previous one. */

    printf("ENTER MAINLOOP\n");
    g_main_loop_run(mainloop_handle);
    printf("LEAVE MAINLOOP\n");

    /* Release background activity object */
    background_activity_unref(activity);

    disconnect_from_systembus();

    g_main_loop_unref(mainloop_handle);
    return 0;
}
