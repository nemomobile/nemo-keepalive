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

#ifndef KEEPALIVE_GLIB_DISPLAYKEEPALIVE_H_
# define KEEPALIVE_GLIB_DISPLAYKEEPALIVE_H_

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

# pragma GCC visibility push(default)

/** Opaque display keepalive structure
 *
 * Allocate via displaykeepalive_new() and
 * release via displaykeepalive_unref().
 */
typedef struct displaykeepalive_t displaykeepalive_t;

/** Create display keepalive object
 *
 * Initially has reference count of 1.
 *
 * Use displaykeepalive_ref() to increment reference count and
 * displaykeepalive_unref() to decrement reference count.
 *
 * Will be automatically released after reference count drops to zero.
 *
 * @return pointer to display keepalive object, or NULL
 */
displaykeepalive_t *displaykeepalive_new(void);

/** Increment reference count of display keepalive object
 *
 * Passing NULL object is explicitly allowed and does nothing.
 *
 * @param self display keepalive object pointer
 *
 * @return pointer to display keepalive object, or NULL in case of errors
 */
displaykeepalive_t *displaykeepalive_ref(displaykeepalive_t *self);

/** Decrement reference count of display keepalive object
 *
 * Passing NULL object is explicitly allowed and does nothing.
 *
 * The object will be released if reference count reaches zero.
 *
 * @param self display keepalive object pointer
 */
void displaykeepalive_unref(displaykeepalive_t *self);

/** Disable display normal display blanking policy
 *
 * The display keepalive object makes the necessary dbus ipc that keeps
 * the display from blanking while/when the following conditions are met:
 * 1) display is already on
 * 2) lockscreen is not shown
 * 3) mce is running
 */
void displaykeepalive_start(displaykeepalive_t *self);

/** Allow display normal display blanking policy
 */
void displaykeepalive_stop(displaykeepalive_t *self);

# pragma GCC visibility pop

# ifdef __cplusplus
};
# endif

#endif // KEEPALIVE_GLIB_DISPLAYKEEPALIVE_H_
