# makefile
# Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
# Licensed under the Apache License Version 2.0.

CXX				= clang++
CXXFLAGS			= -stdlib=libc++ -std=gnu++11 -Isource/include

CXXSRC			= $(shell find source -iname "*.cpp")

.DEFAULT_GOAL = all
.PHONY: all

all: run


scripts/corec: $(CXXSRC)
	@$(CXX) $(CXXFLAGS) -o $@ $(CXXSRC)

run: scripts/corec
	@scripts/corec scripts/test.crs
