set(
  LIBBSG_SRCS
  action.c
  appearance.c
  appearance_action.c
  backend_scene.c
  camera.c
  complexity.c
  database_source.c
  draw_intent.c
  draw_style.c
  draw_set.c
  draw_source.c
  export.c
  feature.c
  field.c
  geometry.c
  identity.c
  interaction.c
  line_layer.c
  light.c
  lod.cpp
  lod_node.c
  material.c
  measure.c
  mesh_lod.cpp
  node.c
  node_group.c
  node_root.c
  node_shape.c
  node_transform.c
  object.c
  overlay.c
  payload.c
  payload_typed.c
  pick.c
  hud.c
  render.c
  render_item.c
  render_settings.c
  scene_builder.c
  scene_graph.cpp
  scene_store.c
  selection.c
  sensor.c
  util.cpp
  view_scope.c
  view_state.c
  view_set.cpp
  visibility.c
  vlist.c
  adc.c
  diff.c
  font.c
  hash.c
  knobs.cpp
  polygon.c
  polygon_fill.cpp
  polygon_op.cpp
  snap.c
  snap_action.c
  state.c
)

set(BSG_LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

# Note - libbsg_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
# bv_vlist.c defines PLOT3_IMPLEMENTATION before including bsg/plot3.h, so it
# must be excluded from unity builds to ensure the plot3_* function bodies are
# compiled exactly once.
brlcad_addlib(libbsg "${LIBBSG_SRCS}" "" "${BSG_LOCAL_INCLUDE_DIRS}"
  PUBLIC_LIBS ${libbsg_deps}
  UNITY_BUILD_SKIP vlist.c
)
set_target_properties(libbsg PROPERTIES VERSION 1.0.0 SOVERSION 1)
if(HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET libbsg APPEND PROPERTY COMPILE_DEFINITIONS "BSG_DLL_EXPORTS")
  set_property(TARGET libbsg APPEND PROPERTY COMPILE_DEFINITIONS "PLOT3_DLL_EXPORTS")
  if(TARGET libbsg-obj)
    set_property(TARGET libbsg-obj APPEND PROPERTY COMPILE_DEFINITIONS "BSG_DLL_EXPORTS")
    set_property(TARGET libbsg-obj APPEND PROPERTY COMPILE_DEFINITIONS "PLOT3_DLL_EXPORTS")
  endif(TARGET libbsg-obj)
endif(HIDE_INTERNAL_SYMBOLS)

add_subdirectory(tests)

cmakefiles(
  CMakeLists.txt
  action_private.h
  bsg_private.h
  draw_source_private.h
  field_private.h
  geometry_private.h
  identity_private.h
  node_private.h
  node_storage_private.h
  object_private.h
  overlay_private.h
  payload_private.h
  polygon_private.h
  scene_object_private.h
  scene_store_private.h
  sensor_private.h
  state_private.h
  action.c
  appearance.c
  appearance_action.c
  backend_scene.c
  camera.c
  complexity.c
  database_source.c
  draw_intent.c
  draw_style.c
  draw_set.c
  feature.c
  export.c
  field.c
  geometry.c
  identity.c
  interaction.c
  light.c
  lod_node.c
  material.c
  measure.c
  mesh_lod.cpp
  node.c
  node_group.c
  node_root.c
  node_shape.c
  node_transform.c
  object.c
  overlay.c
  payload.c
  payload_typed.c
  hud.c
  render.c
  render_item.c
  render_settings.c
  scene_builder.c
  scene_store.c
  selection.c
  sensor.c
  adc.c
  diff.c
  font.c
  hash.c
  vlist.c
  util.cpp
  polygon.c
  polygon_fill.cpp
  polygon_op.cpp
  snap.c
  snap_action.c
  state.c
  view_set.cpp
  view_state.c
  visibility.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
