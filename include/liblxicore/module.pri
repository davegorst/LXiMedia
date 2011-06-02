TEMPLATE = lib
CONFIG += plugin
CONFIG += qt thread warn_on

macx|win32 {
  DESTDIR = $${OUT_PWD}/$${LXIMEDIA_DIR}/bin/lximedia
} else {
  LXIMEDIA_VERSION_MAJOR = $$system(cat $${PWD}/../../VERSION)
  LXIMEDIA_VERSION_MAJOR ~= s/\\.[0-9]+.+/
  DESTDIR = $${OUT_PWD}/$${LXIMEDIA_DIR}/bin/lximedia$${LXIMEDIA_VERSION_MAJOR}
}

TARGET = $${MODULE_NAME}
INCLUDEPATH += $${PWD}/../../src/
DEPENDPATH += $${PWD}/../../src/
include($${PWD}/../config.pri)

unix {
    target.path = /usr/lib/lximedia0
    INSTALLS += target
}
