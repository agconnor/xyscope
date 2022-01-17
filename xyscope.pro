QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
QT += widgets multimedia

CONFIG += debug
SOURCES = main.cpp

LIBS += -L/usr/local/lib -lfftw3 -lboost_container
INCLUDEPATH += /usr/local/include

# install
target.path = .
INSTALLS += target
