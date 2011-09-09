if(0)
#  if BUILD_CUBIT
#  CUBIT_DIR = Cubit
#  CUBIT_FILES =
#  else !BUILD_CUBIT
#  CUBIT_DIR =
#  CUBIT_FILES = \
#  	Cubit/Makefile.am \
#  	Cubit/Makefile.in \
#  	Cubit/g-sat.cpp
#  endif
#
#  if BUILD_EF
#  EF_DIR = EndgameFramework
#  EF_FILES =
#  else !BUILD_EF
#  EF_DIR =
#  EF_FILES = \
#  	EndgameFramework/Makefile.am \
#  	EndgameFramework/Makefile.in
#  endif
#
#  if WITH_PROE
#  PROE_DIR = ProEngineer
#  PROE_FILES =
#  else !WITH_PROE
#  PROE_DIR =
#  PROE_FILES = \
#  	ProEngineer/Makefile.am \
#  	ProEngineer/Makefile.in \
#  	ProEngineer/install.doc \
#  	ProEngineer/mk.in \
#  	ProEngineer/proe-brl.c \
#  	ProEngineer/proe_brl.res \
#  	ProEngineer/proe_brl_error.res \
#  	ProEngineer/proe_brl_gen_error.res \
#  	ProEngineer/protk.dat.in \
#  	ProEngineer/usermsg.txt
#  endif
#
#  if BUILD_UG
#  UG_DIR = Unigraphics
#  UG_FILES =
#  else !BUILD_UG
#  UG_DIR =
#  UG_FILES = \
#  	Unigraphics/Makefile.am \
#  	Unigraphics/Makefile.in \
#  	Unigraphics/conv.h \
#  	Unigraphics/log.h \
#  	Unigraphics/ug-g.1 \
#  	Unigraphics/ug-g.c \
#  	Unigraphics/ug_misc.c \
#  	Unigraphics/ug_misc.h
#  endif
#
#  SUBDIRS = \
#  	$(CUBIT_DIR) \
#  	$(EF_DIR) \
#  	$(PROE_DIR) \
#  	$(UG_DIR)
#
#  EXTRA_DIST = \
#  	README \
#  	$(CUBIT_FILES) \
#  	$(EF_FILES) \
#  	$(PROE_FILES) \
#  	$(UG_FILES)
#
#  DIST_SUBDIRS = \
#  	Cubit \
#  	EndgameFramework \
#  	ProEngineer \
#  	Unigraphics
#
#  include $(top_srcdir)/misc/Makefile.defs
endif (0)

