CXX := clang++
CXXFLAGS := --std=c++11 -g -Wall $(shell pkg-config --libs --cflags libelf++ libdwarf++) -O0

all: profiler

clean:
	rm -rf profiler

profiler: profiler.cc inspect.h
	$(CXX) $(CXXFLAGS) -o $@ profiler.cc

