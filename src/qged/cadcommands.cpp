/*                 C A D C O M M A N D S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file cadcommands.cpp
 *
 * Brief description
 *
 */

#include "cadcommands.h"
#include "cadhelp.h"

#include "bu.h"

int null_gui_cmd(QString *args, CADApp *app)
{
        Q_UNUSED(app);
        Q_UNUSED(args);

	return 0;
}

void cad_register_commands(CADApp *app)
{
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

