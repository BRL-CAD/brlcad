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
static char RCSid[] = "@(#)$Header$ (BRL)";
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

#include "tk.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "externs.h"
#include "./ged.h"
#include "./cmd.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./sedit.h"

#include "./mgedtcl.h"

int get_more_default();
void mged_setup(), cmd_setup(), mged_compat();
void mged_print_result();

extern void set_scroll();  /* defined in set.c */
extern void sync();
extern int gui_setup();

struct cmd_list head_cmd_list;
struct cmd_list *curr_cmd_list;

int glob_compat_mode = 1;
int output_as_return = 1;

int mged_cmd();
struct bu_vls tcl_output_hook;

Tcl_Interp *interp = NULL;
Tk_Window tkwin;


/*
 *			C M D _ L E F T _ M O U S E
 *
 *  Default old-MGED binding for left mouse button.
 */
int
cmd_left_mouse(clientData, interp, argc, argv)
	ClientData	clientData;
	Tcl_Interp	*interp;
	int		argc;
	char		*argv[];
{
	if(argc < 4 || 4 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help L");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if( atoi(argv[1]) != 0 ){
		int status;
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "zoom 0.5\n");
		status = Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return status;
	}

	return TCL_OK;
}

/*
 *			C M D _ R I G H T _ M O U S E
 *
 *  Default old-MGED binding for right mouse button.
 */
int
cmd_right_mouse(clientData, interp, argc, argv)
	ClientData	clientData;
	Tcl_Interp	*interp;
	int		argc;
	char		*argv[];
{
	if(argc < 4 || 4 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help R");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if( atoi(argv[1]) != 0 ){
		int status;
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "zoom 2.0\n");
		status = Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return status;
	}

	return TCL_OK;
}

struct cmdtab {
	char *ct_name;
	int (*ct_func)();
};

static struct cmdtab cmdtab[] = {
	"%", f_comm,
	"3ptarb", f_3ptarb,
	"adc", f_adc,
	"add_view", f_add_view,
	"ae", f_aetview,
	"aim", f_aim,
	"aip", f_aip,
	"analyze", f_analyze,
	"arb", f_arbdef,
	"arced", f_arced,
	"area", f_area,
	"attach", f_attach,
	"B", f_blast,
	"bev", f_bev,
	"c", f_comb_std,
	"cat", f_cat,
	"center", f_center,
	"color", f_color,
	"comb", f_comb,
	"comb_color", f_comb_color,
	"copyeval", f_copyeval,
	"copymat", f_copymat,
	"cp", f_copy,
	"cpi", f_copy_inv,
	"d", f_erase,
	"dall", f_erase_all,
/*	"db", cmd_db, */
	"dbconcat", f_concat,
	"debugbu", f_debugbu,
	"debugdir", f_debugdir,
	"debuglib", f_debuglib,
	"debugmem", f_debugmem,
	"debugnmg", f_debugnmg,
	"decompose", f_decompose,
	"delay", f_delay,
	"delete_view", f_delete_view,
	"dm", f_dm,
	"draw", f_edit,
	"dup", f_dup,
	"E", f_evedit,
	"e", f_edit,
	"eac", f_eac,
	"echo", cmd_echo,
	"edcodes", f_edcodes,
	"edmater", f_edmater,
	"edcolor", f_edcolor,
	"edcomb", f_edcomb,
	"edgedir", f_edgedir,
	"erase", f_erase,
	"erase_all", f_erase_all,
	"ev", f_ev,
	"eqn", f_eqn,
	"exit", f_quit,
	"extrude", f_extrude,
	"expand", cmd_expand,
	"eye_pt", f_eye_pt,
	"facedef", f_facedef,
	"facetize", f_facetize,
	"find", f_find,
	"fix", f_fix,
	"fracture", f_fracture,
	"g", f_group,
	"get_view", f_get_view,
	"goto_view", f_goto_view,
	"output_hook", cmd_output_hook,
#ifdef HIDELINE
	"H", f_hideline,
#endif
	"history", f_history,
	"hist_prev", cmd_prev,
	"hist_next", cmd_next,
	"hist_add", cmd_hist_add,
	"i", f_instance,
	"idents", f_tables,
	"ill", f_ill,
	"in", f_in,
	"inside", f_inside,
	"item", f_itemair,
	"joint", f_joint,
	"journal", f_journal,
	"keep", f_keep,
	"keypoint", f_keypoint,
	"kill", f_kill,
	"killall", f_killall,
	"killtree", f_killtree,
	"knob", f_knob,
	"l", cmd_list,
	"L", cmd_left_mouse,
	"labelvert", f_labelvert,
	"listeval", f_pathsum,
	"loadtk", cmd_tk,
	"lookat", f_lookat,
	"ls", dir_print,
	"M", f_mouse,
	"mrot", f_mrot,
	"make", f_make,
	"make_bb", f_make_bb,
	"mater", f_mater,
	"matpick", f_matpick,
	"memprint", f_memprint,
	"mirface", f_mirface,
	"mirror", f_mirror,
	"model2view", f_model2view,
	"mv", f_name,
	"mvall", f_mvall,
	"next_view", f_next_view,
	"nirt", f_nirt,
	"nmg_simplify", f_nmg_simplify,
	"oed", cmd_oed,
	"opendb", f_opendb,
	"orientation", f_orientation,
	"orot", f_rot_obj,
	"oscale", f_sc_obj,
	"overlay", f_overlay,
	"p", f_param,
	"paths", f_pathsum,
	"pathlist", cmd_pathlist,
	"permute", f_permute,
	"plot", f_plot,
	"pl", f_pl,
	"polybinout", f_polybinout,
	"pov", f_pov,
	"prcolor", f_prcolor,
	"prefix", f_prefix,
	"prev_view", f_prev_view,
	"preview", f_preview,
	"press", f_press,
	"ps", f_ps,
	"push", f_push,
	"putmat", f_putmat,
	"q", f_quit,
	"quit", f_quit,
	"qorot", f_qorot,
	"qvrot", f_qvrot,
	"r", f_region,
	"R", cmd_right_mouse,
	"rcodes", f_rcodes,
	"red", f_red,
	"redraw_vlist", cmd_redraw_vlist,
	"refresh", f_refresh,
	"regdebug", f_regdebug,
	"regdef", f_regdef,
	"regions", f_tables,
	"release", f_release,
	"rfarb", f_rfarb,
	"rm", f_rm,
	"rmater", f_rmater,
	"rmats", f_rmats,
	"rot", f_rot,
	"rotobj", f_rot_obj,
	"rrt", f_rrt,
	"rt", f_rt,
	"rtcheck", f_rtcheck,
#if 0
	"savedit", f_savedit,
#endif
	"savekey", f_savekey,
	"saveview", f_saveview,
	"sca", f_sca,
	"showmats", f_showmats,
	"sed", f_sed,
	"setview", f_setview,
	"shells", f_shells,
	"shader", f_shader,
	"share_menu", f_share_menu,
	"size", f_view,
	"sliders", cmd_sliders,
	"solids", f_tables,
	"solids_on_ray", cmd_solids_on_ray,
	"status", f_status,
	"summary", f_summary,
	"sv", f_slewview,
	"svb", f_svbase,
	"sync", f_sync,
	"t", dir_print,
	"ted", f_tedit,
	"tie", f_tie,
	"title", f_title,
	"toggle_view", f_toggle_view,
	"tol", f_tol,
	"tops", f_tops,
	"tra", f_tra,
	"track", f_amtrack,
	"translate", f_tr_obj,
	"tree", f_tree,
	"unaim", f_unaim,
	"units", f_units,
	"untie", f_untie,
	"mged_update", f_update,
	"vars", f_set,
	"vdraw", cmd_vdraw,
	"viewget", cmd_viewget,
	"viewset", cmd_viewset,
	"view2model", f_view2model,
	"vrmgr", f_vrmgr,
	"vrot", f_vrot,
	"vrot_center", f_vrot_center,
	"wcodes", f_wcodes,
	"whatid", f_whatid,
	"whichair", f_which_air,
	"whichid", f_which_id,
	"which_shader", f_which_shader,
	"who", cmd_who,
	"winset", f_winset,
	"wmater", f_wmater,
	"x", f_debug,
	"xpush", f_xpush,
	"Z", f_zap,
	"zoom", f_zoom,
	0, 0
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
	bu_add_hook(output_catch, (genptr_t)vp);
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
	bu_delete_hook(output_catch, (genptr_t)vp);
}

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

#if 0
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
	static int level = 0;

	if (level > 50) {
		bu_delete_hook(gui_output, clientData);
		/* Now safe to run bu_log? */
		bu_log("Ack! Something horrible just happened recursively.\n");
		return 0;
	}

	Tcl_DStringInit(&tclcommand);
	(void)Tcl_DStringAppendElement(&tclcommand, bu_vls_addr(&tcl_output_hook));
	(void)Tcl_DStringAppendElement(&tclcommand, str);

	++level;
	Tcl_Eval((Tcl_Interp *)clientData, Tcl_DStringValue(&tclcommand));
	--level;

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
		bu_vls_printf(&vls, "help output_hook");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_delete_hook(gui_output, (genptr_t)interp);/* Delete the existing hook */

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
	bu_add_hook(gui_output, (genptr_t)interp);
    
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

	sprintf( buf, "%ld", (long)(*((void **)clientData)) );
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
	struct bu_vls str2;
	char *filename;

	/* The following is for GUI output hooks: contains name of function to
	   run with output */
	bu_vls_init(&tcl_output_hook);

	/* Create the interpreter */
	interp = Tcl_CreateInterp();

	/* This runs the init.tcl script */
	if( Tcl_Init(interp) == TCL_ERROR )
		bu_log("Tcl_Init error %s\n", interp->result);

	/* register commands */
	cmd_setup();

	(void)Tcl_CreateCommand(interp, "mmenu_set", cmd_nop, (ClientData)NULL,
				(Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "mmenu_get", cmd_mmenu_get,
				(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	history_setup();
#if !TRY_NEW_MGED_VARS
	mged_variable_setup(interp);
#endif

	Tcl_LinkVar(interp, "edit_class", (char *)&es_edclass, TCL_LINK_INT);

	bu_vls_init(&edit_info_vls);
	bu_vls_init(&edit_rate_model_tran_vls[X]);
	bu_vls_init(&edit_rate_model_tran_vls[Y]);
	bu_vls_init(&edit_rate_model_tran_vls[Z]);
	bu_vls_init(&edit_rate_view_tran_vls[X]);
	bu_vls_init(&edit_rate_view_tran_vls[Y]);
	bu_vls_init(&edit_rate_view_tran_vls[Z]);
	bu_vls_init(&edit_rate_model_rotate_vls[X]);
	bu_vls_init(&edit_rate_model_rotate_vls[Y]);
	bu_vls_init(&edit_rate_model_rotate_vls[Z]);
	bu_vls_init(&edit_rate_object_rotate_vls[X]);
	bu_vls_init(&edit_rate_object_rotate_vls[Y]);
	bu_vls_init(&edit_rate_object_rotate_vls[Z]);
	bu_vls_init(&edit_rate_view_rotate_vls[X]);
	bu_vls_init(&edit_rate_view_rotate_vls[Y]);
	bu_vls_init(&edit_rate_view_rotate_vls[Z]);
	bu_vls_init(&edit_rate_scale_vls);
	bu_vls_init(&edit_absolute_model_tran_vls[X]);
	bu_vls_init(&edit_absolute_model_tran_vls[Y]);
	bu_vls_init(&edit_absolute_model_tran_vls[Z]);
	bu_vls_init(&edit_absolute_view_tran_vls[X]);
	bu_vls_init(&edit_absolute_view_tran_vls[Y]);
	bu_vls_init(&edit_absolute_view_tran_vls[Z]);
	bu_vls_init(&edit_absolute_model_rotate_vls[X]);
	bu_vls_init(&edit_absolute_model_rotate_vls[Y]);
	bu_vls_init(&edit_absolute_model_rotate_vls[Z]);
	bu_vls_init(&edit_absolute_object_rotate_vls[X]);
	bu_vls_init(&edit_absolute_object_rotate_vls[Y]);
	bu_vls_init(&edit_absolute_object_rotate_vls[Z]);
	bu_vls_init(&edit_absolute_view_rotate_vls[X]);
	bu_vls_init(&edit_absolute_view_rotate_vls[Y]);
	bu_vls_init(&edit_absolute_view_rotate_vls[Z]);
	bu_vls_init(&edit_absolute_scale_vls);

	bu_vls_strcpy(&edit_info_vls, "edit_info");
	bu_vls_strcpy(&edit_rate_model_tran_vls[X], "edit_rate_model_tran(X)");
	bu_vls_strcpy(&edit_rate_model_tran_vls[Y], "edit_rate_model_tran(Y)");
	bu_vls_strcpy(&edit_rate_model_tran_vls[Z], "edit_rate_model_tran(Z)");
	bu_vls_strcpy(&edit_rate_view_tran_vls[X], "edit_rate_view_tran(X)");
	bu_vls_strcpy(&edit_rate_view_tran_vls[Y], "edit_rate_view_tran(Y)");
	bu_vls_strcpy(&edit_rate_view_tran_vls[Z], "edit_rate_view_tran(Z)");
	bu_vls_strcpy(&edit_rate_model_rotate_vls[X], "edit_rate_model_rotate(X)");
	bu_vls_strcpy(&edit_rate_model_rotate_vls[Y], "edit_rate_model_rotate(Y)");
	bu_vls_strcpy(&edit_rate_model_rotate_vls[Z], "edit_rate_model_rotate(Z)");
	bu_vls_strcpy(&edit_rate_object_rotate_vls[X], "edit_rate_object_rotate(X)");
	bu_vls_strcpy(&edit_rate_object_rotate_vls[Y], "edit_rate_object_rotate(Y)");
	bu_vls_strcpy(&edit_rate_object_rotate_vls[Z], "edit_rate_object_rotate(Z)");
	bu_vls_strcpy(&edit_rate_view_rotate_vls[X], "edit_rate_view_rotate(X)");
	bu_vls_strcpy(&edit_rate_view_rotate_vls[Y], "edit_rate_view_rotate(Y)");
	bu_vls_strcpy(&edit_rate_view_rotate_vls[Z], "edit_rate_view_rotate(Z)");
	bu_vls_strcpy(&edit_rate_scale_vls, "edit_rate_scale");
	bu_vls_strcpy(&edit_absolute_model_tran_vls[X], "edit_abs_model_tran(X)");
	bu_vls_strcpy(&edit_absolute_model_tran_vls[Y], "edit_abs_model_tran(Y)");
	bu_vls_strcpy(&edit_absolute_model_tran_vls[Z], "edit_abs_model_tran(Z)");
	bu_vls_strcpy(&edit_absolute_view_tran_vls[X], "edit_abs_view_tran(X)");
	bu_vls_strcpy(&edit_absolute_view_tran_vls[Y], "edit_abs_view_tran(Y)");
	bu_vls_strcpy(&edit_absolute_view_tran_vls[Z], "edit_abs_view_tran(Z)");
	bu_vls_strcpy(&edit_absolute_model_rotate_vls[X], "edit_abs_model_rotate(X)");
	bu_vls_strcpy(&edit_absolute_model_rotate_vls[Y], "edit_abs_model_rotate(Y)");
	bu_vls_strcpy(&edit_absolute_model_rotate_vls[Z], "edit_abs_model_rotate(Z)");
	bu_vls_strcpy(&edit_absolute_object_rotate_vls[X], "edit_abs_object_rotate(X)");
	bu_vls_strcpy(&edit_absolute_object_rotate_vls[Y], "edit_abs_object_rotate(Y)");
	bu_vls_strcpy(&edit_absolute_object_rotate_vls[Z], "edit_abs_object_rotate(Z)");
	bu_vls_strcpy(&edit_absolute_view_rotate_vls[X], "edit_abs_view_rotate(X)");
	bu_vls_strcpy(&edit_absolute_view_rotate_vls[Y], "edit_abs_view_rotate(Y)");
	bu_vls_strcpy(&edit_absolute_view_rotate_vls[Z], "edit_abs_view_rotate(Z)");
	bu_vls_strcpy(&edit_absolute_scale_vls, "edit_abs_scale");

	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_model_tran_vls[X]),
		    (char *)&edit_rate_model_tran[X], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_model_tran_vls[Y]),
		    (char *)&edit_rate_model_tran[Y], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_model_tran_vls[Z]),
		    (char *)&edit_rate_model_tran[Z], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_view_tran_vls[X]),
		    (char *)&edit_rate_view_tran[X], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_view_tran_vls[Y]),
		    (char *)&edit_rate_view_tran[Y], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_view_tran_vls[Z]),
		    (char *)&edit_rate_view_tran[Z], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_model_rotate_vls[X]),
		    (char *)&edit_rate_model_rotate[X], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_model_rotate_vls[Y]),
		    (char *)&edit_rate_model_rotate[Y], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_model_rotate_vls[Z]),
		    (char *)&edit_rate_model_rotate[Z], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_object_rotate_vls[X]),
		    (char *)&edit_rate_object_rotate[X], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_object_rotate_vls[Y]),
		    (char *)&edit_rate_object_rotate[Y], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_object_rotate_vls[Z]),
		    (char *)&edit_rate_object_rotate[Z], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_view_rotate_vls[X]),
		    (char *)&edit_rate_view_rotate[X], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_view_rotate_vls[Y]),
		    (char *)&edit_rate_view_rotate[Y], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_view_rotate_vls[Z]),
		    (char *)&edit_rate_view_rotate[Z], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_rate_scale_vls),
		    (char *)&edit_rate_scale, TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_model_tran_vls[X]),
		    (char *)&edit_absolute_model_tran[X], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_model_tran_vls[Y]),
		    (char *)&edit_absolute_model_tran[Y], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_model_tran_vls[Z]),
		    (char *)&edit_absolute_model_tran[Z], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_view_tran_vls[X]),
		    (char *)&edit_absolute_view_tran[X], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_view_tran_vls[Y]),
		    (char *)&edit_absolute_view_tran[Y], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_view_tran_vls[Z]),
		    (char *)&edit_absolute_view_tran[Z], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_model_rotate_vls[X]),
		    (char *)&edit_absolute_model_rotate[X], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_model_rotate_vls[Y]),
		    (char *)&edit_absolute_model_rotate[Y], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_model_rotate_vls[Z]),
		    (char *)&edit_absolute_model_rotate[Z], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_object_rotate_vls[X]),
		    (char *)&edit_absolute_object_rotate[X], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_object_rotate_vls[Y]),
		    (char *)&edit_absolute_object_rotate[Y], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_object_rotate_vls[Z]),
		    (char *)&edit_absolute_object_rotate[Z], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_view_rotate_vls[X]),
		    (char *)&edit_absolute_view_rotate[X], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_view_rotate_vls[Y]),
		    (char *)&edit_absolute_view_rotate[Y], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_view_rotate_vls[Z]),
		    (char *)&edit_absolute_view_rotate[Z], TCL_LINK_DOUBLE);
	Tcl_LinkVar(interp, bu_vls_addr(&edit_absolute_scale_vls),
		    (char *)&edit_absolute_scale, TCL_LINK_DOUBLE);

#ifdef BRLCAD_TCL_LIBRARY
	filename = BRLCAD_TCL_LIBRARY;
#else
	filename = "/usr/brlcad/tclscripts";
#endif
	bu_vls_init(&str);
	bu_vls_init(&str2);
	bu_vls_printf(&str2, "%s/mged", filename);
	bu_vls_printf(&str, "set auto_path [linsert $auto_path 0 %s %s]",
		      bu_vls_addr(&str2), filename);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
	bu_vls_free(&str);
	bu_vls_free(&str2);

	Tcl_ResetResult(interp);
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
	bu_vls_free(&temp);

	(void)Tcl_CreateCommand(interp, "mged_glob", cmd_mged_glob,
				(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "cmd_init", cmd_init, (ClientData)NULL,
				(Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "cmd_close", cmd_close, (ClientData)NULL,
				(Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "cmd_set", cmd_set, (ClientData)NULL,
				(Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "cmd_get", cmd_get, (ClientData)NULL,
				(Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "get_more_default", get_more_default, (ClientData)NULL,
				(Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "stuff_str", cmd_stuff_str, (ClientData)NULL,
				(Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "get_dbip", cmd_get_ptr, (ClientData)&dbip, (Tcl_CmdDeleteProc *)NULL);

	/* A synonym, to allow cut-n-paste of rt animation scripts into mged */
	(void)Tcl_CreateCommand(interp, "viewsize", f_view, (ClientData)NULL,
				(Tcl_CmdDeleteProc *)NULL);

	Tcl_LinkVar(interp, "glob_compat_mode", (char *)&glob_compat_mode,
		    TCL_LINK_BOOLEAN);
	Tcl_LinkVar(interp, "output_as_return", (char *)&output_as_return,
		    TCL_LINK_BOOLEAN);

	/* Provide Tcl interfaces to the fundamental BRL-CAD libraries */
	bn_tcl_setup( interp );
	rt_tcl_setup( interp );

	tkwin = NULL;
}


/* initialize a new command window */
int
cmd_init(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	struct cmd_list *clp;
	int name_not_used = 1;

	if(argc != 2){
		Tcl_AppendResult(interp, "Usage: cmd_init name", (char *)NULL);
		return TCL_ERROR;
	}

	/* Search to see if there exists a command window with this name */
	for( BU_LIST_FOR(clp, cmd_list, &head_cmd_list.l) )
		if(!strcmp(argv[1], bu_vls_addr(&clp->name))){
			name_not_used = 0;
			break;
		}

	if(name_not_used){
		clp = (struct cmd_list *)bu_malloc(sizeof(struct cmd_list), "cmd_list");
		bzero((void *)clp, sizeof(struct cmd_list));
		BU_LIST_APPEND(&head_cmd_list.l, &clp->l);
		clp->cur_hist = head_cmd_list.cur_hist;
		bu_vls_init(&clp->more_default);
		bu_vls_init(&clp->name);
		bu_vls_strcpy(&clp->name, argv[1]);
	}else{
		clp->cur_hist = head_cmd_list.cur_hist;

		if(clp->aim != NULL){
			clp->aim->aim = CMD_LIST_NULL;
			clp->aim = DM_LIST_NULL;
		}
	}

	return TCL_OK;
}

/* close a command window */
int
cmd_close(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	struct cmd_list *clp;

	if(argc != 2){
		Tcl_AppendResult(interp, "Usage: cmd_close id", (char *)NULL);
		return TCL_ERROR;
	}

	/* First, search to see if there exists a command window with the name
	   in argv[1] */
	for( BU_LIST_FOR(clp, cmd_list, &head_cmd_list.l) )
		if(!strcmp(argv[1], bu_vls_addr(&clp->name)))
			break;

	if(clp == &head_cmd_list){
		if(!strcmp(argv[1], "mged"))
			Tcl_AppendResult(interp, "cmd_close: not allowed to close \"mged\"", (char *)NULL);
		else
			Tcl_AppendResult(interp, "cmd_close: did not find \"", argv[1], "\"", (char *)NULL);
		return TCL_ERROR;
	}

	if(clp == curr_cmd_list)
		curr_cmd_list = &head_cmd_list;

	BU_LIST_DEQUEUE( &clp->l );
	if(clp->aim != NULL)
		clp->aim->aim = CMD_LIST_NULL;
	bu_vls_free(&clp->more_default);
	bu_vls_free(&clp->name);
	bu_free((genptr_t)clp, "cmd_close: clp");

	return TCL_OK;
}

/* returns a list of ids associated with the current command window */
int
cmd_get(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	struct dm_list *p;
	struct bu_vls vls;

	if(!curr_cmd_list->aim){
		Tcl_AppendElement(interp, bu_vls_addr(&pathName));
		Tcl_AppendElement(interp, bu_vls_addr(curr_dm_list->s_info->opp));
		if(curr_dm_list->aim)
			Tcl_AppendElement(interp, bu_vls_addr(&curr_dm_list->aim->name));
		else
			Tcl_AppendElement(interp, bu_vls_addr(&curr_cmd_list->name));

		return TCL_OK;
	}

	Tcl_AppendElement(interp, bu_vls_addr(&curr_cmd_list->aim->_dmp->dm_pathName));
	Tcl_AppendElement(interp, bu_vls_addr(curr_cmd_list->aim->s_info->opp));
	Tcl_AppendElement(interp, bu_vls_addr(&curr_cmd_list->name));
	bu_vls_init(&vls);

	/* return all ids associated with the current command window */
	for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
		/* The display manager tied to the current command window shares
		   information with display manager p */
		if(curr_cmd_list->aim->s_info == p->s_info)
			/* This display manager is tied to a command window */
			if(p->aim)
				bu_vls_printf(&vls, "{%S} ", &p->aim->name);
	}

	Tcl_AppendElement(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_OK;
}


/* given an id sets the current command window */
int
cmd_set(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	if(argc != 2){
		Tcl_AppendResult(interp, "Usage: cmd_set id", (char *)NULL);
		return TCL_ERROR;
	}

	for( BU_LIST_FOR(curr_cmd_list, cmd_list, &head_cmd_list.l) ){
		if(strcmp(bu_vls_addr(&curr_cmd_list->name), argv[1]))
			continue;

		break;
	}

	if(curr_cmd_list->aim)
		curr_dm_list = curr_cmd_list->aim;

	bu_vls_trunc(&curr_cmd_list->more_default, 0);
	return TCL_OK;
}

int
get_more_default(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	struct cmd_list *p;

	if(argc != 1){
		Tcl_AppendResult(interp, "Usage: get_more_default", (char *)NULL);
		return TCL_ERROR;
	}

	Tcl_AppendResult(interp, bu_vls_addr(&curr_cmd_list->more_default), (char *)NULL);
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
		Tcl_AppendResult(interp, "cmd_mged_glob: There must be only one argument.", (char *)NULL);
		return TCL_ERROR;
	}

	bu_vls_init(&src);
	bu_vls_init(&dest);
	bu_vls_strcpy(&src, argv[1]);
	mged_compat( &dest, &src );
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
mged_compat( dest, src )
	struct bu_vls *dest, *src;
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

		if( firstword )
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
cmdline(vp, record)
	struct bu_vls *vp;
	int record;
{
	int	status;
	struct bu_vls globbed;
	struct bu_vls tmp_vls;
	struct timeval start, finish;
	size_t len;
	extern struct bu_vls mged_prompt;
	char *cp;

	BU_CK_VLS(vp);

	if (bu_vls_strlen(vp) <= 0)
		return CMD_OK;
		
	bu_vls_init(&globbed);
	bu_vls_init(&tmp_vls);

	/* MUST MAKE A BACKUP OF THE INPUT STRING AND USE THAT IN THE CALL TO
	   Tcl_Eval!!!
       
	   You never know who might change the string (append to it...)
	   (f_mouse is notorious for adding things to the input string)
	   If it were to change while it was still being evaluated, Horrible Things
	   could happen.
	*/

	if (glob_compat_mode)
		mged_compat(&globbed, vp);
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


			/* A user typed this command so let everybody see, then record
			   it in the history. */
			if (record && tkwin != NULL) {
				bu_vls_printf(&tmp_vls, "distribute_text {} {%s} {%s}",
					      bu_vls_addr(&globbed), interp->result);
				Tcl_Eval(interp, bu_vls_addr(&tmp_vls));
				Tcl_SetResult(interp, "", TCL_STATIC);
			}

			if(record)
				history_record(vp, &start, &finish, CMD_OK);

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
			history_record(vp, &start, &finish, CMD_BAD);

		bu_vls_strcpy(&mged_prompt, MGED_PROMPT);
		status = CMD_BAD;

		/* Fall through to end */
	}

 end:
	bu_vls_free(&globbed);
	bu_vls_free(&tmp_vls);
	return status;
}


void
mged_print_result(status)
	int status;
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
mged_cmd(argc, argv, in_functions)
	int argc;
	char **argv;
	struct funtab in_functions[];
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

	if(argc < 0 || MAXARGS < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help ?");
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
f_quit(clientData, interp, argc, argv )
	ClientData clientData;
	Tcl_Interp *interp;
	int	argc;
	char	**argv;
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
}

/* wrapper for sync() */

int
f_sync(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	register int i;

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

/* Hook for displays with no buttons */
int
f_press(clientData, interp, argc, argv )
	ClientData clientData;
	Tcl_Interp *interp;
	int	argc;
	char	**argv;
{
	register int i;

	if(argc < 2 || MAXARGS < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help press");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	for( i = 1; i < argc; i++ )
		press( argv[i] );

	return TCL_OK;
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

	if(argc < 1 || MAXARGS < argc){
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

int
f_aim(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	struct cmd_list *clp;
	struct cmd_list *save_cclp;
	struct dm_list *dlp;
	struct dm_list *save_cdlp;
	struct bu_vls vls2;

	if(argc < 1 || 3 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help aim");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if(argc == 1){
		for( BU_LIST_FOR(clp, cmd_list, &head_cmd_list.l) )
			if(clp->aim)
				Tcl_AppendResult(interp, bu_vls_addr(&clp->name), " ---> ",
						 bu_vls_addr(&clp->aim->_dmp->dm_pathName),
						 "\n", (char *)NULL);
			else
				Tcl_AppendResult(interp, bu_vls_addr(&clp->name), " ---> ",
						 "\n", (char *)NULL);

		if(clp->aim)
			Tcl_AppendResult(interp, bu_vls_addr(&clp->name), " ---> ",
					 bu_vls_addr(&clp->aim->_dmp->dm_pathName),
					 "\n", (char *)NULL);
		else
			Tcl_AppendResult(interp, bu_vls_addr(&clp->name), " ---> ",
					 "\n", (char *)NULL);

		return TCL_OK;
	}

	for( BU_LIST_FOR(clp, cmd_list, &head_cmd_list.l) )
		if(!strcmp(bu_vls_addr(&clp->name), argv[1]))
			break;

	if(clp == &head_cmd_list &&
	   (strcmp(bu_vls_addr(&head_cmd_list.name), argv[1]))){
		Tcl_AppendResult(interp, "f_aim: unrecognized command_window - ", argv[1],
				 "\n", (char *)NULL);
		return TCL_ERROR;
	}

	/* print out the display manager being aimed at */
	if(argc == 2){
		if(clp->aim)
			Tcl_AppendResult(interp, bu_vls_addr(&clp->name), " ---> ",
					 bu_vls_addr(&clp->aim->_dmp->dm_pathName),
					 "\n", (char *)NULL);
		else
			Tcl_AppendResult(interp, bu_vls_addr(&clp->name), " ---> ", "\n", (char *)NULL);

		return TCL_OK;
	}

	bu_vls_init(&vls2);

	if(*argv[2] != '.')
		bu_vls_printf(&vls2, ".%s", argv[2]);
	else
		bu_vls_strcpy(&vls2, argv[2]);

	for( BU_LIST_FOR(dlp, dm_list, &head_dm_list.l) )
		if(!strcmp(bu_vls_addr(&vls2), bu_vls_addr(&dlp->_dmp->dm_pathName)))
			break;

	if(dlp == &head_dm_list){
		Tcl_AppendResult(interp, "f_aim: unrecognized pathName - ",
				 bu_vls_addr(&vls2), "\n", (char *)NULL);
		bu_vls_free(&vls2);
		return TCL_ERROR;
	}

	bu_vls_free(&vls2);

	/* already aiming */
	if(clp->aim)
		clp->aim->aim = (struct cmd_list *)NULL;

	clp->aim = dlp;

	/* already being aimed at */
	if(dlp->aim)
		dlp->aim->aim = (struct dm_list *)NULL;

	dlp->aim = clp;

	save_cdlp = curr_dm_list;
	save_cclp = curr_cmd_list;
	curr_dm_list = dlp;
	curr_cmd_list = clp;
	set_scroll();
	curr_dm_list = save_cdlp;
	curr_cmd_list = save_cclp;

	Tcl_AppendResult(interp, bu_vls_addr(&clp->name), " ---> ",
			 bu_vls_addr(&clp->aim->_dmp->dm_pathName),
			 "\n", (char *)NULL);

	return TCL_OK;
}

int
f_unaim(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	struct cmd_list *clp;

	if(argc != 2){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help unaim");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	for( BU_LIST_FOR(clp, cmd_list, &head_cmd_list.l) )
		if(!strcmp(bu_vls_addr(&clp->name), argv[1]))
			break;

	if(clp == &head_cmd_list &&
	   (strcmp(bu_vls_addr(&head_cmd_list.name), argv[1]))){
		Tcl_AppendResult(interp, "f_unaim: unrecognized command_window - ", argv[1],
				 "\n", (char *)NULL);
		return TCL_ERROR;
	}

	if(clp->aim)
		clp->aim->aim = (struct cmd_list *)NULL;

	clp->aim = (struct dm_list *)NULL;

	return TCL_OK;
}

int
f_ps(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	int i;
	int status;
	char *av[2];
	struct dm_list *dml;
	struct shared_info *sip;

	if(argc < 2 || MAXARGS < argc){
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

	sip = curr_dm_list->s_info;  /* save state info pointer */
	curr_dm_list->s_info = dml->s_info;  /* use dml's state info */
	mged_variables = dml->_mged_variables; /* struct copy */
#if 1
	bu_free((genptr_t)curr_dm_list->menu_vars,"f_ps: menu_vars");
	curr_dm_list->menu_vars = dml->menu_vars;
#else
	bcopy((void *)dml->_menu_array, (void *)menu_array,
	      sizeof(struct menu_item *) * NMENU);
	menuflag = dml->_menuflag;
	menu_top = dml->_menu_top;
	cur_menu = dml->_cur_menu;
	cur_item = dml->_cur_menu_item;
#endif
	scroll_top = dml->_scroll_top;
	scroll_active = dml->_scroll_active;
	scroll_y = dml->_scroll_y;
	scroll_edit = dml->_scroll_edit;
	bcopy((void *)dml->_scroll_array, (void *)scroll_array,
	      sizeof(struct scroll_item *) * 6);

	dirty = 1;
	refresh();

	curr_dm_list->s_info = sip;  /* restore state info pointer */
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
	int i;
	int status;
	char *av[2];
	struct dm_list *dml;
	struct shared_info *sip;

	if(argc < 2 || MAXARGS < argc){
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

	sip = curr_dm_list->s_info;  /* save state info pointer */
	curr_dm_list->s_info = dml->s_info;  /* use dml's state info */
	mged_variables = dml->_mged_variables; /* struct copy */
#if 1
	bu_free((genptr_t)curr_dm_list->menu_vars,"f_pl: menu_vars");
	curr_dm_list->menu_vars = dml->menu_vars;
#else
	bcopy((void *)dml->_menu_array, (void *)menu_array,
	      sizeof(struct menu_item *) * NMENU);
	menuflag = dml->_menuflag;
	menu_top = dml->_menu_top;
	cur_menu = dml->_cur_menu;
	cur_item = dml->_cur_menu_item;
#endif
	scroll_top = dml->_scroll_top;
	scroll_active = dml->_scroll_active;
	scroll_y = dml->_scroll_y;
	scroll_edit = dml->_scroll_edit;
	bcopy( (void *)dml->_scroll_array, (void *)scroll_array,
	       sizeof(struct scroll_item *) * 6);

	dirty = 1;
	refresh();

	curr_dm_list->s_info = sip;  /* restore state info pointer */
	av[0] = "release";
	av[1] = NULL;
	status = f_release(clientData, interp, 1, av);
	curr_dm_list = dml;

	return status;
}

int
f_update(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int     argc;
	char    **argv;
{
	if(argc < 1 || 1 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help mged_update");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	event_check(1);  /* non-blocking */

	if( sedraw > 0)
		sedit();

	refresh();

	return TCL_OK;
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
		bu_vls_printf(&vls, "help winset");
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
	for( BU_LIST_FOR(p, dm_list, &head_dm_list.l ) ){
		if( !strcmp( argv[1], bu_vls_addr( &p->_dmp->dm_pathName ) ) ){
			curr_dm_list = p;

			if(curr_dm_list->aim)
				curr_cmd_list = curr_dm_list->aim;
			else
				curr_cmd_list = &head_cmd_list;

			return TCL_OK;
		}
	}

	Tcl_AppendResult(interp, "Unrecognized pathname - ", argv[1],
			 "\n", (char *)NULL);
	return TCL_ERROR;
}
