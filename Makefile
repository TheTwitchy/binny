#binny Makefile
#Relies on libncurses, which may or may not be installed by default.

all:
	gcc -o binny -lncurses binny.c

clean:
	rm binny