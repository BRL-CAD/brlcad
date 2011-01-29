# Minimal directories required for any BRL-CAD functionality

# The other subdirectory does not share (at least, not universally)
# certain definitions common to all other directories in BRL-CAD, 
# so add this first.  It is always on in any case, and if any src/other
# directory requires BRL-CAD specific directories or definitions they
# will need to call them out explicitly
ADD_SUBDIRECTORY(other)

# Define some common definitions and includes here that are used
# in all BRL-CAD builds

include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	)
add_definitions(
	-DHAVE_CONFIG_H
	-DBRLCADBUILD=1
	)

IF(MSVC)
   add_definitions(
	   -DBRLCAD_DLL
	   -D_CRT_SECURE_NO_WARNINGS
	   -DCMAKE_HEADERS
   )
ENDIF(MSVC)

# We will always need the required directories,
# so go ahead and add them

SET(required_dirs
	libbu 
	libbn 
	libsysv 
	librt 
	external
)

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
# The interaction of MSVC and Boost looks problematic - 
# since we're not using libpc at the moment, conditionalize
IF(NOT MSVC)
	SET(LIBPC libpc)
ENDIF(NOT MSVC)
# anything not included from above
SET(remaining_dirs 
	libcursor 
	libdm 
	libfft 
	libicv
	libmultispectral 
	${LIBPC}
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
	shapes 
	sig 
	tab 
	tclscripts 
	util 
	vdeck
)

# FIXME: Ideally these should all be in the above list,
# need to fix them on Windows.
SET(non_win32_dirs
	remrt
	rttherm
	)

#######################
# put it all together #
#######################

IF(BRLCAD-ENABLE_BENCHMARK_ONLY)
		  FOREACH(subdir ${benchmark_dirs})
					 ADD_SUBDIRECTORY(${subdir})
		  ENDFOREACH(subdir ${benchmark_dirs})
		  IF(BRLCAD-ENABLE_RTSERVER_ONLY AND BRLCAD-ENABLE_RTSERVER)
					 ADD_SUBDIRECTORY(librtserver)
		  ENDIF(BRLCAD-ENABLE_RTSERVER_ONLY AND BRLCAD-ENABLE_RTSERVER)
ENDIF(BRLCAD-ENABLE_BENCHMARK_ONLY)

IF(BRLCAD-ENABLE_RTSERVER_ONLY AND BRLCAD-ENABLE_RTSERVER)
		  ADD_SUBDIRECTORY(librtserver)
ENDIF(BRLCAD-ENABLE_RTSERVER_ONLY AND BRLCAD-ENABLE_RTSERVER)

IF(NOT BRLCAD-ENABLE_BENCHMARK_ONLY AND NOT BRLCAD-ENABLE_RTSERVER_ONLY)
		  FOREACH(subdir ${benchmark_dirs})
					 ADD_SUBDIRECTORY(${subdir})
		  ENDFOREACH(subdir ${benchmark_dirs})
		  IF(BRLCAD-ENABLE_RTSERVER)
		     ADD_SUBDIRECTORY(librtserver)
		  ENDIF(BRLCAD-ENABLE_RTSERVER)
		  FOREACH(subdir ${remaining_dirs})
					 ADD_SUBDIRECTORY(${subdir})
		  ENDFOREACH(subdir ${remaining_dirs})
		  IF(NOT WIN32)
			  FOREACH(subdir ${non_win32_dirs})
					 ADD_SUBDIRECTORY(${subdir})
			  ENDFOREACH(subdir ${non_win32_dirs})
		  ENDIF(NOT WIN32)
ENDIF(NOT BRLCAD-ENABLE_BENCHMARK_ONLY AND NOT BRLCAD-ENABLE_RTSERVER_ONLY)
	
