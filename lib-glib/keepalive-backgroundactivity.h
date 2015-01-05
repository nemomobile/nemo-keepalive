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

#ifndef KEEPALIVE_GLIB_BACKGROUNDACTIVITY_H_
# define KEEPALIVE_GLIB_BACKGROUNDACTIVITY_H_

# include <stdbool.h>

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

# pragma GCC visibility push(default)

/** Opaque background activity object
 *
 * Allocate via background_activity_new() and
 * release via background_activity_unref().
 */
typedef struct background_activity_t background_activity_t;

/** Background activity notification function type
 *
 * @param activity   background activity object pointer
 * @param user_data  data pointer set via background_activity_set_user_data()
 */
typedef void (*background_activity_event_fn)(background_activity_t *activity, void *user_data);

/** User data free function type
 *
 * Called when background activity object is deleted after the
 * final reference is dropped via background_activity_unref(), or
 * when background_activity_set_user_data() is called while user_data
 * is already set.
 *
 * @param user_data  as set via background_activity_set_notify()
 */
typedef void (*background_activity_free_fn)(void *);

/** Enumeration of global wakeup slots for background activity objects */
typedef enum
{                                                                    // ORIGIN:
    BACKGROUND_ACTIVITY_FREQUENCY_RANGE                =            0, // Nemomobile
    BACKGROUND_ACTIVITY_FREQUENCY_THIRTY_SECONDS       =           30, // Meego
    BACKGROUND_ACTIVITY_FREQUENCY_TWO_AND_HALF_MINUTES =  30 + 2 * 60, // Meego
    BACKGROUND_ACTIVITY_FREQUENCY_FIVE_MINUTES         =       5 * 60, // Meego
    BACKGROUND_ACTIVITY_FREQUENCY_TEN_MINUTES          =      10 * 60, // Meego
    BACKGROUND_ACTIVITY_FREQUENCY_FIFTEEN_MINUTES      =      15 * 60, // Android
    BACKGROUND_ACTIVITY_FREQUENCY_THIRTY_MINUTES       =      30 * 60, // Meego & Android
    BACKGROUND_ACTIVITY_FREQUENCY_ONE_HOUR             =  1 * 60 * 60, // Meego & Android
    BACKGROUND_ACTIVITY_FREQUENCY_TWO_HOURS            =  2 * 60 * 60, // Meego
    BACKGROUND_ACTIVITY_FREQUENCY_FOUR_HOURS           =  4 * 60 * 60, // Nemomobile
    BACKGROUND_ACTIVITY_FREQUENCY_EIGHT_HOURS          =  8 * 60 * 60, // Nemomobile
    BACKGROUND_ACTIVITY_FREQUENCY_TEN_HOURS            = 10 * 60 * 60, // Meego
    BACKGROUND_ACTIVITY_FREQUENCY_TWELVE_HOURS         = 12 * 60 * 60, // Android
    BACKGROUND_ACTIVITY_FREQUENCY_TWENTY_FOUR_HOURS    = 24 * 60 * 60, // Android

    BACKGROUND_ACTIVITY_FREQUENCY_MAXIMUM_FREQUENCY    =   0x7fffffff, // due to 32-bit libiphb ranges

} background_activity_frequency_t;

/** Create background activity object
 *
 * Initially has reference count of 1.
 *
 * Use background_activity_ref() to increment reference count and
 * background_activity_unref() to decrement reference count.
 *
 * Will be automatically released after reference count drops to zero.
 *
 * @return pointer to background activity object, or NULL
 */
background_activity_t *background_activity_new(void);

/** Increment reference count of background activity object
 *
 * Passing NULL object is explicitly allowed and does nothing.
 *
 * @param self  background activity object pointer
 *
 * @return pointer to background activity object, or NULL in case of errors
 */
background_activity_t *background_activity_ref(background_activity_t *self);

/** Decrement reference count of background activity object
 *
 * Passing NULL object is explicitly allowed and does nothing.
 *
 * The object will be released if reference count reaches zero.
 *
 * @param self  background activity object pointer
 */
void background_activity_unref(background_activity_t *self);

/** Set global wakeup slot for background activity object
 *
 * Global wakeup slot is defined as the next time in the future when
 * some monotonic clock source has full seconds that is multiple of
 * the given amount of seconds.
 *
 * So every process that is using 10 minute slot is woken up
 * simultaneously. And processes that are using 5 minute slots
 * wake up simultaneously with the 10 minute wakeups and once
 * in between the 10 minute slots.
 *
 * Note that the 1st wakeup can occur anything from zero to
 * specified amount of time.
 *
 *
 * SLOT |0.........1.........2.........3.........4.........5
 *      |012345678901234567890123456789012345678901234567890 -> time
 *      |
 *   2  |  x x x x x x x x x x x x x x x x x x x x x x x x x
 *   5  |     x    x    x    x    x    x    x    x    x    x
 *  10  |          x         x         x         x         x
 *
 * @param self  background activity object pointer
 * @param slot  seconds
 */
void background_activity_set_wakeup_slot(background_activity_t *self,
                                         background_activity_frequency_t slot);

/** Get global wakeup slot used by background activity object
 *
 * @param self  background activity object pointer
 *
 * @return seconds, or BACKGROUND_ACTIVITY_FREQUENCY_RANGE if
 *  ranged wakeup rather than global wakeup slot is used.
 */
background_activity_frequency_t background_activity_get_wakeup_slot(const background_activity_t *self);

/** Set wakeup range for background activity object
 *
 * The iphb daemon (or dsme iphb plugin nowadays) tries to minimize
 * the amount of wakeups by maximizing the amount of processes that
 * are woken up simultaneously.
 *
 * For this purpose the wakeup range should be as wide as logically
 * makes sense for the requesting client, but note that server side
 * logic does not necessarily take the values as is. If range_lo is
 * really small compared to range_hi, it will be scaled up to avoid
 * seemingly premature wake ups.
 *
 * As a rule of the thumb range_lo should be >= range_hi * 75% and
 * range_hi - range_lo >= heartbeat delay (= hw watchdog kick period,
 * for historical reasons 12 seconds).
 *
 * If negative range_hi is used, wakeup range is adjusted to have
 * the same length as heartbeat delay.
 *
 * @param self      background activity object pointer
 * @param range_lo  minimum sleep time in seconds
 * @param range_hi  maximum sleep time in seconds, or -1
 */
void background_activity_set_wakeup_range(background_activity_t *self, int range_lo, int range_hi);

/** Get wakeup range used by background activity object
 *
 * If global wakeup slot is used, the same value is returned for
 * both range_lo and range_hi.
 *
 * @param self      background activity object pointer
 * @param range_lo  [output] minimum sleep time in seconds
 * @param range_hi  [output] maximum sleep time in seconds
 */
void background_activity_get_wakeup_range(const background_activity_t *self,
                                          int *range_lo, int *range_hi);

/** Check if background activity object is in stopped state
 *
 * Stopped state means the object is not waiting for iphb
 * wakeup to occur and is not blocking device from suspending.
 *
 * @param self  background activity object pointer
 *
 * @return true if object is in stopped state, false otherwise
 */
bool background_activity_is_stopped(const background_activity_t *self);

/** Check if background activity object is in waiting state
 *
 * Waiting state means the object is waiting for iphb wakeup
 * to occur.
 *
 * @param self  background activity object pointer
 *
 * @return true if object is in waiting state, false otherwise
 */
bool background_activity_is_waiting(const background_activity_t *self);

/** Check if background activity object is in running state
 *
 * Waiting state means the object is blocking the device from
 * entering suspend.
 *
 * @param self  background activity object pointer
 *
 * @return true if object is in running state, false otherwise
 */
bool background_activity_is_running(const background_activity_t *self);

/** Set background activity object to waiting state
 *
 * IPHB wakeup slot or range is programmed. Automatic transition
 * to running state is made when wakeup occurs.
 *
 * @param self  background activity object pointer
 */
void background_activity_wait(background_activity_t *self);

/** Set background activity object to running state
 *
 * CPU-keepalive session is started and device is blocked from
 * suspending. Application code MUST terminate running state
 * as soon as possible by calling:
 *
 * background_activity_wait() -> schedule next wakeup
 * background_activity_stop() -> end keepalive session
 *
 * @param self  background activity object pointer
 */
void background_activity_run(background_activity_t *self);

/** Set background activity object to running state
 *
 * Active CPU-keepalive session is stopped / iphb wakeup canceled.
 *
 * @param self  background activity object pointer
 */
void background_activity_stop(background_activity_t *self);

/** Get client id used for cpu keepalive dbus ipc
 *
 * Every cpu-keepalive session needs to have unique within process
 * id string. This function returns the id string associated with
 * the background activity object.
 *
 * @param self  background activity object pointer
 *
 * @return id string
 */
const char *background_activity_get_id(const background_activity_t *self);

/** Set user_data argument to be passed to notification callbacks
 *
 * If non-null free_cb is specified, the user_data given is released
 * by it when a) the background activity object is deleted b) user
 * data is updated via another background_activity_set_user_data()
 * call.
 *
 * @param self       background activity object pointer
 * @param user_data  data pointer
 * @param free_cb    data free callback
 */

void background_activity_set_user_data(background_activity_t *self,
                                       void *user_data,
                                       background_activity_free_fn free_cb);

/** Get user_data argument be passed to notification callbacks
 *
 * @param self       background activity object pointer
 *
 * @return pointer set via background_activity_set_user_data()
 */
void *background_activity_get_user_data(const background_activity_t *self);

/** Clear and free user_data that would be passed to notification callbacks
 *
 * @param self       background activity object pointer
 */
void background_activity_free_user_data(background_activity_t *self);

/** Detach user_data from background activity object without freeing it
 *
 * Normally user data attached to background activity objects is
 * released when object is deleted or replaced by new user data.
 * If this is not desirable for some reason, the user data can be
 * detached from object via background_activity_steal_user_data().
 *
 * The caller must release the returned data as appropriate when
 * it is no longer needed.
 *
 * @param self       background activity object pointer
 *
 * @return pointer previously set via background_activity_set_user_data()
 */
void *background_activity_steal_user_data(background_activity_t *self);

/** Set notification function to be called on running state
 *
 * When background activity object makes transition to running
 * state the specified notification function is called.
 *
 * NOTE: In order not to block suspend forever, the execution
 * chain started by the callback function MUST end with call to
 * either background_activity_wait() or background_activity_stop().
 *
 * @param self  background activity object pointer
 * @param cb    callback function pointer, or NULL
 */
void background_activity_set_running_callback(background_activity_t *self,
                                              background_activity_event_fn cb);

/** Set notification function to be called on waiting state
 *
 * When background activity object makes transition to waiting
 * state the specified notification function is called.
 *
 * @param self  background activity object pointer
 * @param cb    callback function pointer, or NULL
 */
void background_activity_set_waiting_callback(background_activity_t *self,
                                              background_activity_event_fn cb);

/** Set notification function to be called on stopped state
 *
 * When background activity object makes transition to stopped
 * state the specified notification function is called.
 *
 * @param self  background activity object pointer
 * @param cb    callback function pointer, or NULL
 */
void background_activity_set_stopped_callback(background_activity_t *self,
                                              background_activity_event_fn cb);
# pragma GCC visibility pop

# ifdef __cplusplus
};
# endif

#endif // KEEPALIVE_GLIB_BACKGROUNDACTIVITY_H_
