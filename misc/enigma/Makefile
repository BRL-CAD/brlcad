LDFLAGS=-lcrypt

all:	enigma

install:	enigma
	cp enigma /usr/brlcad/bin/.
	cp enigma.1 /usr/brlcad/man/man1/.
	ln -s enigma /usr/brlcad/bin/crypt

clean:
	rm -f enigma *.o
