#include "cadcommands.h"
#include "cadhelp.h"

int null_gui_cmd(QString *args, CADApp *app)
{
        Q_UNUSED(app);
        Q_UNUSED(args);

	return 0;
}

void cad_register_commands(CADApp *app)
{
    // Geometry Editing (libged) commands
    app->register_command(QString("3ptarb"), ged_3ptarb);      // cmd_ged_more_wrapper
    app->register_command(QString("adc"), ged_adc);            // to_view_func
    app->register_command(QString("adjust"), ged_adjust);
    app->register_command(QString("ae"), ged_aet);             // cmd_ged_view_wrapper
    app->register_command(QString("aet"), ged_aet);             // cmd_ged_view_wrapper
    app->register_command(QString("ae2dir"), ged_ae2dir);
    app->register_command(QString("analyze"), ged_analyze);    // cmd_ged_info_wrapper
    app->register_command(QString("annotate"), ged_annotate);
    app->register_command(QString("cad::append_pipept"), ged_append_pipept);
    app->register_command(QString("arb"), ged_arb);
    app->register_command(QString("arced"), ged_arced);
    app->register_command(QString("arot"), ged_arot);          // to_view_func_plus
    app->register_command(QString("attr"), ged_attr);
    app->register_command(QString("bb"), ged_bb);
    app->register_command(QString("bev"), ged_bev);
    app->register_command(QString("bo"), ged_bo);
    app->register_command(QString("bot"), ged_bot);
    app->register_command(QString("bot_condense"), ged_bot_condense);
    app->register_command(QString("bot_decimate"), ged_bot_decimate);
    app->register_command(QString("bot_dump"), ged_bot_dump);
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
    app->register_command(QString("combmem"), ged_combmem);
    app->register_command(QString("constraint"), ged_constraint);
    app->register_command(QString("copyeval"), ged_copyeval);
    app->register_command(QString("copymat"), ged_copymat);
    app->register_command(QString("cp"), ged_copy);
    app->register_command(QString("cpi"), ged_cpi);
    app->register_command(QString("d"),  ged_erase); // cmd_ged_erase_wrapper
    app->register_command(QString("db_glob"), ged_glob);
    app->register_command(QString("dbconcat"), ged_concat);
    app->register_command(QString("dbfind"), ged_find); // cmd_ged_info_wrapper
    app->register_command(QString("dbip"), ged_dbip);
    app->register_command(QString("dbversion"), ged_version);
    app->register_command(QString("dbot_dump"), ged_dbot_dump);
    app->register_command(QString("debugbu"), ged_debugbu);
    app->register_command(QString("debugdir"), ged_debugdir);
    app->register_command(QString("debuglib"), ged_debuglib);
    app->register_command(QString("debugmem"), ged_debugmem);
    app->register_command(QString("debugnmg"), ged_debugnmg);
    app->register_command(QString("decompose"), ged_decompose);
    app->register_command(QString("delay"), ged_delay);
    app->register_command(QString("delete_metaballpt"), ged_delete_metaballpt);
    app->register_command(QString("delete_pipept"), ged_delete_pipept);
    app->register_command(QString("dir2ae"), ged_dir2ae);
    app->register_command(QString("draw"), ged_draw); // to_autoview_func
    app->register_command(QString("dump"), ged_dump);
    app->register_command(QString("dup"), ged_dup);
    app->register_command(QString("E"), ged_E); // to_autoview_func
    app->register_command(QString("e"), ged_draw); // to_autoview_func
    app->register_command(QString("eac"), ged_eac); // cmd_ged_view_wrapper
    app->register_command(QString("echo"), ged_echo);
    app->register_command(QString("edarb"), ged_edarb); // to_more_args_func
    app->register_command(QString("edcodes"), ged_edcodes);
    app->register_command(QString("edcolor"), ged_edcolor);
    app->register_command(QString("edcomb"), ged_edcomb);
    app->register_command(QString("edit"), ged_edit);
    app->register_command(QString("edmater"), ged_edmater);
    app->register_command(QString("erase"), ged_erase);  // to_pass_through_and_refresh_func
    app->register_command(QString("ev"), ged_ev); // to_autoview_func
    app->register_command(QString("expand"), ged_expand);  
    app->register_command(QString("exists"), ged_exists);
    app->register_command(QString("eye"), ged_eye); // cmd_ged_view_wrapper
    app->register_command(QString("eye_pos"), ged_eye_pos); // cmd_ged_view_wrapper
    app->register_command(QString("eye_pt"), ged_eye); // cmd_ged_view_wrapper
    app->register_command(QString("facetize"), ged_facetize);
    app->register_command(QString("fb2pix"), ged_fb2pix); // to_view_func
    app->register_command(QString("fbclear"), ged_fbclear); // to_view_func
    app->register_command(QString("find_arb_edge"), ged_find_arb_edge_nearest_pt); // to_view_func
    app->register_command(QString("find_bot_edge"), ged_find_bot_edge_nearest_pt); // to_view_func
    app->register_command(QString("find_botpt"), ged_find_botpt_nearest_pt); // to_view_func
    app->register_command(QString("find_pipept"), ged_find_pipept_nearest_pt); // to_view_func
    app->register_command(QString("form"), ged_form);
    app->register_command(QString("fracture"), ged_fracture);
    app->register_command(QString("g"), ged_group);
    app->register_command(QString("gdiff"), ged_gdiff);
    app->register_command(QString("get"), ged_get);
    app->register_command(QString("get_autoview"), ged_get_autoview);
    app->register_command(QString("get_bot_edges"), ged_get_bot_edges);
    app->register_command(QString("get_comb"), ged_get_comb);
    app->register_command(QString("get_dbip"), ged_dbip);
    app->register_command(QString("get_eyemodel"), ged_get_eyemodel);
    app->register_command(QString("get_type"), ged_get_type);
    app->register_command(QString("glob"), ged_glob);
    app->register_command(QString("gqa"), ged_gqa); // cmd_ged_gqa
    app->register_command(QString("graph"), ged_graph);
    app->register_command(QString("grid"), ged_grid);
    app->register_command(QString("grid2model_lu"), ged_grid2model_lu); // to_view_func_less
    app->register_command(QString("grid2view_lu"), ged_grid2view_lu); // to_view_func_less
    app->register_command(QString("group"), ged_group);
    app->register_command(QString("hide"), ged_hide);
    app->register_command(QString("how"), ged_how);
    app->register_command(QString("human"), ged_human);
    app->register_command(QString("i"), ged_instance);
    app->register_command(QString("idents"), ged_tables);
    app->register_command(QString("illum"), ged_illum); // to_pass_through_and_refresh_func
    app->register_command(QString("importFg4Section"), ged_importFg4Section);
    app->register_command(QString("in"), ged_in); //cmd_ged_in
    app->register_command(QString("inside"), ged_inside); //cmd_ged_inside // to_more_args_func
    app->register_command(QString("item"), ged_item);
    app->register_command(QString("isize"), ged_isize); //to_view_func
    app->register_command(QString("joint"), ged_joint);
    app->register_command(QString("joint2"), ged_joint2);
    app->register_command(QString("keep"), ged_keep);
    app->register_command(QString("keypoint"), ged_keypoint); // to_view_func
    app->register_command(QString("kill"), ged_kill); // cmd_ged_erase_wrapper
    app->register_command(QString("killall"), ged_killall); // cmd_ged_erase_wrapper
    app->register_command(QString("killrefs"), ged_killrefs); // cmd_ged_erase_wrapper
    app->register_command(QString("killtree"), ged_killtree); // cmd_ged_erase_wrapper
    app->register_command(QString("l"), ged_list); // cmd_ged_info_wrapper
    app->register_command(QString("listeval"), ged_pathsum);
    app->register_command(QString("loadview"), ged_loadview); // cmd_ged_view_wrapper
    app->register_command(QString("lod"), ged_lod);
    app->register_command(QString("log"), ged_log);
    app->register_command(QString("lookat"), ged_lookat); // cmd_ged_view_wrapper
    app->register_command(QString("ls"), ged_ls);
    app->register_command(QString("lt"), ged_lt);
    app->register_command(QString("m2v_point"), ged_m2v_point);
    app->register_command(QString("make_name"), ged_make_name);
    app->register_command(QString("make_pnts"), ged_make_pnts); // cmd_ged_more_wrapper
    app->register_command(QString("match"), ged_match);
    app->register_command(QString("mater"), ged_mater);
    app->register_command(QString("memprint"), ged_memprint);
    app->register_command(QString("mirror"), ged_mirror);  // libtclcad calls to_mirror??
    app->register_command(QString("model2grid_lu"), ged_model2grid_lu);
    app->register_command(QString("model2view"), ged_model2view);
    app->register_command(QString("model2view_lu"), ged_model2view_lu);
    app->register_command(QString("mouse_add_metaballpt"), ged_add_metaballpt); //  to_mouse_append_pt_common
    app->register_command(QString("mouse_append_pipept"), ged_append_pipept); // to_mouse_append_pt_common
    app->register_command(QString("mouse_move_metaballpt"), ged_move_metaballpt); // to_mouse_move_pt_common
    app->register_command(QString("mouse_move_pipept"), ged_move_pipept); // to_mouse_move_pt_common
    app->register_command(QString("mouse_prepend_pipept"), ged_prepend_pipept); // to_mouse_append_pt_common
    app->register_command(QString("move_arb_edge"), ged_move_arb_edge);
    app->register_command(QString("move_arb_face"),   ged_move_arb_face);
    app->register_command(QString("move_pipept"), ged_move_pipept); // to_move_pt_common
    app->register_command(QString("move_metaballpt"), ged_move_metaballpt); // to_move_pt_common
    app->register_command(QString("mv"), ged_move);
    app->register_command(QString("mvall"), ged_move_all);
    app->register_command(QString("nirt"), ged_nirt);
    app->register_command(QString("nmg_collapse"), ged_nmg_collapse);
    app->register_command(QString("nmg_fix_normals"), ged_nmg_fix_normals);
    app->register_command(QString("nmg_simplify"), ged_nmg_simplify);
    app->register_command(QString("ocenter"), ged_ocenter);
    app->register_command(QString("open"), ged_reopen);  // to_pass_through_and_refresh_func
    app->register_command(QString("orientation"), ged_orient); // cmd_ged_view_wrapper
    app->register_command(QString("orotate"), ged_orotate);
    app->register_command(QString("oscale"), ged_oscale);
    app->register_command(QString("otranslate"), ged_otranslate);
    app->register_command(QString("overlay"), ged_overlay);  //to_autoview_func
    app->register_command(QString("pathlist"), ged_pathlist);
    app->register_command(QString("paths"), ged_pathsum);
    app->register_command(QString("perspective"), ged_perspective);
    app->register_command(QString("pix2fb"), ged_pix2fb); // to_view_func
    app->register_command(QString("plot"), ged_plot);
    app->register_command(QString("png"), ged_png);  // also a to_png in libtclcad...
    app->register_command(QString("pngwf"), ged_png);  // to_view_func
    app->register_command(QString("polybinout"), ged_polybinout);
    app->register_command(QString("pov"), ged_pmat);
    app->register_command(QString("prcolor"), ged_prcolor);
    app->register_command(QString("prefix"), ged_prefix);
    app->register_command(QString("prepend_pipept"), ged_prepend_pipept);
    app->register_command(QString("preview"), ged_preview); // cmd_ged_dm_wrapper
    app->register_command(QString("protate"), ged_protate);
    app->register_command(QString("ps"), ged_ps);
    app->register_command(QString("pscale"), ged_pscale);
    app->register_command(QString("pset"), ged_pset);
    app->register_command(QString("ptranslate"), ged_ptranslate);
    app->register_command(QString("pull"), ged_pull);
    app->register_command(QString("push"), ged_push);
    app->register_command(QString("put"), ged_put);
    app->register_command(QString("put_comb"), ged_put_comb);
    app->register_command(QString("putmat"), ged_putmat);
    app->register_command(QString("qray"), ged_qray);
    app->register_command(QString("quat"), ged_quat);
    app->register_command(QString("qvrot"), ged_qvrot); // cmd_ged_view_wrapper
    app->register_command(QString("r"), ged_region);
    app->register_command(QString("rect"), ged_rect);
    app->register_command(QString("red"), ged_red);
    app->register_command(QString("rcodes"), ged_rcodes);
    app->register_command(QString("regdef"), ged_regdef);
    app->register_command(QString("regions"), ged_tables);
    app->register_command(QString("report"), ged_report);
    app->register_command(QString("rfarb"), ged_rfarb);
    app->register_command(QString("rm"), ged_remove); // to_pass_through_and_refresh_func
    app->register_command(QString("rmap"), ged_rmap);
    app->register_command(QString("rmat"), ged_rmat); // to_view_func
    app->register_command(QString("rmater"), ged_rmater);
    app->register_command(QString("rot"), ged_rot); // to_view_func_plus
    app->register_command(QString("rot_about"), ged_rotate_about); // to_view_func
    app->register_command(QString("rot_point"), ged_rot_point); // to_view_func
    app->register_command(QString("rotate_arb_face"), ged_rotate_arb_face);
    app->register_command(QString("rrt"), ged_rrt); // to_view_func
    app->register_command(QString("rselect"), ged_rselect); // to_view_func
    app->register_command(QString("rt"), ged_rt); // to_view_func
    app->register_command(QString("rtabort"), ged_rtabort);
    app->register_command(QString("rtarea"), ged_rt); // to_view_func
    app->register_command(QString("rtcheck"), ged_rtcheck); // to_view_func
    app->register_command(QString("rtedge"), ged_rt); // to_view_func
    app->register_command(QString("rtweight"), ged_rt); // to_view_func
    app->register_command(QString("rtwizard"), ged_rtwizard); // to_view_func
    app->register_command(QString("savekey"), ged_savekey);
    app->register_command(QString("saveview"), ged_saveview);
    app->register_command(QString("sca"), ged_scale); // to_view_func_plus
    app->register_command(QString("screengrab"), ged_screen_grab); // cmd_ged_dm_wrapper
    app->register_command(QString("search"), ged_search);
    app->register_command(QString("select"), ged_select); //to_view_func
    app->register_command(QString("set_output_script"), ged_set_output_script);
    app->register_command(QString("set_transparency"), ged_set_transparency);
    app->register_command(QString("set_uplotOutputMode"), ged_set_uplotOutputMode);
    app->register_command(QString("setview"), ged_setview); //to_view_func_plus
    app->register_command(QString("shaded_mode"), ged_shaded_mode);
    app->register_command(QString("shader"), ged_shader);
    app->register_command(QString("shells"), ged_shells);
    app->register_command(QString("showmats"), ged_showmats);
    app->register_command(QString("size"), ged_size);
    app->register_command(QString("simulate"), ged_simulate); // cmd_ged_simulate_wrapper
    app->register_command(QString("slew"), ged_slew);
    app->register_command(QString("solid_report"), ged_report);
    app->register_command(QString("solids"), ged_tables);
    app->register_command(QString("solids_on_ray"), ged_solids_on_ray);
    app->register_command(QString("summary"), ged_summary);
    app->register_command(QString("sync"), ged_sync);
    app->register_command(QString("t"), ged_ls);
    app->register_command(QString("tire"), ged_tire);
    app->register_command(QString("title"), ged_title);
    app->register_command(QString("tol"), ged_tol);
    app->register_command(QString("tops"), ged_tops);
    app->register_command(QString("tree"), ged_tree);
    app->register_command(QString("unhide"), ged_unhide);
    app->register_command(QString("units"), ged_units);
    app->register_command(QString("v2m_point"), ged_v2m_point);
    app->register_command(QString("vdraw"), ged_vdraw);  // to_pass_through_and_refresh_func
    app->register_command(QString("version"), ged_version);
    app->register_command(QString("view"), ged_view_func); // to_view_func_plus
    app->register_command(QString("view2grid_lu"), ged_view2grid_lu);
    app->register_command(QString("view2model"), ged_view2model); // to_view_func_less
    app->register_command(QString("view2model_lu"), ged_view2model_lu);  // to_view_func_less
    app->register_command(QString("view2model_vec"), ged_view2model_vec);  // to_view_func_less
    app->register_command(QString("viewdir"), ged_viewdir);
    app->register_command(QString("vnirt"), ged_vnirt); // to_view_func
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
    app->register_command(QString("ypr"), ged_ypr);
    app->register_command(QString("zap"), ged_zap); // to_pass_through_and_refresh_func
    app->register_command(QString("zoom"), ged_zoom); // to_pass_through_and_refresh_func

    // GUI commands
    app->register_gui_command(QString("man"), cad_man_view);

    app->register_gui_command(QString("B"), null_gui_cmd); // cmd_blast
    app->register_gui_command(QString("cad::base2local"), null_gui_cmd); // cmd_blast
    app->register_gui_command(QString("arot"), null_gui_cmd); // cmd_arot
    app->register_gui_command(QString("autoview"), null_gui_cmd); // cmd_autoview
    app->register_gui_command(QString("cad::bg"), null_gui_cmd); // to_bg
    app->register_gui_command(QString("cad::blast"), null_gui_cmd); // to_blast
    app->register_gui_command(QString("cad::bot_edge_split"), null_gui_cmd); // to_bot_edge_split
    app->register_gui_command(QString("cad::bot_face_split"), null_gui_cmd); // to_bot_face_split
    app->register_gui_command(QString("cad::bounds"), null_gui_cmd); // to_bounds
    app->register_gui_command(QString("clear"), null_gui_cmd); // in libtclcad this is ged_zap, but should be console clear...
    app->register_gui_command(QString("center"), null_gui_cmd); // cmd_center
    app->register_gui_command(QString("cad::configure"), null_gui_cmd); // to_configure
    app->register_gui_command(QString("cad::constrain_rmode"), null_gui_cmd); // to_constrain_rmode
    app->register_gui_command(QString("cad::constrain_tmode"), null_gui_cmd); // to_constrain_tmode
    app->register_gui_command(QString("cmd_win"), null_gui_cmd); // cmd_cmd_win

    app->register_gui_command(QString("cad::data_arrows"), null_gui_cmd);            // to_data_arrows
    app->register_gui_command(QString("cad::data_axes"), null_gui_cmd);              // to_data_axes
    app->register_gui_command(QString("cad::data_labels"), null_gui_cmd);            // to_data_labels
    app->register_gui_command(QString("cad::data_lines"), null_gui_cmd);             // to_data_lines
    app->register_gui_command(QString("cad::data_polygons"), null_gui_cmd);          // to_data_polygons
    app->register_gui_command(QString("cad::data_move"), null_gui_cmd);              // to_data_move
    app->register_gui_command(QString("cad::data_move_object_mode"), null_gui_cmd);  // to_data_move_object_mode
    app->register_gui_command(QString("cad::data_move_point_mode"), null_gui_cmd);   // to_data_move_point_mode
    app->register_gui_command(QString("cad::data_pick"), null_gui_cmd);              // to_data_pick
    app->register_gui_command(QString("cad::data_scale_mode"), null_gui_cmd);        // to_data_scale_mode
    app->register_gui_command(QString("cad::data_vZ"), null_gui_cmd);                // to_data_vZ

    app->register_gui_command(QString("db"), null_gui_cmd); // cmd_stub
    app->register_gui_command(QString("cad::delete_view"), null_gui_cmd); // to_delete_view
    app->register_gui_command(QString("cad::dlist_on"), null_gui_cmd); // to_dlist_on
    app->register_gui_command(QString("cad::edit_motion_delta_callback"), null_gui_cmd); // to_edit_motion_delta_callback
    app->register_gui_command(QString("em"), null_gui_cmd); // cmd_emuves
    app->register_gui_command(QString("cad::faceplace"), null_gui_cmd); // to_faceplate
    app->register_gui_command(QString("cad::fontsize"), null_gui_cmd); // to_fontsize
    app->register_gui_command(QString("get_more_default"), null_gui_cmd); // cmd_get_more_default
    app->register_gui_command(QString("get_prev_mouse"), null_gui_cmd); // to_get_prev_mouse
    app->register_gui_command(QString("handle_expose"), null_gui_cmd); // to_handle_expose
    app->register_gui_command(QString("has_embedded_fb"), null_gui_cmd); // cmd_has_embedded_fb
    app->register_gui_command(QString("hide_view"), null_gui_cmd); // to_hide_view
    app->register_gui_command(QString("hist"), null_gui_cmd); // cmd_hist
    app->register_gui_command(QString("idle_mode"), null_gui_cmd); // to_idle_mode
    app->register_gui_command(QString("init_view_bindings"), null_gui_cmd); // to_init_view_bindings
    app->register_gui_command(QString("light"), null_gui_cmd); // to_light
    app->register_gui_command(QString("list_views"), null_gui_cmd); // to_list_views
    app->register_gui_command(QString("listen"), null_gui_cmd); // to_listen
    app->register_gui_command(QString("lm"), null_gui_cmd); // cmd_lm
    app->register_gui_command(QString("loadtk"), null_gui_cmd); // cmd_tk
    app->register_gui_command(QString("make"), null_gui_cmd); // to_make
    app->register_gui_command(QString("mirror"), null_gui_cmd); // to_mirror - there is a ged_mirror...
    app->register_gui_command(QString("mmenu_get"), null_gui_cmd); // cmd_mmenu_get
    app->register_gui_command(QString("mmenu_set"), null_gui_cmd); // cmd_nop
    app->register_gui_command(QString("model_axes"), null_gui_cmd); // to_model_axes
    app->register_gui_command(QString("more_args_callback"), null_gui_cmd); // to_more_args_callback

    app->register_gui_command(QString("mouse_brep_selection_append"), null_gui_cmd);  // to_mouse_brep_selection_append
    app->register_gui_command(QString("mouse_brep_selection_translate"), null_gui_cmd);  // to_mouse_brep_selection_translate
    app->register_gui_command(QString("mouse_constrain_rot"), null_gui_cmd);  //     to_mouse_constrain_rot
    app->register_gui_command(QString("mouse_constrain_trans"), null_gui_cmd);  //   to_mouse_constrain_trans
    app->register_gui_command(QString("mouse_data_scale"), null_gui_cmd);  //        to_mouse_data_scale
    app->register_gui_command(QString("mouse_find_arb_edge"), null_gui_cmd);  //     to_mouse_find_arb_edge
    app->register_gui_command(QString("mouse_find_bot_edge"), null_gui_cmd);  //     to_mouse_find_bot_edge
    app->register_gui_command(QString("mouse_find_botpt"), null_gui_cmd);  //        to_mouse_find_botpt
    app->register_gui_command(QString("mouse_find_metaballpt"), null_gui_cmd);  //   to_mouse_find_metaballpt
    app->register_gui_command(QString("mouse_find_pipept"), null_gui_cmd);  //       to_mouse_find_pipept
    app->register_gui_command(QString("mouse_joint_select"), null_gui_cmd);  // to_mouse_joint_select
    app->register_gui_command(QString("mouse_joint_selection_translate"), null_gui_cmd);  // to_mouse_joint_selection_translate
    app->register_gui_command(QString("mouse_move_arb_edge"), null_gui_cmd);  //     to_mouse_move_arb_edge
    app->register_gui_command(QString("mouse_move_arb_face"), null_gui_cmd);  //     to_mouse_move_arb_face
    app->register_gui_command(QString("mouse_move_botpt"), null_gui_cmd);  //        to_mouse_move_botpt
    app->register_gui_command(QString("mouse_move_botpts"), null_gui_cmd);  //       to_mouse_move_botpts
    app->register_gui_command(QString("mouse_orotate"), null_gui_cmd);  //   to_mouse_orotate
    app->register_gui_command(QString("mouse_oscale"), null_gui_cmd);  //    to_mouse_oscale
    app->register_gui_command(QString("mouse_otranslate"), null_gui_cmd);  //        to_mouse_otranslate
    app->register_gui_command(QString("mouse_poly_circ"), null_gui_cmd);  // to_mouse_poly_circ
    app->register_gui_command(QString("mouse_poly_cont"), null_gui_cmd);  // to_mouse_poly_cont
    app->register_gui_command(QString("mouse_poly_ell"), null_gui_cmd);  //  to_mouse_poly_ell
    app->register_gui_command(QString("mouse_poly_rect"), null_gui_cmd);  // to_mouse_poly_rect
    app->register_gui_command(QString("mouse_protate"), null_gui_cmd);  //   to_mouse_protate
    app->register_gui_command(QString("mouse_pscale"), null_gui_cmd);  //    to_mouse_pscale
    app->register_gui_command(QString("mouse_ptranslate"), null_gui_cmd);  // to_mouse_ptranslate
    app->register_gui_command(QString("mouse_ray"), null_gui_cmd);  // to_mouse_ray
    app->register_gui_command(QString("mouse_rect"), null_gui_cmd);  // to_mouse_rect
    app->register_gui_command(QString("mouse_rot"), null_gui_cmd);  //  to_mouse_rot
    app->register_gui_command(QString("mouse_rotate_arb_face"), null_gui_cmd);  // to_mouse_rotate_arb_face
    app->register_gui_command(QString("mouse_scale"), null_gui_cmd);  //     to_mouse_scale
    app->register_gui_command(QString("mouse_trans"), null_gui_cmd);  //     to_mouse_trans
    app->register_gui_command(QString("move_arb_edge_mode"), null_gui_cmd);  //      to_move_arb_edge_mode
    app->register_gui_command(QString("move_arb_face_mode"), null_gui_cmd);  //      to_move_arb_face_mode
    app->register_gui_command(QString("move_botpt"), null_gui_cmd);  //      to_move_botpt
    app->register_gui_command(QString("move_botpt_mode"), null_gui_cmd);  // to_move_botpt_mode
    app->register_gui_command(QString("move_botpts"), null_gui_cmd);  //     to_move_botpts
    app->register_gui_command(QString("move_botpts_mode"), null_gui_cmd);  //        to_move_botpts_mode
    app->register_gui_command(QString("move_metaballpt_mode"), null_gui_cmd);  //    to_move_metaballpt_mode
    app->register_gui_command(QString("move_pipept_mode"), null_gui_cmd);  //        to_move_pipept_mode



    app->register_gui_command(QString("mrot"), null_gui_cmd); // cmd_mrot
    app->register_gui_command(QString("new_view"), null_gui_cmd); // to_new_view
    app->register_gui_command(QString("nmg_collapse"), null_gui_cmd); // cmd_nmg_collapse
    app->register_gui_command(QString("oed"), null_gui_cmd); // cmd_oed
    app->register_gui_command(QString("orotate_mode"), null_gui_cmd); // to_orotate_mode
    app->register_gui_command(QString("oscale_mode"), null_gui_cmd); // to_oscale_mode
    app->register_gui_command(QString("otranslate_mode"), null_gui_cmd); // to_otranslate_mode



    app->register_gui_command(QString("output_hook"), null_gui_cmd); // cmd_output_hook
    app->register_gui_command(QString("overlay"), null_gui_cmd); // cmd_overlay
    app->register_gui_command(QString("paint_rect_area"), null_gui_cmd); // to_paint_rect_area
    app->register_gui_command(QString("parse_points"), null_gui_cmd); // cmd_parse_points
    app->register_gui_command(QString("pix"), null_gui_cmd); // to_pix
    app->register_gui_command(QString("poly_circ_mode"), null_gui_cmd);  //  to_poly_circ_mode
    app->register_gui_command(QString("poly_cont_build"), null_gui_cmd);  // to_poly_cont_build
    app->register_gui_command(QString("poly_cont_build_end"), null_gui_cmd);  //   to_poly_cont_build_end
    app->register_gui_command(QString("poly_ell_mode"), null_gui_cmd);  //  to_poly_ell_mode 
    app->register_gui_command(QString("poly_rect_mode"), null_gui_cmd);  //  to_poly_rect_mode
    app->register_gui_command(QString("prim_label"), null_gui_cmd);  //  to_prim_label
    app->register_gui_command(QString("protate_mode"), null_gui_cmd);  // to_protate_mode
    app->register_gui_command(QString("pscale_mode"), null_gui_cmd);  // to_pscale_mode
    app->register_gui_command(QString("ptranslate_mode"), null_gui_cmd);  // to_ptranslate_mode
    app->register_gui_command(QString("rect_mode"), null_gui_cmd);  // to_rect_mode
    app->register_gui_command(QString("redraw"), null_gui_cmd);  // to_redraw
    app->register_gui_command(QString("refresh"), null_gui_cmd);  // to_refresh
    app->register_gui_command(QString("refresh_all"), null_gui_cmd);  // to_refresh_all
    app->register_gui_command(QString("refresh_on"), null_gui_cmd);  // to_refresh_on
			    
    app->register_gui_command(QString("rot"), null_gui_cmd); // cmd_rot

    app->register_gui_command(QString("rotate_arb_face_mode"), null_gui_cmd); // to_rotate_arb_face_mode
    app->register_gui_command(QString("rotate_mode"), null_gui_cmd); // to_rotate_mode
    app->register_gui_command(QString("rt_end_callback"), null_gui_cmd); // to_rt_end_callback
    app->register_gui_command(QString("rt_gettrees"), null_gui_cmd); // cmd_rt_gettrees
    app->register_gui_command(QString("rtarea"), null_gui_cmd); // cmd_rt
    app->register_gui_command(QString("rtcheck"), null_gui_cmd); // cmd_rt
    app->register_gui_command(QString("rtedge"), null_gui_cmd); // cmd_rt
    app->register_gui_command(QString("rtweight"), null_gui_cmd); // cmd_rt
    app->register_gui_command(QString("scale_mode"), null_gui_cmd);  //      to_scale_mode
    app->register_gui_command(QString("screen2model"), null_gui_cmd);  //    to_screen2model
    app->register_gui_command(QString("screen2view"), null_gui_cmd);  //     to_screen2view
    app->register_gui_command(QString("sdata_arrows"), null_gui_cmd);  //    to_data_arrows
    app->register_gui_command(QString("sdata_axes"), null_gui_cmd);  //      to_data_axes
    app->register_gui_command(QString("sdata_labels"), null_gui_cmd);  //    to_data_labels
    app->register_gui_command(QString("sdata_lines"), null_gui_cmd);  //     to_data_lines
    app->register_gui_command(QString("sdata_polygons"), null_gui_cmd);  //  to_data_polygons
    app->register_gui_command(QString("set_coord"), null_gui_cmd);  //  to_set_coord
    app->register_gui_command(QString("set_fb_mode"), null_gui_cmd);  //  to_set_fb_mode

    app->register_gui_command(QString("set_more_default"), null_gui_cmd); // cmd_set_more_default
    app->register_gui_command(QString("snap_view"), null_gui_cmd); // to_snap_view
    app->register_gui_command(QString("stuff_str"), null_gui_cmd); // cmd_stuff_str
    app->register_gui_command(QString("tra"), null_gui_cmd); // cmd_tra
    app->register_gui_command(QString("view_axes"), null_gui_cmd); // to_view_axes
    app->register_gui_command(QString("view_callback"), null_gui_cmd); // to_view_callback
    app->register_gui_command(QString("view_win_size"), null_gui_cmd); // to_view_win_size
    app->register_gui_command(QString("view2screen"), null_gui_cmd); // to_view2screen
    app->register_gui_command(QString("viewsize"), null_gui_cmd); // cmd_size /* alias "size" for saveview scripts */
    app->register_gui_command(QString("vmake"), null_gui_cmd); // to_vmake
    app->register_gui_command(QString("vslew"), null_gui_cmd); // to_vslew
    app->register_gui_command(QString("vrot"), null_gui_cmd); // cmd_vrot
    app->register_gui_command(QString("Z"), null_gui_cmd); // cmd_zap
    app->register_gui_command(QString("zbuffer"), null_gui_cmd); // to_zbuffer
    app->register_gui_command(QString("zclip"), null_gui_cmd); // to_zclip
    app->register_gui_command(QString("zoom"), null_gui_cmd); // cmd_zoom

    // Archer only commands
#if 0
    cd
    closedb
    copy
    cpi
    dbExpand
    delete
    exit
    igraph
    move
    opendb
    p
    pwd
    q
    quit
    scale
    sed
    track
    translate
#endif

    app->register_gui_command(QString("search"), search_postprocess, QString("postprocess"));

}



int search_postprocess(QString *results, CADApp *app)
{
    Q_UNUSED(app);
    QStringList result_items = results->split("\n", QString::SkipEmptyParts);
    QStringList formatted_results;

    if (QString(results->at(0)) != QString("/")) return 1;

    int i = 0;
    int count = result_items.count();
    while (i < count) {
	QString new_item("<a href=\"");
	new_item.append(result_items.at(i));
	new_item.append("\">");
	new_item.append(result_items.at(i));
	new_item.append("</a>");
	if (i < count - 1) {
	    new_item.append("<br>");
	}
	formatted_results.push_back(new_item);
	i++;
    }

    QString local = formatted_results.join(QString(""));

    *results = formatted_results.join(QString(""));

    return 2;
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

