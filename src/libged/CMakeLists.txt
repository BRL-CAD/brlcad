
# if GED command stuff is pushed into the actual subdirs, the autoconf vs cmake
# verification (sh/cmakecheck.sh) will fail on dist-hook.  For now, just
# handle it as part of libged
if(BRLCAD_ENABLE_BULLET)
  add_definitions(-DHAVE_BULLET=1)
  set(LIBGED_CMD_INCLUDE_DIRS
    ${LIBGED_CMD_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/simulate
    )
  include_directories(SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/simulate/bullet
    ${CMAKE_CURRENT_SOURCE_DIR}/simulate/LinearMath
    ${CMAKE_CURRENT_SOURCE_DIR}/simulate/BulletCollision
    ${CMAKE_CURRENT_SOURCE_DIR}/simulate/BulletDynamics
    ${CMAKE_CURRENT_SOURCE_DIR}/simulate/BulletSoftBody
    )
endif(BRLCAD_ENABLE_BULLET)
include(simulate/bullet/CMakeLists.txt)

if(ADAPTAGRAMS_FOUND)
  set(LIBGED_CMD_LIBS_LIST ${LIBGED_CMD_LIBS_LIST} ${ADAPTAGRAMS_LIBRARIES})
  set(LIBGED_CMD_INCLUDE_DIRS ${LIBGED_CMD_INCLUDE_DIRS} ${AVOID_INCLUDE_DIR})
endif(ADAPTAGRAMS_FOUND)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/debug")
set(DEBUG_CMD_CPP "${CMAKE_CURRENT_BINARY_DIR}/debug_cmd.cpp")

add_custom_command(OUTPUT ${DEBUG_CMD_CPP}
  COMMAND debug2c ${CMAKE_BINARY_DIR}/cmakefiles.cmake ${DEBUG_CMD_CPP}
  DEPENDS debug2c
  COMMENT "[DEBUG2C] Scanning for debugging variables"
  )
DISTCLEAN(${DEBUG_CMD_CPP})
set_source_files_properties(debug/debug.cpp PROPERTIES OBJECT_DEPENDS ${DEBUG_CMD_CPP})

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/env")
set(ENV_CMD_C "${CMAKE_CURRENT_BINARY_DIR}/env_cmd.c")

add_custom_command(OUTPUT ${ENV_CMD_C}
  COMMAND env2c ${CMAKE_BINARY_DIR}/cmakefiles.cmake ${ENV_CMD_C}
  DEPENDS env2c
  COMMENT "[ENV2C] Scanning for environment variable usage"
  )
DISTCLEAN(${ENV_CMD_C})
set_source_files_properties(env/env.c PROPERTIES OBJECT_DEPENDS ${ENV_CMD_C})

set(LIBGED_SOURCES
  ${LIBGED_SOURCES}
  3ptarb/3ptarb.c
  adc/adc.c
  adjust/adjust.c
  ae2dir/ae2dir.c
  analyze/analyze.c
  annotate/annotate.c
  arb/arb.c
  arced/arced.c
  arot/arot.c
  attr/attr.cpp
  autoview/autoview.c
  bb/bb.c
  bev/bev.c
  bigE/bigE.c
  blast/blast.c
  bo/bo.c
  bot/bot.cpp
  bot/check.cpp
  bot/extrude.cpp
  bot/remesh.cpp
  bot_condense/bot_condense.c
  bot_decimate/bot_decimate.c
  bot_dump/bot_dump.c
  bot_face_fuse/bot_face_fuse.c
  bot_face_sort/bot_face_sort.c
  bot_flip/bot_flip.c
  bot_fuse/bot_fuse.c
  bot_merge/bot_merge.c
  bot_smooth/bot_smooth.c
  bot_split/bot_split.c
  bot_sync/bot_sync.c
  bot_vertex_fuse/bot_vertex_fuse.c
  brep/brep.cpp
  brep/conversion.cpp
  brep/csg.cpp
  brep/info.cpp
  brep/intersect.cpp
  brep/pick.cpp
  brep/plot.cpp
  brep/tikz.cpp
  brep/valid.cpp
  cat/cat.c
  cc/cc.c
  check/check.c
  check/check_adj_air.c
  check/check_centroid.c
  check/check_exp_air.c
  check/check_gap.c
  check/check_mass.c
  check/check_moments.c
  check/check_overlaps.c
  check/check_surf_area.c
  check/check_unconf_air.c
  check/check_volume.c
  constraint/constraint.c
  clone/clone.c
  coil/coil.c
  color/color.c
  columns/columns.c
  comb/comb.c
  comb_color/comb_color.c
  comb_std/comb_std.c
  combmem/combmem.c
  concat/concat.cpp
  copy/copy.c
  copyeval/copyeval.c
  copymat/copymat.c
  cpi/cpi.c
  dag/dag.cpp
  dbip/dbip.c
  debug/debug.cpp
  debugbu/debugbu.c
  debugdir/debugdir.c
  debuglib/debuglib.c
  debugnmg/debugnmg.c
  decompose/decompose.c
  delay/delay.c
  dir2ae/dir2ae.c
  display_list/display_list.c
  draw/draw.c
  draw_calc/draw_calc.cpp
  dsp/dsp.c
  dump/dump.c
  dup/dup.c
  eac/eac.c
  echo/echo.c
  edarb/edarb.c
  edbot/edbot.c
  edcodes/edcodes.c
  edcomb/edcomb.c
  edit/edit.c
  edit_metaball/edit_metaball.c
  editit/editit.c
  edmater/edmater.c
  edpipe/edpipe.c
  env/env.c
  erase/erase.c
  exists/exists.c
  expand/expand.c
  eye_pos/eye_pos.c
  facedef/facedef.c
  facetize/facetize.cpp
  facetize_log/facetize_log.c
  fb2pix/fb2pix.c
  fbclear/fbclear.c
  find/find.c
  form/form.c
  fracture/fracture.c
  gdiff/gdiff.c
  gdiff/gdiff.c
  ged.c
  ged_util/ged_util.c
  get/get.c
  get_autoview/get_autoview.c
  get_comb/get_comb.c
  get_eyemodel/get_eyemodel.c
  get_obj_bounds/get_obj_bounds.c
  get_solid_kp/get_solid_kp.c
  get_type/get_type.c
  glob/glob.c
  gqa/gqa.c
  grid/grid.c
  grid2model_lu/grid2model_lu.c
  grid2view_lu/grid2view_lu.c
  group/group.c
  heal/heal.c
  help/help.cpp
  hide/hide.c
  how/how.c
  human/human.c
  illum/illum.c
  importFg4Section/importFg4Section.c
  inside/inside.c
  instance/instance.c
  isize/isize.c
  item/item.c
  joint/joint.c
  joint2/joint2.c
  keep/keep.c
  keypoint/keypoint.c
  kill/kill.c
  killall/killall.c
  killrefs/killrefs.c
  killtree/killtree.c
  label/label.c
  lc/lc.c
  libfuncs/libfuncs.c
  lint/lint.cpp
  list/list.c
  loadview/loadview.c
  lod/lod.c
  log/log.c
  lookat/lookat.c
  ls/ls.c
  lt/lt.c
  m2v_point/m2v_point.c
  make/make.c
  make_name/make_name.c
  match/match.c
  mater/mater.cpp
  mirror/mirror.c
  model2grid_lu/model2grid_lu.c
  model2view/model2view.c
  model2view_lu/model2view_lu.c
  move/move.c
  move_all/move_all.c
  move_arb_edge/move_arb_edge.c
  move_arb_face/move_arb_face.c
  mrot/mrot.c
  nirt/nirt.c
  nmg/nmg.c
  nmg_cmface/nmg_cmface.c
  nmg_collapse/nmg_collapse.c
  nmg_fix_normals/nmg_fix_normals.c
  nmg_kill_f/nmg_kill_f.c
  nmg_kill_v/nmg_kill_v.c
  nmg_make_v/nmg_make_v.c
  nmg_mm/nmg_mm.c
  nmg_move_v/nmg_move_v.c
  nmg_simplify/nmg_simplify.c
  ocenter/ocenter.c
  open/open.c
  orient/orient.c
  orotate/orotate.c
  oscale/oscale.c
  otranslate/otranslate.c
  overlay/overlay.c
  path/path.c
  pathlist/pathlist.c
  pathsum/pathsum.c
  perspective/perspective.c
  pix2fb/pix2fb.c
  plot/plot.c
  pmat/pmat.c
  pmodel2view/pmodel2view.c
  png/png.c
  png2fb/png2fb.c
  pnts/pnts.cpp
  pnts_util.c
  polyclip/polyclip.cpp
  prcolor/prcolor.c
  prefix/prefix.c
  preview/preview.c
  process/process.c
  protate/protate.c
  ps/ps.c
  pscale/pscale.c
  pset/pset.c
  ptranslate/ptranslate.c
  pull/pull.c
  push/push.c
  put/put.c
  put_comb/put_comb.c
  putmat/putmat.c
  qray/qray.c
  qvrot/qvrot.c
  rcodes/rcodes.c
  rect/rect.c
  red/red.c
  regdef/regdef.c
  region/region.c
  remove/remove.c
  rfarb/rfarb.c
  rmap/rmap.c
  rmat/rmat.c
  rmater/rmater.c
  rot/rot.c
  rot_point/rot_point.c
  rotate_about/rotate_about.c
  rotate_arb_face/rotate_arb_face.c
  rotate_eto/rotate_eto.c
  rotate_extrude/rotate_extrude.c
  rotate_hyp/rotate_hyp.c
  rotate_tgc/rotate_tgc.c
  rrt/rrt.c
  rt/rt.c
  rtabort/rtabort.c
  rtcheck/rtcheck.c
  rtwizard/rtwizard.c
  savekey/savekey.c
  saveview/saveview.c
  scale/scale.c
  scale_ehy/scale_ehy.c
  scale_ell/scale_ell.c
  scale_epa/scale_epa.c
  scale_eto/scale_eto.c
  scale_extrude/scale_extrude.c
  scale_hyp/scale_hyp.c
  scale_part/scale_part.c
  scale_rhc/scale_rhc.c
  scale_rpc/scale_rpc.c
  scale_superell/scale_superell.c
  scale_tgc/scale_tgc.c
  scale_tor/scale_tor.c
  screengrab/screengrab.c
  search/search.c
  select/select.c
  set_output_script/set_output_script.c
  set_transparency/set_transparency.c
  set_uplotOutputMode/set_uplotOutputMode.c
  setview/setview.c
  shaded_mode/shaded_mode.c
  shader/shader.c
  shells/shells.c
  showmats/showmats.c
  simulate/ged_command.cpp
  simulate/rt_collision_algorithm.cpp
  simulate/rt_collision_shape.cpp
  simulate/rt_debug_draw.cpp
  simulate/rt_instance.cpp
  simulate/rt_motion_state.cpp
  simulate/simulation.cpp
  simulate/utility.cpp
  slew/slew.c
  solid_report/solid_report.c
  solids_on_ray/solids_on_ray.c
  sphgroup/sphgroup.c
  summary/summary.c
  sync/sync.c
  tables/tables.c
  tire/tire.c
  title/title.c
  tol/tol.c
  tops/tops.c
  tra/tra.c
  trace/trace.c
  track/track.c
  translate_extrude/translate_extrude.c
  translate_tgc/translate_tgc.c
  tree/tree.c
  typein/typein.c
  unhide/unhide.c
  units/units.c
  v2m_point/v2m_point.c
  vdraw/vdraw.c
  version/version.c
  view/aet.c
  view/center.cpp
  view/data_lines.c
  view/eye.c
  view/quat.c
  view/size.c
  view/snap.c
  view/ypr.c
  view/view.c
  view2grid_lu/view2grid_lu.c
  view2model/view2model.c
  view2model_lu/view2model_lu.c
  view2model_vec/view2model_vec.c
  viewdir/viewdir.c
  voxelize/voxelize.c
  vrot/vrot.c
  vutil/vutil.c
  wcodes/wcodes.c
  wdb_importFg4Section/wdb_importFg4Section.c
  whatid/whatid.c
  which/which.cpp
  which_shader/which_shader.c
  who/who.c
  wmater/wmater.c
  xpush/xpush.c
  zap/zap.c
  zoom/zoom.c
  ${LIBGED_CMD_SRCS}
  )

# Include directories needed by libged users
set(GED_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${ANALYZE_INCLUDE_DIRS}
  ${BG_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${NMG_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${BREP_INCLUDE_DIRS}
  )

# Local include directories
# includes from plugins
set(GED_LOCAL_INCLUDE_DIRS
  ${PNG_INCLUDE_DIRS}
  ${REGEX_INCLUDE_DIRS}
  ${LIBGED_CMD_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/../libbg
  ${CMAKE_CURRENT_BINARY_DIR}
  )

BRLCAD_LIB_INCLUDE_DIRS(ged GED_INCLUDE_DIRS GED_LOCAL_INCLUDE_DIRS)

if(BRLCAD_ENABLE_BULLET)
  set(CADBULLET_LIBRARY cadbullet)
endif(BRLCAD_ENABLE_BULLET)

if (BRLCAD_ENABLE_OPENVDB)
  include_directories(${OpenVDB_INCLUDE_DIRS})
  include_directories(${Tbb_INCLUDE_DIRS})
  include_directories(${IlmBase_INCLUDE_DIRS})
  set(LIBGED_CMD_LIBS_LIST ${LIBGED_CMD_LIBS_LIST} "${OpenVDB_LIBRARIES};${IlmBase_LIBRARIES};${Tbb_LIBRARIES}")
  # disable warnings due to unquellable issues in Boost and IlmBase/half headers
  set_property(SOURCE bot_remesh.cpp APPEND PROPERTY COMPILE_FLAGS "${OpenVDB_DEFINITIONS} -w")
endif (BRLCAD_ENABLE_OPENVDB)

BRLCAD_ADDLIB(libged "${LIBGED_SOURCES}" "libwdb;liboptical;librt;libbrep;libnmg;libdm;libbg;libbn;libbu;libicv;libanalyze;${LIBGED_CMD_LIBS_LIST};${CADBULLET_LIBRARY};${PNG_LIBRARIES};${REGEX_LIBRARIES};${WINSOCK_LIB};${M_LIBRARY}")
SET_TARGET_PROPERTIES(libged PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)
add_subdirectory(simulate/tests)

BRLCAD_ADDEXEC(test_help help/test_help.c libged TEST)

set(ged_ignore_files
  CMakeLists.txt
  README
  TODO
  alphanum.h
  bot/ged_bot.h
  brep/ged_brep.h
  check/check_private.h
  ged_private.h
  joint/joint.h
  osg.cpp
  pnts_util.h
  qray.h
  simulate/NOTES
  simulate/rt_collision_algorithm.hpp
  simulate/rt_collision_shape.hpp
  simulate/rt_debug_draw.hpp
  simulate/rt_instance.hpp
  simulate/rt_motion_state.hpp
  simulate/simulation.hpp
  simulate/utility.hpp
  )
CMAKEFILES(${ged_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
