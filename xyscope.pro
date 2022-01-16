QT += widgets multimedia

CONFIG += debug
SOURCES = main.cpp

LIBS += -L/usr/local/lib -lfftw3
INCLUDEPATH += /usr/local/include

# install
target.path = .
INSTALLS += target

include(../shared/shared.pri)
