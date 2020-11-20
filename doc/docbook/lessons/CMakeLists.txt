# Style sheet for XSLT transformation to book HTML pages
if(BRLCAD_EXTRADOCS_HTML)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-lesson-xhtml-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-lesson-xhtml-stylesheet.xsl)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-lesson-xhtml-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS_HTML)
if(BRLCAD_EXTRADOCS_PHP)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-lesson-xhtml-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-lesson-xhtml-stylesheet.xsl)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-lesson-xhtml-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS_PHP)

# Files for PDF products
if(BRLCAD_EXTRADOCS_PDF)
  # Style sheet for XSLT transformation to PDF
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-lesson-fo-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-lesson-fo-stylesheet.xsl)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-lesson-fo-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS_PDF)

# For HTML and FO (FO is an intermediate file used in the
# XML->PDF transformation) we use variables to hold the full
# stylesheet path.
set(XSL_PHP_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/wordpress.xsl")
set(XSL_HTML_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-lesson-xhtml-stylesheet.xsl")
set(XSL_FO_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-lesson-fo-stylesheet.xsl")

set(lessons_EN
  mged01_creating_primitive_shapes.xml
  mged02_learning_viewing_options.xml
  mged03_using_insert_command.xml
  mged04_assign_mat_prop_rt.xml
  mged05_learning_boolean_expressions.xml
  mged06_creating_a_goblet.xml
  mged07_goblet_material_properties.xml
  mged08_goblet_material_properties2.xml
  mged09_globe_in_display_box.xml
  mged10_creating_mug.xml
  mged11_refining_mug.xml
  mged12_mug_through_gui.xml
  mged13_placing_shapes_in_3d.xml
  mged14_placing_shapes_in_3d_2.xml
  mged15_creating_a_toy_truck.xml
  mged16_mod_tech_struct.xml
  )

set(lessons_EN_IMAGES
  images/mged01_commandwindow.png
  images/mged01_graphicswindow.png
  images/mged01_terminal.png
  images/mged02_coordsys.png
  images/mged02_defaultradio.png
  images/mged02_flower_zoomin.png
  images/mged02_flower_zoomout.png
  images/mged02_multipane.png
  images/mged02_radio_35_25.png
  images/mged02_radio_front.png
  images/mged02_radio_right.png
  images/mged02_radio_top.png
  images/mged02_truck_0_0_14.png
  images/mged02_truck_270_-90.png
  images/mged02_truck_270_90.png
  images/mged02_truck_35_0.png
  images/mged02_truck_35_25.png
  images/mged02_truck_35_60.png
  images/mged02_truck_35_90.png
  images/mged02_truck_front.png
  images/mged02_truck_left.png
  images/mged02_truck_rear.png
  images/mged02_truck_right.png
  images/mged03_shapes_in_command.png
  images/mged03_shapes_make_command.png
  images/mged03_wireframe_sph.png
  images/mged03_wireframe_sph_rcc.png
  images/mged04_commandwindow.png
  images/mged04_fb_overlay_mode.png
  images/mged04_raytracedshapes.png
  images/mged05_combinationeditor.png
  images/mged05_intersectionspheres.png
  images/mged05_raytraceddome.png
  images/mged05_raytracedpart1.png
  images/mged05_raytracedpart2.png
  images/mged05_raytracedpart3.png
  images/mged05_subtractionspheres.png
  images/mged05_twoprimitivespheres.png
  images/mged05_unionspheres.png
  images/mged06_base_subtracted_vol.png
  images/mged06_goblet1stsphere.png
  images/mged06_gobletbase.png
  images/mged06_gobletbasestembasin.png
  images/mged06_gobletstem.png
  images/mged06_rtgoblet.png
  images/mged06_rtgobletaz_35_el_25.png
  images/mged07_goblet_complete_window.png
  images/mged07_goblet_mater_complete.png
  images/mged07_goblet_mirror.png
  images/mged07_goblet_semitransparent.png
  images/mged08_comb_editor_window_mater_prop.png
  images/mged08_goblet_combeditor.png
  images/mged08_goblet_reflectivity_1.png
  images/mged08_goblet_reflectivity_2.png
  images/mged08_refractive_index.png
  images/mged08_spec_vs_diff_reflectivity.png
  images/mged09_globe_base_box_raytraced.png
  images/mged09_globe_base_box_wireframe.png
  images/mged09_globe_inside_outside_box.png
  images/mged09_globe_outside_display_box.png
  images/mged09_globe_raytraced.png
  images/mged09_globe_wireframe.png
  images/mged10_mug_wireframe.png
  images/mged11_mug_raytraced_norim.png
  images/mged11_mug_raytraced_rim.png
  images/mged12_cyls_frontview.png
  images/mged12_mug_gui_finished_raytraced.png
  images/mged12_mug_handle_frontview.png
  images/mged12_mug_handle_topview.png
  images/mged12_mug_rim_frontview.png
  images/mged13_candle_comb_ed_checker.png
  images/mged13_candle_complete_front.png
  images/mged13_candle_design.png
  images/mged13_candle_raytraced_overlay.png
  images/mged13_candle_tabletop_base_candle_front.png
  images/mged13_candle_tabletop_base_front.png
  images/mged13_candle_tabletop_eto1_rcc_wireframe_front.png
  images/mged13_candle_tabletop_eto1_wireframe_front.png
  images/mged13_candle_tabletop_wireframe_front.png
  images/mged14_candle_base_sphere_topview.png
  images/mged14_candle_multipane.png
  images/mged14_candle_spheres_az35el25.png
  images/mged14_candle_spheres_frontview.png
  images/mged14_candle_spheres_raytraced.png
  images/mged14_candle_spheres_topview.png
  images/mged14_primitive_editor.png
  images/mged14_primitive_selection_dropdown.png
  images/mged15_cameo_shader.png
  images/mged15_prim_ed_wheel_well.png
  images/mged15_truck_2nd_wheel_wheel_place.png
  images/mged15_truck_ambient_lighting.png
  images/mged15_truck_cab1s.png
  images/mged15_truck_cab_body.png
  images/mged15_truck_cab_cutoff.png
  images/mged15_truck_default_lighting.png
  images/mged15_truck_design.png
  images/mged15_truck_multipane_1stwheel.png
  images/mged15_truck_wheel_wells_wireframe.png
  images/mged15_truck_wheels.png
  images/mged16_radio_casecut_raytrace.png
  images/mged16_radio_cut_raytrace.png
  images/mged16_radio_cut_wireframe.png
  images/mged16_radio_default_components_props.png
  images/mged16_radio_new_design.png
  images/mged16_radio_transparent.png
  images/mged16_radio_wireframe_circ_board.png
  images/mged16_radio_with_mater_props.png
  images/mged16_radio_structure_1.png
  images/mged16_radio_structure_2.png
  images/mged16_radio_structure_3.png
  images/mged16_radio_cutaways.png
  )
if(BRLCAD_EXTRADOCS)
  ADD_DOC(lessons_EN_IMAGES lessons/images)
else(BRLCAD_EXTRADOCS)
  CMAKEFILES(${lessons_EN_IMAGES})
endif(BRLCAD_EXTRADOCS)

ADD_DOCBOOK("HTML;PHP;PDF" lessons_EN lessons lessons_EN_IMAGES_cp)

add_subdirectory(es)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
