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
	${TK_CPPFLAGS} \
	-I$(top_srcdir)/src/other/tcl/generic \
	-I$(top_srcdir)/src/other/tcl/unix \
	-I$(top_srcdir)/src/other/tk/generic \
	-I$(top_srcdir)/src/other/tk/unix

bwish_SOURCES = \
	cmd.c \
	input.c \
	main.c \
	tcl.c

blib_LIBS = \
	${TCLCAD} ${TCLCAD_LIBS} \
	${RT} ${RT_LIBS} \
	${DM} ${DM_LIBS} \
	${FB} ${FB_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${PKG} ${PKG_LIBS} \
	${ITK} \
	${ITCL} \
	${TK} \
	${TCL} \
	${TERMIO} ${TERMIO_LIBS} \
	${SYSV} ${SYSV_LIBS}

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

DEPS=src/libdm

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bwish_OBJECTS) \
	$(btclsh_OBJECTS) \
	$(bin_PROGRAMS)
