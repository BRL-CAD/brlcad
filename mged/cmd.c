/*
 *			C M D . C
 *
 * Functions -
 *	cmdline		Process commands typed on the keyboard
 *	parse_line	Parse command line into argument vector
 *	do_cmd		Check arg counts, run a command
 *
 * The U. S. Army Ballistic Research Laboratory
 */

#include	<math.h>
#include	<signal.h>
#include	<stdio.h>
#include "ged_types.h"
#include "3d.h"
#include "sedit.h"
#include "ged.h"
#include "dir.h"
#include "solid.h"
#include "dm.h"
#include "vmath.h"

extern void	perror();
extern int	atoi(), execl(), fork(), nice(), wait();
extern long	time();

#define MAXARGS		 20	/* Maximum number of args per line */
int	inpara;			/* parameter input from keyboard */
int	numargs;		/* number of args */
char *cmd_args[MAXARGS + 1];	/* array of pointers to args */

static void	do_cmd();
void	f_help(), f_center(), f_press(), f_view(), f_blast();
void	f_edit(), f_evedit(), f_delobj();
void	f_debug(), f_regdebug(), f_name(), f_copy(), f_instance();
void	f_region(), f_itemair(), f_modify(), f_kill(), f_list();
void	f_zap(), f_group(), f_param(), f_mirror(), f_face();
void	f_delmem(), f_arbdef(), f_return(), f_comm(), f_quit();
void	f_edcomb(), f_status(), f_rot();
void	f_refresh(), f_fix(), f_rt();

static struct funtab {
	char *ft_name;
	void (*ft_func)();
	int ft_min;
	int ft_max;
	char *ft_messages;
} funtab[] = {
	{"?",f_help,0,100,"? (help message)"},
	{"B",f_blast,2,20,"B <objects> (blast screen, edit objects)"},
	"e",f_edit,2,20,"e <objects> (edit named objects)",
	"E",f_evedit,2,20,"E <objects> (evaluated edit of objects)",
	"d",f_delobj,2,20,"d <objects> (delete list of objects)",
	"t",dir_print,1,1,"t (table of contents)",
	"mv",f_name,3,3,"mv oldname newname (rename object)",
	"cp",f_copy,3,3,"cp oldname newname (copy [duplicate] object)",
	"i",f_instance,4,5,"i object combname instname [operation] (create instance)",
	"r",f_region,4,20,"r regionname <operation solidname> (create region)",
	"item",f_itemair,3,4,"item region item [air] (change item # or air code)",
	"mater",f_modify,4,4,"mater region mat los (modify material or los)",
	"kill",f_kill,2,20,"kill <objects> (delete objects from file)",
	"l",f_list,2,2,"l object (list object attributes)",
	"Z",f_zap,1,1,"Z (zap all objects off screen)",
	"g",f_group,3,20,"g groupname <objects> (group objects)",
	"p",f_param,2,4,"p dx [dy dz] (set parameters)",
	"mirror",f_mirror,4,4,"mirror oldsolid newsolid axis",
	"extrude",f_face,3,3,"extrude #### distance (extrude face)",
	"rm",f_delmem,3,20,"rm comb <members> (remove members from comb)",
	"arb",f_arbdef,4,4,"arb name rot fb (make arb8, rotation + fallback)",
	"\n",f_return,1,1,"NOP",
	"%",f_comm,1,1,"% (escape to interactive shell)",
	"q",f_quit,1,1,"q (quit)",
	"center",f_center,4,4,"center x y z (debug, set view center)",
	"press",f_press,2,2,"press button# (debug, emulate button press)",
	"size",f_view,2,2,"size size (debug, set view size)",
	"rot",f_rot,3,3,"rot axis degrees (debug, rotate view)",
	"x",f_debug,1,1,"x (debug, list drawn objects)",
	"regdump",f_regdebug,1,1,"regdump (debug, toggle register print)",
	"edcomb",f_edcomb,7,7,"edcomb combname flag item air mat los (edit combination record info)",
	"status",f_status,1,1,"status (debug, get view status)",
	"fix",f_fix,1,1,"fix (restart display processor after hardware error)",
	"refresh",f_refresh,1,1,"refresh (debug, send new control list)",
	"rt",f_rt,1,20,"rt [options] (TEST)"
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

/* Parse commandline into argument vector */
/* Returns nonzero value if shell command */
int
parse_line()
{
#define MAXLINE		128		/* Maximum number of chars per line */
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
		quit();
		/* NOTREACHED */
	}

	cmd_args[numargs] = &line[0];

	/* Handle single "newline" so that paged Table-of-Contents works */
	if( *lp == '\n' )  {
		numargs++;
		cmd_args[numargs] = (char *)0;
		return(0);
	}

	/* Handle "!" shell escape char so the shell can parse the line */
	if( *lp == '!' )  {
		system( &line[1] );
		printf("!\n");
		return(1);		/* Don't process command line! */
	}

	/* In case first character is not "white space" */
	if( (*lp != ' ') && (*lp != '\t') && (*lp != '\0') )
		numargs++;		/* holds # of args */

	for( lp = &line[0]; *lp != '\0'; *lp++ )  {
		if( (*lp == ' ') || (*lp == '\t') || (*lp == '\n') )  {
			*lp = '\0';
			lp1 = lp + 1;
			if( (*lp1 != ' ') && (*lp1 != '\t') &&
			    (*lp1 != '\n') && (*lp1 != '\0') )  {
				if( numargs >= MAXARGS )  {
					printf("too many arguments, excess flushed\n");
					cmd_args[MAXARGS] = (char *)0;
					return(0);
				}
				cmd_args[numargs] = lp1;
				numargs++;
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
	register int i;

	if( numargs == 0 )  {
		printf("no command entered, type ? for help\n");
		return;
	}

	for( i = 0; i < NFUNC ; i++ )  {
		if( strcmp( funtab[i].ft_name, cmd_args[0] ) == 0 )  {
			/* We have a match */
			if( (funtab[i].ft_min <= numargs) &&
			    (numargs <= funtab[i].ft_max) )  {
				/* We have the right number of args */
				funtab[i].ft_func(numargs);	/* finally! */
				return;
			}
			printf("error in number of args\n");
			printf("%s\n", funtab[i].ft_messages );
			return;
		}
	}
	printf("%s: no such command, type ? for help\n", cmd_args[0] );
}

/* Input parameter editing changes from keyboard */
/* Format: p dx [dy dz]		*/
static void
f_param()
{
	register int i;

	if( es_edflag == 0 )  {
		(void)printf("A solid editor option not selected\n");
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
}

/* Let the user temporarily escape from the editor */
/* Format: %	*/
static void
f_comm()
{

	register int pid, rpid;
	int retcode;

	(void)signal( SIGINT, SIG_IGN );
	(void)signal( SIGQUIT, SIG_IGN );
	if ( ( pid = fork()) == 0 )  {
#ifdef	pdp11
		/* The PDP-11 VG device driver nices up the process. */
		(void)nice( 10 );
#endif
		(void)signal( SIGINT, SIG_DFL );
		(void)signal( SIGQUIT, SIG_DFL );
		(void)execl("/bin/sh","-",(char *)NULL);
		perror("/bin/sh");
		finish( 11 );
	}
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;
	(void)signal(SIGINT, quit);
	(void)signal(SIGQUIT, sig3);

	(void)printf("!\n");
}

/* Quit and exit gracefully */
/* Format: q	*/
static void
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
	int i;
	
	(void)printf("?\n");
	(void)printf("The following commands are available:\n");
	for( i = 0; i < NFUNC; i++ )  {
		(void)printf("%s\n", funtab[i].ft_messages );
	}
	(void)printf("\n");
}

/* Carriage return hook */
static void
f_return()
{
	/* presently unused */
}
