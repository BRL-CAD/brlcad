add_definitions(-D_CONSOLE -DBRLCAD_DLL)

verbose_add_subdirectory("src/conv" 3dm)
verbose_add_subdirectory("src/conv" asc)
verbose_add_subdirectory("src/conv" gcv)
verbose_add_subdirectory("src/conv" step)
verbose_add_subdirectory("src/conv" gltf)

verbose_add_subdirectory("src/conv" iges)
verbose_add_subdirectory("src/conv" intaval)
verbose_add_subdirectory("src/conv" ply)
verbose_add_subdirectory("src/conv" raw)
verbose_add_subdirectory("src/conv" vdeck)

brlcad_addexec(asc-nmg nmg/asc-nmg.c "libwdb;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(bot_dump bot_dump.c "libged;librt" FOLDER Conv)

set(
  comgeom-g_SRCS
  comgeom/cvt.c
  comgeom/f2a.c
  comgeom/mat.c
  comgeom/read.c
  comgeom/region.c
  comgeom/solid.c
  comgeom/tools.c
)
brlcad_addexec(comgeom-g "${comgeom-g_SRCS}" "libwdb;libbn;libbu;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(conv-vg2g conv-vg2g.c "libbu" FOLDER Conv)

brlcad_addexec(dbupgrade dbupgrade.c "libwdb;librt;libbu" FOLDER Conv)

brlcad_addexec(k-g k-g.cpp "libwdb;librt;libbu;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(dxf-g dxf/dxf-g.c "libwdb;librt;libnmg;libbn;libbu;${M_LIBRARY}" FOLDER Conv)

if(BRLCAD_ENABLE_TCL)
  brlcad_addexec(enf-g enf-g.c "libwdb;${TCL_LIBRARY};${M_LIBRARY}" FOLDER Conv)
  target_include_directories(enf-g SYSTEM PRIVATE ${TCL_INCLUDE_PATH})
else(BRLCAD_ENABLE_TCL)
  cmakefiles(enf-g.c)
endif(BRLCAD_ENABLE_TCL)

brlcad_addexec(g-dot g-dot.c "libged;libbu" FOLDER Conv)

brlcad_addexec(g-dxf dxf/g-dxf.c "librt;libgcv;libnmg;libbu" FOLDER Conv)
add_dependencies(g-dxf gcv_plugins)

brlcad_addexec(g-egg g-egg.c "libwdb;libgcv;libnmg" FOLDER Conv)
add_dependencies(g-egg gcv_plugins)

brlcad_addexec(g-nff g-nff.c "libwdb;libnmg;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(fast4-g fast4-g.c "libwdb;librt;libnmg;libbu;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(jack-g jack/jack-g.c "libwdb;librt;libnmg;libbu" FOLDER Conv)

brlcad_addexec(g-jack jack/g-jack.c "librt;libnmg;libbu" FOLDER Conv)

brlcad_addexec(g-off off/g-off.c "librt;libnmg;libbu" FOLDER Conv)

brlcad_addexec(off-g off/off-g.c "libwdb;librt;libnmg;libbu" FOLDER Conv)

brlcad_addexec(stl-g stl/stl-g.c "libwdb;librt;libbu;${WINSOCK_LIB}" FOLDER Conv)

brlcad_addexec(g-stl stl/g-stl.c "librt;libgcv;libnmg;libbu;${WINSOCK_LIB}" FOLDER Conv)
add_dependencies(g-stl gcv_plugins)

brlcad_addexec(g4-g5 g4-g5.c "librt;libbu" NO_INSTALL)

brlcad_addexec(g5-g4 g5-g4.c "libwdb;librt;libbu" NO_INSTALL)

brlcad_addexec(g-acad g-acad.c "librt;libnmg;libbu" FOLDER Conv)

brlcad_addexec(g-obj g-obj.c "librt;libnmg;libbu" FOLDER Conv)

# TODO - Ugh; need to either merge this into libgcv or remove it in favor of
# that implementation...
set(obj-g_SRCS obj-g.c ../libgcv/plugins/obj/tri_face.c)
brlcad_addexec(obj-g "${obj-g_SRCS}" "libbu;libbn;librt;libnmg;libwdb;libwfobj" FOLDER Conv)
target_include_directories(obj-g PRIVATE ${BRLCAD_SOURCE_DIR}/src/libgcv/plugins/obj)
target_include_directories(obj-g PRIVATE ${BRLCAD_SOURCE_DIR}/src/libgcv/plugins/obj/wfobj)

brlcad_addexec(g-voxel g-voxel.c "librt;libbu;libwdb;libanalyze;" FOLDER Conv)

if(BRLCAD_ENABLE_OPENVDB)
  brlcad_addexec(g-vdb g-vdb.cpp "librt;libbu;libwdb;libanalyze;${OpenVDB_LIBRARIES};${IlmBase_LIBRARIES};${Tbb_LIBRARIES}" FOLDER Conv)
  target_include_directories(g-vdb SYSTEM PRIVATE ${OpenVDB_INCLUDE_DIRS} ${Tbb_INCLUDE_DIRS} ${IlmBase_INCLUDE_DIRS})
  # warnings off due to unfixable errors in boost and Ilmbase/half headers
  set_property(SOURCE g-vdb.cpp APPEND PROPERTY COMPILE_FLAGS "${OpenVDB_DEFINITIONS} -w")
else(BRLCAD_ENABLE_OPENVDB)
  cmakefiles(g-vdb.cpp)
endif(BRLCAD_ENABLE_OPENVDB)

brlcad_addexec(patch-g patch/patch-g.c "libwdb;librt;libnmg;libbu;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(rpatch patch/rpatch.c "librt;libbu" FOLDER Conv)

brlcad_addexec(tankill-g tankill/tankill-g.c "libwdb;librt;libnmg;libbu" FOLDER Conv)

brlcad_addexec(g-var g-var.c "librt;libbu;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(g-vrml g-vrml.c "librt;libnmg;libbu;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(g-x3d g-x3d.c "librt;libnmg;libbu;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(nastran-g nastran-g.c "libwdb;librt;libnmg;libbu;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(shp-g "shp/shp-g.c;shp/shapelib/safileio.c;shp/shapelib/shpopen.c" "libwdb;librt;libbu" FOLDER Conv)

brlcad_addexec(g-shell-rect g-shell-rect.c "libwdb;librt;libnmg;libbu;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(g-xxx g-xxx.c "librt;libnmg;libbu" NO_INSTALL)
brlcad_adddata(g-xxx.c sample_applications)

brlcad_addexec(g-xxx_facets g-xxx_facets.c "librt;libnmg;libbu" NO_INSTALL)
brlcad_adddata(g-xxx_facets.c sample_applications)

brlcad_addexec(walk_example walk_example.c "libbu;librt" NO_INSTALL)
brlcad_adddata(walk_example.c sample_applications)

set(
  conv_ignore_files
  CMakeLists.txt
  Formats.csv
  comgeom/try.sh
  dbclean.sh
  dxf/dxf.h
  patch/patch-g.h
  patch/pull_comp.sh
  patch/pull_solidsub.sh
  patch/rpatch.f
  shp/shapelib/COPYING
  shp/shapelib/license.html
  shp/shapelib/shapefil.h
  shp/README
)
cmakefiles(${conv_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
