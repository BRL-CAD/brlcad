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

#include "tcl.h"
#include "tk.h"

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "rtstring.h"
#include "rtlist.h"
#include "rtgeom.h"
#include "externs.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"
#include "./sedit.h"

#include "./mgedtcl.h"

#define MORE_ARGS_STR    "more arguments needed::"

int cmd_sig2();
int cmd_mged_glob();
int cmd_init();
int cmd_set();
int f_tran(), f_irot();
void set_tran();

extern mat_t    ModelDelta;

extern int f_nmg_simplify();
extern int f_make_bb();

extern short earb4[5][18];
extern short earb5[9][18];
extern short earb6[10][18];
extern short earb7[12][18];
extern short earb8[12][18];
extern struct rt_db_internal es_int;
extern short int fixv; /* used in ECMD_ARB_ROTATE_FACE,f_eqn(): fixed vertex */

struct cmd_list head_cmd_list;
struct cmd_list *curr_cmd_list;
point_t e_axis_pos;
void set_e_axis_pos();


/* Carl Nuzman experimental */
#if 1
extern int cmd_vdraw();
extern int cmd_read_center();
extern int cmd_read_scale();
#endif

extern Tcl_CmdProc	cmd_fhelp;

extern void sync();
int	inpara;			/* parameter input from keyboard */

int glob_compat_mode = 1;
int output_as_return = 1;

int mged_cmd();
struct rt_vls tcl_output_hook;

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
	static char	cmd[] = "zoom 0.5\n";
	if( argc < 2 )  {
		Tcl_SetResult(interp, "insufficient args", TCL_STATIC);
		return TCL_ERROR;
	}
	if( atoi(argv[1]) != 0 )
		return Tcl_Eval( interp, cmd );
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
	static char	cmd[] = "zoom 2\n";
	if( argc < 2 )  {
		Tcl_SetResult(interp, "insufficient args", TCL_STATIC);
		return TCL_ERROR;
	}
	if( atoi(argv[1]) != 0 )
		return Tcl_Eval( interp, cmd );
	return TCL_OK;
}


struct funtab {
    char *ft_name;
    char *ft_parms;
    char *ft_comment;
    int (*ft_func)();
    int ft_min;
    int ft_max;
    int tcl_converted;
};

static struct funtab funtab[] = {
"", "", "Primary command Table.",
        0, 0, 0, FALSE,
"?", "", "summary of available commands",
        cmd_fhelp,0,MAXARGS,TRUE,
"%", "", "escape to interactive shell",
	f_comm,1,1,FALSE,
"3ptarb", "", "makes arb given 3 pts, 2 coord of 4th pt, and thickness",
	f_3ptarb, 1, 27,FALSE,
"adc", "[<a1|a2|dst|dh|dv|hv|dx|dy|dz|xyz|reset|help> value(s)]",
	"control the angle/distance cursor",
        f_adc, 1, 5, FALSE,
"ae", "azim elev", "set view using az and elev angles",
	f_aeview, 3, 3, FALSE,
"aip", "[fb]", "advance illumination pointer or path position forward or backward",
        f_aip, 1, 2, FALSE,
"analyze", "[arbname]", "analyze faces of ARB",
	f_analyze,1,MAXARGS,FALSE,
"apropos", "keyword", "finds commands whose descriptions contain the given keyword",
        cmd_apropos, 2, 2, TRUE,
"arb", "name rot fb", "make arb8, rotation + fallback",
	f_arbdef,4,4,FALSE,
"arced", "a/b ...anim_command...", "edit matrix or materials on combination's arc",
	f_arced, 3,MAXARGS,FALSE,
"area", "[endpoint_tolerance]", "calculate presented area of view",
	f_area, 1, 2, FALSE,
"attach", "[device]", "attach to a display processor, or NU",
	f_attach,1,3,FALSE,
"B", "<objects>", "clear screen, edit objects",
	f_blast,2,MAXARGS,FALSE,
"bev",	"[-t] [-P#] new_obj obj1 op obj2 op obj3 op ...", "Boolean evaluation of objects via NMG's",
	f_bev, 2, MAXARGS, FALSE,
#if 0
"button", "number", "simulates a button press, not intended for the user",
	f_button, 2, 2, FALSE,
#endif
"c", "[-gr] comb_name [boolean_expr]", "create or extend a combination using standard notation",
	f_comb_std,3,MAXARGS,FALSE,
"cat", "<objects>", "list attributes (brief)",
	f_cat,2,MAXARGS,FALSE,
"center", "x y z", "set view center",
	f_center, 4,4, FALSE,
"color", "low high r g b str", "make color entry",
	f_color, 7, 7, FALSE,
"comb", "comb_name <operation solid>", "create or extend combination w/booleans",
	f_comb,4,MAXARGS,FALSE,
"dbconcat", "file [prefix]", "concatenate 'file' onto end of present database.  Run 'dup file' first.",
	f_concat, 2, 3, FALSE,
"copyeval", "new_solid path_to_old_solid (seperate path components with spaces, not /)",
	"copy an 'evaluated' path solid",
	f_copyeval, 1, 27, FALSE,
"cp", "from to", "copy [duplicate] object",
	f_copy,3,3, FALSE,
"cpi", "from to", "copy cylinder and position at end of original cylinder",
	f_copy_inv,3,3,FALSE,
"d", "<objects>", "delete list of objects",
	f_delobj,2,MAXARGS,FALSE,
"db", "command", "database manipulation routines",
	cmd_db, 1, MAXARGS, TRUE,
"debugdir", "", "Print in-memory directory, for debugging",
	f_debugdir, 1, 1, FALSE,
"debuglib", "[hex_code]", "Show/set debugging bit vector for librt",
	f_debuglib,1,2,FALSE,
"debugmem", "", "Print librt memory use map",
	f_debugmem, 1, 1, FALSE,
"debugnmg", "[hex code]", "Show/set debugging bit vector for NMG",
	f_debugnmg,1,2,FALSE,
"delay", "sec usec", "Delay for the specified amount of time",
	f_delay,3,3,FALSE,
"dm", "set var [val]", "Do display-manager specific command",
	f_dm, 2, MAXARGS, FALSE,
"dup", "file [prefix]", "check for dup names in 'file'",
	f_dup, 2, 3, FALSE,
"E", "<objects>", "evaluated edit of objects",
	f_evedit,2,MAXARGS,FALSE,
"e", "<objects>", "edit objects",
	f_edit,2,MAXARGS,FALSE,
"echo", "[text]", "echo arguments back",
	cmd_echo, 1, MAXARGS, TRUE,
"edcodes", "object(s)", "edit region ident codes",
	f_edcodes, 2, MAXARGS, FALSE,
"edcolor", "", "text edit color table",
	f_edcolor, 1, 1, FALSE,
"edcomb", "combname Regionflag regionid air los [GIFTmater]", "edit combination record info",
	f_edcomb,6,7,FALSE,
"edgedir", "[delta_x delta_y delta_z]|[rot fb]", "define direction of ARB edge being moved",
	f_edgedir, 3, 4, FALSE,
"ev",	"[-dnqstuvwT] [-P #] <objects>", "evaluate objects via NMG tessellation",
	f_ev, 2, MAXARGS, FALSE,
"eqn", "A B C", "planar equation coefficients",
	f_eqn, 4, 4, FALSE,
"exit", "", "exit",
	f_quit,1,1,FALSE,
"extrude", "#### distance", "extrude dist from face",
	f_extrude,3,3,FALSE,
"expand", "wildcard expression", "expands wildcard expression",
        cmd_expand, 1, MAXARGS, TRUE,
"facedef", "####", "define new face for an arb",
	f_facedef, 2, MAXARGS, FALSE,
"facetize", "[-tT] [-P#] new_obj old_obj(s)", "convert objects to faceted NMG objects at current tol",
	f_facetize, 3, MAXARGS, FALSE,
"find", "<objects>", "find all references to objects",
	f_find, 1, MAXARGS, FALSE,
"fix", "", "fix display after hardware error",
	f_fix,1,1,FALSE,
"fracture", "NMGsolid [prefix]", "fracture an NMG solid into many NMG solids, each containing one face\n",
	f_fracture, 2, 3, FALSE,
"g", "groupname <objects>", "group objects",
	f_group,3,MAXARGS,FALSE,
"getknob", "knobname", "Gets the current setting of the given knob",
        cmd_getknob, 2, 2, TRUE,
"output_hook", "output_hook_name",
       "All output is sent to the Tcl procedure \"output_hook_name\"",
	cmd_output_hook, 1, 2, TRUE,
#ifdef HIDELINE
"H", "plotfile [step_size %epsilon]", "produce hidden-line unix-plot",
	f_hideline,2,4,FALSE,
#endif
"help", "[commands]", "give usage message for given commands",
	f_help,0,MAXARGS,FALSE,
"history", "[-delays]", "list command history",
	f_history, 1, 4,FALSE,
"hist_prev", "", "Returns previous command in history",
        cmd_prev, 1, 1, TRUE,
"hist_next", "", "Returns next command in history",
        cmd_next, 1, 1, TRUE,
"hist_add", "", "Adds command to the history (without executing it)",
        cmd_hist_add, 1, 1, TRUE,
"i", "obj combination [operation]", "add instance of obj to comb",
	f_instance,3,4,FALSE,
"idents", "file object(s)", "make ascii summary of region idents",
	f_tables, 3, MAXARGS, FALSE,
"iknob", "id [val]", "increment knob value",
       f_knob,2,3, FALSE,
"ill", "name", "illuminate object",
	f_ill,2,2,FALSE,
"in", "[-f] [-s] parameters...", "keyboard entry of solids.  -f for no drawing, -s to enter solid edit",
	f_in, 1, MAXARGS, FALSE,
"inside", "", "finds inside solid per specified thicknesses",
	f_inside, 1, MAXARGS, FALSE,
"irot", "x y z", "incremental/relative rotate",
        f_irot, 4, 4, FALSE,
"item", "region item [air]", "change item # or air code",
	f_itemair,3,4,FALSE,
"itran", "x y z", "incremental/relative translate using normalized screen coordinates",
        f_tran, 4, 4,FALSE,
"joint", "command [options]", "articualtion/animation commands",
	f_joint, 1, MAXARGS, FALSE,
"journal", "fileName", "record all commands and timings to journal",
	f_journal, 1, 2, FALSE,
"keep", "keep_file object(s)", "save named objects in specified file",
	f_keep, 3, MAXARGS, FALSE,
"keypoint", "[x y z | reset]", "set/see center of editing transformations",
	f_keypoint,1,4, FALSE,
"kill", "[-f] <objects>", "delete object[s] from file",
	f_kill,2,MAXARGS,FALSE,
"killall", "<objects>", "kill object[s] and all references",
	f_killall, 2, MAXARGS,FALSE,
"killtree", "<object>", "kill complete tree[s] - BE CAREFUL",
	f_killtree, 2, MAXARGS, FALSE,
"knob", "id [val]", "emulate knob twist",
	f_knob,2,3, FALSE,
"l", "<objects>", "list attributes (verbose)",
	cmd_list,2,MAXARGS, TRUE,
"L",  "1|0 xpos ypos", "handle a left mouse event",
	cmd_left_mouse, 4,4, TRUE,
"labelvert", "object[s]", "label vertices of wireframes of objects",
	f_labelvert, 2, MAXARGS, FALSE,
"listeval", "", "lists 'evaluated' path solids",
	f_pathsum, 1, MAXARGS, FALSE,
"loadtk", "[DISPLAY]", "Initializes Tk window library",
        cmd_tk, 1, 2, TRUE,
"ls", "", "table of contents",
	dir_print,1,MAXARGS, TRUE,
"M", "1|0 xpos ypos", "handle a middle mouse event",
	f_mouse, 4,4, FALSE,
"make", "name <arb8|sph|ellg|tor|tgc|rpc|rhc|epa|ehy|eto|part|grip|half|nmg|pipe>", "create a primitive",
	f_make,3,3,FALSE,
"make_bb", "new_rpp_name obj1 [obj2 obj3 ...]", "make a bounding box solid enclosing specified objects",
	f_make_bb, 1, MAXARGS, FALSE,
"mater", "comb [material]", "assign/delete material to combination",
	f_mater,2,8,FALSE,
"matpick", "# or a/b", "select arc which has matrix to be edited, in O_PATH state",
	f_matpick, 2,2,FALSE,
"memprint", "", "print memory maps",
	f_memprint, 1, 1,FALSE,
"mirface", "#### axis", "mirror an ARB face",
	f_mirface,3,3,FALSE,
"mirror", "old new axis", "Arb mirror ??",
	f_mirror,4,4,FALSE,
"mv", "old new", "rename object",
	f_name,3,3,FALSE,
"mvall", "oldname newname", "rename object everywhere",
	f_mvall, 3, 3,FALSE,
"nirt", "", "trace a single ray from current view",
	f_nirt,1,MAXARGS,FALSE,
"nmg_simplify", "[arb|tgc|ell|poly] new_solid nmg_solid", "simplify nmg_solid, if possible",
	f_nmg_simplify, 3,4,FALSE,
"comb_color", "comb R G B", "assign a color to a combination (like 'mater')",
	f_comb_color, 5,5,FALSE,
"oed", "path_lhs path_rhs", "Go from view to object_edit of path_lhs/path_rhs",
	cmd_oed, 3, 3, TRUE,
"opendb", "database.g", "Close current .g file, and open new .g file",
	f_opendb, 2, 2,FALSE,
"orientation", "x y z w", "Set view direction from quaternion",
	f_orientation, 5, 5,FALSE,
"orot", "xdeg ydeg zdeg", "rotate object being edited",
	f_rot_obj, 4, 4,FALSE,
"overlay", "file.plot [name]", "Read UNIX-Plot as named overlay",
	f_overlay, 2, 3,FALSE,
"p", "dx [dy dz]", "set parameters",
	f_param,2,4,FALSE,
"paths", "pattern", "lists all paths matching input path",
	f_pathsum, 1, MAXARGS,FALSE,
"pathlist", "name(s)", "list all paths from name(s) to leaves",
	cmd_pathlist, 1, MAXARGS,TRUE,
"permute", "tuple", "permute vertices of an ARB",
	f_permute,2,2,FALSE,
"plot", "[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]", "make UNIX-plot of view",
	f_plot, 2, MAXARGS,FALSE,
"polybinout", "file", "store vlist polygons into polygon file (experimental)",
	f_polybinout, 2, 2,FALSE,
"pov", "args", "experimental:  set point-of-view",
	f_pov, 3+4+1, MAXARGS,FALSE,
"prcolor", "", "print color&material table",
	f_prcolor, 1, 1,FALSE,
"prefix", "new_prefix object(s)", "prefix each occurrence of object name(s)",
	f_prefix, 3, MAXARGS,FALSE,
"preview", "[-v] [-d sec_delay] rt_script_file", "preview new style RT animation script",
	f_preview, 2, MAXARGS,FALSE,
"press", "button_label", "emulate button press",
	f_press,2,MAXARGS,FALSE,
#if 0
"ps", "[f] file", "create postscript file of current view with or without the faceplate",
        f_ps, 2, 3,FALSE,
#endif
"push", "object[s]", "pushes object's path transformations to solids",
	f_push, 2, MAXARGS,FALSE,
"putmat", "a/b {I | m0 m1 ... m16}", "replace matrix on combination's arc",
	f_putmat, 3,MAXARGS,FALSE,
"q", "", "quit",
	f_quit,1,1,FALSE,
"quit", "", "quit",
	f_quit,1,1,FALSE,
"qorot", "x y z dx dy dz theta", "rotate object being edited about specified vector",
	f_qorot, 8, 8,FALSE,
"qvrot", "dx dy dz theta", "set view from direction vector and twist angle",
	f_qvrot, 5, 5,FALSE,
"r", "region <operation solid>", "create or extend a Region combination",
	f_region,4,MAXARGS,FALSE,
"R",  "1|0 xpos ypos", "handle a right mouse event",
	cmd_right_mouse, 4,4, TRUE,
"red", "object", "edit a group or region using a text editor",
	f_red, 2, 2,FALSE,
"refresh", "", "send new control list",
	f_refresh, 1,1,FALSE,
"regdebug", "", "toggle register print",
	f_regdebug, 1,2,FALSE,
"regdef", "item [air] [los] [GIFTmaterial]", "change next region default codes",
	f_regdef, 2, 5,FALSE,
"regions", "file object(s)", "make ascii summary of regions",
	f_tables, 3, MAXARGS,FALSE,
"release", "[name]", "release display processor",
	f_release,1,2,FALSE,
"rfarb", "", "makes arb given point, 2 coord of 3 pts, rot, fb, thickness",
	f_rfarb, 1, 27,FALSE,
"rm", "comb <members>", "remove members from comb",
	f_rm,3,MAXARGS,FALSE,
"rmats", "file", "load views from file (experimental)",
	f_rmats,2,MAXARGS,FALSE,
"rotobj", "xdeg ydeg zdeg", "rotate object being edited",
	f_rot_obj, 4, 4,FALSE,
"rrt", "prog [options]", "invoke prog with view",
	f_rrt,2,MAXARGS,FALSE,
"rt", "[options]", "do raytrace of view",
	f_rt,1,MAXARGS,FALSE,
"rtcheck", "[options]", "check for overlaps in current view",
	f_rtcheck,1,MAXARGS,FALSE,
#if 0
"savedit", "", "save current edit and remain in edit state",
	f_savedit, 1, 1,FALSE,
#endif
"savekey", "file [time]", "save keyframe in file (experimental)",
	f_savekey,2,MAXARGS,FALSE,
"saveview", "file [args]", "save view in file for RT",
	f_saveview,2,MAXARGS,FALSE,
"showmats", "path", "show xform matrices along path",
	f_showmats,2,2,FALSE,
"oscale", "factor", "scale object by factor",
	f_sc_obj,2,2,FALSE,
"sed", "<path>", "solid-edit named solid",
	f_sed,2,2,FALSE,
"setview", "x y z", "set the view given angles x, y, and z in degrees",
        f_setview,4,4,FALSE,
"vars",	"[var=opt]", "assign/display mged variables",
	f_set,1,2,FALSE,
"shells", "nmg_model", "breaks model into seperate shells",
	f_shells, 2,2,FALSE,
"shader", "comb material [arg(s)]", "assign materials (like 'mater')",
	f_shader, 3,MAXARGS,FALSE,
"size", "size", "set view size",
	f_view, 2,2,FALSE,
#if 0
"slider", "slider number, value", "adjust sliders using keyboard",
	f_slider, 3,3,FALSE,
#endif
"sliders", "[{on|off}]", "turns the sliders on or off, or reads current state",
        cmd_sliders, 1, 2, TRUE,
"solids", "file object(s)", "make ascii summary of solid parameters",
	f_tables, 3, MAXARGS,FALSE,
"solids_on_ray", "h v", "List all displayed solids along a ray",
        cmd_solids_on_ray, 1, 3, TRUE,
"status", "", "get view status",
	f_status, 1,1,FALSE,
"summary", "[s r g]", "count/list solid/reg/groups",
	f_summary,1,2,FALSE,
"sv", "x y", "Move view center to (x, y, 0)",
	f_slewview, 3, 3,FALSE,
"sync",	"",	"forces UNIX sync",
	f_sync, 1, 1,FALSE,
"t", "", "table of contents",
	dir_print,1,MAXARGS,TRUE,
"tab", "object[s]", "tabulates objects as stored in database",
	f_tabobj, 2, MAXARGS,FALSE,
"ted", "", "text edit a solid's parameters",
	f_tedit,1,1,FALSE,
"tie", "pathName1 pathName2", "tie display manager pathName1 to display manager pathName2",
        f_tie, 3,3,FALSE,
"title", "string", "change the title",
	f_title,1,MAXARGS,FALSE,
"tol", "[abs #] [rel #] [norm #] [dist #] [perp #]", "show/set tessellation and calculation tolerances",
	f_tol, 1, 11,FALSE,
"tops", "", "find all top level objects",
	f_tops,1,1,FALSE,
"track", "<parameters>", "adds tracks to database",
	f_amtrack, 1, 27,FALSE,
"tran", "x y z", "absolute translate using view coordinates",
        f_tran, 4, 4,FALSE,
"translate", "x y z", "trans object to x,y, z",
	f_tr_obj,4,4,FALSE,
"tree",	"object(s)", "print out a tree of all members of an object",
	f_tree, 2, MAXARGS,FALSE,
"units", "[mm|cm|m|in|ft|...]", "change units",
	f_units,1,2,FALSE,
#if 1
"vdraw", "write|insert|delete|read|length|show [args]", "Expermental drawing (cnuzman)",
	cmd_vdraw, 2, 7, TRUE,
"read_center", "", "Experimental - return coords of view center",
	cmd_read_center, 1, 1, TRUE,
"read_scale", "", "Experimental - return coords of view scale",
	cmd_read_scale, 1, 1, TRUE,
#endif
"vrmgr", "host {master|slave|overview}", "link with Virtual Reality manager",
	f_vrmgr, 3, MAXARGS,FALSE,
"vrot", "xdeg ydeg zdeg", "rotate viewpoint",
	f_vrot,4,4,FALSE,
"vrot_center", "v|m x y z", "set center point of viewpoint rotation, in model or view coords",
	f_vrot_center, 5, 5,FALSE,
"whichid", "ident(s)", "lists all regions with given ident code",
	f_which_id, 2, MAXARGS,FALSE,
"winset", "pathname", "sets the window focus to the Tcl/Tk window with pathname",
        f_winset, 1, 2, FALSE,
"x", "lvl", "print solid table & vector list",
	f_debug, 1,2,FALSE,
"xpush", "object", "Experimental Push Command",
	f_xpush, 2,2,FALSE,
"Z", "", "zap all objects off screen",
	f_zap,1,1,FALSE,
"zoom", "scale_factor", "zoom view in or out",
	f_zoom, 2,2,FALSE,
0, 0, 0,
	0, 0, 0, 0
};


/*
 *                        O U T P U T _ C A T C H
 *
 * Gets the output from rt_log and appends it to clientdata vls.
 */

HIDDEN int
output_catch(clientdata, str)
genptr_t clientdata;
char *str;
{
    register struct rt_vls *vp = (struct rt_vls *)clientdata;
    register int len;

    len = rt_vls_strlen(vp);
    rt_vls_strcat(vp, str);
    len = rt_vls_strlen(vp) - len;

    return len;
}

/*
 *                 S T A R T _ C A T C H I N G _ O U T P U T
 *
 * Sets up hooks to rt_log so that all output is caught in the given vls.
 *
 */

void
start_catching_output(vp)
struct rt_vls *vp;
{
    rt_add_hook(output_catch, (genptr_t)vp);
}

/*
 *                 S T O P _ C A T C H I N G _ O U T P U T
 *
 * Turns off the output catch hook.
 */

void
stop_catching_output(vp)
struct rt_vls *vp;
{
    rt_delete_hook(output_catch, (genptr_t)vp);
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
    struct rt_vls result;
    int catch_output;

    argv[0] = ((struct funtab *)clientData)->ft_name;

    /* We now leave the world of Tcl where everything prints its results
       in the interp->result field.  Here, stuff gets printed with the
       rt_log command; hence, we must catch such output and stuff it into
       the result string.  Do this *only* if "output_as_return" global
       variable is set.  Make a local copy of this variable in case it's
       changed by our command. */

    catch_output = output_as_return;
    
    rt_vls_init(&result);

    if (catch_output)
	start_catching_output(&result);
    status = mged_cmd(argc, argv, funtab);
    if (catch_output)
	stop_catching_output(&result);

    /* Remove any trailing newlines. */

    if (catch_output) {
	len = rt_vls_strlen(&result);
	while (len > 0 && rt_vls_addr(&result)[len-1] == '\n')
	    rt_vls_trunc(&result, --len);
    }
    
    switch (status) {
    case CMD_OK:
	if (catch_output)
	    Tcl_SetResult(interp, rt_vls_addr(&result), TCL_VOLATILE);
	status = TCL_OK;
	break;
    case CMD_BAD:
	if (catch_output)
	    Tcl_SetResult(interp, rt_vls_addr(&result), TCL_VOLATILE);
	status = TCL_ERROR;
	break;
    case CMD_MORE:
	Tcl_SetResult(interp, MORE_ARGS_STR, TCL_STATIC);
	if (catch_output) {
	    Tcl_AppendResult(interp, rt_vls_addr(&result), (char *)NULL);
	}
	status = TCL_ERROR;
	break;
    default:
	Tcl_SetResult(interp, "error executing mged routine::", TCL_STATIC);
	if (catch_output) {
	    Tcl_AppendResult(interp, rt_vls_addr(&result), (char *)NULL);
	}
	status = TCL_ERROR;
	break;
    }
    
    rt_vls_free(&result);
    return status;
}

/*
 *                            G U I _ O U T P U T
 *
 * Used as a hook for rt_log output.  Sends output to the Tcl procedure whose
 * name is contained in the vls "tcl_output_hook".  Useful for user interface
 * building.
 */

int
gui_output(clientData, str)
genptr_t clientData;
char *str;
{
    Tcl_DString tclcommand;
    static int level = 0;

    if (level > 50) {
	rt_delete_hook(gui_output, clientData);
	/* Now safe to run rt_log? */
    	rt_log("Ack! Something horrible just happened recursively.\n");
	return 0;
    }

    Tcl_DStringInit(&tclcommand);
    (void)Tcl_DStringAppendElement(&tclcommand, rt_vls_addr(&tcl_output_hook));
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
    /* XXX Screen name should be same as attachment, or should ask, or
       should be settable from "-display" option. */

    if (tkwin != NULL)  {
	Tcl_SetResult(interp, "loadtk: already attached to display", TCL_STATIC);
	return TCL_ERROR;
    }

    if( argc != 2 )  {
	tkwin = Tk_CreateMainWindow(interp, (char *)NULL, "MGED", "MGED");
    } else {
	tkwin = Tk_CreateMainWindow(interp, argv[1], "MGED", "MGED");
    }
    if (tkwin == NULL)
	return TCL_ERROR;

    if (tkwin != NULL) {
	Tk_GeometryRequest(tkwin, 100, 20);

	/* This runs the tk.tcl script */
	if (Tk_Init(interp) == TCL_ERROR)
	    return TCL_ERROR;
    }
    
    /* Handle any delayed events which result */
    while (Tk_DoOneEvent(TK_DONT_WAIT | TK_ALL_EVENTS))
	;

    return TCL_OK;
}    

/*
 *	G E T K N O B
 *
 *	Procedure called by the Tcl/Tk interface code to find the values
 *	of the knobs/sliders.
 */

int
cmd_getknob(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
/*XXX I need to come back later and fix this */
    register int i;

    static struct {
	char *knobname;
	fastf_t *variable;
    } knobs[] = {
	"ax", (fastf_t *)NULL, 
	"ay", (fastf_t *)NULL,
	"az", (fastf_t *)NULL,
	"aX", (fastf_t *)NULL,
	"aY", (fastf_t *)NULL,
	"aZ", (fastf_t *)NULL,
	"aS", (fastf_t *)NULL,
	"x", (fastf_t *)NULL,
	"y", (fastf_t *)NULL,
	"z", (fastf_t *)NULL,
	"X", (fastf_t *)NULL,
	"Y", (fastf_t *)NULL,
	"Z", (fastf_t *)NULL,
	"S", (fastf_t *)NULL
    };

    knobs[0].variable = &absolute_rotate[X];
    knobs[1].variable = &absolute_rotate[Y];
    knobs[2].variable = &absolute_rotate[Z];
    knobs[3].variable = &absolute_slew[X];
    knobs[4].variable = &absolute_slew[Y];
    knobs[5].variable = &absolute_slew[Z];
    knobs[6].variable = &absolute_zoom;
    knobs[7].variable = &rate_rotate[X];
    knobs[8].variable = &rate_rotate[Y];
    knobs[9].variable = &rate_rotate[Z];
    knobs[10].variable = &rate_slew[X];
    knobs[11].variable = &rate_slew[Y];
    knobs[12].variable = &rate_slew[Z];
    knobs[13].variable = &rate_zoom;
	
    if( argc < 2 ) {
	Tcl_SetResult(interp, "getknob: need a knob name", TCL_STATIC);
	return TCL_ERROR;
    }

    for (i = 0; i < sizeof(knobs); i++)
	if (strcmp(knobs[i].knobname, argv[1]) == 0) {
	    sprintf(interp->result, "%lf", *(knobs[i].variable));
	    return TCL_OK;
	}
    
    Tcl_SetResult(interp, "getknob: invalid knob name", TCL_STATIC);
    return TCL_ERROR;
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
    struct rt_vls infocommand;
    int status;

    if (argc > 2) {
	Tcl_SetResult(interp,
		      "too many args: should be \"output_hook [hookName]\"",
		      TCL_STATIC);
	return TCL_ERROR;
    }

    rt_delete_hook(gui_output, (genptr_t)interp);/* Delete the existing hook */

    if (argc < 2)
	return TCL_OK;

    /* Make sure the command exists before putting in the hook! */
    
    rt_vls_init(&infocommand);
    rt_vls_strcat(&infocommand, "info commands ");
    rt_vls_strcat(&infocommand, argv[1]);
    status = Tcl_Eval(interp, rt_vls_addr(&infocommand));
    rt_vls_free(&infocommand);

    if (status != TCL_OK || interp->result[0] == '\0') {
	Tcl_SetResult(interp, "command does not exist", TCL_STATIC);
	return TCL_ERROR;
    }

    /* Also, don't allow silly infinite loops. */

    if (strcmp(argv[1], argv[0]) == 0) {
	Tcl_SetResult(interp, "Don't be silly.", TCL_STATIC);
	return TCL_ERROR;
    }

    /* Set up the hook! */

    rt_vls_strcpy(&tcl_output_hook, argv[1]);
    rt_add_hook(gui_output, (genptr_t)interp);
    
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


/* 			C M D _ S E T U P
 *
 * Sets up the Tcl interpreter and calls other setup functions.
 */ 

void
cmd_setup(interactive)
int interactive;
{
    register struct funtab *ftp;
    struct rt_vls temp;

    rt_vls_init(&temp);

    /* The following is for GUI output hooks: contains name of function to
       run with output */
    rt_vls_init(&tcl_output_hook);

    /* Create the interpreter */

    interp = Tcl_CreateInterp();

#if 0
    Tcl_SetVar(interp, "tcl_interactive", interactive ? "1" : "0",
	       TCL_GLOBAL_ONLY);
#endif

    /* This runs the init.tcl script */
    if( Tcl_Init(interp) == TCL_ERROR )
	rt_log("Tcl_Init error %s\n", interp->result);

    /* Finally, add in all the MGED commands.  Warn if they conflict */
    for (ftp = funtab+1; ftp->ft_name != NULL; ftp++) {
#if 0
	rt_vls_strcpy(&temp, "info commands ");
	rt_vls_strcat(&temp, ftp->ft_name);
	if (Tcl_Eval(interp, rt_vls_addr(&temp)) != TCL_OK ||
	    interp->result[0] != '\0') {
	    rt_log("WARNING:  '%s' name collision (%s)\n", ftp->ft_name,
		   interp->result);
	}
#endif
	rt_vls_strcpy(&temp, "_mged_");
	rt_vls_strcat(&temp, ftp->ft_name);
	
	if (ftp->tcl_converted) {
	    (void)Tcl_CreateCommand(interp, ftp->ft_name, ftp->ft_func,
				   (ClientData)ftp, (Tcl_CmdDeleteProc *)NULL);
	    (void)Tcl_CreateCommand(interp, rt_vls_addr(&temp), ftp->ft_func,
				   (ClientData)ftp, (Tcl_CmdDeleteProc *)NULL);
	} else {
	    (void)Tcl_CreateCommand(interp, ftp->ft_name, cmd_wrapper, 	    
			           (ClientData)ftp, (Tcl_CmdDeleteProc *)NULL);
	    (void)Tcl_CreateCommand(interp, rt_vls_addr(&temp), cmd_wrapper,
				   (ClientData)ftp, (Tcl_CmdDeleteProc *)NULL);
	}
    }

#if 1
    (void)Tcl_CreateCommand(interp, "sigint", cmd_sig2, (ClientData)NULL,
			    (Tcl_CmdDeleteProc *)NULL);
    (void)Tcl_CreateCommand(interp, "mged_glob", cmd_mged_glob, (ClientData)NULL,
			    (Tcl_CmdDeleteProc *)NULL);
    (void)Tcl_CreateCommand(interp, "cmd_init", cmd_init, (ClientData)NULL,
			    (Tcl_CmdDeleteProc *)NULL);
    (void)Tcl_CreateCommand(interp, "cmd_set", cmd_set, (ClientData)NULL,
			    (Tcl_CmdDeleteProc *)NULL);
#endif

#if 0
    /* Link to some internal variables */
    Tcl_LinkVar(interp, "mged_center_x", (char *)&toViewcenter[MDX],
		TCL_LINK_DOUBLE);
    Tcl_LinkVar(interp, "mged_center_y", (char *)&toViewcenter[MDY],
		TCL_LINK_DOUBLE);
    Tcl_LinkVar(interp, "mged_center_z", (char *)&toViewcenter[MDZ],
		TCL_LINK_DOUBLE);

#endif

    Tcl_LinkVar(interp, "glob_compat_mode", (char *)&glob_compat_mode,
		TCL_LINK_BOOLEAN);
    Tcl_LinkVar(interp, "output_as_return", (char *)&output_as_return,
		TCL_LINK_BOOLEAN);

	math_setup();

    rt_vls_free(&temp);
    tkwin = NULL;

    history_setup();
    mged_variable_setup(interp);
}


int
cmd_init(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  struct cmd_list *clp;

  if(argc != 2){
    Tcl_SetResult(interp, "Usage: cmd_init id", TCL_STATIC);
    return TCL_ERROR;
  }

  clp = (struct cmd_list *)rt_malloc(sizeof(struct cmd_list), "cmd_list");
  bzero((void *)clp, sizeof(struct cmd_list));
  RT_LIST_APPEND(&head_cmd_list.l, &clp->l);
  strcpy((char *)clp->name, argv[1]);
  clp->cur_hist = head_cmd_list.cur_hist;

  return TCL_OK;
}


int
cmd_set(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  struct cmd_list *p;

  if(argc != 2){
    Tcl_SetResult(interp, "Usage: cmd_set id", TCL_STATIC);
    return TCL_ERROR;
  }

  for( RT_LIST_FOR(p, cmd_list, &head_cmd_list.l) ){
    if(strcmp((char *)p->name, argv[1]))
      continue;

    curr_cmd_list = p;
    break;
  }

  if(p == &head_cmd_list)
    curr_cmd_list = &head_cmd_list;

  if(curr_cmd_list->aim)
    curr_dm_list = curr_cmd_list->aim;

  return TCL_OK;
}


int
cmd_sig2(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  sig2();
}


int
cmd_mged_glob(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  struct rt_vls dest, src;

  if(argc != 2){
    Tcl_SetResult(interp, "cmd_mged_glob: There must be only one argument.", TCL_STATIC);
    return TCL_ERROR;
  }

  rt_vls_init(&src);
  rt_vls_init(&dest);
  rt_vls_strcpy(&src, argv[1]);
  mged_compat( &dest, &src );
  Tcl_SetResult(interp, rt_vls_addr(&dest), TCL_VOLATILE);
  rt_vls_free(&src);
  rt_vls_free(&dest);

  return TCL_OK;
}


/*
 * debackslash, backslash_specials, mged_compat: routines for original
 *   mged emulation mode
 */

void
debackslash( dest, src )
struct rt_vls *dest, *src;
{
    char *ptr;

    ptr = rt_vls_addr(src);
    while( *ptr ) {
	if( *ptr == '\\' )
	    ++ptr;
	if( *ptr == '\0' )
	    break;
	rt_vls_putc( dest, *ptr++ );
    }
}

void
backslash_specials( dest, src )
struct rt_vls *dest, *src;
{
    int backslashed;
    char *ptr, buf[2];

    buf[1] = '\0';
    backslashed = 0;
    for( ptr = rt_vls_addr( src ); *ptr; ptr++ ) {
	if( *ptr == '[' && !backslashed )
	    rt_vls_strcat( dest, "\\[" );
	else if( *ptr == ']' && !backslashed )
	    rt_vls_strcat( dest, "\\]" );
	else if( backslashed ) {
	    rt_vls_strcat( dest, "\\" );
	    buf[0] = *ptr;
	    rt_vls_strcat( dest, buf );
	    backslashed = 0;
	} else if( *ptr == '\\' )
	    backslashed = 1;
	else {
	    buf[0] = *ptr;
	    rt_vls_strcat( dest, buf );
	}
    }
}

/*                    M G E D _ C O M P A T
 *
 * This routine is called to perform wildcard expansion and character quoting
 * on the given vls (typically input from the keyboard.)
 */

int
mged_compat( dest, src )
struct rt_vls *dest, *src;
{
    char *start, *end;          /* Start and ends of words */
    int regexp;                 /* Set to TRUE when word is a regexp */
    int backslashed;
    int firstword;
    struct rt_vls word;         /* Current word being processed */
    struct rt_vls temp;

    rt_vls_init( &word );
    rt_vls_init( &temp );
    
    start = end = rt_vls_addr( src );
    firstword = 1;
    while( *end != '\0' ) {            /* Run through entire string */

	/* First, pass along leading whitespace. */

	start = end;                   /* Begin where last word ended */
	while( *start != '\0' ) {
	    if( *start == ' '  ||
	        *start == '\t' ||
	        *start == '\n' )
		rt_vls_putc( dest, *start++ );
	    else
		break;
	}
	if( *start == '\0' )
	    break;

	/* Next, advance "end" pointer to the end of the word, while adding
	   each character to the "word" vls.  Also make a note of any
	   unbackslashed wildcard characters. */

	end = start;
	rt_vls_trunc( &word, 0 );
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
	    rt_vls_putc( &word, *end++ );
	}

	if( firstword )
	    regexp = 0;

	/* Now, if the word was suspected of being a wildcard, try to match
	   it to the database. */

	if( regexp ) {
	    rt_vls_trunc( &temp, 0 );
	    if( regexp_match_all(&temp, rt_vls_addr(&word)) == 0 ) {
		debackslash( &temp, &word );
		backslash_specials( dest, &temp );
	    } else
		rt_vls_vlscat( dest, &temp );
	} else {
	    debackslash( dest, &word );
	}

	firstword = 0;
    }

    rt_vls_free( &temp );
    rt_vls_free( &word );
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
struct rt_vls *vp;
int record;
{
    int	status;
    struct rt_vls globbed;
    struct timeval start, finish;
    size_t len;
    extern struct rt_vls mged_prompt;

    RT_VLS_CHECK(vp);

    if (rt_vls_strlen(vp) <= 0) return 0;
		
    rt_vls_init(&globbed);


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
	rt_vls_vlscat(&globbed, vp);

    gettimeofday(&start, (struct timezone *)NULL);
    status = Tcl_Eval(interp, rt_vls_addr(&globbed));
    gettimeofday(&finish, (struct timezone *)NULL);

    /* Contemplate the result reported by the Tcl interpreter. */

    switch (status) {
    case TCL_OK:
	len = strlen(interp->result);

    /* If the command had something to say, print it out. */	     

	if (len > 0) rt_log("%s%s", interp->result,
			    interp->result[len-1] == '\n' ? "" : "\n");

    /* Then record it in the history, if desired. */

	if (record) history_record(vp, &start, &finish, CMD_OK);

	rt_vls_free(&globbed);
	rt_vls_strcpy(&mged_prompt, MGED_PROMPT);
	return CMD_OK;

    case TCL_ERROR:
    default:

    /* First check to see if it's a secret message from cmd_wrapper. */

	if (strstr(interp->result, MORE_ARGS_STR) == interp->result) {
#if 0
	    rt_vls_strcpy(&mged_prompt,interp->result+sizeof(MORE_ARGS_STR)-1);
	    Tcl_SetResult(interp, rt_vls_addr(&mged_prompt), TCL_VOLATILE);
#else
	    rt_vls_trunc(&mged_prompt, 0);
	    rt_vls_printf(&mged_prompt, "\r%s",
			  interp->result+sizeof(MORE_ARGS_STR)-1);
	    Tcl_SetResult(interp, rt_vls_addr(&mged_prompt) + 1, TCL_VOLATILE);
#endif
	    rt_vls_free(&globbed);
	    return CMD_MORE;
	}

    /* Otherwise, it's just a regular old error. */    

	len = strlen(interp->result);
	if (len > 0) rt_log("%s%s", interp->result,
			    interp->result[len-1] == '\n' ? "" : "\n");

	if (record) history_record(vp, &start, &finish, CMD_BAD);
	rt_vls_free(&globbed);
	rt_vls_strcpy(&mged_prompt, MGED_PROMPT);
	return CMD_BAD;
    }
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
	functions = funtab;
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
		rt_log("mged_cmd(): Invalid return from %s\n",
		       ftp->ft_name);
		return CMD_BAD;
	    }
	}
	rt_log("Usage: %s%s %s\n\t(%s)\n",functions[0].ft_name,
	       ftp->ft_name, ftp->ft_parms, ftp->ft_comment);
	return CMD_BAD;
    }
    rt_log("%s%s: no such command, type '%s?' for help\n",
	   functions[0].ft_name, argv[0], functions[0].ft_name);
    return CMD_BAD;
}


/*
 *               C M D _ A P R O P O S
 *
 * Returns a list of commands whose descriptions contain the given keyword
 * contained in argv[1].  This version is case-sensitive.
 */

int
cmd_apropos(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    register struct funtab *ftp;
    char *keyword;

	if( argc < 2 )  {
		Tcl_SetResult(interp, "apropos: insufficient args", TCL_STATIC);
		return TCL_ERROR;
	}

    keyword = argv[1];
    if (keyword == NULL)
	keyword = "";
    
    for (ftp = funtab+1; ftp->ft_name != NULL; ftp++)
	if (strstr(ftp->ft_name, keyword) != NULL ||
	    strstr(ftp->ft_parms, keyword) != NULL ||
	    strstr(ftp->ft_comment, keyword) != NULL)
	    Tcl_AppendElement(interp, ftp->ft_name);
	
    return TCL_OK;
}


/* Let the user temporarily escape from the editor */
/* Format: %	*/

int
f_comm( argc, argv )
int	argc;
char	**argv;
{

	register int pid, rpid;
	int retcode;

	(void)signal( SIGINT, SIG_IGN );
	if ( ( pid = fork()) == 0 )  {
		(void)signal( SIGINT, SIG_DFL );
		(void)execl("/bin/sh","-",(char *)NULL);
		perror("/bin/sh");
		mged_finish( 11 );
	}

	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;
	(void)signal(SIGINT, cur_sigint);
	rt_log("!\n");

	return CMD_OK;  /* ? */
}

/* Quit and exit gracefully */
/* Format: q	*/

int
f_quit( argc, argv )
int	argc;
char	**argv;
{
	if( state != ST_VIEW )
		button( BE_REJECT );
	quit();			/* Exiting time */
	/* NOTREACHED */
}

/* wrapper for sync() */

int
f_sync(argc, argv)
int argc;
char **argv;
{

    register int i;
    sync();
    
    return CMD_OK;
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
			rt_log("Usage: %s%s %s\n\t(%s)\n", functions->ft_name,
			    ftp->ft_name, ftp->ft_parms, ftp->ft_comment);
			break;
		}
		if( !ftp->ft_name ) {
			rt_log("%s%s: no such command, type '%s?' for help\n",
			    functions->ft_name, argv[i], functions->ft_name);
			bad = 1;
		}
	}

	return bad ? CMD_BAD : CMD_OK;
}

/*
 *			F _ H E L P
 *
 *  Print a help message, two lines for each command.
 *  Or, help with the indicated commands.
 */
int f_help2();

int
f_help( argc, argv )
int	argc;
char	**argv;
{
#if 0
	/* There needs to be a better way to trigger this */
	if( argc <= 1 )  {
		/* User typed just "help" */
		system("Mosaic http://ftp.arl.mil/ftp/brl-cad/html/mged &");
	}
#endif
	return f_help2(argc, argv, &funtab[0]);
}

int
f_help2(argc, argv, functions)
int argc;
char **argv;
struct funtab *functions;
{
	register struct funtab *ftp;

	if( argc <= 1 )  {
		rt_log("The following commands are available:\n");
		for( ftp = functions+1; ftp->ft_name; ftp++ )  {
			rt_log("%s%s %s\n\t(%s)\n", functions->ft_name,
			    ftp->ft_name, ftp->ft_parms, ftp->ft_comment);
		}
		return CMD_OK;
	}
	return helpcomm( argc, argv, functions );
}

/*
 *			F _ F H E L P
 *
 *  Print a fast help message;  just tabulate the commands available.
 */
int
cmd_fhelp( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct funtab *ftp;
	struct rt_vls		str;

	rt_vls_init(&str);
	for( ftp = &funtab[1]; ftp->ft_name; ftp++ )  {
		vls_col_item( &str, ftp->ft_name);
	}
	vls_col_eol( &str );

	/* XXX should be rt_vls_strgrab() */
	Tcl_SetResult(interp, rt_vls_strdup( &str), TCL_DYNAMIC);

	rt_vls_free(&str);
	return TCL_OK;
}

int
f_fhelp2( argc, argv, functions)
int	argc;
char	**argv;
struct funtab *functions;
{
	register struct funtab *ftp;
	struct rt_vls		str;

	if( argc <= 1 )  {
		rt_vls_init(&str);
		rt_log("The following %scommands are available:\n",
		    functions->ft_name);
		for( ftp = functions+1; ftp->ft_name; ftp++ )  {
			vls_col_item( &str, ftp->ft_name);
		}
		vls_col_eol( &str );
		rt_log("%s", rt_vls_addr( &str ) );
		rt_vls_free(&str);
		return CMD_OK;
	}
	return helpcomm( argc, argv, functions );
}

/* Hook for displays with no buttons */
int
f_press( argc, argv )
int	argc;
char	**argv;
{
	register int i;

	for( i = 1; i < argc; i++ )
		press( argv[i] );

	return CMD_OK;
}

int
f_summary( argc, argv )
int	argc;
char	**argv;
{
	register char *cp;
	int flags = 0;
	int bad;

	bad = 0;
	if( argc <= 1 )  {
		dir_summary(0);
		return CMD_OK;
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
			rt_log("summary:  S R or G are only valid parmaters\n");
			bad = 1;
			break;
	}
	dir_summary(flags);
	return bad ? CMD_BAD : CMD_OK;
}

/*
 *                          C M D _ E C H O
 *
 * Concatenates its arguments and "rt_log"s the resulting string.
 */

int
cmd_echo(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	*argv[];
{
    register int i;

    for( i=1; i < argc; i++ )  {
	rt_log( i==1 ? "%s" : " %s", argv[i] );
    }
    rt_log("\n");

    return TCL_OK;
}

#if 0
int
f_button(argc, argv)
int	argc;
char	*argv[];
{
  int	save_journal;
  int result;

  if(button_hook){
    save_journal = journal;
    journal = 0;
    result = (*button_hook)(atoi(argv[1]));
    journal = save_journal;
    return result;
  }

  rt_log( "button: currently not available for this display manager\n");
  return CMD_BAD;
}

int
f_slider(argc, argv)
int	argc;
char	*argv[];
{
  int	save_journal;
  int result;

  if(slider_hook){
    save_journal = journal;
    journal = 0;
    result = (*slider_hook)(atoi(argv[1]), atoi(argv[2]));
    journal = save_journal;
    return result;
  }

  rt_log( "slider: currently not available for this display manager\n");
  return CMD_BAD;
}

int
f_savedit(argc, argv)
int	argc;
char	*argv[];
{
  struct rt_vls str;
  char	line[35];
  int o_ipathpos;
  register struct solid *o_illump;

  o_illump = illump;
  rt_vls_init(&str);

  if(state == ST_S_EDIT){
    rt_vls_strcpy( &str, "press accept\npress sill\n" );
    cmdline(&str, 0);
    illump = o_illump;
    rt_vls_strcpy( &str, "M 1 0 0\n");
    cmdline(&str, 0);
    return CMD_OK;
  }else if(state == ST_O_EDIT){
    o_ipathpos = ipathpos;
    rt_vls_strcpy( &str, "press accept\npress oill\n" );
    cmdline(&str, 0);
    (void)chg_state( ST_O_PICK, ST_O_PATH, "savedit");
    illump = o_illump;
    ipathpos = o_ipathpos;
    rt_vls_strcpy( &str, "M 1 0 0\n");
    cmdline(&str, 0);
    return CMD_OK;
  }

  rt_log( "Savedit will only work in an edit state\n");
  rt_vls_free(&str);
  return CMD_BAD;
}

int
f_openw(argc, argv)
int     argc;
char    *argv[];
{
  if(openw_hook){
  	if(argc == 1)
		return (*openw_hook)(NULL);
  	else
		return (*openw_hook)(argv[1]);
  }

  rt_log( "openw: currently not available\n");
  return CMD_BAD;
}

int
f_closew(argc, argv)
int     argc;
char    *argv[];
{
  if(closew_hook)
    return (*closew_hook)(argv[1]);

  rt_log( "closew: currently not available\n");
  return CMD_BAD;
}

static
int
f_ps(argc, argv)
int argc;
char *argv[];
{
  struct dm *o_dmp;
  void (*o_dotitles_hook)();
  int o_faceplate;
  static int windowbounds[] = {
    2047, -2048, 2047, -2048, 2047, -2048
  };

  o_faceplate = mged_variables.faceplate;
  if(argc == 3){
    if(*argv[1] == 'f'){
      mged_variables.faceplate = 1;
      ++argv;
    }else{
      rt_log( "Usage: ps filename [f]\n");
      return CMD_BAD;
    }
  }else
    mged_variables.faceplate = 0;

  if( (ps_fp = fopen( argv[1], "w" )) == NULL )  {
	perror(argv[1]);
	return CMD_BAD;
  }

  o_dotitles_hook = dotitles_hook;
  dotitles_hook = NULL;
  o_dmp = dmp;
  dmp = &dm_PS;
  dmp->dmr_window(windowbounds);
#if 0
  if(dmp->dmr_open())
	goto clean_up;
#else

	setbuf( ps_fp, ps_ttybuf );

	mged_fputs( "%!PS-Adobe-1.0\n\
%begin(plot)\n\
%%DocumentFonts:  Courier\n", ps_fp );
	fprintf(ps_fp, "%%%%Title: %s\n", argv[1] );
	mged_fputs( "\
%%Creator: MGED dm-ps.c\n\
%%BoundingBox: 0 0 324 324	% 4.5in square, for TeX\n\
%%EndComments\n\
\n", ps_fp );

	mged_fputs( "\
4 setlinewidth\n\
\n\
% Sizes, made functions to avoid scaling if not needed\n\
/FntH /Courier findfont 80 scalefont def\n\
/DFntL { /FntL /Courier findfont 73.4 scalefont def } def\n\
/DFntM { /FntM /Courier findfont 50.2 scalefont def } def\n\
/DFntS { /FntS /Courier findfont 44 scalefont def } def\n\
\n\
% line styles\n\
/NV { [] 0 setdash } def	% normal vectors\n\
/DV { [8] 0 setdash } def	% dotted vectors\n\
/DDV { [8 8 32 8] 0 setdash } def	% dot-dash vectors\n\
/SDV { [32 8] 0 setdash } def	% short-dash vectors\n\
/LDV { [64 8] 0 setdash } def	% long-dash vectors\n\
\n\
/NEWPG {\n\
	.0791 .0791 scale	% 0-4096 to 324 units (4.5 inches)\n\
} def\n\
\n\
FntH  setfont\n\
NEWPG\n\
", ps_fp);

	in_middle = 0;
#endif

  color_soltab();
  dmaflag = 1;
  refresh();
  dmp->dmr_close();
clean_up:
  dmp = o_dmp;
  dotitles_hook = o_dotitles_hook;
  mged_variables.faceplate = o_faceplate;
  return CMD_OK;
}
#endif

int
f_setview(argc, argv)
int     argc;
char    *argv[];
{
  double x, y, z;

  if(sscanf(argv[1], "%lf", &x) < 1){
    rt_log("f_setview: bad x value - %s\n", argv[1]);
    return CMD_BAD;
  }

  if(sscanf(argv[2], "%lf", &y) < 1){
    rt_log("f_setview: bad y value - %s\n", argv[2]);
    return CMD_BAD;
  }

  if(sscanf(argv[3], "%lf", &z) < 1){
    rt_log("f_setview: bad z value - %s\n", argv[3]);
    return CMD_BAD;
  }

  setview(x, y, z);

  return CMD_OK;
}

int
f_slewview(argc, argv)
int	argc;
char	*argv[];
{
  int x, y;
  vect_t tabvec;


  if(sscanf(argv[1], "%d", &x) < 1){
    rt_log("f_slewview: bad x value - %s\n", argv[1]);
    return CMD_BAD;
  }

  if(sscanf(argv[2], "%d", &y) < 1){
    rt_log("f_slewview: bad y value - %s\n", argv[2]);
    return CMD_BAD;
  }

  tabvec[X] =  x / 2047.0;
  tabvec[Y] =  y / 2047.0;
  tabvec[Z] = 0;
  slewview( tabvec );

  return CMD_OK;
}

void
set_e_axis_pos()
{
  int	i;

  tran_x = tran_y = tran_z = 0;
  rot_x = rot_y = rot_z = 0;

  update_views = 1;

  switch(es_int.idb_type){
  case	ID_ARB8:
  case	ID_ARBN:
    if(state == ST_O_EDIT)
      i = 0;
    else
      switch(es_edflag){
      case	STRANS:
	i = 0;
	break;
      case	EARB:
	switch(es_type){
	case	ARB5:
	  i = earb5[es_menu][0];
	  break;
	case	ARB6:
	  i = earb6[es_menu][0];
	  break;
	case	ARB7:
	  i = earb7[es_menu][0];
	  break;
	case	ARB8:
	  i = earb8[es_menu][0];
	  break;
	default:
	  i = 0;
	  break;
	}
	break;
      case	PTARB:
	switch(es_type){
	case    ARB4:
	  i = es_menu;	/* index for point 1,2,3 or 4 */
	  break;
	case    ARB5:
	case	ARB7:
	  i = 4;	/* index for point 5 */
	  break;
	case    ARB6:
	  i = es_menu;	/* index for point 5 or 6 */
	  break;
	default:
	  i = 0;
	  break;
	}
	break;
      case ECMD_ARB_MOVE_FACE:
	switch(es_type){
	case	ARB4:
	  i = arb_faces[0][es_menu * 4];
	  break;
	case	ARB5:
	  i = arb_faces[1][es_menu * 4];  		
	  break;
	case	ARB6:
	  i = arb_faces[2][es_menu * 4];  		
	  break;
	case	ARB7:
	  i = arb_faces[3][es_menu * 4];  		
	  break;
	case	ARB8:
	  i = arb_faces[4][es_menu * 4];  		
	  break;
	default:
	  i = 0;
	  break;
	}
	break;
      case ECMD_ARB_ROTATE_FACE:
	i = fixv;
	break;
      default:
	i = 0;
	break;
      }

    VMOVE(e_axis_pos, ((struct rt_arb_internal *)es_int.idb_ptr)->pt[i]);
    break;
  case ID_TGC:
  case ID_REC:
    if(es_edflag == ECMD_TGC_MV_H ||
       es_edflag == ECMD_TGC_MV_HH){
      struct rt_tgc_internal  *tgc = (struct rt_tgc_internal *)es_int.idb_ptr;

      VADD2(e_axis_pos, tgc->h, tgc->v);
      break;
    }
  default:
    VMOVE(e_axis_pos, es_keypoint);
    break;
  }
}

int
f_winset( argc, argv )
int     argc;
char    **argv;
{
  register struct dm_list *p;

  /* print pathname of drawing window with primary focus */
  if( argc == 1 ){
    rt_log( "%s\n", rt_vls_addr(&pathName) );
    return CMD_OK;
  }

  /* change primary focus to window argv[1] */
  for( RT_LIST_FOR(p, dm_list, &head_dm_list.l ) ){
    if( !strcmp( argv[1], rt_vls_addr( &p->_pathName ) ) ){
      curr_dm_list = p;
      return CMD_OK;
    }
  }

  rt_log( "Unrecognized pathname - %s\n", argv[1] );
  return CMD_BAD;
}

/*
 *                         S E T _ T R A N
 *
 * Calculate the values for tran_x, tran_y, and tran_z.
 *
 *
 */
void
set_tran(x, y, z)
fastf_t x, y, z;
{
  point_t diff;
  point_t old_pos;
  point_t new_pos;
  point_t view_pos;

  diff[X] = x - e_axis_pos[X];
  diff[Y] = y - e_axis_pos[Y];
  diff[Z] = z - e_axis_pos[Z];
  
  /* If there is more than one active view, then tran_x/y/z
     needs to be initialized for each view. */
  MAT_DELTAS_GET_NEG(old_pos, toViewcenter);
  VADD2(new_pos, old_pos, diff);
  MAT4X3PNT(view_pos, model2view, new_pos);

  tran_x = view_pos[X];
  tran_y = view_pos[Y];
  tran_z = view_pos[Z];
}


int
f_tran(argc, argv)
int     argc;
char    *argv[];
{
  double x, y, z;
  int itran;
  char cmd[128];
  vect_t view_pos;
  point_t old_pos;
  point_t new_pos;
  point_t diff;
  struct rt_vls str;

  rt_vls_init(&str);
  
  sscanf(argv[1], "%lf", &x);
  sscanf(argv[2], "%lf", &y);
  sscanf(argv[3], "%lf", &z);

  itran = !strcmp(argv[0], "itran");

  if(itran){
    tran_x += x;
    tran_y += y;
    tran_z += z;
  }else{
    tran_x = x;
    tran_y = y;
    tran_z = z;
  }

  VSET( view_pos, tran_x, tran_y, tran_z );
  MAT4X3PNT( new_pos, view2model, view_pos );
  MAT_DELTAS_GET_NEG( old_pos, toViewcenter );
  VSUB2( diff, new_pos, old_pos );

  if(state == ST_S_EDIT || state == ST_O_EDIT)
    sprintf(cmd, "M %d %d %d\n", 1, (int)(tran_x*2048.0), (int)(tran_y*2048.0));
  else{
    VADD2(new_pos, orig_pos, diff);
    MAT_DELTAS_VEC( toViewcenter, new_pos);
    MAT_DELTAS_VEC( ModelDelta, new_pos);
    new_mats();

    rt_vls_free(&str);
    return CMD_OK;
  }

  tran_set = 1;
  rt_vls_strcpy( &str, cmd );
  cmdline(&str, False);
  rt_vls_free(&str);
  tran_set = 0;

  return CMD_OK;
}

int
f_irot(argc, argv)
int argc;
char *argv[];
{
  double x, y, z;
  char cmd[128];
  struct rt_vls str;
  vect_t view_pos;
  point_t new_pos;
  point_t old_pos;
  point_t diff;

  rt_vls_init(&str);

  sscanf(argv[1], "%lf", &x);
  sscanf(argv[2], "%lf", &y);
  sscanf(argv[3], "%lf", &z);

  MAT_DELTAS_GET(old_pos, toViewcenter);

  if(state == ST_VIEW)
    sprintf(cmd, "vrot %f %f %f\n", x, y, z);
  else{
    rot_x += x;
    rot_y += y;
    rot_z += z;

    if(state == ST_O_EDIT)
      sprintf(cmd, "rotobj %f %f %f\n", rot_x, rot_y, rot_z);
    else if(state == ST_S_EDIT)
      sprintf(cmd, "p %f %f %f\n", rot_x, rot_y, rot_z);
  }

  rot_set = 1;
  rt_vls_strcpy( &str, cmd );
  cmdline(&str, False);
  rt_vls_free(&str);
  rot_set = 0;

  if(state == ST_VIEW){
    MAT_DELTAS_GET_NEG(new_pos, toViewcenter);
    VSUB2(diff, new_pos, orig_pos);
    VADD2(new_pos, old_pos, diff);
    VSET(view_pos, new_pos[X], new_pos[Y], new_pos[Z]);
    MAT4X3PNT( new_pos, model2view, view_pos);
    tran_x = new_pos[X];
    tran_y = new_pos[Y];
    tran_z = new_pos[Z];
  }

  return CMD_OK;
}
