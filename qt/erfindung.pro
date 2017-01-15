TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
	../src/main.cpp \
    ../src/DrawRectangle.cpp \
    ../src/ErfiGpu.cpp \
    ../src/Assembler.cpp \
    ../src/ErfiCpu.cpp  \
    ../src/FixedPointUtil.cpp
    
HEADERS += \
    ../src/DrawRectangle.hpp \
    ../src/FixedPointUtil.hpp \
    ../src/ErfiGpu.hpp \
    ../src/ErfiDefs.hpp \
    ../src/Assembler.hpp \
    ../src/ErfiCpu.hpp

TARGET = erfindung

CONFIG (debug, debug|release) {
    DEFINES += MACRO_DEBUG linux MACRO_COMPILER_GCC
    QMAKE_CXXFLAGS += -O0 -pthread
    QMAKE_LFLAGS += -pthread
}

CONFIG (release, debug|release) {
    DEFINES += linux NDEBUG MACRO_RELEASE MACRO_COMPILER_GCC
	QMAKE_CXXFLAGS += -O3
}

INCLUDEPATH += \
    /media/data/dev/c++/inc

QMAKE_CXXFLAGS += -std=c++11 -Wall -pedantic -pg -Werror

LIBS += -L/media/data/dev/c++/lib/g++/GnuLinux64/ \
	-L/usr/lib/ \
    \ #-lz \
	-lsfml-system \
	-lsfml-graphics \
	-lsfml-window \
	-lsfml-audio \
