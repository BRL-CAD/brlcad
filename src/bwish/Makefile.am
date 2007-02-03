# $Id$

bin_PROGRAMS = \
	btclsh \
	bwish

# static is required to avoid MIPSpro 7.3 long rpath linker bug
if LINK_STATIC_REQUIRED
bwish_LDFLAGS = -static
endif

bwish_CFLAGS = \
	-DBWISH=1 \
	${X_CFLAGS} \
	${ITCL_CPPFLAGS} \
	${ITK_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	${TK_CPPFLAGS}

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

btclsh_CFLAGS = \
	${ITCL_CPPFLAGS} \
	${TCL_CPPFLAGS}

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
