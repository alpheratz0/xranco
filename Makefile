.POSIX:
.PHONY: all clean install uninstall dist

include config.mk

all: xranco

xranco: xranco.o
	$(CC) $(LDFLAGS) -o xranco xranco.o $(LDLIBS)

clean:
	rm -f xranco xranco.o xranco-$(VERSION).tar.gz

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f xranco $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/xranco
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -f xranco.1 $(DESTDIR)$(MANPREFIX)/man1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/xranco.1

dist: clean
	mkdir -p xranco-$(VERSION)
	cp -R COPYING config.mk Makefile README xranco.1 \
		xranco.c xranco-$(VERSION)
	tar -cf xranco-$(VERSION).tar xranco-$(VERSION)
	gzip xranco-$(VERSION).tar
	rm -rf xranco-$(VERSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/xranco
	rm -f $(DESTDIR)$(MANPREFIX)/man1/xranco.1
