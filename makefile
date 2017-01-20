# Makefile for Orion-X3/Orion-X4/mx and derivatives
# Written in 2011
# This makefile is licensed under the WTFPL



WARNINGS		:= -Wno-unused-parameter -Wno-sign-conversion -Wno-padded -Wno-old-style-cast -Wno-conversion -Wno-shadow -Wno-missing-noreturn -Wno-unused-macros -Wno-switch-enum -Wno-deprecated -Wno-format-nonliteral -Wno-trigraphs -Wno-unused-const-variable


CLANGWARNINGS	:= -Wno-undefined-func-template -Wno-comma -Wno-nullability-completeness -Wno-redundant-move -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-reserved-id-macro -Wno-extra-semi -Wno-gnu-zero-variadic-macro-arguments -Wno-shift-sign-overflow -Wno-exit-time-destructors -Wno-global-constructors -Wno-c++98-compat-pedantic -Wno-documentation-unknown-command -Wno-weak-vtables -Wno-c++98-compat


SYSROOT			:= build/sysroot
PREFIX			:= usr/local
OUTPUTBIN		:= flaxc

OUTPUT			:= $(SYSROOT)/$(PREFIX)/bin/$(OUTPUTBIN)

CC				?= "clang"
CXX				?= "clang++"
LLVM_CONFIG		?= "llvm-config"


CXXSRC			:= $(shell find source -iname "*.cpp")
CSRC			:= $(shell find source -iname "*.c")

CXXOBJ			:= $(CXXSRC:.cpp=.cpp.o)
COBJ			:= $(CSRC:.c=.c.o)

FLXLIBLOCATION	:= $(SYSROOT)/$(PREFIX)/lib
FLXSRC			:= $(shell find libs -iname "*.flx")

CXXDEPS			:= $(CXXSRC:.cpp=.cpp.m)


NUMFILES		:= $$(($(words $(CXXSRC)) + $(words $(CSRC))))



SANITISE		:=

CXXFLAGS		+= -std=c++14 -O3 -g -c -Wall -frtti -fexceptions -fno-omit-frame-pointer -Wno-old-style-cast
CFLAGS			+= -std=c11 -O3 -g -c -Wall -fno-omit-frame-pointer -Wno-overlength-strings

LDFLAGS			+= $(SANITISE)

FLXFLAGS		+= -sysroot $(SYSROOT)


TESTBIN			:= build/test
GLTESTBIN		:= build/gltest

TESTSRC			:= build/test.flx
GLTESTSRC		:= build/gltest.flx



.DEFAULT_GOAL = osx
-include $(CXXDEPS)


.PHONY: copylibs jit compile clean build osx linux ci prep satest osxflags

prep:
	@mkdir -p $(dir $(OUTPUT))

osxflags: CXXFLAGS += -march=native -fmodules -Weverything -Xclang -fcolor-diagnostics $(SANITISE) $(CLANGWARNINGS)
osxflags: CFLAGS += -fmodules -Xclang -fcolor-diagnostics $(SANITISE) $(CLANGWARNINGS)

osxflags:


osx: prep jit osxflags

satest: prep osxflags build
	@$(OUTPUT) $(FLXFLAGS) -run -o build/standalone build/standalone.flx

ci: linux

linux: prep jit

jit: build
	@$(OUTPUT) $(FLXFLAGS) -run -o $(TESTBIN) $(TESTSRC)

compile: build
	@$(OUTPUT) $(FLXFLAGS) -o $(TESTBIN) $(TESTSRC) -lm

gltest: build
	@$(OUTPUT) $(FLXFLAGS) -run -framework GLUT -framework OpenGL -lsdl2 -o $(GLTESTBIN) $(GLTESTSRC)

build: $(OUTPUT) copylibs
	# built


copylibs: $(FLXSRC)
	@mkdir -p $(FLXLIBLOCATION)/flaxlibs
	@cp -R libs $(FLXLIBLOCATION)/
	@rm -r $(FLXLIBLOCATION)/flaxlibs
	@mv $(FLXLIBLOCATION)/libs $(FLXLIBLOCATION)/flaxlibs


$(OUTPUT): $(CXXOBJ) $(COBJ)
	@printf "# linking\n"
	@$(CXX) -o $@ $(CXXOBJ) $(COBJ) $(shell $(LLVM_CONFIG) --cxxflags --ldflags --system-libs --libs core engine native linker bitwriter lto vectorize all-targets object) $(LDFLAGS)


%.cpp.o: %.cpp
	@$(eval DONEFILES += "CPP")
	@printf "# compiling [$(words $(DONEFILES))/$(NUMFILES)] $<\n"
	@$(CXX) $(CXXFLAGS) $(WARNINGS) -Isource/include -I$(shell $(LLVM_CONFIG) --includedir) -MMD -MP -MF $<.m -o $@ $<


%.c.o: %.c
	@$(eval DONEFILES += "C")
	@printf "# compiling [$(words $(DONEFILES))/$(NUMFILES)] $<\n"
	@$(CC) $(CFLAGS) $(WARNINGS) -Isource/utf8rewind/include/utf8rewind -MMD -MP -MF $<.m -o $@ $<





# haha
clena: clean
clean:
	@rm $(OUTPUT)
	@find source -name "*.o" | xargs rm -f
	@find source -name "*.c.d" | xargs rm -f
	@find source -name "*.cpp.d" | xargs rm -f









