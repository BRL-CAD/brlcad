# Minimal directories required for any BRL-CAD functionality

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
	   -D_CRT_SECURE_NO_WARNINGS
	   -DCMAKE_HEADERS
   )
   # FIXME: these should be proper library checks, not based on MSVC "platform"
   SET(WINSOCK_LIB ws2_32.lib)
   SET(PSAPI_LIB psapi.lib)
   SET(RPCRT_LIB rpcrt4.lib)
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
	#SET(LIBPC libpc)
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
FOREACH(subdir ${benchmark_dirs})
	ADD_SUBDIRECTORY(${subdir})
ENDFOREACH(subdir ${benchmark_dirs})
IF(BRLCAD-ENABLE_RTSERVER)
	ADD_SUBDIRECTORY(librtserver)
ELSE(BRLCAD-ENABLE_RTSERVER)
	CMAKEFILES(librtserver)
ENDIF(BRLCAD-ENABLE_RTSERVER)
FOREACH(subdir ${remaining_dirs})
	ADD_SUBDIRECTORY(${subdir})
ENDFOREACH(subdir ${remaining_dirs})
IF(NOT WIN32)
	FOREACH(subdir ${non_win32_dirs})
		ADD_SUBDIRECTORY(${subdir})
	ENDFOREACH(subdir ${non_win32_dirs})
ENDIF(NOT WIN32)

DISTCHECK_IGNORE_ITEM(README)
DISTCHECK_IGNORE_ITEM(other)
