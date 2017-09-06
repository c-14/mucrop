# mucrop
# See LICENSE file for copyright and license details.

include config.mk

DEPS    = util/error.h util/mem.h
OBJS    = mucrop.o util/error.o util/mem.o

.PHONY: all clean

all: mucrop

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

mucrop: $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f mucrop *.o util/*.o
