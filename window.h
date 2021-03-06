#ifndef MU_WINDOW_H
#define MU_WINDOW_H

#include <xcb/xcb.h>

#include "util/error.h"

typedef struct Point {
	int16_t x;
	int16_t y;
} Point;

struct mu_window {
	xcb_connection_t *c;
	xcb_screen_t     *screen;
	xcb_drawable_t   win;
	xcb_pixmap_t     pix;
	xcb_gcontext_t   gc;

	struct xkb_context *xkb;
	struct xkb_keymap *keymap;
	struct xkb_state *keyboard_state;

	size_t width;
	size_t height;
	int16_t xoff;
	int16_t yoff;
};

extern struct mu_window *create_window(struct mu_error **err, size_t width, size_t height);
extern void destroy_window(struct mu_window **window);

extern void map_window(struct mu_window *window);
extern void scale_to_window(size_t *width, size_t *height, size_t w_width, size_t w_height);
extern int update_geometry(struct mu_error **err, struct mu_window *window);

extern int create_pixmap(struct mu_error **err, struct mu_window *window, size_t width, size_t height);
extern int create_gc(struct mu_error **err, struct mu_window *window);

extern int draw_bbox(struct mu_error **err, struct mu_window *window, Point *p1, Point *p2);
extern int clear_bbox(struct mu_error **err, struct mu_window *window, Point *p1, Point *p2);

extern int load_image(struct mu_error **err, struct mu_window *window, unsigned char *data, size_t len, size_t width, size_t height);
extern int handle_expose(struct mu_error **err, struct mu_window *window, size_t width, size_t height, xcb_expose_event_t *ev);
extern int resize_window(struct mu_error **err, struct mu_window *window, size_t sizes[4], xcb_configure_notify_event_t *ev);

#endif
