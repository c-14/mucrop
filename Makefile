# mucrop
# See LICENSE file for copyright and license details.

include config.mk

DEPS    = window.h util/error.h util/mem.h
OBJS    = mucrop.o window.c util/error.o util/mem.o

.PHONY: all clean

all: mucrop

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

mucrop: $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f mucrop *.o util/*.o
