# -*- mode: sh -*-

TEMPLATE     = app
TARGET       = backgroundactivity_linger

CONFIG      += qt debug link_pkgconfig
PKGCONFIG   +=

LIBS        += -L../../lib -lkeepalive

SOURCES     += backgroundactivity_linger.cpp

SOURCES     += ../common/debugf.c
HEADERS     += ../common/debugf.h

target.path = $$[QT_INSTALL_BINS]

INSTALLS += target
