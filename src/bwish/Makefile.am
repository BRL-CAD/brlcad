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

bwish_CFLAGS = ${X_CFLAGS} -DBWISH=1

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

# hack to work around the irix compiler crashing on the libtool btclsh
# relink attempt when we get to tclscripts.
.libs/lt-btclsh: btclsh
	@if test -f "$(top_builddir)/src/bwish/.libs/btclsh" ; then \
		$(CP) $(top_builddir)/src/bwish/.libs/btclsh $(top_builddir)/src/bwish/.libs/lt-btclsh ; \
	fi

all-am: $(BTCLSH_DEPENDENCY)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bwish_OBJECTS) \
	$(btclsh_OBJECTS) \
	$(bin_PROGRAMS)
