#
#		M A K E F I L E 
#
#  $Revision$
#  @(#)$Header$;
#

# For the VAX
#CC = cc
#CFLAGS=	-O -DJOBCONTROL -DVMUNIX
#LB=	-ltermlib

# For BRLUNIX (pdp11)
#LDFLAGS = -i
#CC = cc -7 -DBRLUNIX
#LB=	-ltermlib7

# For SGI 4D
# compile flags
# -cckr works in all cases.
# -xansi works only in cases where -G is greater 16
# -ansi does not work at all nor can I dbx the core file
CC = cc
#CFLAGS= -cckr -g -DSYS5 -Dindex=strchr -Drindex=strrchr
#CFLAGS= -xansi -g -G64 -DSYS5 -Dindex=strchr -Drindex=strrchr
#CFLAGS= -xansi -g -DSYS5 -Dindex=strchr -Drindex=strrchr
CFLAGS= -ansi -O2 -DSYS5 -Dindex=strchr -Drindex=strrchr
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
	sh mkversion
	$(CC) $(LDFLAGS) -o xjove version.o $(OBJECTS) $(LB)
	size xjove

xfindcom:	findcom.o jove_tune.o
	$(CC) findcom.o jove_tune.o -o xfindcom

$(OBJECTS)				: jove.h

jove_disp.c jove_draw.c jove_wind.c	: termcap.h

jove_io.c	jove_screen.c		: termcap.h jove_temp.h

install: xjove xfindcom
	install -c xjove $(BINDIR)/jove
	install -c xfindcom $(LIBDIR)/findcom
	install -c -m 755 teach-jove.sh $(BINDIR)/teach-jove

clean:
	rm -f *.o core xjove xfindcom

tags:	$(OBJECTS)
	ctags *.c
