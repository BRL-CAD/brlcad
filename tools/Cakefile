/*
 *			tools/Cakefile
 */

#define SRCDIR	tools
#define PRODUCTS	applymap avg4 crop fant into mcut mergechan pyrmask \
	repos rleaddcom rlebg rlebox rlecat rlecomp \
	rledither rleflip rlehdr rlehisto rleldmap rlemandl \
	rlenoise rlepatch rleprint rlequant rlescale \
	rleselect rlesetbg rlespiff rlesplice rlesplit \
	rlestereo rleswap rlezoom smush to8 tobw unexp \
	unslice rleClock font.c \
	cubitorle giftorle graytorle painttorle rawtorle \
	rletoabA60 rletoascii rletogray rletopaint \
	rletops rletoraw targatorle wasatchrle \
	rletogif rletoabA62 

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

crop:	crop.o LIBRLE rle_box.o
	CC LDFLAGS crop.o rle_box.o LIB_PRE''LIBRLE LIBES -o crop

fant:	fant.o LIBRLE
	CC LDFLAGS fant.o LIB_PRE''LIBRLE LIBES -o fant

into:	into.o LIBRLE
	CC LDFLAGS into.o LIB_PRE''LIBRLE LIBES -o into

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

rlebox:	rlebox.o LIBRLE rle_box.o
	CC LDFLAGS rlebox.o rle_box.o LIB_PRE''LIBRLE LIBES -o rlebox

rlecat:	rlecat.o LIBRLE
	CC LDFLAGS rlecat.o LIB_PRE''LIBRLE LIBES -o rlecat

rlecomp: rlecomp.o LIBRLE
	CC LDFLAGS rlecomp.o LIB_PRE''LIBRLE LIBES -o rlecomp

rledither: rledither.o LIBRLE
	CC LDFLAGS rledither.o LIB_PRE''LIBRLE LIBES -o rledither

rlestereo: rlestereo.o LIBRLE
	CC LDFLAGS rlestereo.o LIB_PRE''LIBRLE LIBES -o rlestereo

rlesplice: rlesplice.o LIBRLE
	CC LDFLAGS rlesplice.o LIB_PRE''LIBRLE LIBES -o rlesplice

rlespiff: rlespiff.o LIBRLE
	CC LDFLAGS rlespiff.o LIB_PRE''LIBRLE LIBES -o rlespiff

rlenoise: rlenoise.o LIBRLE
	CC LDFLAGS rlenoise.o LIB_PRE''LIBRLE LIBES -o rlenoise

rleselect: rleselect.o LIBRLE
	CC LDFLAGS rleselect.o LIB_PRE''LIBRLE LIBES -o rleselect

rlescale: rlescale.o LIBRLE
	CC LDFLAGS rlescale.o LIB_PRE''LIBRLE LIBES -o rlescale

rlequant: rlequant.o LIBRLE
	CC LDFLAGS rlequant.o LIB_PRE''LIBRLE LIBES -o rlequant

rleprint: rleprint.o LIBRLE
	CC LDFLAGS rleprint.o LIB_PRE''LIBRLE LIBES -o rleprint

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

font.c:	../SRCDIR/font.src
	rm -f font.c
	sh ../SRCDIR/makeFont.sh ../SRCDIR/font.src > font.c

rleClock:	rleClock.o font.c LIBRLE
	CC LDFLAGS rleClock.o font.c LIB_PRE''LIBRLE LIBES -o rleClock

cubitorle:	cubitorle.o
	CC LDFLAGS -o cubitorle cubitorle.o LIB_PRE''LIBRLE LIBES

giftorle:	giftorle.o
	CC LDFLAGS giftorle.o LIB_PRE''LIBRLE LIBES -o giftorle

graytorle:	graytorle.o
	CC LDFLAGS graytorle.o LIB_PRE''LIBRLE LIBES -o graytorle

painttorle:	painttorle.o
	CC LDFLAGS painttorle.o LIB_PRE''LIBRLE LIBES -o painttorle

rawtorle:	rawtorle.o
	CC LDFLAGS rawtorle.o LIB_PRE''LIBRLE LIBES -o rawtorle

rletoabA60:	rletoabA60.o
	CC LDFLAGS rletoabA60.o LIB_PRE''LIBRLE LIBES -o rletoabA60

rletoascii:	rletoascii.o
	CC LDFLAGS rletoascii.o LIB_PRE''LIBRLE LIBES -o rletoascii

rletogray:	rletogray.o
	CC LDFLAGS rletogray.o LIB_PRE''LIBRLE LIBES -o rletogray

rletopaint:	rletopaint.o
	CC LDFLAGS rletopaint.o LIB_PRE''LIBRLE LIBES -o rletopaint

rletops:	rletops.o
	CC LDFLAGS rletops.o LIB_PRE''LIBRLE LIBES -o rletops

rletoraw:	rletoraw.o
	CC LDFLAGS rletoraw.o LIB_PRE''LIBRLE LIBES -o rletoraw

targatorle:	targatorle.o
	CC LDFLAGS targatorle.o LIB_PRE''LIBRLE LIBES -o targatorle

wasatchrle:	wasatchrle.o
	CC LDFLAGS wasatchrle.o LIB_PRE''LIBRLE LIBES -o wasatchrle

rletogif:	rletogif.o gifencod.o compgif.o
	CC LDFLAGS rletogif.o gifencod.o compgif.o LIB_PRE''LIBRLE LIBES -o rletogif

rletoabA62:	rletoabA62.o rle.o
	CC LDFLAGS rletoabA62.o rle.o LIB_PRE''LIBRLE LIBES -o rletoabA62

#include "../Cakefile.rules"
