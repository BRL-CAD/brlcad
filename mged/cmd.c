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

extern void	sync();

#define	MAXARGS		2000	/* Maximum number of args per line */
int	inpara;			/* parameter input from keyboard */

extern int	cmd_glob();

int	mged_cmd();
int	f_sync();

/* XXX Move to ged.h */
MGED_EXTERN(int f_shader, (int argc, char **argv));

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
"concat", "file [prefix]", "concatenate 'file' onto end of present database.  Run 'dup file' first.",
	f_concat, 2, 3,
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
"scale", "factor", "scale object by factor",
	f_sc_obj,2,2,
"sed", "solid", "solid-edit named solid",
	f_sed,2,2,
"set",	"[var=opt]", "assign/display mged variables",
	f_set,1,2,
"shader", "comb material [arg(s)]", "assign materials (like 'mater')",
	f_shader, 3,MAXARGS,
"size", "size", "set view size",
	f_view, 2,2,
"solids", "file object(s)", "make ascii summary of solid parameters",
	f_tables, 3, MAXARGS,
"source", "file/pipe", "read and process file/pipe of commands",
	f_source, 2,MAXARGS,
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


void
cmd_setup()
{
	return;
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
		
		rt_vls_strcpy( &cmd_buf, rt_vls_addr(&cmd) );
		i = parse_line( rt_vls_addr(&cmd_buf), &argc, argv );

		while (i == 0) {
			if (mged_cmd(argc, argv, funtab) != CMD_MORE)
				break;

			/* If we get here, it means the command failed due
			   to insufficient arguments.  In this case, grab some
			   more from stdin and call the command again. */

			rt_vls_gets( &str, stdin );

			/* Remove newline */
			rt_vls_trunc( &cmd, strlen(rt_vls_addr(&cmd))-1 );

			rt_vls_strcat( &cmd, " " );
			rt_vls_vlscatzap( &cmd, &str );
			rt_vls_strcat( &cmd, "\n" );
			rt_vls_strcpy( &cmd_buf, rt_vls_addr(&cmd) );
			i = parse_line( rt_vls_addr(&cmd_buf), &argc, argv );

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
		(void)printf("!\n");
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
				(void)printf("More than %d arguments, excess flushed\n", MAXARGS);
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

	if( argc == 0 )  {
		(void)printf("no command entered, type '%s?' for help\n",
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
				printf("mged_cmd(): Invalid return from %s\n",
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
	(void)printf("!\n");

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

	if( argc <= 1 )  {
		(void)printf("The following commands are available:\n");
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
		(void)printf("The following %scommands are available:\n",
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
			(void)printf("summary:  S R or G are only valid parmaters\n");
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
		fprintf(stdout, i==1 ? "%s" : " %s", argv[i]);
	}
	fprintf(stdout, "\n");

	return CMD_OK;
}
