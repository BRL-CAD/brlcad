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

# ASC file Conversion Tests
add_subdirectory(asc)

# DSP Regression Tests
add_subdirectory(dsp)

# Image Conversion Regression Tests
add_subdirectory(icv)

# Geometry Editing Library Regression Tests
add_subdirectory(ged)

# Geometry Conversion Regression Tests
add_subdirectory(gcv)

# moss.g Regression Tests
add_subdirectory(moss)

# NURBS Regression Tests
add_subdirectory(nurbs)

# libpkg Regression Tests
add_subdirectory(pkg)

# Region EDit (red) Regression Tests
add_subdirectory(red)

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

  if (TARGET mged AND TARGET asc2g)

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

    add_test(NAME regress-nirt COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/nirt.sh" ${CMAKE_SOURCE_DIR})
    BRLCAD_REGRESSION_TEST(regress-nirt "mged;nirt" TEST_DEFINED)

    add_test(NAME regress-comgeom COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/comgeom.sh" ${CMAKE_SOURCE_DIR})
    BRLCAD_REGRESSION_TEST(regress-comgeom "mged;asc2g;vdeck;comgeom-g" TEST_DEFINED)

  endif (TARGET mged AND TARGET asc2g)

  add_test(NAME regress-usage COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/usage.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-usage "rt" STAND_ALONE TEST_DEFINED)

  # TODO - test for Python before adding this one
  add_test(NAME regress-flawfinder COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/flawfinder.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-flawfinder "" STAND_ALONE TEST_DEFINED)

endif(SH_EXEC)

CMAKEFILES(
  bots.sh
  comgeom.sh
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
  nirt.sh
  nirt/ovlps.g
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
  asc.log
  nirt.g
  nirt.log
  nirt.mged
  nirt.out
  nirt.ref
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
