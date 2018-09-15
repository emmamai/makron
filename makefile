CFLAGS ?= -Wall -g
# xcb headers
CFLAGS != pkgconf --cflags xcb
OUT := makron

LIBS != pkgconf --libs xcb

all: $(OUT)

$(OUT): src/*.c
	$(CC) -o $(OUT) src/*.c $(LIBS) $(CFLAGS)

clean:
	rm $(OUT)
