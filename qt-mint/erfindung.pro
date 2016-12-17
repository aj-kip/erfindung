TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    ../src/main.cpp \
    \#../src/DrawRectangle.cpp \
    ../src/ErfiGpu.cpp

HEADERS += \
    \#../src/DrawRectangle.hpp \
    ../src/FixedPointUtil.hpp \
    ../src/ErfiGpu.hpp \
    ../src/ErfiDefs.hpp

TARGET = erfindung

CONFIG (debug, debug|release) {
	DEFINES += DEBUG ACL_DEBUG linux
	QMAKE_CXXFLAGS += -O0
}

CONFIG (release, debug|release) {
	DEFINES += ACL_NO_DEBUG linux NDEBUG
	QMAKE_CXXFLAGS += -O3
}

INCLUDEPATH += \
    /home/andy/Documents/dev/c++/inc

QMAKE_CXXFLAGS += -std=c++11 -Wall -pedantic -pg -Werror -static-libstdc++

LIBS += -L/home/andy/Documents/dev/c++/lib/g++/GnuLinux64 \
	-L/usr/lib/ \
    -lz #\
    #-lsfml-system \
    #-lsfml-graphics \
    #-lsfml-window \
    #-lsfml-audio
