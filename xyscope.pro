QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
QT += widgets multimedia

CONFIG += debug
SOURCES = main.cpp raster_view.cpp analytic_scope.cpp spectrum_scope.cpp
HEADERS = raster_view.hpp raster_image.hpp spectrum_scope.hpp analytic_scope.hpp

LIBS += -L/usr/local/lib -lfftw3_omp -lm -lfftw3
INCLUDEPATH += /usr/local/include

# install
target.path = .
INSTALLS += target
