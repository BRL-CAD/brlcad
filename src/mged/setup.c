/*                         S E T U P . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2008 United States Government as represented by
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
/** @file setup.c
 *
 *  routines to initialize mged
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

extern void init_qray(void);


struct gedtab {
    char *name;
    int (*func)(int, const char *[]);
};
static struct gedtab newtab[] = {
    {"edmater", ged_edmater},
    {NULL, NULL}
};

struct cmdtab {
    char *ct_name;
    int (*ct_func)();
};
static struct cmdtab cmdtab[] = {
    {"%", f_comm},
    {"35, 25", bv_35_25},
    {"3ptarb", f_3ptarb},
    {"45, 45", bv_45_45},
    {"B", cmd_blast},
    {"accept", be_accept},
    {"adc", f_adc},
    {"adjust", cmd_adjust},
    {"ae", cmd_aetview},
    {"ae2dir", cmd_ae2dir},
    {"aip", f_aip},
    {"analyze", f_analyze},
    {"arb", f_arbdef},
    {"arced", f_arced},
    {"area", f_area},
    {"arot", cmd_arot},
    {"attach", f_attach},
    {"attr", cmd_attr},
    {"autoview", cmd_autoview},
    {"bev", f_bev},
    {"bo", f_bo},
#if 0
    {"import_body", cmd_import_body},
    {"export_body", cmd_export_body},
#endif
    {"bomb", f_bomb},
    {"bot_condense", f_bot_condense},
    {"bot_decimate", cmd_bot_decimate},
    {"bot_face_fuse", f_bot_face_fuse},
    {"bot_face_sort", cmd_bot_face_sort},
    {"bot_merge", f_bot_merge},
    {"bot_smooth", cmd_bot_smooth },
    {"bot_split", f_bot_split},
    {"bot_vertex_fuse", f_bot_fuse},
    {"bottom",	bv_bottom},
    {"c", cmd_comb_std},
    {"cat", cmd_cat},
    {"center", cmd_center},
    {"clone", f_clone},
    {"closedb", f_closedb},
    {"cmd_win", cmd_cmd_win},
    {"color", cmd_color},
    {"comb", cmd_comb},
    {"comb_color", f_comb_color},
    {"copyeval", cmd_copyeval},
    {"copymat", f_copymat},
    {"cp", cmd_copy},
    {"cpi", f_copy_inv},
    {"d", cmd_erase},
    {"dall", cmd_erase_all},
    {"db_glob", cmd_mged_glob},
    {"dbconcat", cmd_concat},
    {"dbfind", cmd_find},
    {"dbip",	cmd_dbip},
    {"dbversion", cmd_dbversion},
    {"debugbu", f_debugbu},
    {"debugdir", f_debugdir},
    {"debuglib", f_debuglib},
    {"debugmem", f_debugmem},
    {"debugnmg", f_debugnmg},
    {"decompose", f_decompose},
    {"delay", f_delay},
    {"dir2ae", cmd_dir2ae},
    {"dump",	cmd_dump},
    {"dm", f_dm},
    {"draw", cmd_draw},
    {"dup", cmd_dup},
    {"E", cmd_E},
    {"e", cmd_draw},
    {"eac", f_eac},
    {"echo", cmd_echo},
    {"edcodes", f_edcodes},
    {"edcolor", f_edcolor},
    {"edcomb", f_edcomb},
    {"edgedir", f_edgedir},
    {"edmater", f_edmater},
    {"em", cmd_emuves},
    {"erase", cmd_erase},
    {"erase_all", cmd_erase_all},
    {"ev", cmd_ev},
    {"e_muves", f_e_muves},
    {"eqn", f_eqn},
    {"exit", f_quit},
    {"expand", cmd_expand},
    {"extrude", f_extrude},
    {"eye_pt", cmd_eye_pt},
    {"facedef", f_facedef},
    {"facetize", f_facetize},
    {"form",	cmd_form},
    {"fracture", f_fracture},
    {"front",	bv_front},
    {"g", cmd_group},
    {"get",		cmd_get},
    {"get_autoview", cmd_get_autoview},
    {"get_comb", cmd_get_comb},
    {"get_dbip", cmd_get_ptr},
    {"get_dm_list", f_get_dm_list},
    {"get_more_default", cmd_get_more_default},
    {"get_sed", f_get_sedit},
    {"get_sed_menus", f_get_sedit_menus},
    {"get_solid_keypoint", f_get_solid_keypoint},
    {"grid2model_lu", f_grid2model_lu},
    {"grid2view_lu", f_grid2view_lu},
#ifdef HIDELINE
    {"H", f_hideline},
#endif
    {"has_embedded_fb", cmd_has_embedded_fb},
    {"hide", cmd_hide },
    {"hist", cmd_hist},
    {"history", f_history},
    {"i", cmd_instance},
    {"idents", f_tables},
    {"ill", f_ill},
    {"in", f_in},
    {"inside", f_inside},
    {"item", f_itemair},
    {"joint", f_joint},
    {"journal", f_journal},
    {"keep", cmd_keep},
    {"keypoint", f_keypoint},
    {"kill", cmd_kill},
    {"killall", cmd_killall},
    {"killtree", cmd_killtree},
    {"knob", f_knob},
    {"l", cmd_list},
    {"l_muves", f_l_muves},
    {"labelvert", f_labelvert},
    {"left",		bv_left},
    {"lm", cmd_lm},
    {"lt", cmd_lt},
    {"loadtk", cmd_tk},
    {"listeval", cmd_pathsum},
    {"loadview", f_loadview},
    {"lookat", cmd_lookat},
    {"ls", cmd_ls},
    {"M", f_mouse},
    {"make", f_make},
    {"make_bb", cmd_make_bb},
    {"make_name", cmd_make_name},
    {"match",	cmd_match},
    {"mater", f_mater},
    {"matpick", f_matpick},
    {"memprint", f_memprint},
    {"mged_update", f_update},
    {"mged_wait", f_wait},
    {"mirface", f_mirface},
    {"mirror", f_mirror},
    {"mmenu_get", cmd_mmenu_get},
    {"mmenu_set", cmd_nop},
    {"model2grid_lu", f_model2grid_lu},
    {"model2view", f_model2view},
    {"model2view_lu", f_model2view_lu},
    {"mrot", cmd_mrot},
    {"mv", cmd_name},
    {"mvall", cmd_mvall},
    {"nirt", f_nirt},
    {"nmg_collapse", cmd_nmg_collapse},
    {"nmg_simplify", cmd_nmg_simplify},
    {"o_rotate",		be_o_rotate},
    {"o_scale",	be_o_scale},
    {"oed", cmd_oed},
    {"oed_apply", f_oedit_apply},
    {"oed_reset", f_oedit_reset},
    {"oill",		be_o_illuminate},
    {"opendb", f_opendb},
    {"orientation", cmd_orientation},
    {"orot", f_rot_obj},
    {"oscale", f_sc_obj},
    {"output_hook", cmd_output_hook},
    {"overlay", cmd_overlay},
    {"ox",		be_o_x},
    {"oxscale",	be_o_xscale},
    {"oxy",		be_o_xy},
    {"oy",		be_o_y},
    {"oyscale",	be_o_yscale},
    {"ozscale",	be_o_zscale},
    {"p", f_param},
    {"parse_points", cmd_parse_points},
    {"pathlist", cmd_pathlist},
    {"paths", cmd_pathsum},
    {"permute", f_permute},
    {"pl", f_pl},
    {"plot", f_plot},
    {"polybinout", f_polybinout},
    {"pov", cmd_pov},
    {"prcolor", cmd_prcolor},
    {"prefix", f_prefix},
    {"press", f_press},
    {"preview", f_preview},
    {"ps", f_ps},
    {"push", cmd_push},
    {"put",		cmd_put},
    {"put_comb", cmd_put_comb},
    {"put_sed", f_put_sedit},
    {"putmat", f_putmat},
    {"q", f_quit},
    {"qorot", f_qorot},
    {"qray", f_qray},
    {"query_ray", f_nirt},
    {"quit", f_quit},
    {"qvrot", f_qvrot},
    {"r", cmd_region},
    {"rate",		bv_rate_toggle},
    {"rcodes", f_rcodes},
    {"read_muves", f_read_muves},
    {"rear",	bv_rear},
    {"red", f_red},
    {"redraw_vlist", cmd_redraw_vlist},
    {"refresh", f_refresh},
    {"regdebug", f_regdebug},
    {"regdef", f_regdef},
    {"regions", f_tables},
    {"reject",	be_reject},
    {"release", f_release},
    {"reset",	bv_reset},
    {"restore",	bv_vrestore},
    {"rfarb", f_rfarb},
    {"right",	bv_right},
    {"rm", cmd_remove},
    {"rmater", f_rmater},
    {"rmats", f_rmats},
    {"rot", cmd_rot},
    {"rotobj", f_rot_obj},
    {"rrt", cmd_rrt},
    {"rset", f_rset},
    {"rt", cmd_rt},
    {"rt_gettrees", cmd_rt_gettrees},
    {"rtabort", cmd_rtabort},
    {"rtarea", cmd_rt},
    {"rtcheck", cmd_rt},
    {"rtedge", cmd_rt},
    {"rtweight", cmd_rt},
    {"save",		bv_vsave},
    {"solid_report", cmd_solid_report},
    {"savekey", f_savekey},
    {"saveview", f_saveview},
    {"sca", cmd_sca},
    {"sed", f_sed},
    {"sed_apply", f_sedit_apply},
    {"sed_reset", f_sedit_reset},
    {"sedit",	be_s_edit},
    {"set_more_default", cmd_set_more_default},
    {"setview", cmd_setview},
    {"shaded_mode", cmd_shaded_mode},
    {"shader", f_shader},
    {"share", f_share},
    {"shells", cmd_shells},
    {"showmats", cmd_showmats},
    {"sill",		be_s_illuminate},
    {"size", cmd_size},
    {"solid_report", cmd_solid_report},
    {"solids", f_tables},
    {"solids_on_ray", cmd_solids_on_ray},
    {"srot",		be_s_rotate},
    {"sscale",	be_s_scale},
    {"status", f_status},
    {"stuff_str", cmd_stuff_str},
    {"summary", cmd_summary},
    {"sv", f_slewview},
    {"svb", f_svbase},
    {"sxy",		be_s_trans},
    {"sync", f_sync},
    {"t", cmd_ls},
    {"t_muves", f_t_muves},
    {"ted", f_tedit},
    {"tie", f_tie},
    {"title", cmd_title},
    {"tol", cmd_tol},
    {"top",		bv_top},
    {"tops", cmd_tops},
    {"tra", cmd_tra},
    {"track", f_amtrack},
    {"tracker", f_tracker},
    {"translate", f_tr_obj},
    {"tree", cmd_tree},
    {"unhide", cmd_unhide},
    {"units", cmd_units},
    {"vars", f_set},
    {"vdraw", cmd_vdraw},
    {"view", f_view},
    {"view_ring", f_view_ring},
#if 0
    {"viewget", cmd_viewget},
    {"viewset", cmd_viewset},
#endif
    {"view2grid_lu", f_view2grid_lu},
    {"view2model", f_view2model},
    {"view2model_lu", f_view2model_lu},
    {"view2model_vec", f_view2model_vec},
    {"viewdir", cmd_viewdir},
    {"viewsize", cmd_size},		/* alias "size" for saveview scripts */
    {"vnirt", f_vnirt},
    {"vquery_ray", f_vnirt},
#if 0
    {"vrmgr", f_vrmgr},
#endif
    {"vrot", cmd_vrot},
#if 0
    {"vrot_center", f_vrot_center},
#endif
    {"wcodes", f_wcodes},
    {"whatid", cmd_whatid},
    {"which_shader", f_which_shader},
    {"whichair", cmd_which},
    {"whichid", cmd_which},
    {"who", cmd_who},
    {"winset", f_winset},
    {"wmater", f_wmater},
    {"x", cmd_solid_report},
    {"xpush", cmd_xpush},
    {"Z", cmd_zap},
    {"zoom", cmd_zoom},
    {"zoomin",	bv_zoomin},
    {"zoomout",	bv_zoomout},
    {NULL, NULL}
};


void
mged_rtCmdNotify()
{
    pr_prompt();
}

/**
 * C M D _ S E T U P
 *
 * Register all MGED commands.
 */
HIDDEN void
cmd_setup(void)
{
    register struct cmdtab *ctp;
    struct bu_vls temp;
    struct bu_vls	vls;
    const char *pathname;
    char buffer[1024];

    /* from cmd.c */
    extern int glob_compat_mode;
    extern int output_as_return;

    bu_vls_init(&temp);
    for (ctp = cmdtab; ctp->ct_name != NULL; ctp++) {
	bu_vls_strcpy(&temp, "_mged_");
	bu_vls_strcat(&temp, ctp->ct_name);

	(void)Tcl_CreateCommand(interp, ctp->ct_name, ctp->ct_func,
				(ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, bu_vls_addr(&temp), ctp->ct_func,
				(ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }

    /* overrides/wraps the built-in tree command */

    /* Locate the BRL-CAD-specific Tcl scripts */
    pathname = bu_brlcad_data("tclscripts", 1);
    snprintf(buffer, sizeof(buffer), "%s", pathname);

#ifdef _WIN32
    if (pathname) {
	/* XXXXXXXXXXXXXXX UGLY XXXXXXXXXXXXXXXXXX*/
	int i;

	bu_strlcat(buffer, "/", MAXPATHLEN);

	for (i=0;i<strlen(buffer);i++) {
	    if (buffer[i]=='\\')
		buffer[i]='/'; }

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "source %s/mged/tree.tcl", buffer);
	(void)Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
#endif

    /* link some tcl variables to these corresponding globals */
    Tcl_LinkVar(interp, "glob_compat_mode", (char *)&glob_compat_mode, TCL_LINK_BOOLEAN);
    Tcl_LinkVar(interp, "output_as_return", (char *)&output_as_return, TCL_LINK_BOOLEAN);

    /* Provide Tcl interfaces to the fundamental BRL-CAD libraries */
    Bu_Init(interp);
    Bn_Init(interp);
    Rt_Init(interp);
    Ged_Init(interp);

    tkwin = NULL;

    bu_vls_free(&temp);
}


/*
 * Initialize mged, configure the path, set up the tcl interpreter.
 */
void
mged_setup(void)
{
    int try_auto_path = 0;

    int init_tcl = 1;
    int init_itcl = 1;
    struct bu_vls str;
    const char *name = bu_getprogname();

    /* locate our run-time binary (must be called before Tcl_CreateInterp()) */
    if (name) {
	Tcl_FindExecutable(name);
    } else {
	Tcl_FindExecutable("mged");
    }

    /* Create the interpreter */
    interp = Tcl_CreateInterp();

    /* a two-pass init loop.  the first pass just tries default init
     * routines while the second calls tclcad_auto_path() to help it
     * find other, potentially uninstalled, resources.
     */
    while (1) {

	/* not called first time through, give Tcl_Init() a chance */
	if (try_auto_path) {
	    /* Locate the BRL-CAD-specific Tcl scripts, set the auto_path */
	    tclcad_auto_path(interp);
	}

	/* Initialize Tcl */
	Tcl_ResetResult(interp);
	if (init_tcl && Tcl_Init(interp) == TCL_ERROR) {
	    if (!try_auto_path) {
		try_auto_path=1;
		continue;
	    }
	    bu_log("Tcl_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    break;
	}
	init_tcl=0;

	/* warn if tcl_library isn't set by now */
	if (try_auto_path) {
	    tclcad_tcl_library(interp);
	}

	/* Initialize [incr Tcl] */
	Tcl_ResetResult(interp);
	if (init_itcl && Itcl_Init(interp) == TCL_ERROR) {
	    if (!try_auto_path) {
		try_auto_path=1;
		continue;
	    }
	    bu_log("Itcl_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    break;
	}
	Tcl_StaticPackage(interp, "Itcl", Itcl_Init, Itcl_SafeInit);
	init_itcl=0;

	/* don't actually want to loop forever */
	break;

    } /* end iteration over Init() routines that need auto_path */
    Tcl_ResetResult(interp);

    /* if we haven't loaded by now, load auto_path so we find our tclscripts */
    if (!try_auto_path) {
	/* Locate the BRL-CAD-specific Tcl scripts */
	tclcad_auto_path(interp);
    }

    /* Import [incr Tcl] commands into the global namespace. */
    if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp), "::itcl::*", /* allowOverwrite */ 1) != TCL_OK) {
	bu_log("Tcl_Import ERROR: %s\n", Tcl_GetStringResult(interp));
	Tcl_ResetResult(interp);
    }

    /* Initialize libbu */
    if (Bu_Init(interp) == TCL_ERROR) {
	bu_log("Bu_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	Tcl_ResetResult(interp);
    }

    /* Initialize libbn */
    if (Bn_Init(interp) == TCL_ERROR) {
	bu_log("Bn_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	Tcl_ResetResult(interp);
    }

    /* Initialize librt */
    if (Rt_Init(interp) == TCL_ERROR) {
	bu_log("Rt_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	Tcl_ResetResult(interp);
    }

    /* Initialize libged */
    if (Ged_Init(interp) == TCL_ERROR) {
	bu_log("Ged_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	Tcl_ResetResult(interp);
    }

    /* initialize MGED's drawable geometry object */
    dgop = dgo_open_cmd("mged", wdbp);
    dgop->dgo_rtCmdNotify = mged_rtCmdNotify;

    view_state->vs_vop = vo_open_cmd("");
    view_state->vs_vop->vo_callback = mged_view_obj_callback;
    view_state->vs_vop->vo_clientData = view_state;
    view_state->vs_vop->vo_scale = 500;
    view_state->vs_vop->vo_size = 2.0 * view_state->vs_vop->vo_scale;
    view_state->vs_vop->vo_invSize = 1.0 / view_state->vs_vop->vo_size;
    MAT_DELTAS_GET_NEG(view_state->vs_orig_pos, view_state->vs_vop->vo_center);

    /* register commands */
    cmd_setup();

    history_setup();
    mged_global_variable_setup(interp);
#if !TRY_NEW_MGED_VARS
    mged_variable_setup(interp);
#endif

    /* Tcl needs to write nulls onto subscripted variable names */
    bu_vls_init(&str);
    bu_vls_printf( &str, "%s(state)", MGED_DISPLAY_VAR );
    Tcl_SetVar(interp, bu_vls_addr(&str), state_str[state], TCL_GLOBAL_ONLY);

    /* initialize "Query Ray" variables */
    init_qray();

    Tcl_ResetResult(interp);

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
