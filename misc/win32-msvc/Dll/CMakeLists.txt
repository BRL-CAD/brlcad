# write brlcadrc_defines.h
file(READ ../../../include/conf/MAJOR majorVersionNumber)
file(READ ../../../include/conf/MINOR minorVersionNumber)
file(READ ../../../include/conf/PATCH patchVersionNumber)

file(WRITE brlcadrc_defines.h
    "#define BRLCAD_DLL_RC_MAJOR ${majorVersionNumber}"
    "#define BRLCAD_DLL_RC_MINOR ${minorVersionNumber}"
    "#define BRLCAD_DLL_RC_PATCH ${patchVersionNumber}"
)


# for all targets
include_directories(
    ../../../include
    ../../../src/other/tcl/generic
)


# build the brlcad.dll
set(BRLCAD_SOURCES
    TclDummies.c
    brlcad.def
    brlcad.rc
)

add_library(brlcad SHARED ${BRLCAD_SOURCES})

target_link_libraries(brlcad
    libbn
    libbu
    libged
    libregex
    librt
    libsysv
    libwdb
    libz
    openNURBS
)

set_property(TARGET brlcad PROPERTY LINK_INTERFACE_LIBRARIES "")


# build the BrlcadCore.dll
set(BRLCADCORE_SOURCES
    BrlcadCore.def
    BrlcadCore.rc
)

add_library(BrlcadCore SHARED ${BRLCADCORE_SOURCES})

target_link_libraries(BrlcadCore
    libbn
    libbu
    libged
    libregex
    librt
    libsysv
    libwdb
    libz
    openNURBS
    tcl85
)

set_property(TARGET BrlcadCore PROPERTY LINK_INTERFACE_LIBRARIES "")
