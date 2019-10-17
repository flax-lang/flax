# bleugh.
# so we can call from our main makefile.

CSRC    := $(shell find source -name "*.c")
COBJ    := $(CSRC:.c=.c.o)

.PHONY: all

all: ../libutf8rewind.a
	@:

../libutf8rewind.a: $(COBJ)
	@echo "# libutf8rewind"
	@$(AR) rcs ../libutf8rewind.a $(COBJ)

%.cc.o: %.cc
	@echo "# fmt/$(notdir $<)"
	@$(CXX) -std=c11 -O3 -c -Iinclude -o $@ $<




