EXTRA_PROGRAMS = step-g

step_g_SOURCES = \
	AdvancedBrepShapeRepresentation.cpp \
	BRLCADWrapper.cpp \
	STEPWrapper.cpp \
	needFunc.cc \
	step-g.cpp

fedex_hdrs = \
	SdaiCONFIG_CONTROL_DESIGN.h \
	Sdaiclasses.h \
	schema.h

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
	STEPWrapper.h \
	needFunc.h

# per-program CPPFLAGS wasn't added until automake 1.7
step_g_CFLAGS = \
	${OPENNURBS_CPPFLAGS} \
	${STEP_CPPFLAGS} \
	${TCL_CPPFLAGS}

#BUILT_SOURCES = \
#	$(fedex_srcs) \
#	$(fedex_headers)

step_g_LDADD = \
	${BU} \
	${WDB} \
	${RT} \
	${STEP}

$(fedex_srcs):
	echo "Building fedex sources: ${fedex_srcs} ${fedex_hdrs}"
	$(STEP_FEDEX_PLUS) $(STEP_AP203)

$(fedex_hdrs): ${fedex_srcs}


# $(step_g_OBJECTS): $(fedex_srcs)

CLEANFILES = $(fedex_srcs)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(step_g_OBJECTS) \
	$(bin_PROGRAMS)
