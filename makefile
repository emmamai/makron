CFLAGS ?= -Wall -g
# xcb headers
CFLAGS != pkgconf --cflags xcb

LIBS != pkgconf --libs xcb

all: makron

makron: main.c
	gcc -o makron main.c $(LIBS) $(CFLAGS)

clean:
	rm makron
