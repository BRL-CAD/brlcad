set(
  articles_EN
  TEMPLATE.adoc
  about.adoc
  build_pattern.adoc
  build_region.adoc
  camo_shader.adoc
  ebm_primitive.adoc
  gcv.adoc
  ged.adoc
  main_menu.adoc
  mged.adoc
  mgedrc.adoc
  nirt.adoc
  oed.adoc
  overlap_tool.adoc
  pipes.adoc
  procedural_geometry.adoc
  projection_shader.adoc
  tire.adoc
)

if(BRLCAD_EXTRADOCS)
  add_asciidoc("HTML" articles_EN "articles" "")
endif(BRLCAD_EXTRADOCS)

set(
  articles_EN_IMAGES
  images/Lee_tux.gif
  images/build_pattern_fig01.png
  images/build_pattern_fig02.png
  images/build_pattern_fig03.png
  images/build_pattern_fig04.png
  images/build_pattern_fig05.png
  images/build_pattern_fig06.png
  images/build_pattern_fig07.png
  images/build_region_fig01.png
  images/build_region_fig02.png
  images/build_region_fig03.png
  images/build_region_fig04.png
  images/ebm_primitive_fig01.png
  images/ebm_primitive_fig02.png
  images/ebm_primitive_fig03.png
  images/gcv_architecture.png
  images/gcv_unsupported_tessellation.png
  images/gcv_using_gdiff.png
  images/mgedrc_fig01.png
  images/mgedrc_fig02.png
  images/mgedrc_fig03.png
  images/moss_ssm.gif
  images/nirt_fig01.png
  images/nirt_fig02.png
  images/nirt_fig03.png
  images/nirt_fig04.png
  images/nirt_fig05.png
  images/nirt_fig06.png
  images/nirt_fig07.png
  images/nirt_fig08.png
  images/nirt_fig09.png
  images/nirt_fig10.png
  images/nirt_fig11.png
  images/nirt_fig12.png
  images/nirt_fig13.png
  images/nirt_fig14.png
  images/nirt_fig15.png
  images/nirt_fig16.png
  images/nirt_fig17.png
  images/nirt_fig18.png
  images/nirt_fig19.png
  images/nirt_fig20.png
  images/nirt_fig21.png
  images/nirt_fig22.png
  images/nirt_fig23.png
  images/nirt_fig24.png
  images/oed_fig01.png
  images/oed_fig02.png
  images/oed_fig03.png
  images/oed_fig04.png
  images/oed_fig05.png
  images/oed_fig06.png
  images/oed_fig07.png
  images/oed_fig08.png
  images/oed_fig09.png
  images/oed_fig10.png
  images/oed_fig11.png
  images/oed_fig12.png
  images/oed_fig13.png
  images/overlap_tool_fig01.gif
  images/overlap_tool_fig02.gif
  images/overlap_tool_fig03.png
  images/pipes_fig01.png
  images/pipes_fig02.png
  images/pipes_fig03.png
  images/pipes_fig04.png
  images/pipes_fig05.png
  images/pipes_fig06.png
  images/pipes_fig07.png
  images/pipes_fig08.png
  images/pipes_fig09.png
  images/pipes_fig10.png
  images/pipes_fig11.png
  images/pipes_fig12.png
  images/pipes_fig13.png
  images/projection_shader_fig01.png
  images/projection_shader_fig02.png
  images/projection_shader_fig03.png
  images/projection_shader_fig04.png
  images/projection_shader_fig05.png
  images/projection_shader_fig06.png
  images/projection_shader_fig07.png
  images/tire_fig01.png
  images/tire_fig02.png
  images/tire_fig03.png
  images/tire_fig04.png
  images/tire_fig05.png
  images/tire_fig06.png
  images/tire_fig07.png
  images/tire_fig08.png
  images/tire_fig09.png
  images/tire_fig10.png
  images/tire_fig11.png
  images/tire_fig12.png
  images/tire_fig13.png
  images/tire_fig14.png
  images/base.gif
  images/bool.gif
  images/coord-axes.gif
  images/cup_and_mug.gif
  images/cup_and_mug_small.gif
  images/cup_out+in.gif
  images/cup_outside.gif
  images/cup_w_handle.gif
  images/faceplate.gif
  images/faceplate_menu.gif
  images/faceplate_menu_sm.gif
  images/faceplate_sm.gif
  images/fillet.gif
  images/fillet.jpg
  images/handle.gif
  images/handle.jpg
  images/mug_camo.gif
  images/mug_camo.jpg
  images/mug_green.gif
  images/mug_green.jpg
  images/prims.gif
  images/rim.gif
  images/rim.jpg
  images/gallery_terragen.png
  images/gallery_voxelize.png
  images/gallery_lattice.png
  images/gallery_loftwing.png
  images/gallery_loftwing_prop.png
  images/gallery_gears.png
  images/gallery_helicoil.png
  images/gallery_specimen.png
  images/gallery_sphflake.png
  images/gallery_donuts.png
  images/gallery_menger.png
  images/gallery_pyramid.png
  images/gallery_torii.png
  images/gallery_masonry_frame.png
  images/gallery_masonry_sheetrock.png
  images/gallery_masonry_brick.png
  images/gallery_masonry_bm.png
  images/gallery_brickwall.png
  images/gallery_mkbuilding.png
  images/gallery_cchannel.png
  images/gallery_ringworld.png
  images/gallery_vegetation.png
  images/gallery_clutter.png
  images/gallery_metaball.png
  images/gallery_molecule.png
  images/gallery_pdb-g.png
  images/gallery_pipe.png
  images/gallery_pipetest.png
  images/gallery_tube.png
  images/gallery_lens.png
  images/gallery_brep_simple.png
  images/gallery_brep_cube.png
  images/gallery_brep_cobb.png
  images/gallery_brep_surface.png
  images/gallery_brep_trimmed.png
  images/gallery_brep_invalid.png
  images/gallery_breplicator.png
  images/gallery_csgbrep.png
  images/gallery_surfaceintersect.png
  images/gallery_spltest.png
  images/gallery_tea.png
  images/gallery_wavy.png
  images/gallery_kurt.png
  images/gallery_sketch.png
  images/gallery_wdb_example.png
  images/gallery_globe.png
  images/gallery_bottest.png
  images/gallery_pix2g.png
  images/gallery_bolt.png
  images/gallery_coil.png
  images/gallery_fence.png
  images/gallery_gastank.png
  images/gallery_handle.png
  images/gallery_human.png
  images/gallery_picket_fence.png
  images/gallery_tire.png
  images/gallery_window.png
  images/gallery_window_frame.png
  images/gallery_wire.png
)
if(TARGET mged OR TARGET qged OR TARGET archer)
  if(BRLCAD_EXTRADOCS)
    add_doc(articles_EN_IMAGES html/articles/images)
    add_asciidoc("HTML" articles_EN articles articles_EN_IMAGES_cp)
  endif(BRLCAD_EXTRADOCS)
endif(TARGET mged OR TARGET qged OR TARGET archer)

# Procedural geometry gallery -- opt-in regeneration of the committed snapshot.
#
# The .adoc article and the gallery_*.png images above are a committed snapshot
# (consumed by the normal doc build like any other article).  This target
# rebuilds that snapshot from the actual built tools via a pure-CMake generator
# (no shell, python, or platform assumptions).  It is opt-in and excluded from
# the default build; a developer regenerates, reviews the diff, and commits.
option(BRLCAD_GENERATE_GALLERY "Regenerate the procedural geometry gallery snapshot" OFF)
if(BRLCAD_GENERATE_GALLERY AND TARGET rt AND TARGET mged)
  # Demo generator programs to build before regenerating (those that exist).
  set(
    GALLERY_DEMO_TARGETS
    terragen voxelize lattice loftwing gears helicoil specimen sphflake donuts
    menger pyramid torii masonry brickwall mkbuilding cchannel ringworld
    vegetation clutter metaball molecule pdb-g pipe pipetest tube lens
    brep_simple brep_cube brep_cobb brep_surface brep_trimmed brep_invalid
    breplicator csgbrep surfaceintersect spltest tea wavy kurt sketch
    wdb_example globe bottest pix2g bolt coil fence gastank handle human
    picket_fence tire window window_frame wire
  )
  set(gallery_deps rt mged)
  foreach(t ${GALLERY_DEMO_TARGETS})
    if(TARGET ${t})
      list(APPEND gallery_deps ${t})
    endif(TARGET ${t})
  endforeach(t)

  add_custom_target(
    procedural-gallery-snapshot
    COMMAND
      ${CMAKE_COMMAND} -DRT=$<TARGET_FILE:rt> -DMGED=$<TARGET_FILE:mged>
      -DBINDIR=$<TARGET_FILE_DIR:rt>
      -DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/gallery/gallery_manifest.cmake
      -DIMGDIR=${CMAKE_CURRENT_SOURCE_DIR}/images
      -DADOC=${CMAKE_CURRENT_SOURCE_DIR}/procedural_geometry.adoc
      -DINPUTDIR=${CMAKE_CURRENT_SOURCE_DIR}/gallery/inputs
      -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/gallery_work -DSIZE=512
      "-DTITLE=BRL-CAD Procedural Geometry Gallery"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/gallery/generate_gallery.cmake
    DEPENDS ${gallery_deps}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Regenerating procedural geometry gallery snapshot (images + adoc)"
    VERBATIM
    USES_TERMINAL
  )
  set_target_properties(procedural-gallery-snapshot PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
endif(BRLCAD_GENERATE_GALLERY AND TARGET rt AND TARGET mged)

cmakefiles(
  CMakeLists.txt
  ${articles_EN}
  ${articles_EN_IMAGES}
  images/tire_fig12.svg
  images/tire_fig13.svg
  gallery/gallery_manifest.cmake
  gallery/generate_gallery.cmake
  gallery/inputs/pipe.stdin
  gallery/inputs/wire.stdin
  gallery/inputs/tube.stdin
  gallery/inputs/moss64.pix
  gallery/inputs/caffeine.pdb
  gallery/inputs/mol-cube.dat
)

add_subdirectory(hy)
add_subdirectory(it)
add_subdirectory(ru)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
