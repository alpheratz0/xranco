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

*/

#define MAX_COLORS 9

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

struct color {
	GC bg, text;
	unsigned long rgb;
	char hex[64];
};

static Display *display;
static Window window, root;
static unsigned int width, height;
static int ncolors = 1;
static struct color colors[MAX_COLORS];
static XFontStruct *font;
static Atom wm_delete_window, wm_window_opacity;

static void
die(const char *err)
{
	fprintf(stderr, "xranco: %s\n", err);
	exit(1);
}

static void
dief(const char *err, ...)
{
	va_list list;
	fputs("xranco: ", stderr);
	va_start(list, err);
	vfprintf(stderr, err, list);
	va_end(list);
	fputc('\n', stderr);
	exit(1);
}

static void
create_window(void)
{
	int i;
	unsigned char opacity[4];

	if (NULL == (display = XOpenDisplay(NULL))) {
		die("can't open display");
	}

	width = 640;
	height = 480;
	root = DefaultRootWindow(display);
	window = XCreateSimpleWindow(display, root, 0, 0, width, height, 0, 0, 0);
	wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
	wm_window_opacity = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
	opacity[0] = opacity[1] = opacity[2] = opacity[3] = 0xff;

	XStoreName(display, window, "xranco");
	XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
	XSetWMProtocols(display, window, &wm_delete_window, 1);
	XChangeProperty(display, window, wm_window_opacity, XA_CARDINAL, 32, PropModeReplace, opacity, 1);

	if (NULL == (font = XLoadQueryFont(display, "fixed"))) {
		die("can't open font");
	}

	for (i = 0; i < ncolors; ++i) {
		colors[i].rgb = rand() & 0xffffff;
		colors[i].bg = XCreateGC(display, window, 0, NULL);
		colors[i].text = XCreateGC(display, window, 0, NULL);

		XSetForeground(display, colors[i].bg, colors[i].rgb);
		XSetForeground(display, colors[i].text, 0x000000);
		XSetFont(display, colors[i].text, font->fid);

		snprintf(colors[i].hex, sizeof(colors[i].hex) - 1, "#%06x", (unsigned)(colors[i].rgb));
	}

	XMapWindow(display, window);
}

static void
destroy_window(void)
{
	int i;

	XUnloadFont(display, font->fid);

	for (i = 0; i < ncolors; ++i) {
		XFreeGC(display, colors[i].bg);
		XFreeGC(display, colors[i].text);
	}

	XCloseDisplay(display);
}

static int
get_color_brightness(unsigned long color)
{
	double brightness;

	brightness = 0.2126 * ((color >> 16) & 0xff) +
		   0.7152 * ((color >> 8) & 0xff) +
		   0.0722 * (color & 0xff);

	return (int)(brightness);
}

static void
h_keypress(XKeyEvent *ev)
{
	int i, from, to;
	KeySym key;

	key = XLookupKeysym(ev, 0);

	switch (key) {
		case XK_1: case XK_2: case XK_3: case XK_4:
		case XK_5: case XK_6: case XK_7: case XK_8:
		case XK_9:
			from = key - XK_1;
			to = from + 1;

			if (to > ncolors) {
				return;
			}
			break;
		case XK_space:
			from = 0;
			to = ncolors;
			break;
		default:
			return;
	}

	for (i = from; i < to; ++i) {
		colors[i].rgb = rand() & 0xffffff;

		XSetForeground(display, colors[i].bg, colors[i].rgb);
		XSetForeground(
			display, colors[i].text,
			get_color_brightness(colors[i].rgb) < 50 ?
				0xffffff :
				0x000000
		);

		snprintf(colors[i].hex, sizeof(colors[i].hex) - 1, "#%06x", (unsigned)(colors[i].rgb));
	}

	XClearArea(display, window, 0, 0, 1, 1, True);
}

static void
h_expose(XExposeEvent *ev)
{
	int i, cw, x;

	if (ev->count == 0) {
		x = 0;
		cw = width / ncolors;

		for (i = 0; i < ncolors; ++i) {
			XFillRectangle(display, window, colors[i].bg, x, 0, cw, height);
			XDrawString(
				display, window, colors[i].text, x + (cw - XTextWidth(font, colors[i].hex, strlen(colors[i].hex))) / 2,
				(height + font->ascent) / 2, colors[i].hex, strlen(colors[i].hex)
			);
			x += cw;
		}
	}
}

static void
h_configure(XConfigureEvent *ev)
{
	width  = ev->width;
	height = ev->height;
}

static void
h_client_message(XClientMessageEvent *ev)
{
	if ((Atom)(ev->data.l[0]) == wm_delete_window) {
		destroy_window();
		exit(0);
	}
}

static int
match_opt(const char *in, const char *sh, const char *lo)
{
	return (strcmp(in, sh) == 0) || (strcmp(in, lo) == 0);
}

static int
match_numeric_opt(const char *in, int from, int to)
{
	return *in == '-' && *(in+1) >= ('0' + from) && *(in+1) <= ('0' + to) && *(in+2) == '\0';
}

static void
usage(void)
{
	puts("usage: xranco [-hv123456789]");
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

	if (++argv, --argc > 0) {
		if (match_opt(*argv, "-h", "--help")) usage();
		else if (match_opt(*argv, "-v", "--version")) version();
		else if (match_numeric_opt(*argv, 1, MAX_COLORS)) ncolors = atoi(&(*argv)[1]);
		else if (**argv == '-') dief("invalid option %s", *argv);
		else dief("unexpected argument: %s", *argv);
	}

	srand(getpid());
	create_window();

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
