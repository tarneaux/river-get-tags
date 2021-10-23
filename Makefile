SCANNER := wayland-scanner

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

CFLAGS=-Wall -Werror -Wextra -Wpedantic -Wno-unused-parameter -Wconversion -Wformat-security -Wformat -Wsign-conversion -Wfloat-conversion -Wunused-result $(shell pkg-config --cflags pixman-1)
LIBS=-lwayland-client $(shell pkg-config --libs pixman-1) -lrt
OBJ=river-shifttags.o river-status-unstable-v1.o river-control-unstable-v1.o
GEN=river-status-unstable-v1.c river-status-unstable-v1.h river-control-unstable-v1.c river-control-unstable-v1.h

river-shifttags: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

$(OBJ): $(GEN)

%.c: %.xml
	$(SCANNER) private-code < $< > $@

%.h: %.xml
	$(SCANNER) client-header < $< > $@

install: river-shifttags
	install -D river-shifttags   $(DESTDIR)$(BINDIR)/river-shifttags
	install -D river-shifttags.1 $(DESTDIR)$(MANDIR)/man1/river-shifttags.1

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/river-shifttags
	$(RM) $(DESTDIR)$(MANDIR)/man1/river-shifttags.1

run: river-shifttags
	./river-shifttags

clean:
	$(RM) river-shifttags $(GEN) $(OBJ)

.PHONY: clean install

