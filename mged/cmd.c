/*
 *			C M D . C
 *
 * Functions -
 *	f_press		hook for displays with no buttons
 *	f_summary	do directory summary
 *	mged_cmd		Check arg counts, run a command
 *
 *  Authors -
 *	Michael John Muuss
 *	Charles M. Kennedy
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/time.h>
#include <time.h>

#ifdef DM_X
#include "tk.h"
#else
#include "tcl.h"
#endif

#include "tclInt.h"
#include "itcl.h"

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "./ged.h"
#include "./cmd.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./sedit.h"

#include "./mgedtcl.h"

int bv_zoomin(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_zoomout(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_rate_toggle(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_top(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_bottom(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_right(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_left(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_front(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_rear(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_vrestore(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_vsave(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_adcursor(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_reset(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_45_45(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int bv_35_25(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_o_illuminate(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_s_illuminate(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_o_scale(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_o_x(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_o_y(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_o_xy(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_o_rotate(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_accept(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_reject(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_s_edit(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_s_rotate(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_s_trans(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_s_scale(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_o_xscale(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_o_yscale(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int be_o_zscale(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);

void mged_setup(), cmd_setup(), mged_compat();
void mged_print_result();
void mged_global_variable_setup();
int f_bot_fuse(), f_bot_condense(), f_bot_face_fuse();
extern int f_hide(), f_unhide();

#ifndef HAVE_UNISTD_H
extern void sync();
#endif
extern void init_qray();			/* in qray.c */
extern int gui_setup();				/* in attach.c */
extern int mged_default_dlist;			/* in attach.c */
extern int classic_mged;			/* in ged.c */
extern int bot_vertex_fuse(), bot_condense();
struct cmd_list head_cmd_list;
struct cmd_list *curr_cmd_list;

int glob_compat_mode = 1;
int output_as_return = 1;

int mged_cmd();
struct bu_vls tcl_output_hook;

Tcl_Interp *interp = NULL;

#ifdef DM_X
Tk_Window tkwin;
#endif

struct cmdtab {
	char *ct_name;
	int (*ct_func)();
};

#if 1
int f_test_bomb_hook(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
#endif

static struct cmdtab cmdtab[] = {
#if 1
	{"test_bomb_hook", f_test_bomb_hook},
#endif
	{"%", f_comm},
	{"35,25",	bv_35_25},
	{"3ptarb", f_3ptarb},
	{"45,45",	bv_45_45},
	{"accept",	be_accept},
	{"adc", f_adc},
	{"ae", f_aetview},
	{"aip", f_aip},
	{"analyze", f_analyze},
	{"arb", f_arbdef},
	{"arced", f_arced},
	{"area", f_area},
	{"arot", f_arot},
#ifdef DM_X
	{"attach", f_attach},
#endif
	{"autoview", f_autoview},
	{"B", f_blast},
	{"bev", f_bev},
	{"import_body", cmd_import_body},
	{"export_body", cmd_export_body},
	{"bot_face_fuse", f_bot_face_fuse},
	{"bot_vertex_fuse", f_bot_fuse},
	{"bot_condense", f_bot_condense},
	{"bottom",	bv_bottom},
	{"c", f_comb_std},
	{"cat", f_cat},
	{"center", f_center},
	{"cmd_win", cmd_cmd_win},
	{"color", f_color},
	{"comb", f_comb},
	{"comb_color", f_comb_color},
	{"copyeval", f_copyeval},
	{"copymat", f_copymat},
	{"cp", f_copy},
	{"cpi", f_copy_inv},
	{"d", f_erase},
	{"dall", f_erase_all},
	{"db_glob", cmd_mged_glob},
	{"dbconcat", f_concat},
	{"dbfind", f_find},
	{"debugbu", f_debugbu},
	{"debugdir", f_debugdir},
	{"debuglib", f_debuglib},
	{"debugmem", f_debugmem},
	{"debugnmg", f_debugnmg},
	{"decompose", f_decompose},
	{"delay", f_delay},
#ifdef DM_X
	{"dm", f_dm},
#endif
	{"draw", f_edit},
	{"dup", f_dup},
#ifdef DM_X
	{"E", f_evedit},
#endif
	{"e", f_edit},
	{"eac", f_eac},
	{"echo", cmd_echo},
	{"edcodes", f_edcodes},
	{"edcolor", f_edcolor},
	{"edcomb", f_edcomb},
	{"edgedir", f_edgedir},
	{"edmater", f_edmater},
	{"erase", f_erase},
	{"erase_all", f_erase_all},
#ifdef DM_X
	{"ev", f_ev},
#endif
	{"eqn", f_eqn},
	{"exit", f_quit},
	{"extrude", f_extrude},
	{"expand", cmd_expand},
	{"eye_pt", f_eye_pt},
	{"e_muves", f_e_muves},
	{"facedef", f_facedef},
	{"facetize", f_facetize},
	{"fracture", f_fracture},
	{"front",	bv_front},
	{"g", f_group},
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
	{"hide", f_hide },
	{"history", f_history},
	{"hist", cmd_hist},
	{"i", f_instance},
	{"idents", f_tables},
	{"ill", f_ill},
	{"in", f_in},
	{"inside", f_inside},
	{"item", f_itemair},
	{"joint", f_joint},
	{"journal", f_journal},
	{"keep", f_keep},
	{"keypoint", f_keypoint},
	{"kill", f_kill},
	{"killall", f_killall},
	{"killtree", f_killtree},
	{"knob", f_knob},
	{"l", cmd_list},
	{"l_muves", f_l_muves},
	{"labelvert", f_labelvert},
	{"left",		bv_left},
	{"listeval", f_pathsum},
#ifdef DM_X
	{"loadtk", cmd_tk},
#endif
	{"lookat", f_lookat},
	{"ls", dir_print},
	{"M", f_mouse},
	{"make", f_make},
	{"make_bb", f_make_bb},
	{"make_name", f_make_name},
	{"mater", f_mater},
	{"matpick", f_matpick},
	{"memprint", f_memprint},
#ifdef DM_X
	{"mged_update", f_update},
	{"mged_wait", f_wait},
#endif
	{"mirface", f_mirface},
	{"mirror", f_mirror},
#ifdef DM_X
	{"mmenu_get", cmd_mmenu_get},
	{"mmenu_set", cmd_nop},
#endif
	{"model2grid_lu", f_model2grid_lu},
	{"model2view", f_model2view},
	{"model2view_lu", f_model2view_lu},
	{"mrot", f_mrot},
	{"mv", f_name},
	{"mvall", f_mvall},
	{"nirt", f_nirt},
	{"nmg_collapse", f_edge_collapse},
	{"nmg_simplify", f_nmg_simplify},
	{"o_rotate",		be_o_rotate},
	{"o_scale",	be_o_scale},
	{"oed", cmd_oed},
	{"oed_apply", f_oedit_apply},
	{"oed_reset", f_oedit_reset},
#ifdef DM_X
	{"oill",		be_o_illuminate},
#endif
	{"opendb", f_opendb},
	{"orientation", f_orientation},
	{"orot", f_rot_obj},
	{"oscale", f_sc_obj},
	{"output_hook", cmd_output_hook},
	{"overlay", f_overlay},
	{"ox",		be_o_x},
	{"oxy",		be_o_xy},
	{"oxscale",	be_o_xscale},
	{"oy",		be_o_y},
	{"oyscale",	be_o_yscale},
	{"ozscale",	be_o_zscale},
	{"p", f_param},
	{"pathlist", cmd_pathlist},
	{"paths", f_pathsum},
	{"permute", f_permute},
	{"plot", f_plot},
	{"pl", f_pl},
	{"polybinout", f_polybinout},
	{"pov", f_pov},
	{"prcolor", f_prcolor},
	{"prefix", f_prefix},
	{"press", f_press},
	{"preview", f_preview},
	{"ps", f_ps},
	{"push", f_push},
	{"put_comb", cmd_put_comb},
	{"put_sed", f_put_sedit},
	{"putmat", f_putmat},
	{"q", f_quit},
	{"qray", f_qray},
	{"query_ray", f_nirt},
	{"quit", f_quit},
	{"qorot", f_qorot},
	{"qvrot", f_qvrot},
	{"r", f_region},
	{"rate",		bv_rate_toggle},
	{"rcodes", f_rcodes},
	{"read_muves", f_read_muves},
	{"rear",	bv_rear},
	{"red", f_red},
	{"redraw_vlist", cmd_redraw_vlist},
#ifdef DM_X
	{"refresh", f_refresh},
#endif
	{"regdebug", f_regdebug},
	{"regdef", f_regdef},
	{"regions", f_tables},
	{"reject",	be_reject},
	{"release", f_release},
	{"reset",	bv_reset},
	{"restore",	bv_vrestore},
	{"right",	bv_right},
	{"rfarb", f_rfarb},
	{"rm", f_rm},
	{"rmater", f_rmater},
	{"rmats", f_rmats},
	{"rot", f_rot},
	{"rotobj", f_rot_obj},
	{"rrt", f_rrt},
	{"rset", f_rset},
	{"rt", f_rt},
	{"rt_abort", cmd_rt_abort},
	{"rtcheck", f_rtcheck},
	{"save",		bv_vsave},
#if 0
	{"savedit", f_savedit},
#endif
	{"savekey", f_savekey},
	{"saveview", f_saveview},
	{"sca", f_sca},
	{"sed", f_sed},
	{"sedit",	be_s_edit},
	{"sed_apply", f_sedit_apply},
	{"sed_reset", f_sedit_reset},
	{"set_more_default", cmd_set_more_default},
	{"setview", f_setview},
	{"shader", f_shader},
	{"share", f_share},
	{"shells", f_shells},
	{"showmats", f_showmats},
	{"sill",		be_s_illuminate},
	{"size", f_size},
	{"solids", f_tables},
	{"solids_on_ray", cmd_solids_on_ray},
	{"srot",		be_s_rotate},
	{"sscale",	be_s_scale},
	{"status", f_status},
	{"stuff_str", cmd_stuff_str},
	{"summary", f_summary},
	{"sv", f_slewview},
	{"svb", f_svbase},
	{"sync", f_sync},
	{"sxy",		be_s_trans},
	{"t", dir_print},
	{"ted", f_tedit},
	{"tie", f_tie},
	{"title", f_title},
	{"tol", f_tol},
	{"top",		bv_top},
	{"tops", f_tops},
	{"tra", f_tra},
	{"track", f_amtrack},
	{"translate", f_tr_obj},
	{"tree", f_tree},
	{"t_muves", f_t_muves},
	{"unhide", f_unhide},
	{"units", f_units},
	{"vars", f_set},
	{"vdraw", cmd_vdraw},
	{"view", f_view},
	{"view_ring", f_view_ring},
	{"viewget", cmd_viewget},
	{"viewset", cmd_viewset},
	{"viewsize", f_size},		/* alias "size" for saveview scripts */
	{"view2grid_lu", f_view2grid_lu},
	{"view2model", f_view2model},
	{"view2model_vec", f_view2model_vec},
	{"view2model_lu", f_view2model_lu},
	{"vnirt", f_vnirt},
	{"vquery_ray", f_vnirt},
	{"vrmgr", f_vrmgr},
	{"vrot", f_vrot},
#if 0
	{"vrot_center", f_vrot_center},
#endif
	{"wcodes", f_wcodes},
	{"whatid", f_whatid},
	{"whichair", f_which},
	{"whichid", f_which},
	{"which_shader", f_which_shader},
	{"who", cmd_who},
	{"winset", f_winset},
	{"wmater", f_wmater},
	{"x", f_debug},
	{"xpush", f_xpush},
	{"Z", f_zap},
	{"zoom", f_zoom},
	{"zoomin",	bv_zoomin},
	{"zoomout",	bv_zoomout},
	{0, 0}
};


/*
 *                        O U T P U T _ C A T C H
 *
 * Gets the output from bu_log and appends it to clientdata vls.
 */

HIDDEN int
output_catch(clientdata, str)
	genptr_t clientdata;
	genptr_t str;
{
	register struct bu_vls *vp = (struct bu_vls *)clientdata;
	register int len;

	BU_CK_VLS(vp);
	len = bu_vls_strlen(vp);
	bu_vls_strcat(vp, str);
	len = bu_vls_strlen(vp) - len;

	return len;
}

/*
 *                 S T A R T _ C A T C H I N G _ O U T P U T
 *
 * Sets up hooks to bu_log so that all output is caught in the given vls.
 *
 */

void
start_catching_output(vp)
	struct bu_vls *vp;
{
	bu_log_add_hook(output_catch, (genptr_t)vp);
}

/*
 *                 S T O P _ C A T C H I N G _ O U T P U T
 *
 * Turns off the output catch hook.
 */

void
stop_catching_output(vp)
	struct bu_vls *vp;
{
	bu_log_delete_hook(output_catch, (genptr_t)vp);
}

#if 0
/*
 *	T C L _ A P P I N I T
 *
 *	Called by the Tcl/Tk libraries for initialization.
 *	Unncessary in our case; cmd_setup does all the work.
 */


int
Tcl_AppInit(interp)
	Tcl_Interp *interp;		/* Interpreter for application. */
{
	return TCL_OK;
}

/*			C M D _ W R A P P E R
 *
 * Translates between MGED's "CMD_OK/BAD/MORE" result codes to ones that
 * Tcl can understand.
 */

int
cmd_wrapper(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	int status;
	size_t len;
	struct bu_vls result;
	int catch_output;

	argv[0] = ((struct funtab *)clientData)->ft_name;

	/* We now leave the world of Tcl where everything prints its results
	   in the interp->result field.  Here, stuff gets printed with the
	   bu_log command; hence, we must catch such output and stuff it into
	   the result string.  Do this *only* if "output_as_return" global
	   variable is set.  Make a local copy of this variable in case it's
	   changed by our command. */

	catch_output = output_as_return;
    
	bu_vls_init(&result);

	if (catch_output)
		start_catching_output(&result);
	status = mged_cmd(argc, argv, funtab);
	if (catch_output)
		stop_catching_output(&result);

	/* Remove any trailing newlines. */

	if (catch_output) {
		len = bu_vls_strlen(&result);
		while (len > 0 && bu_vls_addr(&result)[len-1] == '\n')
			bu_vls_trunc(&result, --len);
	}
    
	switch (status) {
	case CMD_OK:
		if (catch_output)
			Tcl_SetResult(interp, bu_vls_addr(&result), TCL_VOLATILE);
		status = TCL_OK;
		break;
	case CMD_BAD:
		if (catch_output)
			Tcl_SetResult(interp, bu_vls_addr(&result), TCL_VOLATILE);
		status = TCL_ERROR;
		break;
	case CMD_MORE:
		Tcl_SetResult(interp, MORE_ARGS_STR, TCL_STATIC);
		if (catch_output) {
			Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
		}
		status = TCL_ERROR;
		break;
	default:
		Tcl_SetResult(interp, "error executing mged routine::", TCL_STATIC);
		if (catch_output) {
			Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
		}
		status = TCL_ERROR;
		break;
	}
    
	bu_vls_free(&result);
	return status;
}
#endif

/*
 *                            G U I _ O U T P U T
 *
 * Used as a hook for bu_log output.  Sends output to the Tcl procedure whose
 * name is contained in the vls "tcl_output_hook".  Useful for user interface
 * building.
 */

int
gui_output(clientData, str)
	genptr_t clientData;
	genptr_t str;
{
	Tcl_DString tclcommand;
	Tcl_Obj *save_result;
	static int level = 0;

	if (level > 50) {
		bu_log_delete_hook(gui_output, clientData);
		/* Now safe to run bu_log? */
		bu_log("Ack! Something horrible just happened recursively.\n");
		return 0;
	}

	Tcl_DStringInit(&tclcommand);
	(void)Tcl_DStringAppendElement(&tclcommand, bu_vls_addr(&tcl_output_hook));
	(void)Tcl_DStringAppendElement(&tclcommand, str);

	save_result = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(save_result);
	++level;
	Tcl_Eval((Tcl_Interp *)clientData, Tcl_DStringValue(&tclcommand));
	--level;
	Tcl_SetObjResult(interp, save_result);
	Tcl_DecrRefCount(save_result);

	Tcl_DStringFree(&tclcommand);
	return strlen(str);
}

/*
 *                     C M D _ T K
 *
 *  Command for initializing the Tk window and defaults.
 *
 *  Usage:  loadtk [displayname[.screennum]]
 */
int
cmd_tk(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	int status;

	if(argc < 1 || 2 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help loadtk");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if(argc == 1)
		status = gui_setup((char *)NULL);
	else
		status = gui_setup(argv[1]);

	return status;
}    

/*
 *   C M D _ O U T P U T _ H O O K
 *
 *   Hooks the output to the given output hook.
 *   Removes the existing output hook!
 */

int
cmd_output_hook(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;    
	int argc;
	char **argv;
{
	struct bu_vls infocommand;
	int status;

	if(argc < 1 || 2 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helpdevel output_hook");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_log_delete_hook(gui_output, (genptr_t)interp);/* Delete the existing hook */

	if (argc < 2)
		return TCL_OK;

	/* Make sure the command exists before putting in the hook! */
    
	bu_vls_init(&infocommand);
	bu_vls_strcat(&infocommand, "info commands ");
	bu_vls_strcat(&infocommand, argv[1]);
	status = Tcl_Eval(interp, bu_vls_addr(&infocommand));
	bu_vls_free(&infocommand);

	if (status != TCL_OK || interp->result[0] == '\0') {
		Tcl_AppendResult(interp, "command does not exist", (char *)NULL);
		return TCL_ERROR;
	}

	/* Also, don't allow silly infinite loops. */

	if (strcmp(argv[1], argv[0]) == 0) {
		Tcl_AppendResult(interp, "Don't be silly.", (char *)NULL);
		return TCL_ERROR;
	}

	/* Set up the hook! */

	bu_vls_strcpy(&tcl_output_hook, argv[1]);
	bu_log_add_hook(gui_output, (genptr_t)interp);
    
	Tcl_ResetResult(interp);
	return TCL_OK;
}


/*
 *			C M D _ N O P
 */
int
cmd_nop(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	return TCL_OK;
}


/*
 *			C M D _ G E T _ P T R
 *
 *  Returns an appropriately-formatted string that can later be reinterpreted
 *  (using atol() and a cast) as a a pointer.
 */

int
cmd_get_ptr(clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	char buf[128];

	sprintf( buf, "%ld", (long)(*((void **)&dbip)) );
	Tcl_AppendResult( interp, buf, (char *)NULL );
	return TCL_OK;
}

/*
 *
 * Sets up the Tcl interpreter
 */ 
void
mged_setup()
{
	struct bu_vls str;
	char *filename;

	/* The following is for GUI output hooks: contains name of function to
	   run with output */
	bu_vls_init(&tcl_output_hook);

	/* Locate the BRL-CAD-specific Tcl scripts */
	filename = bu_brlcad_path( "" );

	/* Create the interpreter */
	interp = Tcl_CreateInterp();

	/* This runs the init.tcl script */
	if( Tcl_Init(interp) == TCL_ERROR )
		bu_log("Tcl_Init error %s\n", interp->result);

	/* Initialize [incr Tcl] */
	if (Itcl_Init(interp) == TCL_ERROR)
	  bu_log("Itcl_Init error %s\n", interp->result);

	/* Import [incr Tcl] commands into the global namespace. */
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		       "::itcl::*", /* allowOverwrite */ 1) != TCL_OK)
	  bu_log("Tcl_Import error %s\n", interp->result);

	/* Initialize libbu */
	Bu_Init(interp);

	/* Initialize libbn */
	Bn_Init(interp);

	/* Initialize librt */
	if (Rt_Init(interp) == TCL_ERROR) {
		bu_log("Rt_Init error %s\n", interp->result);
	}

	/* register commands */
	cmd_setup();

	history_setup();
	mged_global_variable_setup(interp);
#if !TRY_NEW_MGED_VARS
	mged_variable_setup(interp);
#endif

	bu_vls_init(&str);
	bu_vls_printf(&str, "set auto_path [linsert $auto_path 0 \
                             %stclscripts/mged %stclscripts \
                             %stclscripts/lib %stclscripts/util]",
		      filename, filename, filename, filename);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));

	/* Tcl needs to write nulls onto subscripted variable names */
	bu_vls_trunc( &str, 0 );
	bu_vls_printf( &str, "%s(state)", MGED_DISPLAY_VAR );
	Tcl_SetVar(interp, bu_vls_addr(&str), state_str[state],
		   TCL_GLOBAL_ONLY);

	/* initialize "Query Ray" variables */
	init_qray();

	Tcl_ResetResult(interp);

	bu_vls_free(&str);
}

/* 			C M D _ S E T U P
 * Register all the MGED commands.
 */
void
cmd_setup()
{
	register struct cmdtab *ctp;
	struct bu_vls temp;

	bu_vls_init(&temp);
	for (ctp = cmdtab; ctp->ct_name != NULL; ctp++) {
#if 0
		bu_vls_strcpy(&temp, "info commands ");
		bu_vls_strcat(&temp, ctp->ct_name);
		if (Tcl_Eval(interp, bu_vls_addr(&temp)) != TCL_OK ||
		    interp->result[0] != '\0') {
			bu_log("WARNING:  '%s' name collision (%s)\n", ctp->ct_name,
			       interp->result);
		}
#endif
		bu_vls_strcpy(&temp, "_mged_");
		bu_vls_strcat(&temp, ctp->ct_name);
	
		(void)Tcl_CreateCommand(interp, ctp->ct_name, ctp->ct_func,
					(ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
		(void)Tcl_CreateCommand(interp, bu_vls_addr(&temp), ctp->ct_func,
					(ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
	}

	/* overrides/wraps the built-in tree command */
	{
		struct bu_vls	vls;
		char		*pathname;

		/* Locate the BRL-CAD-specific Tcl scripts */
		pathname = bu_brlcad_path("");

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "source %stclscripts/mged/tree.tcl", pathname);
		(void)Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
	}

	bu_vls_strcpy(&temp, "glob_compat_mode");
	Tcl_LinkVar(interp, bu_vls_addr(&temp), (char *)&glob_compat_mode,
		    TCL_LINK_BOOLEAN);
	bu_vls_strcpy(&temp, "output_as_return");
	Tcl_LinkVar(interp, bu_vls_addr(&temp), (char *)&output_as_return,
		    TCL_LINK_BOOLEAN);

	/* Provide Tcl interfaces to the fundamental BRL-CAD libraries */
	bu_tcl_setup( interp );
	bn_tcl_setup( interp );
	Rt_Init(interp);

#ifdef DM_X
	tkwin = NULL;
#endif

	bu_vls_free(&temp);
}

int
cmd_cmd_win(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	struct bu_vls vls;

	bu_vls_init(&vls);

	if(argc < 2){
		bu_vls_printf(&vls, "helpdevel cmd_win");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if(strcmp(argv[1], "open") == 0){
		struct cmd_list *clp;
		int name_not_used = 1;

		if(argc != 3){
			bu_vls_printf(&vls, "helpdevel cmd_win");
			Tcl_Eval(interp, bu_vls_addr(&vls));
			bu_vls_free(&vls);
			return TCL_ERROR;
		}

		/* Search to see if there exists a command window with this name */
		for( BU_LIST_FOR(clp, cmd_list, &head_cmd_list.l) )
			if(!strcmp(argv[2], bu_vls_addr(&clp->cl_name))){
				name_not_used = 0;
				break;
			}

		if(name_not_used){
			clp = (struct cmd_list *)bu_malloc(sizeof(struct cmd_list), "cmd_list");
			bzero((void *)clp, sizeof(struct cmd_list));
			BU_LIST_APPEND(&head_cmd_list.l, &clp->l);
			clp->cl_cur_hist = head_cmd_list.cl_cur_hist;
			bu_vls_init(&clp->cl_more_default);
			bu_vls_init(&clp->cl_name);
			bu_vls_strcpy(&clp->cl_name, argv[2]);
		}

		bu_vls_free(&vls);
		return TCL_OK;
	}

	if(strcmp(argv[1], "close") == 0){
		struct cmd_list *clp;

		if(argc != 3){
			bu_vls_printf(&vls, "helpdevel cmd_win");
			Tcl_Eval(interp, bu_vls_addr(&vls));
			bu_vls_free(&vls);
			return TCL_ERROR;
		}

		/* First, search to see if there exists a command window with the name
		   in argv[2] */
		for( BU_LIST_FOR(clp, cmd_list, &head_cmd_list.l) )
			if(!strcmp(argv[2], bu_vls_addr(&clp->cl_name)))
				break;

		if(clp == &head_cmd_list){
			if(!strcmp(argv[2], "mged"))
				Tcl_AppendResult(interp, "cmd_close: not allowed to close \"mged\"",
								(char *)NULL);
			else
				Tcl_AppendResult(interp, "cmd_close: did not find \"", argv[2],
							"\"", (char *)NULL);
			return TCL_ERROR;
		}

		if(clp == curr_cmd_list)
			curr_cmd_list = &head_cmd_list;

		BU_LIST_DEQUEUE( &clp->l );
		if(clp->cl_tie != NULL)
			clp->cl_tie->dml_tie = CMD_LIST_NULL;
		bu_vls_free(&clp->cl_more_default);
		bu_vls_free(&clp->cl_name);
		bu_free((genptr_t)clp, "cmd_close: clp");

		bu_vls_free(&vls);
		return TCL_OK;
	}

	if(strcmp(argv[1], "get") == 0){
		if(argc != 2){
			bu_vls_printf(&vls, "helpdevel cmd_win");
			Tcl_Eval(interp, bu_vls_addr(&vls));
			bu_vls_free(&vls);

			return TCL_ERROR;
		}

		Tcl_AppendElement(interp, bu_vls_addr(&curr_cmd_list->cl_name));

		bu_vls_free(&vls);
		return TCL_OK;
	}

	if(strcmp(argv[1], "set") == 0){
		if(argc != 3){
			bu_vls_printf(&vls, "helpdevel cmd_win");
			Tcl_Eval(interp, bu_vls_addr(&vls));
			bu_vls_free(&vls);
			return TCL_ERROR;
		}

		for( BU_LIST_FOR(curr_cmd_list, cmd_list, &head_cmd_list.l) ){
			if(strcmp(bu_vls_addr(&curr_cmd_list->cl_name), argv[2]))
				continue;

			break;
		}

		if(curr_cmd_list->cl_tie)
			curr_dm_list = curr_cmd_list->cl_tie;

		bu_vls_trunc(&curr_cmd_list->cl_more_default, 0);
		bu_vls_free(&vls);
		return TCL_OK;
	}

	bu_vls_printf(&vls, "helpdevel cmd_win");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

#if 0
cmd_get(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	struct dm_list *p;
	struct cmd_list *save_clp;
	struct bu_vls vls;
	int first = 1;

	if(argc != 1){
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helpdevel cmd_get");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	save_clp = curr_cmd_list;
	bu_vls_init(&vls);

	if(!curr_cmd_list->cl_tie){
		if(curr_dm_list->dml_tie){
			Tcl_AppendElement(interp, bu_vls_addr(&curr_dm_list->dml_tie->cl_name));
			curr_cmd_list = curr_dm_list->dml_tie;
			Tcl_AppendElement(interp, bu_vls_addr(&pathName));
		}else{
			Tcl_AppendElement(interp, bu_vls_addr(&curr_cmd_list->cl_name));
			Tcl_AppendElement(interp, bu_vls_addr(&pathName));
			Tcl_AppendElement(interp, bu_vls_addr(&vls));
			bu_vls_free(&vls);
			return TCL_OK;
		}
	}else{
		Tcl_AppendElement(interp, bu_vls_addr(&curr_cmd_list->cl_name));
		Tcl_AppendElement(interp, bu_vls_addr(&curr_cmd_list->cl_tie->dml_dmp->dm_pathName));
	}

	/* return all ids associated with the current command window */
	FOR_ALL_DISPLAYS(p, &head_dm_list.l){
		/* The display manager tied to the current command window shares
		   information with display manager p */
		if(curr_cmd_list->cl_tie->dml_view_state == p->dml_view_state)
			/* This display manager is tied to a command window */
			if(p->dml_tie)
				if(first){
					bu_vls_printf(&vls, "%S", &p->dml_tie->cl_name);
					first = 0;
				} else
					bu_vls_printf(&vls, " %S", &p->dml_tie->cl_name);
	}

	Tcl_AppendElement(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	curr_cmd_list = save_clp;
	return TCL_OK;
}
#endif

int
cmd_get_more_default(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	if(argc != 1){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helpdevel get_more_default");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	Tcl_AppendResult(interp, bu_vls_addr(&curr_cmd_list->cl_more_default), (char *)NULL);
	return TCL_OK;
}

int
cmd_set_more_default(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	if(argc != 2){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helpdevel set_more_default");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_strcpy(&curr_cmd_list->cl_more_default, argv[1]);
	return TCL_OK;
}


int
cmd_mged_glob(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	struct bu_vls dest, src;

	if(argc != 2){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help db_glob");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_init(&src);
	bu_vls_init(&dest);
	bu_vls_strcpy(&src, argv[1]);
	mged_compat(&dest, &src, 1);
	Tcl_AppendResult(interp, bu_vls_addr(&dest), (char *)NULL);
	bu_vls_free(&src);
	bu_vls_free(&dest);

	return TCL_OK;
}


/*
 * debackslash, backslash_specials, mged_compat: routines for original
 *   mged emulation mode
 */

void
debackslash( dest, src )
	struct bu_vls *dest, *src;
{
	char *ptr;

	ptr = bu_vls_addr(src);
	while( *ptr ) {
		if( *ptr == '\\' )
			++ptr;
		if( *ptr == '\0' )
			break;
		bu_vls_putc( dest, *ptr++ );
	}
}

void
backslash_specials( dest, src )
	struct bu_vls *dest, *src;
{
	int backslashed;
	char *ptr, buf[2];

	buf[1] = '\0';
	backslashed = 0;
	for( ptr = bu_vls_addr( src ); *ptr; ptr++ ) {
		if( *ptr == '[' && !backslashed )
			bu_vls_strcat( dest, "\\[" );
		else if( *ptr == ']' && !backslashed )
			bu_vls_strcat( dest, "\\]" );
		else if( backslashed ) {
			bu_vls_strcat( dest, "\\" );
			buf[0] = *ptr;
			bu_vls_strcat( dest, buf );
			backslashed = 0;
		} else if( *ptr == '\\' )
			backslashed = 1;
		else {
			buf[0] = *ptr;
			bu_vls_strcat( dest, buf );
		}
	}
}

/*                    M G E D _ C O M P A T
 *
 * This routine is called to perform wildcard expansion and character quoting
 * on the given vls (typically input from the keyboard.)
 */

void
mged_compat( dest, src, use_first )
	struct bu_vls *dest, *src;
	int use_first;
{
	char *start, *end;          /* Start and ends of words */
	int regexp;                 /* Set to TRUE when word is a regexp */
	int backslashed;
	int firstword;
	struct bu_vls word;         /* Current word being processed */
	struct bu_vls temp;

	if(dbip == DBI_NULL){
	  bu_vls_vlscat(dest, src);
	  return;
	}

	bu_vls_init( &word );
	bu_vls_init( &temp );
    
	start = end = bu_vls_addr( src );
	firstword = 1;
	while( *end != '\0' ) {            /* Run through entire string */

		/* First, pass along leading whitespace. */

		start = end;                   /* Begin where last word ended */
		while( *start != '\0' ) {
			if( *start == ' '  ||
			    *start == '\t' ||
			    *start == '\n' )
				bu_vls_putc( dest, *start++ );
			else
				break;
		}
		if( *start == '\0' )
			break;

		/* Next, advance "end" pointer to the end of the word, while adding
		   each character to the "word" vls.  Also make a note of any
		   unbackslashed wildcard characters. */

		end = start;
		bu_vls_trunc( &word, 0 );
		regexp = 0;
		backslashed = 0;
		while( *end != '\0' ) {
			if( *end == ' '  ||
			    *end == '\t' ||
			    *end == '\n' )
				break;
			if( (*end == '*' || *end == '?' || *end == '[') && !backslashed )
				regexp = 1;
			if( *end == '\\' && !backslashed )
				backslashed = 1;
			else
				backslashed = 0;
			bu_vls_putc( &word, *end++ );
		}

		if( firstword && !use_first )
			regexp = 0;

		/* Now, if the word was suspected of being a wildcard, try to match
		   it to the database. */

		if( regexp ) {
			bu_vls_trunc( &temp, 0 );
			if( db_regexp_match_all( &temp, dbip,
						 bu_vls_addr(&word) ) == 0 ) {
				debackslash( &temp, &word );
				backslash_specials( dest, &temp );
			} else
				bu_vls_vlscat( dest, &temp );
		} else {
			debackslash( dest, &word );
		}

		firstword = 0;
	}

	bu_vls_free( &temp );
	bu_vls_free( &word );
}
    
/*
 *			C M D L I N E
 *
 *  This routine is called to process a vls full of commands.
 *  Each command is newline terminated.
 *  The input string will not be altered in any way.
 *
 *  Returns -
 *	!0	when a prompt needs to be printed.
 *	 0	no prompt needed.
 */

int
cmdline( struct bu_vls *vp, int record )
{
	int	status;
	struct bu_vls globbed;
	struct bu_vls tmp_vls;
	struct bu_vls save_vp;
	struct timeval start, finish;
	size_t len;
	extern struct bu_vls mged_prompt;
	char *cp;

	BU_CK_VLS(vp);

	if (bu_vls_strlen(vp) <= 0)
		return CMD_OK;
		
	bu_vls_init(&globbed);
	bu_vls_init(&tmp_vls);
	bu_vls_init(&save_vp);
	bu_vls_vlscat(&save_vp, vp);

	/* MUST MAKE A BACKUP OF THE INPUT STRING AND USE THAT IN THE CALL TO
	   Tcl_Eval!!!
       
	   You never know who might change the string (append to it...)
	   (f_mouse is notorious for adding things to the input string)
	   If it were to change while it was still being evaluated, Horrible Things
	   could happen.
	*/

	if (glob_compat_mode)
		mged_compat(&globbed, vp, 0);
	else
		bu_vls_vlscat(&globbed, vp);

	gettimeofday(&start, (struct timezone *)NULL);
	status = Tcl_Eval(interp, bu_vls_addr(&globbed));
	gettimeofday(&finish, (struct timezone *)NULL);

	/* Contemplate the result reported by the Tcl interpreter. */

	switch (status) {
	case TCL_RETURN:
	case TCL_OK:
		if( setjmp( jmp_env ) == 0 ){
			len = strlen(interp->result);

			/* If the command had something to say, print it out. */	     
			if (len > 0){
				(void)signal( SIGINT, sig3);  /* allow interupts */

				bu_log("%s%s", interp->result,
				       interp->result[len-1] == '\n' ? "" : "\n");

				(void)signal( SIGINT, SIG_IGN );
			}


#ifdef DM_X
			/* A user typed this command so let everybody see, then record
			   it in the history. */
			if (record && tkwin != NULL) {
				bu_vls_printf(&tmp_vls, "distribute_text {} {%s} {%s}",
					      bu_vls_addr(&save_vp), interp->result);
				Tcl_Eval(interp, bu_vls_addr(&tmp_vls));
				Tcl_SetResult(interp, "", TCL_STATIC);
			}
#endif

			if(record)
				history_record(&save_vp, &start, &finish, CMD_OK);

		}else{
/* XXXXXX */
			bu_semaphore_release(BU_SEM_SYSCALL);
			bu_log("\n");
		}
      
		bu_vls_strcpy(&mged_prompt, MGED_PROMPT);
		status = CMD_OK;
		goto end;

	case TCL_ERROR:
	default:

		/* First check to see if it's a secret message. */

		if ((cp = strstr(interp->result, MORE_ARGS_STR)) != NULL) {
			if(cp == interp->result){
				bu_vls_trunc(&mged_prompt, 0);
				bu_vls_printf(&mged_prompt, "\r%s",
					      interp->result+sizeof(MORE_ARGS_STR)-1);
			}else{
				len = cp - interp->result;
				bu_log("%*s%s", len, interp->result, interp->result[len-1] == '\n' ? "" : "\n");
				bu_vls_trunc(&mged_prompt, 0);
				bu_vls_printf(&mged_prompt, "\r%s",
					      interp->result+sizeof(MORE_ARGS_STR)-1+len);
			}

			status = CMD_MORE;
			goto end;
		}

		/* Otherwise, it's just a regular old error. */    

		len = strlen(interp->result);
		if (len > 0) bu_log("%s%s", interp->result,
				    interp->result[len-1] == '\n' ? "" : "\n");

		if (record)
			history_record(&save_vp, &start, &finish, CMD_BAD);

		bu_vls_strcpy(&mged_prompt, MGED_PROMPT);
		status = CMD_BAD;

		/* Fall through to end */
	}

 end:
	bu_vls_free(&globbed);
	bu_vls_free(&tmp_vls);
	bu_vls_free(&save_vp);

	return status;
}


void
mged_print_result(int status)
{
	int len;
	extern void pr_prompt();

#if 0
	switch (status) {
	case TCL_OK:
		len = strlen(interp->result);

		/* If the command had something to say, print it out. */	     
		if (len > 0){
			bu_log("%s%s", interp->result,
			       interp->result[len-1] == '\n' ? "" : "\n");

			pr_prompt();
		}

		break;

	case TCL_ERROR:
	default:
		len = strlen(interp->result);
		if (len > 0){
			bu_log("%s%s", interp->result,
			       interp->result[len-1] == '\n' ? "" : "\n");

			pr_prompt();
		}

		break;
	}
#else
	len = strlen(interp->result);
	if (len > 0){
		bu_log("%s%s", interp->result,
		       interp->result[len-1] == '\n' ? "" : "\n");

		pr_prompt();
	}
#endif

	Tcl_ResetResult(interp);
}

/*
 *			M G E D _ C M D
 *
 *  Check a table for the command, check for the correct minimum and maximum
 *  number of arguments, and pass control to the proper function.  If the
 *  number of arguments is incorrect, print out a short help message.
 */

int
mged_cmd(
	int argc,
	char **argv,
	struct funtab in_functions[])
{
	register struct funtab *ftp;
	struct funtab *functions;

	if (argc == 0)
		return CMD_OK;	/* No command entered, that's fine */

	/* if no function table is provided, use the default mged function table */
	if( in_functions == (struct funtab *)NULL )
	{
		bu_log("mged_cmd: failed to supply function table!\n");
		return CMD_BAD;
	}
	else
		functions = in_functions;

	for (ftp = &functions[1]; ftp->ft_name; ftp++) {
		if (strcmp(ftp->ft_name, argv[0]) != 0)
			continue;
		/* We have a match */
		if ((ftp->ft_min <= argc) && (argc <= ftp->ft_max)) {
			/* Input has the right number of args.
			 * Call function listed in table, with
			 * main(argc, argv) style args
			 */

			switch (ftp->ft_func(argc, argv)) {
			case CMD_OK:
				return CMD_OK;
			case CMD_BAD:
				return CMD_BAD;
			case CMD_MORE:
				return CMD_MORE;
			default:
				Tcl_AppendResult(interp, "mged_cmd(): Invalid return from ",
						 ftp->ft_name, "\n", (char *)NULL);
				return CMD_BAD;
			}
		}

		Tcl_AppendResult(interp, "Usage: ", functions[0].ft_name, ftp->ft_name,
				 " ", ftp->ft_parms, "\n\t(", ftp->ft_comment,
				 ")\n", (char *)NULL);
		return CMD_BAD;
	}

	Tcl_AppendResult(interp, functions[0].ft_name, argv[0],
			 ": no such command, type '", functions[0].ft_name,
			 "?' for help\n", (char *)NULL);
	return CMD_BAD;
}

/* Let the user temporarily escape from the editor */
/* Format: %	*/

int
f_comm(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int	argc;
	char	**argv;
{

	register int pid, rpid;
	int retcode;

	if(argc != 1 || !classic_mged || curr_cmd_list != &head_cmd_list){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help %s", argv[0]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	(void)signal( SIGINT, SIG_IGN );
	if ( ( pid = fork()) == 0 )  {
		(void)signal( SIGINT, SIG_DFL );
		(void)execl("/bin/sh","-",(char *)NULL);
		perror("/bin/sh");
		mged_finish( 11 );
	}

	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;

	Tcl_AppendResult(interp, "!\n", (char *)NULL);

	return TCL_OK;
}

/* Quit and exit gracefully */
/* Format: q	*/

int
f_quit(
	ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
	if(argc < 1 || 1 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help %s", argv[0]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if( state != ST_VIEW )
		button( BE_REJECT );

	quit();			/* Exiting time */
	/* NOTREACHED */
	return TCL_OK;
}

/* wrapper for sync() */

int
f_sync(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{

	if(argc < 1 || 1 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help sync");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	sync();
    
	return TCL_OK;
}

/*
 *			H E L P C O M M
 *
 *  Common code for help commands
 */

static int
helpcomm( argc, argv, functions)
	int	argc;
	char	**argv;
	struct funtab *functions;
{
	register struct funtab *ftp;
	register int	i, bad;

	bad = 0;
	
	/* Help command(s) */
	for( i=1; i<argc; i++ )  {
		for( ftp = functions+1; ftp->ft_name; ftp++ )  {
			if( strcmp( ftp->ft_name, argv[i] ) != 0 )
				continue;

			Tcl_AppendResult(interp, "Usage: ", functions->ft_name, ftp->ft_name,
					 " ", ftp->ft_parms, "\n\t(", ftp->ft_comment, ")\n", (char *)NULL);
			break;
		}
		if( !ftp->ft_name ) {
			Tcl_AppendResult(interp, functions->ft_name, argv[i],
					 ": no such command, type '", functions->ft_name,
					 "?' for help\n", (char *)NULL);
			bad = 1;
		}
	}

	return bad ? TCL_ERROR : TCL_OK;
}

/*
 *			F _ H E L P
 *
 *  Print a help message, two lines for each command.
 *  Or, help with the indicated commands.
 */

int
f_help2(argc, argv, functions)
	int argc;
	char **argv;
	struct funtab *functions;
{
	register struct funtab *ftp;

	if( argc <= 1 )  {
		Tcl_AppendResult(interp, "The following commands are available:\n", (char *)NULL);
		for( ftp = functions+1; ftp->ft_name; ftp++ )  {
			Tcl_AppendResult(interp,  functions->ft_name, ftp->ft_name, " ",
					 ftp->ft_parms, "\n\t(", ftp->ft_comment, ")\n", (char *)NULL);
		}
		return TCL_OK;
	}
	return helpcomm( argc, argv, functions );
}

int
f_fhelp2( argc, argv, functions)
	int	argc;
	char	**argv;
	struct funtab *functions;
{
	register struct funtab *ftp;
	struct bu_vls		str;

	if( argc <= 1 )  {
		bu_vls_init(&str);
		Tcl_AppendResult(interp, "The following ", functions->ft_name,
				 " commands are available:\n", (char *)NULL);
		for( ftp = functions+1; ftp->ft_name; ftp++ )  {
			vls_col_item( &str, ftp->ft_name);
		}
		vls_col_eol( &str );
		Tcl_AppendResult(interp, bu_vls_addr( &str ), (char *)NULL);
		bu_vls_free(&str);
		return TCL_OK;
	}
	return helpcomm( argc, argv, functions );
}

int
f_summary(clientData, interp, argc, argv )
	ClientData clientData;
	Tcl_Interp *interp;
	int	argc;
	char	**argv;
{
	register char *cp;
	int flags = 0;
	int bad;

	if(argc < 1 || 2 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help summary");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bad = 0;
	if( argc <= 1 )  {
		dir_summary(0);
		return TCL_OK;
	}
	cp = argv[1];
	while( *cp )  switch( *cp++ )  {
	case 's':
		flags |= DIR_SOLID;
		break;
	case 'r':
		flags |= DIR_REGION;
		break;
	case 'g':
		flags |= DIR_COMB;
		break;
	default:
		Tcl_AppendResult(interp, "summary:  S R or G are only valid parmaters\n",
				 (char *)NULL);
		bad = 1;
		break;
	}

	dir_summary(flags);
	return bad ? TCL_ERROR : TCL_OK;
}

/*
 *                          C M D _ E C H O
 *
 * Concatenates its arguments and "bu_log"s the resulting string.
 */

int
cmd_echo(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int	argc;
	char	*argv[];
{
	register int i;

	if(argc < 1){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help echo");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	for( i=1; i < argc; i++ )  {
		Tcl_AppendResult(interp, i==1 ? "" : " ", argv[i], (char *)NULL);
	}

	Tcl_AppendResult(interp, "\n", (char *)NULL);

	return TCL_OK;
}

#if 0
int
f_savedit(argc, argv)
	int	argc;
	char	*argv[];
{
	struct bu_vls str;
	char	line[35];
	int o_ipathpos;
	register struct solid *o_illump;

	o_illump = illump;
	bu_vls_init(&str);

	if(state == ST_S_EDIT){
		bu_vls_strcpy( &str, "press accept\npress sill\n" );
		cmdline(&str, 0);
		illump = o_illump;
		bu_vls_strcpy( &str, "M 1 0 0\n");
		cmdline(&str, 0);
		return CMD_OK;
	}else if(state == ST_O_EDIT){
		o_ipathpos = ipathpos;
		bu_vls_strcpy( &str, "press accept\npress oill\n" );
		cmdline(&str, 0);
		(void)chg_state( ST_O_PICK, ST_O_PATH, "savedit");
		illump = o_illump;
		ipathpos = o_ipathpos;
		bu_vls_strcpy( &str, "M 1 0 0\n");
		cmdline(&str, 0);
		return CMD_OK;
	}

	bu_log( "Savedit will only work in an edit state\n");
	bu_vls_free(&str);
	return CMD_BAD;
}
#endif

/*
 * SYNOPSIS
 *	tie [cw [dm]]
 *	tie -u cw
 *
 * DESCRIPTION
 *	This command ties/associates a command window (cw) to a display manager window (dm).
 *	When a command window is tied to a display manager window, all commands issued from
 *	this window will be directed at a particular display manager. Otherwise, the
 *	commands issued will be directed at the current display manager window.
 *
 * EXAMPLES
 *	tie		--->	returns a list of the command_window/display_manager associations
 *	tie cw1		--->	returns the display_manager, if it exists, associated with cw1
 *	tie cw1 dm1	--->	associated cw1 with dm1
 *	tie -u cw1	--->	removes the association, if it exists, cw1 has with a display manager
 */
int
f_tie(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	register int uflag = 0;		/* untie flag */
	struct cmd_list *clp;
	struct dm_list *dlp;
	struct bu_vls vls;

	bu_vls_init(&vls);

	if(argc < 1 || 3 < argc){
		bu_vls_printf(&vls, "helpdevel tie");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if(argc == 1){
		for( BU_LIST_FOR(clp, cmd_list, &head_cmd_list.l) ){
			bu_vls_trunc(&vls, 0);
			if(clp->cl_tie){
				bu_vls_printf(&vls, "%S %S", &clp->cl_name,
						&clp->cl_tie->dml_dmp->dm_pathName);
				Tcl_AppendElement(interp, bu_vls_addr(&vls));
			}else{
				bu_vls_printf(&vls, "%S {}", &clp->cl_name);
				Tcl_AppendElement(interp, bu_vls_addr(&vls));
			}
		}

		bu_vls_trunc(&vls, 0);
		if(clp->cl_tie){
			bu_vls_printf(&vls, "%S %S", &clp->cl_name,
                                                &clp->cl_tie->dml_dmp->dm_pathName);
			Tcl_AppendElement(interp, bu_vls_addr(&vls));
		}else{
			bu_vls_printf(&vls, "%S {}", &clp->cl_name);
			Tcl_AppendElement(interp, bu_vls_addr(&vls));
		}

		bu_vls_free(&vls);
		return TCL_OK;
	}

	if(argv[1][0] == '-' && argv[1][1] == 'u'){
		uflag = 1;
		--argc;
		++argv;
	}

	if(argc < 2){
                bu_vls_printf(&vls, "help tie");
                Tcl_Eval(interp, bu_vls_addr(&vls));
                bu_vls_free(&vls);
                return TCL_ERROR;
	}

	for( BU_LIST_FOR(clp, cmd_list, &head_cmd_list.l) )
		if(!strcmp(bu_vls_addr(&clp->cl_name), argv[1]))
			break;

	if(clp == &head_cmd_list &&
	   (strcmp(bu_vls_addr(&head_cmd_list.cl_name), argv[1]))){
		Tcl_AppendResult(interp, "f_tie: unrecognized command_window - ", argv[1],
				 "\n", (char *)NULL);
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if(uflag){
		if(clp->cl_tie)
			clp->cl_tie->dml_tie = (struct cmd_list *)NULL;

		clp->cl_tie = (struct dm_list *)NULL;

		bu_vls_free(&vls);
		return TCL_OK;
	}

	/* print out the display manager that we're tied to */
	if(argc == 2){
		if(clp->cl_tie)
			Tcl_AppendElement(interp, bu_vls_addr(&clp->cl_tie->dml_dmp->dm_pathName));
		else
			Tcl_AppendElement(interp, "");

		bu_vls_free(&vls);
		return TCL_OK;
	}

	if(*argv[2] != '.')
		bu_vls_printf(&vls, ".%s", argv[2]);
	else
		bu_vls_strcpy(&vls, argv[2]);

	FOR_ALL_DISPLAYS(dlp, &head_dm_list.l)
		if(!strcmp(bu_vls_addr(&vls), bu_vls_addr(&dlp->dml_dmp->dm_pathName)))
			break;

	if(dlp == &head_dm_list){
		Tcl_AppendResult(interp, "f_tie: unrecognized pathName - ",
				 bu_vls_addr(&vls), "\n", (char *)NULL);
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* already tied */
	if(clp->cl_tie)
		clp->cl_tie->dml_tie = (struct cmd_list *)NULL;

	clp->cl_tie = dlp;

	/* already tied */
	if(dlp->dml_tie)
		dlp->dml_tie->cl_tie = (struct dm_list *)NULL;

	dlp->dml_tie = clp;

	bu_vls_free(&vls);
	return TCL_OK;
}

int
f_ps(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	int status;
	char *av[2];
	struct dm_list *dml;
	struct _view_state *vsp;

	if(argc < 2){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help ps");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	dml = curr_dm_list;
	status = mged_attach(&which_dm[DM_PS_INDEX], argc, argv);
	if(status == TCL_ERROR)
		return TCL_ERROR;

	vsp = view_state;  /* save state info pointer */
	view_state = dml->dml_view_state;  /* use dml's state info */
	*mged_variables = *dml->dml_mged_variables; /* struct copy */

	bu_free((genptr_t)menu_state,"f_ps: menu_state");
	menu_state = dml->dml_menu_state;

	scroll_top = dml->dml_scroll_top;
	scroll_active = dml->dml_scroll_active;
	scroll_y = dml->dml_scroll_y;
	bcopy((void *)dml->dml_scroll_array, (void *)scroll_array,
	      sizeof(struct scroll_item *) * 6);

	dirty = 1;
	refresh();

	view_state = vsp;  /* restore state info pointer */
	av[0] = "release";
	av[1] = NULL;
	status = f_release(clientData, interp, 1, av);
	curr_dm_list = dml;

	return status;
}

/*
 * Experimental - like f_plot except we attach to dm-plot, passing along
 *                any arguments.
 */
int
f_pl(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	int status;
	char *av[2];
	struct dm_list *dml;
	struct _view_state *vsp;

	if(argc < 2){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help pl");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	dml = curr_dm_list;
	status = mged_attach(&which_dm[DM_PLOT_INDEX], argc, argv);
	if(status == TCL_ERROR)
		return TCL_ERROR;

	vsp = view_state;  /* save state info pointer */
	view_state = dml->dml_view_state;  /* use dml's state info */
	*mged_variables = *dml->dml_mged_variables; /* struct copy */

	bu_free((genptr_t)menu_state,"f_pl: menu_state");
	menu_state = dml->dml_menu_state;

	scroll_top = dml->dml_scroll_top;
	scroll_active = dml->dml_scroll_active;
	scroll_y = dml->dml_scroll_y;
	bcopy( (void *)dml->dml_scroll_array, (void *)scroll_array,
	       sizeof(struct scroll_item *) * 6);

	dirty = 1;
	refresh();

	view_state = vsp;  /* restore state info pointer */
	av[0] = "release";
	av[1] = NULL;
	status = f_release(clientData, interp, 1, av);
	curr_dm_list = dml;

	return status;
}

int
f_winset(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int     argc;
	char    **argv;
{
	register struct dm_list *p;

	if(argc < 1 || 2 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helpdevel winset");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* print pathname of drawing window with primary focus */
	if( argc == 1 ){
		Tcl_AppendResult(interp, bu_vls_addr(&pathName), (char *)NULL);
		return TCL_OK;
	}

	/* change primary focus to window argv[1] */
	FOR_ALL_DISPLAYS(p, &head_dm_list.l){
		if( !strcmp( argv[1], bu_vls_addr( &p->dml_dmp->dm_pathName ) ) ){
			curr_dm_list = p;

			if(curr_dm_list->dml_tie)
				curr_cmd_list = curr_dm_list->dml_tie;
			else
				curr_cmd_list = &head_cmd_list;

			return TCL_OK;
		}
	}

	Tcl_AppendResult(interp, "Unrecognized pathname - ", argv[1],
			 "\n", (char *)NULL);
	return TCL_ERROR;
}

void
mged_global_variable_setup(interp)
Tcl_Interp *interp;
{
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_strcpy(&vls, "mged_default(dlist)");
	Tcl_LinkVar(interp, bu_vls_addr(&vls), (char *)&mged_default_dlist, TCL_LINK_INT);
	bu_vls_strcpy(&vls, "edit_class");
	Tcl_LinkVar(interp, bu_vls_addr(&vls), (char *)&es_edclass, TCL_LINK_INT);
	bu_vls_strcpy(&vls, "edit_solid_flag");
	Tcl_LinkVar(interp, bu_vls_addr(&vls), (char *)&es_edflag, TCL_LINK_INT);
	bu_vls_strcpy(&vls, "edit_object_flag");
	Tcl_LinkVar(interp, bu_vls_addr(&vls), (char *)&edobj, TCL_LINK_INT);

	bu_vls_init(&edit_info_vls);
	bu_vls_strcpy(&edit_info_vls, "edit_info");
}

int
f_bot_face_fuse( clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int     argc;
	char    **argv;
{
	struct directory *old_dp, *new_dp;
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc != 3){
	  Tcl_AppendResult(interp, "Usage:\nbot_face_fuse new_bot_solid old_bot_solid\n", (char *)NULL );
	  return TCL_ERROR;
	}

	if( (old_dp = db_lookup( dbip, argv[2], LOOKUP_NOISY )) == DIR_NULL )
		return TCL_ERROR;

	if( rt_db_get_internal( &intern, old_dp, dbip, bn_mat_identity, &rt_uniresource ) < 0 )
	{
	  Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( intern.idb_type != ID_BOT )
	{
		Tcl_AppendResult(interp, argv[2], " is not a BOT solid!!!\n", (char *)NULL );
		return TCL_ERROR;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC( bot );

	(void) rt_bot_face_fuse( bot );

	if( (new_dp=db_diradd( dbip, argv[1], -1L, 0, DIR_SOLID, (genptr_t)&intern.idb_type)) == DIR_NULL )
	{
		Tcl_AppendResult(interp, "Cannot add ", argv[1], " to directory\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( rt_db_put_internal( new_dp, dbip, &intern, &rt_uniresource ) < 0 )
	{
		rt_db_free_internal( &intern, &rt_uniresource );
		TCL_WRITE_ERR_return;
	}
	return TCL_OK;
}

int
f_bot_fuse(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int     argc;
	char    **argv;
{
	struct directory *old_dp, *new_dp;
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;
	int count1=0;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc != 3){
	  Tcl_AppendResult(interp, "Usage:\nbot_fuse new_bot_solid old_bot_solid\n", (char *)NULL );
	  return TCL_ERROR;
	}

	if( (old_dp = db_lookup( dbip, argv[2], LOOKUP_NOISY )) == DIR_NULL )
		return TCL_ERROR;

	if( rt_db_get_internal( &intern, old_dp, dbip, bn_mat_identity, &rt_uniresource ) < 0 )
	{
	  Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( intern.idb_type != ID_BOT )
	{
		Tcl_AppendResult(interp, argv[2], " is not a BOT solid!!!\n", (char *)NULL );
		return TCL_ERROR;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC( bot );

	count1 = rt_bot_vertex_fuse( bot );
	if( count1 )
		(void)rt_bot_condense( bot );

	if( (new_dp=db_diradd( dbip, argv[1], -1L, 0, DIR_SOLID, (genptr_t)&intern.idb_type)) == DIR_NULL )
	{
		Tcl_AppendResult(interp, "Cannot add ", argv[1], " to directory\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( rt_db_put_internal( new_dp, dbip, &intern, &rt_uniresource ) < 0 )
	{
		rt_db_free_internal( &intern, &rt_uniresource );
		TCL_WRITE_ERR_return;
	}
	return TCL_OK;
}

int
f_bot_condense(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int     argc;
	char    **argv;
{
	struct directory *old_dp, *new_dp;
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;
	int count2=0;
	char count_str[255];

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc != 3){
	  Tcl_AppendResult(interp, "Usage:\nbot_condense new_bot_solid old_bot_solid\n", (char *)NULL );
	  return TCL_ERROR;
	}

	if( (old_dp = db_lookup( dbip, argv[2], LOOKUP_NOISY )) == DIR_NULL )
		return TCL_ERROR;

	if( rt_db_get_internal( &intern, old_dp, dbip, bn_mat_identity, &rt_uniresource ) < 0 )
	{
	  Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( intern.idb_type != ID_BOT )
	{
		Tcl_AppendResult(interp, argv[2], " is not a BOT solid!!!\n", (char *)NULL );
		return TCL_ERROR;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC( bot );

	count2 = rt_bot_condense( bot );
	sprintf( count_str, "%d", count2 );
	Tcl_AppendResult(interp, count_str, " dead vertices eliminated\n", (char *)NULL );

	if( (new_dp=db_diradd( dbip, argv[1], -1L, 0, DIR_SOLID, (genptr_t)&intern.idb_type)) == DIR_NULL )
	{
		Tcl_AppendResult(interp, "Cannot add ", argv[1], " to directory\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( rt_db_put_internal( new_dp, dbip, &intern, &rt_uniresource ) < 0 )
	{
		rt_db_free_internal( &intern, &rt_uniresource );
		TCL_WRITE_ERR_return;
	}
	return TCL_OK;
}

#if 1
int
f_test_bomb_hook(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int     argc;
	char    **argv;
{
	bu_bomb("\nTesting MGED's bomb hook!\n");

	/* This is never reached */
	return TCL_OK;
}
#endif
