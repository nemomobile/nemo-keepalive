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

#include "keepalive-backgroundactivity.h"
#include "keepalive-heartbeat.h"
#include "keepalive-cpukeepalive.h"

#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= *
 * CONSTANTS
 * ========================================================================= */

/* Logging prefix for this module */
#define PFIX "background activity"

/** Memory tag for marking live background_activity_t objects */
#define BACKGROUND_ACTIVITY_MAJICK_ALIVE 0x54913336

/** Memory tag for marking dead background_activity_t objects */
#define BACKGROUND_ACTIVITY_MAJICK_DEAD  0x00000000

/* ========================================================================= *
 * TYPES
 * ========================================================================= */

/** Enumeration of states background activity object can be in */
typedef enum
{
    /** Neither waiting for heartbeat wakeup nor blocking suspend */
    BACKGROUND_ACTIVITY_STATE_STOPPED = 0,

    /** Waiting for heartbeat wakeup */
    BACKGROUND_ACTIVITY_STATE_WAITING = 1,

    /** Blocking suspend */
    BACKGROUND_ACTIVITY_STATE_RUNNING = 2,

} background_activity_state_t;

static const char *
background_activity_state_repr(background_activity_state_t state)
{
    const char *res = "UNKNOWN";
    switch( state ) {
    case BACKGROUND_ACTIVITY_STATE_STOPPED: res = "STOPPED"; break;
    case BACKGROUND_ACTIVITY_STATE_WAITING: res = "WAITING"; break;
    case BACKGROUND_ACTIVITY_STATE_RUNNING: res = "RUNNING"; break;
    default: break;
    }
    return res;
}

/** Wakeup delay using either Global slot or range */
typedef struct
{
    /** Global wakeup slot, or BACKGROUND_ACTIVITY_FREQUENCY_RANGE
     *  in case ranged wakeup is to be used */
    background_activity_frequency_t wd_slot;

    /** Minimum ranged wait period length */
    int                             wd_range_lo;

    /** Maximum ranged wait period length */
    int                             wd_range_hi;
} wakeup_delay_t;

/** State data for background activity object
 */
struct background_activity_t
{
    /** Simple memory tag to catch usage of obviously bogus
     *  background_activity_t pointers */
    unsigned                        bga_majick;

    /** Reference count; initially 1, released when drops to 0 */
    unsigned                        bga_ref_count;

    /** Current state: Stopped, Waiting or Running */
    background_activity_state_t     bga_state;

    /** Requested wakeup slot/range */
    wakeup_delay_t                  bga_wakeup_curr;

    /** Last wakeup slot/range actually used for iphb ipc
     *
     * Used for detecting Waiting -> Waiting transitions
     * that need to reprogram the wait time */
    wakeup_delay_t                  bga_wakeup_last;

    /** User data pointer passed to notification callbacks */
    void                           *bga_user_data;

    /** Callback for freeing bga_user_data */
    background_activity_free_fn     bga_user_free;

    /** Notify transition to Running state */
    background_activity_event_fn    bga_running_cb;

    /** Notify transition to Waiting state */
    background_activity_event_fn    bga_waiting_cb;

    /** Notify transition to Stopped state */
    background_activity_event_fn    bga_stopped_cb;

    /** For iphb wakeup ipc with dsme */
    heartbeat_t                    *bga_heartbeat;

    /** For cpu keepalive ipc with mce */
    cpukeepalive_t                 *bga_keepalive;

    // Update also: background_activity_ctor() & background_activity_dtor()
};

/* ========================================================================= *
 * INTERNAL FUNCTION PROTOTYPES
 * ========================================================================= */

// WAKEUP_DELAY

static void wakeup_delay_set_slot  (wakeup_delay_t *self, background_activity_frequency_t slot);
static void wakeup_delay_set_range (wakeup_delay_t *self, int range_lo, int range_hi);
static bool wakeup_delay_eq_p      (const wakeup_delay_t *self, const wakeup_delay_t *that);

// CONSTRUCT_DESTRUCT
static void background_activity_ctor      (background_activity_t *self);
static void background_activity_dtor      (background_activity_t *self);
static bool background_activity_is_valid  (const background_activity_t *self);

// STATE_TRANSITIONS

static background_activity_state_t background_activity_get_state (const background_activity_t *self);
static void                        background_activity_set_state (background_activity_t *self, background_activity_state_t state);
static bool                        background_activity_in_state  (const background_activity_t *self,
                                                                  background_activity_state_t state);

// HEARTBEAT_WAKEUP

static void background_activity_heartbeat_wakeup_cb (void *aptr);

/* ========================================================================= *
 * INTERNAL FUNCTIONS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * WAKEUP_DELAY
 * ------------------------------------------------------------------------- */

/** Default initial wakeup delay */
static const wakeup_delay_t wakeup_delay_default =
{
    .wd_slot     = BACKGROUND_ACTIVITY_FREQUENCY_ONE_HOUR,
    .wd_range_lo = BACKGROUND_ACTIVITY_FREQUENCY_ONE_HOUR,
    .wd_range_hi = BACKGROUND_ACTIVITY_FREQUENCY_ONE_HOUR,
};

/** Set wakeup delay to use global wakeup slot
 *
 * @param self  wake up delay object
 * @param slot  global wakeup slot to use
 */
static void
wakeup_delay_set_slot(wakeup_delay_t *self,
                      background_activity_frequency_t slot)
{
    // basically it is just a second count, but it must be

    // a) not smaller than the smallest allowed global slot

    if( slot < BACKGROUND_ACTIVITY_FREQUENCY_THIRTY_SECONDS )
         slot = BACKGROUND_ACTIVITY_FREQUENCY_THIRTY_SECONDS;

    // b) evenly divisible by the smallest allowed global slot

    slot = slot - (slot % BACKGROUND_ACTIVITY_FREQUENCY_THIRTY_SECONDS);

    self->wd_slot     = slot;
    self->wd_range_lo = slot;
    self->wd_range_hi = slot;
}

/** Set wakeup delay to use wakeup range
 *
 * @param self      wake up delay object
 * @param range_lo  minimum seconds to wait
 * @param range_hi  maximum seconds to wait
 */
static void
wakeup_delay_set_range(wakeup_delay_t *self,
                       int range_lo, int range_hi)
{
    /* Currently there is no way to tell what kind of hw watchdog
     * kicking period dsme is using - assume that it is 12 seconds */
    const int heartbeat_period = 12;

    /* Zero wait is not supported */
    if( range_lo < 1 )
         range_lo = 1;

    /* Expand invalid range to heartbeat length */
    if( range_hi <= range_lo )
         range_hi = range_lo + heartbeat_period;

    self->wd_slot     = BACKGROUND_ACTIVITY_FREQUENCY_RANGE;
    self->wd_range_lo = range_lo;
    self->wd_range_hi = range_hi;
}

/** Predicate for: two wake up delay objects are the same
 *
 * @param self      wake up delay object
 */
static bool
wakeup_delay_eq_p(const wakeup_delay_t *self, const wakeup_delay_t *that)
{
    return (self->wd_slot     == that->wd_slot     &&
            self->wd_range_lo == that->wd_range_lo &&
            self->wd_range_hi == that->wd_range_hi);
}

/* ------------------------------------------------------------------------- *
 * CONSTRUCT_DESTRUCT
 * ------------------------------------------------------------------------- */

/** Check if background activity pointer is valid
 *
 * @param self  background activity object pointer
 *
 * @return true if pointer is likely to be valid, false otherwise
 */
static bool
background_activity_is_valid(const background_activity_t *self)
{
    return self && self->bga_majick == BACKGROUND_ACTIVITY_MAJICK_ALIVE;
}

/** Construct background activity object
 *
 * @param self  pointer to uninitialized background activity object
 */
static void
background_activity_ctor(background_activity_t *self)
{
    /* Flag object as valid */
    self->bga_majick = BACKGROUND_ACTIVITY_MAJICK_ALIVE;

    /* Initial ref count is one */
    self->bga_ref_count = 1;

    /* In stopped state */
    self->bga_state = BACKGROUND_ACTIVITY_STATE_STOPPED;

    /* Sane wakeup delay defaults */
    self->bga_wakeup_curr = wakeup_delay_default;
    self->bga_wakeup_last = wakeup_delay_default;

    /* No user data */
    self->bga_user_data = 0;
    self->bga_user_free = 0;

    /* No notification callbacks */
    self->bga_running_cb = 0;
    self->bga_waiting_cb = 0;
    self->bga_stopped_cb = 0;

    /* Heartbeat object for waking up */
    self->bga_heartbeat  = heartbeat_new();
    heartbeat_set_notify(self->bga_heartbeat,
                         background_activity_heartbeat_wakeup_cb,
                         self, 0);

    /* Keepalive object for staying up */
    self->bga_keepalive  = cpukeepalive_new();

    log_debug(PFIX"(%s): created", background_activity_get_id(self));
}

/** Construct background activity object
 *
 * @param self  background activity object pointer
 */
static void
background_activity_dtor(background_activity_t *self)
{
    log_debug(PFIX"(%s): deleted", background_activity_get_id(self));

    /* Relase user data */
    background_activity_free_user_data(self);

    /* Detach heartbeat object */
    heartbeat_unref(self->bga_heartbeat),
        self->bga_heartbeat = 0;

    /* Detach keepalive object */
    cpukeepalive_unref(self->bga_keepalive),
        self->bga_keepalive = 0;

    /* Flag object as invalid */
    self->bga_majick = BACKGROUND_ACTIVITY_MAJICK_DEAD;
}

/* ------------------------------------------------------------------------- *
 * STATE_TRANSITIONS
 * ------------------------------------------------------------------------- */

/* Set state of background activity object
 *
 * @param self   background activity object pointer
 * @param state  BACKGROUND_ACTIVITY_STATE_STOPPED|WAITING|RUNNING
 */
static void
background_activity_set_state(background_activity_t *self,
                              background_activity_state_t state)
{
    /* This function is called directly from public helpers, so
     * the object pointer must be validated */
    if( !background_activity_is_valid(self) )
        goto cleanup;

    /* Skip if state does not change; note that changing the length
     * of wait while already waiting is considered a state change */
    if( self->bga_state == state ) {
        if( state != BACKGROUND_ACTIVITY_STATE_WAITING )
            goto cleanup;

        if( wakeup_delay_eq_p(&self->bga_wakeup_curr,
                              &self->bga_wakeup_last) )
            goto cleanup;
    }

    log_notice(PFIX"(%s): state: %s -> %s",
               background_activity_get_id(self),
               background_activity_state_repr(self->bga_state),
               background_activity_state_repr(state));

    /* leave old state */
    bool was_running = false;

    switch( self->bga_state ) {
    case BACKGROUND_ACTIVITY_STATE_STOPPED:
        break;

    case BACKGROUND_ACTIVITY_STATE_WAITING:
        /* heartbeat timer can be cancelled before state transition */
        heartbeat_stop(self->bga_heartbeat);
        break;

    case BACKGROUND_ACTIVITY_STATE_RUNNING:
        /* keepalive timer must be cancelled after state transition */
        was_running = true;
        break;
    }

    /* enter new state */
    switch( state ) {
    case BACKGROUND_ACTIVITY_STATE_STOPPED:
        break;

    case BACKGROUND_ACTIVITY_STATE_WAITING:
        heartbeat_set_delay(self->bga_heartbeat,
                            self->bga_wakeup_curr.wd_range_lo,
                            self->bga_wakeup_curr.wd_range_hi);

        self->bga_wakeup_last = self->bga_wakeup_curr;

        heartbeat_start(self->bga_heartbeat);
        break;

    case BACKGROUND_ACTIVITY_STATE_RUNNING:
        cpukeepalive_start(self->bga_keepalive);
        break;
    }

    /* special case: allow heartbeat timer reprogramming
     * to occur before stopping the keepalive period */
    if( was_running )
        cpukeepalive_stop(self->bga_keepalive);

    /* skip notifications if state does not actually change */
    if( self->bga_state == state )
        goto cleanup;

    /* NOTE: To minimize hazards no member data modifications are
     *       allowed after the notification callbacks are called!
     */
    switch( (self->bga_state = state) ) {
    case BACKGROUND_ACTIVITY_STATE_STOPPED:
        if( self->bga_stopped_cb )
            self->bga_stopped_cb(self, self->bga_user_data);
        break;

    case BACKGROUND_ACTIVITY_STATE_WAITING:
        if( self->bga_waiting_cb )
            self->bga_waiting_cb(self, self->bga_user_data);
        break;

    case BACKGROUND_ACTIVITY_STATE_RUNNING:
        if( self->bga_running_cb ) {
            /* Whatever happens at the callback function, it
             * MUST end up with a call background_activity_stop()
             * or background_activity_wait() or the suspend can
             * be blocked until the process makes an exit */
            self->bga_running_cb(self, self->bga_user_data);
        }
        else {
            /* Refuse to stay in Running state if notification
             * callback is not registered */
            background_activity_stop(self);
        }
        break;
    }

cleanup:

    return;
}

/** Get state of background activity object
 *
 * @param self  background activity object pointer
 *
 * @return BACKGROUND_ACTIVITY_STATE_STOPPED|WAITING|RUNNING
 */
static background_activity_state_t
background_activity_get_state(const background_activity_t *self)
{
    return self->bga_state;
}

/** Predicate function for checking state of background activity object
 *
 * @param self   background activity object pointer
 * @param state  state to check
 *
 * @return true if object is valid and in the given state, false otherwise
 */
static bool
background_activity_in_state(const background_activity_t *self,
                             background_activity_state_t state)
{
    bool in_state = false;

    /* This function is called directly from public helpers, so
     * the object pointer must be validated */
    if( !background_activity_is_valid(self) )
        goto cleanup;

    if( background_activity_get_state(self) == state )
        in_state = true;

cleanup:
    return in_state;
}

/* ------------------------------------------------------------------------- *
 * HEARTBEAT_WAKEUP
 * ------------------------------------------------------------------------- */

/** Handle heartbeat wakeup
 *
 * @param aptr background activity object as void pointer
 */
static void
background_activity_heartbeat_wakeup_cb(void *aptr)
{
    background_activity_t *self = aptr;

    log_notice(PFIX"(%s): iphb wakeup", background_activity_get_id(self));

    if( background_activity_is_waiting(self) )
        background_activity_run(self);
}

/* ========================================================================= *
 * EXTERNAL API  --  documented in: keepalive-backgroundactivity.h
 * ========================================================================= */

background_activity_t *
background_activity_new(void)
{
    log_enter_function();

    background_activity_t *self = calloc(1, sizeof *self);

    if( self )
        background_activity_ctor(self);

    return self;
}

background_activity_t *
background_activity_ref(background_activity_t *self)
{
    log_enter_function();

    background_activity_t *ref = 0;

    if( !background_activity_is_valid(self) )
        goto cleanup;

    ++self->bga_ref_count;

    ref = self;

cleanup:
    return ref;
}

void
background_activity_unref(background_activity_t *self)
{
    log_enter_function();

    if( !background_activity_is_valid(self) )
        goto cleanup;

    if( --self->bga_ref_count != 0 )
         goto cleanup;

    background_activity_dtor(self);
    free(self);

cleanup:
    return;
}

background_activity_frequency_t
background_activity_get_wakeup_slot(const background_activity_t *self)
{
    background_activity_frequency_t slot = BACKGROUND_ACTIVITY_FREQUENCY_RANGE;

    if( background_activity_is_valid(self) )
        slot = self->bga_wakeup_curr.wd_slot;

    return slot;
}

void
background_activity_set_wakeup_slot(background_activity_t *self,
                                    background_activity_frequency_t slot)
{
    if( !background_activity_is_valid(self) )
        goto cleanup;

    wakeup_delay_set_slot(&self->bga_wakeup_curr, slot);

cleanup:
    return;
}

void
background_activity_get_wakeup_range(const background_activity_t *self,
                                     int *range_lo, int *range_hi)
{
    if( !background_activity_is_valid(self) )
        goto cleanup;

    *range_lo = self->bga_wakeup_curr.wd_range_lo;
    *range_hi = self->bga_wakeup_curr.wd_range_hi;

cleanup:
    return;
}

void
background_activity_set_wakeup_range(background_activity_t *self,
                                     int range_lo, int range_hi)
{
    if( !background_activity_is_valid(self) )
        goto cleanup;

    wakeup_delay_set_range(&self->bga_wakeup_curr, range_lo, range_hi);

cleanup:
    return;
}

bool
background_activity_is_waiting(const background_activity_t *self)
{
    /* The self pointer is validated by background_activity_in_state() */
    return background_activity_in_state(self,
                                        BACKGROUND_ACTIVITY_STATE_WAITING);
}

bool
background_activity_is_running(const background_activity_t *self)
{
    /* The self pointer is validated by background_activity_in_state() */
    return background_activity_in_state(self,
                                        BACKGROUND_ACTIVITY_STATE_RUNNING);
}

bool
background_activity_is_stopped(const background_activity_t *self)
{
    /* The self pointer is validated by background_activity_in_state() */
    return background_activity_in_state(self,
                                        BACKGROUND_ACTIVITY_STATE_STOPPED);
}

void background_activity_wait(background_activity_t *self)
{
    /* The self pointer is validated by background_activity_set_state() */
    background_activity_set_state(self, BACKGROUND_ACTIVITY_STATE_WAITING);
}

void background_activity_run(background_activity_t *self)
{
    /* The self pointer is validated by background_activity_set_state() */
    background_activity_set_state(self, BACKGROUND_ACTIVITY_STATE_RUNNING);
}

void background_activity_stop(background_activity_t *self)
{
    /* The self pointer is validated by background_activity_set_state() */
    background_activity_set_state(self, BACKGROUND_ACTIVITY_STATE_STOPPED);
}

const char *
background_activity_get_id(const background_activity_t *self)
{
    const char *id = 0;

    if( !background_activity_is_valid(self) )
        goto cleanup;

    id = cpukeepalive_get_id(self->bga_keepalive);

cleanup:
    return id;
}

void
background_activity_free_user_data(background_activity_t *self)
{
    if( !background_activity_is_valid(self) )
        goto cleanup;

    if( self->bga_user_data && self->bga_user_free )
         self->bga_user_free(self->bga_user_data);

    self->bga_user_data = 0;
    self->bga_user_free = 0;

cleanup:
    return;
}

void *
background_activity_get_user_data(const background_activity_t *self)
{
    void *user_data = 0;

    if( !background_activity_is_valid(self) )
        goto cleanup;

    user_data = self->bga_user_data;

cleanup:
    return user_data;
}

void *
background_activity_steal_user_data(background_activity_t *self)
{
    void *user_data = 0;

    if( !background_activity_is_valid(self) )
        goto cleanup;

    user_data = self->bga_user_data;

    self->bga_user_data = 0;
    self->bga_user_free = 0;

cleanup:
    return user_data;
}

void
background_activity_set_user_data(background_activity_t *self,
                                  void *user_data,
                                  background_activity_free_fn free_cb)
{
    if( !background_activity_is_valid(self) )
        goto cleanup;

    /* Release old user data */
    background_activity_free_user_data(self);

    /* Attach new user data */
    self->bga_user_data = user_data;
    self->bga_user_free = free_cb;

cleanup:
    return;
}

void
background_activity_set_running_callback(background_activity_t *self,
                                         background_activity_event_fn cb)
{
    if( !background_activity_is_valid(self) )
        goto cleanup;

    self->bga_running_cb = cb;

cleanup:
    return;
}

void
background_activity_set_waiting_callback(background_activity_t *self,
                                         background_activity_event_fn cb)
{
    if( !background_activity_is_valid(self) )
        goto cleanup;

    self->bga_waiting_cb = cb;

cleanup:
    return;
}

void
background_activity_set_stopped_callback(background_activity_t *self,
                                         background_activity_event_fn cb)
{
    if( !background_activity_is_valid(self) )
        goto cleanup;

    self->bga_stopped_cb = cb;

cleanup:
    return;
}
