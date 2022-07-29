VERSION = 0.1.0

CC      = cc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Os -DVERSION=\"${VERSION}\"
LDLIBS  = -lX11
LDFLAGS = -s ${LDLIBS}

PREFIX    = /usr/local
MANPREFIX = ${PREFIX}/share/man

all: xranco

.c.o:
	${CC} -c ${CFLAGS} $<

xranco: xranco.o
	${CC} -o $@ $< ${LDFLAGS}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f xranco ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/xranco
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f xranco.1 ${DESTDIR}${MANPREFIX}/man1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/xranco.1

dist: clean
	mkdir -p xranco-${VERSION}
	cp -R LICENSE Makefile README xranco.1 xranco.c xranco-${VERSION}
	tar -cf xranco-${VERSION}.tar xranco-${VERSION}
	gzip xranco-${VERSION}.tar
	rm -rf xranco-${VERSION}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/xranco
	rm -f ${DESTDIR}${MANPREFIX}/man1/xranco.1

clean:
	rm -f xranco xranco.o xranco-${VERSION}.tar.gz

.PHONY: all clean install uninstall dist
