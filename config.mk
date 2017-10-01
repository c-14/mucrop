# paths
PREFIX = /usr/local

# libevent
MAGICK_CFLAGS  = `pkg-config --cflags MagickWand`
MAGICK_LDFLAGS = `pkg-config --libs MagickWand`

# xcb
XCB_CFLAGS = `pkg-config --cflags xcb xcb-image`
XCB_LDFLAGS = `pkg-config --libs xcb xcb-image`

# custom flags
EXTRA_CFLAGS  = -std=c99 -DDEBUG -fsanitize=undefined -fsanitize=address
EXTRA_LDFLAGS = -lunwind

# flags
WFLAGS  = -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS  = $(WFLAGS) $(MAGICK_CFLAGS) $(XCB_CFLAGS) -pipe -fstack-protector -g -ggdb $(EXTRA_CFLAGS)
LDFLAGS = $(MAGICK_LDFLAGS) $(XCB_LDFLAGS) $(EXTRA_LDFLAGS)

# compiler and linker
CC = clang
