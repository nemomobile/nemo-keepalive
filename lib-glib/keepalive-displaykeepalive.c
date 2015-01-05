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

#include "keepalive-displaykeepalive.h"

#include "xdbus.h"
#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <glib.h>
#include <dbus/dbus.h>

#include <mce/dbus-names.h>
#include <mce/mode-names.h>

/** Display keepalive renew time */
#define DISPLAY_KEEPALIVE_RENEW_MS (60 * 1000)

/* Logging prefix for this module */
#define PFIX "displaykeepalive: "

/* ========================================================================= *
 * GENERIC HELPERS
 * ========================================================================= */

static inline bool
eq(const char *a, const char *b)
{
    return (a && b) ? !strcmp(a, b) : (a == b);
}

/* ========================================================================= *
 * TYPES
 * ========================================================================= */

/** Enumeration of states a D-Bus service can be in */
typedef enum {
    NAMEOWNER_UNKNOWN,
    NAMEOWNER_STOPPED,
    NAMEOWNER_RUNNING,
} nameowner_t;

/** Enumeration of states display can be in */
typedef enum {
    DISPLAYSTATE_UNKNOWN,
    DISPLAYSTATE_OFF,
    DISPLAYSTATE_DIM,
    DISPLAYSTATE_ON,
} displaystate_t;

/** Enumeration of states tklock can be in */
typedef enum
{
    TKLOCK_UNKNOWN,
    TKLOCK_LOCKED,
    TKLOCK_UNLOCKED,
} tklockstate_t;

/** Memory tag for marking live displaykeepalive_t objects */
#define DISPLAYKEEPALIVE_MAJICK_ALIVE 0x548ead10

/** Memory tag for marking dead displaykeepalive_t objects */
#define DISPLAYKEEPALIVE_MAJICK_DEAD  0x00000000

/** Display keepalive state object
 */
struct displaykeepalive_t
{
    /** Simple memory tag to catch usage of obviously bogus
     *  displaykeepalive_t pointers */
    unsigned         dka_majick;

    /** Reference count; initially 1, released when drops to 0 */
    unsigned         dka_refcount;

    /** Flag for: preventing display blanking requested */
    bool             dka_requested;

    /** Flag for: we've already tried to connect to system bus */
    bool             dka_connect_attempted;

    /** System bus connection */
    DBusConnection  *dka_systembus;

    /** Flag for: signal filters installed */
    bool             dka_filter_added;

    /** Current tklock state */
    tklockstate_t    dka_tklock_state;

    /** Async dbus query for initial dka_tklock_state value */
    DBusPendingCall *dka_tklock_state_pc;

    /** Current display state */
    displaystate_t   dka_display_state;

    /** Async dbus query for initial dka_display_state value */
    DBusPendingCall *dka_display_state_pc;

    /** Current com.nokia.mce name ownership state */
    nameowner_t      dka_mce_service;

    /** Async dbus query for initial dka_mce_service value */
    DBusPendingCall *dka_mce_service_pc;

    /** Timer id for active display keepalive session */
    guint            dka_renew_timer_id;

    /** Idle callback id for starting/stopping keepalive session */
    guint            dka_rethink_id;

    // NOTE: displaykeepalive_ctor & displaykeepalive_dtor
};

/* ========================================================================= *
 * INTERNAL FUNCTIONS
 * ========================================================================= */

// CONSTRUCT_DESTRUCT
static void              displaykeepalive_ctor                          (displaykeepalive_t *self);
static void              displaykeepalive_dtor                          (displaykeepalive_t *self);
static bool              displaykeepalive_is_valid                      (const displaykeepalive_t *self);

// KEEPALIVE_SESSION
static void              displaykeepalive_session_ipc                   (displaykeepalive_t *self, const char *method);
static gboolean          displaykeepalive_session_cb                    (gpointer aptr);
static void              displaykeepalive_session_start                 (displaykeepalive_t *self);
static void              displaykeepalive_session_stop                  (displaykeepalive_t *self);

// RETHINK_STATE
static void              displaykeepalive_rethink_now                   (displaykeepalive_t *self);
static gboolean          displaykeepalive_rethink_idle_cb               (gpointer aptr);
static void              displaykeepalive_rethink_schedule              (displaykeepalive_t *self);
static void              displaykeepalive_rethink_cancel                (displaykeepalive_t *self);

// MCE_SERVICE_TRACKING
static nameowner_t       displaykeepalive_mce_owner_get                 (const displaykeepalive_t *self);
static void              displaykeepalive_mce_owner_set                 (displaykeepalive_t *self, nameowner_t state);
static void              displaykeepalive_mce_owner_query_reply_cb      (DBusPendingCall *pc, void *aptr);
static void              displaykeepalive_mce_owner_query_start         (displaykeepalive_t *self);
static void              displaykeepalive_mce_owner_query_cancel        (displaykeepalive_t *self);

// TKLOCK_STATE_TRACKING
static tklockstate_t     displaykeepalive_tklock_get                    (const displaykeepalive_t *self);
static void              displaykeepalive_tklock_set                    (displaykeepalive_t *self, tklockstate_t state);
static void              displaykeepalive_tklock_query_reply_cb         (DBusPendingCall *pc, void *aptr);
static void              displaykeepalive_tklock_query_start            (displaykeepalive_t *self);
static void              displaykeepalive_tklock_query_cancel           (displaykeepalive_t *self);

// DISPLAY_STATE_TRACKING
static displaystate_t    displaykeepalive_display_get                   (const displaykeepalive_t *self);
static void              displaykeepalive_display_set                   (displaykeepalive_t *self, displaystate_t state);
static void              displaykeepalive_display_query_reply_cb        (DBusPendingCall *pc, void *aptr);
static void              displaykeepalive_display_query_start           (displaykeepalive_t *self);
static void              displaykeepalive_display_query_cancel          (displaykeepalive_t *self);

// DBUS_SIGNAL_HANDLING
static void              displaykeepalive_dbus_tklock_signal_cb         (displaykeepalive_t *self, DBusMessage *sig);
static void              displaykeepalive_dbus_display_signal_cb        (displaykeepalive_t *self, DBusMessage *sig);
static void              displaykeepalive_dbus_nameowner_signal_cb      (displaykeepalive_t *self, DBusMessage *sig);

// DBUS_MESSAGE_FILTERS
static DBusHandlerResult displaykeepalive_dbus_filter_cb                (DBusConnection *con, DBusMessage *msg, void *aptr);
static void              displaykeepalive_dbus_filter_install           (displaykeepalive_t *self);
static void              displaykeepalive_dbus_filter_remove            (displaykeepalive_t *self);

// DBUS_CONNECTION
static void              displaykeepalive_dbus_connect                  (displaykeepalive_t *self);
static void              displaykeepalive_dbus_disconnect               (displaykeepalive_t *self);

/* ------------------------------------------------------------------------- *
 * CONSTRUCT_DESTRUCT
 * ------------------------------------------------------------------------- */

/** Constructor for displaykeepalive_t objects
 */
static void
displaykeepalive_ctor(displaykeepalive_t *self)
{
    /* Mark as valid */
    self->dka_majick = DISPLAYKEEPALIVE_MAJICK_ALIVE;

    /* Initialize ref count to one */
    self->dka_refcount = 1;

    /* Session neither requested nor running */
    self->dka_requested = false;
    self->dka_renew_timer_id = 0;

    /* No system bus connection */
    self->dka_connect_attempted = false;
    self->dka_systembus = 0;
    self->dka_filter_added = false;

    /* Tklock state is not known */
    self->dka_tklock_state = TKLOCK_UNKNOWN;
    self->dka_tklock_state_pc = 0;

    /* Display state is not known */
    self->dka_display_state = DISPLAYSTATE_UNKNOWN;
    self->dka_display_state_pc = 0;

    /* MCE availability is not known */
    self->dka_mce_service = NAMEOWNER_UNKNOWN;
    self->dka_mce_service_pc = 0;

    /* No pending session rethink scheduled */
    self->dka_rethink_id = 0;
}

/** Destructor for displaykeepalive_t objects
 */
static void
displaykeepalive_dtor(displaykeepalive_t *self)
{
    /* Forced stopping of keepalive session */
    displaykeepalive_stop(self);
    displaykeepalive_rethink_now(self);

    /* Disconnecting also cancels pending async method calls */
    displaykeepalive_dbus_disconnect(self);

    /* Make sure we leave no timers with stale callbacks behind */
    displaykeepalive_rethink_cancel(self);

    /* Mark as invalid */
    self->dka_majick = DISPLAYKEEPALIVE_MAJICK_DEAD;
}

/** Predicate for: displaykeepalive_t object is valid
 */
static bool
displaykeepalive_is_valid(const displaykeepalive_t *self)
{
    return self && self->dka_majick == DISPLAYKEEPALIVE_MAJICK_ALIVE;
}

/* ========================================================================= *
 * KEEPALIVE_SESSION
 * ========================================================================= */

/** Helper for making mce dbus method calls for which we want no reply
 */
static void
displaykeepalive_session_ipc(displaykeepalive_t *self, const char *method)
{
    xdbus_simple_call(self->dka_systembus,
                      MCE_SERVICE,
                      MCE_REQUEST_PATH,
                      MCE_REQUEST_IF,
                      method,
                      DBUS_TYPE_INVALID);
}

/** Timer callback for renewing display keepalive session
 */
static gboolean
displaykeepalive_session_cb(gpointer aptr)
{
    gboolean keep_going = FALSE;
    displaykeepalive_t *self = aptr;

    if( !self->dka_renew_timer_id  )
        goto cleanup;

    log_enter_function();

    displaykeepalive_session_ipc(self, MCE_PREVENT_BLANK_REQ);
    keep_going = TRUE;

cleanup:
    if( !keep_going && self->dka_renew_timer_id  )
        self->dka_renew_timer_id  = 0;

    return keep_going;
}

/** Start display keepalive session
 */
static void
displaykeepalive_session_start(displaykeepalive_t *self)
{
    if( self->dka_renew_timer_id )
        goto cleanup;

    log_enter_function();

    self->dka_renew_timer_id =
        g_timeout_add(DISPLAY_KEEPALIVE_RENEW_MS,
                      displaykeepalive_session_cb, self);

    displaykeepalive_session_ipc(self, MCE_PREVENT_BLANK_REQ);

cleanup:
    return;
}

/** Stop display keepalive session
 */
static void
displaykeepalive_session_stop(displaykeepalive_t *self)
{
    if( !self->dka_renew_timer_id )
        goto cleanup;

    log_enter_function();

    g_source_remove(self->dka_renew_timer_id),
        self->dka_renew_timer_id = 0;

    displaykeepalive_session_ipc(self, MCE_CANCEL_PREVENT_BLANK_REQ);

cleanup:
    return;
}

/* ------------------------------------------------------------------------- *
 * RETHINK_STATE
 * ------------------------------------------------------------------------- */

static void
displaykeepalive_rethink_now(displaykeepalive_t *self)
{
    bool need_renew_loop = false;

    displaykeepalive_rethink_cancel(self);

    /* Preventing display blanking is possible when mce is running,
     * display is on and lockscreen is not active */

    if( displaykeepalive_mce_owner_get(self) != NAMEOWNER_RUNNING )
        goto cleanup;

    if( displaykeepalive_display_get(self) != DISPLAYSTATE_ON )
        goto cleanup;

    if( displaykeepalive_tklock_get(self) != TKLOCK_UNLOCKED )
        goto cleanup;

    need_renew_loop = self->dka_requested;

cleanup:

    if( need_renew_loop )
        displaykeepalive_session_start(self);
    else
        displaykeepalive_session_stop(self);
}

static gboolean
displaykeepalive_rethink_idle_cb(gpointer aptr)
{
    displaykeepalive_t *self = aptr;

    if( !self->dka_rethink_id )
        goto cleanup;

    log_enter_function();

    /* To avoid removing the source id that we're about cancel
     * via returning FALSE from here, we need to clear the idle
     * callback id before calling isplaykeepalive_rethink_now() */
    self->dka_rethink_id = 0;

    displaykeepalive_rethink_now(self);

cleanup:
    return FALSE;
}

static void
displaykeepalive_rethink_schedule(displaykeepalive_t *self)
{
    if( !self->dka_rethink_id ) {
        self->dka_rethink_id =
            g_idle_add(displaykeepalive_rethink_idle_cb, self);
    }
}

static void
displaykeepalive_rethink_cancel(displaykeepalive_t *self)
{
    if( self->dka_rethink_id ) {
        g_source_remove(self->dka_rethink_id),
            self->dka_rethink_id = 0;
    }
}

/* ------------------------------------------------------------------------- *
 * MCE_SERVICE_TRACKING
 * ------------------------------------------------------------------------- */

static nameowner_t
displaykeepalive_mce_owner_get(const displaykeepalive_t *self)
{
    return self->dka_mce_service;
}

static void
displaykeepalive_mce_owner_set(displaykeepalive_t *self,
                                 nameowner_t state)
{
    displaykeepalive_mce_owner_query_cancel(self);

    if( self->dka_mce_service != state ) {
        log_notice(PFIX"MCE_SERVICE: %d -> %d",
                   self->dka_mce_service, state);
        self->dka_mce_service = state;

        if( self->dka_mce_service == NAMEOWNER_RUNNING ) {
            displaykeepalive_tklock_query_start(self);
            displaykeepalive_display_query_start(self);
        }
        else {
            displaykeepalive_tklock_set(self, TKLOCK_UNKNOWN);
            displaykeepalive_display_set(self, DISPLAYSTATE_UNKNOWN);
        }

        displaykeepalive_rethink_schedule(self);
    }
}

static void
displaykeepalive_mce_owner_query_reply_cb(DBusPendingCall *pc, void *aptr)
{
    displaykeepalive_t *self = aptr;
    DBusMessage        *rsp  = 0;
    DBusError           err  = DBUS_ERROR_INIT;

    if( self->dka_mce_service_pc != pc )
        goto cleanup;

    log_enter_function();

    self->dka_mce_service_pc = 0;

    if( !(rsp = dbus_pending_call_steal_reply(pc)) )
        goto cleanup;

    const char *owner = 0;

    if( dbus_set_error_from_message(&err, rsp) ||
        !dbus_message_get_args(rsp, &err,
                               DBUS_TYPE_STRING, &owner,
                               DBUS_TYPE_INVALID) ) {
        if( strcmp(err.name, DBUS_ERROR_NAME_HAS_NO_OWNER) )
            log_warning(PFIX"GetNameOwner reply: %s: %s", err.name, err.message);
    }

    displaykeepalive_mce_owner_set(self,
                                     (owner && *owner) ?
                                     NAMEOWNER_RUNNING : NAMEOWNER_STOPPED);

cleanup:

    if( rsp )
        dbus_message_unref(rsp);

    dbus_error_free(&err);
}

static void
displaykeepalive_mce_owner_query_start(displaykeepalive_t *self)
{
    if( self->dka_mce_service_pc )
        goto cleanup;

    log_enter_function();

    const char *arg = MCE_SERVICE;

    self->dka_mce_service_pc =
        xdbus_method_call(self->dka_systembus,
                          DBUS_SERVICE_DBUS,
                          DBUS_PATH_DBUS,
                          DBUS_INTERFACE_DBUS,
                          "GetNameOwner",
                          displaykeepalive_mce_owner_query_reply_cb,
                          self, 0,
                          DBUS_TYPE_STRING, &arg,
                          DBUS_TYPE_INVALID);
cleanup:
    return;
}

static void
displaykeepalive_mce_owner_query_cancel(displaykeepalive_t *self)
{
    if( self->dka_mce_service_pc ) {
        log_enter_function();

        dbus_pending_call_cancel(self->dka_mce_service_pc),
            self->dka_mce_service_pc = 0;
    }
}

/* ------------------------------------------------------------------------- *
 * TKLOCK_STATE_TRACKING
 * ------------------------------------------------------------------------- */

static tklockstate_t
displaykeepalive_tklock_get(const displaykeepalive_t *self)
{
    return self->dka_tklock_state;
}

static void
displaykeepalive_tklock_set(displaykeepalive_t *self,
                             tklockstate_t state)
{
    displaykeepalive_tklock_query_cancel(self);

    if( self->dka_tklock_state != state ) {
        log_notice(PFIX"TKLOCK_STATE: %d -> %d",
                   self->dka_tklock_state, state);
        self->dka_tklock_state = state;

        displaykeepalive_rethink_schedule(self);
    }
}

static void
displaykeepalive_tklock_query_reply_cb(DBusPendingCall *pc, void *aptr)
{
    displaykeepalive_t *self = aptr;

    DBusMessage *rsp = 0;

    if( self->dka_tklock_state_pc != pc )
        goto cleanup;

    log_enter_function();

    self->dka_tklock_state_pc = 0;

    if( !(rsp = dbus_pending_call_steal_reply(pc)) )
        goto cleanup;

    // reply to query == change signal
    displaykeepalive_dbus_tklock_signal_cb(self, rsp);

cleanup:
    if( rsp )
        dbus_message_unref(rsp);
}

static void
displaykeepalive_tklock_query_cancel(displaykeepalive_t *self)
{
    if( self->dka_tklock_state_pc ) {
        log_enter_function();

        dbus_pending_call_cancel(self->dka_tklock_state_pc),
            self->dka_tklock_state_pc = 0;
    }
}

static void
displaykeepalive_tklock_query_start(displaykeepalive_t *self)
{
    if( self->dka_tklock_state_pc )
        goto cleanup;

    log_enter_function();

    self->dka_tklock_state_pc =
        xdbus_method_call(self->dka_systembus,
                          MCE_SERVICE,
                          MCE_REQUEST_PATH,
                          MCE_REQUEST_IF,
                          MCE_TKLOCK_MODE_GET,
                          displaykeepalive_tklock_query_reply_cb,
                          self, 0,
                          DBUS_TYPE_INVALID);
cleanup:
    return;
}

/* ------------------------------------------------------------------------- *
 * DISPLAY_STATE_TRACKING
 * ------------------------------------------------------------------------- */

static displaystate_t
displaykeepalive_display_get(const displaykeepalive_t *self)
{
    return self->dka_display_state;
}

static void
displaykeepalive_display_set(displaykeepalive_t *self,
                             displaystate_t state)
{
    displaykeepalive_display_query_cancel(self);

    if( self->dka_display_state != state ) {
        log_notice(PFIX"DISPLAY_STATE: %d -> %d",
                   self->dka_display_state, state);
        self->dka_display_state = state;

        displaykeepalive_rethink_schedule(self);
    }
}

static void
displaykeepalive_display_query_reply_cb(DBusPendingCall *pc, void *aptr)
{
    displaykeepalive_t *self = aptr;

    DBusMessage *rsp = 0;

    if( self->dka_display_state_pc != pc )
        goto cleanup;

    log_enter_function();

    self->dka_display_state_pc = 0;

    if( !(rsp = dbus_pending_call_steal_reply(pc)) )
        goto cleanup;

    // reply to query == change signal
    displaykeepalive_dbus_display_signal_cb(self, rsp);

cleanup:
    if( rsp )
        dbus_message_unref(rsp);
}

static void
displaykeepalive_display_query_start(displaykeepalive_t *self)
{
    if( self->dka_display_state_pc )
        goto cleanup;

    log_enter_function();

    self->dka_display_state_pc =
        xdbus_method_call(self->dka_systembus,
                          MCE_SERVICE,
                          MCE_REQUEST_PATH,
                          MCE_REQUEST_IF,
                          MCE_DISPLAY_STATUS_GET,
                          displaykeepalive_display_query_reply_cb,
                          self, 0,
                          DBUS_TYPE_INVALID);
cleanup:
    return;
}

static void
displaykeepalive_display_query_cancel(displaykeepalive_t *self)
{
    if( self->dka_display_state_pc ) {
        log_enter_function();

        dbus_pending_call_cancel(self->dka_display_state_pc),
            self->dka_display_state_pc = 0;
    }
}

/* ------------------------------------------------------------------------- *
 * DBUS_SIGNAL_HANDLING
 * ------------------------------------------------------------------------- */

#define DBUS_NAMEOWENERCHANGED_SIG "NameOwnerChanged"

static void
displaykeepalive_dbus_tklock_signal_cb(displaykeepalive_t *self, DBusMessage *sig)
{
    log_enter_function();

    const char *state_name = 0;

    DBusError err = DBUS_ERROR_INIT;

    if( !dbus_message_get_args(sig, &err,
                               DBUS_TYPE_STRING, &state_name,
                               DBUS_TYPE_INVALID) ) {
        log_warning(PFIX"can't parse tklock state signal: %s: %s",
                    err.name, err.message);
        goto cleanup;
    }

    tklockstate_t state = TKLOCK_LOCKED;

    if( eq(state_name, MCE_TK_UNLOCKED) )
        state = TKLOCK_UNLOCKED;

    displaykeepalive_tklock_set(self, state);

cleanup:

    dbus_error_free(&err);

    return;
}

static void
displaykeepalive_dbus_display_signal_cb(displaykeepalive_t *self, DBusMessage *sig)
{
    log_enter_function();

    const char *state_name = 0;

    DBusError err = DBUS_ERROR_INIT;

    if( !dbus_message_get_args(sig, &err,
                               DBUS_TYPE_STRING, &state_name,
                               DBUS_TYPE_INVALID) ) {
        log_warning(PFIX"can't parse display state signal: %s: %s",
                    err.name, err.message);
        goto cleanup;
    }

    displaystate_t state = DISPLAYSTATE_UNKNOWN;

    if( eq(state_name, MCE_DISPLAY_OFF_STRING) )
        state = DISPLAYSTATE_OFF;
    else if( eq(state_name, MCE_DISPLAY_DIM_STRING) )
        state = DISPLAYSTATE_DIM;
    else if( eq(state_name, MCE_DISPLAY_ON_STRING) )
        state = DISPLAYSTATE_ON;

    displaykeepalive_display_set(self, state);

cleanup:

    dbus_error_free(&err);

    return;
}

static void
displaykeepalive_dbus_nameowner_signal_cb(displaykeepalive_t *self, DBusMessage *sig)
{
    log_enter_function();

    const char *name = 0;
    const char *prev = 0;
    const char *curr = 0;

    DBusError err = DBUS_ERROR_INIT;

    if( !dbus_message_get_args(sig, &err,
                               DBUS_TYPE_STRING, &name,
                               DBUS_TYPE_STRING, &prev,
                               DBUS_TYPE_STRING, &curr,
                               DBUS_TYPE_INVALID) ) {
        log_warning(PFIX"can't parse name owner changed signal: %s: %s",
                    err.name, err.message);
        goto cleanup;
    }

    if( eq(name, MCE_SERVICE) ) {
        displaykeepalive_mce_owner_set(self,
                                         *curr ? NAMEOWNER_RUNNING : NAMEOWNER_STOPPED);
    }

cleanup:

    dbus_error_free(&err);

    return;
}

/* ------------------------------------------------------------------------- *
 * DBUS_MESSAGE_FILTERS
 * ------------------------------------------------------------------------- */

/** D-Bus rule for listening to mce name ownership changes */
static const char rule_nameowner_mce[] = ""
"type='signal'"
",sender='"DBUS_SERVICE_DBUS"'"
",path='"DBUS_PATH_DBUS"'"
",interface='"DBUS_INTERFACE_DBUS"'"
",member='"DBUS_NAMEOWENERCHANGED_SIG"'"
",arg0='"MCE_SERVICE"'"
;

/** D-Bus rule for listening to display state changes */
static const char rule_display_state[] = ""
"type='signal'"
",sender='"MCE_SERVICE"'"
",path='"MCE_SIGNAL_PATH"'"
",interface='"MCE_SIGNAL_IF"'"
",member='"MCE_DISPLAY_SIG"'"
;

/** D-Bus rule for listening to tklock state changes */
static const char rule_tklock_state[] = ""
"type='signal'"
",sender='"MCE_SERVICE"'"
",path='"MCE_SIGNAL_PATH"'"
",interface='"MCE_SIGNAL_IF"'"
",member='"MCE_TKLOCK_MODE_SIG"'"
;

/** D-Bus message filter callback for handling signals
 */
static DBusHandlerResult
displaykeepalive_dbus_filter_cb(DBusConnection *con,
                                DBusMessage *msg,
                                void *aptr)
{
    (void)con;

    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    displaykeepalive_t *self = aptr;

    if( !msg )
        goto cleanup;

    if( dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL )
        goto cleanup;

    const char *interface = dbus_message_get_interface(msg);
    if( !interface )
        goto cleanup;

    const char *member = dbus_message_get_member(msg);
    if( !member )
        goto cleanup;

    if( !strcmp(interface, MCE_SIGNAL_IF) ) {
        if( !strcmp(member, MCE_DISPLAY_SIG) )
            displaykeepalive_dbus_display_signal_cb(self, msg);
        else if( !strcmp(member, MCE_TKLOCK_MODE_SIG) )
            displaykeepalive_dbus_tklock_signal_cb(self, msg);
    }
    else if( !strcmp(interface, DBUS_INTERFACE_DBUS) ) {
        if( !strcmp(member, DBUS_NAMEOWENERCHANGED_SIG) )
            displaykeepalive_dbus_nameowner_signal_cb(self, msg);
    }

cleanup:
    return result;
}

/** Start listening to D-Bus signals
 */
static void
displaykeepalive_dbus_filter_install(displaykeepalive_t *self)
{
    if( self->dka_filter_added )
        goto cleanup;

    log_enter_function();

    self->dka_filter_added =
        dbus_connection_add_filter(self->dka_systembus,
                                   displaykeepalive_dbus_filter_cb,
                                   self, 0);

    if( !self->dka_filter_added )
        goto cleanup;

    if( xdbus_connection_is_valid(self->dka_systembus) ){
        dbus_bus_add_match(self->dka_systembus, rule_nameowner_mce, 0);
        dbus_bus_add_match(self->dka_systembus, rule_tklock_state, 0);
        dbus_bus_add_match(self->dka_systembus, rule_display_state, 0);
    }

cleanup:
    return;
}

/** Stop listening to D-Bus signals
 */
static void
displaykeepalive_dbus_filter_remove(displaykeepalive_t *self)
{
    if( !self->dka_filter_added )
        goto cleanup;

    log_enter_function();

    self->dka_filter_added = false;

    dbus_connection_remove_filter(self->dka_systembus,
                                  displaykeepalive_dbus_filter_cb,
                                  self);

    if( xdbus_connection_is_valid(self->dka_systembus) ){
        dbus_bus_remove_match(self->dka_systembus, rule_nameowner_mce, 0);
        dbus_bus_remove_match(self->dka_systembus, rule_tklock_state, 0);
        dbus_bus_remove_match(self->dka_systembus, rule_display_state, 0);
    }

cleanup:
    return;
}

/* ========================================================================= *
 * DBUS_CONNECTION
 * ========================================================================= */

/** Connect to D-Bus System Bus
 */
static void
displaykeepalive_dbus_connect(displaykeepalive_t *self)
{
    DBusError err = DBUS_ERROR_INIT;

    /* Attempt system bus connect only once */
    if( self->dka_connect_attempted )
        goto cleanup;

    log_enter_function();

    self->dka_connect_attempted = true;

    self->dka_systembus = dbus_bus_get(DBUS_BUS_SYSTEM, &err);

    if( !self->dka_systembus  ) {
        log_warning(PFIX"can't connect to system bus: %s: %s",
                    err.name, err.message);
        goto cleanup;
    }

    /* Assumption: The application itself is handling attaching
     *             the shared systembus connection to mainloop,
     *             either via dbus_connection_setup_with_g_main()
     *             or something equivalent. */

    /* Install signal filters */
    displaykeepalive_dbus_filter_install(self);

    /* Initiate async mce availability query */
    displaykeepalive_mce_owner_query_start(self);

cleanup:

    dbus_error_free(&err);

    return;
}

/** Disconnect from D-Bus System Bus
 */
static void
displaykeepalive_dbus_disconnect(displaykeepalive_t *self)
{
    /* If connection was not made, no need to undo stuff */
    if( !self->dka_systembus )
        goto cleanup;

    log_enter_function();

    /* Cancel any pending async method calls */
    displaykeepalive_mce_owner_query_cancel(self);
    displaykeepalive_display_query_cancel(self);
    displaykeepalive_tklock_query_cancel(self);

    /* Remove signal filters */
    displaykeepalive_dbus_filter_remove(self);

    /* Detach from system bus */
    dbus_connection_unref(self->dka_systembus),
        self->dka_systembus = 0;

    /* Note: As we do not clear dka_connect_attempted flag,
     *       re-connecting this object is not possible */

cleanup:

    return;
}

/* ========================================================================= *
 * EXTERNAL API --  documented in: keepalive-displaykeepalive.h
 * ========================================================================= */

displaykeepalive_t *
displaykeepalive_new(void)
{
    log_enter_function();

    displaykeepalive_t *self = calloc(1, sizeof *self);

    if( self )
        displaykeepalive_ctor(self);

    return self;
}

displaykeepalive_t *
displaykeepalive_ref(displaykeepalive_t *self)
{
    log_enter_function();

    displaykeepalive_t *ref = 0;

    if( !displaykeepalive_is_valid(self) )
        goto cleanup;

    ++self->dka_refcount;

    ref = self;

cleanup:
    return ref;
}

void
displaykeepalive_unref(displaykeepalive_t *self)
{
    log_enter_function();

    if( !displaykeepalive_is_valid(self) )
        goto cleanup;

    if( --self->dka_refcount != 0 )
        goto cleanup;

    displaykeepalive_dtor(self);
    free(self);

cleanup:
    return;
}

void
displaykeepalive_start(displaykeepalive_t *self)
{
    if( !displaykeepalive_is_valid(self) )
        goto cleanup;

    if( self->dka_requested )
        goto cleanup;

    /* Set we-want-to-prevent-blanking flag */
    self->dka_requested = true;

    /* Connect to systembus */
    displaykeepalive_dbus_connect(self);

    /* Check if keepalive session can be started */
    displaykeepalive_rethink_schedule(self);

cleanup:
    return;
}

void
displaykeepalive_stop(displaykeepalive_t *self)
{
    if( !displaykeepalive_is_valid(self) )
        goto cleanup;

    if( !self->dka_requested )
        goto cleanup;

    /* Clear we-want-to-prevent-blanking flag */
    self->dka_requested = false;

    /* Check if keepalive session needs to be stopped */
    displaykeepalive_rethink_schedule(self);

cleanup:
    return;
}
