CFLAGS=	-O -DBRL
CC = cc
#BRL CC = cc -7

LB=	-ltermlib -lc
#BRL LB=	-ltermlib7

BINDIR=/usr/brl/bin
LIBDIR=/usr/brl/lib/jove

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
		jove_wind.o

all:	xjove xfindcom

xjove:	$(OBJECTS)
	$(CC) -z -o xjove $(OBJECTS) $(LB)
#PDP	$(CC) -i -o xjove $(OBJECTS) $(LB)
	size xjove

xfindcom:	findcom.c jove_tune.h
	$(CC) findcom.c -o xfindcom

$(OBJECTS)				: jove.h

jove.h					: jove_tune.h

jove_disp.c jove_draw.c jove_wind.c	: termcap.h

jove_io.c	jove_screen.c		: termcap.h jove_temp.h

install:	cp-jove cp-findcom

clean:
	rm -f *.o xjove xfindcom

tags:	$(OBJECTS)
	ctags *.c

cp-jove:	xjove
	cp xjove $(BINDIR)/jove
	chmod 751 $(BINDIR)/jove

cp-findcom:	xfindcom
	cp xfindcom $(LIBDIR)/findcom
	chmod 751 $(LIBDIR)/findcom
