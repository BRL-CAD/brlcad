/*                           C M D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file cmd.h
 *
 */

#include "common.h"

#include <tcl.h>
#include "ged.h"

#include "./mged.h"  /* for BU_EXTERN */

typedef int (*tcl_func_ptr)(ClientData, Tcl_Interp *, int, const char *[]);

struct cmdtab {
    const char *name;
    tcl_func_ptr tcl_func;
    ged_func_ptr ged_func;
};


/* Commands */

BU_EXTERN(int cmd_ged_edit_wrapper, (ClientData, Tcl_Interp *, int, const char *argv[]));
BU_EXTERN(int cmd_ged_info_wrapper, (ClientData, Tcl_Interp *, int, const char *argv[]));
BU_EXTERN(int cmd_ged_erase_wrapper, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_ged_gqa, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_ged_in, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_ged_inside, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_ged_more_wrapper, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_ged_plain_wrapper, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_ged_view_wrapper, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_E, (ClientData, Tcl_Interp *, int, const char **));
BU_EXTERN(int cmd_arot, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_autoview, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_blast, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_center, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_cmd_win, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_draw, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_emuves, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_ev, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_export_body, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_get, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_get_comb, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_get_more_default, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_get_ptr, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_has_embedded_fb, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_hist, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_import_body, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_kill, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_list, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_lm, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_ls, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_mmenu_get, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_mrot, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_nmg_collapse, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_nop, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_oed, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_output_hook, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_overlay, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_parse_points, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_pov, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_put, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_put_comb, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_redraw_vlist, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_rot, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_rrt, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_rt, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_rt_gettrees, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_sca, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_search, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_set_more_default, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_setview, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_shaded_mode, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_size, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_stub, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_stuff_str, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_tk, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_tol, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_tra, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_units, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_viewget, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_viewset, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_vrot, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int cmd_zap, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int cmd_zoom, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_3ptarb, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_adc, (ClientData, Tcl_Interp *, int, const char **));
BU_EXTERN(int f_aip, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_amtrack, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_arbdef, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_area, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_attach, (ClientData, Tcl_Interp *, int, const char **));
BU_EXTERN(int f_bev, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_bo, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_bomb, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int f_closedb, (ClientData, Tcl_Interp *, int, const char **));
BU_EXTERN(int f_comm, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int f_copy_inv, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_delay, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_dm, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_e_muves, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_edcodes, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_edcolor, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_edgedir, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_edmater, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int f_eqn, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_extrude, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_facedef, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_facetize, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_fhelp, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_get_dm_list, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_get_sedit, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_get_sedit_menus, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_get_solid_keypoint, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_grid2model_lu, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_grid2view_lu, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_help, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_hideline, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_history, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_ill, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int f_in, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_joint, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_journal, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_keypoint, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_knob, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_l_muves, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_labelvert, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_list, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_make, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_matpick, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_memprint, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_mirface, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_model2grid_lu, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_model2view, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_model2view_lu, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_mouse, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_nirt, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_oedit_apply, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_oedit_reset, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_opendb, (ClientData, Tcl_Interp *, int, const char **));
BU_EXTERN(int f_param, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_permute, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_pl, (ClientData, Tcl_Interp *, int, const char **));
BU_EXTERN(int f_plot, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_polybinout, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_prefix, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_press, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_ps, (ClientData, Tcl_Interp *, int, const char **));
BU_EXTERN(int f_put_sedit, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_qorot, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_qray, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_quit, (ClientData, Tcl_Interp *, int, const char **));
BU_EXTERN(int f_rcodes, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_read_muves, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_red, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_refresh, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_regdebug, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_release, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_rfarb, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_rmats, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_rot_obj, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_rset, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_sc_obj, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_sed, (ClientData, Tcl_Interp *, int, const char *[]));
BU_EXTERN(int f_sedit_apply, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_sedit_reset, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_set, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_share, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_slewview, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_status, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_svbase, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_t_muves, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_tables, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_tedit, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_tie, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_tr_obj, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_tracker, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_update, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_view, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_view2grid_lu, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_view2model, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_view2model_lu, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_view2model_vec, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_view_ring, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_vnirt, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_vrmgr, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_vrot_center, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_wait, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_wcodes, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_which_shader, (ClientData, Tcl_Interp *, int, char **));
BU_EXTERN(int f_winset, (ClientData, Tcl_Interp *, int, char **));


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
