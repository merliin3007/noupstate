CC=gcc
CFLAGS=-c -Wall -Wextra -pedantic -Werror -Wshadow -std=gnu11
LDFLAGS=
LIBFLAGS=
SOURCES=$(wildcard src/*.c)
OBJECTS=$(SOURCES:.c=.o)
BINARY=bin/noupstate

all: $(SOURCES) $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBFLAGS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@ $(LIBFLAGS)

clean:
	rm $(OBJECTS)

ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

install: $(BINARY)
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 1 $(BINARY) $(DESTDIR)$(PREFIX)/bin/

