#			cakeaux/Makefile
#
#	Makefile for some of cake's auxiliary programs
#
# @(#)$Header$ (BRL)
SHELL=/bin/sh

# Some systems need LEXOPTS= -l to get yylineno.
LEXOPTS=
# LEXOPTS= -l

PROGS = base ccincl later need needed cakesub subcmd usrc gsrc cakeinclude

BINDIR=/usr/brlcad/bin

all:		$(PROGS)

install:	$(PROGS)
		cp $(PROGS) $(BINDIR)

base:		base.o pattern_s.o
		cc -o base base.o pattern_s.o

cakeinclude:	cakeinclude.o
		cc -o cakeinclude cakeinclude.o

ccincl:		ccincl.o ccincl_s.o
		cc -o ccincl ccincl.o ccincl_s.o

later:		later.c
		cc -o later later.c

need:		need.c
		cc -o need need.c

needed:		needed.c
		cc -o needed needed.c

refs:		refs.o pattern_s.o
		cc -o refs refs.o pattern_s.o

cakesub:		cakesub.c
		cc -o cakesub cakesub.c

subcmd:		subcmd.c
		cc -o subcmd subcmd.c

usrc:		usrc.c
		cc -o usrc usrc.c

gsrc:		gsrc.c
		cc -o gsrc gsrc.c

ccincl_s.c:	ccincl_s.l
		lex $(LEXOPTS) ccincl_s.l
		@mv lex.yy.c ccincl_s.c

pattern_s.c:	pattern_s.l
		lex $(LEXOPTS) pattern_s.l
		@mv lex.yy.c pattern_s.c

clean:		
		-/bin/rm -f Make_errs *.o *_s.c

gclean:		
		-/bin/rm -f *_s.c

clobber:	clean
		-/bin/rm -f ${PROGS}
