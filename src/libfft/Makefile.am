# $Id$

lib_LTLIBRARIES = libfft.la

AM_CFLAGS=-I$(top_srcdir)/h

noinst_PROGRAMS = \
	fftc \
	ifftc

SRCS = \
	fftfast.c \
	splitdit.c \
	ditsplit.c

BUILT_SOURCES = \
	irfft128.c \
	irfft16.c \
	irfft256.c \
	irfft32.c \
	irfft512.c \
	irfft64.c \
	rfft128.c \
	rfft16.c \
	rfft256.c \
	rfft32.c \
	rfft512.c \
	rfft64.c \
	vers.c

libfft_la_SOURCES = \
	$(SRCS) \
	$(BUILT_SOURCES)

noinst_HEADERS = \
	complex.h \
	fft.h

libfft_la_LDFLAGS = -version-info 19:1

fftc_SOURCES = \
	fftc.c \
	splitditc.c
fftc_LDADD = -lm

ifftc_SOURCES = \
	ditsplitc.c \
	ifftc.c
ifftc_LDADD = -lm

rfft16.c:	fftc
	./fftc 16 > rfft16.c

rfft32.c:	fftc
	./fftc 32 > rfft32.c

rfft64.c:	fftc
	./fftc 64 > rfft64.c

rfft128.c:	fftc
	./fftc 128 > rfft128.c

rfft256.c:	fftc
	./fftc 256 > rfft256.c

rfft512.c:	fftc
	./fftc 512 > rfft512.c

irfft16.c:	ifftc
	./ifftc 16 > irfft16.c

irfft32.c:	ifftc
	./ifftc 32 > irfft32.c

irfft64.c:	ifftc
	./ifftc 64 > irfft64.c

irfft128.c:	ifftc
	./ifftc 128 > irfft128.c

irfft256.c:	ifftc
	./ifftc 256 > irfft256.c

irfft512.c:	ifftc
	./ifftc 512 > irfft512.c


vers.c: $(SRCS)
	sh $(top_srcdir)/sh/vers.sh fft_version "BRL-CAD FFT Library" > $@

clean-local:
	${RM} ${BUILT_SOURCES}
