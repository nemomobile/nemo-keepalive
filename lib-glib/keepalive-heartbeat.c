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

#include "keepalive-heartbeat.h"

#include "logging.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <iphbd/libiphb.h>

#include <glib.h>

/* ========================================================================= *
 * CONSTANTS
 * ========================================================================= */

/** Memory tag for marking live heartbeat_t objects */
#define HB_MAJICK_ALIVE 0x5492a037

/** Memory tag for marking dead heartbeat_t objects */
#define HB_MAJICK_DEAD  0x00000000

/** Delay between iphb connect attempts */
#define HB_CONNECT_TIMEOUT_MS (5 * 1000)

/* Logging prefix for this module */
#define PFIX "heartbeat: "

/* ========================================================================= *
 * TYPES
 * ========================================================================= */

struct heartbeat_t
{
    /** Simple memory tag to catch obviously bogus heartbeat_t pointers */
    unsigned             hb_majick;

    /** Reference count; initially 1, released when drops to 0 */
    unsigned             hb_refcount;

    /** Current minimum wakeup wait length */
    int                  hb_delay_lo;

    /** Current maximum wakeup wait length */
    int                  hb_delay_hi;

    /** Flag for: wakeup has been requested  */
    bool                 hb_started;

    /** Flag for: wakeup has been programmed */
    bool                 hb_waiting;

    /** IPHB connection handle */
    iphb_t               hb_iphb_handle;

    /** I/O watch id for hb_iphb_handle file descriptor */
    guint                hb_wakeup_watch_id;

    /** Timer id for: retrying connection attempts */
    guint                hb_connect_timer_id;

    /** User data to be passed for hb_user_notify */
    void                *hb_user_data;

    /** Free callback to be used for releasing hb_user_data */
    heartbeat_free_fn    hb_user_free;

    /** Wakeup notification callback set via heartbeat_set_notify() */
    heartbeat_wakeup_fn  hb_user_notify;
};

/* ========================================================================= *
 * INTERNAL FUNCTION PROTOTYPES
 * ========================================================================= */

// UTILITY
static guint    heartbeat_add_iowatch                   (int fd, bool close_on_unref, GIOCondition cnd, GIOFunc io_cb, gpointer aptr);

// CONSTRUCT_DESTRUCT
static void     heartbeat_ctor                          (heartbeat_t *self);
static void     heartbeat_dtor                          (heartbeat_t *self);
static bool     heartbeat_is_valid                      (const heartbeat_t *self);

// USER_DATA
static void     heartbeat_user_data_clear               (heartbeat_t *self);

// IPHB_WAKEUP
static gboolean heartbeat_iphb_wakeup_cb                (GIOChannel *chn, GIOCondition cnd, gpointer data);
static void     heartbeat_iphb_wakeup_schedule          (heartbeat_t *self);

// IPHB_CONNECT_TIMER
static gboolean heartbeat_iphb_connect_timer_cb         (gpointer aptr);
static void     heartbeat_iphb_connect_timer_stop       (heartbeat_t *self);
static void     heartbeat_iphb_connect_timer_start      (heartbeat_t *self);
static bool     heartbeat_iphb_connect_timer_is_active  (const heartbeat_t *self);

// IPHB_CONNECTION
static bool     heartbeat_iphb_connection_try_open      (heartbeat_t *self);
static void     heartbeat_iphb_connection_open          (heartbeat_t *self);
static void     heartbeat_iphb_connection_close         (heartbeat_t *self);

/* ========================================================================= *
 * INTERNAL FUNCTIONS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

/** Helper for creating I/O watch for file descriptor
 */
static guint
heartbeat_add_iowatch(int fd, bool close_on_unref,
                      GIOCondition cnd, GIOFunc io_cb, gpointer aptr)
{
    log_enter_function();

    guint         wid = 0;
    GIOChannel   *chn = 0;

    if( !(chn = g_io_channel_unix_new(fd)) )
        goto cleanup;

    g_io_channel_set_close_on_unref(chn, close_on_unref);

    cnd |= G_IO_ERR | G_IO_HUP | G_IO_NVAL;

    if( !(wid = g_io_add_watch(chn, cnd, io_cb, aptr)) )
        goto cleanup;

cleanup:
    if( chn != 0 ) g_io_channel_unref(chn);

    return wid;

}

/* ------------------------------------------------------------------------- *
 * CONSTRUCT_DESTRUCT
 * ------------------------------------------------------------------------- */

/** Construct heartbeat object
 *
 * @param self  pointer to uninitialized heartbeat object
 */
static void
heartbeat_ctor(heartbeat_t *self)
{
    /* Mark object as valid */
    self->hb_majick   = HB_MAJICK_ALIVE;

    /* Init refcount book keeping */
    self->hb_refcount = 1;

    /* Sane default wait period */
    self->hb_delay_lo = 60 * 60;
    self->hb_delay_hi = 60 * 60;

    /* Clear state data */
    self->hb_started  = false;
    self->hb_waiting  = false;

    /* No iphb connection */
    self->hb_iphb_handle      = 0;
    self->hb_wakeup_watch_id  = 0;
    self->hb_connect_timer_id = 0;

    /* No user data */
    self->hb_user_data   = 0;
    self->hb_user_free   = 0;

    /* No notification callback */
    self->hb_user_notify = 0;
}

/** Destruct heartbeat object
 *
 * @param self  heartbeat object
 */
static void
heartbeat_dtor(heartbeat_t *self)
{
    /* Break iphb connection */
    heartbeat_iphb_connection_close(self);

    /* Stop reconnect attempts */
    heartbeat_iphb_connect_timer_stop(self);

    /* Release user data */
    heartbeat_user_data_clear(self);

    /* Mark object as invalid */
    self->hb_majick = HB_MAJICK_DEAD;
}

/** Predicate for: heartbeat object is valid
 *
 * @param self  heartbeat object
 */
static bool
heartbeat_is_valid(const heartbeat_t *self)
{
    return self != 0 && self->hb_majick == HB_MAJICK_ALIVE;
}

/* ------------------------------------------------------------------------- *
 * USER_DATA
 * ------------------------------------------------------------------------- */

/** Release user data
 *
 * @param self  heartbeat object
 */
static void
heartbeat_user_data_clear(heartbeat_t *self)
{
    log_enter_function();

    if( self->hb_user_data && self->hb_user_free )
        self->hb_user_free(self->hb_user_data);

    self->hb_user_data   = 0;
    self->hb_user_free   = 0;
}

/* ------------------------------------------------------------------------- *
 * IPHB_WAKEUP
 * ------------------------------------------------------------------------- */

/** Calback for handling iphb wakeups
 *
 * @param chn  io channel
 * @param cnd  io condition
 * @param data heartbeat object as void pointer
 *
 * @return TRUE to keep io watch alive, or FALSE to disable it
 */
static gboolean
heartbeat_iphb_wakeup_cb(GIOChannel *chn,
                         GIOCondition cnd,
                         gpointer data)
{
    log_enter_function();

    gboolean keep_going = FALSE;

    heartbeat_t *self = data;

    int fd = g_io_channel_unix_get_fd(chn);

    if( fd < 0 )
        goto cleanup_nak;

    if( cnd & ~G_IO_IN )
        goto cleanup_nak;

    char buf[256];

    /* Stopping/reprogramming iphb flushes pending input
     * from the socket. If that happens after decision
     * to call this input callback is already made, simple
     * read could block and that can't be allowed. */
    int rc = recv(fd, buf, sizeof buf, MSG_DONTWAIT);

    if( rc == 0 ) {
        log_error(PFIX"unexpected eof");
        goto cleanup_nak;
    }

    if( rc == -1 ) {
        if( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK )
            goto cleanup_ack;

        log_error(PFIX"read error: %m");
        goto cleanup_nak;
    }

    if( !self->hb_waiting )
        goto cleanup_ack;

    /* clear state data */
    self->hb_started  = false;
    self->hb_waiting  = false;

    /* notify */
    if( self->hb_user_notify )
        self->hb_user_notify(self->hb_user_data);

cleanup_ack:
    keep_going = TRUE;

cleanup_nak:

    if( !keep_going ) {
        self->hb_wakeup_watch_id = 0;

        bool was_started = self->hb_started;
        heartbeat_iphb_connection_close(self);

        self->hb_started = was_started;
        heartbeat_iphb_connection_open(self);
    }

    return keep_going;
}

/** Request iphb wakeup at currently active wakeup range/slot
 *
 * @param self  heartbeat object
 */
static void
heartbeat_iphb_wakeup_schedule(heartbeat_t *self)
{
    log_enter_function();

    // must be started
    if( !self->hb_started )
        goto cleanup;

    // but not in waiting state yet
    if( self->hb_waiting )
        goto cleanup;

    // must be connected
    heartbeat_iphb_connection_open(self);
    if( !self->hb_iphb_handle )
        goto cleanup;

    int lo = self->hb_delay_lo;
    int hi = self->hb_delay_hi;
    log_notice(PFIX"iphb_wait2(%d, %d)", lo, hi);
    iphb_wait2(self->hb_iphb_handle, lo, hi, 0, 1);
    self->hb_waiting = true;

cleanup:
    return;
}

/* ------------------------------------------------------------------------- *
 * IPHB_CONNECT_TIMER
 * ------------------------------------------------------------------------- */

/** Callback for connect reattempt timer
 *
 * @param aptr  heartbeat object as void pointer
 *
 * @return TRUE to keep timer repeating, or FALSE to stop it
 */
static gboolean
heartbeat_iphb_connect_timer_cb(gpointer aptr)
{
    heartbeat_t *self = aptr;

    if( !self->hb_wakeup_watch_id )
        goto cleanup;

    log_enter_function();

    if( !heartbeat_iphb_connection_try_open(self) )
        goto cleanup;

    self->hb_wakeup_watch_id = 0;

    heartbeat_iphb_wakeup_schedule(self);

cleanup:
    return self->hb_wakeup_watch_id != 0;
}

/** Cancel connect reattempt timer
 *
 * @param aptr  heartbeat object as void pointer
 */
static void
heartbeat_iphb_connect_timer_stop(heartbeat_t *self)
{
    if( self->hb_connect_timer_id ) {
        log_enter_function();

        g_source_remove(self->hb_connect_timer_id),
            self->hb_connect_timer_id = 0;
    }
}

/** Start connect reattempt timer
 *
 * @param aptr  heartbeat object as void pointer
 */
static void
heartbeat_iphb_connect_timer_start(heartbeat_t *self)
{
    if( !self->hb_connect_timer_id ) {
        log_enter_function();

        self->hb_connect_timer_id =
            g_timeout_add(HB_CONNECT_TIMEOUT_MS,
                          heartbeat_iphb_connect_timer_cb,
                          self);
    }
}

/** Predicate for connect reattempt timer is active
 *
 * @param aptr  heartbeat object as void pointer
 */
static bool
heartbeat_iphb_connect_timer_is_active(const heartbeat_t *self)
{
    return self->hb_connect_timer_id != 0;
}

/* ------------------------------------------------------------------------- *
 * IPHB_CONNECTION
 * ------------------------------------------------------------------------- */

/** Try to establish iphb socket connection now
 *
 * @param aptr  heartbeat object as void pointer
 */
static bool
heartbeat_iphb_connection_try_open(heartbeat_t *self)
{
    iphb_t handle = 0;

    if( self->hb_iphb_handle )
        goto cleanup;

    log_enter_function();

    if( !(handle = iphb_open(0)) ) {
        log_warning(PFIX"iphb_open: %m");
        goto cleanup;
    }

    int fd;

    if( (fd = iphb_get_fd(handle)) == -1 ) {
        log_warning(PFIX"iphb_get_fd: %m");
        goto cleanup;
    }

    /* set up io watch */
    self->hb_wakeup_watch_id =
        heartbeat_add_iowatch(fd, false, G_IO_IN,
                              heartbeat_iphb_wakeup_cb, self);

    if( !self->hb_wakeup_watch_id )
        goto cleanup;

    /* heartbeat_t owns the handle */
    self->hb_iphb_handle = handle, handle = 0;

cleanup:

    if( handle ) iphb_close(handle);

    return self->hb_iphb_handle != 0;
}

/** Start connecting to iphb socket
 *
 * @param aptr  heartbeat object as void pointer
 */
static void
heartbeat_iphb_connection_open(heartbeat_t *self)
{
    log_enter_function();

    if( heartbeat_iphb_connect_timer_is_active(self) ) {
        // Retry timer already set up
    }
    else if( !heartbeat_iphb_connection_try_open(self) ) {
        // Could not connect now - start retry timer
        heartbeat_iphb_connect_timer_start(self);
    }
}

/** Close connection to iphb socket
 *
 * @param aptr  heartbeat object as void pointer
 */
static void
heartbeat_iphb_connection_close(heartbeat_t *self)
{
    log_enter_function();

    /* stop iphb timer */
    heartbeat_stop(self);

    /* remove io watch */
    if( self->hb_wakeup_watch_id ) {
        g_source_remove(self->hb_wakeup_watch_id),
            self->hb_wakeup_watch_id = 0;
    }

    /* close handle */
    if( self->hb_iphb_handle ) {
        iphb_close(self->hb_iphb_handle),
            self->hb_iphb_handle = 0;
    }
}

/* ========================================================================= *
 * EXTERNAL API --  documented in: keepalive-hearbeat.h
 * ========================================================================= */

heartbeat_t *
heartbeat_new(void)
{
    log_enter_function();

    heartbeat_t *self = calloc(1, sizeof *self);

    if( self )
        heartbeat_ctor(self);

    return self;
}

heartbeat_t *
heartbeat_ref(heartbeat_t *self)
{
    log_enter_function();

    if( !heartbeat_is_valid(self) )
        return 0;

    ++self->hb_refcount;
    return self;
}

void
heartbeat_unref(heartbeat_t *self)
{
    log_enter_function();

    if( !heartbeat_is_valid(self) )
        goto cleanup;

    if( --self->hb_refcount > 0 )
        goto cleanup;

    heartbeat_dtor(self);
    free(self);

cleanup:
    return;
}

void
heartbeat_set_notify(heartbeat_t *self,
                     heartbeat_wakeup_fn notify_cb,
                     void *user_data,
                     heartbeat_free_fn user_free_cb)
{
    log_enter_function();

    heartbeat_user_data_clear(self);

    self->hb_user_data   = user_data;
    self->hb_user_free   = user_free_cb;

    self->hb_user_notify = notify_cb;

}

void
heartbeat_set_delay(heartbeat_t *self, int delay_lo, int delay_hi)
{
    log_enter_function();

    if( !heartbeat_is_valid(self) )
        goto cleanup;

    if( delay_lo < 1 )
        delay_lo = 1;

    if( delay_hi < delay_lo )
        delay_hi = delay_lo;

    self->hb_delay_lo = delay_lo;
    self->hb_delay_hi = delay_hi;

cleanup:
    return;
}

void
heartbeat_start(heartbeat_t *self)
{
    log_enter_function();

    if( !heartbeat_is_valid(self) )
        goto cleanup;

    self->hb_started = true;
    heartbeat_iphb_wakeup_schedule(self);

cleanup:
    return;
}

void
heartbeat_stop(heartbeat_t *self)
{
    log_enter_function();

    if( !heartbeat_is_valid(self) )
        goto cleanup;

    if( self->hb_waiting && self->hb_iphb_handle )
        iphb_wait2(self->hb_iphb_handle, 0, 0, 0, 0);

    self->hb_waiting = false;
    self->hb_started = false;

cleanup:
    return;
}
