QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
QMAKE_CXXFLAGS += -Xpreprocessor -fopenmp -lomp -I/usr/local/include
QMAKE_LFLAGS +=  -lomp
QT += widgets multimedia

CONFIG += debug
SOURCES = main.cpp raster_view.cpp hilbert_scatter_view.cpp spectrum_view.cpp
HEADERS = raster_view.hpp hilbert_scatter_view.hpp spectrum_view.hpp

LIBS += -L/usr/local/lib -lfftw3_omp -lm -lfftw3 -lboost_container -lomp 
INCLUDEPATH += /usr/local/include

# install
target.path = .
INSTALLS += target
