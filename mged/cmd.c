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

#include	<math.h>
#include	<signal.h>
#include	<stdio.h>
#include "ged_types.h"
#include "../h/db.h"
#include "sedit.h"
#include "ged.h"
#include "objdir.h"
#include "solid.h"
#include "dm.h"
#include "../h/vmath.h"

extern void	perror();
extern int	atoi(), execl(), fork(), nice(), wait();
extern long	time();

#define MAXARGS		 200	/* Maximum number of args per line */
int	maxargs = MAXARGS;	/* For dir.c */
int	inpara;			/* parameter input from keyboard */
int	numargs;		/* number of args */
char *cmd_args[MAXARGS + 1];	/* array of pointers to args */

extern int	cmd_glob();

static void	do_cmd();
void	f_help(), f_center(), f_press(), f_view(), f_blast();
void	f_edit(), f_evedit(), f_delobj();
void	f_debug(), f_regdebug(), f_name(), f_copy(), f_instance();
void	f_region(), f_itemair(), f_modify(), f_kill(), f_list();
void	f_zap(), f_group(), f_param(), f_mirror(), f_extrude();
void	f_rm(), f_arbdef(), f_comm(), f_quit();
void	f_edcomb(), f_status(), f_vrot();
void	f_refresh(), f_fix(), f_rt(), f_saveview();
void	f_make(), f_attach(), f_release();
void	f_tedit(), f_memprint();
void	f_mirface(), f_units(), f_title();
void	f_rot_obj(), f_tr_obj(), f_sc_obj();
void	f_analyze(), f_sed();
void	f_ill(), f_knob(), f_tops(), f_summary();
void	f_prcolor(), f_color(), f_edcolor();
void	f_plot(), f_area(), f_find(), f_edgedir();
void	f_regdef(), f_aeview(), f_in();

static struct funtab {
	char *ft_name;
	char *ft_parms;
	char *ft_comment;
	void (*ft_func)();
	int ft_min;
	int ft_max;
} funtab[] = {

"?", "", "help message",
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
"mv", "old new", "rename object",
	f_name,3,3,
"cp", "from to", "copy [duplicate] object",
	f_copy,3,3,
"i", "obj combination [operation]", "add instance of obj to comb",
	f_instance,3,4,
"r", "region <operation solid>", "create region",
	f_region,4,MAXARGS,
"item", "region item [air]", "change item # or air code",
	f_itemair,3,4,
"mater", "region mat los", "modify material or los",
	f_modify,4,4,
"kill", "<objects>", "delete objects from file",
	f_kill,2,MAXARGS,
"l", "<objects>", "list attributes",
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
"edcomb", "combname flag item air mat los", "edit combination record info",
	f_edcomb,7,7,
"status", "", "get view status",
	f_status, 1,1,
"fix", "", "fix display after hardware error",
	f_fix,1,1,
"refresh", "", "send new control list",
	f_refresh, 1,1,
"rt", "[options]", "do raytrace of view",
	f_rt,1,MAXARGS,
"saveview", "file [args]", "save view in file for RT",
	f_saveview,2,MAXARGS,
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
"plot", "[-zclip] [-2d] [-grid] [out_file] [|filter]", "make UNIX-plot of view",
	f_plot, 2, MAXARGS,
"area", "[endpoint_tolerance]", "calculate presented area of view",
	f_area, 1, 2,
"ae", "azim elev", "set view using az and elev angles",
	f_aeview, 3, 3,
"regdef", "item [air] [material] [los]", "change next region default codes",
	f_regdef, 2, 5,
"edgedir", "edgedir delta_x delta_y delta_z", "define direction of ARB edge being moved",
	f_edgedir, 3, 4,
"in", "", "keyboard entry of solids",
	f_in, 1, 27,
"memprint", "", "print memory maps",
	f_memprint, 1, 1
};
#define NFUNC	( (sizeof(funtab)) / (sizeof(struct funtab)) )

/*
 *			C M D L I N E
 *
 * This routine is called to process a user's command, as typed
 * on the standard input.  Once the
 * main loop of the editor is entered, this routine will be called
 * to process commands which have been typed in completely.
 */
void
cmdline()
{
	if( parse_line() )  {
		/* don't process this command line */
		return;
	}
	(void)do_cmd();
}

/*
 *			P A R S E _ L I N E
 *
 * Parse commandline into argument vector
 * Returns nonzero value if input is to be ignored
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
	(void)fgets( line, MAXLINE, stdin );

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
static void
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
		(void)printf("%s: error in number of args\n", ftp->ft_name);
		(void)printf("%s: %s\n", ftp->ft_name, ftp->ft_parms);
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
				return;
			}
		}
	}
	/* check if need to convert to the base unit */
	switch( es_edflag ) {

		case STRANS:
		case PSCALE:
		case EARB:
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
		finish( 11 );
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
	
	(void)printf("?\n");
	(void)printf("The following commands are available:\n");
	for( ftp = &funtab[0]; ftp < &funtab[NFUNC]; ftp++ )  {
		col_item(ftp->ft_name);
		col_item(ftp->ft_parms);
		col_item(ftp->ft_comment);
		col_eol();
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
