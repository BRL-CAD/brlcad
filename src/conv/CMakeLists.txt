ADD_SUBDIRECTORY(step)
ADD_SUBDIRECTORY(iges)
#ADD_SUBDIRECTORY(intaval)

include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BRLCAD_ZLIB_INCLUDE_DIR}
	${BRLCAD_REGEX_INCLUDE_DIR}
	${BRLCAD_OPENNURBS_INCLUDE_DIR}
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

add_executable(3dm-g 3dm/3dm-g.cpp)
set_target_properties(3dm-g PROPERTIES COMPILE_DEFINITIONS "OBJ_BREP=1")
target_link_libraries(3dm-g wdb ${BRLCAD_OPENNURBS_LIBRARY})
INSTALL(TARGETS 3dm-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(asc2g asc/asc2g.c)
target_link_libraries(asc2g bu librt wdb ged) 
INSTALL(TARGETS asc2g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g2asc asc/g2asc.c)
target_link_libraries(g2asc bu librt)
INSTALL(TARGETS g2asc RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(bot-bldxf dxf/bot-bldxf.c)
target_link_libraries(bot-bldxf bu librt)
INSTALL(TARGETS bot-bldxf RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(comgeom-g
    comgeom/cvt.c
    comgeom/f2a.c
    comgeom/mat.c
    comgeom/read.c
    comgeom/region.c
    comgeom/solid.c
    comgeom/tools.c
)
target_link_libraries(comgeom-g bu bn wdb)
INSTALL(TARGETS comgeom-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(dbupgrade dbupgrade.c)
target_link_libraries(dbupgrade bu librt wdb)
INSTALL(TARGETS dbupgrade RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(dxf-g dxf/dxf-g.c)
target_link_libraries(dxf-g bu bn librt wdb)
INSTALL(TARGETS dxf-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-dxf dxf/g-dxf.c)
target_link_libraries(g-dxf bu librt gcv)
INSTALL(TARGETS g-dxf RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(fast4-g fast4-g.c)
target_link_libraries(fast4-g bu librt wdb)
INSTALL(TARGETS fast4-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(jack-g jack/jack-g.c)
target_link_libraries(jack-g bu librt wdb)
INSTALL(TARGETS jack-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-jack jack/g-jack.c)
target_link_libraries(g-jack bu librt)
INSTALL(TARGETS g-jack RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-off off/g-off.c)
target_link_libraries(g-off bu librt)
INSTALL(TARGETS g-off RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(off-g off/off-g.c)
target_link_libraries(off-g bu librt wdb)
INSTALL(TARGETS off-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(stl-g stl/stl-g.c)
target_link_libraries(stl-g bu librt wdb)
INSTALL(TARGETS stl-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-stl stl/g-stl.c)
target_link_libraries(g-stl bu librt gcv)
INSTALL(TARGETS g-stl RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g4-g5 g4-g5.c)
target_link_libraries(g4-g5 bu librt)
INSTALL(TARGETS g4-g5 RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g5-g4 g5-g4.c)
target_link_libraries(g5-g4 bu librt wdb)
INSTALL(TARGETS g5-g4 RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-acad g-acad.c)
target_link_libraries(g-acad bu librt)
INSTALL(TARGETS g-acad RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-obj g-obj.c)
target_link_libraries(g-obj bu librt)
INSTALL(TARGETS g-obj RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(obj-g obj-g.c)
target_link_libraries(obj-g bu librt)
INSTALL(TARGETS obj-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(patch-g patch/patch-g.c)
target_link_libraries(patch-g bu librt wdb)
INSTALL(TARGETS patch-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(rpatch patch/rpatch.c)
target_link_libraries(rpatch bu librt)
INSTALL(TARGETS rpatch RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-tankill tankill/g-tankill.c)
target_link_libraries(g-tankill bu librt)
INSTALL(TARGETS g-tankill RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(tankill-g tankill/tankill-g.c)
target_link_libraries(tankill-g bu librt wdb)
INSTALL(TARGETS tankill-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-var g-var.c)
target_link_libraries(g-var bu librt)
INSTALL(TARGETS g-var RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-vrml g-vrml.c)
target_link_libraries(g-vrml bu librt)
INSTALL(TARGETS g-vrml RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-x3d g-x3d.c)
target_link_libraries(g-x3d bu librt)
INSTALL(TARGETS g-x3d RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(nastran-g nastran-g.c)
target_link_libraries(nastran-g bu librt wdb)
INSTALL(TARGETS nastran-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(ply-g ply-g.c)
target_link_libraries(ply-g bu librt)
INSTALL(TARGETS ply-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(proe-g proe-g.c)
target_link_libraries(proe-g bu librt wdb)
INSTALL(TARGETS proe-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(viewpoint-g viewpoint-g.c)
target_link_libraries(viewpoint-g bu librt wdb)
INSTALL(TARGETS viewpoint-g RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

add_executable(g-xxx_facets g-xxx_facets.c)
target_link_libraries(g-xxx_facets bu librt)
INSTALL(TARGETS g-xxx_facets RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})
