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
#define STATIC_PRODUCTS	../SRCDIR/font.src

#define	SRCSUFF	.c
#define MANSECTION	1

#include "../Cakefile.defs"
#include "../Cakefile.prog"

/* Explicit composition of each product */

applymap:	applymap.o
	CC LDFLAGS applymap.o LIBRLE LIBES -o applymap

avg4:	avg4.o
	CC LDFLAGS avg4.o LIBRLE LIBES -o avg4

comp:	comp.o
	CC LDFLAGS comp.o LIBRLE LIBES -o comp

crop:	crop.o rle_box.o
	CC LDFLAGS crop.o rle_box.o LIBRLE LIBES -o crop

fant:	fant.o
	CC LDFLAGS fant.o LIBRLE LIBES -o fant

into:	into.o
	CC LDFLAGS into.o LIBRLE LIBES -o into

mcut:	mcut.o
	CC LDFLAGS mcut.o LIBRLE LIBES -o mcut

mergechan:	mergechan.o
	CC LDFLAGS mergechan.o LIBRLE LIBES -o mergechan

pyrmask:	pyrmask.o pyrlib.o
	CC LDFLAGS pyrmask.o pyrlib.o LIBRLE LIBES -o pyrmask

repos:	repos.o
	CC LDFLAGS repos.o LIBRLE LIBES -o repos

rlebg:	rlebg.o
	CC LDFLAGS rlebg.o LIBRLE LIBES -o rlebg

rleflip:	rleflip.o
	CC LDFLAGS rleflip.o LIBRLE LIBES -o rleflip

rlehdr:	rlehdr.o
	CC LDFLAGS rlehdr.o LIBRLE LIBES -o rlehdr

rleldmap:	rleldmap.o
	CC LDFLAGS rleldmap.o LIBRLE LIBES -o rleldmap

rlesetbg:	rlesetbg.o
	CC LDFLAGS rlesetbg.o LIBRLE LIBES -o rlesetbg

rleaddcom:	rleaddcom.o
	CC LDFLAGS rleaddcom.o LIBRLE LIBES -o rleaddcom

rlehisto:	rlehisto.o
	CC LDFLAGS rlehisto.o LIBRLE LIBES -o rlehisto

smush:	smush.o
	CC LDFLAGS smush.o LIBRLE LIBES -o smush

rlebox:	rlebox.o rle_box.o
	CC LDFLAGS rlebox.o rle_box.o LIBRLE LIBES -o rlebox

rlecat:	rlecat.o
	CC LDFLAGS rlecat.o LIBRLE LIBES -o rlecat

rlecomp: rlecomp.o
	CC LDFLAGS rlecomp.o LIBRLE LIBES -o rlecomp

rledither: rledither.o
	CC LDFLAGS rledither.o LIBRLE LIBES -o rledither

rlestereo: rlestereo.o
	CC LDFLAGS rlestereo.o LIBRLE LIBES -o rlestereo

rlesplice: rlesplice.o
	CC LDFLAGS rlesplice.o LIBRLE LIBES -o rlesplice

#ifdef __NetBSD__
rlespiff: rlespiff.o
	CC LDFLAGS rlespiff.o LIBRLE LIBES -lcompat -o rlespiff
#else
rlespiff: rlespiff.o
	CC LDFLAGS rlespiff.o LIBRLE LIBES -o rlespiff
#endif

rlenoise: rlenoise.o
	CC LDFLAGS rlenoise.o LIBRLE LIBES -o rlenoise

rleselect: rleselect.o
	CC LDFLAGS rleselect.o LIBRLE LIBES -o rleselect

rlescale: rlescale.o
	CC LDFLAGS rlescale.o LIBRLE LIBES -o rlescale

rlequant: rlequant.o
	CC LDFLAGS rlequant.o LIBRLE LIBES -o rlequant

rleprint: rleprint.o
	CC LDFLAGS rleprint.o LIBRLE LIBES -o rleprint

rlezoom:	rlezoom.o
	CC LDFLAGS rlezoom.o LIBRLE LIBES -o rlezoom

rlesplit:	rlesplit.o
	CC LDFLAGS rlesplit.o LIBRLE LIBES -o rlesplit

rleswap:	rleswap.o
	CC LDFLAGS rleswap.o LIBRLE LIBES -o rleswap

to8:	to8.o
	CC LDFLAGS to8.o LIBRLE LIBES -o to8

tobw:	tobw.o
	CC LDFLAGS tobw.o LIBRLE LIBES -o tobw

unexp:	unexp.o
	CC LDFLAGS unexp.o LIBRLE LIBES -o unexp

unslice:	unslice.o
	CC LDFLAGS unslice.o LIBRLE LIBES -o unslice

rlemandl:	rlemandl.o
	CC LDFLAGS rlemandl.o LIBRLE LIBES -o rlemandl

rlepatch:	rlepatch.o
	CC LDFLAGS rlepatch.o LIBRLE LIBES -o rlepatch

font.c:	../SRCDIR/font.src
	rm -f font.c
	sh ../SRCDIR/makeFont.sh ../SRCDIR/font.src > font.c

rleClock:	rleClock.o font.c
	CC LDFLAGS rleClock.o font.c LIBRLE LIBES -o rleClock

cubitorle:	cubitorle.o
	CC LDFLAGS -o cubitorle cubitorle.o LIBRLE LIBES

giftorle:	giftorle.o
	CC LDFLAGS giftorle.o LIBRLE LIBES -o giftorle

graytorle:	graytorle.o
	CC LDFLAGS graytorle.o LIBRLE LIBES -o graytorle

painttorle:	painttorle.o
	CC LDFLAGS painttorle.o LIBRLE LIBES -o painttorle

rawtorle:	rawtorle.o
	CC LDFLAGS rawtorle.o LIBRLE LIBES -o rawtorle

rletoabA60:	rletoabA60.o
	CC LDFLAGS rletoabA60.o LIBRLE LIBES -o rletoabA60

rletoascii:	rletoascii.o
	CC LDFLAGS rletoascii.o LIBRLE LIBES -o rletoascii

rletogray:	rletogray.o
	CC LDFLAGS rletogray.o LIBRLE LIBES -o rletogray

rletopaint:	rletopaint.o
	CC LDFLAGS rletopaint.o LIBRLE LIBES -o rletopaint

rletops:	rletops.o
	CC LDFLAGS rletops.o LIBRLE LIBES -o rletops

rletoraw:	rletoraw.o
	CC LDFLAGS rletoraw.o LIBRLE LIBES -o rletoraw

targatorle:	targatorle.o
	CC LDFLAGS targatorle.o LIBRLE LIBES -o targatorle

wasatchrle:	wasatchrle.o
	CC LDFLAGS wasatchrle.o LIBRLE LIBES -o wasatchrle

rletogif:	rletogif.o gifencod.o compgif.o
	CC LDFLAGS rletogif.o gifencod.o compgif.o LIBRLE LIBES -o rletogif

rletoabA62:	rletoabA62.o rle.o
	CC LDFLAGS rletoabA62.o rle.o LIBRLE LIBES -o rletoabA62

#include "../Cakefile.rules"
