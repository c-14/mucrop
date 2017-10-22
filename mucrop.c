#define _POSIX_C_SOURCE 199309L
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include <MagickWand/MagickWand.h>

#include "window.h"
#include "util/error.h"
#include "util/time.h"

struct mucrop_core {
	MagickWand *wand;
	struct mu_window *window;
	struct mu_error *errlist;

	unsigned char *image;
	size_t length;

	size_t o_width;
	size_t o_height;
	size_t width;
	size_t height;

	int16_t start_x;
	int16_t start_y;
	int16_t stop_x;
	int16_t stop_y;

	bool quit;
	bool wait;
	bool crop;
};

#define RaiseWandException(wand, errlist) \
{ \
	char *description; \
	ExceptionType severity; \
\
	description = MagickGetException(wand, &severity); \
	MU_PUSH_ERRSTR(errlist, description); \
	description = (char *) MagickRelinquishMemory(description); \
}


int ping_image(struct mucrop_core *core, const char *filename)
{
	MagickBooleanType status;

	status = MagickPingImage(core->wand, filename);
	if (status == MagickFalse) {
		RaiseWandException(core->wand, &core->errlist);
		return -1;
	}

	// Get Output Resolution/Preferred Output Format
	// Convert Copy of Image to Format (RGBA/YUV/Whatever)
	// GetImageBlob and destroy copy of Image
	/* MagickResetIterator(core->wand); */
	core->o_width  = MagickGetImageWidth(core->wand);
	core->o_height = MagickGetImageHeight(core->wand);

	return 0;
}

int read_image(struct mucrop_core *core, const char *filename)
{
	MagickBooleanType status;

	core->width = core->o_width;
	core->height = core->o_height;

	status = MagickReadImage(core->wand, filename);
	if (status == MagickFalse) {
		RaiseWandException(core->wand, &core->errlist);
		return -1;
	}

	scale_to_window(&core->width, &core->height, core->window->width, core->window->height);
	MagickResizeImage(core->wand, core->width, core->height, LanczosFilter);

	MagickSetImageFormat(core->wand, "bgra");
	core->image = MagickGetImageBlob(core->wand, &core->length);

	ClearMagickWand(core->wand);

	return 0;
}

int start_crop(struct mucrop_core *core, xcb_button_press_event_t *ev)
{
	if (!(ev->detail & XCB_BUTTON_INDEX_1))
		return 0;

	core->start_x = ev->event_x;
	core->start_y = ev->event_y;

	return 1;
}

int stop_crop(struct mucrop_core *core, const char *src_filename, const char *dst_filename, xcb_button_release_event_t *ev)
{
	MagickBooleanType status;
	ssize_t x, y;
	size_t width, height;
	double scale;

	if (!(ev->detail & XCB_BUTTON_INDEX_1))
		return 0;

	core->stop_x = ev->event_x;
	core->stop_y = ev->event_y;

	if (core->start_x > core->stop_x) {
		int16_t tmp = core->start_x;
		core->start_x = core->stop_x;
		core->stop_x = tmp;
	}
	if (core->start_y > core->stop_y) {
		int16_t tmp = core->start_y;
		core->start_y = core->stop_y;
		core->stop_y = tmp;
	}

	if (core->start_x < core->window->xoff) {
		x = 0;
	} else {
		x = core->start_x - core->window->xoff;
	}
	if (core->start_y < core->window->yoff) {
		y = 0;
	} else {
		y = core->start_y - core->window->yoff;
	}

	if ((uint16_t)core->stop_x > core->window->xoff + core->width) {
		width = core->width - x;
	} else {
		width = core->stop_x - (x + core->window->xoff);
	}
	if ((uint16_t)core->stop_y > core->window->yoff + core->height) {
		height = core->height - y;
	} else {
		height = core->stop_y - (y + core->window->yoff);
	}

	if (width <= 0 || height <= 0)
		return 0;

	scale   = (double)core->o_width / (double)core->width;
	x      *= scale;
	width  *= scale;
	scale   = (double)core->o_height / (double)core->height;
	y      *= scale;
	height *= scale;

	status = MagickReadImage(core->wand, src_filename);
	if (status == MagickFalse) {
		RaiseWandException(core->wand, &core->errlist);
		return -1;
	}

	MagickCropImage(core->wand, width, height, x, y);
	MagickWriteImage(core->wand, dst_filename);

	ClearMagickWand(core->wand);

	return 1;
}

void handle_keypress(struct mucrop_core *core, xcb_key_press_event_t *key)
{
	switch (key->detail) {
		case 0x35: // q
			core->quit = true;
			break;
		case 0x42: // ESC
			core->crop = false;
			break;
		default:
			printf("%x\n", key->detail);
			break;
	}
}

int main(int argc, const char *argv[])
{
	struct mucrop_core core = {};
	xcb_generic_event_t *ev;
	const char *src_filename = argv[1];
	const char *dst_filename = argv[2];
	struct timespec tp;
	bool resize = false;
	int ret = 0;

	MagickWandGenesis();
	core.wand = NewMagickWand();

	core.errlist = create_errlist(3);
	if (core.errlist == NULL) {
		perror("malloc");
		ret = EX_OSERR;
		goto fail;
	}

	ret = ping_image(&core, src_filename);
	if (ret != 0) {
		goto fail;
	}

	core.window = create_window(&core.errlist, core.o_width, core.o_height);
	if (core.window == NULL) {
		ret = EX_OSERR;
		goto fail;
	}
	core.width = core.window->width;
	core.height = core.window->height;

	create_pixmap(&core.errlist, core.window, core.width, core.height);
	create_gc(&core.errlist, core.window);

	ret = read_image(&core, src_filename);
	if (ret != 0)
		goto fail;

	load_image(&core.errlist, core.window, core.image, core.length, core.width, core.height);

	map_window(core.window);

	core.wait = true;
	while (!core.quit) {
		size_t sizes[4] = { core.width, core.height, core.o_width, core.o_height };
		if (core.wait) {
			ev = xcb_wait_for_event(core.window->c);
		} else {
			ev = xcb_poll_for_event(core.window->c);
		}
		if (!ev) {
			struct timespec now;
			if (resize) {
				clock_gettime(CLOCK_MONOTONIC, &now);
				if (difftimespec(&now, &tp) > 500) {
					if (read_image(&core, src_filename) != 0)
						goto fail;
					load_image(&core.errlist, core.window, core.image, core.length, core.width, core.height);
					core.wait = true;
					resize = false;
				}
			}
			continue;
		}
		switch (ev->response_type & ~0x80) {
			case XCB_KEY_PRESS:
				handle_keypress(&core, (xcb_key_press_event_t *)ev);
				break;
			case XCB_BUTTON_PRESS:
				if (start_crop(&core, (xcb_button_press_event_t *)ev) > 0)
					core.crop = true;
				break;
			case XCB_BUTTON_RELEASE:
				if (core.crop) {
					ret = stop_crop(&core, src_filename, dst_filename, (xcb_button_release_event_t *)ev);
					if (ret == 0)
						core.crop = false;
					else if (ret < 0)
						goto fail;
					else if (ret > 0)
						core.quit = true;
				}
				break;
			case XCB_EXPOSE:
				handle_expose(&core.errlist, core.window, core.width, core.height, (xcb_expose_event_t *)ev);
				break;
			case XCB_CONFIGURE_NOTIFY:
				if (resize_window(&core.errlist, core.window, sizes, (xcb_configure_notify_event_t *)ev)) {
					core.wait = false;
					resize = true;
					clock_gettime(CLOCK_MONOTONIC, &tp);
				}
				break;
			default:
				break;
		}
		free(ev);
	}

fail:
	ret |= process_errors(core.errlist);
	free_errlist(&core.errlist);
	destroy_window(&core.window);

	/* MagickRelinquishMemory(core.image); */
	ClearMagickWand(core.wand);
	core.wand = DestroyMagickWand(core.wand);
	MagickWandTerminus();

	if (ret < 0) {
		return EX_SOFTWARE;
	}
	return ret;
}
