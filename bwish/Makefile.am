# $Id$

bin_PROGRAMS = bwish btclsh
AM_CFLAGS = -I$(top_srcdir)/h

bwish_SOURCES = cmd.c input.c main.c tcl.c
bwish_LDADD = @FB@ @DM@ @ITK_LIBS@ @TK_LIBS@ @TCLCAD@ @ITCL_LIBS@ @SYSV@ @TERMIO@ @WDB@
bwish_CFLAGS = -DBWISH=1

btclsh_SOURCES = cmd.c input.c main.c tcl.c
btclsh_LDADD = @ITCL_LIBS@ @PKG@ @RT@ @SYSV@ @TERMIO@ @WDB@

