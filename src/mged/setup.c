/*                         S E T U P . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file mged/setup.c
 *
 * routines to initialize mged
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <tcl.h>
#include <itcl.h>
#include <string.h>

/* common headers */
#include "bio.h"
#include "bu.h"
#include "bn.h"
#include "dg.h"
#include "vmath.h"
#include "tclcad.h"
#include "ged.h"

/* local headers */
#include "./mged.h"
#include "./cmd.h"


/* catch auto-formatting errors in this file.  be careful as there are
 * mged_display() vars that use comma too.
 */
#define COMMA ','


extern Tk_Window tkwin; /* in cmd.c */

extern void init_qray(void);
extern void mged_global_variable_setup(Tcl_Interp *interpreter);

const char cmd3525[] = {'3', '5', COMMA, '2', '5', '\0'};
const char cmd4545[] = {'4', '5', COMMA, '4', '5', '\0'};

static struct cmdtab mged_cmdtab[] = {
    {"%", f_comm, GED_FUNC_PTR_NULL},
    {cmd3525, f_bv_35_25, GED_FUNC_PTR_NULL}, /* 35,25 */
    {"3ptarb", cmd_ged_more_wrapper, ged_3ptarb},
    {cmd4545, f_bv_45_45, GED_FUNC_PTR_NULL}, /* 45,45 */
    {"B", cmd_blast, GED_FUNC_PTR_NULL},
    {"accept", f_be_accept, GED_FUNC_PTR_NULL},
    {"adc", f_adc, GED_FUNC_PTR_NULL},
    {"adjust", cmd_ged_plain_wrapper, ged_adjust},
    {"ae", cmd_ged_view_wrapper, ged_aet},
    {"ae2dir", cmd_ged_plain_wrapper, ged_ae2dir},
    {"aip", f_aip, GED_FUNC_PTR_NULL},
    {"alter", cmd_ged_plain_wrapper, ged_alter},
    {"analyze", cmd_ged_info_wrapper, ged_analyze},
    {"annotate", cmd_ged_plain_wrapper, ged_annotate},
    {"arb", cmd_ged_plain_wrapper, ged_arb},
    {"arced", cmd_ged_plain_wrapper, ged_arced},
    {"area", f_area, GED_FUNC_PTR_NULL},
    {"arot", cmd_arot, GED_FUNC_PTR_NULL},
    {"attach", f_attach, GED_FUNC_PTR_NULL},
    {"attr", cmd_ged_plain_wrapper, ged_attr},
    {"autoview", cmd_autoview, GED_FUNC_PTR_NULL},
    {"bb", cmd_ged_plain_wrapper, ged_bb},
    {"bev", cmd_ged_plain_wrapper, ged_bev},
    {"bo", cmd_ged_plain_wrapper, ged_bo},
#if 0
    {"import_body", cmd_import_body, GED_FUNC_PTR_NULL},
    {"export_body", cmd_export_body, GED_FUNC_PTR_NULL},
#endif
    {"bomb", f_bomb, GED_FUNC_PTR_NULL},
    {"bot", cmd_ged_plain_wrapper, ged_bot},
    {"bot_condense", cmd_ged_plain_wrapper, ged_bot_condense},
    {"bot_decimate", cmd_ged_plain_wrapper, ged_bot_decimate},
    {"bot_face_fuse", cmd_ged_plain_wrapper, ged_bot_face_fuse},
    {"bot_face_sort", cmd_ged_plain_wrapper, ged_bot_face_sort},
    {"bot_flip", cmd_ged_plain_wrapper, ged_bot_flip},
    {"bot_merge", cmd_ged_plain_wrapper, ged_bot_merge},
    {"bot_smooth", cmd_ged_plain_wrapper, ged_bot_smooth},
    {"bot_split", cmd_ged_plain_wrapper, ged_bot_split},
    {"bot_sync", cmd_ged_plain_wrapper, ged_bot_sync},
    {"bot_vertex_fuse", cmd_ged_plain_wrapper, ged_bot_vertex_fuse},
    {"bottom", f_bv_bottom, GED_FUNC_PTR_NULL},
    {"brep", cmd_ged_view_wrapper, ged_brep},
    {"c", cmd_ged_plain_wrapper, ged_comb_std},
    {"cat", cmd_ged_info_wrapper, ged_cat},
    {"cc", cmd_ged_plain_wrapper, ged_cc},
    {"center", cmd_center, GED_FUNC_PTR_NULL},
    {"clone", cmd_ged_edit_wrapper, ged_clone},
    {"closedb", f_closedb, GED_FUNC_PTR_NULL},
    {"cmd_win", cmd_cmd_win, GED_FUNC_PTR_NULL},
    {"color", cmd_ged_plain_wrapper, ged_color},
    {"comb", cmd_ged_plain_wrapper, ged_comb},
    {"comb_color", cmd_ged_plain_wrapper, ged_comb_color},
    {"copyeval", cmd_ged_plain_wrapper, ged_copyeval},
    {"copymat", cmd_ged_plain_wrapper, ged_copymat},
    {"cp", cmd_ged_plain_wrapper, ged_copy},
    {"cpi", f_copy_inv, GED_FUNC_PTR_NULL},
    {"d", cmd_ged_erase_wrapper, ged_erase},
    {"db", cmd_stub, GED_FUNC_PTR_NULL},
    {"db_glob", cmd_ged_plain_wrapper, ged_glob},
    {"dbconcat", cmd_ged_plain_wrapper, ged_concat},
    {"dbfind", cmd_ged_info_wrapper, ged_find},
    {"dbip", cmd_ged_plain_wrapper, ged_dbip},
    {"dbversion", cmd_ged_plain_wrapper, ged_version},
    {"debugbu", cmd_ged_plain_wrapper, ged_debugbu},
    {"debugdir", cmd_ged_plain_wrapper, ged_debugdir},
    {"debuglib", cmd_ged_plain_wrapper, ged_debuglib},
    {"debugmem", cmd_ged_plain_wrapper, ged_debugmem},
    {"debugnmg", cmd_ged_plain_wrapper, ged_debugnmg},
    {"decompose", cmd_ged_plain_wrapper, ged_decompose},
    {"delay", cmd_ged_plain_wrapper, ged_delay},
    {"dir2ae", cmd_ged_plain_wrapper, ged_dir2ae},
    {"dump", cmd_ged_plain_wrapper, ged_dump},
    {"dm", f_dm, GED_FUNC_PTR_NULL},
    {"draw", cmd_draw, GED_FUNC_PTR_NULL},
    {"dup", cmd_ged_plain_wrapper, ged_dup},
    {"E", cmd_E, GED_FUNC_PTR_NULL},
    {"e", cmd_draw, GED_FUNC_PTR_NULL},
    {"eac", cmd_ged_view_wrapper, ged_eac},
    {"echo", cmd_ged_plain_wrapper, ged_echo},
    {"edcodes", f_edcodes, GED_FUNC_PTR_NULL},
    {"color", cmd_ged_plain_wrapper, ged_color},
    {"edcolor", f_edcolor, GED_FUNC_PTR_NULL},
    {"edcomb", cmd_ged_plain_wrapper, ged_edcomb},
    {"edgedir", f_edgedir, GED_FUNC_PTR_NULL},
    {"edmater", f_edmater, GED_FUNC_PTR_NULL},
    {"em", cmd_emuves, GED_FUNC_PTR_NULL},
    {"erase", cmd_ged_erase_wrapper, ged_erase},
    {"ev", cmd_ev, GED_FUNC_PTR_NULL},
    {"e_muves", f_e_muves, GED_FUNC_PTR_NULL},
    {"eqn", f_eqn, GED_FUNC_PTR_NULL},
    {"exit", f_quit, GED_FUNC_PTR_NULL},
    {"expand", cmd_ged_plain_wrapper, ged_expand},
    {"extrude", f_extrude, GED_FUNC_PTR_NULL},
    {"eye_pt", cmd_ged_view_wrapper, ged_eye},
    {"facedef", f_facedef, GED_FUNC_PTR_NULL},
    {"facetize", cmd_ged_plain_wrapper, ged_facetize},
    {"form", cmd_ged_plain_wrapper, ged_form},
    {"fracture", cmd_ged_plain_wrapper, ged_fracture},
    {"front", f_bv_front, GED_FUNC_PTR_NULL},
    {"g", cmd_ged_plain_wrapper, ged_group},
    {"get", cmd_ged_plain_wrapper, ged_get},
    {"get_autoview", cmd_ged_plain_wrapper, ged_get_autoview},
    {"get_comb", cmd_ged_plain_wrapper, ged_get_comb},
    {"get_dbip", cmd_ged_plain_wrapper, ged_dbip},
    {"get_dm_list", f_get_dm_list, GED_FUNC_PTR_NULL},
    {"get_more_default", cmd_get_more_default, GED_FUNC_PTR_NULL},
    {"get_sed", f_get_sedit, GED_FUNC_PTR_NULL},
    {"get_sed_menus", f_get_sedit_menus, GED_FUNC_PTR_NULL},
    {"get_solid_keypoint", f_get_solid_keypoint, GED_FUNC_PTR_NULL},
    {"gqa", cmd_ged_gqa, ged_gqa},
    {"grid2model_lu", cmd_ged_plain_wrapper, ged_grid2model_lu},
    {"grid2view_lu", cmd_ged_plain_wrapper, ged_grid2view_lu},
#ifdef HIDELINE
    {"H", f_hideline, GED_FUNC_PTR_NULL},
#endif
    {"has_embedded_fb", cmd_has_embedded_fb, GED_FUNC_PTR_NULL},
    {"hide", cmd_ged_plain_wrapper, ged_hide},
    {"hist", cmd_hist, GED_FUNC_PTR_NULL},
    {"history", f_history, GED_FUNC_PTR_NULL},
    {"i", cmd_ged_plain_wrapper, ged_instance},
    {"idents", cmd_ged_plain_wrapper, ged_tables},
    {"ill", f_ill, GED_FUNC_PTR_NULL},
    {"in", cmd_ged_in, ged_in},
    {"inside", cmd_ged_inside, ged_inside},
    {"item", cmd_ged_plain_wrapper, ged_item},
    {"joint", f_joint, GED_FUNC_PTR_NULL},
    {"journal", f_journal, GED_FUNC_PTR_NULL},
    {"keep", cmd_ged_plain_wrapper, ged_keep},
    {"keypoint", f_keypoint, GED_FUNC_PTR_NULL},
    {"kill", cmd_ged_erase_wrapper, ged_kill},
    {"killall", cmd_ged_erase_wrapper, ged_killall},
    {"killrefs", cmd_ged_erase_wrapper, ged_killrefs},
    {"killtree", cmd_ged_erase_wrapper, ged_killtree},
    {"knob", f_knob, GED_FUNC_PTR_NULL},
    {"l", cmd_ged_info_wrapper, ged_list},
    {"l_muves", f_l_muves, GED_FUNC_PTR_NULL},
    {"labelvert", f_labelvert, GED_FUNC_PTR_NULL},
    {"left", f_bv_left, GED_FUNC_PTR_NULL},
    {"lm", cmd_lm, GED_FUNC_PTR_NULL},
    {"lt", cmd_ged_plain_wrapper, ged_lt},
    {"loadtk", cmd_tk, GED_FUNC_PTR_NULL},
    {"listeval", cmd_ged_plain_wrapper, ged_pathsum},
    {"loadview", cmd_ged_view_wrapper, ged_loadview},
    {"lookat", cmd_ged_view_wrapper, ged_lookat},
    {"ls", cmd_ged_plain_wrapper, ged_ls},
    {"M", f_mouse, GED_FUNC_PTR_NULL},
    {"m2v_point", cmd_ged_plain_wrapper, ged_m2v_point},
    {"make", f_make, GED_FUNC_PTR_NULL},
    {"make_bb", cmd_ged_plain_wrapper, ged_make_bb},
    {"make_name", cmd_ged_plain_wrapper, ged_make_name},
    {"make_pnts", cmd_ged_more_wrapper, ged_make_pnts},
    {"match", cmd_ged_plain_wrapper, ged_match},
    {"mater", cmd_ged_plain_wrapper, ged_mater},
    {"matpick", f_matpick, GED_FUNC_PTR_NULL},
    {"memprint", f_memprint, GED_FUNC_PTR_NULL},
    {"mged_update", f_update, GED_FUNC_PTR_NULL},
    {"mged_wait", f_wait, GED_FUNC_PTR_NULL},
    {"mirface", f_mirface, GED_FUNC_PTR_NULL},
    {"mirror", cmd_ged_plain_wrapper, ged_mirror},
    {"mmenu_get", cmd_mmenu_get, GED_FUNC_PTR_NULL},
    {"mmenu_set", cmd_nop, GED_FUNC_PTR_NULL},
    {"model2grid_lu", cmd_ged_plain_wrapper, ged_model2grid_lu},
    {"model2view", cmd_ged_plain_wrapper, ged_model2view},
    {"model2view_lu", cmd_ged_plain_wrapper, ged_model2view_lu},
    {"mrot", cmd_mrot, GED_FUNC_PTR_NULL},
    {"mv", cmd_ged_plain_wrapper, ged_move},
    {"mvall", cmd_ged_plain_wrapper, ged_move_all},
    {"nirt", f_nirt, GED_FUNC_PTR_NULL},
    {"nmg_collapse", cmd_nmg_collapse, GED_FUNC_PTR_NULL},
    {"nmg_fix_normals", cmd_ged_plain_wrapper, ged_nmg_fix_normals},
    {"nmg_simplify", cmd_ged_plain_wrapper, ged_nmg_simplify},
    {"o_rotate", f_be_o_rotate, GED_FUNC_PTR_NULL},
    {"o_scale", f_be_o_scale, GED_FUNC_PTR_NULL},
    {"oed", cmd_oed, GED_FUNC_PTR_NULL},
    {"oed_apply", f_oedit_apply, GED_FUNC_PTR_NULL},
    {"oed_reset", f_oedit_reset, GED_FUNC_PTR_NULL},
    {"oill", f_be_o_illuminate, GED_FUNC_PTR_NULL},
    {"opendb", f_opendb, GED_FUNC_PTR_NULL},
    {"orientation", cmd_ged_view_wrapper, ged_orient},
    {"orot", f_rot_obj, GED_FUNC_PTR_NULL},
    {"oscale", f_sc_obj, GED_FUNC_PTR_NULL},
    {"output_hook", cmd_output_hook, GED_FUNC_PTR_NULL},
    {"overlay", cmd_overlay, GED_FUNC_PTR_NULL},
    {"ox", f_be_o_x, GED_FUNC_PTR_NULL},
    {"oxscale", f_be_o_xscale, GED_FUNC_PTR_NULL},
    {"oxy", f_be_o_xy, GED_FUNC_PTR_NULL},
    {"oy", f_be_o_y, GED_FUNC_PTR_NULL},
    {"oyscale", f_be_o_yscale, GED_FUNC_PTR_NULL},
    {"ozscale", f_be_o_zscale, GED_FUNC_PTR_NULL},
    {"p", f_param, GED_FUNC_PTR_NULL},
    {"parse_points", cmd_parse_points, GED_FUNC_PTR_NULL},
    {"pathlist", cmd_ged_plain_wrapper, ged_pathlist},
    {"paths", cmd_ged_plain_wrapper, ged_pathsum},
    {"permute", f_permute, GED_FUNC_PTR_NULL},
    {"pl", f_pl, GED_FUNC_PTR_NULL},
    {"plot", cmd_ged_plain_wrapper, ged_plot},
    {"png", cmd_ged_plain_wrapper, ged_png},
    {"polybinout", f_polybinout, GED_FUNC_PTR_NULL},
    {"pov", cmd_pov, GED_FUNC_PTR_NULL},
    {"prcolor", cmd_ged_plain_wrapper, ged_prcolor},
    {"prefix", cmd_ged_plain_wrapper, ged_prefix},
    {"press", f_press, GED_FUNC_PTR_NULL},
    {"preview", cmd_ged_dm_wrapper, ged_preview},
    {"ps", f_ps, GED_FUNC_PTR_NULL},
    {"push", cmd_ged_plain_wrapper, ged_push},
    {"put", cmd_ged_plain_wrapper, ged_put},
    {"put_comb", cmd_ged_plain_wrapper, ged_put_comb},
    {"put_sed", f_put_sedit, GED_FUNC_PTR_NULL},
    {"putmat", cmd_ged_plain_wrapper, ged_putmat},
    {"q", f_quit, GED_FUNC_PTR_NULL},
    {"qorot", f_qorot, GED_FUNC_PTR_NULL},
    {"qray", cmd_ged_plain_wrapper, ged_qray},
    {"query_ray", f_nirt, GED_FUNC_PTR_NULL},
    {"quit", f_quit, GED_FUNC_PTR_NULL},
    {"qvrot", cmd_ged_view_wrapper, ged_qvrot},
    {"r", cmd_ged_plain_wrapper, ged_region},
    {"rate", f_bv_rate_toggle, GED_FUNC_PTR_NULL},
    {"rcodes", cmd_ged_plain_wrapper, ged_rcodes},
    {"read_muves", f_read_muves, GED_FUNC_PTR_NULL},
    {"rear", f_bv_rear, GED_FUNC_PTR_NULL},
    {"red", f_red, GED_FUNC_PTR_NULL},
    {"redraw_vlist", cmd_redraw_vlist, GED_FUNC_PTR_NULL},
    {"refresh", f_refresh, GED_FUNC_PTR_NULL},
    {"regdebug", f_regdebug, GED_FUNC_PTR_NULL},
    {"regdef", cmd_ged_plain_wrapper, ged_regdef},
    {"regions", cmd_ged_plain_wrapper, ged_tables},
    {"reject", f_be_reject, GED_FUNC_PTR_NULL},
    {"release", f_release, GED_FUNC_PTR_NULL},
    {"reset", f_bv_reset, GED_FUNC_PTR_NULL},
    {"restore", f_bv_vrestore, GED_FUNC_PTR_NULL},
    {"rfarb", f_rfarb, GED_FUNC_PTR_NULL},
    {"right", f_bv_right, GED_FUNC_PTR_NULL},
    {"rm", cmd_ged_plain_wrapper, ged_remove},
    {"rmater", cmd_ged_plain_wrapper, ged_rmater},
    {"rmats", f_rmats, GED_FUNC_PTR_NULL},
    {"rot", cmd_rot, GED_FUNC_PTR_NULL},
    {"rotobj", f_rot_obj, GED_FUNC_PTR_NULL},
    {"rrt", cmd_rrt, GED_FUNC_PTR_NULL},
    {"rset", f_rset, GED_FUNC_PTR_NULL},
    {"rt", cmd_rt, GED_FUNC_PTR_NULL},
    {"rt_gettrees", cmd_rt_gettrees, GED_FUNC_PTR_NULL},
    {"rtabort", cmd_ged_plain_wrapper, ged_rtabort},
    {"rtarea", cmd_rt, GED_FUNC_PTR_NULL},
    {"rtcheck", cmd_rt, GED_FUNC_PTR_NULL},
    {"rtedge", cmd_rt, GED_FUNC_PTR_NULL},
    {"rtweight", cmd_rt, GED_FUNC_PTR_NULL},
    {"save", f_bv_vsave, GED_FUNC_PTR_NULL},
    {"savekey", cmd_ged_plain_wrapper, ged_savekey},
    {"saveview", cmd_ged_plain_wrapper, ged_saveview},
    {"sca", cmd_sca, GED_FUNC_PTR_NULL},
    {"screengrab", cmd_ged_dm_wrapper, ged_screen_grab},
    {"search", cmd_search, GED_FUNC_PTR_NULL},
    {"sed", f_sed, GED_FUNC_PTR_NULL},
    {"sed_apply", f_sedit_apply, GED_FUNC_PTR_NULL},
    {"sed_reset", f_sedit_reset, GED_FUNC_PTR_NULL},
    {"sedit", f_be_s_edit, GED_FUNC_PTR_NULL},
    {"set_more_default", cmd_set_more_default, GED_FUNC_PTR_NULL},
    {"setview", cmd_setview, GED_FUNC_PTR_NULL},
    {"shaded_mode", cmd_shaded_mode, GED_FUNC_PTR_NULL},
    {"shader", cmd_ged_plain_wrapper, ged_shader},
    {"share", f_share, GED_FUNC_PTR_NULL},
    {"shells", cmd_ged_plain_wrapper, ged_shells},
    {"showmats", cmd_ged_plain_wrapper, ged_showmats},
    {"sill", f_be_s_illuminate, GED_FUNC_PTR_NULL},
    {"size", cmd_size, GED_FUNC_PTR_NULL},
    {"solid_report", cmd_ged_plain_wrapper, ged_report},
    {"solids", cmd_ged_plain_wrapper, ged_tables},
    {"solids_on_ray", cmd_ged_plain_wrapper, ged_solids_on_ray},
    {"srot", f_be_s_rotate, GED_FUNC_PTR_NULL},
    {"sscale", f_be_s_scale, GED_FUNC_PTR_NULL},
    {"status", f_status, GED_FUNC_PTR_NULL},
    {"stuff_str", cmd_stuff_str, GED_FUNC_PTR_NULL},
    {"summary", cmd_ged_plain_wrapper, ged_summary},
    {"sv", f_slewview, GED_FUNC_PTR_NULL},
    {"svb", f_svbase, GED_FUNC_PTR_NULL},
    {"sxy", f_be_s_trans, GED_FUNC_PTR_NULL},
    {"sync", cmd_ged_plain_wrapper, ged_sync},
    {"t", cmd_ged_plain_wrapper, ged_ls},
    {"t_muves", f_t_muves, GED_FUNC_PTR_NULL},
    {"ted", f_tedit, GED_FUNC_PTR_NULL},
    {"tie", f_tie, GED_FUNC_PTR_NULL},
    {"tire", cmd_ged_plain_wrapper, ged_tire},
    {"title", cmd_ged_plain_wrapper, ged_title},
    {"tol", cmd_tol, GED_FUNC_PTR_NULL},
    {"top", f_bv_top, GED_FUNC_PTR_NULL},
    {"tops", cmd_ged_plain_wrapper, ged_tops},
    {"tra", cmd_tra, GED_FUNC_PTR_NULL},
    {"track", f_amtrack, GED_FUNC_PTR_NULL},
    {"tracker", f_tracker, GED_FUNC_PTR_NULL},
    {"translate", f_tr_obj, GED_FUNC_PTR_NULL},
    {"tree", cmd_ged_plain_wrapper, ged_tree},
    {"unhide", cmd_ged_plain_wrapper, ged_unhide},
    {"units", cmd_units, GED_FUNC_PTR_NULL},
    {"v2m_point", cmd_ged_plain_wrapper, ged_v2m_point},
    {"vars", f_set, GED_FUNC_PTR_NULL},
    {"vdraw", cmd_ged_plain_wrapper, ged_vdraw},
    {"view", cmd_ged_view_wrapper, ged_view},
    {"view_ring", f_view_ring, GED_FUNC_PTR_NULL},
#if 0
    {"viewget", cmd_viewget, GED_FUNC_PTR_NULL},
    {"viewset", cmd_viewset, GED_FUNC_PTR_NULL},
#endif
    {"view2grid_lu", cmd_ged_plain_wrapper, ged_view2grid_lu},
    {"view2model", cmd_ged_plain_wrapper, ged_view2model},
    {"view2model_lu", cmd_ged_plain_wrapper, ged_view2model_lu},
    {"view2model_vec", cmd_ged_plain_wrapper, ged_view2model_vec},
    {"viewdir", cmd_ged_plain_wrapper, ged_viewdir},
    {"viewsize", cmd_size, GED_FUNC_PTR_NULL}, /* alias "size" for saveview scripts */
    {"vnirt", f_vnirt, GED_FUNC_PTR_NULL},
    {"vquery_ray", f_vnirt, GED_FUNC_PTR_NULL},
#if 0
    {"vrmgr", f_vrmgr, GED_FUNC_PTR_NULL},
#endif
    {"vrot", cmd_vrot, GED_FUNC_PTR_NULL},
#if 0
    {"vrot_center", f_vrot_center, GED_FUNC_PTR_NULL},
#endif
    {"wcodes", cmd_ged_plain_wrapper, ged_wcodes},
    {"whatid", cmd_ged_plain_wrapper, ged_whatid},
    {"which_shader", cmd_ged_plain_wrapper, ged_which_shader},
    {"whichair", cmd_ged_plain_wrapper, ged_which},
    {"whichid", cmd_ged_plain_wrapper, ged_which},
    {"who", cmd_ged_plain_wrapper, ged_who},
    {"winset", f_winset, GED_FUNC_PTR_NULL},
    {"wmater", cmd_ged_plain_wrapper, ged_wmater},
    {"x", cmd_ged_plain_wrapper, ged_report},
    {"xpush", cmd_ged_plain_wrapper, ged_xpush},
    {"Z", cmd_zap, GED_FUNC_PTR_NULL},
    {"zoom", cmd_zoom, GED_FUNC_PTR_NULL},
    {"zoomin", f_bv_zoomin, GED_FUNC_PTR_NULL},
    {"zoomout", f_bv_zoomout, GED_FUNC_PTR_NULL},
    {NULL, NULL, GED_FUNC_PTR_NULL}
};


/**
 * C M D _ S E T U P
 *
 * Register all MGED commands.
 */
HIDDEN void
cmd_setup(void)
{
    struct cmdtab *ctp;
    struct bu_vls temp;
    const char *pathname;
    char buffer[1024];

    /* from cmd.c */
    extern int glob_compat_mode;
    extern int output_as_return;

    bu_vls_init(&temp);
    for (ctp = mged_cmdtab; ctp->name != NULL; ctp++) {
	bu_vls_strcpy(&temp, "_mged_");
	bu_vls_strcat(&temp, ctp->name);

	(void)Tcl_CreateCommand(INTERP, ctp->name, ctp->tcl_func,
				(ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(INTERP, bu_vls_addr(&temp), ctp->tcl_func,
				(ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }

    /* overrides/wraps the built-in tree command */

    /* Locate the BRL-CAD-specific Tcl scripts */
    pathname = bu_brlcad_data("tclscripts", 1);
    snprintf(buffer, sizeof(buffer), "%s", pathname);

    /* link some tcl variables to these corresponding globals */
    Tcl_LinkVar(INTERP, "glob_compat_mode", (char *)&glob_compat_mode, TCL_LINK_BOOLEAN);
    Tcl_LinkVar(INTERP, "output_as_return", (char *)&output_as_return, TCL_LINK_BOOLEAN);

    /* Provide Tcl interfaces to the fundamental BRL-CAD libraries */
    Bu_Init(INTERP);
    Bn_Init(INTERP);
    Rt_Init(INTERP);
    Go_Init(INTERP);

    tkwin = NULL;

    bu_vls_free(&temp);
}


static void
mged_output_handler(struct ged *UNUSED(gp), char *line)
{
    if (line)
	bu_log("%s", line);
}


static void
mged_refresh_handler(void *UNUSED(clientdata))
{
    view_state->vs_flag = 1;
    refresh();
}


/*
 * Initialize mged, configure the path, set up the tcl interpreter.
 */
void
mged_setup(Tcl_Interp **interpreter)
{
    int try_auto_path = 0;

    int init_tcl = 1;
    int init_itcl = 1;
    struct bu_vls str;
    const char *name = bu_argv0_full_path();

    /* locate our run-time binary (must be called before Tcl_CreateInterp()) */
    if (name) {
	Tcl_FindExecutable(name);
    } else {
	Tcl_FindExecutable("mged");
    }

    if (interpreter && *interpreter != NULL)
	Tcl_DeleteInterp(*interpreter);

    /* Create the interpreter */
    *interpreter = Tcl_CreateInterp();

    /* a two-pass init loop.  the first pass just tries default init
     * routines while the second calls tclcad_auto_path() to help it
     * find other, potentially uninstalled, resources.
     */
    while (1) {

	/* not called first time through, give Tcl_Init() a chance */
	if (try_auto_path) {
	    /* Locate the BRL-CAD-specific Tcl scripts, set the auto_path */
	    tclcad_auto_path(*interpreter);
	}

	/* Initialize Tcl */
	Tcl_ResetResult(*interpreter);
	if (init_tcl && Tcl_Init(*interpreter) == TCL_ERROR) {
	    if (!try_auto_path) {
		try_auto_path = 1;
		continue;
	    }
	    bu_log("Tcl_Init ERROR:\n%s\n", Tcl_GetStringResult(*interpreter));
	    break;
	}
	init_tcl = 0;

	/* Initialize [incr Tcl] */
	Tcl_ResetResult(*interpreter);
	/* NOTE: Calling "package require Itcl" here is apparently
	 * insufficient without other changes elsewhere.  The
	 * Combination Editor in mged fails with an iwidgets class
	 * already loaded error if we don't perform Itcl_Init() here.
	 */
	if (init_itcl && Itcl_Init(*interpreter) == TCL_ERROR) {
	    if (!try_auto_path) {
		Tcl_Namespace *nsp;

		try_auto_path = 1;
		/* Itcl_Init() leaves initialization in a bad state
		 * and can cause retry failures.  cleanup manually.
		 */
		Tcl_DeleteCommand(*interpreter, "::itcl::class");
		nsp = Tcl_FindNamespace(*interpreter, "::itcl", NULL, 0);
		if(nsp)
		    Tcl_DeleteNamespace(nsp);
		continue;
	    }
	    bu_log("Itcl_Init ERROR:\n%s\n", Tcl_GetStringResult(*interpreter));
	    break;
	}
	init_itcl = 0;

	/* don't actually want to loop forever */
	break;

    } /* end iteration over Init() routines that need auto_path */
    Tcl_ResetResult(*interpreter);

    /* if we haven't loaded by now, load auto_path so we find our tclscripts */
    if (!try_auto_path) {
	/* Locate the BRL-CAD-specific Tcl scripts */
	tclcad_auto_path(*interpreter);
    }

    /*XXX FIXME: Should not be importing Itcl into the global namespace */
    /* Import [incr Tcl] commands into the global namespace. */
    if (Tcl_Import(*interpreter, Tcl_GetGlobalNamespace(*interpreter), "::itcl::*", /* allowOverwrite */ 1) != TCL_OK) {
	bu_log("Tcl_Import ERROR: %s\n", Tcl_GetStringResult(*interpreter));
	Tcl_ResetResult(*interpreter);
    }

    /* Initialize libbu */
    if (Bu_Init(*interpreter) == TCL_ERROR) {
	bu_log("Bu_Init ERROR:\n%s\n", Tcl_GetStringResult(*interpreter));
	Tcl_ResetResult(*interpreter);
    }

    /* Initialize libbn */
    if (Bn_Init(*interpreter) == TCL_ERROR) {
	bu_log("Bn_Init ERROR:\n%s\n", Tcl_GetStringResult(*interpreter));
	Tcl_ResetResult(*interpreter);
    }

    /* Initialize librt */
    if (Rt_Init(*interpreter) == TCL_ERROR) {
	bu_log("Rt_Init ERROR:\n%s\n", Tcl_GetStringResult(*interpreter));
	Tcl_ResetResult(*interpreter);
    }

    /* Initialize libged */
    if (Go_Init(*interpreter) == TCL_ERROR) {
	bu_log("Ged_Init ERROR:\n%s\n", Tcl_GetStringResult(*interpreter));
	Tcl_ResetResult(*interpreter);
    }

    BU_GETSTRUCT(view_state->vs_gvp, ged_view);
    ged_view_init(view_state->vs_gvp);

    view_state->vs_gvp->gv_callback = mged_view_callback;
    view_state->vs_gvp->gv_clientData = (genptr_t)view_state;
    MAT_DELTAS_GET_NEG(view_state->vs_orig_pos, view_state->vs_gvp->gv_center);

    if (gedp) {
	/* release any allocated memory */
	ged_free(gedp);
    } else {
	BU_GETSTRUCT(gedp, ged);
    }
    GED_INIT(gedp, NULL);

    gedp->ged_output_handler = mged_output_handler;
    gedp->ged_refresh_handler = mged_refresh_handler;

    /* register commands */
    cmd_setup();

    history_setup();
    mged_global_variable_setup(*interpreter);
    mged_variable_setup(*interpreter);

    /* Tcl needs to write nulls onto subscripted variable names */
    bu_vls_init(&str);
    bu_vls_printf(&str, "%s(state)", MGED_DISPLAY_VAR);
    Tcl_SetVar(*interpreter, bu_vls_addr(&str), state_str[STATE], TCL_GLOBAL_ONLY);

    /* Set defaults for view status variables */
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "set mged_display(.topid_0.ur,ang) {ang=(0.00 0.00 0.00)};\
set mged_display(.topid_0.ur,aet) {az=35.00  el=25.00  tw=0.00};\
set mged_display(.topid_0.ur,size) sz=1000.000;\
set mged_display(.topid_0.ur,center) {cent=(0.000 0.000 0.000)};\
set mged_display(units) mm");
    Tcl_Eval(*interpreter, bu_vls_addr(&str));

    Tcl_ResetResult(*interpreter);

    bu_vls_free(&str);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
