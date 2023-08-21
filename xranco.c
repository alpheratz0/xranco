/*
	Copyright (C) 2022-2023 <alpheratz99@protonmail.com>

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License version 2 as published by
	the Free Software Foundation.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc., 59
	Temple Place, Suite 330, Boston, MA 02111-1307 USA

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

#define MAX_COLORS (9)
#define HEX_STR_LEN (sizeof("#000000") - 1)
#define LEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define XATOM(name) (XInternAtom(display, name, False))

/* x11 atoms */
#define WM_DELETE_WINDOW               XATOM("WM_DELETE_WINDOW")
#define _NET_WM_WINDOW_OPACITY         XATOM("_NET_WM_WINDOW_OPACITY")

struct Color {
	GC bg, text;
	unsigned long rgb;
	char hex[HEX_STR_LEN+1];
};

struct Point {
	int x, y;
};

struct Rectangle {
	int x, y;
	int width, height;
};

struct Palette {
	int count;
	struct Color colors[MAX_COLORS];
};

static Display *display;
static Window window, root;
static unsigned int width, height;
static struct Palette palette;
static XFontStruct *font;

static void
die(const char *fmt, ...)
{
	va_list args;

	fputs("xranco: ", stderr);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	exit(1);
}

static const char *
enotnull(const char *str, const char *name)
{
	if (NULL == str)
		die("%s cannot be null", name);
	return str;
}

static void
create_window(void)
{
	Atom protocols[1];

	if (NULL == (display = XOpenDisplay(NULL)))
		die("can't open display");

	if (NULL == (font = XLoadQueryFont(display, "fixed")))
		die("can't open font");

	width = height = 600;
	root = DefaultRootWindow(display);
	window = XCreateSimpleWindow(display, root, 0, 0, width, height, 0, 0, 0);
	protocols[0] = WM_DELETE_WINDOW;

	XStoreName(display, window, "xranco");
	XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
	XSetWMProtocols(display, window, protocols, LEN(protocols));

	XChangeProperty(
		display, window, _NET_WM_WINDOW_OPACITY, XA_CARDINAL, 32,
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
		0.7152 * ((color >>  8) & 0xff) +
		0.0722 * ((color >>  0) & 0xff)
	);
}

static void
set_color(int idx, unsigned long color)
{
	struct Color *c;

	c = &palette.colors[idx];
	c->rgb = color;

	XSetForeground(display, c->bg, color);

	XSetForeground(
		display, c->text,
		get_color_brightness(color) < 50 ?
			0xffffff :
			0x000000
	);

	snprintf(c->hex, LEN(c->hex), "#%06lx", color);
}

static void
add_color(unsigned long color)
{
	struct Color *c;

	c = &palette.colors[palette.count++];
	c->bg = XCreateGC(display, window, 0, NULL);
	c->text = XCreateGC(display, window, 0, NULL);

	XSetFont(display, c->text, font->fid);
	XSetFont(display, c->bg, font->fid);

	set_color(palette.count - 1, color);
}

static void
create_palette(int count)
{
	while (--count >= 0)
		add_color(rand() & 0xffffff);
}

static void
load_palette(const char *path)
{
	FILE *fp;
	unsigned long color;

	if (NULL == (fp = fopen(path, "r")))
		die("failed to open file %s: %s", path, strerror(errno));

	while (palette.count < MAX_COLORS && fscanf(fp, "#%06lx\n", &color) == 1)
		add_color(color & 0xffffff);

	if (palette.count == 0)
		die("invalid file format");

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
	struct Color *c;
	struct Rectangle box = { 0 };
	struct Point tpos = { 0 };
	int i, j, wavail;

	if (ev->count != 0)
		return;

	wavail = width;
	tpos.y = (height + font->ascent) / 2;
	box.height = height;

	for (i = 0; i < palette.count; ++i) {
		box.x += box.width;
		box.width = wavail / (palette.count - i);
		tpos.x = box.x + (box.width - XTextWidth(font, "0", 1) * HEX_STR_LEN) / 2;
		wavail -= box.width;

		c = &palette.colors[i];

		XFillRectangle(display, window, c->bg, box.x, box.y, box.width, box.height);
		XDrawString(display, window, c->text, tpos.x, tpos.y, c->hex, HEX_STR_LEN);

		if (((int)(height)) / 2 < (palette.count + 1) * (font->ascent + 5))
			continue;

		for (j = 0; j < palette.count; ++j) {
			if (j == i)
				continue;
			XDrawString(display, window, palette.colors[j].bg,
					tpos.x, height - (j + 1 - (j > i)) * (font->ascent + 5),
					palette.colors[j].hex, HEX_STR_LEN);
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

	if ((Atom)(ev->data.l[0]) == WM_DELETE_WINDOW) {
		for (i = 0; i < palette.count; ++i)
			printf("%s\n", palette.colors[i].hex);
		destroy_window();
		exit(0);
	}
}

static void
usage(void)
{
	puts("usage: xranco [-hv123456789] [-l palette_file]");
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
	int ncolors;
	const char *loadpath;

	ncolors = 1;
	loadpath = NULL;

	while (++argv, --argc > 0) {
		if ((*argv)[0] == '-' && (*argv)[1] != '\0' && (*argv)[2] == '\0') {
			switch ((*argv)[1]) {
			case 'h': usage(); break;
			case 'v': version(); break;
			case 'l': --argc; loadpath = enotnull(*++argv, "path"); break;
			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9': ncolors = atoi(*argv + 1); break;
			default: die("invalid option %s", *argv); break;
			}
		} else {
			die("unexpected argument: %s", *argv);
		}
	}

	srand(getpid());
	create_window();

	if (NULL == loadpath) create_palette(ncolors);
	else load_palette(loadpath);

	while (1) {
		XNextEvent(display, &ev);
		switch (ev.type) {
		case ClientMessage:     h_client_message(&ev.xclient); break;
		case Expose:            h_expose(&ev.xexpose); break;
		case ConfigureNotify:   h_configure(&ev.xconfigure); break;
		case KeyPress:          h_keypress(&ev.xkey); break;
		}
	}

	/* UNREACHABLE */
	return 0;
}
