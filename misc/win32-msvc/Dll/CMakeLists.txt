# write brlcadrc_defines.h
file(READ ../../../include/conf/MAJOR majorVersionNumber)
file(READ ../../../include/conf/MINOR minorVersionNumber)
file(READ ../../../include/conf/PATCH patchVersionNumber)

file(WRITE brlcadrc_defines.h
    "#define BRLCAD_DLL_RC_MAJOR ${majorVersionNumber}"
    "#define BRLCAD_DLL_RC_MINOR ${minorVersionNumber}"
    "#define BRLCAD_DLL_RC_PATCH ${patchVersionNumber}"
)

# build the DLL
set(BRLCADCOMMON_SOURCES
    TclDummies.c
    brlcad.def
    brlcad.rc
)

include_directories(
    ../../../include
    ../../../src/other/tcl/generic
)

add_library(BrlcadCommon SHARED ${BRLCADCOMMON_SOURCES})

target_link_libraries(BrlcadCommon
	libbn.lib
	libbu
	libregex
	librt
	libsysv
	libwdb
	opennurbs
	libz
)
