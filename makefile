# Makefile for Orion-X3/Orion-X4/mx and derivatives
# Written in 2011
# This makefile is licensed under the WTFPL



WARNINGS		+= -Wno-unused-parameter -Wno-sign-conversion -Wno-padded -Wno-c++98-compat -Wno-weak-vtables -Wno-documentation-unknown-command -Wno-old-style-cast -Wno-c++98-compat-pedantic -Wno-conversion -Wno-shadow -Wno-global-constructors -Wno-exit-time-destructors -Wno-missing-noreturn -Wno-unused-macros -Wno-switch-enum -Wno-deprecated -Wno-shift-sign-overflow -Wno-format-nonliteral -Wno-gnu-zero-variadic-macro-arguments -Wno-trigraphs -Wno-extra-semi -Wno-reserved-id-macro -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-redundant-move -Wno-nullability-completeness -Wno-comma -Wno-undefined-func-template -Wno-unused-const-variable


SYSROOT			:= build/sysroot
PREFIX			:= usr/local
OUTPUTBIN		:= flaxc

OUTPUT			:= $(SYSROOT)/$(PREFIX)/$(OUTPUTBIN)

CC				?= "clang"
CXX				?= "clang++"
LLVM_CONFIG		?= "llvm-config-3.8"


CXXSRC			:= $(shell find source -iname "*.cpp")
CSRC			:= $(shell find source -iname "*.c")

CXXOBJ			:= $(CXXSRC:.cpp=.cpp.o)
COBJ			:= $(CSRC:.c=.c.o)

FLXLIBLOCATION	:= $(SYSROOT)/$(PREFIX)/lib
FLXSRC			:= $(shell find libs -iname "*.flx")

CXXDEPS			:= $(CXXSRC:.cpp=.cpp.m)


NUMFILES		:= $$(($(words $(CXXSRC)) + $(words $(CSRC))))



SANITISE		:=

CXXFLAGS		+= -std=c++1z -O0 -g -c -fmodules -Wall -Weverything -frtti -fexceptions -fno-omit-frame-pointer -Xclang -fcolor-diagnostics $(SANITISE)
CFLAGS			+= -std=c11 -O0 -g -c -fmodules -Wall -fno-omit-frame-pointer -Xclang -fcolor-diagnostics -Wno-overlength-strings -Wno-missing-variable-declarations

LDFLAGS			+= $(SANITISE)

FLXFLAGS		+= -sysroot $(SYSROOT)


TESTBIN			:= build/test
GLTESTBIN		:= build/gltest

TESTSRC			:= build/test.flx
GLTESTSRC		:= build/gltest.flx



.DEFAULT_GOAL = jit
-include $(CXXDEPS)


.PHONY: copylibs jit compile clean build

jit: build
	@$(OUTPUT) $(FLXFLAGS) -run -o $(TESTBIN)  $(TESTSRC)

compile: build
	@$(OUTPUT) $(FLXFLAGS) -o $(TESTBIN)  $(TESTSRC)

gltest: build
	@$(OUTPUT) $(FLXFLAGS) -run -framework GLUT -framework OpenGL -lsdl2 -o $(GLTESTBIN)  $(GLTESTSRC)

build: $(OUTPUT) copylibs
	# built


copylibs: $(FLXSRC)
	@mkdir -p $(FLXLIBLOCATION)/flaxlibs
	@cp -R libs $(FLXLIBLOCATION)/
	@rm -r $(FLXLIBLOCATION)/flaxlibs
	@mv $(FLXLIBLOCATION)/libs $(FLXLIBLOCATION)/flaxlibs


$(OUTPUT): $(CXXOBJ) $(COBJ)
	@$(CXX) -o $@ $(shell $(LLVM_CONFIG) --cxxflags --ldflags --system-libs --libs core engine native linker bitwriter lto vectorize) $(CXXOBJ) $(COBJ) $(LDFLAGS)


%.cpp.o: %.cpp
	@$(CXX) $(CXXFLAGS) $(WARNINGS) -Isource/include -I$(shell $(LLVM_CONFIG) --includedir) -MMD -MF $<.m -o $@ $<

	@$(eval DONEFILES += "CPP")
	@printf "\r                                               \r$(words $(DONEFILES)) / $(NUMFILES) ($(notdir $<))"


%.c.o: %.c
	@$(CC) $(CFLAGS) $(WARNINGS) -Isource/utf8rewind/include/utf8rewind -MMD -MF $<.m -o $@ $<

	@$(eval DONEFILES += "C")
	@printf "\r                                               \r$(words $(DONEFILES)) / $(NUMFILES) ($(notdir $<))"





# haha
clena: clean
clean:
	@find source -name "*.o" | xargs rm -f
	@find source -name "*.c.d" | xargs rm -f
	@find source -name "*.cpp.d" | xargs rm -f









