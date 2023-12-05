SCANNER := wayland-scanner

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

CFLAGS=-Wall -Werror -Wextra -Wpedantic -Wno-unused-parameter -Wconversion -Wformat-security -Wformat -Wsign-conversion -Wfloat-conversion -Wunused-result $(shell pkg-config --cflags pixman-1)
LIBS=-lwayland-client $(shell pkg-config --libs pixman-1) -lrt
OBJ=river-get-tags.o river-status-unstable-v1.o river-control-unstable-v1.o
GEN=river-status-unstable-v1.c river-status-unstable-v1.h river-control-unstable-v1.c river-control-unstable-v1.h

river-get-tags: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

$(OBJ): $(GEN)

%.c: %.xml
	$(SCANNER) private-code < $< > $@

%.h: %.xml
	$(SCANNER) client-header < $< > $@

install: river-get-tags
	install -D river-get-tags   $(DESTDIR)$(BINDIR)/river-get-tags

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/river-get-tags

run: river-get-tags
	./river-get-tags

clean:
	$(RM) river-get-tags $(GEN) $(OBJ)

.PHONY: clean install

