set(
  lessons_ADOC
  mged01_creating_primitive_shapes.adoc
  mged02_learning_viewing_options.adoc
  mged03_using_insert_command.adoc
  mged04_assign_mat_prop_rt.adoc
  mged05_learning_boolean_expressions.adoc
  mged06_creating_a_goblet.adoc
  mged07_goblet_material_properties.adoc
  mged08_goblet_material_properties2.adoc
  mged09_globe_in_display_box.adoc
  mged10_creating_mug.adoc
  mged11_refining_mug.adoc
  mged12_mug_through_gui.adoc
  mged13_placing_shapes_in_3d.adoc
  mged14_placing_shapes_in_3d_2.adoc
  mged15_creating_a_toy_truck.adoc
  mged16_mod_tech_struct.adoc
)

if(BRLCAD_EXTRADOCS)
  ADD_ASCIIDOC("HTML" lessons_ADOC "" "")
endif(BRLCAD_EXTRADOCS)

cmakefiles(
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
