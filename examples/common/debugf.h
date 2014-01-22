/* ------------------------------------------------------------------------- *
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Simo Piiroinen <simo.piiroinen@jollamobile.com>
 * License: BSD
 * ------------------------------------------------------------------------- */

#ifndef DEBUGF_H_
#define DEBUGF_H_

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

void keepalive_debugf(const char *path,
                      int line,
                      const char *func,
                      const char *fmt,
                      ...) __attribute__ ((format (printf, 4, 5)));;

#ifdef __cplusplus
};
#endif

#define debugf(FMT, ARGS...) keepalive_debugf(__FILE__, __LINE__, __PRETTY_FUNCTION__, FMT, ## ARGS)
#define HERE debugf("...\n");

#endif /* DEBUGF_H_ */
