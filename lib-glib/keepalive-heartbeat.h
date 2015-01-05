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

#ifndef KEEPALIVE_GLIB_HEARTBEAT_H_
# define KEEPALIVE_GLIB_HEARTBEAT_H_

# include <stdbool.h>

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

# pragma GCC visibility push(default)

/** Opaque heartbeat wakeup object
 *
 * Allocate via heartbeat_new() and
 * release via heartbeat_unref().
 */
typedef struct heartbeat_t heartbeat_t;

/** Hearbeat wakeup function type
 *
 * @param aptr user_data set via heartbeat_set_notify()
 */
typedef void (*heartbeat_wakeup_fn)(void *aptr);

/** User data free function type
 *
 * Called when heartbeat object is deleted after the
 * final reference is dropped via heartbeat_unref(), or
 * when heartbeat_set_notify() is called while user_data
 * is already set.
 *
 * @param user_data set via heartbeat_set_notify()
 */
typedef void (*heartbeat_free_fn)(void *aptr);

/** Create heartbeat wakeup object
 *
 * Initially has reference count of 1.
 *
 * Use heartbeat_ref() to increment reference count and
 * heartbeat_unref() to decrement reference count.
 *
 * Will be automatically released after reference count drops to zero.
 *
 * @return pointer to heartbeat wakeup object, or NULL
 */
heartbeat_t *heartbeat_new(void);

/** Increment reference count of heartbeat wakeup object
 *
 * Passing NULL object is explicitly allowed and does nothing.
 *
 * @param self heartbeat wakeup object pointer
 *
 * @return pointer to heartbeat wakeup object, or NULL in case of errors
 */
heartbeat_t *heartbeat_ref(heartbeat_t *self);

/** Decrement reference count of heartbeat wakeup object
 *
 * Passing NULL object is explicitly allowed and does nothing.
 *
 * The object will be released if reference count reaches zero.
 *
 * @param self heartbeat wakeup object pointer
 */
void heartbeat_unref(heartbeat_t *self);

/** Set wakeup delay range
 *
 * If delay_lo == delay_hi, the value specifies global wakeup slot
 * rather seconds from current time.
 *
 * If delay_hi < delay_lo, then the upper bound is adjusted so that
 * it is delay_lo + heartbeat interval (=12 seconds, but may vary
 * from one device to another).
 *
 * @param self      heartbeat wakeup object pointer
 * @param delay_lo  minimum seconds to wakeup
 * @param delay_hi  maximum seconds to wakeup
 */
void heartbeat_set_delay(heartbeat_t *self,
                         int delay_lo,
                         int delay_hi);

/** Set notification callback function to use on hearbeat wakeup
 *
 * If non-null user_free_cb is given, it is assumed that heartbeat
 * wakeup object owns user_data and it will be released when
 * heartbeat object is deleted or when heartbeat_set_notify()
 * is called again.
 *
 * @param self          heartbeat wakeup object pointer
 * @param notify_cb     callback function pointer, or NULL
 * @param user_data     data to pass to callback function
 * @param user_free_cb  callback function for releasing user_data
 */
void heartbeat_set_notify(heartbeat_t *self,
                          heartbeat_wakeup_fn notify_cb,
                          void *user_data,
                          heartbeat_free_fn user_free_cb);

/** Start waiting for heartbeat wakeup
 *
 * @param self          heartbeat wakeup object pointer
 */
void         heartbeat_start(heartbeat_t *self);

/** Cancel already scheduled heartbeat wakeup
 *
 * @param self          heartbeat wakeup object pointer
 */
void         heartbeat_stop(heartbeat_t *self);

# pragma GCC visibility pop

# ifdef __cplusplus
};
# endif

#endif /* KEEPALIVE_GLIB_HEARTBEAT_H_ */
