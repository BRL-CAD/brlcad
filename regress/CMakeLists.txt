# Define the toplevel regression target - unless specifically excluded
# via a macro argument, this should collect all regression targets.
include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0 AND NOT BRLCAD_SERIAL_TESTING)
  set(JFLAG "-j${N}")
else()
  set(JFLAG)
endif()
if (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_CFG_INTDIR} -L Regression --output-on-failure --output-log "${CMAKE_BINARY_DIR}/regress_output.log" ${JFLAG})
else (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress COMMAND ${CMAKE_CTEST_COMMAND} -L Regression --output-on-failure --output-log "${CMAKE_BINARY_DIR}/regress_output.log" ${JFLAG})
endif (CMAKE_CONFIGURATION_TYPES)
set_target_properties(regress PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(regress PROPERTIES FOLDER "BRL-CAD Regression Tests")

# Image Conversion Regression Tests
add_subdirectory(icv)

# Geometry Editing Library Regression Tests
add_subdirectory(ged)

# Geometry Conversion Regression Tests
add_subdirectory(gcv)

# NURBS Regression Tests
add_subdirectory(nurbs)

# libpkg Regression Tests
add_subdirectory(pkg)

# Burst Regression Tests
add_subdirectory(burst)

# RtWizard Image Generation Regression Tests
add_subdirectory(rtwizard)

# MGED command tests
add_subdirectory(mged)

# Fuzz tests
include(Fuzzer)
brlcad_check_fuzzer()
if (${HAVE_FUZZER})
  add_subdirectory(fuzz)
endif (${HAVE_FUZZER})

# gchecker tests
add_subdirectory(gchecker)

# License check
add_subdirectory(licenses)

# Repository check
add_subdirectory(repository)

# Simulation of a 3rd party BRL-CAD library client code
add_subdirectory(user)

if(SH_EXEC)

  add_test(NAME regress-moss COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/moss.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-moss "rt;asc2g;pixdiff;pix-png;png-pix" TEST_DEFINED)

  add_test(NAME regress-lights COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/lights.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-lights "rt;asc2g;pixdiff" TEST_DEFINED)

  add_test(NAME regress-solids COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/solids.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-solids "rt;mged;pixdiff;asc2pix;gencolor" TEST_DEFINED)

  add_test(NAME regress-shaders COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/shaders.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-shaders "rt;mged;pixdiff;gencolor" TEST_DEFINED)

  add_test(NAME regress-spdi COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/spdi.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-spdi "rt;mged;pixdiff" TEST_DEFINED)

  add_test(NAME regress-rtedge COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/rtedge.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-rtedge "rt;rtedge;pixdiff;asc2g" TEST_DEFINED)

  add_test(NAME regress-iges COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/iges.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-iges "iges-g;g-iges;mged;asc2g" TEST_DEFINED)

  add_test(NAME regress-weight COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/weight.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-weight "rtweight;mged" TEST_DEFINED)

  add_test(NAME regress-gqa COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/gqa.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-gqa "gqa;mged" TEST_DEFINED)

  add_test(NAME regress-bots COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/bots.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-bots "mged;pixdiff;rt" TEST_DEFINED)

  add_test(NAME regress-mged COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/mged.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-mged mged TEST_DEFINED)

  add_test(NAME regress-red COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/red.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-red mged TEST_DEFINED)

  add_test(NAME regress-nirt COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/nirt.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-nirt "mged;nirt" TEST_DEFINED)

  add_test(NAME regress-comgeom COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/comgeom.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-comgeom "mged;asc2g;vdeck;comgeom-g" TEST_DEFINED)

  add_test(NAME regress-dsp COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/dsp.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-dsp "mged;cv;asc2pix;rt;pix-bw" TEST_DEFINED)

  add_test(NAME regress-asc2dsp COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/asc2dsp.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-asc2dsp "cv;asc2pix;pix-bw;asc2dsp" TEST_DEFINED)

  add_test(NAME regress-usage COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/usage.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-usage "rt" STAND_ALONE TEST_DEFINED)

  # TODO - test for Python before adding this one
  add_test(NAME regress-flawfinder COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/flawfinder.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-flawfinder "" STAND_ALONE TEST_DEFINED)

endif(SH_EXEC)

CMAKEFILES(
  analyze.asc
  asc2dsp.sh
  bots.sh
  comgeom.sh
  dsp.sh
  dsp/asc2dsp-new.asc
  dsp/asc2dsp-old.asc
  dsp/dsp-1-1.asc
  dsp/dsp-1-2.asc
  dsp/dsp-2-1.asc
  dsp/dsp-2-2.asc
  dsp/dsp-2-3.asc
  dsp/dsp-2-4.asc
  dsp/dsp-2-5.asc
  dsp/dsp-3-1.asc
  dsp/dsp-3-2.asc
  dsp/dsp-3-3.asc
  dsp/dsp-3-4.asc
  dsp/dsp-3-5.asc
  dsp/dsp-3-6.asc
  dsp/dsp-3-7.asc
  dsp/dsp-3-8.asc
  dsp/dsp-common.sh
  dsp/run-dsp-case-set-0.sh
  dsp/run-dsp-case-set-1.sh
  dsp/run-dsp-case-set-2.sh
  dsp/run-dsp-case-set-3.sh
  flawfinder.sh
  fuzz/CMakeLists.txt
  fuzz/fuzz_ged.cpp
  fuzz/fuzz_test1.cxx
  fuzz/run.sh
  g-dot.sh
  g-nff.sh
  gqa.sh
  iges.sh
  library.sh
  lights.ref.pix
  lights.cmake
  lights.sh
  mged.sh
  mged_test.sh
  moss.ref.pix
  moss.sh
  nirt.sh
  nirt/ovlps.g
  red.sh
  rtedge.ref.pix.gz
  rtedge.ref2.pix.gz
  rtedge.ref3.pix.gz
  rtedge.ref4.pix.gz
  rtedge.ref5.pix.gz
  rtedge.sh
  shaders.eagleCAD-512x438.pix
  shaders.ref.pix
  shaders.sh
  solids.ref.pix
  solids.sh
  solids.simple.ref.pix
  spdi.ref.pix
  spdi.sh
  tgms/comgeom.cg
  tgms/comgeom.tgt-1-v4-documented.cg
  tgms/comgeom.tgt-1-v4.cg
  tgms/havoc.g.gz
  tgms/m35.asc.gz
  tgms/m35.cg.gz
  tgms/solids.dsp.dat
  tgms/solids.mged
  tgms/solids.simple.mged
  usage.sh
  weight.sh
  )

# list of temporary files
set(regress_outfiles
  .density
  .density.weight1
  asc2dsp.log
  asc2dsp.new.dsp
  asc2dsp.old.bw
  asc2dsp.old.dsp
  bots.diff.log
  bots.diff.pix
  bots.g
  bots.lh.pix
  bots.log
  bots.no.pix
  bots.rh.pix
  bots.rl.diff.pix
  bots.rn.diff.pix
  bots.rs.diff.pix
  bots.sph.pix
  bots.sync.pix
  comgeom.log
  comgeom.m35-baseline.cg
  comgeom.m35.asc
  comgeom.m35.cg
  comgeom.m35.g
  comgeom.t-v4.g
  comgeom.t-v5.g
  density_table.txt
  dsp-2-1.bw
  dsp-2-1.dsp
  dsp-2-1.g
  dsp-2-1.log
  dsp-2-1.pix
  dsp-2-1.rt.pix
  dsp-2-2.bw
  dsp-2-2.dsp
  dsp-2-2.g
  dsp-2-2.log
  dsp-2-2.pix
  dsp-2-2.rt.pix
  dsp-2-3.bw
  dsp-2-3.dsp
  dsp-2-3.g
  dsp-2-3.log
  dsp-2-3.pix
  dsp-2-3.rt.pix
  dsp-2-4.bw
  dsp-2-4.dsp
  dsp-2-4.g
  dsp-2-4.log
  dsp-2-4.pix
  dsp-2-4.rt.pix
  dsp-2-5.bw
  dsp-2-5.dsp
  dsp-2-5.g
  dsp-2-5.log
  dsp-2-5.pix
  dsp-2-5.rt.pix
  dsp-3-1.bw
  dsp-3-1.dsp
  dsp-3-1.g
  dsp-3-1.log
  dsp-3-1.pix
  dsp-3-1.rt.pix
  dsp-3-2.bw
  dsp-3-2.dsp
  dsp-3-2.g
  dsp-3-2.log
  dsp-3-2.pix
  dsp-3-2.rt.pix
  dsp-3-3.bw
  dsp-3-3.dsp
  dsp-3-3.g
  dsp-3-3.log
  dsp-3-3.pix
  dsp-3-3.rt.pix
  dsp-3-4.bw
  dsp-3-4.dsp
  dsp-3-4.g
  dsp-3-4.log
  dsp-3-4.pix
  dsp-3-4.rt.pix
  dsp-3-5.bw
  dsp-3-5.dsp
  dsp-3-5.g
  dsp-3-5.log
  dsp-3-5.pix
  dsp-3-5.rt.pix
  dsp-3-6.bw
  dsp-3-6.dsp
  dsp-3-6.g
  dsp-3-6.log
  dsp-3-6.pix
  dsp-3-6.rt.pix
  dsp-3-7.bw
  dsp-3-7.dsp
  dsp-3-7.g
  dsp-3-7.log
  dsp-3-7.pix
  dsp-3-7.rt.pix
  dsp-3-8.bw
  dsp-3-8.dsp
  dsp-3-8.g
  dsp-3-8.log
  dsp-3-8.pix
  dsp-3-8.rt.pix
  dsp.log
  gqa.adj_air.plot3
  gqa.exp_air.plot3
  gqa.g
  gqa.gaps.plot3
  gqa.log
  gqa.mged
  gqa.overlaps.plot3
  gqa.volume.plot3
  iges.export.iges
  iges.export.stdout.iges
  iges.g
  iges.import.export.iges
  iges.import.g
  iges.import2.export.iges
  iges.import2.g
  iges.import3.export.iges
  iges.import3.g
  iges.log
  iges.m35.asc
  iges.m35.g
  iges.m35.r516.iges
  lights.asc
  lights.diff.pix
  lights.g
  lights.log
  lights.pix
  mged.g
  mged.log
  moss.asc
  moss.g
  moss.log
  moss.pix
  moss.pix.diff
  moss.pix.png
  moss.pix.png.pix
  moss.roundtrip.diff
  nirt.g
  nirt.log
  nirt.mged
  nirt.out
  nirt.ref
  red.air.empty.out
  red.air.empty.out.new
  red.air.safe.out
  red.air.safe.out.new
  red.air.safe.out.test
  red.air.unsafe.out
  red.air.unsafe.out.new
  red.air.unsafe.out.test
  red.color.delim.out
  red.color.delim.out.new
  red.color.delim.out.test
  red.color.empty.out
  red.color.empty.out.new
  red.color.safe.out
  red.color.safe.out.new
  red.color.safe.out.test
  red.color.unsafe.out
  red.color.unsafe.out.new
  red.edit.sh
  red.g
  red.hps.r.out
  red.log
  red.los.empty.out
  red.los.empty.out.new
  red.los.safe.out
  red.los.safe.out.new
  red.los.safe.out.test
  red.los.unsafe.out
  red.los.unsafe.out.new
  red.los.unsafe.out.test
  red.material_id.empty.out
  red.material_id.empty.out.new
  red.material_id.safe.out
  red.material_id.safe.out.new
  red.material_id.safe.out.test
  red.material_id.unsafe.out
  red.material_id.unsafe.out.new
  red.material_id.unsafe.out.test
  red.matrix.noop.out
  red.matrix.noop.out.new
  red.name.empty.out
  red.name.empty.out.new
  red.name.emptyedit.out
  red.name.emptyedit.out.new
  red.name.safe.out
  red.name.safe.out.new
  red.name.safe.out.test
  red.name.unsafe.out
  red.name.unsafe.out.new
  red.name.unsafe.out.test
  red.region.edit-comb.out
  red.region.edit-comb.out.new
  red.region.edit-comb.out.test
  red.region.edit-comb.out.test2
  red.region.empty.out
  red.region.empty.out.new
  red.region.no.out
  red.region.no.out.new
  red.region.one.out
  red.region.one.out.new
  red.region.yes.out
  red.region.yes.out.new
  red.region.zero.out
  red.region.zero.out.new
  red.region_id.empty.out
  red.region_id.empty.out.new
  red.region_id.safe.out
  red.region_id.safe.out.new
  red.region_id.safe.out.test
  red.region_id.unsafe.out
  red.region_id.unsafe.out.new
  red.region_id.unsafe.out.test
  red.shader.empty.out
  red.shader.empty.out.new
  red.shader.safe.out
  red.shader.safe.out.new
  red.shader.safe.out.test
  red.shader.unsafe.out
  red.shader.unsafe.out.new
  red.shader.unsafe.out.test
  red.sph.r.out
  red.sph_rot.c.out
  red.tree.empty.out
  red.tree.empty.out.new
  red.tree.safe.out
  red.tree.safe.out.new
  red.tree.safe.out.test
  red.tree.unsafe.out
  red.tree.unsafe.out.new
  red.virgin.out
  red.virgin.out.new
  region_ids
  regions
  rtedge.2.pix
  rtedge.3.pix
  rtedge.4.pix
  rtedge.5.pix
  rtedge.diff.pix
  rtedge.diff2.pix
  rtedge.diff3.pix
  rtedge.diff4.pix
  rtedge.diff5.pix
  rtedge.havoc.g
  rtedge.log
  rtedge.pix
  rtedge.ref.pix
  rtedge.ref2.pix
  rtedge.ref3.pix
  rtedge.ref4.pix
  rtedge.ref5.pix
  shaders.ebm.bw
  shaders.ell_2.prj
  shaders.g
  shaders.half.prj
  shaders.log
  shaders.mged
  shaders.rt
  shaders.rt.diff.pix
  shaders.rt.log
  shaders.rt.pix
  solids
  solids.dsp.pix
  solids.ebm.bw
  solids.g
  solids.log
  solids.pix.diff
  solids.rt
  solids.rt.log
  solids.rt.pix
  solids.simple.g
  solids.simple.pix.diff
  solids.simple.rt
  solids.simple.rt.log
  solids.simple.rt.pix
  spdi.diff.pix
  spdi.g
  spdi.log
  spdi.mged
  spdi.pix
  weight.g
  weight.log
  weight.mged
  weight.out
  weight.out_ns
  weight.ref
  weight.ref_ns
  weight.test2.g
  weight.test2.mged
  weight.test2.out
  weight.test2.out_ns
  weight.test2.ref
  weight.test2.ref_ns
  )

foreach(outfile ${regress_outfiles})
  set(regress_outfiles_fullpaths ${regress_outfiles_fullpaths} "${CMAKE_CURRENT_BINARY_DIR}/${outfile}")
endforeach(outfile ${regress_outfiles})
set(regress_outfiles_fullpaths ${regress_outfiles_fullpaths} "${CMAKE_BINARY_DIR}/regress_output.log")

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${regress_outfiles_fullpaths}")
DISTCLEAN(${regress_outfiles_fullpaths})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
