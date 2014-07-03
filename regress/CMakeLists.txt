macro(Regression_Test testname depends_list)
  add_custom_target(regress-${testname}
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/${testname}.sh ${CMAKE_SOURCE_DIR}
    DEPENDS ${depends_list}
    )
  set_target_properties(regress-${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
endmacro(Regression_Test)

# Geometry Conversion Regression Tests (experimental)
add_subdirectory(gcv)

# STEP file conversion (experimental, requires web access)
add_subdirectory(step)

# MGED command tests
add_subdirectory(mged)

if(SH_EXEC)
  Regression_Test(mged mged)
  Regression_Test(moss "rt;asc2g;pixdiff;asc2pix;pix-png;png-pix")
  Regression_Test(lights "rt;asc2g;pixdiff;asc2pix")
  Regression_Test(solids "rt;mged;pixdiff;asc2pix;gencolor")
  Regression_Test(shaders "rt;mged;pixdiff;asc2pix;gencolor")
  Regression_Test(spdi "rt;mged;pixdiff;asc2pix")
  Regression_Test(iges "iges-g;g-iges;mged;asc2g")
  Regression_Test(weight "rtweight;mged")
  Regression_Test(gqa "gqa;mged")
  Regression_Test(fastgen "fast4-g;gdiff")
  Regression_Test(bots "mged;pixdiff;rt")
  Regression_Test(flawfinder "")
  Regression_Test(repository "")
  Regression_Test(red mged)
  Regression_Test(nirt "mged;nirt")
  Regression_Test(comgeom "mged;asc2g;vdeck;comgeom-g")
  #add_custom_target(regress-quote
  #  ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/quote.sh ${CMAKE_SOURCE_DIR}
  #  DEPENDS test_quote
  #  )

  set(rtwizard_deps btclsh rt rtedge fbclear fbserv pix-fb fb-pix pix-bw bw-pix
    pixmatte asc2g asc2pix pixdiff util_tclIndex util_pkgIndex.tcl
    rtwizard_lib_tclIndex rtwizard_lib_pkgIndex.tcl rtwizard_tclIndex
    rtwizard_pkgIndex.tcl rtwizard_TCLSCRIPTS_cp lib_tclIndex
    lib_pkgIndex.tcl)
  Regression_Test(rtwizard "${rtwizard_deps}")

  Regression_Test(dsp "mged;cv;asc2pix;rt;pix-bw")
  Regression_Test(asc2dsp "cv;asc2pix;pix-bw;asc2dsp")

  add_custom_target(regress
    DEPENDS regress-mged regress-moss regress-lights
    regress-solids regress-shaders regress-spdi regress-iges
    regress-weight regress-gqa regress-fastgen regress-bots
    regress-repository regress-red regress-nirt
    regress-comgeom regress-dsp regress-asc2dsp
    )

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
  fastgen.sh
  fastgen_dos.fast4
  flawfinder.sh
  g-dot.sh
  g-nff.sh
  gcv
  gqa.sh
  iges.sh
  library.sh
  lights.sh
  lights_ref.asc
  main.sh
  master_fetch.sh
  master_prep.sh
  mged
  mged.sh
  mged_test.sh
  moss.sh
  mosspix.asc
  nightly.sh
  nirt.sh
  nurbs/brep-3dm.tar.bz2
  quote.sh
  red.sh
  repository.sh
  rtwizard
  rtwizard.sh
  shaders.sh
  shaderspix.asc
  slave_build.sh
  solids.sh
  solids-simplepix.asc
  solidspix.asc
  spdi.sh
  spdipix.asc
  testlib.c
  tgms/cgtest.cg
  tgms/comgeom-tgt-1-v4-documented.cg
  tgms/comgeom-tgt-1-v4.cg
  tgms/dsp.dat
  tgms/m35.asc.gz
  tgms/m35.cg.gz
  tgms/solids-simple.mged
  tgms/solids.mged
  weight.sh
  )

# list of temporary files
set(regress_outfiles
  .density
  adj_air.plot3
  asc2dsp-new.dsp
  asc2dsp-old.bw
  asc2dsp-old.dsp
  asc2dsp-old.pix
  asc2dsp.log
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
  comgeom-g.log
  comgeom.m35.asc
  comgeom.m35-baseline.cg
  comgeom.m35.cg
  comgeom.m35.g
  comgeom.t-v4.g
  comgeom.t-v5.g
  core
  density_table.txt
  dsp.pix
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
  eagleCAD-512x438.pix
  ebm.bw
  exp_air.plot3
  fastgen_box.fast4
  fastgen_dos.g
  fastgen_unix.g
  g-dot.dot
  g-dot.g
  g-dot.log
  g-nff.err
  g-nff.log
  g-nff.m35.asc
  g-nff.m35.g
  g-nff.m35.nff
  gaps.plot3
  gqa.g
  gqa.log
  gqa.mged
  gqa_mged.log
  iges.g
  iges.log
  iges_file.iges
  iges_file2.iges
  iges_file3.iges
  iges_new.g
  iges_stdout.iges
  iges_stdout2.iges
  iges_stdout_new.g
  iges.m35.asc
  iges.m35.g
  lights.g
  lights.log
  lights.pix
  lights_diff.pix
  lights_ref.pix
  mged.log
  moss-diff.log
  moss-png.log
  moss.asc
  moss.g
  moss.log
  moss.pix
  moss.pix.diff
  moss.png
  moss2.pix
  moss_png.diff
  moss_ref.pix
  nirt.g
  nirt.log
  nirt.mged
  nirt.out
  nirt.ref
  nirt_g.log
  overlaps.plot3
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
  red.color.unsafe.out.test
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
  red.sph_rot.c.out
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
  red.region.edit-comb.out.test2
  red.region.edit-comb.out.new
  red.region.edit-comb.out
  red.region.edit-comb.out.test
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
  rtwizard.m35.g
  rtwizard_test1.diff.pix
  rtwizard_test1.pix
  rtwizard_test1.pixdiff.log
  rtwizard_test1_ref.asc
  rtwizard_test1_ref.pix
  shaders.dat
  shaders.g
  shaders.log
  shaders.mged
  shaders.rt
  shaders.rt.diff.pix
  shaders.rt.log
  shaders.rt.pix
  shaders.rt.pixdiff.log
  shaders.txt
  shaders_ref.pix
  solids
  solids-diff.log
  solids-simple-diff.log
  solids-simple.g
  solids-simple.log
  solids-simple.pix.diff
  solids-simple.rt
  solids-simple.rt.log
  solids-simple.rt.pix
  solids-simple_ref.pix
  solids.g
  solids.log
  solids.mged
  solids.pix.diff
  solids.rt
  solids.rt.log
  solids.rt.pix
  solids_ref.pix
  spdi.g
  spdi.log
  spdi.mged
  spdi.pix
  spdi_diff.pix
  spdi_mged.log
  spdi_ref.pix
  t.g
  vdeck.log
  volume.plot3
  )

foreach(outfile ${regress_outfiles})
  set(regress_outfiles_fullpaths ${regress_outfiles_fullpaths} ${CMAKE_CURRENT_BINARY_DIR}/${outfile})
endforeach(outfile ${regress_outfiles})

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${regress_outfiles_fullpaths}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
