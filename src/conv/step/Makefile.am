bin_PROGRAMS = step-g

AM_CPPFLAGS = ${STEP_CPPFLAGS} ${TCL_CPPFLAGS} ${OPENNURBS_CPPFLAGS}

fedex_sources = \
	schema.cc schema.h compstructs.cc \
	Sdaiclasses.h SdaiAll.cc \
	SdaiCONFIG_CONTROL_DESIGN.h \
	SdaiCONFIG_CONTROL_DESIGN.init.cc \
	SdaiCONFIG_CONTROL_DESIGN.cc 	

noinst_HEADERS = \
	STEPWrapper.h \
	BRLCADWrapper.h \
	AdvancedBrepShapeRepresentation.h

step_g_SOURCES = \
	STEPWrapper.cpp \
	BRLCADWrapper.cpp \
	AdvancedBrepShapeRepresentation.cpp \
	step-g.cpp \
	${top_srcdir}/src/other/step/src/test/needFunc.cc \
	$(fedex_sources)

step_g_DEPENDENCIES = \
	$(STEP_LIBS) \
	$(STEP_FEDEX_PLUS) \
	$(STEP_AP203)

step_g_LDADD = ${BU} ${WDB} ${RT} ${STEP_LIBS}

include $(top_srcdir)/misc/Makefile.defs

$(STEP_FEDEX_PLUS):

$(fedex_sources): $(STEP_FEDEX_PLUS)
	echo "Building fedex_sources: ${fedex_sources}"
	$(STEP_FEDEX_PLUS) $(STEP_AP203)

$(step_g_OBJECTS):	$(fedex_sources)

FAST_OBJECTS = \
	$(step_g_OBJECTS) \
	$(bin_PROGRAMS)

CLEANFILES=$(fedex_sources)
