TEMPLATE = subdirs
CONFIG += ordered

win32 {
  OUT_DIR = $$replace(OUT_PWD,/,\\)\\..\\bin

  system(copy /Y $$replace(PWD,/,\\)\\..\\COPYING $${OUT_DIR} > NUL)
  system(copy /Y $$replace(PWD,/,\\)\\..\\README $${OUT_DIR} > NUL)
  system(copy /Y $$replace(PWD,/,\\)\\..\\VERSION $${OUT_DIR} > NUL)

  release {
    system(copy /Y $$replace(PWD,/,\\)\\win32\\*.nsi $${OUT_DIR} > NUL)
  }
}