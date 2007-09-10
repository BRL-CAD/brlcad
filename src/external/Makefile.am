# $Id$

EXTRA_DIST = README

if BUILD_CUBIT
CUBIT_DIR = Cubit
else !BUILD_CUBIT
CUBIT_DIR =
EXTRA_DIST += \
	Cubit/Makefile.am \
	Cubit/Makefile.in \
	Cubit/g-sat.cxx
endif

if BUILD_EF
EF_DIR = EndgameFramework
else !BUILD_EF
EF_DIR =
EXTRA_DIST += \
	EndgameFramework/Makefile.am \
	EndgameFramework/Makefile.in
endif

if WITH_PROE
PROE_DIR = ProEngineer
else !WITH_PROE
PROE_DIR =
EXTRA_DIST += \
	ProEngineer/Makefile.am \
	ProEngineer/Makefile.in \
	ProEngineer/install.doc \
	ProEngineer/mk.in \
	ProEngineer/proe-brl.c \
	ProEngineer/proe_brl.res \
	ProEngineer/proe_brl_error.res \
	ProEngineer/proe_brl_gen_error.res \
	ProEngineer/protk.dat.in \
	ProEngineer/usermsg.txt
endif

if BUILD_UG
UG_DIR = Unigraphics
else !BUILD_UG
UG_DIR =
EXTRA_DIST += \
	Unigraphics/Makefile.am \
	Unigraphics/Makefile.in \
	Unigraphics/conv.h \
	Unigraphics/log.h \
	Unigraphics/ug-g.1 \
	Unigraphics/ug-g.c \
	Unigraphics/ug_misc.c \
	Unigraphics/ug_misc.h
endif

SUBDIRS = \
	$(CUBIT_DIR) \
	$(EF_DIR) \
	$(PROE_DIR) \
	$(UG_DIR)

DIST_SUBDIRS = \
	Cubit \
	EndgameFramework \
	ProEngineer \
	Unigraphics

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
