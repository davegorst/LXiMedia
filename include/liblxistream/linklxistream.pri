# Linking the lxistream library
include($${PWD}/../liblxicore/linklxicore.pri)

!contains(CONFIG, staticlib) {
  !contains(LINKED, LXiStream) {
    LINKED += LXiStream
  
    unix|win32-g++:LIBS += -L$${OUT_PWD}/$${LXIMEDIA_DIR}/bin -lLXiStream
    win32-msvc2005|win32-msvc2008|win32-msvc2010:LIBS += $$replace(OUT_PWD,/,\\)\\$$replace(LXIMEDIA_DIR,/,\\)\\bin\\LXiStream.lib
  }
}
