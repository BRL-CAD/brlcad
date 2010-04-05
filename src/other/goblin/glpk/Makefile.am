
lib_LTLIBRARIES = libglpk.la
noinst_LTLIBRARIES = libglpk_nil.la

GLPK_INC = \
	glpavl.h \
	glpbcs.h \
	glpchol.h \
	glpdmp.h \
	glphbsm.h \
	glpies.h \
	glpinv.h \
	glpipm.h \
	glpk.h \
	glplib.h \
	glplpp.h \
	glplpt.h \
	glplpx.h \
	glpluf.h \
	glpmat.h \
	glpmip.h \
	glpmpl.h \
	glpmps.h \
	glpqmd.h \
	glpspm.h \
	glpspx.h \
	glpstr.h \
	glptsp.h

GLPK_SRC = \
	glpavl.c \
	glpbcs1.c \
	glpbcs2.c \
	glpchol.c \
	glpdmp.c \
	glphbsm.c \
	glpies1.c \
	glpies2.c \
	glpies3.c \
	glpinv.c \
	glpipm.c \
	glplib1.c \
	glplib2.c \
	glplib3.c \
	glplpp1.c \
	glplpp2.c \
	glplpt.c \
	glplpx1.c \
	glplpx2.c \
	glplpx3.c \
	glplpx4.c \
	glplpx5.c \
	glplpx6a.c \
	glplpx6b.c \
	glplpx6c.c \
	glplpx7.c \
	glplpx8a.c \
	glplpx8c.c \
	glplpx8d.c \
	glpluf.c \
	glpmat.c \
	glpmip1.c \
	glpmpl1.c \
	glpmpl2.c \
	glpmpl3.c \
	glpmpl4.c \
	glpmps.c \
	glpqmd.c \
	glpspm.c \
	glpspx1.c \
	glpspx2.c \
	glpstr.c \
	glptsp.c


glpkincludedir = $(includedir)/glpk
glpkinclude_HEADERS = $(GLPK_INC)

libglpk_nil_la_SOURCES = $(GLPK_SRC)

libglpk_la_SOURCES =
libglpk_la_LIBADD = libglpk_nil.la

EXTRA_DIST = \
        CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

