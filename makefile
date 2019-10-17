# Makefile for Orion-X3/Orion-X4/mx and derivatives
# Written in 2011
# This makefile is licensed under the WTFPL



WARNINGS		:= -Wno-unused-parameter -Wno-sign-conversion -Wno-padded -Wno-conversion -Wno-shadow -Wno-missing-noreturn -Wno-unused-macros -Wno-switch-enum -Wno-deprecated -Wno-format-nonliteral -Wno-trigraphs -Wno-unused-const-variable -Wno-deprecated-declarations

GCCWARNINGS     := -Wno-init-list-lifetime


CLANGWARNINGS	:= -Wno-undefined-func-template -Wno-comma -Wno-nullability-completeness -Wno-redundant-move -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-reserved-id-macro -Wno-extra-semi -Wno-gnu-zero-variadic-macro-arguments -Wno-shift-sign-overflow -Wno-exit-time-destructors -Wno-global-constructors -Wno-c++98-compat-pedantic -Wno-documentation-unknown-command -Wno-weak-vtables -Wno-c++98-compat -Wold-style-cast


SYSROOT			:= build/sysroot
PREFIX			:= usr/local
OUTPUTBIN		:= flaxc

OUTPUT			:= $(SYSROOT)/$(PREFIX)/bin/$(OUTPUTBIN)

CC				?= "clang"
CXX				?= "clang++"
LLVM_CONFIG		?= "llvm-config"


CXXSRC			:= $(shell find source external -iname "*.cpp")
CXXOBJ			:= $(CXXSRC:.cpp=.cpp.o)
CXXDEPS			:= $(CXXSRC:.cpp=.cpp.d)

PRECOMP_HDRS	:= source/include/precompile.h
PRECOMP_GCH		:= $(PRECOMP_HDRS:.h=.h.gch)

FLXLIBLOCATION	:= $(SYSROOT)/$(PREFIX)/lib
FLXSRC			:= $(shell find libs -iname "*.flx")


NUMFILES		:= $$(($(words $(CXXSRC))))

DEFINES         := -D__USE_MINGW_ANSI_STDIO=1
SANITISE		:=

CXXFLAGS		+= -std=c++17 -O0 -g -c -Wall -frtti -fno-exceptions -fno-omit-frame-pointer $(SANITISE) $(DEFINES)
CFLAGS			+= -std=c11 -O0 -g -c -Wall -fno-omit-frame-pointer -Wno-overlength-strings $(SANITISE) $(DEFINES)

LDFLAGS			+= $(SANITISE)



UNAME_IDENT		:= $(shell uname)
COMPILER_IDENT	:= $(shell $(CC) --version | head -n 1)


ifeq ("$(UNAME_IDENT)","Darwin")
	LIBFFI_CFLAGS  := $(shell env PKG_CONFIG_PATH=/usr/local/opt/libffi/lib/pkgconfig pkg-config --cflags libffi)
	LIBFFI_LDFLAGS := $(shell env PKG_CONFIG_PATH=/usr/local/opt/libffi/lib/pkgconfig pkg-config --libs libffi)
else
	LIBFFI_CFLAGS  := $(shell pkg-config --cflags libffi)
	LIBFFI_LDFLAGS := $(shell pkg-config --libs libffi)

	# on linux, we need to explicitly export our functions
	# like __declspec(dllexport) except __attribute__((visibility("default")))
	LDFLAGS += -Wl,--export-dynamic
endif

MPFR_CFLAGS     := $(shell pkg-config --cflags mpfr)
MPFR_LDFLAGS    := $(shell pkg-config --libs mpfr)

CFLAGS   += $(MPFR_CFLAGS) $(LIBFFI_CFLAGS)
CXXFLAGS += $(MPFR_CFLAGS) $(LIBFFI_CFLAGS)
LDFLAGS  += $(MPFR_LDFLAGS) $(LIBFFI_LDFLAGS)

ifneq (,$(findstring clang,$(COMPILER_IDENT)))
	CXXFLAGS += -Xclang -fcolor-diagnostics $(SANITISE) $(CLANGWARNINGS)
	CFLAGS   += -Xclang -fcolor-diagnostics $(SANITISE) $(CLANGWARNINGS)
else
	CXXFLAGS += (GCCWARNINGS)
endif


UTF8REWIND_AR   := external/libutf8rewind.a


FLXFLAGS		+= -sysroot $(SYSROOT) --ffi-escape

SUPERTINYBIN	:= build/supertiny
GLTESTBIN		:= build/gltest
TESTBIN			:= build/tester

SUPERTINYSRC	:= build/supertiny.flx
GLTESTSRC		:= build/gltest.flx
TESTSRC			:= build/tester.flx


.DEFAULT_GOAL = jit
-include $(CXXDEPS)


.PHONY: copylibs jit compile clean build linux ci satest tiny

satest: build
	@$(OUTPUT) $(FLXFLAGS) -run build/standalone.flx

tester: build
	@$(OUTPUT) $(FLXFLAGS) -run build/tester.flx

ci: test

jit: build
	@$(OUTPUT) $(FLXFLAGS) -run -o $(SUPERTINYBIN) $(SUPERTINYSRC)

compile: build
	@$(OUTPUT) $(FLXFLAGS) -o $(SUPERTINYBIN) $(SUPERTINYSRC) -lm

test: build
	@$(OUTPUT) $(FLXFLAGS) -run -o $(TESTBIN) $(TESTSRC)

gltest: build
	@$(OUTPUT) $(FLXFLAGS) -run -framework GLUT -framework OpenGL -lsdl2 -o $(GLTESTBIN) $(GLTESTSRC)

build1:

build: build1 $(OUTPUT) copylibs
	# built

build/%.flx: build
	@$(OUTPUT) $(FLXFLAGS) -run -profile $@



copylibs: $(FLXSRC)
	@mkdir -p $(FLXLIBLOCATION)/flaxlibs
	@cp -R libs $(FLXLIBLOCATION)/
	@rm -r $(FLXLIBLOCATION)/flaxlibs
	@mv $(FLXLIBLOCATION)/libs $(FLXLIBLOCATION)/flaxlibs


$(OUTPUT): $(PRECOMP_GCH) $(CXXOBJ) $(COBJ) $(UTF8REWIND_AR)
	@printf "# linking\n"
	@mkdir -p $(dir $(OUTPUT))
	@$(CXX) -o $@ $(CXXOBJ) $(COBJ) $(LDFLAGS) -Lexternal $(shell $(LLVM_CONFIG) --cxxflags --ldflags --system-libs --libs core engine native linker bitwriter lto vectorize all-targets object orcjit) -lmpfr -lgmp -lpthread -ldl -lffi -lutf8rewind


%.cpp.o: %.cpp
	@$(eval DONEFILES += "CPP")
	@printf "# compiling [$(words $(DONEFILES))/$(NUMFILES)] $<\n"
	@$(CXX) $(CXXFLAGS) $(WARNINGS) -include source/include/precompile.h -Isource/include -Iexternal -I$(shell $(LLVM_CONFIG) --includedir) -MMD -MP -o $@ $<

%.h.gch: %.h
	@printf "# precompiling header $<\n"
	@$(CXX) $(CXXFLAGS) $(WARNINGS) -o $@ $<


$(UTF8REWIND_AR):
	@make -C external/utf8rewind all


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









