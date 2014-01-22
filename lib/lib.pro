# -*- mode: sh -*-

TEMPLATE   = lib
TARGET     = keepalive
TARGET     = $$qtLibraryTarget($$TARGET)
TARGETPATH = $$[QT_INSTALL_LIBS]

QT        += dbus
QT        -= gui
CONFIG    += qt debug link_pkgconfig
CONFIG    += create_pc create_prl no_install_prl
PKGCONFIG += libiphb

#DEFINES += DEBUG_TRACE

system(qdbusxml2cpp -p mceiface.h:mceiface.cpp mceiface.xml)

SOURCES += \
    displayblanking.cpp \
    displayblanking_p.cpp \
    backgroundactivity.cpp \
    backgroundactivity_p.cpp \
    mceiface.cpp \
    heartbeat.cpp

PUBLIC_HEADERS += \
    displayblanking.h \
    backgroundactivity.h

PRIVATE_HEADERS += \
    displayblanking_p.h \
    mceiface.h \
    heartbeat.h \
    backgroundactivity_p.h \
    common.h

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS

develheaders.path  = /usr/include/keepalive
develheaders.files = $$PUBLIC_HEADERS

target.path     = $$[QT_INSTALL_LIBS]

pkgconfig.files = $$PWD/pkgconfig/keepalive.pc
pkgconfig.path  = $$target.path/pkgconfig

QMAKE_PKGCONFIG_NAME        = lib$$TARGET
QMAKE_PKGCONFIG_DESCRIPTION = Nemomobile cpu/display keepalive development files
QMAKE_PKGCONFIG_LIBDIR      = $$target.path
QMAKE_PKGCONFIG_INCDIR      = $$develheaders.path
QMAKE_PKGCONFIG_DESTDIR     = pkgconfig
QMAKE_PKGCONFIG_REQUIRES    = Qt5Core Qt5DBus

INSTALLS += target develheaders pkgconfig
