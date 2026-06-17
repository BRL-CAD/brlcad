set(
  bsg_headers
  adc.h
  action.h
  annotation.h
  appearance.h
  backend_scene.h
  camera.h
  complexity.h
  database_source.h
  defines.h
  draw_style.h
  draw_ctx.h
  draw_intent.h
  draw_set.h
  draw_source.h
  edit_preview.h
  events.h
  export.h
  faceplate.h
  feature.h
  field.h
  geometry.h
  group.h
  identity.h
  interaction.h
  light.h
  lod.h
  material.h
  measure.h
  node.h
  object.h
  obol_node.h
  overlay.h
  payload.h
  pick.h
  plot3.h
  polygon.h
  render.h
  render_item.h
  backend_adapter.h
  hud.h
  scene_object.h
  scene_builder.h
  scene_set.h
  selection.h
  separator.h
  sensor.h
  snap.h
  snap_action.h
  state.h
  switch.h
  tcl_data.h
  util.h
  view_set.h
  view_scope.h
  view_state.h
  vlist.h
)
brlcad_manage_files(bsg_headers ${INCLUDE_DIR}/brlcad/bsg REQUIRED libbsg)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
