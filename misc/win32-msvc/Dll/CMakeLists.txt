# write brlcadversion.h
file(READ ../../../include/conf/MAJOR majorVersionNumber)
file(READ ../../../include/conf/MINOR minorVersionNumber)
file(READ ../../../include/conf/PATCH patchVersionNumber)

set(brlcadVersionContent
    "#define BRLCAD_LIB_MAJOR ${majorVersionNumber}"
    "#define BRLCAD_LIB_MINOR ${minorVersionNumber}"
    "#define BRLCAD_LIB_PATCH ${patchVersionNumber}"
)

file(WRITE brlcadversion.h
    ${brlcadVersionContent}
)

file(WRITE ../../../../rt^3/include/brlcad/brlcadversion.h
    ${brlcadVersionContent}
)


# for all targets
include_directories(
    ../../../include
    ../../../src/other/libregex
    ../../../src/other/tcl/generic
)


# build the BrlcadCore.dll
set(BRLCADCORE_SOURCES
    BrlcadCore.def
    BrlcadCore.rc
)

add_library(BrlcadCore SHARED ${BRLCADCORE_SOURCES})

target_link_libraries(BrlcadCore
    libbn
    libbu
    libgcv
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


# build the brlcad.dll
set(BRLCAD_SOURCES
    TclDummies.c
    brlcad.def
    brlcad.rc
)

set(BRLCAD_HEADERS
    ../../../include/bn.h
    ../../../include/brlcad.h
    ../../../include/brep.h
    ../../../include/bu.h
    ../../../include/common.h
    ../../../include/db.h
    ../../../include/db5.h
    ../../../include/magic.h
    ../../../include/opennurbs_ext.h
    ../../../include/nmg.h
    ../../../include/nurb.h
    ../../../include/pc.h
    ../../../include/raytrace.h
    ../../../include/rtgeom.h
    ../../../include/vmath.h
    ../../../include/wdb.h
)

if(MSVC)
    set(BRLCAD_HEADERS
        ${BRLCAD_HEADERS}
        ../../../include/pstdint.h
    )
endif(MSVC)

set(COREINTERFACE_SOURCES
    ../../../../rt^3/src/coreInterface/Arb8.cpp
    ../../../../rt^3/src/coreInterface/Combination.cpp
    ../../../../rt^3/src/coreInterface/Cone.cpp
    ../../../../rt^3/src/coreInterface/ConstDatabase.cpp
    ../../../../rt^3/src/coreInterface/Database.cpp
    ../../../../rt^3/src/coreInterface/Ellipsoid.cpp
    ../../../../rt^3/src/coreInterface/EllipticalTorus.cpp
    ../../../../rt^3/src/coreInterface/FileDatabase.cpp
    ../../../../rt^3/src/coreInterface/Halfspace.cpp
    ../../../../rt^3/src/coreInterface/HyperbolicCylinder.cpp
    ../../../../rt^3/src/coreInterface/Hyperboloid.cpp
    ../../../../rt^3/src/coreInterface/MemoryDatabase.cpp
    ../../../../rt^3/src/coreInterface/Object.cpp
    ../../../../rt^3/src/coreInterface/ParabolicCylinder.cpp
    ../../../../rt^3/src/coreInterface/Paraboloid.cpp
    ../../../../rt^3/src/coreInterface/Particle.cpp
    ../../../../rt^3/src/coreInterface/Torus.cpp
    ../../../../rt^3/src/coreInterface/Unknown.cpp
    ../../../../rt^3/src/coreInterface/globals.cpp
)

file(GLOB COREINTERFACE_HEADERS ../../../../rt^3/include/brlcad/*.h)

include_directories(
    ../../../../rt^3/include
    ../../../src/other/libz
    ../../../src/other/opennurbs
)

add_definitions(
    "-DBRLCAD_COREINTERFACE_EXPORT=__declspec(dllexport)"
)

add_library(brlcad SHARED
    ${BRLCAD_SOURCES}
    ${COREINTERFACE_SOURCES}
)

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

install(TARGETS brlcad
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${BRLCAD_HEADERS} DESTINATION include)

install(FILES ${COREINTERFACE_HEADERS} DESTINATION include/brlcad)
