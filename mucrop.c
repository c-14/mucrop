#define _POSIX_C_SOURCE 199309L
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

#include <MagickWand/MagickWand.h>

#include "window.h"
#include "util/error.h"
#include "util/time.h"

enum mucrop_states {
	MU_COMP = (1 << 1),
	MU_CROP = (1 << 2),
	MU_QUIT = (1 << 3),
	MU_RESI = (1 << 4),
	MU_SAVE = (1 << 5),
	MU_UNDO = (1 << 6),
	MU_WAIT = (1 << 7)
};

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

	Point bound_origin;

	Point crop_origin;
	size_t crop_width;
	size_t crop_height;

	uint16_t state_flags;
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
	if ((core->width != core->o_width) || (core->height != core->o_height))
		MagickResizeImage(core->wand, core->width, core->height, LanczosFilter);

	MagickSetImageFormat(core->wand, "bgra");
	core->image = MagickGetImageBlob(core->wand, &core->length);

	ClearMagickWand(core->wand);

	return 0;
}

int reload_image(struct mucrop_core *core, const char *filename)
{
	MagickBooleanType status;
	size_t width, height;

	status = MagickReadImage(core->wand, filename);
	if (status == MagickFalse) {
		RaiseWandException(core->wand, &core->errlist);
		return -1;
	}

	if (core->state_flags & MU_CROP) {
		width  = core->width  = core->crop_width;
		height = core->height = core->crop_height;
		MagickCropImage(core->wand, core->width, core->height, core->crop_origin.x, core->crop_origin.y);
	} else {
		width  = core->width  = core->o_width;
		height = core->height = core->o_height;
	}

	scale_to_window(&core->width, &core->height, core->window->width, core->window->height);
	if ((core->width != width) || (core->height != height))
		MagickResizeImage(core->wand, core->width, core->height, LanczosFilter);

	MagickSetImageFormat(core->wand, "bgra");
	core->image = MagickGetImageBlob(core->wand, &core->length);

	ClearMagickWand(core->wand);

	return load_image(&core->errlist, core->window, core->image, core->length, core->width, core->height);
;
}

int bound_init(Point *bound_origin, xcb_button_press_event_t *ev)
{
	if (!(ev->detail & XCB_BUTTON_INDEX_1))
		return 0;

	bound_origin->x = ev->event_x;
	bound_origin->y = ev->event_y;

	return 1;
}

int bound_compute(struct mucrop_core *core, Point *bound_origin, xcb_button_release_event_t *ev)
{
	size_t x, y, width, height;
	double scale_x, scale_y;
	Point bound_end;

	if (!(ev->detail & XCB_BUTTON_INDEX_1))
		return 0;

	bound_end.x = ev->event_x;
	bound_end.y = ev->event_y;

	if (bound_origin->x > bound_end.x) {
		int16_t tmp = bound_origin->x;
		bound_origin->x = bound_end.x;
		bound_end.x = tmp;
	}
	if (bound_origin->y > bound_end.y) {
		int16_t tmp = bound_origin->y;
		bound_origin->y = bound_end.y;
		bound_end.y = tmp;
	}

	if (bound_origin->x < core->window->xoff) {
		x = 0;
	} else {
		x = bound_origin->x - core->window->xoff;
	}
	if (bound_origin->y < core->window->yoff) {
		y = 0;
	} else {
		y = bound_origin->y - core->window->yoff;
	}

	if ((uint16_t)bound_end.x > core->window->xoff + core->width) {
		width = core->width - x;
	} else {
		width = bound_end.x - (x + core->window->xoff);
	}
	if ((uint16_t)bound_end.y > core->window->yoff + core->height) {
		height = core->height - y;
	} else {
		height = bound_end.y - (y + core->window->yoff);
	}

	if (width <= 0 || height <= 0)
		return 0;

	if (core->state_flags & MU_CROP) {
		scale_x = (double)core->crop_width / (double)core->width;
		scale_y = (double)core->crop_height / (double)core->height;
	} else {
		scale_x = (double)core->o_width / (double)core->width;
		scale_y = (double)core->o_height / (double)core->height;
	}

	x      *= scale_x;
	width  *= scale_x;
	y      *= scale_y;
	height *= scale_y;

	if (core->state_flags & MU_CROP) {
		x += core->crop_origin.x;
		y += core->crop_origin.y;
	}

	core->crop_origin.x = x;
	core->crop_origin.y = y;
	core->crop_width = width;
	core->crop_height = height;

	return 1;
}

int crop_image(struct mucrop_core *core, const char *src_filename, const char *dst_filename)
{
	MagickBooleanType status;

	status = MagickReadImage(core->wand, src_filename);
	if (status == MagickFalse) {
		RaiseWandException(core->wand, &core->errlist);
		return -1;
	}

	MagickCropImage(core->wand, core->crop_width, core->crop_height, core->crop_origin.x, core->crop_origin.y);
	MagickWriteImage(core->wand, dst_filename);

	ClearMagickWand(core->wand);

	return 1;
}

int handle_mouse_motion(struct mucrop_core *core, Point *bound_origin, xcb_motion_notify_event_t *ev)
{
	Point cur_pos = { ev->event_x, ev->event_y };

	if (core->state_flags & MU_COMP) {
		return draw_bbox(&core->errlist, core->window, bound_origin, &cur_pos);
	}
	return 0;
}

int handle_buttonpress(struct mucrop_core *core, xcb_button_press_event_t *button)
{
	switch (button->detail) {
		case 0x01:
			if (bound_init(&core->bound_origin, button) > 0)
				core->state_flags |= MU_COMP;
			break;
		case 0x03:
			core->state_flags &= ~MU_COMP;
			return clear_bbox(&core->errlist, core->window, NULL, NULL);
		default:
			break;
	}

	return 0;
}

int handle_keypress(struct mucrop_core *core, xcb_key_press_event_t *key)
{
	xkb_keysym_t symbol = xkb_state_key_get_one_sym(core->window->keyboard_state, key->detail);

	switch (symbol) {
		case XKB_KEY_q: // q
			core->state_flags |= MU_QUIT;
			break;
		case XKB_KEY_w: // w
			core->state_flags |= MU_QUIT | MU_SAVE;
			break;
		case XKB_KEY_Escape: // ESC
			core->state_flags &= ~MU_COMP;
			return clear_bbox(&core->errlist, core->window, NULL, NULL);
		default:
			break;
	}

	return 0;
}

static void handle_x11_error(struct mucrop_core *core)
{
	MU_PUSH_ERRSTR(&core->errlist, "Received X11 error, dying");
	core->state_flags |= MU_QUIT;
}

static void usage(bool err)
{
	fputs("usage: mucrop <src_filename> [dst_filename]\n", err ? stderr : stdout);
}

int main(int argc, const char *argv[])
{
	struct mucrop_core core = {};
	xcb_generic_event_t *ev;
	const char *src_filename = argv[1];
	const char *dst_filename;
	struct timespec tp;
	int ret = 0;

	switch (argc) {
		case 2:
			dst_filename = src_filename;
			break;
		case 3:
			dst_filename = argv[2];
			break;
		default:
			usage(true);
			return EX_USAGE;
	}

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

	core.state_flags |= MU_WAIT;
	while (!(core.state_flags & MU_QUIT)) {
		size_t sizes[4] = { core.width, core.height, core.o_width, core.o_height };
		if (core.state_flags & MU_WAIT) {
			ev = xcb_wait_for_event(core.window->c);
		} else {
			ev = xcb_poll_for_event(core.window->c);
		}
		if (!ev) {
			struct timespec now;
			if (core.state_flags & MU_RESI) {
				clock_gettime(CLOCK_MONOTONIC, &now);
				if (difftimespec(&now, &tp) > 500) {
					if (reload_image(&core, src_filename) != 0)
						goto fail;
					core.state_flags |= MU_WAIT;
					core.state_flags &= ~MU_RESI;
				}
			}
			continue;
		}
		switch (ev->response_type & ~0x80) {
			case XCB_KEY_PRESS:
				handle_keypress(&core, (xcb_key_press_event_t *)ev);
				break;
			case XCB_BUTTON_PRESS:
				handle_buttonpress(&core, (xcb_button_press_event_t *)ev);
				break;
			case XCB_BUTTON_RELEASE:
				if (core.state_flags & MU_COMP) {
					core.state_flags &= ~MU_COMP;
					ret = bound_compute(&core, &core.bound_origin, (xcb_button_release_event_t *)ev);
					if (ret > 0) {
						core.state_flags |= MU_CROP;
						if (reload_image(&core, src_filename) != 0)
							goto fail;
					} else if (ret < 0)
						goto fail;
				}
				break;
			case XCB_MOTION_NOTIFY:
				handle_mouse_motion(&core, &core.bound_origin, (xcb_motion_notify_event_t *)ev);
				break;
			case XCB_EXPOSE:
				handle_expose(&core.errlist, core.window, core.width, core.height, (xcb_expose_event_t *)ev);
				break;
			case XCB_CONFIGURE_NOTIFY:
				if (resize_window(&core.errlist, core.window, sizes, (xcb_configure_notify_event_t *)ev)) {
					core.state_flags &= ~MU_WAIT;
					core.state_flags |= MU_RESI;
					clock_gettime(CLOCK_MONOTONIC, &tp);
				}
				break;
				// According to xcb-requests(3), response_type is 0 in error case
				// Since it never mentions the type for the event, throw a generic error instead.
			case 0:
				handle_x11_error(&core);
				break;
			default:
				break;
		}
		free(ev);
	}

	if (core.state_flags & MU_SAVE) {
		crop_image(&core, src_filename, dst_filename);
	}

fail:
	ret |= process_errors(core.errlist);
	free_errlist(&core.errlist);
	if (core.window) {
		destroy_window(&core.window);
	}

	/* MagickRelinquishMemory(core.image); */
	if (core.wand) {
		ClearMagickWand(core.wand);
		core.wand = DestroyMagickWand(core.wand);
		MagickWandTerminus();
	}

	if (ret < 0) {
		return EX_SOFTWARE;
	}
	return ret;
}
