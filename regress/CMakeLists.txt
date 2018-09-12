macro(Add_Regression_Test testname depends_list)
  CMAKE_PARSE_ARGUMENTS(${testname} "EXCLUDE_FROM_REGRESS" "" "" ${ARGN})
  set_target_properties(regress-${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
  if(NOT ${testname}_EXCLUDE_FROM_REGRESS)
    set_tests_properties(regress-${testname} PROPERTIES LABELS "Regression")
  else(NOT ${testname}_EXCLUDE_FROM_REGRESS)
    set_tests_properties(regress-${testname} PROPERTIES LABELS "NOT_WORKING")
  endif(NOT ${testname}_EXCLUDE_FROM_REGRESS)
  if(NOT "${depends_list}" STREQUAL "")
    add_dependencies(regress ${depends_list})
    if(TARGET check)
      add_dependencies(check ${depends_list})
    endif(TARGET check)
  endif(NOT "${depends_list}" STREQUAL "")
  set_tests_properties(regress-${testname} PROPERTIES TIMEOUT 60)
endmacro(Add_Regression_Test)

# Define the toplevel regression target - unless specifically excluded
# via a macro argument, this should collect all regression targets.
include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0)
  set(JFLAG "-j${N}")
else(NOT N EQUAL 0)
  # Huh?  No j flag if we can't get a processor count
  set(JFLAG)
endif(NOT N EQUAL 0)
add_custom_target(regress COMMAND ${CMAKE_CTEST_COMMAND} -L Regression --output-on-failure --output-log "${CMAKE_BINARY_DIR}/regress_output.log" ${JFLAG})
set_target_properties(regress PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(regress PROPERTIES FOLDER "BRL-CAD Regression Tests")

# Image Conversion Regression Tests
add_subdirectory(icv)

# Geometry Conversion Regression Tests
add_subdirectory(gcv)

# RtWizard Image Generation Regression Tests
add_subdirectory(rtwizard)

# MGED command tests
add_subdirectory(mged)

if(SH_EXEC)
  macro(Sh_Regression_Test testname depends_list)
    CMAKE_PARSE_ARGUMENTS(${testname} "EXCLUDE_FROM_REGRESS" "" "" ${ARGN})
    add_test(NAME regress-${testname} COMMAND ${SH_EXEC} "${CMAKE_SOURCE_DIR}/regress/${testname}.sh" ${CMAKE_SOURCE_DIR})
    add_custom_target(regress-${testname} COMMAND ${CMAKE_CTEST_COMMAND} -R ^regress-${testname} --output-on-failure)
    set_target_properties(regress-${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
    if(NOT ${testname}_EXCLUDE_FROM_REGRESS)
      set_tests_properties(regress-${testname} PROPERTIES LABELS "Regression")
    else(NOT ${testname}_EXCLUDE_FROM_REGRESS)
      set_tests_properties(regress-${testname} PROPERTIES LABELS "NOT_WORKING")
    endif(NOT ${testname}_EXCLUDE_FROM_REGRESS)
    if(NOT "${depends_list}" STREQUAL "")
      add_dependencies(regress-${testname} ${depends_list})
      add_dependencies(regress ${depends_list})
      if(TARGET check)
	add_dependencies(check ${depends_list})
      endif(TARGET check)
    endif(NOT "${depends_list}" STREQUAL "")
    set_tests_properties(regress-${testname} PROPERTIES TIMEOUT 60)
  endmacro(Sh_Regression_Test)

  Sh_Regression_Test(moss "rt;asc2g;pixdiff;pix-png;png-pix")
  Sh_Regression_Test(lights "rt;asc2g;pixdiff")
  Sh_Regression_Test(solids "rt;mged;pixdiff;asc2pix;gencolor")
  Sh_Regression_Test(shaders "rt;mged;pixdiff;gencolor")
  Sh_Regression_Test(spdi "rt;mged;pixdiff")
  Sh_Regression_Test(iges "iges-g;g-iges;mged;asc2g")
  Sh_Regression_Test(weight "rtweight;mged")
  Sh_Regression_Test(gqa "gqa;mged")
  Sh_Regression_Test(bots "mged;pixdiff;rt")
  Sh_Regression_Test(usage "rt" EXCLUDE_FROM_REGRESS)
  Sh_Regression_Test(flawfinder "" EXCLUDE_FROM_REGRESS)

  # TODO - keep all tests short.  right now, regress repository
  # violates this as it spends considerable time scanning the entire
  # source tree, then scanning different sets of files for properties.
  Sh_Regression_Test(repository "")
  set_tests_properties(regress-repository PROPERTIES TIMEOUT 600)

  Sh_Regression_Test(mged mged)
  Sh_Regression_Test(red mged)
  Sh_Regression_Test(nirt "mged;nirt")
  Sh_Regression_Test(comgeom "mged;asc2g;vdeck;comgeom-g")
  Sh_Regression_Test(dsp "mged;cv;asc2pix;rt;pix-bw")
  Sh_Regression_Test(asc2dsp "cv;asc2pix;pix-bw;asc2dsp")

endif(SH_EXEC)

CMAKEFILES(
  analyze.asc
  asc2dsp.sh
  bots.sh
  burst/ktank.b
  burst/ktank_air.ids
  burst/ktank_armor.ids
  burst/ktank_crit.ids
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
  g-dot.sh
  g-nff.sh
  gqa.sh
  iges.sh
  library.sh
  lights.pix
  lights.cmake
  lights.sh
  mged.sh
  mged_test.sh
  moss.pix
  moss.sh
  nirt.sh
  nurbs/brep-3dm.tar.bz2
  red.sh
  repository.sh
  shaders.eagleCAD-512x438.pix
  shaders.pix
  shaders.sh
  solids.pix
  solids.sh
  solids.simple.pix
  spdi.pix
  spdi.sh
  tgms/comgeom.cg
  tgms/comgeom.tgt-1-v4-documented.cg
  tgms/comgeom.tgt-1-v4.cg
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
  dsp.2-1.bw
  dsp.2-1.dsp
  dsp.2-1.g
  dsp.2-1.log
  dsp.2-2.bw
  dsp.2-2.dsp
  dsp.2-2.g
  dsp.2-2.log
  dsp.2-3.bw
  dsp.2-3.dsp
  dsp.2-3.g
  dsp.2-3.log
  dsp.2-4.bw
  dsp.2-4.dsp
  dsp.2-4.g
  dsp.2-4.log
  dsp.2-5.bw
  dsp.2-5.dsp
  dsp.2-5.g
  dsp.2-5.log
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
  iges.export2.iges
  iges.g
  iges.import.g
  iges.log
  iges.m35.asc
  iges.m35.g
  iges_file3.iges
  iges_stdout_new.g
  lights.diff.pix
  lights.g
  lights.log
  lights.pix
  lights.ref.pix
  mged.g
  mged.log
  moss.g
  moss.log
  moss.pix
  moss.pix.diff
  moss.pix.png
  moss.pix.png.pix
  moss.ref.pix
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
  repository.log
  shaders.ebm.bw
  shaders.ell_2.prj
  shaders.g
  shaders.half.prj
  shaders.log
  shaders.mged
  shaders.ref.pix
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
  solids.ref.pix
  solids.rt
  solids.rt.log
  solids.rt.pix
  solids.simple.g
  solids.simple.pix.diff
  solids.simple.ref.pix
  solids.simple.rt
  solids.simple.rt.log
  solids.simple.rt.pix
  spdi.diff.pix
  spdi.g
  spdi.log
  spdi.mged
  spdi.pix
  spdi.ref.pix
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
