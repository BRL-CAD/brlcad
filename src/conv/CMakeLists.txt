IF(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	ADD_SUBDIRECTORY(step)
ENDIF(LEX_EXECUTABLE AND YACC_EXECUTABLE)
ADD_SUBDIRECTORY(iges)
ADD_SUBDIRECTORY(intaval)

include_directories(
	${ZLIB_INCLUDE_DIR}
	${REGEX_INCLUDE_DIR}
	${OPENNURBS_INCLUDE_DIR}
	${TCL_INCLUDE_DIRS}
)

IF(MSVC)
add_definitions(
    -DON_DLL_IMPORTS
	 -D_CONSOLE
)
ENDIF(MSVC)

BRLCAD_ADDEXEC(3dm-g 3dm/3dm-g.cpp "libwdb ${OPENNURBS_LIBRARY}")
set_target_properties(3dm-g PROPERTIES COMPILE_DEFINITIONS "OBJ_BREP=1")

BRLCAD_ADDEXEC(asc2g asc/asc2g.c "libged libwdb librt libbu libtclcad ${WINSOCK_LIB} ${M_LIBRARY}")

BRLCAD_ADDEXEC(asc2pix asc/asc2pix.c libbu)

BRLCAD_ADDEXEC(pix2asc asc/pix2asc.c libbu)

BRLCAD_ADDEXEC(asc-nmg nmg/asc-nmg.c "libwdb ${M_LIBRARY}")

BRLCAD_ADDEXEC(g2asc asc/g2asc.c "librt libbu ${WINSOCK_LIB}")

BRLCAD_ADDEXEC(bot-bldxf dxf/bot-bldxf.c "librt libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(bot_dump bot_dump.c "libged librt")

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
BRLCAD_ADDEXEC(comgeom-g "${comgeom-g_SRCS}" "libwdb libbn libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(conv-vg2g conv-vg2g.c "libbu libsysv")

BRLCAD_ADDEXEC(cy-g cy-g.c "libwdb ${M_LIBRARY}")

BRLCAD_ADDEXEC(dbupgrade dbupgrade.c "libwdb librt libbu")

BRLCAD_ADDEXEC(dem-g dem-g.c "libwdb ${M_LIBRARY}")

BRLCAD_ADDEXEC(dxf-g dxf/dxf-g.c "libwdb librt libbn libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(enf-g enf-g.c "libwdb ${M_LIBRARY}")

BRLCAD_ADDEXEC(euclid_format euclid/euclid_format.c librt)

BRLCAD_ADDEXEC(euclid_unformat euclid/euclid_unformat.c "libbn libbu libsysv")

BRLCAD_ADDEXEC(euclid-g euclid/euclid-g.c libwdb)

BRLCAD_ADDEXEC(g-dot g-dot.c "libged libbu")

BRLCAD_ADDEXEC(g-dxf dxf/g-dxf.c "librt libgcv libbu")

BRLCAD_ADDEXEC(g-egg g-egg.c "libwdb libgcv")

BRLCAD_ADDEXEC(g-euclid euclid/g-euclid.c librt)

BRLCAD_ADDEXEC(g-euclid1 euclid/g-euclid1.c librt)

BRLCAD_ADDEXEC(g-nff g-nff.c "libwdb ${M_LIBRARY}")

BRLCAD_ADDEXEC(g-nmg nmg/g-nmg.c libwdb)

BRLCAD_ADDEXEC(fast4-g fast4-g.c "libwdb librt libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(jack-g jack/jack-g.c "libwdb librt libbu")

BRLCAD_ADDEXEC(g-jack jack/g-jack.c "librt libbu")

BRLCAD_ADDEXEC(g-off off/g-off.c "librt libbu")

BRLCAD_ADDEXEC(off-g off/off-g.c "libwdb librt libbu")

BRLCAD_ADDEXEC(stl-g stl/stl-g.c "libwdb librt libbu ${WINSOCK_LIB}")

BRLCAD_ADDEXEC(g-stl stl/g-stl.c "librt libgcv libbu ${WINSOCK_LIB}")

add_executable(g4-g5 g4-g5.c)
target_link_libraries(g4-g5 librt libbu)
IF(MSVC)
	SET_TARGET_PROPERTIES(g4-g5 PROPERTIES COMPILE_FLAGS "-DBRLCAD_DLL")
ENDIF(MSVC)

add_executable(g5-g4 g5-g4.c)
target_link_libraries(g5-g4 libwdb librt libbu)
IF(MSVC)
	SET_TARGET_PROPERTIES(g5-g4 PROPERTIES COMPILE_FLAGS "-DBRLCAD_DLL")
ENDIF(MSVC)

BRLCAD_ADDEXEC(g-acad g-acad.c "librt libbu")

BRLCAD_ADDEXEC(g-obj g-obj.c "librt libbu")

# TODO - Until wfobj functionality is available as a libgcv API,
# obj-g build logic is living in src/libgcv/wfobj. Once an API 
# for libgcv is devised that exposes the wfobj functionality move 
# obj-g build logic back here
#BRLCAD_ADDEXEC(obj-g ${OBJ_G_SRCS} "libbu libbn librt libwdb libgcv")

BRLCAD_ADDEXEC(patch-g patch/patch-g.c "libwdb librt libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(rpatch patch/rpatch.c "librt libbu")

BRLCAD_ADDEXEC(g-tankill tankill/g-tankill.c "librt libbu")

BRLCAD_ADDEXEC(tankill-g tankill/tankill-g.c "libwdb librt libbu")

BRLCAD_ADDEXEC(g-var g-var.c "librt libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(g-vrml g-vrml.c "librt libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(g-x3d g-x3d.c "librt libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(nastran-g nastran-g.c "libwdb librt libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(nmg-bot nmg/nmg-bot.c libwdb)

BRLCAD_ADDEXEC(nmg-rib nmg/nmg-rib.c libwdb)

BRLCAD_ADDEXEC(nmg-sgp nmg/nmg-sgp.c libwdb)

BRLCAD_ADDEXEC(ply-g ply-g.c "librt libbu")

BRLCAD_ADDEXEC(poly-bot poly-bot.c "libwdb ${WINSOCK_LIB}")

BRLCAD_ADDEXEC(proe-g proe-g.c "libwdb librt libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(shp-g "shp/shp-g.c shp/shapelib/safileio.c shp/shapelib/shpopen.c" "libwdb librt libbu")

BRLCAD_ADDEXEC(viewpoint-g viewpoint-g.c "libwdb librt libbu")

add_executable(g-xxx_facets g-xxx_facets.c)
target_link_libraries(g-xxx_facets librt libbu)
IF(MSVC)
	SET_TARGET_PROPERTIES(g-xxx_facets PROPERTIES COMPILE_FLAGS "-DBRLCAD_DLL")
ENDIF(MSVC)

BRLCAD_ADDEXEC(g-shell-rect g-shell-rect.c "libwdb librt libbu ${M_LIBRARY}")

install(PROGRAMS intaval-g.py DESTINATION bin)

BRLCAD_ADDFILE(g-xxx.c sample_applications)
BRLCAD_ADDFILE(g-xxx_facets.c sample_applications)

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
  g-shell-rect.1
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
ADD_MAN_PAGES(1 conv_MANS)

SET(conv_ignore_files
	Formats.csv
	comgeom/3d.h
	comgeom/try.sh
	dbclean.sh
	dxf/dxf.h
	intaval-g.py
	off
	patch/patch-g.h
	patch/pull_comp.sh
	patch/pull_solidsub.sh
	patch/rpatch.f
	shp/shapelib/LICENSE.LGPL
	shp/shapelib/license.html
	shp/shapelib/shapefil.h
	shp/README
	walk_example.c
	)
CMAKEFILES(${conv_ignore_files})
CMAKEFILES(Makefile.am)
