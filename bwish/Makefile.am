# $Id$

bin_PROGRAMS = bwish btclsh

bwish_SOURCES = cmd.c input.c main.c tcl.c
bwish_LDADD = @TCL_LIBS@ ../libbu/libbu.la ../libsysv/libsysv.la ../libtermio/libtermio.la @ITCL_LIBS@ ../libbn/libbn.la @PTHREAD_LIBS@ ../librt/librt.la

CFLAGS = -I$(top_srcdir)/h

btclsh_SOURCES = $(bwish_SOURCES)
btclsh_LDADD = $(bwish_LDADD)
