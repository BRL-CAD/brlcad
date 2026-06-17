set(
  test_deps
  dm-swrast
  ged-close
  ged-draw
  ged-dm
  ged-brep
  ged-facetize
  ged-open
  ged-png2fb
  ged-select
  ged-tol
  ged-view
  ged-zap
)

set(
  basic_view_test_ctrls
  v001_ctrl.png
  v002_ctrl.png
  v003_ctrl.png
  v004_ctrl.png
  v005_ctrl.png
  v006_ctrl.png
  v007_ctrl.png
  v008_ctrl.png
  v009_ctrl.png
  v010_ctrl.png
  v011_ctrl.png
  v012_ctrl.png
  v013_ctrl.png
  v014_ctrl.png
  v015_ctrl.png
  v016_ctrl.png
  v017_ctrl.png
  v018_ctrl.png
  v019_ctrl.png
  v020_ctrl.png
  v021_ctrl.png
  v022_ctrl.png
  v023_ctrl.png
  v024_ctrl.png
  v025_ctrl.png
  v026_ctrl.png
)
brlcad_addexec(ged_test_draw "basic.cpp;util.cpp" "libged;libdm" TEST)
if(TARGET ged_test_draw)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_draw ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_draw)
brlcad_add_test(NAME ged_test_drawing_basic COMMAND ged_test_draw -c "${CMAKE_CURRENT_SOURCE_DIR}")

set(
  faceplate_test_ctrls
  fp001_ctrl.png
  fp002_ctrl.png
  fp003_ctrl.png
  fp004_ctrl.png
  fp005_ctrl.png
  fp006_ctrl.png
  fp007_ctrl.png
  fp008_ctrl.png
  fp009_ctrl.png
)
brlcad_addexec(ged_test_faceplate "faceplate.cpp;util.cpp" "libged;libdm" TEST)
if(TARGET ged_test_faceplate)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_faceplate ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_faceplate)
brlcad_add_test(NAME ged_test_drawing_faceplate COMMAND ged_test_faceplate -c "${CMAKE_CURRENT_SOURCE_DIR}")

set(
  lod_test_ctrls
  lod001_ctrl.png
  lod002_ctrl.png
  lod003_ctrl.png
  lod004_ctrl.png
  lod005_ctrl.png
  lod006_ctrl.png
)
brlcad_addexec(ged_test_lod "lod.cpp;util.cpp" "libged;libdm" TEST)
if(TARGET ged_test_lod)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_lod ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_lod)
brlcad_add_test(NAME ged_test_drawing_lod COMMAND ged_test_lod -c "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_lod)
  set_property(TEST ged_test_drawing_lod APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif(TARGET ged_test_lod)
distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_lod_tmp.g)

set(
  select_test_ctrls
  select001_ctrl.png
  select002_ctrl.png
  select003_ctrl.png
  select004_ctrl.png
  select005_ctrl.png
  select006_ctrl.png
  select007_ctrl.png
  select008_ctrl.png
  select009_ctrl.png
  select010_ctrl.png
  select011_ctrl.png
  select012_ctrl.png
  select013_ctrl.png
  select014_ctrl.png
  select015_ctrl.png
  select016_ctrl.png
  select017_ctrl.png
  select018_ctrl.png
  select019_ctrl.png
  select020_ctrl.png
  select021_ctrl.png
  select022_ctrl.png
  select023_ctrl.png
  select024_ctrl.png
  select001_ctrl.png
  select002_ctrl.png
  select003_ctrl.png
  select004_ctrl.png
  select005_ctrl.png
  select006_ctrl.png
  select007_ctrl.png
  select008_ctrl.png
  select009_ctrl.png
  select010_ctrl.png
  select011_ctrl.png
  select012_ctrl.png
  select013_ctrl.png
  select019_ctrl.png
  select020_ctrl.png
  select021_ctrl.png
  select022_ctrl.png
  select023_ctrl.png
  select024_ctrl.png
)
brlcad_addexec(ged_test_select_draw "select.cpp;util.cpp" "libged;libdm" TEST)
if(TARGET ged_test_select_draw)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_select_draw ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_select_draw)
brlcad_add_test(NAME ged_test_drawing_select COMMAND ged_test_select_draw -c "${CMAKE_CURRENT_SOURCE_DIR}")
distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_select_tmp.g)

set(
  quad_test_ctrls
  quad_00_001_ctrl.png
  quad_00_002_ctrl.png
  quad_00_003_ctrl.png
  quad_00_004_ctrl.png
  quad_00_005_ctrl.png
  quad_00_006_ctrl.png
  quad_00_007_ctrl.png
  quad_00_008_ctrl.png
  quad_01_001_ctrl.png
  quad_01_002_ctrl.png
  quad_01_003_ctrl.png
  quad_01_004_ctrl.png
  quad_01_005_ctrl.png
  quad_01_006_ctrl.png
  quad_01_007_ctrl.png
  quad_01_008_ctrl.png
  quad_02_001_ctrl.png
  quad_02_002_ctrl.png
  quad_02_003_ctrl.png
  quad_02_004_ctrl.png
  quad_02_005_ctrl.png
  quad_02_006_ctrl.png
  quad_02_007_ctrl.png
  quad_02_008_ctrl.png
  quad_03_001_ctrl.png
  quad_03_002_ctrl.png
  quad_03_003_ctrl.png
  quad_03_004_ctrl.png
  quad_03_005_ctrl.png
  quad_03_006_ctrl.png
  quad_03_007_ctrl.png
  quad_03_008_ctrl.png
)
brlcad_addexec(ged_test_quad quad.cpp "libged;libdm" TEST)
if(TARGET ged_test_quad)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_quad ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_quad)
brlcad_add_test(NAME ged_test_drawing_quad COMMAND ged_test_quad -c "${CMAKE_CURRENT_SOURCE_DIR}")
distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_quad_tmp.g)

# The idea here is to test view stability across resize events
set(
  aet_test_ctrls
  aet_00_001_ctrl.png
  aet_00_002_ctrl.png
  aet_01_001_ctrl.png
  aet_01_002_ctrl.png
  aet_02_001_ctrl.png
  aet_02_002_ctrl.png
  aet_03_001_ctrl.png
  aet_03_002_ctrl.png
)
brlcad_addexec(ged_test_aet "aet.cpp;util.cpp" "libged;libdm" TEST)
if(TARGET ged_test_aet)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_aet ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_aet)

# BSG render stability across repeated swrast draws
brlcad_addexec(ged_test_bsg_render_stability "bsg_render_stability.cpp;util.cpp" "libged;libdm" TEST)
if(TARGET ged_test_bsg_render_stability)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_bsg_render_stability ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_bsg_render_stability)
brlcad_add_test(NAME ged_test_bsg_render_stability COMMAND ged_test_bsg_render_stability -c "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_bsg_render_stability)
  set_property(TEST ged_test_bsg_render_stability APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()
distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_bsg_render_stability_tmp.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/bsg_render_stability_cache)

# Multi-view BSG render stability and per-view root independence
brlcad_addexec(ged_test_bsg_quad_stability "bsg_quad_stability.cpp;util.cpp" "libged;libdm;libbsg" TEST)
if(TARGET ged_test_bsg_quad_stability)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_bsg_quad_stability ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_bsg_quad_stability)
brlcad_add_test(NAME ged_test_bsg_quad_stability
  COMMAND ged_test_bsg_quad_stability -c "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_bsg_quad_stability)
  set_property(TEST ged_test_bsg_quad_stability
    APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()
distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_bsg_quad_tmp.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/bsg_quad_stability_cache)

# Typed selection, snap, and measure semantic records
brlcad_addexec(ged_test_selection_semantics "selection_semantics.cpp" "libged;libbsg;libbu" TEST)
if(TARGET ged_test_selection_semantics)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_selection_semantics ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_selection_semantics)
brlcad_add_test(NAME ged_test_selection_semantics
  COMMAND ged_test_selection_semantics "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_selection_semantics)
  set_property(TEST ged_test_selection_semantics
    APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()
distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_selection_semantics_tmp.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/ged_selection_semantics_cache)

brlcad_addexec(ged_test_snap_semantics "snap_semantics.cpp" "libbsg;libbu" TEST)
if(TARGET ged_test_snap_semantics)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_snap_semantics ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_snap_semantics)
brlcad_add_test(NAME ged_test_snap_semantics COMMAND ged_test_snap_semantics)
if(TARGET ged_test_snap_semantics)
  set_property(TEST ged_test_snap_semantics
    APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()

brlcad_addexec(ged_test_measure_semantics "measure_semantics.cpp" "libbsg;libbu" TEST)
if(TARGET ged_test_measure_semantics)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_measure_semantics ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_measure_semantics)
brlcad_add_test(NAME ged_test_measure_semantics COMMAND ged_test_measure_semantics)
if(TARGET ged_test_measure_semantics)
  set_property(TEST ged_test_measure_semantics
    APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()

# LoD cross-run cache stability (populate cache → close → reopen → same pixels)
brlcad_addexec(ged_test_lod_crossrun "lod_crossrun.cpp;util.cpp" "libged;libdm" TEST)
if(TARGET ged_test_lod_crossrun)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_lod_crossrun ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_lod_crossrun)
brlcad_add_test(NAME ged_test_lod_crossrun
  COMMAND ged_test_lod_crossrun -c "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_lod_crossrun)
  set_property(TEST ged_test_lod_crossrun
    APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()
distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_lod_cr_tmp.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/lod_crossrun_cache)

# MGED BSG scene and render contract regression tests
brlcad_addexec(ged_test_mged_bsg "mged_bsg.cpp;util.cpp" "libged;libdm;libbsg" TEST)
if(TARGET ged_test_mged_bsg)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_mged_bsg ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_mged_bsg)
brlcad_add_test(NAME ged_test_mged_bsg COMMAND ged_test_mged_bsg -c "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_mged_bsg)
  set_property(TEST ged_test_mged_bsg APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()
distclean(${CMAKE_CURRENT_BINARY_DIR}/mged_bsg_t1.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/mged_bsg_t2.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/mged_bsg_t3.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/mged_bsg_t4.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/mged_bsg_t5.g)

# rtwizard BSG scene-root and render contract regression tests
brlcad_addexec(ged_test_rtwizard_bsg "rtwizard_bsg.cpp;util.cpp" "libged;libdm;libbsg;libicv" TEST)
if(TARGET ged_test_rtwizard_bsg)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_rtwizard_bsg ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_rtwizard_bsg)
brlcad_add_test(NAME ged_test_rtwizard_bsg COMMAND ged_test_rtwizard_bsg -c "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_rtwizard_bsg)
  set_property(TEST ged_test_rtwizard_bsg APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()
distclean(${CMAKE_CURRENT_BINARY_DIR}/rtw_bsg_t1.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/rtw_bsg_t2.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/rtw_bsg_t3.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/rtw_bsg_t4.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/rtw_bsg_t5.g)

# GED draw-scene API regression.
brlcad_addexec(ged_test_ged_draw_scene "ged_draw_scene.cpp" "libged;libbsg" TEST)
if(TARGET ged_test_ged_draw_scene)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_ged_draw_scene ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_ged_draw_scene)
brlcad_add_test(NAME ged_test_ged_draw_scene COMMAND ged_test_ged_draw_scene "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_ged_draw_scene)
  set_property(TEST ged_test_ged_draw_scene APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()
distclean(${CMAKE_CURRENT_BINARY_DIR}/ged_draw_scene_tmp.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/ged_draw_scene_cache)

# Phase T1/T4 (drawing_stack_modernization): BSG overlay feature lifecycle
# structural test — verifies create/find/visit/remove without a display manager.
brlcad_addexec(ged_test_tcl_overlay_bsg "tcl_overlay_bsg.cpp" "libged;libdm" TEST)
if(TARGET ged_test_tcl_overlay_bsg)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_tcl_overlay_bsg ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_tcl_overlay_bsg)
brlcad_add_test(NAME ged_test_tcl_overlay_bsg
  COMMAND ged_test_tcl_overlay_bsg "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_tcl_overlay_bsg)
  set_property(TEST ged_test_tcl_overlay_bsg APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()
distclean(${CMAKE_CURRENT_BINARY_DIR}/tcl_overlay_bsg_tmp.g)

# Phase V6: independent-view state is structural BSG scope, not a mode bit
brlcad_addexec(ged_test_view_independent "view_independent.cpp" "libged;libbsg" TEST)
if(TARGET ged_test_view_independent)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_view_independent ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_view_independent)
brlcad_add_test(NAME ged_test_view_independent
  COMMAND ged_test_view_independent "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_view_independent)
  set_property(TEST ged_test_view_independent APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()

# Phase V7: consolidated view command grammar and aliases
brlcad_addexec(ged_test_view_command "view_command.cpp" "libged;libbsg" TEST)
if(TARGET ged_test_view_command)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_view_command ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_view_command)
brlcad_add_test(NAME ged_test_view_command
  COMMAND ged_test_view_command "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_view_command)
  set_property(TEST ged_test_view_command APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()

# Lighting/zbuffer/zclip regression test (mged_shaded_mode_helper path)
brlcad_addexec(ged_test_mged_shaded_mode_bsg "mged_shaded_mode_bsg.cpp;util.cpp" "libged;libdm;libbsg;libicv" TEST)
if(TARGET ged_test_mged_shaded_mode_bsg)
  foreach(tdp ${test_deps})
    if (TARGET ${tdp})
      add_dependencies(ged_test_mged_shaded_mode_bsg ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_mged_shaded_mode_bsg)
brlcad_add_test(NAME ged_test_mged_shaded_mode_bsg
  COMMAND ged_test_mged_shaded_mode_bsg "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET ged_test_mged_shaded_mode_bsg)
  set_property(TEST ged_test_mged_shaded_mode_bsg
    APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif()
distclean(${CMAKE_CURRENT_BINARY_DIR}/smb_t1.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/smb_t2.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/smb_t3.g)
distclean(${CMAKE_CURRENT_BINARY_DIR}/smb_t4.g)

foreach(_ged_drawing_stage0_test
    ged_test_drawing_basic
    ged_test_drawing_faceplate
    ged_test_drawing_lod
    ged_test_drawing_select
    ged_test_drawing_quad
    ged_test_bsg_render_stability
    ged_test_bsg_quad_stability
    ged_test_selection_semantics
    ged_test_snap_semantics
    ged_test_measure_semantics
    ged_test_lod_crossrun
    ged_test_mged_bsg
    ged_test_rtwizard_bsg
    ged_test_ged_draw_scene
    ged_test_tcl_overlay_bsg
    ged_test_view_independent
    ged_test_view_command
    ged_test_mged_shaded_mode_bsg)
  if(TEST ${_ged_drawing_stage0_test})
    set_property(TEST ${_ged_drawing_stage0_test} APPEND PROPERTY LABELS drawing_baseline)
  endif()
endforeach()

cmakefiles(
  README
  empty.png
  moss.g
  moss.png
  rook.g
  bsg_render_stability.cpp
  bsg_quad_stability.cpp
  ged_draw_scene.cpp
  selection_semantics.cpp
  snap_semantics.cpp
  measure_semantics.cpp
  tcl_overlay_bsg.cpp
  view_independent.cpp
  view_command.cpp
  lod_crossrun.cpp
  mged_bsg.cpp
  mged_shaded_mode_bsg.cpp
  rtwizard_bsg.cpp
  util.cpp
  scripts/verify_main_parity.sh
  ${basic_view_test_ctrls}
  ${faceplate_test_ctrls}
  ${lod_test_ctrls}
  ${select_test_ctrls}
  ${quad_test_ctrls}
  ${aet_test_ctrls}
)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
