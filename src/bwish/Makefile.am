# $Id$

bin_PROGRAMS = \
	btclsh \
	bwish

bwish_SOURCES = \
	cmd.c \
	input.c \
	main.c \
	tcl.c

bwish_LINK = $(LIBTOOL) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) -o $@

bwish_CFLAGS = -DBWISH=1 ${X_CFLAGS}

bwish_LDADD = \
	${ITCL_LIBS} \
	${TCL} \
	${TK} \
	${ITK_LIBS} \
	${TCLCAD} \
	${FB} \
	${DM} \
	${SYSV} \
	${TERMIO} \
	${WDB} \
	${PNG}

btclsh_SOURCES = \
	cmd.c \
	input.c \
	main.c \
	tcl.c

btclsh_LDADD = \
	${ITCL_LIBS} \
	${TCL} \
	${PKG} \
	${LIBRT} \
	${SYSV} \
	${TERMIO} \
	${WDB}

