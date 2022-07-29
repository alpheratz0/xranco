#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#define BAR_HEIGHT 20
#define DEFAULT_BAR_MESSAGE "Press SPACE to change color..."

static Display *display;
static Window window, root;
static unsigned int width = 640, height = 480;
static unsigned long color;
static char bartext[128];
static GC bg, fg, bar;
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
	unsigned char opacity[4];

	if (NULL == (display = XOpenDisplay(NULL))) {
		die("can't open display");
	}

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

	bg = XCreateGC(display, window, 0, NULL);
	fg = XCreateGC(display, window, 0, NULL);
	bar = XCreateGC(display, window, 0, NULL);

	XSetFont(display, fg, font->fid);
	XSetForeground(display, fg, 0x000000);
	XSetForeground(display, bar, 0xffff00);
	XMapWindow(display, window);
}

static void
destroy_window(void)
{
	XUnloadFont(display, font->fid);
	XFreeGC(display, fg);
	XFreeGC(display, bg);
	XFreeGC(display, bar);
	XCloseDisplay(display);
}

static void
h_keypress(XKeyEvent *ev)
{
	KeySym key;

	key = XLookupKeysym(ev, 0);

	switch (key) {
		case XK_space:
			color = rand() & 0xffffff;
			snprintf(bartext, sizeof(bartext) - 1, "#%06x", (unsigned)(color));
			XSetForeground(display, bg, color);
			XClearArea(display, window, 0, 0, 1, 1, True);
			break;
	}
}

static void
h_expose(XExposeEvent *ev)
{
	if (ev->count == 0) {
		XFillRectangle(display, window, bg, 0, 0, width, height - BAR_HEIGHT);
		XFillRectangle(display, window, bar, 0, height - BAR_HEIGHT, width, BAR_HEIGHT);

		XDrawString(
			display, window, fg, (width - XTextWidth(font, bartext, strlen(bartext))) / 2,
			height - (BAR_HEIGHT - font->ascent) / 2, bartext, strlen(bartext)
		);
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

static inline void
print_opt(const char *sh, const char *lo, const char *desc)
{
	printf("%7s | %-25s %s\n", sh, lo, desc);
}

static void
usage(void)
{
	puts("Usage: xranco [ -hv ]");
	puts("Options are:");
	print_opt("-h", "--help", "display this message and exit");
	print_opt("-v", "--version", "display the program version");
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
		else if (**argv == '-') dief("invalid option %s", *argv);
		else dief("unexpected argument: %s", *argv);
	}

	create_window();
	srand(getpid());
	snprintf(bartext, sizeof(bartext) - 1, "%s", DEFAULT_BAR_MESSAGE);

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
