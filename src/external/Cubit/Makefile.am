
#variables defined
#HOME = /d/mgillich
CGM_DIR = ${HOME}/CGM-10.1

bin_PROGRAMS = g-sat
g_sat_SOURCES = g-sat.cpp

AM_CPPFLAGS = \
	-I${HOME}/local/CGM-10.1/include \
	-I${HOME}/CubitLink

g_sat_LDFLAGS = \
	-Wl,-rpath,${HOME}/local/claro-10.1/bin \
	-Wl,-rpath,${HOME}/local/claro-10.1/libs \
	-L${HOME}/local/claro-10.1/bin \
	-L${HOME}/local/claro-10.1/libs \
	-L${CGM_DIR}/geom/virtual \
	-L${CGM_DIR}/geom/facetbool \
	-L${CGM_DIR}/geom/facet \
	-L${CGM_DIR}/geom/Cholla \
	-L${CGM_DIR}/geom/ACIS \
	-L${CGM_DIR}/geom \
	-L${CGM_DIR}/util

g_sat_LDADD = ${WDB} \
	-lcubit_geom \
	-lacisstep \
	-lxstep \
	-lacisiges \
	-lxiges \
	-lSPAXAssemblyRep \
	-lSPAXInterop \
	-lSPAXAcisBase \
	-lSPAXDefaultGeometryRep \
	-lSPAXBase \
	-lxacis2k \
	-lxcore2k \
	-lSpaAVis \
	-lSpaAWarp \
	-lSpaASurf \
	-lSpaALops \
	-lSpaABlend \
	-lSpaACIS \
	-lSpaBase \
	-lcubiti19 \
	-lacisstep \
	-lxstep \
	-lacisiges \
	-lxiges \
	-lSPAXAssemblyRep \
	-lSPAXInterop \
	-lSPAXAcisBase \
	-lSPAXDefaultGeometryRep \
	-lSPAXBase \
	-lxacis2k \
	-lxcore2k \
	-lSpaAVis \
	-lSpaAWarp \
	-lSpaASurf \
	-lSpaALops \
	-lSpaABlend \
	-lSpaACIS \
	-lSpaBase

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(g_sat_OBJECTS) \
	$(bin_PROGRAMS)
