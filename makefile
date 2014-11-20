# makefile
# Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
# Licensed under the Apache License Version 2.0.

LLVM_CONFIG		= /usr/local/opt/llvm/bin/llvm-config
CXX				= clang++
CXXFLAGS		= -g -stdlib=libc++ -std=gnu++1y -frtti -fexceptions -I/usr/local/opt/llvm/include

CXXSRC			= $(shell find source -iname "*.cpp")
CXXOBJ			= $(CXXSRC:.cpp=.o)

.DEFAULT_GOAL = all
.PHONY: all clean

all: run

clean:
	@rm $(CXXOBJ)

build: scripts/corec

scripts/corec: $(CXXOBJ)
	@$(CXX) `$(LLVM_CONFIG) --cxxflags --ldflags --system-libs --libs core jit native bitwriter` $(CXXFLAGS) -o $@ $(CXXOBJ)

%.o: %.cpp
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

run: scripts/corec
	@scripts/corec scripts/test.crs

compileScript: run
	@$(CXX) -o test test.bc
