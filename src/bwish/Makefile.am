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

blib_LIBS = \
	${TCLCAD} \
	${RT} \
	${DM} \
	${FB} \
	${BN} \
	${BU} \
	${ITK} \
	${ITCL} \
	${TK} \
	${TCL} \
	${TERMIO}

bwish_LDADD = \
	${blib_LIBS}

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
	${blib_LIBS}

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bwish_OBJECTS) \
	$(btclsh_OBJECTS) \
	$(bin_PROGRAMS)
