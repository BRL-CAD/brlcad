# $Id$

bin_PROGRAMS = bwish btclsh
AM_CFLAGS = -I$(top_srcdir)/h

bwish_SOURCES = cmd.c input.c main.c tcl.c
bwish_LDADD = @FB@ @ITCL_LIBS@ @RT@ @SYSV@ @TERMIO@ @WDB@

btclsh_SOURCES = $(bwish_SOURCES)
btclsh_LDADD = @ITCL_LIBS@ @PKG@ @RT@ @SYSV@ @TERMIO@ @WDB@

