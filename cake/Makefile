#	Makefile for Cake
#
#$Header$
SHELL=/bin/sh

# -DATT for SystemV, -DBSD for Berkeley, done by machinetype.sh
#
# For Apollo, may need to add -Tbsd4.3
#
# For gnu compiler use the following two lines:
# CC = gcc
# COPTS = -DCAKEDEBUG=1 `machinetype.sh -d` -fwritable-strings
#
COPTS = -DCAKEDEBUG `machinetype.sh -d`

# for gnu or Linux use "flex" instead of "lex"
LEX = lex

# Some systems (such as NetBSD 0.9a), need to add -l to get yylineno.
LEXOPTS=
# LEXOPTS= -l

BINDIR=/usr/brlcad/bin
MANDIR=/usr/brlcad/man/man1

#LDFLAGS=-k 64k

HDR   =	cake.h conf.h port.h
SRC   =	act.c chase.c entry.c error.c expand.c file.c \
	list.c main.c make.c mem.c pat.c print.c proc.c \
	sym.c sys.c table.c test.c trail.c
OBJ   =	act.o chase.o entry.o error.o expand.o file.o \
	list.o main.o make.o mem.o pat.o print.o proc.o \
	sym.o sys.o table.o test.o trail.o
GEN   =	cake_g.y cake_s.l
GHDR  =	cake_g.h
GSRC  =	cake_g.c cake_s.c
GOBJ  =	cake_g.o cake_s.o

CFLAGS= $(COPTS)

cake:		$(OBJ) $(GOBJ)
		$(CC) $(LDFLAGS) $(CFLAGS) -o cake $(OBJ) $(GOBJ)
#		-/bin/rm fake
#		ln cake fake

all:		cake lint tags

install:	cake
		-if test -f $(BINDIR)/cake; then \
			mv -f $(BINDIR)/cake $(BINDIR)/cake.bak; fi
		mv cake $(BINDIR)/cake
		rm -f $(BINDIR)/fake
		ln -s $(BINDIR)/cake $(BINDIR)/fake
		rm -f $(MANDIR)/cake.1 $(MANDIR)/fake.1
		cp cake.1 $(MANDIR)
		ln -s $(MANDIR)/cake.1 $(MANDIR)/fake.1

$(OBJ):		$(HDR)
$(GOBJ):	$(HDR)

cake_g.c:	cake_g.y
		yacc -dv cake_g.y
		mv -f y.tab.h cake_g.h
		mv -f y.tab.c cake_g.c
		mv -f y.output cake_g.t

cake_s.c:	cake_s.l
		$(LEX) $(LEXOPTS) cake_s.l
		mv -f lex.yy.c cake_s.c

var:		$(SRC) $(GSRC)
		var -DATT $(COPTS) $(SRC) $(GSRC) > Var

plint:		$(SRC) $(GSRC)
		lint -acpx $(COPTS) $(SRC) $(GSRC) > Lint_errs

ulint:		$(SRC) $(GSRC)
		lint -v $(COPTS) $(SRC) $(GSRC) > Lint_errs

lint:		$(SRC) $(GSRC)
		lint -v $(COPTS) $(SRC) $(GSRC) > Lint_errs
		@gred yy Lint_errs
		@gred malloc Lint_errs
		@gred strcpy Lint_errs
		@gred strcat Lint_errs
		@gred printf Lint_errs
		@gred flsbuf Lint_errs
		@gred setbuf Lint_errs
		@gred rcs_id Lint_errs
		@gred longjmp Lint_errs
		@gred sbrk Lint_errs

cchk:		$(SRC)
		cchk $(SRC) > Cchk_errs

tags:		$(SRC) $(GSRC)
		ctags $(SRC) $(GSRC)

cxref:		$(SRC)
		cxref -C $(SRC) > Cxref

ssize:
		ssize $(SRC) $(GEN)

asize:
		ssize $(SRC) $(GEN) $(GSRC)

clean:		
		/bin/rm -f cake_g.h cake_g.t \
			Make_errs Lint_errs Cchk_errs core \
			$(OBJ) $(GOBJ) $(GSRC)

gclean:		
		/bin/rm -f $(GSRC) $(GHDR)

clobber: clean
		/bin/rm -f cake fake
