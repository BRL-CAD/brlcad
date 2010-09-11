
#lib_LTLIBRARIES = libbrlcad.la
#noinst_LTLIBRARIES = libbrlcad_nil.la

#libbrlcad_la_SOURCES =
#libbrlcad_la_LDFLAGS = -version-info 19:1
#libbrlcad_la_LIBADD = libbrlcad_nil.la

#if BUILD_OPENNURBS
#ON_NIL = $(top_builddir)/src/other/openNURBS/libopenNURBS_nil.la
#else
#ON_NIL =
#endif

#if BUILD_BOOST
#PCDIR = libpc
#else
#PCDIR =
#endif

#libbrlcad_nil_la_SOURCES =
#libbrlcad_nil_la_LIBADD = \
#	$(top_builddir)/src/libbu/libbu_nil.la \
#	$(top_builddir)/src/libbn/libbn_nil.la \
#	$(top_builddir)/src/librt/librt_nil.la \
#	$(top_builddir)/src/libwdb/libwdb_nil.la \
#	$(top_builddir)/src/libged/libged_nil.la \
#	${ON_NIL} \
#	${TCL}

# Minimal directories required for any BRL-CAD
# functionality
SET(required_dirs
	other 
	libbu 
	libbn 
	libsysv 
	librt 
	external
)

# We will always need the required directories,
# so go ahead and add them
FOREACH(subdir ${required_dirs})
	ADD_SUBDIRECTORY(${subdir})
ENDFOREACH(subdir ${required_dirs})

# Directories needed to run benchmarks
SET(benchmark_dirs
	libpkg 
	libfb 
	libwdb 
	libgcv 
	libanalyze 
	libged 
	liboptical 
	liborle 
	conv 
	fbserv 
	rt
)

# anything not included from above
SET(remaining_dirs 
	libcursor 
	libdm 
	libfft 
	libicv 
	libmultispectral 
	libtclcad 
	libtermio 
	adrt 
	anim 
	archer 
	brlman 
	burst 
	bwish 
	canon 
	fb 
	vfont 
	fbed 
	gtools 
	halftone 
	irprep 
	java 
	lgt 
	mged 
	nirt 
	proc-db 
	remrt 
	rttherm 
	shapes 
	sig 
	tab 
	tclscripts 
	util 
	vas4 
	vdeck
)

#######################
# put it all together #
#######################

IF(BRLCAD-BUILD_BENCHMARK_ONLY)
		  FOREACH(subdir ${benchmark_dirs})
					 ADD_SUBDIRECTORY(${subdir})
		  ENDFOREACH(subdir ${benchmark_dirs})
		  IF(BRLCAD-BUILD_RTSERVER_ONLY)
					 ADD_SUBDIRECTORY(librtserver)
		  ENDIF(BRLCAD-BUILD_RTSERVER_ONLY)
ENDIF(BRLCAD-BUILD_BENCHMARK_ONLY)

IF(BRLCAD-BUILD_RTSERVER_ONLY)
		  ADD_SUBDIRECTORY(librtserver)
ENDIF(BRLCAD-BUILD_RTSERVER_ONLY)

IF(NOT BRLCAD-BUILD_BENCHMARK_ONLY AND NOT BRLCAD-BUILD_RTSERVER_ONLY)
		  FOREACH(subdir ${benchmark_dirs})
					 ADD_SUBDIRECTORY(${subdir})
		  ENDFOREACH(subdir ${benchmark_dirs})
		  ADD_SUBDIRECTORY(librtserver)
		  FOREACH(subdir ${remaining_dirs})
					 ADD_SUBDIRECTORY(${subdir})
		  ENDFOREACH(subdir ${remaining_dirs})
ENDIF(NOT BRLCAD-BUILD_BENCHMARK_ONLY AND NOT BRLCAD-BUILD_RTSERVER_ONLY)
	
