TEMPLATE = lib
TARGET   = keepaliveplugin
TARGET   = $$qtLibraryTarget($$TARGET)

MODULENAME = org/nemomobile/keepalive
TARGETPATH = $$[QT_INSTALL_QML]/$$MODULENAME

QT          += qml
CONFIG      += plugin
INCLUDEPATH += ../lib
LIBS        += -L../lib -lkeepalive

import.files = qmldir *.qml
import.path  = $$TARGETPATH
target.path  = $$TARGETPATH

OTHER_FILES += qmldir
OTHER_FILES += *.qml

SOURCES += plugin.cpp

SOURCES += declarativebackgroundactivity.cpp
HEADERS += declarativebackgroundactivity.h

INSTALLS += target import
