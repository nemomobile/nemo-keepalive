# ----------------------------------------------------------- -*- mode: sh -*-
# List of targets to build
# ----------------------------------------------------------------------------

TARGETS += keepalive-tool

ROOT ?=
PREFIX ?= /usr

# ----------------------------------------------------------------------------
# Top level targets
# ----------------------------------------------------------------------------

.PHONY: build install clean distclean mostlyclean

build:: $(TARGETS)

install:: build
	install -d $(ROOT)$(PREFIX)/bin
	install -m755 $(TARGETS) $(ROOT)$(PREFIX)/bin/

clean:: mostlyclean
	$(RM) $(TARGETS)

distclean:: clean

mostlyclean::
	$(RM) *.o *~ *.bak

# ----------------------------------------------------------------------------
# Default flags
# ----------------------------------------------------------------------------

CPPFLAGS += -D_GNU_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64

CFLAGS   += -Wall
CFLAGS   += -Os
CFLAGS   += -std=c99
CFLAGS   += -g

LDFLAGS  += -g

LDLIBS   += -Wl,--as-needed

# ----------------------------------------------------------------------------
# Flags from pkg-config
# ----------------------------------------------------------------------------

PKG_NAMES  += glib-2.0
PKG_NAMES  += dbus-1
PKG_NAMES  += dbus-glib-1

# We need to link against the locally-built library
LDLIBS += -L../lib-glib -lkeepalive-glib

PKG_CFLAGS := $(shell pkg-config --cflags $(PKG_NAMES))
PKG_LDLIBS := $(shell pkg-config --libs   $(PKG_NAMES))

CFLAGS     += $(PKG_CFLAGS)
LDLIBS     += $(PKG_LDLIBS)
