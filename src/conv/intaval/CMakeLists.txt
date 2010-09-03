include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BRLCAD_ZLIB_INCLUDE_DIR}
	${BRLCAD_OPENNURBS_INCLUDE_DIR}
	${BRLCAD_REGEX_INCLUDE_DIR}
	${TCL_INCLUDE_PATH}
)

IF(MSVC)
add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
    -DON_DLL_IMPORTS
)
ELSE(MSVC)
add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)
ENDIF(MSVC)

add_executable(tgf-g
    read_dra.cpp
    regtab.cpp
    tgf-g.cpp
    write_brl.cpp
)
target_link_libraries(tgf-g bu librt wdb)
INSTALL(TARGETS tgf-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})
