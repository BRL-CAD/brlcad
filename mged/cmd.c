/*
 *			C M D . C
 *
 * Functions -
 *	cmdline		Process commands typed on the keyboard
 *	parse_line	Parse command line into argument vector
 *	f_press		hook for displays with no buttons
 *	f_summary	do directory summary
 *	do_cmd		Check arg counts, run a command
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

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "./sedit.h"
#include "./ged.h"
#include "./objdir.h"
#include "./solid.h"
#include "./dm.h"

extern void	perror();
extern int	atoi(), execl(), fork(), nice(), wait();
extern long	time();
extern void	sync();

#define MAXARGS		 200	/* Maximum number of args per line */
int	maxargs = MAXARGS;	/* For dir.c */
int	inpara;			/* parameter input from keyboard */
int	numargs;		/* number of args */
char *cmd_args[MAXARGS + 1];	/* array of pointers to args */

extern int	cmd_glob();

static void	f_help(), f_param(), f_comm();
void	do_cmd();
void	f_center(), f_press(), f_view(), f_blast();
void	f_edit(), f_evedit(), f_delobj();
void	f_debug(), f_regdebug(), f_name(), f_copy(), f_instance();
void	f_copy_inv(), f_killall(), f_killtree();
void	f_region(), f_itemair(), f_mater(), f_kill(), f_list();
void	f_zap(), f_group(), f_mirror(), f_extrude();
void	f_rm(), f_arbdef(), f_quit();
void	f_edcomb(), f_status(), f_vrot();
void	f_refresh(), f_fix(), f_rt(), f_rrt();
void	f_saveview(), f_savekey();
void	f_make(), f_attach(), f_release();
void	f_tedit(), f_memprint();
void	f_mirface(), f_units(), f_title();
void	f_rot_obj(), f_tr_obj(), f_sc_obj();
void	f_analyze(), f_sed();
void	f_ill(), f_knob(), f_tops(), f_summary();
void	f_prcolor(), f_color(), f_edcolor(), f_3ptarb(), f_rfarb(), f_which_id();
void	f_plot(), f_area(), f_find(), f_edgedir();
void	f_regdef(), f_aeview(), f_in(), f_tables(), f_edcodes(), f_dup(), f_cat();
void	f_rmats(),f_prefix(), f_keep(), f_tree(), f_inside(), f_mvall(), f_amtrack();
void	f_tabobj(), f_pathsum(), f_copyeval(), f_push(), f_facedef(), f_eqn();
void	f_overlay(), f_rtcheck();

static struct funtab {
	char *ft_name;
	char *ft_parms;
	char *ft_comment;
	void (*ft_func)();
	int ft_min;
	int ft_max;
} funtab[] = {

"?", "", "summary of available commands",
	f_help,0,MAXARGS,
"help", "[commands]", "give usage message for given commands",
	f_help,0,MAXARGS,
"e", "<objects>", "edit objects",
	f_edit,2,MAXARGS,
"B", "<objects>", "clear screen, edit objects",
	f_blast,2,MAXARGS,
"E", "<objects>", "evaluated edit of objects",
	f_evedit,2,MAXARGS,
"d", "<objects>", "delete list of objects",
	f_delobj,2,MAXARGS,
"t", "", "table of contents",
	dir_print,1,MAXARGS,
"ls", "", "table of contents",
	dir_print,1,MAXARGS,
"mv", "old new", "rename object",
	f_name,3,3,
"cp", "from to", "copy [duplicate] object",
	f_copy,3,3,
"cpi", "from to", "copy cylinder and position at end of original cylinder",
	f_copy_inv,3,3,
"i", "obj combination [operation]", "add instance of obj to comb",
	f_instance,3,4,
"r", "region <operation solid>", "create region",
	f_region,4,MAXARGS,
"item", "region item [air]", "change item # or air code",
	f_itemair,3,4,
"mater", "comb [material]", "assign/delete material to combination",
	f_mater,2,3,
"kill", "<objects>", "delete objects from file",
	f_kill,2,MAXARGS,
"l", "<objects>", "list attributes",
	f_list,2,MAXARGS,
"cat", "<objects>", "list attributes",
	f_list,2,MAXARGS,
"find", "<objects>", "find all references to objects",
	f_find, 1, MAXARGS,
"Z", "", "zap all objects off screen",
	f_zap,1,1,
"g", "groupname <objects>", "group objects",
	f_group,3,MAXARGS,
"p", "dx [dy dz]", "set parameters",
	f_param,2,4,
"mirror", "old new axis", "Arb mirror ??",
	f_mirror,4,4,
"mirface", "#### axis", "mirror an ARB face",
	f_mirface,3,3,
"extrude", "#### distance", "extrude dist from face",
	f_extrude,3,3,
"rm", "comb <members>", "remove members from comb",
	f_rm,3,MAXARGS,
"arb", "name rot fb", "make arb8, rotation + fallback",
	f_arbdef,4,4,
"units", "<mm|cm|m|in|ft>", "change units",
	f_units,2,2,
"title", "string", "change the title",
	f_title,2,MAXARGS,
"rotobj", "xdeg ydeg zdeg", "rotate object being edited",
	f_rot_obj, 4, 4,
"vrot", "xdeg ydeg zdeg", "rotate viewpoint",
	f_vrot,4,4,
"translate", "x y z", "trans object to x,y, z",
	f_tr_obj,4,4,
"scale", "factor", "scale object by factor",
	f_sc_obj,2,2,
"analyze", "[arbname]", "analyze faces of ARB",
	f_analyze,1,MAXARGS,
"ill", "name", "illuminate object",
	f_ill,2,2,
"sed", "solid", "solid-edit named solid",
	f_sed,2,2,
"%", "", "escape to interactive shell",
	f_comm,1,1,
"q", "", "quit",
	f_quit,1,1,
"center", "x y z", "set view center",
	f_center, 4,4,
"press", "button_label", "emulate button press",
	f_press,2,MAXARGS,
"knob", "id val", "emulate knob twist",
	f_knob,3,3,
"size", "size", "set view size",
	f_view, 2,2,
"x", "", "list drawn objects",
	f_debug, 1,1,
"regdebug", "", "toggle register print",
	f_regdebug, 1,2,
"edcomb", "combname Regionflag regionid air los [GIFTmater]", "edit combination record info",
	f_edcomb,6,7,
"status", "", "get view status",
	f_status, 1,1,
"fix", "", "fix display after hardware error",
	f_fix,1,1,
"refresh", "", "send new control list",
	f_refresh, 1,1,
"rt", "[options]", "do raytrace of view",
	f_rt,1,MAXARGS,
"rrt", "prog [options]", "invoke prog with view",
	f_rrt,2,MAXARGS,
"saveview", "file [args]", "save view in file for RT",
	f_saveview,2,MAXARGS,
"rmats", "file", "load views from file (experimental)",
	f_rmats,2,MAXARGS,
"savekey", "file [time]", "save keyframe in file (experimental)",
	f_savekey,2,MAXARGS,
"attach", "<device>", "attach to a display processor, or NU",
	f_attach,2,2,
"release", "", "release current display processor [attach NU]",
	f_release,1,1,
"ted", "", "text edit a solid's parameters",
	f_tedit,1,1,
"make", "name <arb8|sph|ellg|tor|tgc>", "create a primitive",
	f_make,3,3,
"tops", "", "find all top level objects",
	f_tops,1,1,
"summary", "[s r g]", "count/list solid/reg/groups",
	f_summary,1,2,
"prcolor", "", "print color&material table",
	f_prcolor, 1, 1,
"color", "low high r g b str", "make color entry",
	f_color, 7, 7,
"edcolor", "", "text edit color table",
	f_edcolor, 1, 1,
"plot", "[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]", "make UNIX-plot of view",
	f_plot, 2, MAXARGS,
"area", "[endpoint_tolerance]", "calculate presented area of view",
	f_area, 1, 2,
"ae", "azim elev", "set view using az and elev angles",
	f_aeview, 3, 3,
"regdef", "item [air] [los] [GIFTmaterial]", "change next region default codes",
	f_regdef, 2, 5,
"edgedir", "[delta_x delta_y delta_z]|[rot fb]", "define direction of ARB edge being moved",
	f_edgedir, 3, 4,
"in", "", "keyboard entry of solids",
	f_in, 1, 27,
"solids", "file object(s)", "make ascii summary of solid parameters",
	f_tables, 3, MAXARGS,
"regions", "file object(s)", "make ascii summary of regions",
	f_tables, 3, MAXARGS,
"idents", "file object(s)", "make ascii summary of region idents",
	f_tables, 3, MAXARGS,
"edcodes", "object(s)", "edit region ident codes",
	f_edcodes, 2, MAXARGS,
"dup", "file [prefix]", "check for dup names in 'file'",
	f_dup, 1, 27,
"concat", "file [prefix]", "concatenate 'file' onto end of present database",
	f_cat, 1, 27,
"prefix", "new_prefix object(s)", "prefix each occurrence of object name(s)",
	f_prefix, 3, MAXARGS,
"keep", "keep_file object(s)", "save named objects in specified file",
	f_keep, 3, MAXARGS,
"tree",	"object(s)", "print out a tree of all members of an object",
	f_tree, 2, MAXARGS,
"inside", "", "finds inside solid per specified thicknesses",
	f_inside, 1, MAXARGS,
"mvall", "oldname newname", "rename object everywhere",
	f_mvall, 3, 3,
"track", "<parameters>", "adds tracks to database",
	f_amtrack, 1, 27,
"3ptarb", "", "makes arb given 3 pts, 2 coord of 4th pt, and thickness",
	f_3ptarb, 1, 27,
"rfarb", "", "makes arb given point, 2 coord of 3 pts, rot, fb, thickness",
	f_rfarb, 1, 27,
"whichid", "ident(s)", "lists all regions with given ident code",
	f_which_id, 1, 27,
"paths", "", "lists all paths matching input path",
	f_pathsum, 1, 27,
"listeval", "", "lists 'evaluated' path solids",
	f_pathsum, 1, 27,
"copyeval", "", "copys an 'evaluated' path solid",
	f_copyeval, 1, 27,
"tab", "object[s]", "tabs objects as stored in database",
	f_tabobj, 2, MAXARGS,
"push", "object[s]", "pushes object's path transformations to solids",
	f_push, 2, MAXARGS,
"killall", "object[s]", "kill object[s] and all references",
	f_killall, 2, MAXARGS,
"killtree", "object[s]", "kill complete tree[s] - BE CAREFUL",
	f_killtree, 2, MAXARGS,
"memprint", "", "print memory maps",
	f_memprint, 1, 1,
"facedef", "####", "define new face for an arb",
	f_facedef, 2, MAXARGS,
"sync",	"",	"forces UNIX sync",
	sync, 1, 1,
"eqn", "A B C", "planar equation coefficients",
	f_eqn, 4, 4,
"overlay", "file.plot [name]", "Read UNIX-Plot as named overlay",
	f_overlay, 2, 3,
"rtcheck", "[options]", "check for overlaps in current view",
	f_rtcheck,1,MAXARGS
};
#define NFUNC	( (sizeof(funtab)) / (sizeof(struct funtab)) )

/*
 *			C M D L I N E
 *
 * This routine is called to process a user's command, as typed
 * on the standard input.  Once the
 * main loop of the editor is entered, this routine will be called
 * to process commands which have been typed in completely.
 * Return value non-zero means to print a prompt.  This is needed
 * when non-blocking I/O is used instead of select.
 */
int
cmdline()
{
	int	i;

	i = parse_line();
	if( i == 0 ) {
		do_cmd();
		return 1;
	}
	if( i < 0 )
		return 0;

	return 1;
}

/*
 *			P A R S E _ L I N E
 *
 * Parse commandline into argument vector
 * Returns nonzero value if input is to be ignored
 * Returns less than zero if there is no input to read.
 */
int
parse_line()
{
#define MAXLINE		512		/* Maximum number of chars per line */
	register char *lp;
	register char *lp1;
	static char line[MAXLINE];

	numargs = 0;
	lp = &line[0];
	*lp = '\0';

	/* Read input line */
	if( fgets( line, MAXLINE, stdin ) == NULL ) {
		if( !feof( stdin ) )
			return -1;	/* nothing to read */
	}

	/* Check for Control-D (EOF) */
	if( feof( stdin ) )  {
		/* Control-D typed, let's hit the road */
		f_quit();
		/* NOTREACHED */
	}

	cmd_args[numargs] = &line[0];

	if( *lp == '\n' )
		return(1);		/* NOP */

	/* Handle "!" shell escape char so the shell can parse the line */
	if( *lp == '!' )  {
		(void)system( &line[1] );
		(void)printf("!\n");
		return(1);		/* Don't process command line! */
	}

	/* In case first character is not "white space" */
	if( (*lp != ' ') && (*lp != '\t') && (*lp != '\0') )
		numargs++;		/* holds # of args */

	for( lp = &line[0]; *lp != '\0'; lp++ )  {
		if( (*lp == ' ') || (*lp == '\t') || (*lp == '\n') )  {
			*lp = '\0';
			lp1 = lp + 1;
			if( (*lp1 != ' ') && (*lp1 != '\t') &&
			    (*lp1 != '\n') && (*lp1 != '\0') )  {
				if( numargs >= MAXARGS )  {
					(void)printf("More than %d arguments, excess flushed\n", MAXARGS);
					cmd_args[MAXARGS] = (char *)0;
					return(0);
				}
				cmd_args[numargs] = lp1;
				/* If not cmd [0], check for regular exp */
				if( numargs++ > 0 )
					(void)cmd_glob();
			}
		}
		/* Finally, a non-space char */
	}
	/* Null terminate pointer array */
	cmd_args[numargs] = (char *)0;
	return(0);
}

/*
 *  Check a table for the command, check for the correct
 *  minimum and maximum number of arguments, and pass control
 *  to the proper function.  If the number of arguments is
 *  incorrect, print out a short help message.
 */
void
do_cmd()
{
	extern char *cmd_args[];
	extern int numargs;
	register struct funtab *ftp;

	if( numargs == 0 )  {
		(void)printf("no command entered, type ? for help\n");
		return;
	}

	for( ftp = &funtab[0]; ftp < &funtab[NFUNC]; ftp++ )  {
		if( strcmp( ftp->ft_name, cmd_args[0] ) != 0 )
			continue;
		/* We have a match */
		if( (ftp->ft_min <= numargs) &&
		    (numargs <= ftp->ft_max) )  {
			/* We have the right number of args */
			ftp->ft_func(numargs);	/* finally! */
			return;
		}
		(void)printf("Usage: %s %s\n", ftp->ft_name, ftp->ft_parms);
		(void)printf("\t(%s)\n", ftp->ft_comment);
		return;
	}
	(void)printf("%s: no such command, type ? for help\n", cmd_args[0] );
}

/* Input parameter editing changes from keyboard */
/* Format: p dx [dy dz]		*/
static void
f_param()
{
	register int i;

	if( es_edflag <= 0 )  {
		(void)printf("A solid editor option not selected\n");
		return;
	}
	if( es_edflag == PROT ) {
		(void)printf("\"p\" command not defined for this option\n");
		return;
	}

	inpara = 1;
	sedraw++;
	for( i = 1; i < numargs; i++ )  {
		es_para[ i - 1 ] = atof( cmd_args[i] );
		if( es_edflag == PSCALE ||
					es_edflag == SSCALE )  {
			if(es_para[0] <= 0.0) {
				(void)printf("ERROR: SCALE FACTOR <= 0\n");
				inpara = 0;
				sedraw = 0;
				return;
			}
		}
	}
	/* check if need to convert to the base unit */
	switch( es_edflag ) {

		case STRANS:
		case PSCALE:
		case EARB:
		case MVFACE:
		case MOVEH:
		case MOVEHH:
		case PTARB:
			/* must convert to base units */
			es_para[0] *= local2base;
			es_para[1] *= local2base;
			es_para[2] *= local2base;

		default:
			return;
	}
}

/* Let the user temporarily escape from the editor */
/* Format: %	*/
static void
f_comm()
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
}

/* Quit and exit gracefully */
/* Format: q	*/
void
f_quit()
{
	if( state != ST_VIEW )
		button( BE_REJECT );
	quit();			/* Exiting time */
	/* NOTREACHED */
}

/* Print a help message */
static void
f_help()
{
	register struct funtab *ftp;
	register int	i;

	if( numargs <= 1 )  {
		(void)printf("The following commands are available:\n");
		for( ftp = &funtab[0]; ftp < &funtab[NFUNC]; ftp++ )  {
			col_item(ftp->ft_name);
		}
		col_eol();
		return;
	}
	/* Help command(s) */
	for( i=1; i<numargs; i++ )  {
		for( ftp = &funtab[0]; ftp < &funtab[NFUNC]; ftp++ )  {
			if( strcmp( ftp->ft_name, cmd_args[i] ) != 0 )
				continue;
			(void)printf("Usage: %s %s\n", ftp->ft_name, ftp->ft_parms);
			(void)printf("\t(%s)\n", ftp->ft_comment);
			break;
		}
		if( ftp == &funtab[NFUNC] )
			(void)printf("%s: no such command, type ? for help\n", cmd_args[i] );
	}
}

/* Hook for displays with no buttons */
void
f_press()
{
	register int i;

	for( i = 1; i < numargs; i++ )
		press( cmd_args[i] );
}

void
f_summary()
{
	register char *cp;
	int flags = 0;

	if( numargs <= 1 )  {
		dir_summary(0);
		return;
	}
	cp = cmd_args[1];
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
			break;
	}
	dir_summary(flags);
}
