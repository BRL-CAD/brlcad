
# Macro for adding commands to libged.  Use in the command subdirectory as follows:
#
# GED_ADD_CMD(CMD_SRCS CMD_INCLUDES CMD_LIBS)
#
# with the last two arguments being optional.
MACRO(GED_ADD_CMD sources)
	get_filename_component(currentdir ${CMAKE_CURRENT_SOURCE_DIR} NAME)
	FOREACH(srcfile ${${sources}})
		LIST(APPEND cmd_srcs "${currentdir}/${srcfile}")
	ENDFOREACH(srcfile ${${sources}})
	SET(LIBGED_CMD_SRCS ${LIBGED_CMD_SRCS} ${cmd_srcs} CACHE INTERNAL "libged cmd srcs" FORCE)
	IF(${ARGV1})
		SET(LIBGED_CMD_INCLUDE_DIRS ${LIBGED_CMD_INCLUDE_DIRS} ${${ARGV1}} CACHE INTERNAL "libged cmd includes" FORCE)
	ENDIF(${ARGV1})
	IF(${ARGV2})
		SET(LIBGED_CMD_LIBS_LIST ${LIBGED_CMD_LIBS_LIST} ${${ARGV2}} CACHE INTERNAL	"libged cmd libs" FORCE)
	ENDIF(${ARGV2})
ENDMACRO(GED_ADD_CMD)

# if this subdir stuff is pushed into the actual subdirs, the autoconf vs cmake
# verification (sh/cmakecheck.sh) will fail on dist-hook.
IF(BULLET_FOUND)
    GED_ADD_CMD(LIBGED_SIM_SOURCES BULLET_INCLUDE_DIR BULLET_LIBRARIES)
    SET(SIM_SRCS simulate/simphysics.cpp simulate/simulate.c simulate/simcollisionalgo.cpp simulate/simcollisionalgo.h)
ELSE(BULLET_FOUND)
    SET(SIM_SRCS "")
    SET(ged_ignore_files simulate/simphysics.cpp simulate/simulate.c simulate/simcollisionalgo.cpp simulate/simcollisionalgo.h)
ENDIF(BULLET_FOUND)

set(LIBGED_SOURCES
    3ptarb.c
    adc.c
    adjust.c
    ae2dir.c
    aet.c
    analyze.c
    annotate.c
    arb.c
    arced.c
    arot.c
    attr.c
    autoview.c
    bb.c
    bev.c
    bigE.c
    blast.c
    bo.c
    bot.c
    bot_condense.c
    bot_decimate.c
    bot_dump.c
    bot_face_fuse.c
    bot_face_sort.c
    bot_flip.c
    bot_merge.c
    bot_smooth.c
    bot_split.c
    bot_sync.c
    bot_vertex_fuse.c
    brep.c
    cat.c
    cc.c
    center.c
    clip.c
    clone.c
    color.c
    comb.c
    combmem.c
    comb_color.c
    comb_std.c
    concat.c
    copy.c
    copymat.c
    copyeval.c
    cpi.c
    dbip.c
    debugbu.c
    debugdir.c
    debuglib.c
    debugmem.c
    debugnmg.c
    decompose.c
    delay.c
    dg_obj.c
    dir2ae.c
    draw.c
    dump.c
    dup.c
    eac.c
    echo.c
    edcodes.c
    edcomb.c
    edit.c
    editit.c
    edmater.c
    edpipe.c
    erase.c
    expand.c
    eye.c
    eye_pos.c
    exists.c
    facetize.c
    fb2pix.c
    find.c
    form.c
    fracture.c
    ged.c
    ged_util.c
    get.c
    get_autoview.c
    get_comb.c
    get_eyemodel.c
    get_obj_bounds.c
    get_solid_kp.c
    get_type.c
    glob.c
    globals.c
    gqa.c
    grid.c
    grid2model_lu.c
    grid2view_lu.c
    group.c
    hide.c
    how.c
    human.c
    illum.c
    importFg4Section.c
    inside.c
    instance.c
    isize.c
    item.c
    keep.c
    keypoint.c
    kill.c
    killall.c
    killrefs.c
    killtree.c
    label.c
    list.c
    loadview.c
    log.c
    lookat.c
    ls.c
    lt.c
    m2v_point.c
    make.c
    make_bb.c
    make_name.c
    make_pnts.c
    match.c
    mater.c
    mirror.c
    model2grid_lu.c
    model2view.c
    model2view_lu.c
    move.c
    move_all.c
    move_arb_edge.c
    move_arb_face.c
    mrot.c
    nirt.c
    nmg_collapse.c
    nmg_fix_normals.c
    nmg_simplify.c
    ocenter.c
    open.c
    orient.c
    orotate.c
    oscale.c
    otranslate.c
    overlay.c
    path.c
    pathlist.c
    pathsum.c
    perspective.c
    pix2fb.c
    plot.c
    pmat.c
    pmodel2view.c
    png.c
    pov.c
    prcolor.c
    prefix.c
    preview.c
    protate.c
    ps.c
    pscale.c
    ptranslate.c
    push.c
    put.c
    put_comb.c
    putmat.c
    qray.c
    quat.c
    qvrot.c
    rcodes.c
    rect.c
    red.c
    regdef.c
    region.c
    remove.c
    report.c
    rfarb.c
    rmap.c
    rmat.c
    rmater.c
    rot.c
    rot_point.c
    rotate_about.c
    rotate_arb_face.c
    rotate_eto.c
    rotate_extrude.c
    rotate_hyp.c
    rotate_tgc.c
    rrt.c
    rt.c
    rtabort.c
    rtcheck.c
    savekey.c
    saveview.c
    scale.c
    scale_ehy.c
    scale_ell.c
    scale_epa.c
    scale_eto.c
    scale_extrude.c
    scale_hyp.c
    scale_part.c
    scale_rhc.c
    scale_rpc.c
    scale_superell.c
    scale_tgc.c
    scale_tor.c
    screengrab.c
    search.c
    select.c
    set_output_script.c
    set_transparency.c
    set_uplotOutputMode.c
    setview.c
    shaded_mode.c
    shader.c
    shells.c
    showmats.c
    size.c
    slew.c
    solids_on_ray.c
    sphgroup.c
    summary.c
    sync.c
    tables.c
    tire.c
    title.c
    tol.c
    tops.c
    tra.c
    trace.c
    track.c
    translate_extrude.c
    translate_tgc.c
    tree.c
    typein.c
    unhide.c
    units.c
    v2m_point.c
    vdraw.c
    version.c
    view.c
    view2grid_lu.c
    view2model.c
    view2model_lu.c
    view2model_vec.c
    view_obj.c
    viewdir.c
    vrot.c
    vutil.c
    wcodes.c
    wdb_bigE.c
    wdb_comb_std.c
    wdb_importFg4Section.c
    wdb_nirt.c
    wdb_obj.c
    wdb_qray.c
    wdb_track.c
    wdb_vdraw.c
    whatid.c
    which.c
    which_shader.c
    who.c
    wmater.c
    xpush.c
    ypr.c
    zap.c
    zoom/zoom.c
    ${SIM_SRCS}
)

# Core logic for adding commands to the libged source/lib/include lists.
FILE(GLOB GED_DIR_CONTENTS ${CMAKE_CURRENT_SOURCE_DIR} *)
FOREACH(item ${GED_DIR_CONTENTS})
	get_filename_component(item_name ${item} NAME)
	IF(IS_DIRECTORY ${item} AND NOT ${item_name} STREQUAL "libged" AND NOT ${item_name} STREQUAL ".svn")
		LIST(APPEND ged_subdirs ${item_name})
	ENDIF(IS_DIRECTORY ${item} AND NOT ${item_name} STREQUAL "libged" AND NOT ${item_name} STREQUAL ".svn")
ENDFOREACH(item ${GED_DIR_CONTENTS})
FOREACH(item ${ged_subdirs})
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${item}/CMakeLists.txt)
		ADD_SUBDIRECTORY(${item})
	ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${item}/CMakeLists.txt)
ENDFOREACH(item ${ged_subdirs})
SET(LIBGED_SOURCES ${LIBGED_SOURCES} ${LIBGED_CMD_SRCS})
IF(LIBGED_CMD_LIBS_LIST)
	LIST(REMOVE_DUPLICATES LIBGED_CMD_LIBS_LIST)
	FOREACH(lib ${LIBGED_CMD_LIBS_LIST})
		SET(LIBGED_CMD_LIBS "${LIBGED_CMD_LIBS} ${lib}")
	ENDFOREACH(lib ${LIBGED_CMD_LIBS_LIST})
ENDIF(LIBGED_CMD_LIBS_LIST)
IF(LIBGED_CMD_INCLUDE_DIRS)
	LIST(REMOVE_DUPLICATES LIBGED_CMD_INCLUDE_DIRS)
ENDIF(LIBGED_CMD_INCLUDE_DIRS)

include_directories(
    ${PNG_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
    ${REGEX_INCLUDE_DIR}
    ${TCL_INCLUDE_DIRS}
	 ${LIBGED_CMD_INCLUDE_DIRS}
)

BRLCAD_ADDLIB(libged "${LIBGED_SOURCES}" "libwdb librt libfb libbu libanalyze ${LIBGED_CMD_LIBS} ${REGEX_LIBRARY} ${WINSOCK_LIB} ${M_LIBRARY}")
SET_TARGET_PROPERTIES(libged PROPERTIES VERSION 19.0.1 SOVERSION 19)

SET(ged_ignore_files
	 ${ged_ignore_files}
    ged_private.h
    qray.h
    simulate/simulate.h
    simulate/simcollisionalgo.h
    wdb_qray.h
)
CMAKEFILES(${ged_ignore_files})
CMAKEFILES(Makefile.am)
