/*                           C M D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file cmd.h
 *
 */
#include "common.h"

#include "./ged.h"  /* for MGED_EXTERN */


/* Commands */

/* MGED_EXTERN(int f_list, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv)); */
MGED_EXTERN(int cmd_E, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_adjust, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_aetview, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_arot, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_attr, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_autoview, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_blast, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_bot_decimate, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_bot_face_sort, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_cat, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_center, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_cmd_win, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_color, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_comb, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_comb_std, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_concat, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_copy, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_copyeval, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_dbip, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_dbversion, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_draw, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_dump, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_dup, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_echo, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_emuves, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_erase, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_erase_all, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_ev, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_expand, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_export_body, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_eye_pt, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_find, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_form, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_get, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_get_autoview, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_get_comb, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_get_more_default, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_get_ptr, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_group, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_hide, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_hist, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_import_body, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_instance, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_keep, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_kill, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_killall, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_killtree, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_list, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_lm, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_lookat, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_ls, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_lt, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_make_bb, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_make_name, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_match, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_mged_glob, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_mmenu_get, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_mrot, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_mvall, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_name, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_nmg_collapse, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_nmg_simplify, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_nop, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_oed, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_orientation, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_output_hook, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_overlay, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_parse_points, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_pathlist, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_pathsum, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_pov, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_prcolor, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_push, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_put, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_put_comb, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_redraw_vlist, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_region, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_remove, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_rot, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_rrt, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_rt, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_rt_gettrees, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_rtabort, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_rtarea, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_rtcheck, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_rtedge, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_rtweight, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_sca, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_set_more_default, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_setview, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_shaded_mode, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_shells, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_showmats, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_size, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_solid_report, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_solids_on_ray, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_stuff_str, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_summary, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_title, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_tk, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_tol, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_tops, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_tra, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_tree, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_unhide, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_units, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_vdraw, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_viewget, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_viewset, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_vrot, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_whatid, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_which, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_who, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_xpush, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_zap, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int cmd_zoom, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_3ptarb, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_adc, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_aip, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_amtrack, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_analyze, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_arbdef, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_arced, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_area, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_attach, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_bev, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_binary, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_clone, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_closedb, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_comb_color, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_comm, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_copy_inv, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_copymat, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_debugbu, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_debugdir, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_debuglib, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_debugmem, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_debugnmg, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_decompose, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_delay, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_dm, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_e_muves, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_eac, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_edcodes, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_edcolor, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_edcomb, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_edgedir, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_edmater, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_eqn, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_extrude, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_facedef, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_facetize, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_fhelp, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_fix, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_fracture, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_get_dm_list, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_get_sedit, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_get_sedit_menus, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_get_solid_keypoint, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_grid2model_lu, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_grid2view_lu, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_help, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_hideline, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_history, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_ill, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_in, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_inside, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_itemair, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_joint, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_journal, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_keypoint, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_knob, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_l_muves, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_labelvert, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_loadview, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_make, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_mater, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_matpick, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_memprint, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_mirface, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_mirror, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_model2grid_lu, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_model2view, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_model2view_lu, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_mouse, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_nirt, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_oedit_apply, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_oedit_reset, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_opendb, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_param, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_permute, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_pl, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_plot, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_polybinout, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_prefix, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_press, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_preview, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_ps, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_put_sedit, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_putmat, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_qorot, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_qray, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_quit, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_quit, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_qvrot, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_rcodes, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_read_muves, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_red, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_refresh, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_regdebug, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_regdef, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_release, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_rfarb, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_rmater, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_rmats, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_rot_obj, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_rset, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_savekey, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_saveview, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_sc_obj, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_sed, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_sedit_apply, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_sedit_reset, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_set, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_shader, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_share, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_slewview, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_source, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_status, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_svbase, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_sync, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_t_muves, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_tables, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_tabobj, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_tedit, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_tie, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_tr_obj, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_tracker, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_update, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_view, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_view2grid_lu, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_view2model, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_view2model_lu, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_view2model_vec, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_view_ring, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_vnirt, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_vrmgr, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_vrot_center, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_wait, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_wcodes, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_which_shader, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_winset, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));
MGED_EXTERN(int f_wmater, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
