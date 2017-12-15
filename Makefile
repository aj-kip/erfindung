PROG = erfindung-cli
CXX = g++
LD = g++
CXXFLAGS = -pthread -Ofast -std=c++11 -Wall -pedantic -Werror -DMACRO_BUILD_STL_ONLY -DMACRO_COMPILER_GCC -Wno-maybe-uninitialized
LFLAGS = -pthread
OBJECTS_DIR = .make_objects

default: $(PROG)

SRCS = \
	src/main.cpp \
	src/parse_program_options.cpp \
	src/ErfiCpu.cpp \
	src/Assembler.cpp \
	src/FixedPointUtil.cpp \
	src/ErfiApu.cpp \
	src/ErfiGpu.cpp \
	src/Debugger.cpp \
	src/ErfiConsole.cpp \
	src/ErfiDefs.cpp \
	src/AssemblerPrivate/TextProcessState.cpp \
	src/AssemblerPrivate/ProcessIoLine.cpp \
	src/AssemblerPrivate/LineParsingHelpers.cpp \
	src/AssemblerPrivate/GetLineProcessingFunction.cpp \
	src/AssemblerPrivate/make_generic_instructions.cpp \
	src/tests.cpp

clean:
	rm $(OBJS)

$(OBJECTS_DIR)/%.o:
	$(CXX) $(CXXFLAGS) -c $*.cpp -o $(OBJECTS_DIR)/$@

OBJS = $(SRCS:%.cpp=%.o)

$(PROG): $(OBJS)
	$(LD) $(LFLAGS) $(OBJS) -o $(PROG)

profile: CXXFLAGS += -pg 
profile: LFLAGS += -pg
profile: $(PROG)

test: $(PROG)
	./$(PROG) -t
	./$(PROG)    > help-text-implicit.txt
	./$(PROG) -h > help-text-explicit.txt
	diff help-text-implicit.txt help-text-explicit.txt
	rm help-text-implicit.txt help-text-explicit.txt
