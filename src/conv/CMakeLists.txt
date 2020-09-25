add_definitions(
  -D_CONSOLE
  -DBRLCAD_DLL
  )

verbose_add_subdirectory("src/conv" 3dm)
verbose_add_subdirectory("src/conv" asc)
verbose_add_subdirectory("src/conv" gcv)
verbose_add_subdirectory("src/conv" step)
verbose_add_subdirectory("src/conv" csg)

verbose_add_subdirectory("src/conv" iges)
verbose_add_subdirectory("src/conv" intaval)
verbose_add_subdirectory("src/conv" ply)
verbose_add_subdirectory("src/conv" raw)
verbose_add_subdirectory("src/conv" vdeck)

set(CONV_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${GCV_INCLUDE_DIRS}
  ${SYSV_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${REGEX_INCLUDE_DIR}
  ${WFOBJ_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(CONV_INCLUDE_DIRS)

BRLCAD_ADDEXEC(asc-nmg nmg/asc-nmg.c "libwdb;${M_LIBRARY}")

BRLCAD_ADDEXEC(bot_dump bot_dump.c "libged;librt")

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

BRLCAD_ADDEXEC(dbupgrade dbupgrade.c "libwdb;librt;libbu")

BRLCAD_ADDEXEC(k-g k-g.cpp "libwdb;librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(dxf-g dxf/dxf-g.c "libwdb;librt;libnmg;libbn;libbu;${M_LIBRARY}")

if(BRLCAD_ENABLE_TCL)
  include_directories(${TCLCAD_INCLUDE_DIRS})
  BRLCAD_ADDEXEC(enf-g enf-g.c "libwdb;${TCL_LIBRARY};${M_LIBRARY}")
else(BRLCAD_ENABLE_TCL)
  CMAKEFILES(enf-g.c)
endif(BRLCAD_ENABLE_TCL)

BRLCAD_ADDEXEC(g-dot g-dot.c "libged;libbu")

BRLCAD_ADDEXEC(g-dxf dxf/g-dxf.c "librt;libgcv;libnmg;libbu")
add_dependencies(g-dxf libgcv_plugins)

BRLCAD_ADDEXEC(g-egg g-egg.c "libwdb;libgcv;libnmg")
add_dependencies(g-egg libgcv_plugins)

BRLCAD_ADDEXEC(g-nff g-nff.c "libwdb;libnmg;${M_LIBRARY}")

BRLCAD_ADDEXEC(fast4-g fast4-g.c "libwdb;librt;libnmg;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(jack-g jack/jack-g.c "libwdb;librt;libnmg;libbu")

BRLCAD_ADDEXEC(g-jack jack/g-jack.c "librt;libnmg;libbu")

BRLCAD_ADDEXEC(g-off off/g-off.c "librt;libnmg;libbu")

BRLCAD_ADDEXEC(off-g off/off-g.c "libwdb;librt;libnmg;libbu")

BRLCAD_ADDEXEC(stl-g stl/stl-g.c "libwdb;librt;libbu;${WINSOCK_LIB}")

BRLCAD_ADDEXEC(g-stl stl/g-stl.c "librt;libgcv;libnmg;libbu;${WINSOCK_LIB}")
add_dependencies(g-stl libgcv_plugins)

BRLCAD_ADDEXEC(g4-g5 g4-g5.c "librt;libbu" NO_INSTALL)

BRLCAD_ADDEXEC(g5-g4 g5-g4.c "libwdb;librt;libbu" NO_INSTALL)

BRLCAD_ADDEXEC(g-acad g-acad.c "librt;libnmg;libbu")

BRLCAD_ADDEXEC(g-obj g-obj.c "librt;libnmg;libbu")

# TODO - Ugh; need to either merge this into libgcv or remove it in favor of
# that implementation...
include_directories(${BRLCAD_SOURCE_DIR}/src/libgcv/plugins/obj)
include_directories(${BRLCAD_SOURCE_DIR}/src/libgcv/plugins/obj/wfobj)
set(obj-g_SRCS obj-g.c ../libgcv/plugins/obj/tri_face.c)
BRLCAD_ADDEXEC(obj-g "${obj-g_SRCS}" "libbu;libbn;librt;libnmg;libwdb;libwfobj")

BRLCAD_ADDEXEC(g-voxel g-voxel.c "librt;libbu;libwdb;libanalyze;")

if (BRLCAD_ENABLE_OPENVDB)
  include_directories(${OpenVDB_INCLUDE_DIRS})
  include_directories(${Tbb_INCLUDE_DIRS})
  include_directories(${IlmBase_INCLUDE_DIRS})
  BRLCAD_ADDEXEC(g-vdb g-vdb.cpp "librt;libbu;libwdb;libanalyze;${OpenVDB_LIBRARIES};${IlmBase_LIBRARIES};${Tbb_LIBRARIES}")
  # warnings off due to unfixable errors in boost and Ilmbase/half headers
  set_property(SOURCE g-vdb.cpp APPEND PROPERTY COMPILE_FLAGS "${OpenVDB_DEFINITIONS} -w")
else (BRLCAD_ENABLE_OPENVDB)
  CMAKEFILES(g-vdb.cpp)
endif (BRLCAD_ENABLE_OPENVDB)

BRLCAD_ADDEXEC(patch-g patch/patch-g.c "libwdb;librt;libnmg;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(rpatch patch/rpatch.c "librt;libbu")

BRLCAD_ADDEXEC(tankill-g tankill/tankill-g.c "libwdb;librt;libnmg;libbu")

BRLCAD_ADDEXEC(g-var g-var.c "librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(g-vrml g-vrml.c "librt;libnmg;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(g-x3d g-x3d.c "librt;libnmg;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(nastran-g nastran-g.c "libwdb;librt;libnmg;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(shp-g "shp/shp-g.c;shp/shapelib/safileio.c;shp/shapelib/shpopen.c" "libwdb;librt;libbu")

BRLCAD_ADDEXEC(g-shell-rect g-shell-rect.c "libwdb;librt;libnmg;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(g-xxx g-xxx.c "librt;libnmg;libbu" NO_INSTALL)
BRLCAD_ADDDATA(g-xxx.c sample_applications)

BRLCAD_ADDEXEC(g-xxx_facets g-xxx_facets.c "librt;libnmg;libbu" NO_INSTALL)
BRLCAD_ADDDATA(g-xxx_facets.c sample_applications)

BRLCAD_ADDEXEC(walk_example walk_example.c "libbu;librt" NO_INSTALL)
BRLCAD_ADDDATA(walk_example.c sample_applications)

set(conv_ignore_files
  CMakeLists.txt
  Formats.csv
  comgeom/try.sh
  dbclean.sh
  dxf/dxf.h
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
