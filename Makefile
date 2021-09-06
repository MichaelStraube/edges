PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

CFLAGS ?= -O2 -Wall -Wextra
CFLAGS += `pkg-config --cflags x11 xi xrandr`
LDLIBS = `pkg-config --libs x11 xi xrandr`

objects = src/edges.o src/util.o

all: edges

edges: $(objects)
	$(CC) $(LDFLAGS) -o edges $(objects) $(LDLIBS)

install: edges
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 edges "$(DESTDIR)$(BINDIR)"
	install -d "$(DESTDIR)$(MANDIR)/man1"
	install -m 644 man/edges.1 "$(DESTDIR)$(MANDIR)/man1"

clean:
	rm -f edges $(objects)

.PHONY:	all install clean
