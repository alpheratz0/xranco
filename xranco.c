/*
	Copyright (C) 2022 <alpheratz99@protonmail.com>

	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU General Public License version 2 as published by the
	Free Software Foundation.

	This program is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place, Suite 330, Boston, MA 02111-1307 USA

	 _____________________________
	( do you want a random color? )
	 -----------------------------
	 o
	  o
		 __
		/  \
		|  |
		@  @
		|  |
		|| |/
		|| ||
		|\_/|
		\___/

*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#define MAX_COLORS           (9)
#define HEX_STR_LEN          (sizeof("#000000") - 1)

struct color {
	GC bg, text;
	unsigned long rgb;
	char hex[64];
};

struct palette {
	int count;
	struct color colors[MAX_COLORS];
};

static Display *display;
static Window window, root;
static unsigned int width, height;
static struct palette palette;
static XFontStruct *font;
static Atom wm_delete_window, wm_window_opacity;

static void
die(const char *err)
{
	fprintf(stderr, "xranco: %s\n", err);
	exit(1);
}

static void
dief(const char *fmt, ...)
{
	va_list args;

	fputs("xranco: ", stderr);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	exit(1);
}

static void
create_window(void)
{
	if (NULL == (display = XOpenDisplay(NULL))) {
		die("can't open display");
	}

	if (NULL == (font = XLoadQueryFont(display, "fixed"))) {
		die("can't open font");
	}

	width = height = 600;
	root = DefaultRootWindow(display);
	window = XCreateSimpleWindow(display, root, 0, 0, width, height, 0, 0, 0);
	wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
	wm_window_opacity = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);

	XStoreName(display, window, "xranco");
	XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
	XSetWMProtocols(display, window, &wm_delete_window, 1);

	XChangeProperty(
		display, window, wm_window_opacity, XA_CARDINAL, 32,
		PropModeReplace, (const unsigned char[]) { 0xff, 0xff, 0xff, 0xff }, 1
	);

	XMapWindow(display, window);
}

static void
destroy_window(void)
{
	XUnloadFont(display, font->fid);

	while (--palette.count >= 0) {
		XFreeGC(display, palette.colors[palette.count].bg);
		XFreeGC(display, palette.colors[palette.count].text);
	}

	XCloseDisplay(display);
}

static inline int
get_color_brightness(unsigned long color)
{
	return (int)(
		0.2126 * ((color >> 16) & 0xff) +
		0.7152 * ((color >> 8) & 0xff) +
		0.0722 * (color & 0xff)
	);
}

static void
set_color(int idx, unsigned long color)
{
	struct color *c;

	c = &palette.colors[idx];
	c->rgb = color;

	XSetForeground(display, c->bg, color);

	XSetForeground(
		display, c->text,
		get_color_brightness(color) < 50 ?
			0xffffff :
			0x000000
	);

	snprintf(c->hex, sizeof(c->hex) - 1, "#%06lx", color);
}

static void
add_color(unsigned long color)
{
	struct color *c;

	c = &palette.colors[palette.count++];
	c->bg = XCreateGC(display, window, 0, NULL);
	c->text = XCreateGC(display, window, 0, NULL);

	XSetFont(display, c->text, font->fid);

	set_color(palette.count - 1, color);
}

static void
create_palette(int count)
{
	while (--count >= 0) {
		add_color(rand() & 0xffffff);
	}
}

static void
load_palette(const char *path)
{
	FILE *fp;
	unsigned long color;

	if (NULL == (fp = fopen(path, "r"))) {
		dief("failed to open file %s: %s", path, strerror(errno));
	}

	while (palette.count < MAX_COLORS && fscanf(fp, "#%06lx\n", &color) == 1) {
		add_color(color & 0xffffff);
	}

	if (palette.count == 0) {
		die("invalid file format");
	}

	fclose(fp);
}

static void
h_keypress(XKeyEvent *ev)
{
	KeySym key;

	key = XLookupKeysym(ev, 0);

	if (key >= XK_1 && key < (XK_1 + (unsigned)(palette.count))) {
		set_color(key - XK_1, rand() & 0xffffff);
		XClearArea(display, window, 0, 0, 0, 0, True);
	}
}

static void
h_expose(XExposeEvent *ev)
{
	struct color *c;
	int i, cw, tx, ty;

	if (ev->count == 0) {
		cw = width / palette.count;
		tx = (cw - XTextWidth(font, "0", 1) * HEX_STR_LEN) / 2;
		ty = (height + font->ascent) / 2;

		for (i = 0; i < palette.count; ++i, tx += cw) {
			c = &palette.colors[i];

			XFillRectangle(display, window, c->bg, cw * i, 0, cw, height);
			XDrawString(display, window, c->text, tx, ty, c->hex, HEX_STR_LEN);
		}
	}
}

static void
h_configure(XConfigureEvent *ev)
{
	width = ev->width;
	height = ev->height;
}

static void
h_client_message(XClientMessageEvent *ev)
{
	int i;

	if ((Atom)(ev->data.l[0]) == wm_delete_window) {
		for (i = 0; i < palette.count; ++i) {
			printf("%s\n", palette.colors[i].hex);
		}

		destroy_window();
		exit(0);
	}
}

static int
match_numeric_opt(const char *in, int from, int to)
{
	return in[0] == '-' &&
		   in[1] >= ('0' + from) &&
		   in[1] <= ('0' + to) &&
		   in[2] == '\0';
}

static void
usage(void)
{
	puts("usage: xranco [-hlv123456789]");
	exit(0);
}

static void
version(void)
{
	puts("xranco version "VERSION);
	exit(0);
}

int
main(int argc, char **argv)
{
	XEvent ev;
	int ncolors = 1;
	const char *loadpath = NULL;

	if (++argv, --argc > 0) {
		if (!strcmp(*argv, "-h")) usage();
		else if (!strcmp(*argv, "-v")) version();
		else if (!strcmp(*argv, "-l")) --argc, loadpath = *++argv;
		else if (match_numeric_opt(*argv, 1, MAX_COLORS)) ncolors = atoi(&(*argv)[1]);
		else if (**argv == '-') dief("invalid option %s", *argv);
		else dief("unexpected argument: %s", *argv);
	}

	srand(getpid());
	create_window();

	if (NULL == loadpath) create_palette(ncolors);
	else load_palette(loadpath);

	while (1) {
		XNextEvent(display, &ev);

		switch (ev.type) {
			case ClientMessage:
				h_client_message(&ev.xclient);
				break;
			case Expose:
				h_expose(&ev.xexpose);
				break;
			case ConfigureNotify:
				h_configure(&ev.xconfigure);
				break;
			case KeyPress:
				h_keypress(&ev.xkey);
				break;
		}
	}

	return 0;
}
