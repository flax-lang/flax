# bleugh.
# so we can call from our main makefile.


.PHONY: all

all: ../liblinenoise.a
	@:

../liblinenoise.a: linenoise.c.o encodings/utf8.c.o
	@echo "# liblinenoise"
	@$(AR) rcs ../liblinenoise.a linenoise.c.o encodings/utf8.c.o

%.c.o: %.c
	@echo "# linenoise/$(notdir $<)"
	@$(CC) -std=c11 -O3 -c -o $@ $<




