lib_LTLIBRARIES = libfft.la

noinst_PROGRAMS = fftc ifftc
SRCS = fftfast.c splitdit.c ditsplit.c
BUILT_SOURCES = rfft16.c rfft32.c rfft64.c rfft128.c rfft256.c irfft16.c irfft32.c irfft64.c irfft128.c irfft256.c vers.c
libfft_la_SOURCES = $(SRCS) vers.c


fftc_SOURCES = fftc.c splitditc.c
fftc_LDADD = -lm

ifftc_SOURCES = ifftc.c ditsplitc.c
ifftc_LDADD = -lm

rfft%.c:	fftc
	./fftc % > rfft%.c

irfft%.c:	ifftc
	./fftc % > irfft%.c

CFLAGS=-I$(top_srcdir)/h
libfft_la_LDFLAGS = -version-info 19:1

vers.c: $(SRCS) $(BUILT_SOURCES)
	$(top_srcdir)/sh/newvers.sh fft_version "BRL-CAD FFT Library"
