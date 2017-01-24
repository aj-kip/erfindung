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
    ../src/FixedPointUtil.cpp \
    ../src/AssemblerPrivate/TextProcessState.cpp \
    ../src/AssemblerPrivate/GetLineProcessingFunction.cpp \
    ../src/AssemblerPrivate/LineParsingHelpers.cpp
    
HEADERS += \
    ../src/DrawRectangle.hpp \
    ../src/FixedPointUtil.hpp \
    ../src/ErfiGpu.hpp \
    ../src/ErfiDefs.hpp \
    ../src/Assembler.hpp \
    ../src/ErfiCpu.hpp \
    ../src/ErfiError.hpp \
    ../src/StringUtil.hpp \
    ../src/AssemblerPrivate/TextProcessState.hpp \
    ../src/AssemblerPrivate/GetLineProcessingFunction.hpp \
    ../src/AssemblerPrivate/LineParsingHelpers.hpp \
    ../src/AssemblerPrivate/CommonDefinitions.hpp

TARGET = erfindung

QMAKE_CXXFLAGS += -pthread
QMAKE_LFLAGS   += -pthread

CONFIG (debug, debug|release) {
    DEFINES += MACRO_DEBUG linux
    QMAKE_CXXFLAGS += -O0
}

CONFIG (release, debug|release) {
    DEFINES += linux NDEBUG MACRO_RELEASE
	QMAKE_CXXFLAGS += -O3
}

linux-g++ {
    DEFINES += MACRO_COMPILER_GCC
}

linux-clang {
    DEFINES += MACRO_COMPILER_CLANG
}

INCLUDEPATH += \
    /media/data/dev/c++/inc

QMAKE_CXXFLAGS += -std=c++11 -Wall -pedantic -pg -Werror

LIBS += -L/media/data/dev/c++/lib/g++/GnuLinux64/ \
	-L/usr/lib/ \
    -lsfml-system \
	-lsfml-graphics \
	-lsfml-window \
    -lsfml-audio
