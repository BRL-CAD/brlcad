#include "cadcommands.h"
#include "cadhelp.h"

void cad_register_commands(CADApp *app)
{
    // Geometry Editing (libged) commands


    app->register_command(QString("3ptarb"), ged_3ptarb);      // cmd_ged_more_wrapper
    app->register_command(QString("adjust"), ged_adjust);
    app->register_command(QString("ae"), ged_aet);             // cmd_ged_view_wrapper
    app->register_command(QString("ae2dir"), ged_ae2dir);
    app->register_command(QString("analyze"), ged_analyze);    // cmd_ged_info_wrapper
    app->register_command(QString("annotate"), ged_annotate);
    app->register_command(QString("arb"), ged_arb);
    app->register_command(QString("arced"), ged_arced);
    app->register_command(QString("attr"), ged_attr);
    app->register_command(QString("bb"), ged_bb);
    app->register_command(QString("bev"), ged_bev);
    app->register_command(QString("bo"), ged_bo);
    app->register_command(QString("bot"), ged_bot);
    app->register_command(QString("bot_condense"), ged_bot_condense);
    app->register_command(QString("bot_decimate"), ged_bot_decimate);
    app->register_command(QString("bot_face_fuse"), ged_bot_face_fuse);
    app->register_command(QString("bot_face_sort"), ged_bot_face_sort);
    app->register_command(QString("bot_flip"), ged_bot_flip);
    app->register_command(QString("bot_fuse"), ged_bot_fuse);
    app->register_command(QString("bot_merge"), ged_bot_merge);
    app->register_command(QString("bot_smooth"), ged_bot_smooth);
    app->register_command(QString("bot_split"), ged_bot_split);
    app->register_command(QString("bot_sync"), ged_bot_sync);
    app->register_command(QString("bot_vertex_fuse"), ged_bot_vertex_fuse);
    app->register_command(QString("brep"), ged_brep); // cmd_ged_view_wrapper
    app->register_command(QString("c"), ged_comb_std);
    app->register_command(QString("cat"), ged_cat); // cmd_ged_info_wrapper
    app->register_command(QString("cc"), ged_cc);
    app->register_command(QString("clone"), ged_clone); // cmd_ged_edit_wrapper
    app->register_command(QString("coil"), ged_coil);
    app->register_command(QString("color"), ged_color);
    app->register_command(QString("comb"), ged_comb);
    app->register_command(QString("comb_color"), ged_comb_color);
    app->register_command(QString("constraint"), ged_constraint);
    app->register_command(QString("copyeval"), ged_copyeval);
    app->register_command(QString("copymat"), ged_copymat);
    app->register_command(QString("cp"), ged_copy);
    app->register_command(QString("d"),  ged_erase); // cmd_ged_erase_wrapper
    app->register_command(QString("db_glob"), ged_glob);
    app->register_command(QString("dbconcat"), ged_concat);
    app->register_command(QString("dbfind"), ged_find); // cmd_ged_info_wrapper
    app->register_command(QString("dbip"), ged_dbip);
    app->register_command(QString("dbversion"), ged_version);
    app->register_command(QString("debugbu"), ged_debugbu);
    app->register_command(QString("debugdir"), ged_debugdir);
    app->register_command(QString("debuglib"), ged_debuglib);
    app->register_command(QString("debugmem"), ged_debugmem);
    app->register_command(QString("debugnmg"), ged_debugnmg);
    app->register_command(QString("decompose"), ged_decompose);
    app->register_command(QString("delay"), ged_delay);
    app->register_command(QString("dir2ae"), ged_dir2ae);
    app->register_command(QString("dump"), ged_dump);
    app->register_command(QString("dup"), ged_dup);
    app->register_command(QString("eac"), ged_eac); // cmd_ged_view_wrapper
    app->register_command(QString("echo"), ged_echo);
    app->register_command(QString("edit"), ged_edit);
    app->register_command(QString("color"), ged_color);
    app->register_command(QString("edcomb"), ged_edcomb);
    app->register_command(QString("erase"), ged_erase); // cmd_ged_erase_wrapper
    app->register_command(QString("expand"), ged_expand);
    app->register_command(QString("eye_pt"), ged_eye); // cmd_ged_view_wrapper
    app->register_command(QString("exists"), ged_exists);
    app->register_command(QString("facetize"), ged_facetize);
    app->register_command(QString("form"), ged_form);
    app->register_command(QString("fracture"), ged_fracture);
    app->register_command(QString("g"), ged_group);
    app->register_command(QString("gdiff"), ged_gdiff);
    app->register_command(QString("get"), ged_get);
    app->register_command(QString("get_autoview"), ged_get_autoview);
    app->register_command(QString("get_comb"), ged_get_comb);
    app->register_command(QString("get_dbip"), ged_dbip);
    app->register_command(QString("graph"), ged_graph);
    app->register_command(QString("gqa"), ged_gqa); // cmd_ged_gqa
    app->register_command(QString("grid2model_lu"), ged_grid2model_lu);
    app->register_command(QString("grid2view_lu"), ged_grid2view_lu);
    app->register_command(QString("hide"), ged_hide);
    app->register_command(QString("i"), ged_instance);
    app->register_command(QString("idents"), ged_tables);
    app->register_command(QString("in"), ged_in); //cmd_ged_in
    app->register_command(QString("inside"), ged_inside); //cmd_ged_inside
    app->register_command(QString("item"), ged_item);
    app->register_command(QString("joint"), ged_joint);
    app->register_command(QString("joint2"), ged_joint2);
    app->register_command(QString("keep"), ged_keep);
    app->register_command(QString("kill"), ged_kill); // cmd_ged_erase_wrapper
    app->register_command(QString("killall"), ged_killall); // cmd_ged_erase_wrapper
    app->register_command(QString("killrefs"), ged_killrefs); // cmd_ged_erase_wrapper
    app->register_command(QString("killtree"), ged_killtree); // cmd_ged_erase_wrapper
    app->register_command(QString("l"), ged_list); // cmd_ged_info_wrapper
    app->register_command(QString("listeval"), ged_pathsum);
    app->register_command(QString("loadview"), ged_loadview); // cmd_ged_view_wrapper
    app->register_command(QString("lod"), ged_lod);
    app->register_command(QString("lookat"), ged_lookat); // cmd_ged_view_wrapper
    app->register_command(QString("ls"), ged_ls);
    app->register_command(QString("lt"), ged_lt);
    app->register_command(QString("m2v_point"), ged_m2v_point);
    app->register_command(QString("make_name"), ged_make_name);
    app->register_command(QString("make_pnts"), ged_make_pnts); // cmd_ged_more_wrapper
    app->register_command(QString("match"), ged_match);
    app->register_command(QString("mater"), ged_mater);
    app->register_command(QString("mirror"), ged_mirror);
    app->register_command(QString("model2grid_lu"), ged_model2grid_lu);
    app->register_command(QString("model2view"), ged_model2view);
    app->register_command(QString("model2view_lu"), ged_model2view_lu);
    app->register_command(QString("mv"), ged_move);
    app->register_command(QString("mvall"), ged_move_all);
    app->register_command(QString("nmg_fix_normals"), ged_nmg_fix_normals);
    app->register_command(QString("nmg_simplify"), ged_nmg_simplify);
    app->register_command(QString("orientation"), ged_orient); // cmd_ged_view_wrapper
    app->register_command(QString("pathlist"), ged_pathlist);
    app->register_command(QString("paths"), ged_pathsum);
    app->register_command(QString("plot"), ged_plot);
    app->register_command(QString("png"), ged_png);
    app->register_command(QString("prcolor"), ged_prcolor);
    app->register_command(QString("prefix"), ged_prefix);
    app->register_command(QString("preview"), ged_preview); // cmd_ged_dm_wrapper
    app->register_command(QString("pull"), ged_pull);
    app->register_command(QString("push"), ged_push);
    app->register_command(QString("put"), ged_put);
    app->register_command(QString("put_comb"), ged_put_comb);
    app->register_command(QString("putmat"), ged_putmat);
    app->register_command(QString("qray"), ged_qray);
    app->register_command(QString("qvrot"), ged_qvrot); // cmd_ged_view_wrapper
    app->register_command(QString("r"), ged_region);
    app->register_command(QString("rcodes"), ged_rcodes);
    app->register_command(QString("regdef"), ged_regdef);
    app->register_command(QString("regions"), ged_tables);
    app->register_command(QString("rm"), ged_remove);
    app->register_command(QString("rmater"), ged_rmater);
    app->register_command(QString("rtabort"), ged_rtabort);
    app->register_command(QString("savekey"), ged_savekey);
    app->register_command(QString("saveview"), ged_saveview);
    app->register_command(QString("screengrab"), ged_screen_grab); // cmd_ged_dm_wrapper
    app->register_command(QString("search"), ged_search);
    app->register_command(QString("select"), ged_select);
    app->register_command(QString("shader"), ged_shader);
    app->register_command(QString("shells"), ged_shells);
    app->register_command(QString("showmats"), ged_showmats);
    app->register_command(QString("simulate"), ged_simulate); // cmd_ged_simulate_wrapper
    app->register_command(QString("solid_report"), ged_report);
    app->register_command(QString("solids"), ged_tables);
    app->register_command(QString("solids_on_ray"), ged_solids_on_ray);
    app->register_command(QString("summary"), ged_summary);
    app->register_command(QString("sync"), ged_sync);
    app->register_command(QString("t"), ged_ls);
    app->register_command(QString("tire"), ged_tire);
    app->register_command(QString("title"), ged_title);
    app->register_command(QString("tops"), ged_tops);
    app->register_command(QString("tree"), ged_tree);
    app->register_command(QString("unhide"), ged_unhide);
    app->register_command(QString("v2m_point"), ged_v2m_point);
    app->register_command(QString("vdraw"), ged_vdraw);
    app->register_command(QString("view"), ged_view_func); // cmd_ged_view_wrapper
    app->register_command(QString("view2grid_lu"), ged_view2grid_lu);
    app->register_command(QString("view2model"), ged_view2model);
    app->register_command(QString("view2model_lu"), ged_view2model_lu);
    app->register_command(QString("view2model_vec"), ged_view2model_vec);
    app->register_command(QString("viewdir"), ged_viewdir);
    app->register_command(QString("voxelize"), ged_voxelize);
    app->register_command(QString("wcodes"), ged_wcodes);
    app->register_command(QString("whatid"), ged_whatid);
    app->register_command(QString("which_shader"), ged_which_shader);
    app->register_command(QString("whichair"), ged_which);
    app->register_command(QString("whichid"), ged_which);
    app->register_command(QString("who"), ged_who);
    app->register_command(QString("wmater"), ged_wmater);
    app->register_command(QString("x"), ged_report);
    app->register_command(QString("xpush"), ged_xpush);

    // GUI commands
    app->register_gui_command(QString("man"), cad_man_view);

    app->register_gui_command(QString("B"), GUI_CMD_PTR_NULL); // cmd_blast
    app->register_gui_command(QString("arot"), GUI_CMD_PTR_NULL); // cmd_arot
    app->register_gui_command(QString("autoview"), GUI_CMD_PTR_NULL); // cmd_autoview
    app->register_gui_command(QString("center"), GUI_CMD_PTR_NULL); // cmd_center
    app->register_gui_command(QString("cmd_win"), GUI_CMD_PTR_NULL); // cmd_cmd_win
    app->register_gui_command(QString("db"), GUI_CMD_PTR_NULL); // cmd_stub
    app->register_gui_command(QString("draw"), GUI_CMD_PTR_NULL); // cmd_draw
    app->register_gui_command(QString("E"), GUI_CMD_PTR_NULL); // cmd_E
    app->register_gui_command(QString("e"), GUI_CMD_PTR_NULL); // cmd_draw
    app->register_gui_command(QString("em"), GUI_CMD_PTR_NULL); // cmd_emuves
    app->register_gui_command(QString("ev"), GUI_CMD_PTR_NULL); // cmd_ev
    app->register_gui_command(QString("get_more_default"), GUI_CMD_PTR_NULL); // cmd_get_more_default
    app->register_gui_command(QString("has_embedded_fb"), GUI_CMD_PTR_NULL); // cmd_has_embedded_fb
    app->register_gui_command(QString("hist"), GUI_CMD_PTR_NULL); // cmd_hist
    app->register_gui_command(QString("lm"), GUI_CMD_PTR_NULL); // cmd_lm
    app->register_gui_command(QString("loadtk"), GUI_CMD_PTR_NULL); // cmd_tk
    app->register_gui_command(QString("mmenu_get"), GUI_CMD_PTR_NULL); // cmd_mmenu_get
    app->register_gui_command(QString("mmenu_set"), GUI_CMD_PTR_NULL); // cmd_nop
    app->register_gui_command(QString("mrot"), GUI_CMD_PTR_NULL); // cmd_mrot
    app->register_gui_command(QString("nmg_collapse"), GUI_CMD_PTR_NULL); // cmd_nmg_collapse
    app->register_gui_command(QString("oed"), GUI_CMD_PTR_NULL); // cmd_oed
    app->register_gui_command(QString("output_hook"), GUI_CMD_PTR_NULL); // cmd_output_hook
    app->register_gui_command(QString("overlay"), GUI_CMD_PTR_NULL); // cmd_overlay
    app->register_gui_command(QString("parse_points"), GUI_CMD_PTR_NULL); // cmd_parse_points
    app->register_gui_command(QString("pov"), GUI_CMD_PTR_NULL); // cmd_pov
    app->register_gui_command(QString("rot"), GUI_CMD_PTR_NULL); // cmd_rot
    app->register_gui_command(QString("rrt"), GUI_CMD_PTR_NULL); // cmd_rrt
    app->register_gui_command(QString("rt"), GUI_CMD_PTR_NULL); // cmd_rt
    app->register_gui_command(QString("rt_gettrees"), GUI_CMD_PTR_NULL); // cmd_rt_gettrees
    app->register_gui_command(QString("rtarea"), GUI_CMD_PTR_NULL); // cmd_rt
    app->register_gui_command(QString("rtcheck"), GUI_CMD_PTR_NULL); // cmd_rt
    app->register_gui_command(QString("rtedge"), GUI_CMD_PTR_NULL); // cmd_rt
    app->register_gui_command(QString("rtweight"), GUI_CMD_PTR_NULL); // cmd_rt
    app->register_gui_command(QString("sca"), GUI_CMD_PTR_NULL); // cmd_sca
    app->register_gui_command(QString("set_more_default"), GUI_CMD_PTR_NULL); // cmd_set_more_default
    app->register_gui_command(QString("setview"), GUI_CMD_PTR_NULL); // cmd_setview
    app->register_gui_command(QString("shaded_mode"), GUI_CMD_PTR_NULL); // cmd_shaded_mode
    app->register_gui_command(QString("size"), GUI_CMD_PTR_NULL); // cmd_size
    app->register_gui_command(QString("stuff_str"), GUI_CMD_PTR_NULL); // cmd_stuff_str
    app->register_gui_command(QString("tol"), GUI_CMD_PTR_NULL); // cmd_tol
    app->register_gui_command(QString("tra"), GUI_CMD_PTR_NULL); // cmd_tra
    app->register_gui_command(QString("units"), GUI_CMD_PTR_NULL); // cmd_units
    app->register_gui_command(QString("viewsize"), GUI_CMD_PTR_NULL); // cmd_size /* alias "size" for saveview scripts */
    app->register_gui_command(QString("vrot"), GUI_CMD_PTR_NULL); // cmd_vrot
    app->register_gui_command(QString("Z"), GUI_CMD_PTR_NULL); // cmd_zap
    app->register_gui_command(QString("zoom"), GUI_CMD_PTR_NULL); // cmd_zoom


}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

