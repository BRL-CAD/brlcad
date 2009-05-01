bin_PROGRAMS = step-g

fedex_headers = \
	SdaiCONFIG_CONTROL_DESIGN.h \
	Sdaiclasses.h \
	schema.h

fedex_srcs = \
	SdaiAll.cc \
	SdaiCONFIG_CONTROL_DESIGN.cc \
	SdaiCONFIG_CONTROL_DESIGN.init.cc \
	compstructs.cc \
	schema.cc

#	$(fedex_srcs)

noinst_HEADERS = \
	$(fedex_headers) \
	AdvancedBrepShapeRepresentation.h \
	BRLCADWrapper.h \
	STEPWrapper.h

step_g_SOURCES = \
	${top_srcdir}/src/other/step/src/test/needFunc.cc \
	AdvancedBrepShapeRepresentation.cpp \
	BRLCADWrapper.cpp \
	STEPWrapper.cpp \
	step-g.cpp

# per-program CPPFLAGS wasn't added until automake 1.7
step_g_CFLAGS = \
	${STEP_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	${OPENNURBS_CPPFLAGS}

BUILT_SOURCES = \
	$(fedex_srcs) \
	$(fedex_headers)

step_g_DEPENDENCIES = \
	$(STEP_LIBS) \
	$(STEP_FEDEX_PLUS) \
	$(STEP_AP203)

step_g_LDADD = \
	${BU} \
	${WDB} \
	${RT} \
	${STEP_LIBS}

$(STEP_FEDEX_PLUS):

$(fedex_sources): $(STEP_FEDEX_PLUS)
	echo "Building fedex_sources: ${fedex_sources}"
	$(STEP_FEDEX_PLUS) $(STEP_AP203)

$(step_g_OBJECTS):	$(fedex_sources)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(step_g_OBJECTS) \
	$(bin_PROGRAMS)

CLEANFILES=$(fedex_sources)
