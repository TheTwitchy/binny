#binny Makefile
#Relies on libncurses, which may or may not be installed by default.

all:
	@test -f /usr/include/curses.h || { echo "error: libncurses-dev is not installed"; exit 1; }
	@gcc -Wall -o binny binny.c -lncurses
	
standalone:
	@test -f /usr/include/curses.h || { echo "error: libncurses-dev is not installed"; exit 1; }
	@gcc -Wall -static -static-libgcc -static-libstdc++ -o binny binny.c -l:libncurses.a -l:libtinfo.a

install:
	@mv ./binny /usr/bin/binny

remove:
	@rm -f /usr/bin/binny

clean:
	@rm -f ./binny
