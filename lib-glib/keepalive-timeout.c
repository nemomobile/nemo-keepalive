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

#include "keepalive-timeout.h"
#include "keepalive-backgroundactivity.h"

#include "logging.h"

#include <stdio.h>

#include <glib.h>

/* ========================================================================= *
 * CONSTANTS
 * ========================================================================= */

/* Logging prefix for this module */
#define PFIX "timeout: "

/* ========================================================================= *
 * TYPES
 * ========================================================================= */

typedef struct keepalive_timeout_t keepalive_timeout_t;

struct keepalive_timeout_t
{
    GSource                kat_source;

    background_activity_t *kat_activity;
    bool                   kat_triggered;
};

/* ========================================================================= *
 * INTERNAL FUNCTION PROTOTYPES
 * ========================================================================= */

// GSOURCE_GLUE

static gboolean keepalive_timeout_prepare_cb  (GSource *srce, gint *timeout);
static gboolean keepalive_timeout_check_cb    (GSource *srce);
static gboolean keepalive_timeout_dispatch_cb (GSource *srce, GSourceFunc cb, gpointer aptr);
static void     keepalive_timeout_finalize_cb (GSource *srce);

// BACKGROUND_ACTIVITY_GLUE

static void     keepalive_timeout_trigger_cb  (background_activity_t *activity, void *aptr);

/* ========================================================================= *
 * INTERNAL FUNCTIONS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * GSOURCE_GLUE
 * ------------------------------------------------------------------------- */

static
gboolean
keepalive_timeout_prepare_cb(GSource *srce, gint *timeout)
{
    keepalive_timeout_t *self = (keepalive_timeout_t *)srce;

    log_enter_function();

    if( self->kat_triggered )
        return *timeout = 0, TRUE;

    return *timeout = -1, FALSE;
}

static
gboolean
keepalive_timeout_check_cb(GSource *srce)
{
    keepalive_timeout_t *self = (keepalive_timeout_t *)srce;

    log_enter_function();

    return self->kat_triggered;
}

static
gboolean
keepalive_timeout_dispatch_cb(GSource *srce, GSourceFunc cb, gpointer aptr)
{
    keepalive_timeout_t *self = (keepalive_timeout_t *)srce;

    log_enter_function();

    bool repeat = cb(aptr);

    if( repeat )
        background_activity_wait(self->kat_activity);
    else
        background_activity_stop(self->kat_activity);

    self->kat_triggered = false;

    return repeat;
}

static
void
keepalive_timeout_finalize_cb(GSource *srce)
{
    keepalive_timeout_t *self = (keepalive_timeout_t *)srce;

    log_enter_function();

    background_activity_unref(self->kat_activity),
        self->kat_activity = 0;
}

static GSourceFuncs keepalive_timeout_funcs =
{
    .prepare  = keepalive_timeout_prepare_cb,
    .check    = keepalive_timeout_check_cb,
    .dispatch = keepalive_timeout_dispatch_cb,
    .finalize = keepalive_timeout_finalize_cb,
};

/* ------------------------------------------------------------------------- *
 * BACKGROUND_ACTIVITY_GLUE
 * ------------------------------------------------------------------------- */

static
void
keepalive_timeout_trigger_cb(background_activity_t *activity, void *aptr)
{
    (void)activity;

    log_enter_function();

    keepalive_timeout_t *self = aptr;

    /* What happens here is:
     * 1) the io watch for iphb connection caused this function to get called
     * 2) we mark the timer to be in triggered state
     * 3) either prepare or check probe is called before glib mainloop
     *    goes to select again
     * 4) which then causes dispatch callback invocation
     * 5) dispatch function
     *      starts keepalive session
     *      calls timer callback, and based on return value
     *      restarts/stops background activity
     */
    self->kat_triggered = true;
}

/* ========================================================================= *
 * EXTERNAL API --  documented in: keepalive-timeout.h
 * ========================================================================= */

guint
keepalive_timeout_add_full(gint priority,
                           guint interval,
                           GSourceFunc func,
                           gpointer data,
                           GDestroyNotify notify)
{
    guint id = 0;

    keepalive_timeout_t *self = (keepalive_timeout_t *)
        g_source_new(&keepalive_timeout_funcs, sizeof *self);

    if( !self )
        goto cleanup;

    self->kat_activity  = background_activity_new();
    self->kat_triggered = false;

    background_activity_set_running_callback(self->kat_activity,
                                             keepalive_timeout_trigger_cb);
    background_activity_set_user_data(self->kat_activity,
                                      self, 0);

    /* Minimum wait as requested */
    int delay_lo = (interval + 999) / 1000;

    /* Default to: let background object decide maximum wait */
    int delay_hi = -1;

    if( priority <= G_PRIORITY_HIGH ) {
        /* Use tighter wakeup range for high priority timeouts */
        delay_hi = delay_lo + 1;
    }

    background_activity_set_wakeup_range(self->kat_activity,
                                         delay_lo, delay_hi);
    background_activity_wait(self->kat_activity);

    g_source_set_callback((GSource*)self, func, data, notify);
    id = g_source_attach((GSource*)self, 0);

cleanup:
    if( self )
        g_source_unref((GSource*)self);

    return id;
}

guint
keepalive_timeout_add(guint interval,
                      GSourceFunc function,
                      gpointer data)
{
    return keepalive_timeout_add_full(G_PRIORITY_DEFAULT, interval,
                                      function, data, 0);
}

guint
keepalive_timeout_add_seconds_full(gint priority,
                                   guint interval,
                                   GSourceFunc function,
                                   gpointer data,
                                   GDestroyNotify notify)
{
    return keepalive_timeout_add_full(priority, interval * 1000,
                                      function, data, notify);
}

guint
keepalive_timeout_add_seconds(guint interval,
                              GSourceFunc function,
                              gpointer data)
{
    return keepalive_timeout_add_full(G_PRIORITY_DEFAULT, interval * 1000,
                                      function, data, 0);
}
