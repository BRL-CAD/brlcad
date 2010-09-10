ADD_SUBDIRECTORY(step)
ADD_SUBDIRECTORY(iges)
ADD_SUBDIRECTORY(intaval)

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

BRLCAD_ADDEXEC(3dm-g 3dm/3dm-g.cpp "wdb ${BRLCAD_OPENNURBS_LIBRARY}")
set_target_properties(3dm-g PROPERTIES COMPILE_DEFINITIONS "OBJ_BREP=1")

BRLCAD_ADDEXEC(asc2g asc/asc2g.c "bu librt wdb ged") 

BRLCAD_ADDEXEC(asc2pix asc/asc2pix.c bu) 

BRLCAD_ADDEXEC(pix2asc asc/pix2asc.c bu) 

BRLCAD_ADDEXEC(asc-nmg nmg/asc-nmg.c wdb) 

BRLCAD_ADDEXEC(g2asc asc/g2asc.c "bu librt")

BRLCAD_ADDEXEC(bot-bldxf dxf/bot-bldxf.c "bu librt")

BRLCAD_ADDEXEC(bot_dump bot_dump.c "librt ged")

BRLCAD_ADDEXEC(bot_shell-vtk bot_shell-vtk.c librt)

SET(comgeom-g_SRCS
    comgeom/cvt.c
    comgeom/f2a.c
    comgeom/mat.c
    comgeom/read.c
    comgeom/region.c
    comgeom/solid.c
    comgeom/tools.c
)
BRLCAD_ADDEXEC(comgeom-g "${comgeom-g_SRCS}" "bu bn wdb")

BRLCAD_ADDEXEC(conv-vg2g conv-vg2g.c "bu sysv")

BRLCAD_ADDEXEC(cy-g cy-g.c wdb)

BRLCAD_ADDEXEC(dbupgrade dbupgrade.c "bu librt wdb")

BRLCAD_ADDEXEC(dem-g dem-g.c wdb)

BRLCAD_ADDEXEC(dxf-g dxf/dxf-g.c "bu bn librt wdb")

BRLCAD_ADDEXEC(enf-g enf-g.c wdb)

BRLCAD_ADDEXEC(euclid_format euclid/euclid_format.c librt)

BRLCAD_ADDEXEC(euclid_unformat euclid/euclid_unformat.c "bu bn sysv")

BRLCAD_ADDEXEC(euclid-g euclid/euclid-g.c wdb)

BRLCAD_ADDEXEC(g-dxf dxf/g-dxf.c "bu librt gcv")

BRLCAD_ADDEXEC(g-egg g-egg.c "wdb gcv")

BRLCAD_ADDEXEC(g-euclid euclid/g-euclid.c librt)

BRLCAD_ADDEXEC(g-euclid1 euclid/g-euclid1.c librt)

BRLCAD_ADDEXEC(g-nff g-nff.c wdb)

BRLCAD_ADDEXEC(g-nmg nmg/g-nmg.c wdb)

BRLCAD_ADDEXEC(fast4-g fast4-g.c "bu librt wdb")

BRLCAD_ADDEXEC(jack-g jack/jack-g.c "bu librt wdb")

BRLCAD_ADDEXEC(g-jack jack/g-jack.c "bu librt")

BRLCAD_ADDEXEC(g-off off/g-off.c "bu librt")

BRLCAD_ADDEXEC(off-g off/off-g.c "bu librt wdb")

BRLCAD_ADDEXEC(stl-g stl/stl-g.c "bu librt wdb")

BRLCAD_ADDEXEC(g-stl stl/g-stl.c "bu librt gcv")

BRLCAD_ADDEXEC(g4-g5 g4-g5.c "bu librt")

BRLCAD_ADDEXEC(g5-g4 g5-g4.c "bu librt wdb")

BRLCAD_ADDEXEC(g-acad g-acad.c "bu librt")

BRLCAD_ADDEXEC(g-obj g-obj.c "bu librt")

BRLCAD_ADDEXEC(obj-g obj-g.c "bu librt")

BRLCAD_ADDEXEC(patch-g patch/patch-g.c "bu librt wdb")

BRLCAD_ADDEXEC(rpatch patch/rpatch.c "bu librt")

BRLCAD_ADDEXEC(g-tankill tankill/g-tankill.c "bu librt")

BRLCAD_ADDEXEC(tankill-g tankill/tankill-g.c "bu librt wdb")

BRLCAD_ADDEXEC(g-var g-var.c "bu librt")

BRLCAD_ADDEXEC(g-vrml g-vrml.c "bu librt")

BRLCAD_ADDEXEC(g-x3d g-x3d.c "bu librt")

BRLCAD_ADDEXEC(nastran-g nastran-g.c "bu librt wdb")

BRLCAD_ADDEXEC(nmg-bot nmg/nmg-bot.c wdb)

BRLCAD_ADDEXEC(nmg-rib nmg/nmg-rib.c wdb)

BRLCAD_ADDEXEC(nmg-sgp nmg/nmg-sgp.c wdb)

BRLCAD_ADDEXEC(ply-g ply-g.c "bu librt")

BRLCAD_ADDEXEC(poly-bot poly-bot.c wdb) 

BRLCAD_ADDEXEC(proe-g proe-g.c "bu librt wdb")

BRLCAD_ADDEXEC(viewpoint-g viewpoint-g.c "bu librt wdb")

BRLCAD_ADDEXEC(g-xxx_facets g-xxx_facets.c "bu librt")

BRLCAD_ADDEXEC(g-shell.rect g-shell.rect.c "bu librt wdb")

install(PROGRAMS intaval-g.py DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

INSTALL(FILES g-xxx.c DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/sample_applications)
INSTALL(FILES g-xxx_facets.c DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/sample_applications)

SET(conv_MANS
  asc/g2asc.1 
  asc/pix2asc.1 
  bot_dump.1 
  bot_shell-vtk.1 
  comgeom/comgeom-g.1 
  conv-vg2g.1 
  cy-g.1 
  dbclean.1 
  dbupgrade.1 
  dxf/dxf-g.1 
  dxf/g-dxf.1 
  enf-g.1 
  euclid/euclid-g.1 
  euclid/g-euclid.1 
  fast4-g.1 
  g-acad.1 
  g-egg.1 
  g-nff.1 
  g-obj.1 
  g-shell.rect.1 
  g-var.1 
  g-vrml.1 
  g-x3d.1 
  nastran-g.1 
  nmg/asc-nmg.1 
  nmg/g-nmg.1 
  nmg/nmg-bot.1 
  nmg/nmg-rib.1 
  patch/patch-g.1 
  patch/rpatch.1 
  ply-g.1 
  poly-bot.1 
  proe-g.1 
  stl/g-stl.1 
  stl/stl-g.1 
  tankill/g-tankill.1 
  tankill/tankill-g.1 
  viewpoint-g.1
)

install(FILES ${conv_MANS} DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
