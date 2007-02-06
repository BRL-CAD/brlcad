# $Id$

if WITH_PROE
PROE_DIR = ProEngineer
else !WITH_PROE
PROE_DIR =
endif

if BUILD_UG
UG_DIR = Unigraphics
else !BUILD_UG
UG_DIR =
endif

if BUILD_EF
EF_DIR = EndgameFramework
else !BUILD_EF
EF_DIR =
endif

SUBDIRS = \
	$(EF_DIR) \
	$(PROE_DIR) \
	$(UG_DIR)

EXTRA_DIST = README

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
