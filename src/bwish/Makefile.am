# $Id$

bin_PROGRAMS = bwish btclsh

bwish_SOURCES = cmd.c input.c main.c tcl.c
bwish_LINK = $(LIBTOOL) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) -o $@
bwish_LDADD = @TK_LIBS@ @ITK_LIBS@ @TCLCAD@ @ITCL_LIBS@ @FB@ @DM@ @SYSV@ @TERMIO@ @WDB@ @PNG_LIBS@
bwish_CFLAGS = -DBWISH=1 @X_CFLAGS@

btclsh_SOURCES = cmd.c input.c main.c tcl.c
btclsh_LDADD = @ITCL_LIBS@ @PKG@ @RT@ @SYSV@ @TERMIO@ @WDB@

