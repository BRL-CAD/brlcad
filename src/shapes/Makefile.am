
bin_PROGRAMS = \
	bolt \
	coil \
	fence \
	gastank \
	handle \
	human \
	picket_fence \
	tire \
	window \
	window_frame \
	wire

AM_CPPFLAGS = \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

LDADD = ${WDB}

bolt_SOURCES = bolt.c
coil_SOURCES = coil.c
fence_SOURCES = fence.c
gastank_SOURCES = gastank.c
handle_SOURCES = handle.c
human_SOURCES = human.c
human_LDADD = \
	${GED} \
	${WDB}
picket_fence_SOURCES = picket_fence.c

tire_SOURCES = tire.c
tire_LDADD = \
	${GED} \
	${WDB}

window_SOURCES = window.c
window_frame_SOURCES = window_frame.c
wire_SOURCES = wire.c

noinst_HEADERS = fence.h

EXTRA_DIST = CMakeLists.txt

DEPENDS = src/libged src/libwdb

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bolt_OBJECTS) \
	$(coil_OBJECTS) \
	$(fence_OBJECTS) \
	$(gastank_OBJECTS) \
	$(handle_OBJECTS) \
	$(human_OBJECTS) \
	$(picket_fence_OBJECTS) \
	$(tire_OBJECTS) \
	$(window_OBJECTS) \
	$(window_frame_OBJECTS) \
	$(wire_OBJECTS) \
	$(bin_PROGRAMS)
