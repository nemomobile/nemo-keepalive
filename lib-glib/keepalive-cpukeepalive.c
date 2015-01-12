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

#include "keepalive-cpukeepalive.h"

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

/** Assumed renew period used while dbus query has not been made yet */
#define CPU_KEEPALIVE_RENEW_MS (60 * 1000)

/* Logging prefix for this module */
#define PFIX "cpukeepalive: "

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

/** Memory tag for marking live cpukeepalive_t objects */
#define CPUKEEPALIVE_MAJICK_ALIVE 0x548ec404

/** Memory tag for marking dead cpukeepalive_t objects */
#define CPUKEEPALIVE_MAJICK_DEAD  0x00000000

/** CPU keepalive state object
 */
struct cpukeepalive_t
{
    /** Simple memory tag to catch usage of obviously bogus
     *  cpukeepalive_t pointers */
    unsigned         cka_majick;

    /** Reference count; initially 1, released when drops to 0 */
    unsigned         cka_refcount;

    /** Unique identifier string */
    char            *cka_id;

    /** Flag for: preventing device suspend requested */
    bool             cka_requested;

    /** Flag for: we've already tried to connect to system bus */
    bool             cka_connect_attempted;

    /** System bus connection */
    DBusConnection  *cka_systembus;

    /** Flag for: signal filters installed */
    bool             cka_filter_added;

    /** Current com.nokia.mce name ownership state */
    nameowner_t      cka_mce_service;

    /** Async dbus query for initial cka_mce_service value */
    DBusPendingCall *cka_mce_service_pc;

    /** Timer id for active cpu keepalive session */
    guint            cka_renew_timer_id;

    /** Renew delay for active cpu keepalive session */
    guint            cka_renew_period_ms;

    /** Async dbus query for initial cka_mce_service value */
    DBusPendingCall *cka_renew_period_pc;

    /** Idle callback id for starting/stopping keepalive session */
    guint            cka_rethink_id;

    // NOTE: cpukeepalive_ctor & cpukeepalive_dtor
};

/* ========================================================================= *
 * INTERNAL FUNCTIONS
 * ========================================================================= */

// CONSTRUCT_DESTRUCT
static void              cpukeepalive_ctor                          (cpukeepalive_t *self);
static void              cpukeepalive_dtor                          (cpukeepalive_t *self);
static bool              cpukeepalive_is_valid                      (const cpukeepalive_t *self);

// RENEW_PERIOD
static guint             cpukeepalive_renew_period_get              (const cpukeepalive_t *self);
static void              cpukeepalive_renew_period_set              (cpukeepalive_t *self, int delay_ms);
static void              cpukeepalive_renew_period_query_reply_cb   (DBusPendingCall *pc, void *aptr);
static void              cpukeepalive_renew_period_query_start      (cpukeepalive_t *self);
static void              cpukeepalive_renew_period_query_cancel     (cpukeepalive_t *self);

// KEEPALIVE_SESSION
static void              cpukeepalive_session_ipc                   (cpukeepalive_t *self, const char *method);
static gboolean          cpukeepalive_session_cb                    (gpointer aptr);
static void              cpukeepalive_session_start                 (cpukeepalive_t *self);
static void              cpukeepalive_session_stop                  (cpukeepalive_t *self);
static void              cpukeepalive_session_restart               (cpukeepalive_t *self);

// RETHINK_STATE
static void              cpukeepalive_rethink_now                   (cpukeepalive_t *self);

// MCE_SERVICE_TRACKING
static nameowner_t       cpukeepalive_mce_owner_get                 (const cpukeepalive_t *self);
static void              cpukeepalive_mce_owner_set                 (cpukeepalive_t *self, nameowner_t state);
static void              cpukeepalive_mce_owner_query_reply_cb      (DBusPendingCall *pc, void *aptr);
static void              cpukeepalive_mce_owner_query_start         (cpukeepalive_t *self);
static void              cpukeepalive_mce_owner_query_cancel        (cpukeepalive_t *self);

// DBUS_SIGNAL_HANDLING
static void              cpukeepalive_dbus_nameowner_signal_cb      (cpukeepalive_t *self, DBusMessage *sig);

// DBUS_MESSAGE_FILTERS
static DBusHandlerResult cpukeepalive_dbus_filter_cb                (DBusConnection *con, DBusMessage *msg, void *aptr);
static void              cpukeepalive_dbus_filter_install           (cpukeepalive_t *self);
static void              cpukeepalive_dbus_filter_remove            (cpukeepalive_t *self);

// DBUS_CONNECTION
static void              cpukeepalive_dbus_connect                  (cpukeepalive_t *self);
static void              cpukeepalive_dbus_disconnect               (cpukeepalive_t *self);

/* ------------------------------------------------------------------------- *
 * CONSTRUCT_DESTRUCT
 * ------------------------------------------------------------------------- */

static char *cpukeepalive_generate_id(void)
{
    static unsigned count = 0;
    char buf[64];
    snprintf(buf, sizeof buf, "glib_cpu_keepalive_%u", ++count);
    return strdup(buf);
}

/** Constructor for cpukeepalive_t objects
 */
static void
cpukeepalive_ctor(cpukeepalive_t *self)
{
    log_enter_function();

    /* Mark as valid */
    self->cka_majick = CPUKEEPALIVE_MAJICK_ALIVE;

    /* Initialize ref count to one */
    self->cka_refcount = 1;

    /* Assign unique (within process) id for use with mce dbus ipc */
    self->cka_id = cpukeepalive_generate_id();

    /* Session neither requested nor running */
    self->cka_requested = false;
    self->cka_renew_timer_id = 0;

    /* No system bus connection */
    self->cka_connect_attempted = false;
    self->cka_systembus = 0;
    self->cka_filter_added = false;

    /* MCE availability is not known */
    self->cka_mce_service = NAMEOWNER_UNKNOWN;
    self->cka_mce_service_pc = 0;

    /* Renew period is unknown */
    self->cka_renew_period_ms = 0;
    self->cka_renew_period_pc = 0;

    /* No pending session rethink scheduled */
    self->cka_rethink_id = 0;

    /* Connect to systembus */
    cpukeepalive_dbus_connect(self);
}

/** Destructor for cpukeepalive_t objects
 */
static void
cpukeepalive_dtor(cpukeepalive_t *self)
{
    log_enter_function();

    /* Forced stopping of keepalive session */
    cpukeepalive_stop(self);
    cpukeepalive_rethink_now(self);

    /* Disconnecting also cancels pending async method calls */
    cpukeepalive_dbus_disconnect(self);

    /* Free id string */
    free(self->cka_id),
        self->cka_id = 0;

    /* Mark as invalid */
    self->cka_majick = CPUKEEPALIVE_MAJICK_DEAD;
}

/** Predicate for: cpukeepalive_t object is valid
 */
static bool
cpukeepalive_is_valid(const cpukeepalive_t *self)
{
    return self && self->cka_majick == CPUKEEPALIVE_MAJICK_ALIVE;
}
/* ========================================================================= *
 * RENEW_PERIOD
 * ========================================================================= */

static guint
cpukeepalive_renew_period_get(const cpukeepalive_t *self)
{
    return self->cka_renew_period_ms ?: CPU_KEEPALIVE_RENEW_MS;
}

static void
cpukeepalive_renew_period_set(cpukeepalive_t *self, int delay_ms)
{
    cpukeepalive_renew_period_query_cancel(self);

    guint delay_old = cpukeepalive_renew_period_get(self);

    if( delay_ms <= 0 )
        self->cka_renew_period_ms = CPU_KEEPALIVE_RENEW_MS;
    else
        self->cka_renew_period_ms = delay_ms;

    guint delay_new = cpukeepalive_renew_period_get(self);

    log_notice(PFIX"renew period: %d", delay_new);

    if( delay_old != delay_new )
        cpukeepalive_session_restart(self);
}

static void
cpukeepalive_renew_period_query_reply_cb(DBusPendingCall *pc, void *aptr)
{
    cpukeepalive_t *self = aptr;
    DBusMessage    *rsp  = 0;
    DBusError       err  = DBUS_ERROR_INIT;
    dbus_int32_t    val  = 0;

    if( self->cka_renew_period_pc != pc )
        goto cleanup;

    log_enter_function();

    self->cka_renew_period_pc = 0;

    if( !(rsp = dbus_pending_call_steal_reply(pc)) )
        goto cleanup;

    if( dbus_set_error_from_message(&err, rsp) ||
        !dbus_message_get_args(rsp, &err,
                               DBUS_TYPE_INT32, &val,
                               DBUS_TYPE_INVALID) ) {
        log_warning(PFIX"renew period reply: %s: %s", err.name, err.message);
    }

cleanup:

    /* Default will be used and further queries blocked  if we could
     * not parse non-zero value from the reply message.*/
    cpukeepalive_renew_period_set(self, val * 1000);

    if( rsp )
        dbus_message_unref(rsp);

    dbus_error_free(&err);
}

static void
cpukeepalive_renew_period_query_start(cpukeepalive_t *self)
{
    if( self->cka_renew_period_ms )
            goto cleanup;

    if( self->cka_renew_period_pc )
        goto cleanup;

    log_enter_function();

    self->cka_renew_period_pc =
        xdbus_method_call(self->cka_systembus,
                          MCE_SERVICE,
                          MCE_REQUEST_PATH,
                          MCE_REQUEST_IF,
                          MCE_CPU_KEEPALIVE_PERIOD_REQ,
                          cpukeepalive_renew_period_query_reply_cb,
                          self, 0,
                          DBUS_TYPE_STRING, &self->cka_id,
                          DBUS_TYPE_INVALID);
cleanup:
    return;
}

static void
cpukeepalive_renew_period_query_cancel(cpukeepalive_t *self)
{
    if( self->cka_renew_period_pc ) {
        log_enter_function();

        dbus_pending_call_cancel(self->cka_renew_period_pc),
            self->cka_renew_period_pc = 0;
    }
}

/* ========================================================================= *
 * KEEPALIVE_SESSION
 * ========================================================================= */

/** Helper for making mce dbus method calls for which we want no reply
 */
static void
cpukeepalive_session_ipc(cpukeepalive_t *self, const char *method)
{
    if( xdbus_connection_is_valid(self->cka_systembus) ) {
        xdbus_simple_call(self->cka_systembus,
                          MCE_SERVICE,
                          MCE_REQUEST_PATH,
                          MCE_REQUEST_IF,
                          method,
                          DBUS_TYPE_STRING, &self->cka_id,
                          DBUS_TYPE_INVALID);

        /* Try to ensure that the method call does not get stuck
         * to output buffer... */
        dbus_connection_flush(self->cka_systembus);
    }
}

/** Timer callback for renewing cpu keepalive session
 */
static gboolean
cpukeepalive_session_cb(gpointer aptr)
{
    gboolean keep_going = FALSE;
    cpukeepalive_t *self = aptr;

    if( !self->cka_renew_timer_id  )
        goto cleanup;

    log_enter_function();

    cpukeepalive_session_ipc(self, MCE_CPU_KEEPALIVE_START_REQ);
    keep_going = TRUE;

cleanup:
    if( !keep_going && self->cka_renew_timer_id  )
        self->cka_renew_timer_id  = 0;

    return keep_going;
}

/** Start cpu keepalive session
 */
static void
cpukeepalive_session_start(cpukeepalive_t *self)
{
    if( self->cka_renew_timer_id )
        goto cleanup;

    log_enter_function();

    cpukeepalive_session_ipc(self, MCE_CPU_KEEPALIVE_START_REQ);

    self->cka_renew_timer_id =
        g_timeout_add(cpukeepalive_renew_period_get(self),
                      cpukeepalive_session_cb, self);

cleanup:
    return;
}

/** Restart cpu keepalive session after renew delay change
 */
static void
cpukeepalive_session_restart(cpukeepalive_t *self)
{
    /* skip if not already running */
    if( !self->cka_renew_timer_id )
        goto cleanup;

    log_enter_function();

    cpukeepalive_session_ipc(self, MCE_CPU_KEEPALIVE_START_REQ);

    g_source_remove(self->cka_renew_timer_id);

    self->cka_renew_timer_id =
        g_timeout_add(cpukeepalive_renew_period_get(self),
                      cpukeepalive_session_cb, self);

cleanup:
    return;
}

/** Stop cpu keepalive session
 */
static void
cpukeepalive_session_stop(cpukeepalive_t *self)
{
    if( !self->cka_renew_timer_id )
        goto cleanup;

    log_enter_function();

    g_source_remove(self->cka_renew_timer_id),
        self->cka_renew_timer_id = 0;

    cpukeepalive_session_ipc(self, MCE_CPU_KEEPALIVE_STOP_REQ);

cleanup:
    return;
}

/* ------------------------------------------------------------------------- *
 * RETHINK_STATE
 * ------------------------------------------------------------------------- */

static void
cpukeepalive_rethink_now(cpukeepalive_t *self)
{
    bool need_renew_loop = false;

    /* Preventing cpu suspending is possible when mce is running */

    // TODO: should we block only when it is known that mce is
    //       not up and running?
    if( cpukeepalive_mce_owner_get(self) != NAMEOWNER_RUNNING )
        goto cleanup;

    need_renew_loop = self->cka_requested;

cleanup:

    if( need_renew_loop )
        cpukeepalive_session_start(self);
    else
        cpukeepalive_session_stop(self);
}

/* ------------------------------------------------------------------------- *
 * MCE_SERVICE_TRACKING
 * ------------------------------------------------------------------------- */

static nameowner_t
cpukeepalive_mce_owner_get(const cpukeepalive_t *self)
{
    return self->cka_mce_service;
}

static void
cpukeepalive_mce_owner_set(cpukeepalive_t *self, nameowner_t state)
{
    cpukeepalive_mce_owner_query_cancel(self);

    if( self->cka_mce_service != state ) {
        log_notice(PFIX"MCE_SERVICE: %d -> %d",
                   self->cka_mce_service, state);
        self->cka_mce_service = state;

        if( self->cka_mce_service == NAMEOWNER_RUNNING )
            cpukeepalive_renew_period_query_start(self);

        cpukeepalive_rethink_now(self);
    }
}

static void
cpukeepalive_mce_owner_query_reply_cb(DBusPendingCall *pc, void *aptr)
{
    cpukeepalive_t *self = aptr;
    DBusMessage        *rsp  = 0;
    DBusError           err  = DBUS_ERROR_INIT;

    if( self->cka_mce_service_pc != pc )
        goto cleanup;

    log_enter_function();

    self->cka_mce_service_pc = 0;

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

    cpukeepalive_mce_owner_set(self,
                               (owner && *owner) ?
                               NAMEOWNER_RUNNING : NAMEOWNER_STOPPED);

cleanup:

    if( rsp )
        dbus_message_unref(rsp);

    dbus_error_free(&err);
}

static void
cpukeepalive_mce_owner_query_start(cpukeepalive_t *self)
{
    if( self->cka_mce_service_pc )
        goto cleanup;

    log_enter_function();

    const char *arg = MCE_SERVICE;

    self->cka_mce_service_pc =
        xdbus_method_call(self->cka_systembus,
                          DBUS_SERVICE_DBUS,
                          DBUS_PATH_DBUS,
                          DBUS_INTERFACE_DBUS,
                          "GetNameOwner",
                          cpukeepalive_mce_owner_query_reply_cb,
                          self, 0,
                          DBUS_TYPE_STRING, &arg,
                          DBUS_TYPE_INVALID);
cleanup:
    return;
}

static void
cpukeepalive_mce_owner_query_cancel(cpukeepalive_t *self)
{
    if( self->cka_mce_service_pc ) {
        log_enter_function();

        dbus_pending_call_cancel(self->cka_mce_service_pc),
            self->cka_mce_service_pc = 0;
    }
}

/* ------------------------------------------------------------------------- *
 * DBUS_SIGNAL_HANDLING
 * ------------------------------------------------------------------------- */

#define DBUS_NAMEOWENERCHANGED_SIG "NameOwnerChanged"

static void
cpukeepalive_dbus_nameowner_signal_cb(cpukeepalive_t *self, DBusMessage *sig)
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
        cpukeepalive_mce_owner_set(self,
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

/** D-Bus message filter callback for handling signals
 */
static DBusHandlerResult
cpukeepalive_dbus_filter_cb(DBusConnection *con,
                                DBusMessage *msg,
                                void *aptr)
{
    (void)con;

    log_enter_function();

    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    cpukeepalive_t *self = aptr;

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

    if( !strcmp(interface, DBUS_INTERFACE_DBUS) ) {
        if( !strcmp(member, DBUS_NAMEOWENERCHANGED_SIG) )
            cpukeepalive_dbus_nameowner_signal_cb(self, msg);
    }

cleanup:
    return result;
}

/** Start listening to D-Bus signals
 */
static void
cpukeepalive_dbus_filter_install(cpukeepalive_t *self)
{
    if( self->cka_filter_added )
        goto cleanup;

    log_enter_function();

    self->cka_filter_added =
        dbus_connection_add_filter(self->cka_systembus,
                                   cpukeepalive_dbus_filter_cb,
                                   self, 0);

    if( !self->cka_filter_added )
        goto cleanup;

    if( xdbus_connection_is_valid(self->cka_systembus) ){
        dbus_bus_add_match(self->cka_systembus, rule_nameowner_mce, 0);
    }

cleanup:
    return;
}

/** Stop listening to D-Bus signals
 */
static void
cpukeepalive_dbus_filter_remove(cpukeepalive_t *self)
{
    if( !self->cka_filter_added )
        goto cleanup;

    log_enter_function();

    self->cka_filter_added = false;

    dbus_connection_remove_filter(self->cka_systembus,
                                  cpukeepalive_dbus_filter_cb,
                                  self);

    if( xdbus_connection_is_valid(self->cka_systembus) ){
        dbus_bus_remove_match(self->cka_systembus, rule_nameowner_mce, 0);
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
cpukeepalive_dbus_connect(cpukeepalive_t *self)
{
    DBusError err = DBUS_ERROR_INIT;

    /* Attempt system bus connect only once */
    if( self->cka_connect_attempted )
        goto cleanup;

    self->cka_connect_attempted = true;

    log_enter_function();

    self->cka_systembus = dbus_bus_get(DBUS_BUS_SYSTEM, &err);

    if( !self->cka_systembus  ) {
        log_warning(PFIX"can't connect to system bus: %s: %s",
                    err.name, err.message);
        goto cleanup;
    }

    /* Assumption: The application itself is handling attaching
     *             the shared systembus connection to mainloop,
     *             either via dbus_connection_setup_with_g_main()
     *             or something equivalent. */

    /* Install signal filters */
    cpukeepalive_dbus_filter_install(self);

    /* Initiate async mce availability query */
    cpukeepalive_mce_owner_query_start(self);

cleanup:

    dbus_error_free(&err);

    return;
}

/** Disconnect from D-Bus System Bus
 */
static void
cpukeepalive_dbus_disconnect(cpukeepalive_t *self)
{

    /* If connection was not made, no need to undo stuff */
    if( !self->cka_systembus )
        goto cleanup;

    log_enter_function();

    /* Cancel any pending async method calls */
    cpukeepalive_mce_owner_query_cancel(self);
    cpukeepalive_renew_period_query_cancel(self);

    /* Remove signal filters */
    cpukeepalive_dbus_filter_remove(self);

    /* Detach from system bus */
    dbus_connection_unref(self->cka_systembus),
        self->cka_systembus = 0;

    /* Note: As we do not clear cka_connect_attempted flag,
     *       re-connecting this object is not possible */

cleanup:

    return;
}

/* ========================================================================= *
 * EXTERNAL API --  documented in: keepalive-cpukeepalive.h
 * ========================================================================= */

cpukeepalive_t *
cpukeepalive_new(void)
{
    log_enter_function();

    cpukeepalive_t *self = calloc(1, sizeof *self);

    if( self )
        cpukeepalive_ctor(self);

    return self;
}

cpukeepalive_t *
cpukeepalive_ref(cpukeepalive_t *self)
{
    log_enter_function();

    cpukeepalive_t *ref = 0;

    if( !cpukeepalive_is_valid(self) )
        goto cleanup;

    ++self->cka_refcount;

    ref = self;

cleanup:
    return ref;
}

void
cpukeepalive_unref(cpukeepalive_t *self)
{
    log_enter_function();

    if( !cpukeepalive_is_valid(self) )
        goto cleanup;

    if( --self->cka_refcount != 0 )
        goto cleanup;

    cpukeepalive_dtor(self);
    free(self);

cleanup:
    return;
}

void
cpukeepalive_start(cpukeepalive_t *self)
{
    if( !cpukeepalive_is_valid(self) )
        goto cleanup;

    if( self->cka_requested )
        goto cleanup;

    /* Set we-want-to-prevent-blanking flag */
    self->cka_requested = true;

    /* Connect to systembus */
    cpukeepalive_dbus_connect(self);

    /* Check if keepalive session can be started */
    cpukeepalive_rethink_now(self);

cleanup:
    return;
}

void
cpukeepalive_stop(cpukeepalive_t *self)
{
    if( !cpukeepalive_is_valid(self) )
        goto cleanup;

    if( !self->cka_requested )
        goto cleanup;

    /* Clear we-want-to-prevent-blanking flag */
    self->cka_requested = false;

    /* Check if keepalive session needs to be stopped */
    cpukeepalive_rethink_now(self);

cleanup:
    return;
}

const char *cpukeepalive_get_id(const cpukeepalive_t *self)
{
    return cpukeepalive_is_valid(self) ? self->cka_id : 0;
}
