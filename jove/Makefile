#
#		M A K E F I L E
#
#  $Revision$
#  @(#)$Header$;
#

# For BSD UNIX on the VAX and Gould
#CC = cc
#CFLAGS=	-O -DBSD=42
#LB=	-ltermlib

# For SGI 4D
CC = cc
CFLAGS= -O2 -DSYS5 -DHAVE_TERMIOS_H
#CFLAGS= -ansi -O2 -DSYS5 -DHAVE_TERMIOS_H
LDFLAGS = -O2
LB=	-lcurses

# For Solaris 2.x
#CC = cc
#CFLAGS = -O -i -Xc -xstrconst -DSYS5 -DHAVE_TERMIOS_H
#LB = -lcurses

# For BSD/OS
CC = cc
CFLAGS= -O2 -DBSD=44 -DHAVE_TERMIOS_H
LB=	-ltermlib

DESTDIR=
BINDIR=${DESTDIR}/usr/brl/bin
LIBDIR=${DESTDIR}/usr/brl/lib/jove

OBJECTS =	jove_buf.o \
		jove_c.o \
		jove_delete.o \
		jove_disp.o \
		jove_draw.o \
		jove_extend.o \
		jove_funcs.o \
		jove_insert.o \
		jove_io.o \
		jove_main.o \
		jove_marks.o \
		jove_misc.o \
		jove_proc.o \
		jove_re.o \
		jove_screen.o \
		jove_term.o \
		jove_wind.o \
		tune.o \
		version.o

all:	xjove xfindcom xteach-jove

xjove:	$(OBJECTS)
	$(CC) $(LDFLAGS) -o xjove $(OBJECTS) $(LB)
	size xjove

xfindcom:	findcom.o tune.o
	$(CC) findcom.o tune.o -o xfindcom

xteach-jove:	teach-jove.sh FRC
	sed -e 's;BinDir;$(BINDIR);' -e 's;LibDir;$(LIBDIR);' < teach-jove.sh > $@

tune.c:		jove_tune.c FRC
	sed -e 's;LibDir;$(LIBDIR);' < jove_tune.c > $@

version.c:	FRC
	if test ! -f version.number ; then echo 0 > version.number; fi
	c=`cat version.number`; \
	rm -f version.number $@; \
	echo $$c | \
		awk '{version = $$1 + 1; };END{printf "%d\n", version > "version.number"; }'; \
	c=`cat version.number`; \
	echo "char	version[] = \"2.$$c\";" > $@

FRC:

$(OBJECTS)				: jove.h

jove_disp.c jove_draw.c jove_wind.c	: termcap.h

jove_io.c	jove_screen.c		: termcap.h jove_temp.h

install:	xjove xfindcom xteach-jove
	-mkdir $(LIBDIR); chmod 755 $(LIBDIR)
	install -c -m 755 xjove $(BINDIR)/jove
	install -c -m 755 xfindcom $(LIBDIR)/findcom
	install -c -m 755 xteach-jove $(BINDIR)/teach-jove

clean:
	rm -f *.o core xjove xfindcom xteach-jove tune.c version.c

tags:	tune.c version.c
	ctags *.c
