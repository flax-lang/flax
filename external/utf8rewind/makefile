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

%.c.o: %.c
	@echo "# utf8rewind/$(notdir $<)"
	@$(CC) -std=c11 -O3 -c -Iinclude -o $@ $<




