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
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"

#ifdef MGED_TCL
#  define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */
#  include "/usr/brl/include/tcl.h"
#  include "/usr/brl/include/tk.h"
#endif

extern void	sync();

#define	MAXARGS		2000	/* Maximum number of args per line */
int	inpara;			/* parameter input from keyboard */

extern int	cmd_glob();

int	mged_cmd();
int	f_sync();
#ifdef MGED_TCL
int	f_gui();

Tcl_Interp *interp;
Tk_Window tkwin;
#endif

struct rt_vls history;

struct funtab {
	char *ft_name;
	char *ft_parms;
	char *ft_comment;
	int (*ft_func)();
	int ft_min;
	int ft_max;
};
static struct funtab funtab[] = {
"", "", "Primary command Table.",
	0, 0, 0,
"?", "", "summary of available commands",
	f_fhelp,0,MAXARGS,
"%", "", "escape to interactive shell",
	f_comm,1,1,
"3ptarb", "", "makes arb given 3 pts, 2 coord of 4th pt, and thickness",
	f_3ptarb, 1, 27,
"adc", "[<a1|a2|dst|dh|dv|hv|dx|dy|dz|xyz|reset|help> value(s)]",
	"control the angle/distance cursor",
        f_adc, 1, 5,
"ae", "azim elev", "set view using az and elev angles",
	f_aeview, 3, 3,
"analyze", "[arbname]", "analyze faces of ARB",
	f_analyze,1,MAXARGS,
"arb", "name rot fb", "make arb8, rotation + fallback",
	f_arbdef,4,4,
"area", "[endpoint_tolerance]", "calculate presented area of view",
	f_area, 1, 2,
"attach", "[device]", "attach to a display processor, or NU",
	f_attach,1,2,
"B", "<objects>", "clear screen, edit objects",
	f_blast,2,MAXARGS,
"cat", "<objects>", "list attributes (brief)",
	f_cat,2,MAXARGS,
"center", "x y z", "set view center",
	f_center, 4,4,
"color", "low high r g b str", "make color entry",
	f_color, 7, 7,
"comb", "comb_name <operation solid>", "create or extend combination w/booleans",
	f_comb,4,MAXARGS,
#ifdef MGED_TCL
"mconcat", "file [prefix]", "concatenate 'file' onto end of present database.  Run 'dup file' first.",
	f_concat, 2, 3,
#else
"concat", "file [prefix]", "concatenate 'file' onto end of present database.  Run 'dup file' first.",
	f_concat, 2, 3,
#endif
"copyeval", "new_solid path_to_old_solid (seperate path components with spaces, not /)",
	"copy an 'evaluated' path solid",
	f_copyeval, 1, 27,
"cp", "from to", "copy [duplicate] object",
	f_copy,3,3,
"cpi", "from to", "copy cylinder and position at end of original cylinder",
	f_copy_inv,3,3,
"d", "<objects>", "delete list of objects",
	f_delobj,2,MAXARGS,
"debugdir", "", "Print in-memory directory, for debugging",
	f_debugdir, 1, 1,
"debuglib", "[hex_code]", "Show/set debugging bit vector for librt",
	f_debuglib,1,2,
"debugmem", "", "Print librt memory use map",
	f_debugmem, 1, 1,
"debugnmg", "[hex code]", "Show/set debugging bit vector for NMG",
	f_debugnmg,1,2,
"dm", "set var=val", "Do display-manager specific command",
	f_dm, 2, MAXARGS,
"dup", "file [prefix]", "check for dup names in 'file'",
	f_dup, 2, 3,
"E", "<objects>", "evaluated edit of objects",
	f_evedit,2,MAXARGS,
"e", "<objects>", "edit objects",
	f_edit,2,MAXARGS,
"echo", "[text]", "echo arguments back",
	f_echo, 1, MAXARGS,
"edcodes", "object(s)", "edit region ident codes",
	f_edcodes, 2, MAXARGS,
"edcolor", "", "text edit color table",
	f_edcolor, 1, 1,
"edcomb", "combname Regionflag regionid air los [GIFTmater]", "edit combination record info",
	f_edcomb,6,7,
"edgedir", "[delta_x delta_y delta_z]|[rot fb]", "define direction of ARB edge being moved",
	f_edgedir, 3, 4,
"ev",	"[-w] [-n] [-P#] [-T] <objects>", "evaluate objects via NMG tessellation",
	f_ev, 2, MAXARGS,
"eqn", "A B C", "planar equation coefficients",
	f_eqn, 4, 4,
"extrude", "#### distance", "extrude dist from face",
	f_extrude,3,3,
"facedef", "####", "define new face for an arb",
	f_facedef, 2, MAXARGS,
"facetize", "[-t] [-P#] new_obj old_obj(s)", "convert objects to faceted NMG objects at current tol",
	f_facetize, 3, MAXARGS,
"find", "<objects>", "find all references to objects",
	f_find, 1, MAXARGS,
"fix", "", "fix display after hardware error",
	f_fix,1,1,
"fracture", "NMGsolid [prefix]", "fracture an NMG solid into many NMG solids, each containing one face\n",
	f_fracture, 2, 3,
"g", "groupname <objects>", "group objects",
	f_group,3,MAXARGS,
#ifdef MGED_TCL
"gui",	"", "Bring up a Tcl/Tk Graphical User Interface",
	f_gui, 1, 1,
#endif
#ifdef HIDELINE
"H", "plotfile [step_size %epsilon]", "produce hidden-line unix-plot",
	f_hideline,2,4,
#endif
"help", "[commands]", "give usage message for given commands",
	f_help,0,MAXARGS,
"i", "obj combination [operation]", "add instance of obj to comb",
	f_instance,3,4,
"idents", "file object(s)", "make ascii summary of region idents",
	f_tables, 3, MAXARGS,
"ill", "name", "illuminate object",
	f_ill,2,2,
"in", "[-f] [-s] parameters...", "keyboard entry of solids.  -f for no drawing, -s to enter solid edit",
	f_in, 1, MAXARGS,
"inside", "", "finds inside solid per specified thicknesses",
	f_inside, 1, MAXARGS,
"item", "region item [air]", "change item # or air code",
	f_itemair,3,4,
"joint", "command [options]", "articualtion/animation commands",
	f_joint, 1, MAXARGS,
"keep", "keep_file object(s)", "save named objects in specified file",
	f_keep, 3, MAXARGS,
"keypoint", "[x y z | reset]", "set/see center of editing transformations",
	f_keypoint,1,4,
"kill", "[-f] <objects>", "delete object[s] from file",
	f_kill,2,MAXARGS,
"killall", "<objects>", "kill object[s] and all references",
	f_killall, 2, MAXARGS,
"killtree", "<object>", "kill complete tree[s] - BE CAREFUL",
	f_killtree, 2, MAXARGS,
"knob", "id [val]", "emulate knob twist",
	f_knob,2,3,
"l", "<objects>", "list attributes (verbose)",
	f_list,2,MAXARGS,
"labelvert", "object[s]", "label vertices of wireframes of objects",
	f_labelvert, 2, MAXARGS,
"listeval", "", "lists 'evaluated' path solids",
	f_pathsum, 1, MAXARGS,
"ls", "", "table of contents",
	dir_print,1,MAXARGS,
"M", "1|0 xpos ypos", "handle a mouse event",
	f_mouse, 4,4,
"make", "name <arb8|sph|ellg|tor|tgc|rpc|rhc|epa|ehy|eto|part|grip|half|nmg>", "create a primitive",
	f_make,3,3,
"mater", "comb [material]", "assign/delete material to combination",
	f_mater,2,3,
"memprint", "", "print memory maps",
	f_memprint, 1, 1,
"mirface", "#### axis", "mirror an ARB face",
	f_mirface,3,3,
"mirror", "old new axis", "Arb mirror ??",
	f_mirror,4,4,
"mv", "old new", "rename object",
	f_name,3,3,
"mvall", "oldname newname", "rename object everywhere",
	f_mvall, 3, 3,
"nirt", "", "trace a single ray from current view",
	f_nirt,1,MAXARGS,
"opendb", "database.g", "Close current .g file, and open new .g file",
	f_opendb, 2, 2,
"orientation", "x y z w", "Set view direction from quaternion",
	f_orientation, 5, 5,
"orot", "xdeg ydeg zdeg", "rotate object being edited",
	f_rot_obj, 4, 4,
"overlay", "file.plot [name]", "Read UNIX-Plot as named overlay",
	f_overlay, 2, 3,
"p", "dx [dy dz]", "set parameters",
	f_param,2,4,
"paths", "pattern", "lists all paths matching input path",
	f_pathsum, 1, MAXARGS,
"permute", "tuple", "permute vertices of an ARB",
	f_permute,2,2,
"plot", "[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]", "make UNIX-plot of view",
	f_plot, 2, MAXARGS,
"polybinout", "file", "store vlist polygons into polygon file (experimental)",
	f_polybinout, 2, 2,
"pov", "args", "experimental:  set point-of-view",
	f_pov, 3+4+1, MAXARGS,
"prcolor", "", "print color&material table",
	f_prcolor, 1, 1,
"prefix", "new_prefix object(s)", "prefix each occurrence of object name(s)",
	f_prefix, 3, MAXARGS,
"preview", "[-v] [-d sec_delay] rt_script_file", "preview new style RT animation script",
	f_preview, 2, MAXARGS,
"press", "button_label", "emulate button press",
	f_press,2,MAXARGS,
"push", "object[s]", "pushes object's path transformations to solids",
	f_push, 2, MAXARGS,
"q", "", "quit",
	f_quit,1,1,
"quit", "", "quit",
	f_quit,1,1,
"qorot", "x y z dx dy dz theta", "rotate object being edited about specified vector",
	f_qorot, 8, 8,
"qvrot", "dx dy dz theta", "set view from direction vector and twist angle",
	f_qvrot, 5, 5,
"r", "region <operation solid>", "create or extend a Region combination",
	f_region,4,MAXARGS,
"red", "object", "edit a group or region using a text editor",
	f_red, 2, 2,
"refresh", "", "send new control list",
	f_refresh, 1,1,
"regdebug", "", "toggle register print",
	f_regdebug, 1,2,
"regdef", "item [air] [los] [GIFTmaterial]", "change next region default codes",
	f_regdef, 2, 5,
"regions", "file object(s)", "make ascii summary of regions",
	f_tables, 3, MAXARGS,
"release", "", "release current display processor [attach NU]",
	f_release,1,1,
"rfarb", "", "makes arb given point, 2 coord of 3 pts, rot, fb, thickness",
	f_rfarb, 1, 27,
"rm", "comb <members>", "remove members from comb",
	f_rm,3,MAXARGS,
"rmats", "file", "load views from file (experimental)",
	f_rmats,2,MAXARGS,
"rotobj", "xdeg ydeg zdeg", "rotate object being edited",
	f_rot_obj, 4, 4,
"rrt", "prog [options]", "invoke prog with view",
	f_rrt,2,MAXARGS,
"rt", "[options]", "do raytrace of view",
	f_rt,1,MAXARGS,
"rtcheck", "[options]", "check for overlaps in current view",
	f_rtcheck,1,MAXARGS,
"savekey", "file [time]", "save keyframe in file (experimental)",
	f_savekey,2,MAXARGS,
"saveview", "file [args]", "save view in file for RT",
	f_saveview,2,MAXARGS,
#ifdef MGED_TCL
"mscale", "factor", "scale object by factor",
	f_sc_obj,2,2,
#else
"scale", "factor", "scale object by factor",
	f_sc_obj,2,2,
#endif
"sed", "solid", "solid-edit named solid",
	f_sed,2,2,
#ifdef MGED_TCL
"mset",	"[var=opt]", "assign/display mged variables",
	f_set,1,2,
#else
"set",	"[var=opt]", "assign/display mged variables",
	f_set,1,2,
#endif
"shader", "comb material [arg(s)]", "assign materials (like 'mater')",
	f_shader, 3,MAXARGS,
"size", "size", "set view size",
	f_view, 2,2,
"solids", "file object(s)", "make ascii summary of solid parameters",
	f_tables, 3, MAXARGS,
#ifndef MGED_TCL
"source", "file/pipe", "read and process file/pipe of commands",
	f_source, 2,MAXARGS,
#endif
"status", "", "get view status",
	f_status, 1,1,
"summary", "[s r g]", "count/list solid/reg/groups",
	f_summary,1,2,
"sync",	"",	"forces UNIX sync",
	f_sync, 1, 1,
"t", "", "table of contents",
	dir_print,1,MAXARGS,
"tab", "object[s]", "tabulates objects as stored in database",
	f_tabobj, 2, MAXARGS,
"ted", "", "text edit a solid's parameters",
	f_tedit,1,1,
"title", "string", "change the title",
	f_title,1,MAXARGS,
"tol", "[abs #]|[rel #]", "show/set absolute or relative tolerance for tessellation",
	f_tol, 1, 3,
"tops", "", "find all top level objects",
	f_tops,1,1,
"track", "<parameters>", "adds tracks to database",
	f_amtrack, 1, 27,
"translate", "x y z", "trans object to x,y, z",
	f_tr_obj,4,4,
"tree",	"object(s)", "print out a tree of all members of an object",
	f_tree, 2, MAXARGS,
"units", "[mm|cm|m|in|ft|...]", "change units",
	f_units,1,2,
"vrmgr", "host {master|slave|overview}", "link with Virtual Reality manager",
	f_vrmgr, 3, MAXARGS,
"vrot", "xdeg ydeg zdeg", "rotate viewpoint",
	f_vrot,4,4,
"vrot_center", "v|m x y z", "set center point of viewpoint rotation, in model or view coords",
	f_vrot_center, 5, 5,
"whichid", "ident(s)", "lists all regions with given ident code",
	f_which_id, 2, MAXARGS,
"x", "lvl", "print solid table & vector list",
	f_debug, 1,2,
"Z", "", "zap all objects off screen",
	f_zap,1,1,
"zoom", "scale_factor", "zoom view in or out",
	f_zoom, 2,2,
0, 0, 0,
	0, 0, 0,
};

#ifdef MGED_TCL

/*			C M D _ W R A P P E R
 *
 */

int
cmd_wrapper(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	switch (mged_cmd(argc, argv, funtab)) {
	case CMD_OK:
		interp->result = "MGED_Ok";
		return TCL_OK;
	case CMD_BAD:
		interp->result = "MGED_Error";
		return TCL_ERROR;
	case CMD_MORE:
		interp->result = "MGED_More";
		return TCL_ERROR;
	default:
		interp->result = "MGED_Unknown";
		return TCL_ERROR;
	}
}

void
mged_tk_hook()
{
	while( Tk_DoOneEvent( TK_DONT_WAIT | TK_ALL_EVENTS ) )  /*NIL*/;
}

void
mged_tk_idle(non_blocking)
int	non_blocking;
{
	int	flags;

	flags = TK_DONT_WAIT | TK_ALL_EVENTS;

	while( Tk_DoOneEvent( flags ) )  {
		/* NIL */	;
	}
}



/*
 *	G U I _ O U T P U T
 *
 *	Used as a hook for rt_log output.  Redirects output to the Tcl/Tk
 *		MGED Interaction window.
 */

int
gui_output( str )
char *str;
{
	char buf[10240];
	int ret;
	char *old;

	old = interp->result; 

	ret = sprintf(buf, ".i.f.text insert insert \"%s\"", str);
	Tcl_Eval(interp, buf);
	Tcl_Eval(interp, ".i.f.text yview -pickplace insert");
	Tcl_Eval(interp, "set printend [.i.f.text index insert]");

	interp->result = old;

	return ret;
}


/*
 *	G U I _ C M D L I N E
 *
 *	Called from the Tcl/Tk section to process commands from
 *	the MGED Interaction window.
 */

int
gui_cmdline( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	static struct rt_vls argstr;
	static int done = 0;

	if( done == 0 ) {
		rt_vls_init( &argstr );
		done = 1;
		rt_vls_strcpy( &argstr, "" );
	}

	if( strlen(rt_vls_addr(&argstr)) > 0 ) {
		/* Remove newline */
		rt_vls_trunc( &argstr, strlen(rt_vls_addr(&argstr))-1 );
		rt_vls_strcat( &argstr, " " );
	}

	rt_vls_strcat( &argstr, argv[1] );

	switch ( Tcl_Eval(interp, rt_vls_addr(&argstr)) ) {
	case TCL_OK:
		if( strcmp(interp->result, "MGED_Ok") != 0 )
			rt_log( interp->result );
			    /* If the command was more than just \n, record. */
		if( strlen(rt_vls_addr(&argstr)) > 1 )
			rt_vls_vlscat( &history, &argstr );
		rt_vls_trunc( &argstr, 0 );
		pr_prompt();
		return TCL_OK;
	case TCL_ERROR:
		if( strcmp(interp->result, "MGED_Error") == 0 ) {
			rt_vls_strcat( &history, "# " );
			rt_vls_vlscat( &history, &argstr );
			rt_vls_trunc( &argstr, 0 );
			pr_prompt();
			return TCL_OK;
		} else if( strcmp(interp->result, "MGED_More") == 0 ) {
			return TCL_OK;
		} else {
			char *tmp;
			tmp = (char *)malloc( strlen(interp->result)+1 );
			strcpy( tmp, interp->result );
			rt_log( "%s\n", tmp );
			rt_vls_strcat( &history, "# " );
			rt_vls_vlscat( &history, &argstr );
			rt_vls_trunc( &argstr, 0 );
			pr_prompt();
			interp->result = tmp;
			interp->freeProc = (void (*)(char *))free;
			return TCL_ERROR;
		}
	default:
		interp->result = "MGED: Unknown Error.";
		return TCL_ERROR;
	}
}


/*
 *	S A V E _ H I S T O R Y
 *
 *	Saves the history to the given filename.
 */

int
save_history( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	FILE *fp;

	if( argc < 2 ) {
		rt_log("save_history: Need a filename\n");
		interp->result = "save_history: Need a filename";
		return TCL_ERROR;
	}

	fp = fopen( argv[1], "a+" );
	if( fp == NULL ) {
		rt_log("save_history: Error opening file\n");
		interp->result = "save_history: Error opening file";
		return TCL_ERROR;
	}

	fprintf( fp, "%s", rt_vls_addr(&history) );	

	fclose( fp );
	return TCL_OK;
}

/*
 *	C M D _ P R E V
 */

int
cmd_prev( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	;
}

/*
 *	F _ G U I
 *
 *	Puts the necessary hooks in to interface with the MGED Interaction
 *		window.
 */

int
f_gui( argc, argv ) 
int argc;
char **argv;
{
	rt_log("Please direct your attention to the mged interaction window.\n");
	rt_add_hook( gui_output );
	Tcl_CreateCommand(interp, "cmdline", gui_cmdline, (ClientData)NULL,
			  (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "save_history", save_history,
			  (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	return CMD_OK;
}

#endif

/* 			C M D _ S E T U P
 *
#ifdef MGED_TCL
 * Sets up the TCL interpreter.
#endif
 */ 

void
cmd_setup(interactive)
int	interactive;
{
        register struct funtab *ftp;
	char	buf[512];

	rt_vls_init( &history );
	rt_vls_strcpy( &history, "" );

#ifdef MGED_TCL

	interp = Tcl_CreateInterp();

	Tcl_CreateCommand(interp, "exit", cmd_wrapper, (ClientData)NULL,
			(Tcl_CmdDeleteProc *)NULL);

	Tcl_SetVar(interp, "tcl_interactive",
		interactive ? "1" : "0", TCL_GLOBAL_ONLY);

	/* This runs the init.tcl script */
	if( Tcl_Init(interp) == TCL_ERROR )
	rt_log("Tcl_Init error %s\n", interp->result);

	/* Screen name should be same as attachment */
	/* This binds in Tk commands.  Do AFTER binding MGED commands */
	tkwin = Tk_CreateMainWindow(interp, NULL, "TkMGED", "tkMGED");
	if (tkwin == NULL)
	rt_log("Error creating Tk window: %s\n", interp->result);

	if (tkwin != NULL) {
		/* XXX HACK! */
		extern void (*extrapoll_hook)();	/* ged.c */
		extern int  extrapoll_fd;		/* ged.c */

		extrapoll_hook = mged_tk_hook;
		extrapoll_fd = Tk_Display(tkwin)->fd;

		Tk_GeometryRequest(tkwin, 200, 200);
#if 0
		Tk_MakeWindowExist(tkwin);
		Tk_MapWindow(tkwin);
#endif
		/* This runs the tk.tcl script */
		if( Tk_Init(interp) == TCL_ERROR )
		rt_log("Tk_init error %s\n", interp->result);
	}

	/* Finally, add in all the MGED commands, if they don't conflict */
        for (ftp = funtab+1; ftp->ft_name != NULL; ftp++)  {
        	rt_log(buf, "info commands %s", ftp->ft_name);
        	if( Tcl_Eval(interp, buf) != TCL_OK ||
		    interp->result[0] != '\0' )  {
        	rt_log("WARNING:  '%s' name collision (%s)\n", ftp->ft_name, interp->result);
        		continue;
        	}
		Tcl_CreateCommand(interp, ftp->ft_name, cmd_wrapper, 
			(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
        }

	/* Handle any delayed events which result */
	while( Tk_DoOneEvent( TK_DONT_WAIT | TK_ALL_EVENTS ) )  /*NIL*/;

	/* Link to some internal variables */
	Tcl_LinkVar(interp, "mged_center_x", (char *)&toViewcenter[MDX], TCL_LINK_DOUBLE );
	Tcl_LinkVar(interp, "mged_center_y", (char *)&toViewcenter[MDY], TCL_LINK_DOUBLE );
	Tcl_LinkVar(interp, "mged_center_z", (char *)&toViewcenter[MDZ], TCL_LINK_DOUBLE );
#endif
}

/* wrapper for sync() */

int
f_sync(argc, argv)
int argc;
char **argv;
{
	sync();
	return CMD_OK;
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
cmdline(vp)
struct rt_vls	*vp;
{
	int	i;
	int	need_prompt = 0;
	int	len;
	register char	*cp;
	char		*ep;
	char		*end;
	struct rt_vls	cmd;
	struct rt_vls	cmd_buf;
	struct rt_vls	str;
	int  	result;
	int	argc;
	char 	*argv[MAXARGS+2];

	RT_VLS_CHECK(vp);

	if( (len = rt_vls_strlen( vp )) <= 0 )  return 0;
		
	cp = rt_vls_addr( vp );
	end = cp + len;

	rt_vls_init( &cmd );
	rt_vls_init( &cmd_buf );
	rt_vls_init( &str );

	while( cp < end )  {
		ep = strchr( cp, '\n' );
		if( ep == NULL )  break;

		/* Copy one cmd, incl newline.  Null terminate */
		rt_vls_strncpy( &cmd, cp, ep-cp+1 );
		/* parse_line insists on it ending with newline&null */
#ifndef MGED_TCL		
		rt_vls_strcpy( &cmd_buf, rt_vls_addr(&cmd) );
		i = parse_line( rt_vls_addr(&cmd_buf), &argc, argv );
#endif

		while (1) {
#ifdef MGED_TCL
			result = Tcl_Eval(interp, rt_vls_addr(&cmd));
			if (result == TCL_OK) {
				if (strcmp(interp->result, "MGED_Ok") != 0
					&& interp->result[0] != '\0') {
					/* Some TCL value to display */
					rt_log("%s\n", interp->result);
				}
				break;  /* XXX Record, later */
			}
			if (result == TCL_ERROR && 
			    strcmp(interp->result, "MGED_Error") != 0 &&
			    strcmp(interp->result, "MGED_More") != 0) {
				rt_log("%s\n", interp->result);
				break;
			}
			if (strcmp(interp->result, "MGED_Error") == 0)  {
				/* Silent error (we already printed out the
				   error message) */
				break;
			}
			if (strcmp(interp->result, "MGED_More") != 0) {
				/* Some TCL error of some sort. */
			rt_log("%s\n", interp->result);
				break;
			}
#else
			if (mged_cmd(argc, argv, funtab) != CMD_MORE)
				break;
#endif

			/* If we get here, it means the command failed due
			   to insufficient arguments.  In this case, grab some
			   more from stdin and call the command again. */

			rt_vls_gets( &str, stdin );

			/* Remove newline */
			rt_vls_trunc( &cmd, strlen(rt_vls_addr(&cmd))-1 );

			rt_vls_strcat( &cmd, " " );
			rt_vls_vlscatzap( &cmd, &str );
			rt_vls_strcat( &cmd, "\n" );
#ifndef MGED_TCL
			rt_vls_strcpy( &cmd_buf, rt_vls_addr(&cmd) );
			i = parse_line( rt_vls_addr(&cmd_buf), &argc, argv );
#endif
			rt_vls_free( &str );
		}
		if( i < 0 )  continue;	/* some kind of error */
		need_prompt = 1;

		cp = ep+1;
		rt_vls_free( &cmd );
		rt_vls_free( &cmd_buf );
	}
	rt_vls_free( &cmd );
	return need_prompt;
}

/*
 *			P A R S E _ L I N E
 *
 * Parse commandline into argument vector
 * Returns nonzero value if input is to be ignored
 * Returns less than zero if there is no input to read.
 */

int
parse_line(line, argcp, argv)
char	*line;
int	*argcp;
char   **argv;
{
	register char *lp;
	register char *lp1;

	(*argcp) = 0;
	lp = &line[0];

	/* Delete leading white space */
	while( (*lp == ' ') || (*lp == '\t'))
		lp++;

	argv[0] = lp;

	if( *lp == '\n' )
		return(1);		/* NOP */

	if( *lp == '#' )
		return 1;		/* NOP -- a comment line */

	/* Handle "!" shell escape char so the shell can parse the line */
	if( *lp == '!' )  {
		(void)system( ++lp);
		rt_log("!\n");
		return(1);		/* Don't process command line! */
	}

	/*  Starting with the first non-whitespace, search ahead for the
	 *  first whitespace (or newline) at the end of each command
	 *  element and turn it into a null.  Then while there is more
	 *  turn it into nulls.  Once the next string is spotted (or
	 *  the of the command line) glob it if necessary and prepare
	 *  for the next command element.
	 */
	for( ; *lp != '\0'; lp++ )  {
		if((*lp == ' ') || (*lp == '\t') || (*lp == '\n'))  {
			*lp = '\0';
			lp1 = lp + 1;
			if((*lp1 == ' ') || (*lp1 == '\t') || (*lp1 == '\n'))
				continue;
			/* If not cmd [0], check for regular exp */
			if( *argcp > 0 )
				(void)cmd_glob(argcp, argv, MAXARGS);
			if( (*argcp)++ >= MAXARGS )  {
				rt_log("More than %d arguments, excess flushed\n", MAXARGS);
				argv[MAXARGS] = (char *)0;
				return(0);
			}
			argv[*argcp] = lp1;
		}
		/* Finally, a non-space char */
	}
	/* Null terminate pointer array */
	argv[*argcp] = (char *)0;
	return(0);
}

/*
 *			M G E D _ C M D
 *
 *  Check a table for the command, check for the correct
 *  minimum and maximum number of arguments, and pass control
 *  to the proper function.  If the number of arguments is
 *  incorrect, print out a short help message.
 */
int
mged_cmd( argc, argv, functions )
int	argc;
char	**argv;
struct funtab *functions;
{
	register struct funtab *ftp;
	struct rt_vls str;

	if( argc == 0 )  {
		rt_log("no command entered, type '%s?' for help\n",
		    functions->ft_name);
		return CMD_BAD;
	}

	for( ftp = functions+1; ftp->ft_name ; ftp++ )  {
		if( strcmp( ftp->ft_name, argv[0] ) != 0 )
			continue;
		/* We have a match */
		if( (ftp->ft_min <= argc) &&
		    (argc <= ftp->ft_max) )  {
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
		rt_log("Usage: %s%s %s\n\t(%s)\n",functions->ft_name,
		    ftp->ft_name, ftp->ft_parms, ftp->ft_comment);
		return CMD_BAD;
	}
	rt_log("%s%s: no such command, type '%s?' for help\n",
	    functions->ft_name, argv[0], functions->ft_name);
	return CMD_BAD;
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
	register int	i;

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
 *  Or, help with the indicated commands.
 */
int f_fhelp2();

int
f_fhelp( argc, argv )
int	argc;
char	**argv;
{
	return f_fhelp2(argc, argv, &funtab[0]);
}

int
f_fhelp2( argc, argv, functions)
int	argc;
char	**argv;
struct funtab *functions;
{
	register struct funtab *ftp;

	if( argc <= 1 )  {
		rt_log("The following %scommands are available:\n",
		    functions->ft_name);
		for( ftp = functions+1; ftp->ft_name; ftp++ )  {
			col_item(ftp->ft_name);
		}
		col_eol();
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
 *			S O U R C E _ F I L E
 *
 */
void
mged_source_file(fp)
register FILE	*fp;
{
	struct rt_vls	str;
	int		len;

	rt_vls_init(&str);

	while( (len = rt_vls_gets( &str, fp )) >= 0 )  {
		rt_vls_strcat( &str, "\n" );
		if( len > 0 )  (void)cmdline( &str );
		rt_vls_trunc( &str, 0 );
	}

	rt_vls_free(&str);
}

int
f_echo( argc, argv )
int	argc;
char	*argv[];
{
	register int i;

	for( i=1; i < argc; i++ )  {
		rt_log( i==1 ? "%s" : " %s", argv[i] );
	}
	rt_log("\n");

	return CMD_OK;
}

#ifdef MGED_TCL

/* TCL */
/* XXX Slightly hacked version */
/* 
 * tkEvent.c --
 *
 *	This file provides basic event-managing facilities,
 *	whereby procedure callbacks may be attached to
 *	certain events.
 *
 * Copyright (c) 1990-1993 The Regents of the University of California.
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 * 
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#ifndef lint
static char rcsid[] = "$Header$ SPRITE (Berkeley)";
#endif

#include "/m/tcl/tk3.6/tkConfig.h"
#include "/m/tcl/tk3.6/tkInt.h"
#include <errno.h>
#include <signal.h>

/*
 * For each timer callback that's pending, there is one record
 * of the following type, chained together in a list sorted by
 * time (earliest event first).
 */

typedef struct TimerEvent {
    struct timeval time;	/* When timer is to fire. */
    void (*proc)  _ANSI_ARGS_((ClientData clientData));
				/* Procedure to call. */
    ClientData clientData;	/* Argument to pass to proc. */
    Tk_TimerToken token;	/* Identifies event so it can be
				 * deleted. */
    struct TimerEvent *nextPtr;	/* Next event in queue, or NULL for
				 * end of queue. */
} TimerEvent;

static TimerEvent *timerQueue;	/* First event in queue. */

/*
 * The information below is used to provide read, write, and
 * exception masks to select during calls to Tk_DoOneEvent.
 */

static int readCount;		/* Number of files for which we */
static int writeCount;		/* care about each event type. */
static int exceptCount;
static fd_mask masks[3*MASK_SIZE];
				/* Integer array containing official
				 * copies of the three sets of
				 * masks. */
static fd_mask ready[3*MASK_SIZE];
				/* Temporary copy of masks, passed
				 * to select and modified by kernel
				 * to indicate which files are
				 * actually ready. */
static fd_mask *readPtr;	/* Pointers to the portions of */
static fd_mask *writePtr;	/* *readyPtr for reading, writing, */
static fd_mask *exceptPtr;	/* and excepting.  Will be NULL if
				 * corresponding count (e.g. readCount
				 * is zero. */
static int numFds = 0;		/* Number of valid bits in mask
				 * arrays (this value is passed
				 * to select). */

/*
 * For each file registered in a call to Tk_CreateFileHandler,
 * and for each display that's currently active, there is one
 * record of the following type.  All of these records are
 * chained together into a single list.
 */

typedef struct FileEvent {
    int fd;			/* Descriptor number for this file. */
    int isDisplay;		/* Non-zero means that this file descriptor
				 * corresponds to a display and should be
				 * treated specially. */
    fd_mask *readPtr;		/* Pointer to word in ready array
				 * for this file's read mask bit. */
    fd_mask *writePtr;		/* Same for write mask bit. */
    fd_mask *exceptPtr;		/* Same for except mask bit. */
    fd_mask mask;		/* Value to AND with mask word to
				 * select just this file's bit. */
    void (*proc)  _ANSI_ARGS_((ClientData clientData, int mask));
				/* Procedure to call.  NULL means
				 * this is a display. */
    ClientData clientData;	/* Argument to pass to proc.  For
				 * displays, this is a (Display *). */
    struct FileEvent *nextPtr;	/* Next in list of all files we
				 * care about (NULL for end of
				 * list). */
} FileEvent;

static FileEvent *fileList;	/* List of all file events. */

/*
 * There is one of the following structures for each of the
 * handlers declared in a call to Tk_DoWhenIdle.  All of the
 * currently-active handlers are linked together into a list.
 */

typedef struct IdleHandler {
    void (*proc)  _ANSI_ARGS_((ClientData clientData));
				/* Procedure to call. */
    ClientData clientData;	/* Value to pass to proc. */
    int generation;		/* Used to distinguish older handlers from
				 * recently-created ones. */
    struct IdleHandler *nextPtr;/* Next in list of active handlers. */
} IdleHandler;

static IdleHandler *idleList = NULL;
				/* First in list of all idle handlers. */
static IdleHandler *lastIdlePtr = NULL;
				/* Last in list (or NULL for empty list). */
static int idleGeneration = 0;	/* Used to fill in the "generation" fields
				 * of IdleHandler structures.  Increments
				 * each time Tk_DoOneEvent starts calling
				 * idle handlers, so that all old handlers
				 * can be called without calling any of the
				 * new ones created by old ones. */

/*
 * There's a potential problem if a handler is deleted while it's
 * current (i.e. its procedure is executing), since Tk_HandleEvent
 * will need to read the handler's "nextPtr" field when the procedure
 * returns.  To handle this problem, structures of the type below
 * indicate the next handler to be processed for any (recursively
 * nested) dispatches in progress.  The nextHandler fields get
 * updated if the handlers pointed to are deleted.  Tk_HandleEvent
 * also needs to know if the entire window gets deleted;  the winPtr
 * field is set to zero if that particular window gets deleted.
 */

typedef struct InProgress {
    XEvent *eventPtr;		 /* Event currently being handled. */
    TkWindow *winPtr;		 /* Window for event.  Gets set to None if
				  * window is deleted while event is being
				  * handled. */
    TkEventHandler *nextHandler; /* Next handler in search. */
    struct InProgress *nextPtr;	 /* Next higher nested search. */
} InProgress;

static InProgress *pendingPtr = NULL;
				/* Topmost search in progress, or
				 * NULL if none. */

/*
 * For each call to Tk_CreateGenericHandler, an instance of the following
 * structure will be created.  All of the active handlers are linked into a
 * list.
 */

typedef struct GenericHandler {
    Tk_GenericProc *proc;	/* Procedure to dispatch on all X events. */
    ClientData clientData;	/* Client data to pass to procedure. */
    int deleteFlag;		/* Flag to set when this handler is deleted. */
    struct GenericHandler *nextPtr;
				/* Next handler in list of all generic
				 * handlers, or NULL for end of list. */
} GenericHandler;

static GenericHandler *genericList = NULL;
				/* First handler in the list, or NULL. */
static GenericHandler *lastGenericPtr = NULL;
				/* Last handler in list. */

/*
 * There's a potential problem if Tk_HandleEvent is entered recursively.
 * A handler cannot be deleted physically until we have returned from
 * calling it.  Otherwise, we're looking at unallocated memory in advancing to
 * its `next' entry.  We deal with the problem by using the `delete flag' and
 * deleting handlers only when it's known that there's no handler active.
 *
 * The following variable has a non-zero value when a handler is active.
 */

static int genericHandlersActive = 0;

/*
 * Array of event masks corresponding to each X event:
 */

static unsigned long eventMasks[] = {
    0,
    0,
    KeyPressMask,			/* KeyPress */
    KeyReleaseMask,			/* KeyRelease */
    ButtonPressMask,			/* ButtonPress */
    ButtonReleaseMask,			/* ButtonRelease */
    PointerMotionMask|PointerMotionHintMask|ButtonMotionMask
	    |Button1MotionMask|Button2MotionMask|Button3MotionMask
	    |Button4MotionMask|Button5MotionMask,
					/* MotionNotify */
    EnterWindowMask,			/* EnterNotify */
    LeaveWindowMask,			/* LeaveNotify */
    FocusChangeMask,			/* FocusIn */
    FocusChangeMask,			/* FocusOut */
    KeymapStateMask,			/* KeymapNotify */
    ExposureMask,			/* Expose */
    ExposureMask,			/* GraphicsExpose */
    ExposureMask,			/* NoExpose */
    VisibilityChangeMask,		/* VisibilityNotify */
    SubstructureNotifyMask,		/* CreateNotify */
    StructureNotifyMask,		/* DestroyNotify */
    StructureNotifyMask,		/* UnmapNotify */
    StructureNotifyMask,		/* MapNotify */
    SubstructureRedirectMask,		/* MapRequest */
    StructureNotifyMask,		/* ReparentNotify */
    StructureNotifyMask,		/* ConfigureNotify */
    SubstructureRedirectMask,		/* ConfigureRequest */
    StructureNotifyMask,		/* GravityNotify */
    ResizeRedirectMask,			/* ResizeRequest */
    StructureNotifyMask,		/* CirculateNotify */
    SubstructureRedirectMask,		/* CirculateRequest */
    PropertyChangeMask,			/* PropertyNotify */
    0,					/* SelectionClear */
    0,					/* SelectionRequest */
    0,					/* SelectionNotify */
    ColormapChangeMask,			/* ColormapNotify */
    0,					/* ClientMessage */
    0,					/* Mapping Notify */
};

/*
 * If someone has called Tk_RestrictEvents, the information below
 * keeps track of it.
 */

static Bool (*restrictProc)  _ANSI_ARGS_((Display *display, XEvent *eventPtr,
    char *arg));		/* Procedure to call.  NULL means no
				 * restrictProc is currently in effect. */
static char *restrictArg;	/* Argument to pass to restrictProc. */

/*
 * The following array keeps track of the last TK_NEVENTS X events, for
 * memory dump analysis.  The tracing is only done if tkEventDebug is set
 * to 1.
 */

#define TK_NEVENTS 32
static XEvent eventTrace[TK_NEVENTS];
static int traceIndex = 0;
int tkEventDebug = 0;

/*
 *--------------------------------------------------------------
 *
 * Tk_CreateEventHandler --
 *
 *	Arrange for a given procedure to be invoked whenever
 *	events from a given class occur in a given window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	From now on, whenever an event of the type given by
 *	mask occurs for token and is processed by Tk_HandleEvent,
 *	proc will be called.  See the manual entry for details
 *	of the calling sequence and return value for proc.
 *
 *--------------------------------------------------------------
 */

void
Tk_CreateEventHandler(token, mask, proc, clientData)
    Tk_Window token;		/* Token for window in which to
				 * create handler. */
    unsigned long mask;		/* Events for which proc should
				 * be called. */
    Tk_EventProc *proc;		/* Procedure to call for each
				 * selected event */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    register TkEventHandler *handlerPtr;
    register TkWindow *winPtr = (TkWindow *) token;
    int found;

    /*
     * Skim through the list of existing handlers to (a) compute the
     * overall event mask for the window (so we can pass this new
     * value to the X system) and (b) see if there's already a handler
     * declared with the same callback and clientData (if so, just
     * change the mask).  If no existing handler matches, then create
     * a new handler.
     */

    found = 0;
    if (winPtr->handlerList == NULL) {
	handlerPtr = (TkEventHandler *) ckalloc(
		(unsigned) sizeof(TkEventHandler));
	winPtr->handlerList = handlerPtr;
	goto initHandler;
    } else {
	for (handlerPtr = winPtr->handlerList; ;
		handlerPtr = handlerPtr->nextPtr) {
	    if ((handlerPtr->proc == proc)
		    && (handlerPtr->clientData == clientData)) {
		handlerPtr->mask = mask;
		found = 1;
	    }
	    if (handlerPtr->nextPtr == NULL) {
		break;
	    }
	}
    }

    /*
     * Create a new handler if no matching old handler was found.
     */

    if (!found) {
	handlerPtr->nextPtr = (TkEventHandler *)
		ckalloc(sizeof(TkEventHandler));
	handlerPtr = handlerPtr->nextPtr;
	initHandler:
	handlerPtr->mask = mask;
	handlerPtr->proc = proc;
	handlerPtr->clientData = clientData;
	handlerPtr->nextPtr = NULL;
    }

    /*
     * No need to call XSelectInput:  Tk always selects on all events
     * for all windows (needed to support bindings on classes and "all").
     */
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeleteEventHandler --
 *
 *	Delete a previously-created handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there existed a handler as described by the
 *	parameters, the handler is deleted so that proc
 *	will not be invoked again.
 *
 *--------------------------------------------------------------
 */

void
Tk_DeleteEventHandler(token, mask, proc, clientData)
    Tk_Window token;		/* Same as corresponding arguments passed */
    unsigned long mask;		/* previously to Tk_CreateEventHandler. */
    Tk_EventProc *proc;
    ClientData clientData;
{
    register TkEventHandler *handlerPtr;
    register InProgress *ipPtr;
    TkEventHandler *prevPtr;
    register TkWindow *winPtr = (TkWindow *) token;

    /*
     * Find the event handler to be deleted, or return
     * immediately if it doesn't exist.
     */

    for (handlerPtr = winPtr->handlerList, prevPtr = NULL; ;
	    prevPtr = handlerPtr, handlerPtr = handlerPtr->nextPtr) {
	if (handlerPtr == NULL) {
	    return;
	}
	if ((handlerPtr->mask == mask) && (handlerPtr->proc == proc)
		&& (handlerPtr->clientData == clientData)) {
	    break;
	}
    }

    /*
     * If Tk_HandleEvent is about to process this handler, tell it to
     * process the next one instead.
     */

    for (ipPtr = pendingPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	if (ipPtr->nextHandler == handlerPtr) {
	    ipPtr->nextHandler = handlerPtr->nextPtr;
	}
    }

    /*
     * Free resources associated with the handler.
     */

    if (prevPtr == NULL) {
	winPtr->handlerList = handlerPtr->nextPtr;
    } else {
	prevPtr->nextPtr = handlerPtr->nextPtr;
    }
    ckfree((char *) handlerPtr);


    /*
     * No need to call XSelectInput:  Tk always selects on all events
     * for all windows (needed to support bindings on classes and "all").
     */
}

/*--------------------------------------------------------------
 *
 * Tk_CreateGenericHandler --
 *
 *	Register a procedure to be called on each X event, regardless
 *	of display or window.  Generic handlers are useful for capturing
 *	events that aren't associated with windows, or events for windows
 *	not managed by Tk.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	From now on, whenever an X event is given to Tk_HandleEvent,
 *	invoke proc, giving it clientData and the event as arguments.
 *
 *--------------------------------------------------------------
 */

void
Tk_CreateGenericHandler(proc, clientData)
     Tk_GenericProc *proc;	/* Procedure to call on every event. */
     ClientData clientData;	/* One-word value to pass to proc. */
{
    GenericHandler *handlerPtr;
    
    handlerPtr = (GenericHandler *) ckalloc (sizeof (GenericHandler));
    
    handlerPtr->proc = proc;
    handlerPtr->clientData = clientData;
    handlerPtr->deleteFlag = 0;
    handlerPtr->nextPtr = NULL;
    if (genericList == NULL) {
	genericList = handlerPtr;
    } else {
	lastGenericPtr->nextPtr = handlerPtr;
    }
    lastGenericPtr = handlerPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeleteGenericHandler --
 *
 *	Delete a previously-created generic handler.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	If there existed a handler as described by the parameters,
 *	that handler is logically deleted so that proc will not be
 *	invoked again.  The physical deletion happens in the event
 *	loop in Tk_HandleEvent.
 *
 *--------------------------------------------------------------
 */

void
Tk_DeleteGenericHandler(proc, clientData)
     Tk_GenericProc *proc;
     ClientData clientData;
{
    GenericHandler * handler;
    
    for (handler = genericList; handler; handler = handler->nextPtr) {
	if ((handler->proc == proc) && (handler->clientData == clientData)) {
	    handler->deleteFlag = 1;
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_HandleEvent --
 *
 *	Given an event, invoke all the handlers that have
 *	been registered for the event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the handlers.
 *
 *--------------------------------------------------------------
 */

void
Tk_HandleEvent(eventPtr)
    XEvent *eventPtr;		/* Event to dispatch. */
{
    register TkEventHandler *handlerPtr;
    register GenericHandler *genericPtr;
    register GenericHandler *genPrevPtr;
    TkWindow *winPtr;
    register unsigned long mask;
    InProgress ip;
    Window handlerWindow;

    /*
     * First off, look for a special trigger event left around by the
     * grab module.  If it's found, call the grab module and discard
     * the event.
     */

    if ((eventPtr->xany.type == -1) && (eventPtr->xany.window == None)) {
	TkGrabTriggerProc(eventPtr);
	return;
    }

    /* 
     * Next, invoke all the generic event handlers (those that are
     * invoked for all events).  If a generic event handler reports that
     * an event is fully processed, go no further.
     */

    for (genPrevPtr = NULL, genericPtr = genericList;  genericPtr != NULL; ) {
	if (genericPtr->deleteFlag) {
	    if (!genericHandlersActive) {
		GenericHandler *tmpPtr;

		/*
		 * This handler needs to be deleted and there are no
		 * calls pending through the handler, so now is a safe
		 * time to delete it.
		 */

		tmpPtr = genericPtr->nextPtr;
		if (genPrevPtr == NULL) {
		    genericList = tmpPtr;
		} else {
		    genPrevPtr->nextPtr = tmpPtr;
		}
		if (tmpPtr == NULL) {
		    lastGenericPtr = genPrevPtr;
		}
		(void) ckfree((char *) genericPtr);
		genericPtr = tmpPtr;
		continue;
	    }
	} else {
	    int done;

	    genericHandlersActive++;
	    done = (*genericPtr->proc)(genericPtr->clientData, eventPtr);
	    genericHandlersActive--;
	    if (done) {
		return;
	    }
	}
	genPrevPtr = genericPtr;
	genericPtr = genPrevPtr->nextPtr;
    }

    /*
     * If the event is a MappingNotify event, find its display and
     * refresh the keyboard mapping information for the display.
     * After that there's nothing else to do with the event, so just
     * quit.
     */

    if (eventPtr->type == MappingNotify) {
	TkDisplay *dispPtr;

	for (dispPtr = tkDisplayList; dispPtr != NULL;
		dispPtr = dispPtr->nextPtr) {
	    if (dispPtr->display != eventPtr->xmapping.display) {
		continue;
	    }
	    XRefreshKeyboardMapping(&eventPtr->xmapping);
	    dispPtr->bindInfoStale = 1;
	    break;
	}
	return;
    }

    /*
     * Events selected by StructureNotify look the same as those
     * selected by SubstructureNotify;  the only difference is
     * whether the "event" and "window" fields are the same.
     * Check it out and convert StructureNotify to
     * SubstructureNotify if necessary.
     */

    handlerWindow = eventPtr->xany.window;
    mask = eventMasks[eventPtr->xany.type];
    if (mask == StructureNotifyMask) {
	if (eventPtr->xmap.event != eventPtr->xmap.window) {
	    mask = SubstructureNotifyMask;
	    handlerWindow = eventPtr->xmap.event;
	}
    }
    if (XFindContext(eventPtr->xany.display, handlerWindow,
	    tkWindowContext, (caddr_t *) &winPtr) != 0) {

	/*
	 * There isn't a TkWindow structure for this window.
	 * However, if the event is a PropertyNotify event then call
	 * the selection manager (it deals beneath-the-table with
	 * certain properties).
	 */

	if (eventPtr->type == PropertyNotify) {
	    TkSelPropProc(eventPtr);
	}
	return;
    }

    /*
     * Call focus-related code to look at FocusIn, FocusOut, Enter,
     * and Leave events;  depending on its return value, ignore the
     * event.
     */

    if ((mask & (FocusChangeMask|EnterWindowMask|LeaveWindowMask))
	    && !TkFocusFilterEvent(winPtr, eventPtr)) {
	return;
    }

    /*
     * Redirect KeyPress and KeyRelease events to the focus window,
     * or ignore them entirely if there is no focus window.  Map the
     * x and y coordinates to make sense in the context of the focus
     * window, if possible (make both -1 if the map-from and map-to
     * windows don't share the same screen).
     */

    if (mask & (KeyPressMask|KeyReleaseMask)) {
	TkWindow *focusPtr;
	int winX, winY, focusX, focusY;

	winPtr->dispPtr->lastEventTime = eventPtr->xkey.time;
	if (winPtr->mainPtr->focusPtr == NULL) {
	    return;
	}
	focusPtr = winPtr->mainPtr->focusPtr;
	if ((focusPtr->display != winPtr->display)
		|| (focusPtr->screenNum != winPtr->screenNum)) {
	    eventPtr->xkey.x = -1;
	    eventPtr->xkey.y = -1;
	} else {
	    Tk_GetRootCoords((Tk_Window) winPtr, &winX, &winY);
	    Tk_GetRootCoords((Tk_Window) focusPtr, &focusX, &focusY);
	    eventPtr->xkey.x -= focusX - winX;
	    eventPtr->xkey.y -= focusY - winY;
	}
	eventPtr->xkey.window = focusPtr->window;
	winPtr = focusPtr;
    }

    /*
     * Call a grab-related procedure to do special processing on
     * pointer events.
     */

    if (mask & (ButtonPressMask|ButtonReleaseMask|PointerMotionMask
	    |EnterWindowMask|LeaveWindowMask)) {
	if (mask & (ButtonPressMask|ButtonReleaseMask)) {
	    winPtr->dispPtr->lastEventTime = eventPtr->xbutton.time;
	} else if (mask & PointerMotionMask) {
	    winPtr->dispPtr->lastEventTime = eventPtr->xmotion.time;
	} else {
	    winPtr->dispPtr->lastEventTime = eventPtr->xcrossing.time;
	}
	if (TkPointerEvent(eventPtr, winPtr) == 0) {
	    return;
	}
    }

    /*
     * For events where it hasn't already been done, update the current
     * time in the display.
     */

    if (eventPtr->type == PropertyNotify) {
	winPtr->dispPtr->lastEventTime = eventPtr->xproperty.time;
    }

    /*
     * There's a potential interaction here with Tk_DeleteEventHandler.
     * Read the documentation for pendingPtr.
     */

    ip.eventPtr = eventPtr;
    ip.winPtr = winPtr;
    ip.nextHandler = NULL;
    ip.nextPtr = pendingPtr;
    pendingPtr = &ip;
    if (mask == 0) {
	if ((eventPtr->type == SelectionClear)
		|| (eventPtr->type == SelectionRequest)
		|| (eventPtr->type == SelectionNotify)) {
	    TkSelEventProc((Tk_Window) winPtr, eventPtr);
	} else if ((eventPtr->type == ClientMessage)
		&& (eventPtr->xclient.message_type ==
		    Tk_InternAtom((Tk_Window) winPtr, "WM_PROTOCOLS"))) {
	    TkWmProtocolEventProc(winPtr, eventPtr);
	}
    } else {
	for (handlerPtr = winPtr->handlerList; handlerPtr != NULL; ) {
	    if ((handlerPtr->mask & mask) != 0) {
		ip.nextHandler = handlerPtr->nextPtr;
		(*(handlerPtr->proc))(handlerPtr->clientData, eventPtr);
		handlerPtr = ip.nextHandler;
	    } else {
		handlerPtr = handlerPtr->nextPtr;
	    }
	}

	/*
	 * Pass the event to the "bind" command mechanism.  But, don't
	 * do this for SubstructureNotify events.  The "bind" command
	 * doesn't support them anyway, and it's easier to filter out
	 * these events here than in the lower-level procedures.
	 */

	if ((ip.winPtr != None) && (mask != SubstructureNotifyMask)) {
	    TkBindEventProc(winPtr, eventPtr);
	}
    }
    pendingPtr = ip.nextPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CreateFileHandler --
 *
 *	Arrange for a given procedure to be invoked whenever
 *	a given file becomes readable or writable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	From now on, whenever the I/O channel given by fd becomes
 *	ready in the way indicated by mask, proc will be invoked.
 *	See the manual entry for details on the calling sequence
 *	to proc.  If fd is already registered then the old mask
 *	and proc and clientData values will be replaced with
 *	new ones.
 *
 *--------------------------------------------------------------
 */

void
Tk_CreateFileHandler(fd, mask, proc, clientData)
    int fd;			/* Integer identifier for stream. */
    int mask;			/* OR'ed combination of TK_READABLE,
				 * TK_WRITABLE, and TK_EXCEPTION:
				 * indicates conditions under which
				 * proc should be called.  TK_IS_DISPLAY
				 * indicates that this is a display and that
				 * clientData is the (Display *) for it,
				 * and that events should be handled
				 * automatically.*/
    Tk_FileProc *proc;		/* Procedure to call for each
				 * selected event. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    register FileEvent *filePtr;
    int index;

    if (fd >= OPEN_MAX) {
	panic("Tk_CreatefileHandler can't handle file id %d", fd);
    }

    /*
     * Make sure the file isn't already registered.  Create a
     * new record in the normal case where there's no existing
     * record.
     */

    for (filePtr = fileList; filePtr != NULL;
	    filePtr = filePtr->nextPtr) {
	if (filePtr->fd == fd) {
	    break;
	}
    }
    index = fd/(NBBY*sizeof(fd_mask));
    if (filePtr == NULL) {
	filePtr = (FileEvent *) ckalloc(sizeof(FileEvent));
	filePtr->fd = fd;
	filePtr->isDisplay = 0;
	filePtr->readPtr = &ready[index];
	filePtr->writePtr = &ready[index+MASK_SIZE];
	filePtr->exceptPtr = &ready[index+2*MASK_SIZE];
	filePtr->mask = 1 << (fd%(NBBY*sizeof(fd_mask)));
	filePtr->nextPtr = fileList;
	fileList = filePtr;
    } else {
	if (masks[index] & filePtr->mask) {
	    readCount--;
	    *filePtr->readPtr &= ~filePtr->mask;
	    masks[index] &= ~filePtr->mask;
	}
	if (masks[index+MASK_SIZE] & filePtr->mask) {
	    writeCount--;
	    *filePtr->writePtr &= ~filePtr->mask;
	    masks[index+MASK_SIZE] &= ~filePtr->mask;
	}
	if (masks[index+2*MASK_SIZE] & filePtr->mask) {
	    exceptCount--;
	    *filePtr->exceptPtr &= ~filePtr->mask;
	    masks[index+2*MASK_SIZE] &= ~filePtr->mask;
	}
    }

    /*
     * The remainder of the initialization below is done
     * regardless of whether or not this is a new record
     * or a modification of an old one.
     */

    if (mask & TK_READABLE) {
	masks[index] |= filePtr->mask;
	readCount++;
    }
    readPtr = (readCount == 0) ? (fd_mask *) NULL : &ready[0];

    if (mask & TK_WRITABLE) {
	masks[index+MASK_SIZE] |= filePtr->mask;
	writeCount++;
    }
    writePtr = (writeCount == 0) ? (fd_mask *) NULL : &ready[MASK_SIZE];

    if (mask & TK_EXCEPTION) {
	masks[index+2*MASK_SIZE] |= filePtr->mask;
	exceptCount++;
    }
    exceptPtr = (exceptCount == 0) ? (fd_mask *) NULL : &ready[2*MASK_SIZE];

    if (mask & TK_IS_DISPLAY) {
	filePtr->isDisplay = 1;
    } else {
	filePtr->isDisplay = 0;
    }

    filePtr->proc = proc;
    filePtr->clientData = clientData;

    if (numFds <= fd) {
	numFds = fd+1;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeleteFileHandler --
 *
 *	Cancel a previously-arranged callback arrangement for
 *	a file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a callback was previously registered on fd, remove it.
 *
 *--------------------------------------------------------------
 */

void
Tk_DeleteFileHandler(fd)
    int fd;			/* Stream id for which to remove
				 * callback procedure. */
{
    register FileEvent *filePtr;
    FileEvent *prevPtr;
    int index;

    /*
     * Find the entry for the given file (and return if there
     * isn't one).
     */

    for (prevPtr = NULL, filePtr = fileList; ;
	    prevPtr = filePtr, filePtr = filePtr->nextPtr) {
	if (filePtr == NULL) {
	    return;
	}
	if (filePtr->fd == fd) {
	    break;
	}
    }

    /*
     * Clean up information in the callback record.
     */

    index = filePtr->fd/(NBBY*sizeof(fd_mask));
    if (masks[index] & filePtr->mask) {
	readCount--;
	*filePtr->readPtr &= ~filePtr->mask;
	masks[index] &= ~filePtr->mask;
    }
    if (masks[index+MASK_SIZE] & filePtr->mask) {
	writeCount--;
	*filePtr->writePtr &= ~filePtr->mask;
	masks[index+MASK_SIZE] &= ~filePtr->mask;
    }
    if (masks[index+2*MASK_SIZE] & filePtr->mask) {
	exceptCount--;
	*filePtr->exceptPtr &= ~filePtr->mask;
	masks[index+2*MASK_SIZE] &= ~filePtr->mask;
    }
    if (prevPtr == NULL) {
	fileList = filePtr->nextPtr;
    } else {
	prevPtr->nextPtr = filePtr->nextPtr;
    }
    ckfree((char *) filePtr);

    /*
     * Recompute numFds.
     */

    numFds = 0;
    for (filePtr = fileList; filePtr != NULL;
	    filePtr = filePtr->nextPtr) {
	if (numFds <= filePtr->fd) {
	    numFds = filePtr->fd+1;
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CreateTimerHandler --
 *
 *	Arrange for a given procedure to be invoked at a particular
 *	time in the future.
 *
 * Results:
 *	The return value is a token for the timer event, which
 *	may be used to delete the event before it fires.
 *
 * Side effects:
 *	When milliseconds have elapsed, proc will be invoked
 *	exactly once.
 *
 *--------------------------------------------------------------
 */

Tk_TimerToken
Tk_CreateTimerHandler(milliseconds, proc, clientData)
    int milliseconds;		/* How many milliseconds to wait
				 * before invoking proc. */
    Tk_TimerProc *proc;		/* Procedure to invoke. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    register TimerEvent *timerPtr, *tPtr2, *prevPtr;
    static int id = 0;

    timerPtr = (TimerEvent *) ckalloc(sizeof(TimerEvent));

    /*
     * Compute when the event should fire.
     */

    (void) gettimeofday(&timerPtr->time, (struct timezone *) NULL);
    timerPtr->time.tv_sec += milliseconds/1000;
    timerPtr->time.tv_usec += (milliseconds%1000)*1000;
    if (timerPtr->time.tv_usec > 1000000) {
	timerPtr->time.tv_usec -= 1000000;
	timerPtr->time.tv_sec += 1;
    }

    /*
     * Fill in other fields for the event.
     */

    timerPtr->proc = proc;
    timerPtr->clientData = clientData;
    id++;
    timerPtr->token = (Tk_TimerToken) id;

    /*
     * Add the event to the queue in the correct position
     * (ordered by event firing time).
     */

    for (tPtr2 = timerQueue, prevPtr = NULL; tPtr2 != NULL;
	    prevPtr = tPtr2, tPtr2 = tPtr2->nextPtr) {
	if ((tPtr2->time.tv_sec > timerPtr->time.tv_sec)
		|| ((tPtr2->time.tv_sec == timerPtr->time.tv_sec)
		&& (tPtr2->time.tv_usec > timerPtr->time.tv_usec))) {
	    break;
	}
    }
    if (prevPtr == NULL) {
	timerPtr->nextPtr = timerQueue;
	timerQueue = timerPtr;
    } else {
	timerPtr->nextPtr = prevPtr->nextPtr;
	prevPtr->nextPtr = timerPtr;
    }
    return timerPtr->token;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeleteTimerHandler --
 *
 *	Delete a previously-registered timer handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroy the timer callback identified by TimerToken,
 *	so that its associated procedure will not be called.
 *	If the callback has already fired, or if the given
 *	token doesn't exist, then nothing happens.
 *
 *--------------------------------------------------------------
 */

void
Tk_DeleteTimerHandler(token)
    Tk_TimerToken token;	/* Result previously returned by
				 * Tk_DeleteTimerHandler. */
{
    register TimerEvent *timerPtr, *prevPtr;

    for (timerPtr = timerQueue, prevPtr = NULL; timerPtr != NULL;
	    prevPtr = timerPtr, timerPtr = timerPtr->nextPtr) {
	if (timerPtr->token != token) {
	    continue;
	}
	if (prevPtr == NULL) {
	    timerQueue = timerPtr->nextPtr;
	} else {
	    prevPtr->nextPtr = timerPtr->nextPtr;
	}
	ckfree((char *) timerPtr);
	return;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DoWhenIdle --
 *
 *	Arrange for proc to be invoked the next time the
 *	system is idle (i.e., just before the next time
 *	that Tk_DoOneEvent would have to wait for something
 *	to happen).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Proc will eventually be called, with clientData
 *	as argument.  See the manual entry for details.
 *
 *--------------------------------------------------------------
 */

void
Tk_DoWhenIdle(proc, clientData)
    Tk_IdleProc *proc;		/* Procedure to invoke. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
{
    register IdleHandler *idlePtr;

    idlePtr = (IdleHandler *) ckalloc(sizeof(IdleHandler));
    idlePtr->proc = proc;
    idlePtr->clientData = clientData;
    idlePtr->generation = idleGeneration;
    idlePtr->nextPtr = NULL;
    if (lastIdlePtr == NULL) {
	idleList = idlePtr;
    } else {
	lastIdlePtr->nextPtr = idlePtr;
    }
    lastIdlePtr = idlePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CancelIdleCall --
 *
 *	If there are any when-idle calls requested to a given procedure
 *	with given clientData, cancel all of them.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the proc/clientData combination were on the when-idle list,
 *	they are removed so that they will never be called.
 *
 *----------------------------------------------------------------------
 */

void
Tk_CancelIdleCall(proc, clientData)
    Tk_IdleProc *proc;		/* Procedure that was previously registered. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
{
    register IdleHandler *idlePtr, *prevPtr;
    IdleHandler *nextPtr;

    for (prevPtr = NULL, idlePtr = idleList; idlePtr != NULL;
	    prevPtr = idlePtr, idlePtr = idlePtr->nextPtr) {
	while ((idlePtr->proc == proc)
		&& (idlePtr->clientData == clientData)) {
	    nextPtr = idlePtr->nextPtr;
	    ckfree((char *) idlePtr);
	    idlePtr = nextPtr;
	    if (prevPtr == NULL) {
		idleList = idlePtr;
	    } else {
		prevPtr->nextPtr = idlePtr;
	    }
	    if (idlePtr == NULL) {
		lastIdlePtr = prevPtr;
		return;
	    }
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DoOneEvent --
 *
 *	Process a single event of some sort.  If there's no
 *	work to do, wait for an event to occur, then process
 *	it.
 *
 * Results:
 *	The return value is 1 if the procedure actually found
 *	an event to process.  If no event was found then 0 is
 *	returned.
 *
 * Side effects:
 *	May delay execution of process while waiting for an
 *	X event, X error, file-ready event, or timer event.
 *	The handling of the event could cause additional
 *	side effects.  Collapses sequences of mouse-motion
 *	events for the same window into a single event by
 *	delaying motion event processing.
 *
 *--------------------------------------------------------------
 */
    static int eventDelayed = 0;	/* Non-zero means there is an event
					 * in delayedMotionEvent. */

int
Tk_DoOneEvent(flags)
    int flags;			/* Miscellaneous flag values:  may be any
				 * combination of TK_DONT_WAIT, TK_X_EVENTS,
				 * TK_FILE_EVENTS, TK_TIMER_EVENTS, and
				 * TK_IDLE_EVENTS. */
{
    register FileEvent *filePtr;
    struct timeval curTime, timeout, *timeoutPtr;
    int numFound;
    static XEvent delayedMotionEvent;	/* Used to hold motion events that
					 * are being saved until later. */

    if ((flags & TK_ALL_EVENTS) == 0) {
	flags |= TK_ALL_EVENTS;
    }

    /*
     * Phase One: see if there's already something ready
     * (either a file or a display) that was left over
     * from before (i.e don't do a select, just check the
     * bits from the last select).
     */

    checkFiles:
    if (tcl_AsyncReady) {
	(void) Tcl_AsyncInvoke((Tcl_Interp *) NULL, 0);
	return 1;
    }
    for (filePtr = fileList; filePtr != NULL;
	    filePtr = filePtr->nextPtr) {
	int mask;

	/*
	 * Displays:  flush output, check for queued events,
	 * and read events from the server if display is ready.
	 * If there are any events, process one and then
	 * return.
	 */

	if (filePtr->isDisplay) {
	    Display *display = (Display *) filePtr->clientData;
	    XEvent event;

	    if (!(flags & TK_X_EVENTS)) {
		continue;
	    }
	    XFlush(display);
	    if ((*filePtr->readPtr) & filePtr->mask) {
		*filePtr->readPtr &= ~filePtr->mask;
		if (XEventsQueued(display, QueuedAfterReading) == 0) {

		    /*
		     * Things are very tricky if there aren't any events
		     * readable at this point (after all, there was
		     * supposedly data available on the connection).
		     * A couple of things could have occurred:
		     * 
		     * One possibility is that there were only error events
		     * in the input from the server.  If this happens,
		     * we should return (we don't want to go to sleep
		     * in XNextEvent below, since this would block out
		     * other sources of input to the process).
		     *
		     * Another possibility is that our connection to the
		     * server has been closed.  This will not necessarily
		     * be detected in XEventsQueued (!!), so if we just
		     * return then there will be an infinite loop.  To
		     * detect such an error, generate a NoOp protocol
		     * request to exercise the connection to the server,
		     * then return.  However, must disable SIGPIPE while
		     * sending the event, or else the process will die
		     * from the signal and won't invoke the X error
		     * function to print a nice message.
		     */

		    void (*oldHandler)();

		    oldHandler = (void (*)()) signal(SIGPIPE, SIG_IGN);
		    XNoOp(display);
		    XFlush(display);
		    (void) signal(SIGPIPE, oldHandler);
		    return 1;
		}
		if (restrictProc != NULL) {
		    if (!XCheckIfEvent(display, &event, restrictProc,
			    restrictArg)) {
			return 1;
		    }
		} else {
		    XNextEvent(display, &event);
		}
	    } else {
		if (QLength(display) == 0) {
		    continue;
		}
		if (restrictProc != NULL) {
		    if (!XCheckIfEvent(display, &event, restrictProc,
			    restrictArg)) {
			continue;
		    }
		} else {
		    XNextEvent(display, &event);
		}
	    }

	    /*
	     * Got an event.  Deal with mouse-motion-collapsing and
	     * event-delaying here.  If there's already an event delayed,
	     * then process that event if it's incompatible with the new
	     * event (new event not mouse motion, or window changed, or
	     * state changed).  If the new event is mouse motion, then
	     * don't process it now;  delay it until later in the hopes
	     * that it can be merged with other mouse motion events
	     * immediately following.
	     */

	    if (tkEventDebug) {
		eventTrace[traceIndex] = event;
		traceIndex = (traceIndex+1) % TK_NEVENTS;
	    }

	    if (eventDelayed) {
		if (((event.type != MotionNotify)
			    && (event.type != GraphicsExpose)
			    && (event.type != NoExpose)
			    && (event.type != Expose))
			|| (event.xmotion.display
			    != delayedMotionEvent.xmotion.display)
			|| (event.xmotion.window
			    != delayedMotionEvent.xmotion.window)) {
		    XEvent copy;

		    /*
		     * Must copy the event out of delayedMotionEvent before
		     * processing it, in order to allow recursive calls to
		     * Tk_DoOneEvent as part of the handler.
		     */

		    copy = delayedMotionEvent;
		    eventDelayed = 0;
		    Tk_HandleEvent(&copy);
		}
	    }
	    if (event.type == MotionNotify) {
		delayedMotionEvent = event;
		eventDelayed = 1;
	    } else {
		Tk_HandleEvent(&event);
	    }
	    return 1;
	}

	/*
	 * Not a display:  if the file is ready, call the
	 * appropriate handler.
	 */

	if (((*filePtr->readPtr | *filePtr->writePtr
		| *filePtr->exceptPtr) & filePtr->mask) == 0) {
	    continue;
	}
	if (!(flags & TK_FILE_EVENTS)) {
	    continue;
	}
	mask = 0;
	if (*filePtr->readPtr & filePtr->mask) {
	    mask |= TK_READABLE;
	    *filePtr->readPtr &= ~filePtr->mask;
	}
	if (*filePtr->writePtr & filePtr->mask) {
	    mask |= TK_WRITABLE;
	    *filePtr->writePtr &= ~filePtr->mask;
	}
	if (*filePtr->exceptPtr & filePtr->mask) {
	    mask |= TK_EXCEPTION;
	    *filePtr->exceptPtr &= ~filePtr->mask;
	}
	(*filePtr->proc)(filePtr->clientData, mask);
	return 1;
    }

    /*
     * Phase Two: get the current time and see if any timer
     * events are ready to fire.  If so, fire one and return.
     */

    checkTime:
    if ((timerQueue != NULL) && (flags & TK_TIMER_EVENTS)) {
	register TimerEvent *timerPtr = timerQueue;

	(void) gettimeofday(&curTime, (struct timezone *) NULL);
	if ((timerPtr->time.tv_sec < curTime.tv_sec)
		|| ((timerPtr->time.tv_sec == curTime.tv_sec)
		&&  (timerPtr->time.tv_usec < curTime.tv_usec))) {
	    timerQueue = timerPtr->nextPtr;
	    (*timerPtr->proc)(timerPtr->clientData);
	    ckfree((char *) timerPtr);
	    return 1;
	}
    }


    /*
     * Phase Three: if there is a delayed motion event, process it
     * now, before any DoWhenIdle handlers.  Better to process before
     * idle handlers than after, because the goal of idle handlers is
     * to delay until after all pending events have been processed.
     * Must free up delayedMotionEvent *before* calling Tk_HandleEvent,
     * so that the event handler can call Tk_DoOneEvent recursively
     * without infinite looping.
     */

    if ((eventDelayed) && (flags & TK_X_EVENTS)) {
	XEvent copy;

	copy = delayedMotionEvent;
	eventDelayed = 0;
	Tk_HandleEvent(&copy);
	return 1;
    }

    /*
     * Phase Four: if there are DoWhenIdle requests pending (or
     * if we're not allowed to block), then do a select with an
     * instantaneous timeout.  If a ready file is found, then go
     * back to process it.
     */

    if (((idleList != NULL) && (flags & TK_IDLE_EVENTS))
	    || (flags & TK_DONT_WAIT)) {
	if (flags & (TK_X_EVENTS|TK_FILE_EVENTS)) {
	    memcpy((VOID *) ready, (VOID *) masks,
		    3*MASK_SIZE*sizeof(fd_mask));
	    timeout.tv_sec = timeout.tv_usec = 0;
	    numFound = select(numFds, (SELECT_MASK *) readPtr,
		    (SELECT_MASK *) writePtr, (SELECT_MASK *) exceptPtr,
		    &timeout);
	    if (numFound == -1) {
		/*
		 * Some systems don't clear the masks after an error, so
		 * we have to do it here.
		 */

		memset((VOID *) ready, 0, 3*MASK_SIZE*sizeof(fd_mask));
	    }
	    if ((numFound > 0) || ((numFound == -1) && (errno == EINTR))) {
		goto checkFiles;
	    }
	}
    }

    /*
     * Phase Five:  process all pending DoWhenIdle requests.
     */

    if ((idleList != NULL) && (flags & TK_IDLE_EVENTS)) {
	register IdleHandler *idlePtr;
	int oldGeneration;

	oldGeneration = idleList->generation;
	idleGeneration++;

	/*
	 * The code below is trickier than it may look, for the following
	 * reasons:
	 *
	 * 1. New handlers can get added to the list while the current
	 *    one is being processed.  If new ones get added, we don't
	 *    want to process them during this pass through the list (want
	 *    to check for other work to do first).  This is implemented
	 *    using the generation number in the handler:  new handlers
	 *    will have a different generation than any of the ones currently
	 *    on the list.
	 * 2. The handler can call Tk_DoOneEvent, so we have to remove
	 *    the hander from the list before calling it. Otherwise an
	 *    infinite loop could result.
	 * 3. Tk_CancelIdleCall can be called to remove an element from
	 *    the list while a handler is executing, so the list could
	 *    change structure during the call.
	 */

	for (idlePtr = idleList;
		((idlePtr != NULL) && (idlePtr->generation == oldGeneration));
		idlePtr = idleList) {
	    idleList = idlePtr->nextPtr;
	    if (idleList == NULL) {
		lastIdlePtr = NULL;
	    }
	    (*idlePtr->proc)(idlePtr->clientData);
	    ckfree((char *) idlePtr);
	}
	return 1;
    }

    /*
     * Phase Six: do a select to wait for either one of the
     * files to become ready or for the first timer event to
     * fire.  Then go back to process the event.
     */

    if ((flags & TK_DONT_WAIT)
	    || !(flags & (TK_TIMER_EVENTS|TK_FILE_EVENTS|TK_X_EVENTS))) {
	return 0;
    }
    if ((timerQueue == NULL) || !(flags & TK_TIMER_EVENTS)) {
	timeoutPtr = NULL;
    } else {
	timeoutPtr = &timeout;
	timeout.tv_sec = timerQueue->time.tv_sec - curTime.tv_sec;
	timeout.tv_usec = timerQueue->time.tv_usec - curTime.tv_usec;
	if (timeout.tv_usec < 0) {
	    timeout.tv_sec -= 1;
	    timeout.tv_usec += 1000000;
	}
    }
    memcpy((VOID *) ready, (VOID *) masks, 3*MASK_SIZE*sizeof(fd_mask));
    numFound = select(numFds, (SELECT_MASK *) readPtr,
	    (SELECT_MASK *) writePtr, (SELECT_MASK *) exceptPtr,
	    timeoutPtr);
    if (numFound == -1) {
	/*
	 * Some systems don't clear the masks after an error, so
	 * we have to do it here.
	 */

	memset((VOID *) ready, 0, 3*MASK_SIZE*sizeof(fd_mask));
    }
    if (numFound == 0) {
	goto checkTime;
    }
    goto checkFiles;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_MainLoop --
 *
 *	Call Tk_DoOneEvent over and over again in an infinite
 *	loop as long as there exist any main windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arbitrary;  depends on handlers for events.
 *
 *--------------------------------------------------------------
 */

void
Tk_MainLoop()
{
    while (tk_NumMainWindows > 0) {
	Tk_DoOneEvent(0);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_Sleep --
 *
 *	Delay execution for the specified number of milliseconds.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

void
Tk_Sleep(ms)
    int ms;			/* Number of milliseconds to sleep. */
{
    static struct timeval delay;

    delay.tv_sec = ms/1000;
    delay.tv_usec = (ms%1000)*1000;
    (void) select(0, (SELECT_MASK *) 0, (SELECT_MASK *) 0,
	    (SELECT_MASK *) 0, &delay);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_RestrictEvents --
 *
 *	This procedure is used to globally restrict the set of events
 *	that will be dispatched.  The restriction is done by filtering
 *	all incoming X events through a procedure that determines
 *	whether they are to be processed immediately or deferred.
 *
 * Results:
 *	The return value is the previous restriction procedure in effect,
 *	if there was one, or NULL if there wasn't.
 *
 * Side effects:
 *	From now on, proc will be called to determine whether to process
 *	or defer each incoming X event.
 *
 *----------------------------------------------------------------------
 */

Tk_RestrictProc *
Tk_RestrictEvents(proc, arg, prevArgPtr)
    Tk_RestrictProc *proc;	/* X "if" procedure to call for each
				 * incoming event.  See "XIfEvent" doc.
				 * for details. */
    char *arg;			/* Arbitrary argument to pass to proc. */
    char **prevArgPtr;		/* Place to store information about previous
				 * argument. */
{
    Bool (*prev)  _ANSI_ARGS_((Display *display, XEvent *eventPtr, char *arg));

    prev = restrictProc;
    *prevArgPtr = restrictArg;
    restrictProc = proc;
    restrictArg = arg;
    return prev;
}

/*
 *--------------------------------------------------------------
 *
 * TkEventDeadWindow --
 *
 *	This procedure is invoked when it is determined that
 *	a window is dead.  It cleans up event-related information
 *	about the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Various things get cleaned up and recycled.
 *
 *--------------------------------------------------------------
 */

void
TkEventDeadWindow(winPtr)
    TkWindow *winPtr;		/* Information about the window
				 * that is being deleted. */
{
    register TkEventHandler *handlerPtr;
    register InProgress *ipPtr;

    /*
     * While deleting all the handlers, be careful to check for
     * Tk_HandleEvent being about to process one of the deleted
     * handlers.  If it is, tell it to quit (all of the handlers
     * are being deleted).
     */

    while (winPtr->handlerList != NULL) {
	handlerPtr = winPtr->handlerList;
	winPtr->handlerList = handlerPtr->nextPtr;
	for (ipPtr = pendingPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    if (ipPtr->nextHandler == handlerPtr) {
		ipPtr->nextHandler = NULL;
	    }
	    if (ipPtr->winPtr == winPtr) {
		ipPtr->winPtr = None;
	    }
	}
	ckfree((char *) handlerPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkCurrentTime --
 *
 *	Try to deduce the current time.  "Current time" means the time
 *	of the event that led to the current code being executed, which
 *	means the time in the most recently-nested invocation of
 *	Tk_HandleEvent.
 *
 * Results:
 *	The return value is the time from the current event, or
 *	CurrentTime if there is no current event or if the current
 *	event contains no time.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Time
TkCurrentTime(dispPtr)
    TkDisplay *dispPtr;		/* Display for which the time is desired. */
{
    register XEvent *eventPtr;

    if (pendingPtr == NULL) {
	return dispPtr->lastEventTime;
    }
    eventPtr = pendingPtr->eventPtr;
    switch (eventPtr->type) {
	case ButtonPress:
	case ButtonRelease:
	    return eventPtr->xbutton.time;
	case KeyPress:
	case KeyRelease:
	    return eventPtr->xkey.time;
	case MotionNotify:
	    return eventPtr->xmotion.time;
	case EnterNotify:
	case LeaveNotify:
	    return eventPtr->xcrossing.time;
	case PropertyNotify:
	    return eventPtr->xproperty.time;
    }
    return dispPtr->lastEventTime;
}

/* TCL */
int
mged_tk_dontwait()
{
	if( eventDelayed || timerQueue || idleList )  return 1;
	return 0;
}

#endif
