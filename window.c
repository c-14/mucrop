#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include "window.h"
#include "util/error.h"

void scale_to_window(size_t *width, size_t *height, size_t w_width, size_t w_height)
{
	double scaling_factor = 0.0;

	if (*height > w_height) {
		scaling_factor = (double)w_height / (double)*height;
		*height = w_height;
		*width = *width * scaling_factor;
		printf("Image too tall, scaling factor: %g, new dimensions %zux%zu\n", scaling_factor, *width, *height);
	}
	if (*width > w_width) {
		size_t t_width = *width;
		*width = w_width;
		scaling_factor = (double)w_width / (double)t_width;
		*height = *height * scaling_factor;
		printf("Image too wide, scaling factor: %g, new dimensions %zux%zu\n", scaling_factor, *width, *height);
	}
}

struct mu_window *create_window(struct mu_error **err, size_t o_width, size_t o_height)
{
	struct mu_window *window = malloc(sizeof(struct mu_window));
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
		return window;
	}
	window->screen = xcb_setup_roots_iterator(xcb_get_setup(window->c)).data;

	window->win = xcb_generate_id(window->c);
	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	values[0] = window->screen->black_pixel;
	values[1] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_BUTTON_PRESS |
		XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_RESIZE_REDIRECT;

	printf("Original dimensions: %zux%zu\n", o_width, o_height);
	window->width = o_width;
	window->height = o_height;
	scale_to_window(&window->width, &window->height,
			window->screen->width_in_pixels, window->screen->height_in_pixels);

	xcb_create_window(window->c, window->screen->root_depth, window->win,
			window->screen->root, 0, 0, window->width, window->height, 0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT, window->screen->root_visual, mask,
			values);

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

	xcb_free_gc(w->c, w->gc);
	xcb_free_pixmap(w->c, w->pix);
	xcb_destroy_window(w->c, w->win);
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

int load_image(struct mu_error **err, struct mu_window *window, unsigned char *data, size_t len, size_t width, size_t height)
{
	xcb_image_t *img;

	img = xcb_image_create_native(window->c, width, height, XCB_IMAGE_FORMAT_Z_PIXMAP, window->screen->root_depth, data, len, data);
	xcb_image_put(window->c, window->pix, window->gc, img, 0, 0, 0);
	xcb_image_destroy(img);

	return 0;
}

int handle_expose(struct mu_error **err, struct mu_window *window, size_t width, size_t height, xcb_expose_event_t *ev)
{
	if (width < window->width)
		window->xoff = (window->width - width) / 2;
	else
		window->xoff = 0;
	if (height < window->height)
		window->yoff = (window->height - height) / 2;
	else
		window->yoff = 0;

	printf("Loading image with offset: x = %hd, y = %hd\n", window->xoff, window->yoff);
	xcb_copy_area(window->c, window->pix, window->win, window->gc, ev->x, ev->y, ev->x + window->xoff, ev->y + window->yoff, ev->width, ev->height);
	xcb_flush(window->c);

	return 0;
}

int resize_window(struct mu_error **err, struct mu_window *window, xcb_resize_request_event_t *ev)
{
	const uint32_t values[] = { ev->width, ev->height };

	if (window->width == ev->width && window->height == ev->height)
		return 0;

	window->width = ev->width;
	window->height = ev->height;
	xcb_configure_window(window->c, window->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
	xcb_flush(window->c);

	return 0;
}
