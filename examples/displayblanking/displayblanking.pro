# -*- mode: sh -*-

TEMPLATE   = app
TARGET     = displayblanking

QT        += dbus
CONFIG    += qt debug link_pkgconfig
PKGCONFIG +=

LIBS        += -L../../lib -lkeepalive

SOURCES     += displayblanking.cpp

SOURCES     += ../common/debugf.c
HEADERS     += ../common/debugf.h

target.path  = $$[QT_INSTALL_BINS]

INSTALLS    += target
