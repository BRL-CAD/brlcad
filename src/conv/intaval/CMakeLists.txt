include_directories(
	${ZLIB_INCLUDE_DIR}
	${OPENNURBS_INCLUDE_DIR}
	${REGEX_INCLUDE_DIR}
	${TCL_INCLUDE_DIRS}
)

IF(MSVC)
add_definitions(
    -DON_DLL_IMPORTS
)
ENDIF(MSVC)

add_executable(tgf-g
    read_dra.cpp
    regtab.cpp
    tgf-g.cpp
    write_brl.cpp
)
target_link_libraries(tgf-g libwdb librt libbu)
INSTALL(TARGETS tgf-g RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
