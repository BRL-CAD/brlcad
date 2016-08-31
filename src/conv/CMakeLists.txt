if(MSVC)
  add_definitions(
    -DON_DLL_IMPORTS
    )
endif(MSVC)

add_definitions(
  -D_CONSOLE
  -DBRLCAD_DLL
  )

add_subdirectory(3dm)
add_subdirectory(asc)
add_subdirectory(gcv)
add_subdirectory(step)
add_subdirectory(csg)

add_subdirectory(iges)
add_subdirectory(intaval)
add_subdirectory(ply)
add_subdirectory(raw)
add_subdirectory(vdeck)

set(CONV_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${GCV_INCLUDE_DIRS}
  ${SYSV_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${REGEX_INCLUDE_DIR}
  ${WFOBJ_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(CONV_INCLUDE_DIRS)

BRLCAD_ADDEXEC(asc-nmg nmg/asc-nmg.c "libwdb;${M_LIBRARY}")

BRLCAD_ADDEXEC(bot-bldxf dxf/bot-bldxf.c "librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(bot_dump bot_dump.c "libged;librt")

BRLCAD_ADDEXEC(bot_shell-vtk bot_shell-vtk.c librt)

set(comgeom-g_SRCS
  comgeom/cvt.c
  comgeom/f2a.c
  comgeom/mat.c
  comgeom/read.c
  comgeom/region.c
  comgeom/solid.c
  comgeom/tools.c
  )
BRLCAD_ADDEXEC(comgeom-g "${comgeom-g_SRCS}" "libwdb;libbn;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(conv-vg2g conv-vg2g.c "libbu")

BRLCAD_ADDEXEC(cy-g cy-g.c "libwdb;${M_LIBRARY}")

BRLCAD_ADDEXEC(dbupgrade dbupgrade.c "libwdb;librt;libbu")

BRLCAD_ADDEXEC(dem-g dem-g.c "libwdb;${M_LIBRARY}")

BRLCAD_ADDEXEC(dxf-g dxf/dxf-g.c "libwdb;librt;libbn;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(enf-g enf-g.c "libwdb;${TCL_LIBRARY};${M_LIBRARY}")

BRLCAD_ADDEXEC(euclid_format euclid/euclid_format.c librt)

BRLCAD_ADDEXEC(euclid_unformat euclid/euclid_unformat.c "libbn;libbu")

BRLCAD_ADDEXEC(euclid-g euclid/euclid-g.c libwdb)

BRLCAD_ADDEXEC(g-dot g-dot.c "libged;libbu")

BRLCAD_ADDEXEC(g-dxf dxf/g-dxf.c "librt;libgcv;libbu")
add_dependencies(g-dxf libgcv_plugins)

BRLCAD_ADDEXEC(g-egg g-egg.c "libwdb;libgcv")
add_dependencies(g-egg libgcv_plugins)

BRLCAD_ADDEXEC(g-euclid euclid/g-euclid.c librt)

BRLCAD_ADDEXEC(g-euclid1 euclid/g-euclid1.c librt)

BRLCAD_ADDEXEC(g-nff g-nff.c "libwdb;${M_LIBRARY}")

BRLCAD_ADDEXEC(g-nmg nmg/g-nmg.c libwdb)

BRLCAD_ADDEXEC(fast4-g fast4-g.c "libwdb;librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(jack-g jack/jack-g.c "libwdb;librt;libbu")

BRLCAD_ADDEXEC(g-jack jack/g-jack.c "librt;libbu")

BRLCAD_ADDEXEC(g-off off/g-off.c "librt;libbu")

BRLCAD_ADDEXEC(off-g off/off-g.c "libwdb;librt;libbu")

BRLCAD_ADDEXEC(stl-g stl/stl-g.c "libwdb;librt;libbu;${WINSOCK_LIB}")

BRLCAD_ADDEXEC(g-stl stl/g-stl.c "librt;libgcv;libbu;${WINSOCK_LIB}")
add_dependencies(g-stl libgcv_plugins)

BRLCAD_ADDEXEC(g4-g5 g4-g5.c "librt;libbu" NO_INSTALL)

BRLCAD_ADDEXEC(g5-g4 g5-g4.c "libwdb;librt;libbu" NO_INSTALL)

BRLCAD_ADDEXEC(g-acad g-acad.c "librt;libbu")

BRLCAD_ADDEXEC(g-obj g-obj.c "librt;libbu")

# TODO - Ugh; need to either merge this into libgcv or remove it in favor of
# that implementation...
include_directories(${BRLCAD_SOURCE_DIR}/src/libgcv/plugins/obj)
include_directories(${BRLCAD_SOURCE_DIR}/src/libgcv/plugins/obj/wfobj)
set(obj-g_SRCS obj-g.c ../libgcv/plugins/obj/tri_face.c)
BRLCAD_ADDEXEC(obj-g "${obj-g_SRCS}" "libbu;libbn;librt;libwdb;libwfobj")

BRLCAD_ADDEXEC(g-voxel g-voxel.c "librt;libbu;libwdb;libanalyze;")

BRLCAD_ADDEXEC(patch-g patch/patch-g.c "libwdb;librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(rpatch patch/rpatch.c "librt;libbu")

BRLCAD_ADDEXEC(g-tankill tankill/g-tankill.c "librt;libbu")

BRLCAD_ADDEXEC(tankill-g tankill/tankill-g.c "libwdb;librt;libbu")

BRLCAD_ADDEXEC(g-var g-var.c "librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(g-vrml g-vrml.c "librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(g-x3d g-x3d.c "librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(nastran-g nastran-g.c "libwdb;librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(nmg-bot nmg/nmg-bot.c libwdb)

BRLCAD_ADDEXEC(nmg-rib nmg/nmg-rib.c libwdb)

BRLCAD_ADDEXEC(nmg-sgp nmg/nmg-sgp.c libwdb)

BRLCAD_ADDEXEC(poly-bot poly-bot.c "libwdb;${WINSOCK_LIB}")

BRLCAD_ADDEXEC(proe-g proe-g.c "libwdb;librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(shp-g "shp/shp-g.c;shp/shapelib/safileio.c;shp/shapelib/shpopen.c" "libwdb;librt;libbu")

BRLCAD_ADDEXEC(viewpoint-g viewpoint-g.c "libwdb;librt;libbu")

BRLCAD_ADDEXEC(g-shell-rect g-shell-rect.c "libwdb;librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(g-xxx g-xxx.c "librt;libbu" NO_INSTALL)
BRLCAD_ADDDATA(g-xxx.c sample_applications)

BRLCAD_ADDEXEC(g-xxx_facets g-xxx_facets.c "librt;libbu" NO_INSTALL)
BRLCAD_ADDDATA(g-xxx_facets.c sample_applications)

BRLCAD_ADDEXEC(walk_example walk_example.c "libbu;librt" NO_INSTALL)
BRLCAD_ADDDATA(walk_example.c sample_applications)

install(PROGRAMS intaval-g.py DESTINATION bin)


set(conv_ignore_files
  Formats.csv
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
  )
CMAKEFILES(${conv_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
