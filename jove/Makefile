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
CFLAGS= -O2 -DSYS5
#CFLAGS= -ansi -O2 -DSYS5
LDFLAGS = -O2
LB=	-lcurses

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
		jove_tune.o \
		jove_wind.o 

all:	xjove xfindcom

xjove:	$(OBJECTS)
	sh mkversion.sh
	$(CC) $(LDFLAGS) -o xjove version.o $(OBJECTS) $(LB)
	size xjove

xfindcom:	findcom.o jove_tune.o
	$(CC) findcom.o jove_tune.o -o xfindcom

$(OBJECTS)				: jove.h

jove_disp.c jove_draw.c jove_wind.c	: termcap.h

jove_io.c	jove_screen.c		: termcap.h jove_temp.h

install: xjove xfindcom
	-mkdir $(LIBDIR); chmod 755 $(LIBDIR)
	install -c -m 755 xjove $(BINDIR)/jove
	install -c -m 755 xfindcom $(LIBDIR)/findcom
	install -c -m 755 teach-jove.sh $(BINDIR)/teach-jove

clean:
	rm -f *.o core xjove xfindcom

tags:	$(OBJECTS)
	ctags *.c
