
if WITH_TK
BWISH=bwish
blib_LIBS = \
	${TCLCAD} \
	${DM} \
	${ITK} \
	${ITCL} \
	${TK} \
	${TCL} \
	${TERMIO}
else
BWISH=
blib_LIBS = \
	${TCLCAD} \
	${DM} \
	${ITCL} \
	${TCL} \
	${TERMIO}
endif

bin_PROGRAMS = \
	btclsh \
	${BWISH}

bwish_CFLAGS = \
	${STRICT_FLAGS} \
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

bwish_LDADD = ${blib_LIBS}

btclsh_CFLAGS = \
	${STRICT_FLAGS} \
	${ITCL_CPPFLAGS} \
	${TCL_CPPFLAGS}

btclsh_SOURCES = \
	cmd.c \
	input.c \
	main.c \
	tcl.c

btclsh_LDADD = \
	${blib_LIBS}

EXTRA_DIST = \
	consoleMain.c \
	unixMain.c \
	cadAppInit.c \
	winMain.c \
	CMakeLists.txt

DEPENDS = src/libdm

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bwish_OBJECTS) \
	$(btclsh_OBJECTS) \
	$(bin_PROGRAMS)
