EXTRA_PROGRAMS = step-g

# FIXME: these AM_CPPFLAGS are redundant with step_g_CFLAGS and the
# latter should be preferred unless there's an overriding convenience
# (like dozens all with the same flags in this dir)
#AM_CPPFLAGS = ${STEP_CPPFLAGS} ${TCL_CPPFLAGS} ${OPENNURBS_CPPFLAGS}

step_g_SOURCES = \
	AdvancedBrepShapeRepresentation.cpp \
	BRLCADWrapper.cpp \
	STEPWrapper.cpp \
	step-g.cpp

fedex_hdrs = \
	SdaiCONFIG_CONTROL_DESIGN.h \
	Sdaiclasses.h \
	schema.h

# FIXME: these should be compiled into a noinst_LTLIBRARIES
# convenience library and linked to step-g instead of being bundled
# directly as a SOURCES.
fedex_srcs = \
	SdaiAll.cc \
	SdaiCONFIG_CONTROL_DESIGN.cc \
	SdaiCONFIG_CONTROL_DESIGN.init.cc \
	compstructs.cc \
	schema.cc

#	$(fedex_hdrs)
noinst_HEADERS = \
	AdvancedBrepShapeRepresentation.h \
	BRLCADWrapper.h \
	STEPWrapper.h

# per-program CPPFLAGS wasn't added until automake 1.7
step_g_CXXFLAGS = \
	${OPENNURBS_CPPFLAGS} \
	${STEP_CPPFLAGS} \
	${TCL_CPPFLAGS}

# FIXME: these declarations are required for proper distchecking if
# the files do not exist before configure is run.
#BUILT_SOURCES = \
#	$(fedex_srcs) \
#	$(fedex_headers)

step_g_LDADD = \
	${BU} \
	${WDB} \
	${RT} \
	${STEP_LIBS}

$(fedex_srcs):
	echo "Building fedex sources: ${fedex_srcs} ${fedex_hdrs}"
	$(STEP_FEDEX_PLUS) $(STEP_AP203)

$(fedex_hdrs): ${fedex_srcs}


##$(step_g_OBJECTS): $(fedex_srcs)

##CLEANFILES = $(fedex_srcs) $(fedex_hdrs)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(step_g_OBJECTS) \
	$(bin_PROGRAMS)
