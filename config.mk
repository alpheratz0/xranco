# Copyright (C) 2022-2023 <alpheratz99@protonmail.com>
# This program is free software.

VERSION   = 0.2.0

CC        = cc
INCS      = -I/usr/X11R6/include
CFLAGS    = -std=c99 -pedantic -Wall -Wextra -Os $(INCS) -DVERSION=\"$(VERSION)\"
LDLIBS    = -lX11
LDFLAGS   = -L/usr/X11R6/lib -s

PREFIX    = /usr/local
MANPREFIX = $(PREFIX)/share/man
