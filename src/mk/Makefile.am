# $Id$

#define SRCDIR	mk

bin_PROGRAMS = \
	mk_bolt \
	mk_handle \
	mk_window \
	mk_winfrm \
	mk_gastank \
	mk_wire

AM_CPPFLAGS = ${TCL_CPPFLAGS}
LDADD = \
	${WDB} \
	${RT} \
	${BN} \
	${BU} \
	${SYSV}

mk_bolt_SOURCES = mk_bolt.c
mk_gastank_SOURCES = mk_gastank.c
mk_handle_SOURCES = mk_handle.c
mk_window_SOURCES = mk_window.c
mk_winfrm_SOURCES = mk_winfrm.c
mk_wire_SOURCES = mk_wire.c

man_MANS = \
	mk_bolt.1 \
	mk_gastank.1 \
	mk_handle.1 \
	mk_window.1 \
	mk_winfrm.1 \
	mk_wire.1

EXTRA_DIST = $(man_MANS)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(mk_bolt_OBJECTS) \
	$(mk_handle_OBJECTS) \
	$(mk_window_OBJECTS) \
	$(mk_winfrm_OBJECTS) \
	$(mk_gastank_OBJECTS) \
	$(mk_wire_OBJECTS) \
	$(bin_PROGRAMS)
