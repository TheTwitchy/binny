#binny Makefile
#Relies on libncurses, which may or may not be installed by default.

all:
	@test -f /usr/include/curses.h || { echo "error: libncurses-dev is not installed"; exit 1; }
	@gcc -Wall -o binny -lncurses binny.c

install:
	@mv ./binny /usr/bin/binny

remove:
	@rm -f /usr/bin/binny

clean:
	@rm -f ./binny
