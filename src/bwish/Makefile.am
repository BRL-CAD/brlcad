# $Id$

bin_PROGRAMS = \
	btclsh \
	bwish

# static is required to avoid MIPSpro 7.3 long rpath linker bug
if LINK_STATIC_REQUIRED
bwish_LDFLAGS = -static
endif

bwish_CFLAGS = ${X_CFLAGS} -DBWISH=1

bwish_SOURCES = \
	cmd.c \
	input.c \
	main.c \
	tcl.c

bwish_LDADD = \
	${TK} \
	${TCL} \
	${ITK} \
	${ITCL} \
	${TCLCAD} \
	${DM} \
	${FB} \
	${LIBRT} \
	${TERMIO} \
	${BN} \
	${BU}

# static is required to avoid MIPSpro 7.3 long rpath linker bug
if LINK_STATIC_REQUIRED
btclsh_LDFLAGS = -static
endif

btclsh_SOURCES = \
	cmd.c \
	input.c \
	main.c \
	tcl.c

btclsh_LDADD = \
	${LIBRT} \
	${ITCL} \
	${TERMIO} \
	${TCL} \
	${TCLCAD} \
	${BN} \
	${BU}


include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bwish_OBJECTS) \
	$(btclsh_OBJECTS) \
	$(bin_PROGRAMS)
