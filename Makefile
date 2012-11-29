CFLAGS := -Wall -D_FILE_OFFSET_BITS=64
CLIBS := -lfuse -pthread

all: simple-cow

simple-cow: simple-cow.c
	gcc $(CFLAGS) $(CLIBS) -o $@ $<
