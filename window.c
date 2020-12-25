#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "window.h"
#include "util/error.h"
#include "util/mem.h"

void scale_to_window(size_t *width, size_t *height, size_t w_width, size_t w_height)
{
	double scaling_factor = 0.0;

	if (*height > w_height) {
		scaling_factor = (double)w_height / (double)*height;
		*height = w_height;
		*width = *width * scaling_factor;
	}
	if (*width > w_width) {
		size_t t_width = *width;
		*width = w_width;
		scaling_factor = (double)w_width / (double)t_width;
		*height = *height * scaling_factor;
	}
}

int init_xkb_state(struct mu_window *window)
{
	int32_t device_id = xkb_x11_get_core_keyboard_device_id(window->c);
	if (device_id == -1) {
		fputs("Failed to get device_id\n", stderr);
		return -1;
	}

	window->keymap = xkb_x11_keymap_new_from_device(window->xkb, window->c, device_id,
			XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (window->keymap == NULL) {
		fputs("Failed to create keymap\n", stderr);
		return -1;
	}

	window->keyboard_state = xkb_x11_state_new_from_device(window->keymap, window->c, device_id);
	if (window->keyboard_state == NULL) {
		fputs("Failed to create xkb state\n", stderr);
		return -1;
	}

	return 0;
}

int init_xkb(struct mu_window *window)
{
	int ret = xkb_x11_setup_xkb_extension(window->c,
			XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION,
			XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, NULL, NULL, NULL, NULL);
	if (ret != 1) {
		fputs("Failed to setup xkb extension\n", stderr);
		return -1;
	}

	window->xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (window->xkb == NULL) {
		fputs("Failed to create xkb context\n", stderr);
		return -1;
	}

	return init_xkb_state(window);
}

void deinit_xkb_state(struct mu_window *window)
{
	if (window->keyboard_state) {
		xkb_state_unref(window->keyboard_state);
		window->keyboard_state = NULL;
	}
	if (window->keymap) {
		xkb_keymap_unref(window->keymap);
		window->keymap = NULL;
	}
}

void deinit_xkb(struct mu_window *window)
{
	deinit_xkb_state(window);

	if (window->xkb) {
		xkb_context_unref(window->xkb);
		window->xkb = NULL;
	}
}

struct mu_window *create_window(struct mu_error **err, size_t o_width, size_t o_height)
{
	struct mu_window *window = mallocz(sizeof(struct mu_window));
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *xerr;
	uint32_t mask = 0;
	uint32_t values[2];
	int ret = 0;

	if (window == NULL) {
		MU_PUSH_ERRNO(err, ENOMEM);
		return NULL;
	}

	window->c = xcb_connect(NULL, NULL);
	ret = xcb_connection_has_error(window->c);
	if (ret != 0) {
		MU_PUSH_ERRSTR(err, "Could not create an XCB connection, dying");
		free(window);
		return NULL;
	}
	window->screen = xcb_setup_roots_iterator(xcb_get_setup(window->c)).data;

	ret = init_xkb(window);
	if (ret != 0) {
		MU_PUSH_ERRSTR(err, "Could not initialize XKB Extension, dying");
		deinit_xkb(window);
		free(window);
		return NULL;
	}

	// FIXME: The XKB Documentation mention using this call to listen to keyboard change events,
	// but I can't find any documentation on what any of the options mean so...
	/* cookie = xcb_xkb_select_events_aux(window->c, j */

	window->win = xcb_generate_id(window->c);
	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	values[0] = window->screen->black_pixel;
	values[1] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_BUTTON_PRESS |
		XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_1_MOTION |
		XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

	window->width = o_width;
	window->height = o_height;
	scale_to_window(&window->width, &window->height,
			window->screen->width_in_pixels, window->screen->height_in_pixels);

	cookie = xcb_create_window(window->c, window->screen->root_depth, window->win,
			window->screen->root, 0, 0, window->width, window->height, 0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT, window->screen->root_visual, mask,
			values);
	xerr = xcb_request_check(window->c, cookie);
	if (xerr) {
		MU_PUSH_ERRF(err, "Could not create window: XCB error %d", xerr->error_code);
		free(err);
		free(window);
		return NULL;
	}

	return window;
}

int update_geometry(struct mu_error **err, struct mu_window *window)
{
	xcb_get_geometry_reply_t *geom;

	geom = xcb_get_geometry_reply(window->c, xcb_get_geometry(window->c, window->win), NULL);
	if (!geom)
		MU_RET_ERRSTR(err, "Could not get geometry of window, dying");

	printf("window is %hux%hu\n", geom->width, geom->height);
	window->width = geom->width;
	window->height = geom->height;
	free(geom);

	return 0;
}

void map_window(struct mu_window *window)
{
	xcb_map_window(window->c, window->win);
	xcb_flush(window->c);
}

void destroy_window(struct mu_window **window)
{
	struct mu_window *w = *window;

	if (w->gc)
		xcb_free_gc(w->c, w->gc);
	if (w->pix)
		xcb_free_pixmap(w->c, w->pix);
	if (w->win)
		xcb_destroy_window(w->c, w->win);
	deinit_xkb(w);
	xcb_disconnect(w->c);

	free(w);
	window = NULL;
}

int create_gc(struct mu_error **err, struct mu_window *window)
{
	uint32_t mask = 0;
	uint32_t values[2];

	mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
	values[0] = window->screen->black_pixel;
	values[1] = 0;

	window->gc = xcb_generate_id(window->c);
	xcb_create_gc(window->c, window->gc, window->pix, mask, values);
	/* xcb_create_gc(window->c, window->gc, window->pix, 0, NULL); */

	return 0;
}

int create_pixmap(struct mu_error **err, struct mu_window *window, size_t width, size_t height)
{
	window->pix = xcb_generate_id(window->c);

	xcb_create_pixmap(window->c, window->screen->root_depth, window->pix, window->win, width, height);

	return 0;
}

static int draw_image(struct mu_error **err, struct mu_window *window, uint16_t loc[4], size_t im_width, size_t im_height)
{
	uint16_t src_x = loc[0], dst_x = src_x, src_y = loc[1], dst_y = src_y, c_width = loc[2], c_height = loc[3];

	if (src_x < window->xoff && c_width > window->xoff) {
		dst_x += window->xoff;
		c_width -= window->xoff;
	} else if (src_x < window->xoff || src_x > im_width + window->xoff) {
		return 0;
	} else {
		src_x -= window->xoff;
	}

	if (src_y < window->yoff && c_height > window->yoff) {
		dst_y += window->yoff;
		c_height -= window->yoff;
	} else if (src_y < window->yoff || src_y > im_height + window->yoff) {
		return 0;
	} else {
		src_y -= window->yoff;
	}

	xcb_copy_area(window->c, window->pix, window->win, window->gc, src_x, src_y, dst_x, dst_y, c_width, c_height);
	xcb_flush(window->c);

	return 0;
}

static int reload_with_offset(struct mu_error **err, struct mu_window *window, size_t width, size_t height)
{
	uint16_t loc[4] = { 0, 0, window->width, window->height };

	if (width < window->width)
		window->xoff = (window->width - width) / 2;
	else
		window->xoff = 0;
	if (height < window->height)
		window->yoff = (window->height - height) / 2;
	else
		window->yoff = 0;

	xcb_clear_area(window->c, 0, window->win, 0, 0, window->width, window->height);

	return draw_image(err, window, loc, width, height);
}

int load_image(struct mu_error **err, struct mu_window *window, unsigned char *data, size_t len, size_t width, size_t height)
{
	xcb_pixmap_t old_pix = window->pix;
	xcb_image_t *img;

	// Recreate pixmap here
	// Create some sort of backing pixmap and then swap them "atomically"?
	create_pixmap(err, window, width, height);

	img = xcb_image_create_native(window->c, width, height, XCB_IMAGE_FORMAT_Z_PIXMAP, window->screen->root_depth, data, len, data);
	xcb_image_put(window->c, window->pix, window->gc, img, 0, 0, 0);
	xcb_image_destroy(img);

	reload_with_offset(err, window, width, height);
	xcb_free_pixmap(window->c, old_pix);

	return 0;
}

int handle_expose(struct mu_error **err, struct mu_window *window, size_t width, size_t height, xcb_expose_event_t *ev)
{
	uint16_t loc[4] = { ev->x, ev->y, ev->width, ev->height };

	return draw_image(err, window, loc, width, height);
}

int resize_window(struct mu_error **err, struct mu_window *window, size_t sizes[4], xcb_configure_notify_event_t *ev)
{
	size_t width = sizes[0], height = sizes[1], o_width = sizes[2], o_height = sizes[3];
	int ret;

	if (window->width == ev->width && window->height == ev->height) {
		return 0;
	} else if (window->width == ev->width || window->height == ev->height) {
		window->width = ev->width;
		window->height = ev->height;
		return reload_with_offset(err, window, width, height);
	} else if (width == o_width && height == o_height && width <= ev->width && height <= ev->height) {
		// Unscaled Case
		window->width = ev->width;
		window->height = ev->height;
		return reload_with_offset(err, window, width, height);
	} else if ((ev->width <= window->width && width <= ev->width) && (ev->height <= window->height && height <= ev->height)) {
		// No need to rescale
		window->width = ev->width;
		window->height = ev->height;
		return reload_with_offset(err, window, width, height);
	}

	window->width = ev->width;
	window->height = ev->height;
	ret = reload_with_offset(err, window, width, height);

	return ret == 0 ? 1 : ret;
}

int draw_bbox(struct mu_error **err, struct mu_window *window, Point *p1, Point *p2)
{
	xcb_rectangle_t rect = { 0, 0, abs(p2->x - p1->x), abs(p2->y - p1->y) };
	uint16_t loc[4] = { 0, 0, window->width, window->height };

	rect.x = p2->x > p1->x ? p1->x : p2->x;
	rect.y = p2->y > p1->y ? p1->y : p2->y;
	loc[0] = 0;
	loc[1] = 0;

	// TODO: maybe optimize this by storing the last box size/pos and only clearing that
	draw_image(err, window, loc, window->width, window->height);

	xcb_poly_rectangle(window->c, window->win, window->gc, 1, &rect);
	xcb_flush(window->c);

	return 0;
}

int clear_bbox(struct mu_error **err, struct mu_window *window, Point *p1, Point *p2)
{
	uint16_t loc[4] = { 0, 0, window->width, window->height };
	(void)p1;
	(void)p2;

	draw_image(err, window, loc, window->width, window->height);
	xcb_flush(window->c);

	return 0;
}
