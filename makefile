# Makefile for Orion-X3/Orion-X4/mx and derivatives
# Written in 2011
# This makefile is licensed under the WTFPL



WARNINGS		:= -Wno-unused-parameter -Wno-sign-conversion -Wno-padded -Wno-conversion -Wno-shadow -Wno-missing-noreturn -Wno-unused-macros -Wno-switch-enum -Wno-deprecated -Wno-format-nonliteral -Wno-trigraphs -Wno-unused-const-variable


CLANGWARNINGS	:= -Wno-undefined-func-template -Wno-comma -Wno-nullability-completeness -Wno-redundant-move -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-reserved-id-macro -Wno-extra-semi -Wno-gnu-zero-variadic-macro-arguments -Wno-shift-sign-overflow -Wno-exit-time-destructors -Wno-global-constructors -Wno-c++98-compat-pedantic -Wno-documentation-unknown-command -Wno-weak-vtables -Wno-c++98-compat


SYSROOT			:= build/sysroot
PREFIX			:= usr/local
OUTPUTBIN		:= flaxc

OUTPUT			:= $(SYSROOT)/$(PREFIX)/bin/$(OUTPUTBIN)

CC				?= "clang"
CXX				?= "clang++"
LLVM_CONFIG		?= "llvm-config"


CXXSRC			:= $(shell find source external -iname "*.cpp")
CSRC			:= $(shell find source external -iname "*.c")

CXXOBJ			:= $(CXXSRC:.cpp=.cpp.o)
COBJ			:= $(CSRC:.c=.c.o)

PRECOMP_HDRS	:= source/include/precompile.h
PRECOMP_GCH		:= $(PRECOMP_HDRS:.h=.h.gch)

FLXLIBLOCATION	:= $(SYSROOT)/$(PREFIX)/lib
FLXSRC			:= $(shell find libs -iname "*.flx")

CXXDEPS			:= $(CXXSRC:.cpp=.cpp.d)


NUMFILES		:= $$(($(words $(CXXSRC)) + $(words $(CSRC))))



SANITISE		:=

CXXFLAGS		+= -std=c++1z -O0 -g -c -Wall -frtti -fexceptions -fno-omit-frame-pointer -Wno-old-style-cast $(SANITISE)
CFLAGS			+= -std=c11 -O0 -g -c -Wall -fno-omit-frame-pointer -Wno-overlength-strings $(SANITISE)

LDFLAGS			+= $(SANITISE)

FLXFLAGS		+= -sysroot $(SYSROOT)


SUPERTINYBIN	:= build/supertiny
GLTESTBIN		:= build/gltest
TESTBIN			:= build/tester

SUPERTINYSRC	:= build/supertiny.flx
GLTESTSRC		:= build/gltest.flx
TESTSRC			:= build/tester.flx



.DEFAULT_GOAL = osx
-include $(CXXDEPS)


.PHONY: copylibs jit compile clean build osx linux ci prep satest tiny osxflags

prep:
	@# echo C++ compiler is: $(CXX)
	@mkdir -p $(dir $(OUTPUT))

osxflags: CXXFLAGS += -march=native -fmodules -Weverything -Xclang -fcolor-diagnostics $(SANITISE) $(CLANGWARNINGS)
osxflags: CFLAGS += -fmodules -Xclang -fcolor-diagnostics $(SANITISE) $(CLANGWARNINGS)

osxflags:


osx: prep jit osxflags

satest: prep osxflags build
	@$(OUTPUT) $(FLXFLAGS) -run build/standalone.flx

tester: prep osxflags build
	@$(OUTPUT) $(FLXFLAGS) -run build/tester.flx

ci: prep test

linux: prep jit

jit: build
	@$(OUTPUT) $(FLXFLAGS) -run -o $(SUPERTINYBIN) $(SUPERTINYSRC)

compile: build
	@$(OUTPUT) $(FLXFLAGS) -o $(SUPERTINYBIN) $(SUPERTINYSRC) -lm

test: build
	@$(OUTPUT) $(FLXFLAGS) -run -o $(TESTBIN) $(TESTSRC)

gltest: build
	@$(OUTPUT) $(FLXFLAGS) -run -framework GLUT -framework OpenGL -lsdl2 -o $(GLTESTBIN) $(GLTESTSRC)

build: $(OUTPUT) copylibs
	# built

build/%.flx: build
	@$(OUTPUT) $(FLXFLAGS) -run $@



copylibs: $(FLXSRC)
	@mkdir -p $(FLXLIBLOCATION)/flaxlibs
	@cp -R libs $(FLXLIBLOCATION)/
	@rm -r $(FLXLIBLOCATION)/flaxlibs
	@mv $(FLXLIBLOCATION)/libs $(FLXLIBLOCATION)/flaxlibs


$(OUTPUT): $(PRECOMP_GCH) $(CXXOBJ) $(COBJ)
	@printf "# linking\n"
	@$(CXX) -o $@ $(CXXOBJ) $(COBJ) $(shell $(LLVM_CONFIG) --cxxflags --ldflags --system-libs --libs core engine native linker bitwriter lto vectorize all-targets object) -lmpfr -lgmp $(LDFLAGS) -lpthread


%.cpp.o: %.cpp
	@$(eval DONEFILES += "CPP")
	@printf "# compiling [$(words $(DONEFILES))/$(NUMFILES)] $<\n"
	@$(CXX) $(CXXFLAGS) $(WARNINGS) -include source/include/precompile.h -Isource/include -Iexternal -I$(shell $(LLVM_CONFIG) --includedir) -MMD -MP -o $@ $<


%.c.o: %.c
	@$(eval DONEFILES += "C")
	@printf "# compiling [$(words $(DONEFILES))/$(NUMFILES)] $<\n"
	@$(CC) $(CFLAGS) $(WARNINGS) -Iexternal/utf8rewind/include/utf8rewind -MMD -MP -o $@ $<


%.h.gch: %.h
	@printf "# precompiling header $<\n"
	@$(CXX) $(CXXFLAGS) $(WARNINGS) -o $@ $<


# haha
clena: clean
clean:
	@rm -f $(OUTPUT)
	@find source -name "*.o" | xargs rm -f
	@find source -name "*.gch*" | xargs rm -f
	@find source -name "*.pch*" | xargs rm -f

	@find source -name "*.c.m" | xargs rm -f
	@find source -name "*.c.d" | xargs rm -f
	@find source -name "*.cpp.m" | xargs rm -f
	@find source -name "*.cpp.d" | xargs rm -f









