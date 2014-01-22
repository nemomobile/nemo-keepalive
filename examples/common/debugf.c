/* ------------------------------------------------------------------------- *
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Simo Piiroinen <simo.piiroinen@jollamobile.com>
 * License: LGPLv2.1
 * ------------------------------------------------------------------------- */

#include "debugf.h"

#include <sys/time.h>

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static void init(void) __attribute__((constructor));

static struct timeval t0;

static void init(void)
{
  gettimeofday(&t0, 0);
}

void keepalive_debugf(const char *path, int line, const char *func, const char *fmt, ...)
{
  (void)path; (void)line;
  struct timeval tv;
  gettimeofday(&tv, 0);
  if( !timerisset(&t0) ) t0 = tv;
  timersub(&tv, &t0, &tv);
  //fprintf(stdout, "%s:%d: ", path, line);
  fprintf(stdout, "[%2ld.%03ld] ", (long)tv.tv_sec, (long)(tv.tv_usec/1000));
  fprintf(stdout, "%s: ", func);
  va_list va;
  va_start(va, fmt);
  vfprintf(stdout, fmt, va);
  va_end(va);
}
