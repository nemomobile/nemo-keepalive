# -*- mode: sh -*-
TEMPLATE = subdirs

SUBDIRS += lib
SUBDIRS += plugin
SUBDIRS += examples/backgroundactivity_periodic
SUBDIRS += examples/backgroundactivity_linger
SUBDIRS += examples/displayblanking
SUBDIRS += tests

examples.files = examples/qml/*.qml
examples.path = $$[QT_INSTALL_EXAMPLES]/keepalive

CONFIG  += ordered

INSTALLS += examples
