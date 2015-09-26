CFLAGS= -Wall -g
DEFINES=

all: makron

makron: main.c
	gcc -L./ -o makron main.c -lxcb $(CFLAGS)

clean:
	rm makron
