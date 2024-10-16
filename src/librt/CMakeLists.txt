brlcad_find_package(manifold REQUIRED)
brlcad_find_package(OPENNURBS REQUIRED SYSPATTERN openNURBS)
brlcad_find_package(REGEX REQUIRED)
if(BRLCAD_ENABLE_ASSETIMPORT)
  # TODO - see if we can use tinygltf to write out the glb file ourselves -
  # Manifold's assimp integration is convenient for producing glb files, which
  # are needed if we're going to submit problem cases upstream, but using it
  # introduces an additional large dependency just for a debugging output...
  brlcad_find_package(pugixml)
  brlcad_find_package(assimp)
endif(BRLCAD_ENABLE_ASSETIMPORT)
brlcad_find_package(GEOGRAM REQUIRED SYSPATTERN geogram)
add_definitions(-DUSE_GEOGRAM)

if(TARGET assimp::assimp AND TARGET pugixml::pugixml)
  add_definitions(-DUSE_ASSETIMPORT)
  set(ASSETIMPORT_LIBRARIES assimp::assimp pugixml::pugixml)
endif(TARGET assimp::assimp AND TARGET pugixml::pugixml)

if(NOT BRLCAD_ENABLE_GCT)
  brlcad_find_package(MMESH REQUIRED)
  if(MMESH_LIBRARY)
    add_definitions(-DBRLCAD_MMESH)
  endif(MMESH_LIBRARY)
endif(NOT BRLCAD_ENABLE_GCT)

if(BRLCAD_ENABLE_OPENCL)
  add_definitions(-DUSE_OPENCL)
endif(BRLCAD_ENABLE_OPENCL)

# FIXME: needs materialX
#  material/materialX.cpp
set(
  LIBRT_SOURCES
  attr.cpp
  attributes.c
  bbox.c
  binunif/binunif.c
  binunif/db5_bin.c
  bool.c
  bool_tess.c
  bundle.c
  cache.c
  cache_lz4.c
  cmd.c
  cyclic.c
  comb/comb.c
  comb/comb_brep.cpp
  comb/comb_mirror.c
  comb/db_comb.c
  material/material.c
  constraint.c
  cut.c
  cut_hlbvh.c
  db5_alloc.c
  db5_attr.c
  db5_attr_registry.cpp
  db5_io.c
  db5_size.cpp
  db5_scan.c
  db5_types.c
  db_alloc.c
  db_anim.c
  db_corrupt.c
  db_diff.c
  db_flags.c
  db_flip.c
  db_fullpath.cpp
  db_inmem.c
  db_io.c
  db_lookup.c
  db_match.c
  db_open.c
  db_scan.c
  db_tree.cpp
  db_walk.c
  dir.c
  dspline.c
  fortray.c
  gdiam/gdiam.cpp
  globals.c
  htbl.c
  ls.c
  mater.c
  memalloc.c
  mkbundle.c
  op.c
  pr.c
  prep.cpp
  primitives/annot/annot.c
  primitives/arb8/arb8.c
  primitives/arb8/arb8_brep.cpp
  primitives/arb8/arb8_mirror.c
  primitives/arb8/arb_edit.c
  primitives/arbn/arbn.c
  primitives/arbn/arbn_brep.cpp
  primitives/arbn/arbn_mirror.c
  primitives/ars/ars.c
  primitives/ars/ars_brep.cpp
  primitives/ars/ars_mirror.c
  primitives/bot/bot.c
  primitives/bot/bot_brep.cpp
  primitives/bot/bot_edge.c
  primitives/bot/bot_mirror.c
  primitives/bot/bot_oriented_bbox.cpp
  primitives/bot/btg.c
  primitives/bot/btgf.c
  primitives/bot/decimate.c
  primitives/bot/plate.cpp
  primitives/bot/repair.cpp
  primitives/bot/sampling_checks.cpp
  primitives/brep/brep.cpp
  primitives/bspline/bspline.cpp
  primitives/bspline/bspline_brep.cpp
  primitives/bspline/bspline_mirror.c
  primitives/cline/cline.c
  primitives/cline/cline_brep.cpp
  primitives/datum/datum.c
  primitives/dsp/dsp.c
  primitives/dsp/dsp_brep.cpp
  primitives/dsp/dsp_mirror.c
  primitives/ebm/ebm.c
  primitives/ebm/ebm_brep.cpp
  primitives/ebm/ebm_mirror.c
  primitives/ehy/ehy.c
  primitives/ehy/ehy_brep.cpp
  primitives/ell/ell.c
  primitives/ell/ell_brep.cpp
  primitives/ell/ell_mirror.c
  primitives/epa/epa.c
  primitives/epa/epa_brep.cpp
  primitives/epa/epa_mirror.c
  primitives/eto/eto.c
  primitives/eto/eto_brep.cpp
  primitives/eto/eto_mirror.c
  primitives/extrude/extrude.c
  primitives/extrude/extrude_brep.cpp
  primitives/generic.c
  primitives/grip/grip.c
  primitives/grip/grip_brep.cpp
  primitives/grip/grip_mirror.c
  primitives/half/half.c
  primitives/half/half_brep.cpp
  primitives/half/half_mirror.c
  primitives/hf/hf.c
  primitives/hf/hf_brep.cpp
  primitives/hrt/hrt.c
  primitives/hrt/hrt_mirror.c
  primitives/hyp/hyp.c
  primitives/hyp/hyp_brep.cpp
  primitives/hyp/hyp_mirror.c
  primitives/joint/joint.c
  primitives/joint/joint_brep.cpp
  primitives/joint/joint_mirror.c
  primitives/metaball/metaball.c
  primitives/metaball/metaball_tri.c
  primitives/mirror.c
  primitives/nmg/nmg.c
  primitives/nmg/nmg_brep.cpp
  primitives/nmg/nmg_mirror.c
  primitives/nmg/nmg_tri_mc.c
  primitives/obj_adjust.c
  primitives/obj_curve.c
  primitives/obj_describe.c
  primitives/obj_export.c
  primitives/obj_free.c
  primitives/obj_get.c
  primitives/obj_ifree.c
  primitives/obj_import.c
  primitives/obj_make.c
  primitives/obj_mirror.c
  primitives/obj_norm.c
  primitives/obj_params.c
  primitives/obj_plot.c
  primitives/obj_prep.c
  primitives/obj_print.c
  primitives/obj_shot.c
  primitives/obj_tess.c
  primitives/obj_tnurb.c
  primitives/obj_uv.c
  primitives/obj_vshot.c
  primitives/obj_xform.c
  primitives/part/part.c
  primitives/part/part_brep.cpp
  primitives/part/part_mirror.c
  primitives/pipe/pipe.c
  primitives/pipe/pipe_brep.cpp
  primitives/pipe/pipe_mirror.c
  primitives/pnts/pnts.c
  primitives/pnts/pnts_brep.cpp
  primitives/poly/poly.c
  primitives/poly/poly_brep.cpp
  primitives/poly/poly_mirror.c
  primitives/primitive_util.c
  primitives/rec/rec.c
  primitives/revolve/revolve.c
  primitives/revolve/revolve_brep.cpp
  primitives/rhc/rhc.c
  primitives/rhc/rhc_brep.cpp
  primitives/rhc/rhc_mirror.c
  primitives/rpc/rpc.c
  primitives/rpc/rpc_brep.cpp
  primitives/rpc/rpc_mirror.c
  primitives/script/script.c
  primitives/sketch/polygons.c
  primitives/sketch/sketch.c
  primitives/sketch/sketch_brep.cpp
  primitives/sketch/sketch_tess.cpp
  primitives/sph/sph.c
  primitives/sph/sph_brep.cpp
  primitives/submodel/submodel.c
  primitives/superell/superell.c
  primitives/superell/superell_brep.cpp
  primitives/superell/superell_mirror.c
  primitives/table.cpp
  primitives/tgc/tgc.c
  primitives/tgc/tgc_brep.cpp
  primitives/tgc/tgc_mirror.c
  primitives/tor/tor.c
  primitives/tor/tor_brep.cpp
  primitives/tor/tor_mirror.c
  primitives/vol/vol.c
  primitives/vol/vol_brep.cpp
  primitives/vol/vol_mirror.c
  primitives/xxx/xxx.c
  reduce.c
  reduce_db.cpp
  regionfix.c
  roots.c
  search.cpp
  shoot.c
  timer.cpp
  tol.c
  transform.c
  tree.c
  uv.c
  vds/vds.c
  vers.c
  view.c
  vlist.c
  vshoot.c
  wdb.c
)

set_source_files_properties(gdiam/gdiam.cpp PROPERTIES COMPILE_FLAGS -w)

set(
  GCT_SRCS
  primitives/bot/gct_decimation/auxiliary/mmbinsort.c
  primitives/bot/gct_decimation/auxiliary/mmbitmap.c
  primitives/bot/gct_decimation/auxiliary/mm.c
  primitives/bot/gct_decimation/auxiliary/mmhash.c
  primitives/bot/gct_decimation/meshoptimization.c
  primitives/bot/gct_decimation/meshoptimizer.c
  primitives/bot/gct_decimation/meshdecimation.c
  primitives/bot/gct_decimation/meshdecimationSSE2.c
  primitives/bot/gct_decimation/meshdecimationSSE3.c
  primitives/bot/gct_decimation/meshdecimationSSE4p1.c
)
if(BRLCAD_ENABLE_GCT AND NOT MMESH_FOUND)
  set(LIBRT_SOURCES ${LIBRT_SOURCES} ${GCT_SRCS})
endif(BRLCAD_ENABLE_GCT AND NOT MMESH_FOUND)

set(
  librt_ignored_files
  cache.h
  librt_private.h
  material/materialX.cpp
  material/materialX.h
  cut_hlbvh.h
  prcomb.c
  primitives/bot/bot_edge.h
  primitives/bot/bot_wireframe.cpp
  ${GCT_SRCS}
  primitives/bot/gct_decimation/auxiliary/cc.h
  primitives/bot/gct_decimation/auxiliary/mm.h
  primitives/bot/gct_decimation/auxiliary/mmatomic.h
  primitives/bot/gct_decimation/auxiliary/mmbinsort.h
  primitives/bot/gct_decimation/auxiliary/mmbitmap.h
  primitives/bot/gct_decimation/auxiliary/mmhash.h
  primitives/bot/gct_decimation/auxiliary/mmthread.h
  primitives/bot/gct_decimation/meshdecimation.h
  primitives/bot/gct_decimation/meshoptimization.h
  primitives/bot/gct_decimation/meshoptimizer.h
  primitives/bot/tie.c
  primitives/bot/tie_kdtree.c
  primitives/bot/tieprivate.h
  primitives/brep/brep_debug.h
  primitives/brep/brep_local.h
  primitives/datum/datum.h
  primitives/dsp/dsp.h
  primitives/fixpt.h
  primitives/metaball/metaball.h
  primitives/revolve/revolve.h
  primitives/sph/benchmark.sh
  primitives/xxx/xxx.h
  primitives/bool.cl
  primitives/common.cl
  primitives/rt.cl
  primitives/solver.cl
  primitives/arb8/arb8_shot.cl
  primitives/arbn/arbn_shot.cl
  primitives/bot/bot_shot.cl
  primitives/ebm/ebm_shot.cl
  primitives/ehy/ehy_shot.cl
  primitives/ell/ell_shot.cl
  primitives/epa/epa_shot.cl
  primitives/eto/eto_shot.cl
  primitives/hrt/hrt_shot.cl
  primitives/hyp/hyp_shot.cl
  primitives/part/part_shot.cl
  primitives/rec/rec_shot.cl
  primitives/rhc/rhc_shot.cl
  primitives/rpc/rpc_shot.cl
  primitives/sph/sph_shot.cl
  primitives/superell/superell_shot.cl
  primitives/tgc/tgc_shot.cl
  primitives/tor/tor_shot.cl
  search.h
  subd_test_bot.asc
  test_bot2nurbs.cpp
  test_brepreport.cpp
  test_dbio.c
  test_nurbsfit.cpp
  test_root3-subd.cpp
  uvpoints.cpp
)
cmakefiles(${librt_ignored_files})

set(
  CL_FILES
  primitives/bool.cl
  primitives/common.cl
  primitives/rt.cl
  primitives/solver.cl
  primitives/arb8/arb8_shot.cl
  primitives/arbn/arbn_shot.cl
  primitives/bot/bot_shot.cl
  primitives/ebm/ebm_shot.cl
  primitives/ehy/ehy_shot.cl
  primitives/ell/ell_shot.cl
  primitives/epa/epa_shot.cl
  primitives/eto/eto_shot.cl
  primitives/hrt/hrt_shot.cl
  primitives/hyp/hyp_shot.cl
  primitives/part/part_shot.cl
  primitives/rec/rec_shot.cl
  primitives/rhc/rhc_shot.cl
  primitives/rpc/rpc_shot.cl
  primitives/sph/sph_shot.cl
  primitives/superell/superell_shot.cl
  primitives/tgc/tgc_shot.cl
  primitives/tor/tor_shot.cl
)

brlcad_adddata(nurb_example.c sample_applications)
brlcad_adddata(raydebug.tcl sample_applications)
brlcad_adddata(CL_FILES opencl)

if(BRLCAD_ENABLE_OPENCL)
  set(OPENCL_LIBS ${OPENCL_LIBRARIES})
endif(BRLCAD_ENABLE_OPENCL)

if(BRLCAD_ENABLE_SPR)
  set(SPR_LIB libSPR)
endif(BRLCAD_ENABLE_SPR)

# Note - librt_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
set(
  LIBRT_LIBS
  ${OPENCL_LIBS}
  ${librt_deps}
  ${OPENNURBS_LIBRARIES}
  ${ASSETIMPORT_LIBRARIES}
  ${GEOGRAM_LIBRARY}
  ${MMESH_LIBRARY}
  ${REGEX_LIBRARIES}
  ${WINSOCK_LIB}
  ${RPCRT_LIB}
  ${STDCXX_LIBRARIES}
  manifold::manifold
)

# Include directories needed by librt users
set(RT_INCLUDE_DIRS ${OPENNURBS_INCLUDE_DIRS} ${OPENCL_INCLUDE_DIRS})

# locally used but not needed by users of the library
set(
  RT_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/vds
  ${CMAKE_CURRENT_SOURCE_DIR}/gdiam
  ${REGEX_INCLUDE_DIRS}
  ${GEOGRAM_INCLUDE_DIR}
  ${MMESH_INCLUDE_DIR}
)
# FIXME: needs materialX
#  ${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/source

brlcad_addlib(librt "${LIBRT_SOURCES}" "${LIBRT_LIBS}" "${RT_INCLUDE_DIRS}" "${RT_LOCAL_INCLUDE_DIRS}")

# set(MATERIALX_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXCore.lib;${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXFormat.lib;${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXGenOsl.lib;${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXGenShader.lib)
# BRLCAD_ADDLIB(librt "${LIBRT_SOURCES}" "${MATERIALX_LIBS};${OPENCL_LIBS};libbrep;libnmg;libbg;libbv;libbn;libbu;${OPENNURBS_LIBRARIES};${REGEX_LIBRARIES};${WINSOCK_LIB};${RPCRT_LIB};${STDCXX_LIBRARIES}")

set_target_properties(librt PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET librt APPEND PROPERTY COMPILE_DEFINITIONS "TIE_DLL_EXPORTS")
  set_property(TARGET librt APPEND PROPERTY COMPILE_DEFINITIONS "DB5_DLL_EXPORTS")
  if(TARGET librt-obj)
    set_property(TARGET librt-obj APPEND PROPERTY COMPILE_DEFINITIONS "TIE_DLL_EXPORTS")
    set_property(TARGET librt-obj APPEND PROPERTY COMPILE_DEFINITIONS "DB5_DLL_EXPORTS")
  endif(TARGET librt-obj)
endif(HIDE_INTERNAL_SYMBOLS)

# loudly append an underscore on musl so that we build by default but
# not without protest.
find_program(MUSL_LD NAMES /lib/ld-musl-x86_64.so.1)
mark_as_advanced(MUSL_LD)
if(MUSL_LD)
  message(
    "WARNING: MUSL LibC Detected.  Protecting BRL-CAD's librt.

     MUSL's dynamic linker erroneously overrides AT RUNTIME 'librt'
     references regardless of rpath or search logic specified during
     compilation and linking.  MUSL claims POSIX allows this despite
     POSIX not defining any dynamic linker behavior whatsoever. They
     (reportedly) do so to ease porting glibc-assuming apps to their
     libc, auto-converting those linked with -lm, -lpthread, -lrt
     and such during runtime loading regardless of search path logic.

     Legacy mention in POSIX 'c99' man page would allow (only) the
     c99 tool to treat BRL-CAD's librt as undefined (not C++, fort77,
     or other tools like ld), however clang/gcc/c99 compiling and
     linking are all well-behaved. Issue reported with suggestion to
     only override as fallback behavior in July 2022.

     Until MUSL changes (see root/ldso/dynlink.c), a workaround is
     employed to avoid user action.  Setting LD_PRELOAD also works.

     WARNING: MUSL ENVIRONMENT UNSUPPORTED. DO NOT RELY ON 'librt_'
     WORKAROUND. PLEASE REPORT IF YOU DEVELOP AGAINST BRL-CAD LIBRT
     AND ENCOUNTER THIS MESSAGE: devs@brlcad.org"
  )
  set_target_properties(librt PROPERTIES OUTPUT_NAME "librt_")
endif(MUSL_LD)

# The following is build code for test applications used during
# development
#
# find_package_eigen(REQUIRED)
#include_directories(
#  ${EIGEN3_INCLUDE_DIR}
#  ${CMAKE_CURRENT_SOURCE_DIR}/../libbrep
#  )
#
#BRLCAD_ADDEXEC(test_bot2nurbs test_bot2nurbs.cpp "librt;libwdb;libbrep;libbu" NO_STRICT NO_INSTALL)
#BRLCAD_ADDEXEC(test_brepreport test_brepreport.cpp "librt;libwdb;libbrep;libbu" NO_STRICT NO_INSTALL)
#BRLCAD_ADDEXEC(test_nurbsfit test_nurbsfit.cpp "librt;libwdb;libbrep;libbu" NO_STRICT NO_INSTALL)
#BRLCAD_ADDEXEC(test_root3-subd test_root3-subd.cpp "librt;libwdb;libbrep;libbu" NO_STRICT NO_INSTALL)
#BRLCAD_ADDEXEC(test_surfacetree test_surfacetree.cpp "librt;libwdb;libbrep;libbu" NO_STRICT NO_INSTALL)

add_subdirectory(tests)

cmakefiles(
  CMakeLists.txt
  vds/README
  vds/COPYING
  vds/vds.h
  gdiam/gdiam.hpp
  gdiam/gdiam_test.cpp
  gdiam/LICENSE.MIT
  gdiam/README
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
