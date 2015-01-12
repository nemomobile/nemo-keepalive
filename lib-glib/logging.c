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

#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

static int log_verbosity = LOGGING_DEFAULT_LEVEL;

static const char *
log_prefix(int lev)
{
    const char *res = "?";

    switch( lev ) {
    case LOG_EMERG:     res = "X"; break;
    case LOG_ALERT:     res = "A"; break;
    case LOG_CRIT:      res = "C"; break;
    case LOG_ERR:       res = "E"; break;
    case LOG_WARNING:   res = "W"; break;
    case LOG_NOTICE:    res = "N"; break;
    case LOG_INFO:      res = "I"; break;
    case LOG_DEBUG:     res = "D"; break;
    }

    return res;
}

void
log_set_verbosity(int lev)
{
    if( lev < LOG_ERR )
        lev = LOG_ERR;

    if( lev > LOG_DEBUG )
        lev = LOG_DEBUG;

    log_verbosity = lev;
}

int
log_get_verbosity(void)
{
    return log_verbosity;
}

bool
log_p(int lev)
{
    /* NOTE: Code must not change errno */
    return lev <= log_verbosity;
}

void
log_emit_(int lev, const char *fmt, ...)
{
    /* Mark down errno on entry */
    int   saved = errno;

    char *text = 0;

    /* Check verbosity also here in case log_xxx() macros
     * were not used by the calling code */
    if( !log_p(lev) )
        goto cleanup;

    va_list va;

    va_start(va, fmt);
    int rc = vasprintf(&text, fmt, va);
    va_end(va);

    if( rc < 0 ) {
        text = 0;
        goto cleanup;
    }

    fprintf(stderr, "keepalive: %s: %s\n", log_prefix(lev), text);

cleanup:

    free(text);

    /* Restore errno before returning */
    errno = saved;
}

static void log_init(void) __attribute__((constructor));

static void log_init(void)
{
    const char *env = getenv("LIBKEEPALIVE_VERBOSITY");
    if( env != 0 )
        log_set_verbosity(strtol(env, 0, 0));
}
