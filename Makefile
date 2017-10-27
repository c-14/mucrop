# mucrop
# See LICENSE file for copyright and license details.

include config.mk

BDIR = $(DESTDIR)/$(PREFIX)
DEPS = window.h util/error.h util/mem.h util/time.h
OBJS = mucrop.o window.c util/error.o util/mem.o util/time.o

.PHONY: all clean install

all: mucrop

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

mucrop: $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

install: mucrop
	install -D mucrop $(BDIR)/bin/mucrop

clean:
	rm -f mucrop *.o util/*.o
