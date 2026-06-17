# Tests for libbsg

brlcad_addexec(test_bsg_vlist test_vlist.c "libbsg;libbu" TEST)
brlcad_addexec(test_bsg_arb8  test_arb8.c  "libbsg;libbu" TEST)

#
# bsg_vlist_cmd_cnt tests
#
brlcad_add_test(NAME libbsg_vlist_null  COMMAND test_bsg_vlist -1)
brlcad_add_test(NAME libbsg_vlist_0    COMMAND test_bsg_vlist  0)
brlcad_add_test(NAME libbsg_vlist_1    COMMAND test_bsg_vlist  1)
brlcad_add_test(NAME libbsg_vlist_5    COMMAND test_bsg_vlist  5)
brlcad_add_test(NAME libbsg_vlist_18   COMMAND test_bsg_vlist 18)
brlcad_add_test(NAME libbsg_vlist_100  COMMAND test_bsg_vlist 100)

#
# bsg_vlist_arb8 — must produce exactly 18 commands
#
brlcad_add_test(NAME libbsg_arb8 COMMAND test_bsg_arb8)

# Scene-graph lifecycle (create/sync/destroy/find/sensor_fire)
brlcad_addexec(test_bsg_scene_graph test_scene_graph.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_scene_graph COMMAND test_bsg_scene_graph)

# Field accessors, sensors, runtime types, payload types
brlcad_addexec(test_bsg_phase6 test_phase6.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_phase6 COMMAND test_bsg_phase6)

# BSG_NODE_LOD lifecycle (create/set_ops/attach_level/cursor)
brlcad_addexec(test_bsg_lod_node test_lod_node.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_lod_node COMMAND test_bsg_lod_node)

# Public database-source metadata
brlcad_addexec(test_bsg_database_source test_database_source.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_database_source COMMAND test_bsg_database_source)

# Identity/revision model API
brlcad_addexec(test_bsg_identity test_identity.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_identity COMMAND test_bsg_identity)

# Material vs appearance split
brlcad_addexec(test_bsg_material_appearance test_material_appearance.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_material_appearance COMMAND test_bsg_material_appearance)

# Object and runtime type system
brlcad_addexec(test_bsg_object test_object.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_object COMMAND test_bsg_object)

# Public typed node refs
brlcad_addexec(test_bsg_node_ref test_node_ref.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_node_ref COMMAND test_bsg_node_ref)

# Public field refs
brlcad_addexec(test_bsg_field_ref test_field_ref.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_field_ref COMMAND test_bsg_field_ref)

# Public typed sensors
brlcad_addexec(test_bsg_sensor_ref test_sensor_ref.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_sensor_ref COMMAND test_bsg_sensor_ref)

# BSG_NODE_VIEW_SCOPE lifecycle (create/visible/destroy)
brlcad_addexec(test_bsg_view_scope test_view_scope.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_view_scope COMMAND test_bsg_view_scope)
brlcad_addexec(test_bsg_view_state test_view_state.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_view_state COMMAND test_bsg_view_state)

# Generic node API (lifecycle/kind/name/child/transform/bounds/revision)
brlcad_addexec(test_bsg_node test_node.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_node COMMAND test_bsg_node)

# Typed scene construction API
brlcad_addexec(test_bsg_scene_builder test_scene_builder.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_scene_builder COMMAND test_bsg_scene_builder)

# Action framework (BBOX/COLLECT/EXPORT)
brlcad_addexec(test_bsg_action test_action.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_action COMMAND test_bsg_action)

# Public traversal state
brlcad_addexec(test_bsg_state test_state.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_state COMMAND test_bsg_state)

# Typed material/draw-style/complexity/camera/light state nodes
brlcad_addexec(test_bsg_state_nodes test_state_nodes.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_state_nodes COMMAND test_bsg_state_nodes)

# Field-backed shape/geometry nodes
brlcad_addexec(test_bsg_geometry_nodes test_geometry_nodes.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_geometry_nodes COMMAND test_bsg_geometry_nodes)

# First-class selection model
brlcad_addexec(test_bsg_selection test_selection.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_selection COMMAND test_bsg_selection)
brlcad_addexec(test_bsg_interaction test_interaction.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_interaction COMMAND test_bsg_interaction)

# Camera snapshot / apply
brlcad_addexec(test_bsg_camera test_camera.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_camera COMMAND test_bsg_camera)

# Renderer-neutral light model
brlcad_addexec(test_bsg_light test_light.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_light COMMAND test_bsg_light)

# Render-request pre-render traversal
brlcad_addexec(test_bsg_render test_render.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_render COMMAND test_bsg_render)
brlcad_addexec(test_bsg_export test_export.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_export COMMAND test_bsg_export)

# Typed pick records and pick/snap/measure actions
brlcad_addexec(test_bsg_pick test_pick.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_pick COMMAND test_bsg_pick)
brlcad_addexec(test_bsg_snap_action test_snap_action.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_snap_action COMMAND test_bsg_snap_action)
brlcad_addexec(test_bsg_measure test_measure.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_measure COMMAND test_bsg_measure)

# HUD root and overlay metadata
brlcad_addexec(test_bsg_feature test_feature.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_feature COMMAND test_bsg_feature)
brlcad_addexec(test_bsg_hud test_hud.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_hud COMMAND test_bsg_hud)
brlcad_addexec(test_bsg_overlay test_overlay.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_overlay COMMAND test_bsg_overlay)

# Render items and backend adapter
brlcad_addexec(test_bsg_render_item test_render_item.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_render_item COMMAND test_bsg_render_item)

# Resolved appearance (appearance_action)
brlcad_addexec(test_bsg_appearance_resolve test_appearance_resolve.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_appearance_resolve COMMAND test_bsg_appearance_resolve)

# Backend adapter invalidation reason mask
brlcad_addexec(test_bsg_backend_adapter test_backend_adapter.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_backend_adapter COMMAND test_bsg_backend_adapter)

# Typed payload coverage
brlcad_addexec(test_bsg_payload_typed test_payload_typed.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_payload_typed COMMAND test_bsg_payload_typed)

# Edit-preview (BSG_PL_EDIT_PREVIEW) payload contract
brlcad_addexec(test_bsg_payload_typed_edit_preview test_payload_typed_edit_preview.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_payload_typed_edit_preview COMMAND test_bsg_payload_typed_edit_preview)

# Draw-intent action API coverage
brlcad_addexec(test_bsg_draw_intent_actions test_draw_intent_actions.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_draw_intent_actions COMMAND test_bsg_draw_intent_actions)

# End-to-end retained drawing architecture acceptance
brlcad_addexec(test_bsg_final_architecture test_final_architecture.c "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_final_architecture COMMAND test_bsg_final_architecture)

# API boundary and source hygiene
brlcad_addexec(test_bsg_api_hygiene test_api_hygiene.c "libbu" TEST)
brlcad_add_test(NAME libbsg_api_hygiene COMMAND test_bsg_api_hygiene ${CMAKE_SOURCE_DIR})

set(
  BSG_PUBLIC_HEADER_ISOLATION_HEADERS
  bsg.h
  bsg/action.h
  bsg/adc.h
  bsg/appearance.h
  bsg/backend_adapter.h
  bsg/backend_scene.h
  bsg/camera.h
  bsg/complexity.h
  bsg/database_source.h
  bsg/defines.h
  bsg/draw_ctx.h
  bsg/draw_intent.h
  bsg/draw_style.h
  bsg/draw_set.h
  bsg/draw_source.h
  bsg/edit_preview.h
  bsg/events.h
  bsg/export.h
  bsg/faceplate.h
  bsg/feature.h
  bsg/field.h
  bsg/geometry.h
  bsg/group.h
  bsg/hud.h
  bsg/identity.h
  bsg/interaction.h
  bsg/light.h
  bsg/lod.h
  bsg/measure.h
  bsg/node.h
  bsg/object.h
  bsg/obol_node.h
  bsg/overlay.h
  bsg/payload.h
  bsg/pick.h
  bsg/plot3.h
  bsg/polygon.h
  bsg/render.h
  bsg/render_item.h
  bsg/scene_builder.h
  bsg/scene_object.h
  bsg/scene_set.h
  bsg/selection.h
  bsg/separator.h
  bsg/sensor.h
  bsg/snap.h
  bsg/snap_action.h
  bsg/state.h
  bsg/switch.h
  bsg/tcl_data.h
  bsg/util.h
  bsg/view_scope.h
  bsg/view_set.h
  bsg/view_state.h
  bsg/vlist.h
)

set(BSG_PUBLIC_HEADER_ISOLATION_SRCS)
foreach(_bsg_header IN LISTS BSG_PUBLIC_HEADER_ISOLATION_HEADERS)
  string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _bsg_header_id "${_bsg_header}")
  set(_bsg_header_src "${CMAKE_CURRENT_BINARY_DIR}/test_public_header_${_bsg_header_id}.c")
  file(WRITE "${_bsg_header_src}"
    "/* Generated by src/libbsg/tests/CMakeLists.txt. */\n#include \"${_bsg_header}\"\nint bsg_public_header_${_bsg_header_id}(void) { return 0; }\n")
  list(APPEND BSG_PUBLIC_HEADER_ISOLATION_SRCS "${_bsg_header_src}")
endforeach()

brlcad_addexec(test_bsg_public_headers "test_public_headers.c;${BSG_PUBLIC_HEADER_ISOLATION_SRCS}" "libbsg;libbu" TEST)
brlcad_add_test(NAME libbsg_public_headers COMMAND test_bsg_public_headers)

foreach(_bsg_stage0_test
    libbsg_api_hygiene
    libbsg_public_headers
    libbsg_action
    libbsg_backend_adapter
    libbsg_database_source
    libbsg_draw_intent_actions
    libbsg_export
    libbsg_feature
    libbsg_field_ref
    libbsg_final_architecture
    libbsg_hud
    libbsg_interaction
    libbsg_lod_node
    libbsg_measure
    libbsg_node
    libbsg_node_ref
    libbsg_object
    libbsg_overlay
    libbsg_payload_typed
    libbsg_payload_typed_edit_preview
    libbsg_pick
    libbsg_render
    libbsg_render_item
    libbsg_scene_builder
    libbsg_scene_graph
    libbsg_selection
    libbsg_sensor_ref
    libbsg_snap_action
    libbsg_state
    libbsg_state_nodes
    libbsg_view_scope
    libbsg_view_state)
  if(TEST ${_bsg_stage0_test})
    set_property(TEST ${_bsg_stage0_test} APPEND PROPERTY LABELS drawing_baseline)
  endif()
endforeach()

cmakefiles(
  CMakeLists.txt
  test_action.c
  test_api_hygiene.c
  test_appearance_resolve.c
  test_arb8.c
  test_backend_adapter.c
  test_camera.c
  test_database_source.c
  test_state_nodes.c
  test_draw_intent_actions.c
  test_export.c
  test_feature.c
  test_field_ref.c
  test_final_architecture.c
  test_identity.c
  test_interaction.c
  test_light.c
  test_lod_node.c
  test_material_appearance.c
  test_measure.c
  test_node.c
  test_node_ref.c
  test_object.c
  test_payload_typed.c
  test_payload_typed_edit_preview.c
  test_phase6.c
  test_pick.c
  test_public_headers.c
  test_hud.c
  test_overlay.c
  test_render.c
  test_render_item.c
  test_scene_builder.c
  test_scene_graph.c
  test_selection.c
  test_sensor_ref.c
  test_snap_action.c
  test_state.c
  test_view_scope.c
  test_view_state.c
  test_vlist.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
