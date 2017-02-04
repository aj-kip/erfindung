#message($$CONFIG)
TEMPLATE = app

CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
	../src/main.cpp \
    ../src/DrawRectangle.cpp \
    ../src/ErfiGpu.cpp \
    ../src/Assembler.cpp \
    ../src/ErfiCpu.cpp \
    ../src/ErfiDefs.cpp \
    ../src/FixedPointUtil.cpp \
    ../src/AssemblerPrivate/TextProcessState.cpp \
    ../src/AssemblerPrivate/GetLineProcessingFunction.cpp \
    ../src/AssemblerPrivate/LineParsingHelpers.cpp \
    ../src/Debugger.cpp
    
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
    ../src/AssemblerPrivate/CommonDefinitions.hpp \
    ../src/Debugger.hpp

TARGET = erfindung

QMAKE_CXXFLAGS += -pthread -Wno-maybe-uninitialized
QMAKE_LFLAGS   += -pthread

CONFIG (debug, debug|release) {
    DEFINES += MACRO_DEBUG linux
    QMAKE_CXXFLAGS += -O0
}

CONFIG (release, debug|release) {
    DEFINES += linux NDEBUG MACRO_RELEASE
    CONFIG -= debug import_qpa_plugin import_plugins testcase_targets qt qpa gcc file_copies warn_on release link_prl incremental shared c++11 console
    QMAKE_CXXFLAGS_RELEASE -= -O2
    QMAKE_CXXFLAGS_RELEASE -= -pg
    QMAKE_LFLAGS_RELEASE   -= -O1
    QMAKE_LFLAGS_RELEASE   -= -pg
    QMAKE_CXXFLAGS += -Ofast
    QMAKE_LFLAGS   += -Ofast -flto
}

linux-g++ {
    DEFINES += MACRO_COMPILER_GCC
}

linux-clang {
    DEFINES += MACRO_COMPILER_CLANG
}

INCLUDEPATH += \
    /media/data/dev/c++/inc

QMAKE_CXXFLAGS += -std=c++11 -Wall -pedantic -Werror

LIBS += -L/media/data/dev/c++/lib/g++/GnuLinux64/ \
	-L/usr/lib/ \
    -lsfml-system \
	-lsfml-graphics \
	-lsfml-window \
    -lsfml-audio
