# Geometry Conversion Regression Tests (experimental)
add_subdirectory(gcv)

# MGED command tests
add_subdirectory(mged)

if(SH_EXEC)
  add_custom_target(regress-mged
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/mged.sh ${CMAKE_SOURCE_DIR}
    DEPENDS mged
    )
  add_custom_target(regress-moss
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/moss.sh ${CMAKE_SOURCE_DIR}
    DEPENDS rt asc2g pixdiff asc2pix pix-png png-pix
    )
  add_custom_target(regress-lights
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/lights.sh ${CMAKE_SOURCE_DIR}
    DEPENDS rt asc2g pixdiff asc2pix
    )
  add_custom_target(regress-solids
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/solids.sh ${CMAKE_SOURCE_DIR}
    DEPENDS rt mged pixdiff asc2pix gencolor
    )
  add_custom_target(regress-shaders
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/shaders.sh ${CMAKE_SOURCE_DIR}
    DEPENDS rt mged pixdiff asc2pix gencolor
    )
  add_custom_target(regress-spdi
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/spdi.sh ${CMAKE_SOURCE_DIR}
    DEPENDS rt mged pixdiff asc2pix
    )
  add_custom_target(regress-iges
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/iges.sh ${CMAKE_SOURCE_DIR}
    DEPENDS iges-g g-iges mged
    )
  add_custom_target(regress-weight
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/weight.sh ${CMAKE_SOURCE_DIR}
    DEPENDS rtweight mged
    )
  add_custom_target(regress-gqa
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/gqa.sh ${CMAKE_SOURCE_DIR}
    DEPENDS g_qa mged
    )
  add_custom_target(regress-fastgen
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/fastgen.sh ${CMAKE_SOURCE_DIR}
    DEPENDS fast4-g g_diff
    )
  add_custom_target(regress-bots
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/bots.sh ${CMAKE_SOURCE_DIR}
    DEPENDS mged pixdiff rt
    )
  add_custom_target(regress-flawfinder
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/flawfinder.sh ${CMAKE_SOURCE_DIR}
    DEPENDS rt
    )
  add_custom_target(regress-repository
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/repository.sh ${CMAKE_SOURCE_DIR}
    DEPENDS rt
    )
  add_custom_target(regress-red
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/red.sh ${CMAKE_SOURCE_DIR}
    DEPENDS mged
    )
  add_custom_target(regress-vls
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/vls.sh ${CMAKE_SOURCE_DIR}
    DEPENDS test_vls
    )
  add_custom_target(regress-nirt
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/nirt.sh ${CMAKE_SOURCE_DIR}
    DEPENDS mged nirt
    )
  add_custom_target(regress-comgeom
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/comgeom.sh ${CMAKE_SOURCE_DIR}
    DEPENDS mged asc2g vdeck comgeom-g
    )
  add_custom_target(regress-gnff
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/g-nff.sh ${CMAKE_SOURCE_DIR}
    DEPENDS mged g-nff
    )
  add_custom_target(regress-gdot
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/g-dot.sh ${CMAKE_SOURCE_DIR}
    DEPENDS mged g-dot
    )
  add_custom_target(regress-quote
    ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/quote.sh ${CMAKE_SOURCE_DIR}
    DEPENDS test_quote
    )

  add_custom_target(regress
    DEPENDS regress-mged regress-moss regress-lights
    regress-solids regress-shaders regress-spdi regress-iges
    regress-weight regress-gqa regress-fastgen regress-bots
    regress-repository regress-red regress-nirt
    regress-vls regress-comgeom regress-gnff regress-gdot
    regress-quote
    )

endif(SH_EXEC)

CMAKEFILES(
  Makefile.am
  analyze.asc
  bots.sh
  comgeom.sh
  dsp.sh
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
  shaders.sh
  shaderspix.asc
  slave_build.sh
  solids.sh
  solidspix.asc
  spdi.sh
  spdipix.asc
  testlib.c
  tgms/m35.asc.gz
  tgms/m35.cg.gz
  vls.sh
  weight.sh
  )

# list of temporary files
set(regress_outfiles
  .density
  adj_air.pl
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
  core
  density_table.txt
  dsp.dat
  eagleCAD-512x438.pix
  ebm.bw
  exp_air.pl
  fastgen_box.fast4
  fastgen_dos.g
  fastgen_unix.g
  g-dot.dot
  g-dot.g
  g-dot.log
  g-nff.err
  g-nff.g
  g-nff.log
  g-nff.nff
  gaps.pl
  gqa.g
  gqa.log
  gqa.mged
  gqa_mged.log
  iges.g
  iges.log
  iges_file.iges
  iges_file2.iges
  iges_new.g
  iges_stdout.iges
  iges_stdout2.iges
  iges_stdout_new.g
  lights.g
  lights.log
  lights.pix
  lights_diff.pix
  lights_ref.pix
  m35.asc
  m35.cg
  m35.g
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
  overlaps.pl
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
  volume.pl
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
