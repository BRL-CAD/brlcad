include(generic/CMakeLists.txt)
include(libtommath/CMakeLists.txt)

set(TCL_SOURCES
    ${TCL_GENERIC_SOURCES}
    ${TCL_LIBTOMMATH_SOURCES}
)

set(TCL_HEADERS
    ${TCL_GENERIC_HEADERS}
)

include_directories(
    generic
    libtommath
)

if(MSVC)
    include(win/CMakeLists.txt)

    set(TCL_SOURCES
        ${TCL_SOURCES}
        ${TCL_WIN_SOURCES}
    )

    include_directories(
        win
    )
endif(MSVC)

add_definitions(
    -Dinline=__inline
    -DBUILD_tcl
    -DTCL_CFGVAL_ENCODING="UTF-8"
    -DTCL_PIPE_DLL="tclpip85.dll"
    -DCFG_RUNTIME_LIBDIR=0
    -DCFG_RUNTIME_BINDIR=0
    -DCFG_RUNTIME_SCRDIR=0
    -DCFG_RUNTIME_INCDIR=0
    -DCFG_RUNTIME_DOCDIR=0
    -DCFG_INSTALL_LIBDIR=0
    -DCFG_INSTALL_BINDIR=0
    -DCFG_INSTALL_SCRDIR=0
    -DCFG_INSTALL_INCDIR=0
    -DCFG_INSTALL_DOCDIR=0
)

add_library(tcl85 SHARED ${TCL_SOURCES})

if(MSVC)
    target_link_libraries(tcl85
            Ws2_32
    )
endif(MSVC)

set_property(TARGET tcl85 PROPERTY LINK_INTERFACE_LIBRARIES "")

if(MSVC)
    add_executable(tclpip85 win/stub16.c)
    set_target_properties(tclpip85 PROPERTIES SUFFIX ".dll")
endif(MSVC)

install(FILES ${TCL_HEADERS} DESTINATION include)
