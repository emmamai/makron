CFLAGS= -Wall
DEFINES=

all: toolwm
	
toolwm: main.c
	gcc -L./ -o toolwm main.c -lxcb $(CFLAGS)

clean:
	rm *.o *.a toolwm
