#			cakeaux/Makefile
#
#	Makefile for some of cake's auxiliary programs
#
# @(#)$Header$ (BRL)
SHELL=/bin/sh

# Use the following two lines for gnu compiler
# CC = gcc
# CFLAGS = -fwritable-strings
CC = cc
CFLAGS = 

# Some systems use "flex" insteda of "lex"
# LEX = flex
LEX = lex

# Some systems need LEXOPTS= -l to get yylineno.
LEXOPTS=
# LEXOPTS= -l

PROGS = base ccincl later need needed cakesub subcmd usrc gsrc cakeinclude

BINDIR=/usr/brlcad/bin

all:		$(PROGS)

install:	$(PROGS)
		cp $(PROGS) $(BINDIR)

base:		base.o pattern_s.o
		$(CC) $(CFLAGS) -o base base.o pattern_s.o

cakeinclude:	cakeinclude.o
		$(CC) $(CFLAGS) -o cakeinclude cakeinclude.o

ccincl:		ccincl.o ccincl_s.o
		$(CC) $(CFLAGS) -o ccincl ccincl.o ccincl_s.o

later:		later.c
		$(CC) $(CFLAGS) -o later later.c

need:		need.c
		$(CC) $(CFLAGS) -o need need.c

needed:		needed.c
		$(CC) $(CFLAGS) -o needed needed.c

refs:		refs.o pattern_s.o
		$(CC) $(CFLAGS) -o refs refs.o pattern_s.o

cakesub:		cakesub.c
		$(CC) $(CFLAGS) -o cakesub cakesub.c

subcmd:		subcmd.c
		$(CC) $(CFLAGS) -o subcmd subcmd.c

usrc:		usrc.c
		$(CC) $(CFLAGS) -o usrc usrc.c

gsrc:		gsrc.c
		$(CC) $(CFLAGS) -o gsrc gsrc.c

ccincl_s.c:	ccincl_s.l
		$(LEX) $(LEXOPTS) ccincl_s.l
		@mv lex.yy.c ccincl_s.c

pattern_s.c:	pattern_s.l
		$(LEX) $(LEXOPTS) pattern_s.l
		@mv lex.yy.c pattern_s.c

clean:		
		-/bin/rm -f Make_errs *.o *_s.c

gclean:		
		-/bin/rm -f *_s.c

clobber:	clean
		-/bin/rm -f ${PROGS}
