/*
 *			tools/Cakefile
 */

#define SRCDIR	tools
#define PRODUCTS	applymap avg4 comp crop fant mcut \
     mergechan pyrmask \
     repos rlebg rleflip rlehdr rleldmap rlesetbg \
     rleaddcom rlehisto smush rlebox rlezoom rlesplit \
     rleswap to8 tobw unexp \
     unslice rlemandl rlepatch rleClock

#define	SRCSUFF	.c
#define MANSECTION	1

#include "../Cakefile.defs"
#include "../Cakefile.prog"

/* Explicit composition of each product */

applymap:	applymap.o LIBRLE
	CC LDFLAGS applymap.o LIB_PRE''LIBRLE LIBES -o applymap

avg4:	avg4.o LIBRLE
	CC LDFLAGS avg4.o LIB_PRE''LIBRLE LIBES -o avg4

comp:	comp.o LIBRLE
	CC LDFLAGS comp.o LIB_PRE''LIBRLE LIBES -o comp

crop:	crop.o LIBRLE
	CC LDFLAGS crop.o LIB_PRE''LIBRLE LIBES -o crop

fant:	fant.o LIBRLE
	CC LDFLAGS fant.o LIB_PRE''LIBRLE LIBES -o fant

mcut:	mcut.o LIBRLE
	CC LDFLAGS mcut.o LIB_PRE''LIBRLE LIBES -o mcut

mergechan:	mergechan.o LIBRLE
	CC LDFLAGS mergechan.o LIB_PRE''LIBRLE LIBES -o mergechan

pyrmask:	pyrmask.o pyrlib.o LIBRLE
	CC LDFLAGS pyrmask.o pyrlib.o LIB_PRE''LIBRLE LIBES -o pyrmask

repos:	repos.o LIBRLE
	CC LDFLAGS repos.o LIB_PRE''LIBRLE LIBES -o repos

rlebg:	rlebg.o LIBRLE
	CC LDFLAGS rlebg.o LIB_PRE''LIBRLE LIBES -o rlebg

rleflip:	rleflip.o LIBRLE
	CC LDFLAGS rleflip.o LIB_PRE''LIBRLE LIBES -o rleflip

rlehdr:	rlehdr.o LIBRLE
	CC LDFLAGS rlehdr.o LIB_PRE''LIBRLE LIBES -o rlehdr

rleldmap:	rleldmap.o LIBRLE
	CC LDFLAGS rleldmap.o LIB_PRE''LIBRLE LIBES -o rleldmap

rlesetbg:	rlesetbg.o LIBRLE
	CC LDFLAGS rlesetbg.o LIB_PRE''LIBRLE LIBES -o rlesetbg

rleaddcom:	rleaddcom.o LIBRLE
	CC LDFLAGS rleaddcom.o LIB_PRE''LIBRLE LIBES -o rleaddcom

rlehisto:	rlehisto.o LIBRLE
	CC LDFLAGS rlehisto.o LIB_PRE''LIBRLE LIBES -o rlehisto

smush:	smush.o LIBRLE
	CC LDFLAGS smush.o LIB_PRE''LIBRLE LIBES -o smush

rlebox:	rlebox.o LIBRLE
	CC LDFLAGS rlebox.o LIB_PRE''LIBRLE LIBES -o rlebox

rlezoom:	rlezoom.o LIBRLE
	CC LDFLAGS rlezoom.o LIB_PRE''LIBRLE LIBES -o rlezoom

rlesplit:	rlesplit.o LIBRLE
	CC LDFLAGS rlesplit.o LIB_PRE''LIBRLE LIBES -o rlesplit

rleswap:	rleswap.o LIBRLE
	CC LDFLAGS rleswap.o LIB_PRE''LIBRLE LIBES -o rleswap

to8:	to8.o LIBRLE
	CC LDFLAGS to8.o LIB_PRE''LIBRLE LIBES -o to8

tobw:	tobw.o LIBRLE
	CC LDFLAGS tobw.o LIB_PRE''LIBRLE LIBES -o tobw

unexp:	unexp.o LIBRLE
	CC LDFLAGS unexp.o LIB_PRE''LIBRLE LIBES -o unexp

unslice:	unslice.o LIBRLE
	CC LDFLAGS unslice.o LIB_PRE''LIBRLE LIBES -o unslice

rlemandl:	rlemandl.o LIBRLE
	CC LDFLAGS rlemandl.o LIB_PRE''LIBRLE LIBES -o rlemandl

rlepatch:	rlepatch.o LIBRLE
	CC LDFLAGS rlepatch.o LIB_PRE''LIBRLE LIBES -o rlepatch

/* From Bob Brown at RIACS */
rleClock:	rleClock.o ../SRCDIR/font.s LIBRLE
	rm -f font.c
	sh ../SRCDIR/makeFont.sh ../SRCDIR/font.s > font.c
	CC LDFLAGS rleClock.o font.c LIB_PRE''LIBRLE LIBES -o rleClock

#include "../Cakefile.rules"
