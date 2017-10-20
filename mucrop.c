#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include <MagickWand/MagickWand.h>

#include "window.h"
#include "util/error.h"

struct mucrop_core {
	MagickWand *wand;
	struct mu_window *window;
	struct mu_error *errlist;

	unsigned char *image;
	size_t o_width;
	size_t o_height;
	size_t width;
	size_t height;
	size_t length;

	bool quit;
	bool wait;
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

void handle_keypress(struct mucrop_core *core, xcb_key_press_event_t *key)
{
	switch (key->detail) {
		case 0x35: // q
			core->quit = true;
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
	const char *filename = argv[1];
	bool first = true;
	int ret = 0;

	MagickWandGenesis();
	core.wand = NewMagickWand();

	core.errlist = create_errlist(3);
	if (core.errlist == NULL) {
		perror("malloc");
		ret = EX_OSERR;
		goto fail;
	}

	ret = ping_image(&core, filename);
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

	read_image(&core, filename);
	load_image(&core.errlist, core.window, core.image, core.length, core.width, core.height);

	map_window(core.window);

	core.wait = true;
	while (!core.quit) {
		if (core.wait) {
			ev = xcb_wait_for_event(core.window->c);
		} else {
			ev = xcb_poll_for_event(core.window->c);
		}
		if (!ev) {
			continue;
		}
		switch (ev->response_type & ~0x80) {
			case XCB_KEY_PRESS:
				handle_keypress(&core, (xcb_key_press_event_t *)ev);
				break;
			case XCB_BUTTON_PRESS:
				break;
			case XCB_BUTTON_RELEASE:
				break;
			case XCB_EXPOSE:
				if (first) {
					first = false;
					update_geometry(&core.errlist, core.window);
					read_image(&core, filename);
					load_image(&core.errlist, core.window, core.image, core.length, core.width, core.height);
				}
				handle_expose(&core.errlist, core.window, core.width, core.height, (xcb_expose_event_t *)ev);
				break;
			case XCB_RESIZE_REQUEST:
				resize_window(&core.errlist, core.window, (xcb_resize_request_event_t *)ev);
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
