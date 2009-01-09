
bin_PROGRAMS = \
	bolt \
	fence \
	gastank \
	handle \
	tire \
	window \
	winfrm \
	wire

AM_CPPFLAGS = ${TCL_CPPFLAGS}
LDADD = ${WDB}

bolt_SOURCES = bolt.c
fence_SOURCES = fence.c
gastank_SOURCES = gastank.c
handle_SOURCES = handle.c

tire_SOURCES = tire.c
tire_LDADD = \
	${GED} \
	${WDB}

window_SOURCES = window.c
winfrm_SOURCES = winfrm.c
wire_SOURCES = wire.c

noinst_HEADERS = fence.h

man_MANS = \
	bolt.1 \
	gastank.1 \
	handle.1 \
	window.1 \
	winfrm.1 \
	wire.1

EXTRA_DIST = $(man_MANS)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bolt_OBJECTS) \
	$(fence_OBJECTS) \
	$(gastank_OBJECTS) \
	$(handle_OBJECTS) \
	$(tire_OBJECTS) \
	$(window_OBJECTS) \
	$(winfrm_OBJECTS) \
	$(wire_OBJECTS) \
	$(bin_PROGRAMS)
