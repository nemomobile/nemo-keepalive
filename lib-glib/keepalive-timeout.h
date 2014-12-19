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

#ifndef KEEPALIVE_GLIB_TIMEOUT_H_
# define KEEPALIVE_GLIB_TIMEOUT_H_

# include <glib.h>

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

# pragma GCC visibility push(default)

/** Drop in replacement for g_timeout_add_full()
 *
 * Unlike normal glib timers, these can wake the device from suspend and
 * keep the device from suspending while the callback function is executed
 * by using iphb wakeups from dsme and cpu keepalive from mce.
 *
 * Note that the wakeup time is not exact.
 *
 * The iphb wakeups use 1 second resolution, so the milliseconds used
 * due to following the glib interface are rounded up to the next full
 * second.
 *
 * If the device needs to wake up from suspend, that happens via RTC
 * alarm interrupts. In that case clock source differences can cause
 * up to one second jitter in wakeups.
 *
 * And the triggering is scheduled via ranged iphb wakeup. By default the
 * range that is used is [interval, interval + heartbeat seconds] - which
 * should guarantee that the device will not wake up solely to serve the
 * timer, but also means the wakeup can occur up to 12 seconds later than
 * requested.
 *
 * If more exact wakeup is absolutely required, the priority parameter
 * of G_PRIORITY_HIGH can be used and the wakeup is scheduled to occur
 * at range of [interval, interval + 1 second].
 *
 * @param priority  the priority of the timeout source. Typically this
 *                  will be in the range between G_PRIORITY_DEFAULT and
 *                  G_PRIORITY_HIGH.
 * @param interval  the time between calls to the function, in milliseconds
 * @param function  function to call
 * @param data      data to pass to function
 * @param notify    function to call when the timeout is removed, or NULL.
 *
 * @return the ID (greater than 0) of the event source
 */
guint keepalive_timeout_add_full(gint priority, guint interval, GSourceFunc function, gpointer data, GDestroyNotify notify);

/** Drop in replacement for g_timeout_add()
 *
 * See g_timeout_add_full() for details.
 *
 * @param interval  the time between calls to the function, in milliseconds
 * @param function  function to call
 * @param data      data to pass to function
 *
 * @return the ID (greater than 0) of the event source
 */
guint keepalive_timeout_add(guint interval, GSourceFunc function, gpointer data);

/** Drop in replacement for g_timeout_add_seconds_full()
 *
 * See g_timeout_add_full() for details.
 *
 * @param priority  the priority of the timeout source. Typically this
 *                  will be in the range between G_PRIORITY_DEFAULT and
 *                  G_PRIORITY_HIGH.
 * @param interval  the time between calls to the function, in seconds
 * @param function  function to call
 * @param data      data to pass to function
 * @param notify    function to call when the timeout is removed, or NULL.
 *
 * @return the ID (greater than 0) of the event source
 */
guint keepalive_timeout_add_seconds_full(gint priority, guint interval, GSourceFunc function, gpointer data, GDestroyNotify notify);

/** Drop in replacement for g_timeout_add_seconds()
 *
 * See g_timeout_add_full() for details.
 *
 * @param interval  the time between calls to the function, in seconds
 * @param function  function to call
 * @param data      data to pass to function
 *
 * @return the ID (greater than 0) of the event source
 */
guint keepalive_timeout_add_seconds(guint interval, GSourceFunc function, gpointer data);

# pragma GCC visibility pop

# ifdef __cplusplus
};
# endif

#endif /* KEEPALIVE_GLIB_TIMEOUT_H_ */
