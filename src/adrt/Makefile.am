
SUBDIRS = \
	bench \
	doc \
	libtexture \
	libtie \
	libtienet \
	librender \
	master \
	misc \
	rise \
	scripts \
	slave

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = adrt.h adrt_struct.h

if HAVE_DOXYGEN
doxygen:
	mkdir doxygen
	doxygen Doxyfile
endif

EXTRA_DIST = \
	AUTHORS \
	README

DEPS = src/libbu

include $(top_srcdir)/misc/Makefile.defs
