/*
 *			G E D . C
 *
 * Mainline portion of the graphics editor
 *
 *  Functions -
 *	pr_prompt	print prompt
 *	main		Mainline portion of the graphics editor
 *	refresh		Internal routine to perform displaylist output writing
 *	usejoy		Apply joystick to viewing perspective
 *	setview		Set the current view
 *	slewview	Slew the view
 *	log_event	Log an event in the log file
 *	mged_finish	Terminate with logging.  To be used instead of exit().
 *	quit		General Exit routine
 *	sig2		user interrupt catcher
 *	new_mats	derive inverse and editing matrices, as required
 *
 *  Authors -
 *	Michael John Muuss
 *	Charles M Kennedy
 *	Douglas A Gwyn
 *	Bob Suckling
 *	Gary Steven Moss
 *	Earl P Weaver
 *	Phil Dykstra
 *	Bob Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

char MGEDCopyRight_Notice[] = "@(#) \
Re-distribution of this software is restricted, as described in \
your 'Statement of Terms and Conditions for the Release of \
The BRL-CAD Pacakge' agreement. \
This software is Copyright (C) 1985,1987,1990,1993 by the United States Army \
in all countries except the USA.  All rights reserved.";

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>

#include "tcl.h"
#include "tk.h"

/* defined in cmd.c */
extern Tcl_Interp *interp;
extern Tk_Window tkwin;

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./mged_solid.h"
#include "./sedit.h"
#include "./mged_dm.h"

#ifndef	LOGFILE
#define LOGFILE	"/vld/lib/gedlog"	/* usage log */
#endif

int dm_pipe[2];
struct db_i *dbip = DBI_NULL;	/* database instance pointer */

int    update_views;
extern struct dm dm_Null;
extern struct _mged_variables default_mged_variables;

double		frametime = 1.0;	/* time needed to draw last frame */
mat_t		ModelDelta;		/* Changes to Viewrot this frame */

int		(*cmdline_hook)() = NULL;
void		(*viewpoint_hook)() = NULL;

static int	windowbounds[6];	/* X hi,lo;  Y hi,lo;  Z hi,lo */

jmp_buf	jmp_env;		/* For non-local gotos */
int             cmd_stuff_str();
void		(*cur_sigint)();	/* Current SIGINT status */
void		sig2(), sig3();
void		new_mats();
void		usejoy();
void            slewview();
int		interactive = 0;	/* >0 means interactive */
int             cbreak_mode = 0;        /* >0 means in cbreak_mode */

static struct bu_vls input_str, scratchline, input_str_prefix;
static int input_str_index = 0;

static int	do_rc();
static void	log_event();
extern char	version[];		/* from vers.c */

struct rt_tol	mged_tol;		/* calculation tolerance */

static char *units_str[] = {
	"none",
	"mm",
	"cm",
	"meters",
	"inches",
	"feet",
	"extra"
};

struct bu_vls mged_prompt;
void pr_prompt(), pr_beep();

#ifdef USE_PROTOTYPES
Tcl_FileProc stdin_input;
#else
void stdin_input();
#endif

/* 
 *			M A I N
 */

int
main(argc,argv)
int argc;
char **argv;
{
	int	rateflag = 0;

	/* Check for proper invocation */
	if( argc < 2 )  {
		fprintf(stdout, "Usage:  %s database [command]\n", argv[0]);
		fflush(stdout);
		return(1);		/* NOT finish() */
	}

	/* Identify ourselves if interactive */
	if( argc == 2 )  {
		if( isatty(fileno(stdin)) )
			interactive = 1;

		fprintf(stdout, "%s\n", version+5);	/* skip @(#) */
		fflush(stdout);
		
		if (isatty(fileno(stdin)) && isatty(fileno(stdout))) {
#ifndef COMMAND_LINE_EDITING
#define COMMAND_LINE_EDITING 1
#endif
 		    cbreak_mode = COMMAND_LINE_EDITING;
		}

	    }

	/* Set up for character-at-a-time terminal IO. */
	if (cbreak_mode) 
	    save_Tty(fileno(stdin));
	
	(void)signal( SIGPIPE, SIG_IGN );

	/*
	 *  Sample and hold current SIGINT setting, so any commands that
	 *  might be run (e.g., by .mgedrc) which establish cur_sigint
	 *  as their signal handler get the initial behavior.
	 *  This will change after setjmp() is called, below.
	 */
	cur_sigint = signal( SIGINT, SIG_IGN );		/* sample */
	(void)signal( SIGINT, cur_sigint );		/* restore */

	/* If multiple processors might be used, initialize for it.
	 * Do not run any commands before here.
	 * Do not use bu_log() or bu_malloc() before here.
	 */
	if( bu_avail_cpus() > 1 )  {
		rt_g.rtg_parallel = 1;
		RES_INIT( &rt_g.res_syscall );
		RES_INIT( &rt_g.res_worker );
		RES_INIT( &rt_g.res_stats );
		RES_INIT( &rt_g.res_results );
		RES_INIT( &rt_g.res_model );
	}

	/* Set up linked lists */
	BU_LIST_INIT(&HeadSolid.l);
	BU_LIST_INIT(&FreeSolid.l);
	BU_LIST_INIT( &rt_g.rtg_vlfree );

	bzero((void *)&head_cmd_list, sizeof(struct cmd_list));
	BU_LIST_INIT(&head_cmd_list.l);
	bu_vls_init(&head_cmd_list.name);
	bu_vls_strcpy(&head_cmd_list.name, "mged");
	curr_cmd_list = &head_cmd_list;

	bzero((void *)&head_dm_list, sizeof(struct dm_list));
	BU_LIST_INIT( &head_dm_list.l );
	head_dm_list._dmp = &dm_Null;
	curr_dm_list = &head_dm_list;
	curr_dm_list->s_info = (struct shared_info *)bu_malloc(sizeof(struct shared_info),
							       "shared_info");
	bzero((void *)curr_dm_list->s_info, sizeof(struct shared_info));
#if 1
	bcopy((void *)&default_mged_variables, (void *)&mged_variables,
	      sizeof(struct _mged_variables));
#else
	mged_variables = default_mged_variables;
#endif
	bu_vls_init(&pathName);
	bu_vls_strcpy(&pathName, "nu");
	rc = 1;
	owner = 1;

	state = ST_VIEW;
	es_edflag = -1;
	inpara = newedge = 0;

	mat_idn( identity );		/* Handy to have around */
	/* init rotation matrix */
	Viewscale = 500;		/* => viewsize of 1000mm (1m) */
	mat_idn( Viewrot );
	mat_idn( toViewcenter );
	mat_idn( modelchanges );
	mat_idn( ModelDelta );
	mat_idn( acc_rot_sol );

	/* These values match old GED.  Use 'tol' command to change them. */
	mged_tol.magic = RT_TOL_MAGIC;
	mged_tol.dist = 0.005;
	mged_tol.dist_sq = mged_tol.dist * mged_tol.dist;
	mged_tol.perp = 1e-6;
	mged_tol.para = 1 - mged_tol.perp;

	rt_prep_timer();		/* Initialize timer */

	new_mats();

	setview( 0.0, 0.0, 0.0 );

	no_memory = 0;		/* memory left */
	es_edflag = -1;		/* no solid editing just now */

	bu_vls_init( &curr_cmd_list->more_default );
	bu_vls_init(&input_str);
	bu_vls_init(&input_str_prefix);
	bu_vls_init(&scratchline);
	bu_vls_init(&mged_prompt);
	input_str_index = 0;

	/* Get set up to use Tcl */
	mged_setup();

	windowbounds[0] = XMAX;		/* XHR */
	windowbounds[1] = XMIN;		/* XLR */
	windowbounds[2] = YMAX;		/* YHR */
	windowbounds[3] = YMIN;		/* YLR */
	windowbounds[4] = 2047;		/* ZHR */
	windowbounds[5] = -2048;	/* ZLR */

	/* Open the database, attach a display manager */
	if(f_opendb( (ClientData)NULL, interp, argc, argv ) == TCL_ERROR)
	  mged_finish(1);
	else
	  bu_log("%s", interp->result);

#if 0
	dmp->dm_setWinBounds(windowbounds);
#endif

	/* --- Now safe to process commands. --- */
	if( interactive )  {
		/* This is an interactive mged, process .mgedrc */
		do_rc();
	}

	/* --- Now safe to process geometry. --- */

	/* If this is an argv[] invocation, do it now */
	if( argc > 2 )  {
	  char *av[2];

	  av[0] = "q";
	  av[1] = NULL;

	  /*
	    Call cmdline instead of calling mged_cmd directly
	    so that access to Tcl/Tk is possible.
	    */
	  for(argc -= 2, argv += 2; argc; --argc, ++argv)
	    bu_vls_printf(&input_str, "%s ", *argv);

	  cmdline(&input_str, TRUE);
	  bu_vls_free(&input_str);

	  f_quit((ClientData)NULL, interp, 1, av);
	  /* NOTREACHED */
	}

#if 0
	refresh();			/* Put up faceplate */
#else
	/* Force mged_display variables to be initialized */
	dotitles(1);
#endif

#if 0
	/* Reset the lights */
	dmp->dm_light( dmp, LIGHT_RESET, 0 );
#endif

	(void)pipe(dm_pipe);
	Tcl_CreateFileHandler(Tcl_GetFile((ClientData)STDIN_FILENO, TCL_UNIX_FD),
			      TCL_READABLE, stdin_input,
			     (ClientData)STDIN_FILENO);
	Tcl_CreateFileHandler(Tcl_GetFile((ClientData)dm_pipe[0], TCL_UNIX_FD),
			      TCL_READABLE, stdin_input,
			     (ClientData)dm_pipe[0]);

	(void)signal( SIGINT, SIG_IGN );

	bu_vls_strcpy(&mged_prompt, MGED_PROMPT);
	pr_prompt();

	/****************  M A I N   L O O P   *********************/

	if (cbreak_mode) {
	    set_Cbreak(fileno(stdin));
	    clr_Echo(fileno(stdin));
	}

	while(1) {
		/* This test stops optimizers from complaining about an infinite loop */
		if( (rateflag = event_check( rateflag )) < 0 )  break;
#if 0
		/* apply solid editing changes if necessary */
		if( sedraw > 0) {
			sedit();
#if 1
			sedraw = 0;
			dmaflag = 1;
#endif
		}
#endif
		/*
		 * Cause the control portion of the displaylist to be
		 * updated to reflect the changes made above.
		 */
		refresh();
	}
	return(0);
}

void
pr_prompt()
{
	if( interactive )
		bu_log("%S", &mged_prompt);
}

void
pr_beep()
{
    bu_log("%c", 7);
}

/*
 * standard input handling
 *
 * When the Tk event handler sees input on standard input, it calls the
 * routine "stdin_input" (registered with the Tcl_CreateFileHandler call).
 * This routine simply appends the new input to a growing string until the
 * command is complete (it is assumed that the routine gets a fill line.)
 *
 * If the command is incomplete, then allow the user to hit ^C to start over,
 * by setting up the multi_line_sig routine as the SIGINT handler.
 */

extern struct bu_vls *history_prev(), *history_cur(), *history_next();

/*
 * stdin_input
 *
 * Called when a single character is ready for reading on standard input
 * (or an entire line if the terminal is not in cbreak mode.)
 */

void
stdin_input(clientData, mask)
ClientData clientData;
int mask;
{
    int count;
    char ch;
    struct bu_vls *vp;
    struct bu_vls temp;
    static int escaped = 0;
    static int bracketed = 0;
    static int freshline = 1;
    long fd;

    fd = (long)clientData;

    /* When not in cbreak mode, just process an entire line of input, and
       don't do any command-line manipulation. */

    if (!cbreak_mode) {
	bu_vls_init(&temp);

	/* Get line from stdin */
	if( bu_vls_gets(&temp, stdin) < 0 )
    		quit();				/* does not return */
	bu_vls_vlscat(&input_str, &temp);

	/* If there are any characters already in the command string (left
	   over from a CMD_MORE), then prepend them to the new input. */

	/* If no input and a default is supplied then use it */
	if(!bu_vls_strlen(&input_str) && bu_vls_strlen(&curr_cmd_list->more_default))
	  bu_vls_printf(&input_str_prefix, "%s%S\n",
			bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
			&curr_cmd_list->more_default);
	else
	  bu_vls_printf(&input_str_prefix, "%s%S\n",
			bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
			&input_str);

	bu_vls_trunc(&curr_cmd_list->more_default, 0);

	/* If a complete line was entered, attempt to execute command. */
	
	if (Tcl_CommandComplete(bu_vls_addr(&input_str_prefix))) {
	    curr_cmd_list = &head_cmd_list;
	    if(curr_cmd_list->aim)
	      curr_dm_list = curr_cmd_list->aim;
	    if (cmdline_hook != NULL) {
		if ((*cmdline_hook)(&input_str))
		    pr_prompt();
		bu_vls_trunc(&input_str, 0);
		bu_vls_trunc(&input_str_prefix, 0);
		(void)signal( SIGINT, SIG_IGN );
	    } else {
		if (cmdline(&input_str_prefix, TRUE) == CMD_MORE) {
		    /* Remove newline */
		    bu_vls_trunc(&input_str_prefix,
				 bu_vls_strlen(&input_str_prefix)-1);
		    bu_vls_trunc(&input_str, 0);

		    (void)signal( SIGINT, sig2 );
		} else {
		    bu_vls_trunc(&input_str_prefix, 0);
		    bu_vls_trunc(&input_str, 0);
		    (void)signal( SIGINT, SIG_IGN );
		}
		pr_prompt();
	    }
	    input_str_index = 0;
	    freshline = 1;
	} else {
	    bu_vls_trunc(&input_str, 0);
	    /* Allow the user to hit ^C. */
	    (void)signal( SIGINT, sig2 );
	}
	bu_vls_free(&temp);
	return;
    }

    /* Grab single character from stdin */

    count = read((int)fd, (void *)&ch, 1);
    if (count <= 0 && feof(stdin)){
      char *av[2];

      av[0] = "q";
      av[1] = NULL;

      f_quit((ClientData)NULL, interp, 1, av);
    }

    /* Process character */
#define CTRL_A      1
#define CTRL_B      2
#define CTRL_D      4
#define CTRL_E      5
#define CTRL_F      6
#define CTRL_K      11
#define CTRL_L      12
#define CTRL_N      14
#define CTRL_P      16
#define CTRL_T      20
#define CTRL_U      21
#define ESC         27
#define BACKSPACE   '\b'
#define DELETE      127

#define SPACES "                                                                                                                                                                                                                                                                                                           "

    /* ANSI arrow keys */
    
    if (escaped && bracketed) {
	if (ch == 'A') ch = CTRL_P;
	if (ch == 'B') ch = CTRL_N;
	if (ch == 'C') ch = CTRL_F;
	if (ch == 'D') ch = CTRL_B;
	escaped = bracketed = 0;
    }

    switch (ch) {
    case ESC:           /* Used for building up ANSI arrow keys */
	escaped = 1;
	break;
    case '\n':          /* Carriage return or line feed */
    case '\r':
	bu_log("\n");   /* Display newline */

	/* If there are any characters already in the command string (left
	   over from a CMD_MORE), then prepend them to the new input. */

	/* If no input and a default is supplied then use it */
	if(!bu_vls_strlen(&input_str) && bu_vls_strlen(&curr_cmd_list->more_default))
	  bu_vls_printf(&input_str_prefix, "%s%S\n",
			bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
			&curr_cmd_list->more_default);
	else
	  bu_vls_printf(&input_str_prefix, "%s%S\n",
			bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
			&input_str);

	bu_vls_trunc(&curr_cmd_list->more_default, 0);

	/* If this forms a complete command (as far as the Tcl parser is
	   concerned) then execute it. */
	
	if (Tcl_CommandComplete(bu_vls_addr(&input_str_prefix))) {
	    curr_cmd_list = &head_cmd_list;
	    if(curr_cmd_list->aim)
	      curr_dm_list = curr_cmd_list->aim;
	    if (cmdline_hook) {  /* Command-line hooks don't do CMD_MORE */
		reset_Tty(fileno(stdin));

		if ((*cmdline_hook)(&input_str_prefix))
		    pr_prompt();

		set_Cbreak(fileno(stdin));
		clr_Echo(fileno(stdin));

		bu_vls_trunc(&input_str, 0);
		bu_vls_trunc(&input_str_prefix, 0);
		(void)signal( SIGINT, SIG_IGN );
	    } else {
		reset_Tty(fileno(stdin)); /* Backwards compatibility */
		(void)signal( SIGINT, SIG_IGN );
		if (cmdline(&input_str_prefix, TRUE) == CMD_MORE) {
		    /* Remove newline */
		    bu_vls_trunc(&input_str_prefix,
				 bu_vls_strlen(&input_str_prefix)-1);
		    bu_vls_trunc(&input_str, 0);
		    (void)signal( SIGINT, sig2 );
       /* *** The mged_prompt vls now contains prompt for more input. *** */
		} else {
		    /* All done; clear all strings. */
		    bu_vls_trunc(&input_str_prefix, 0);
		    bu_vls_trunc(&input_str, 0);
		    (void)signal( SIGINT, SIG_IGN );
		}
		set_Cbreak(fileno(stdin)); /* Back to single-character mode */
		clr_Echo(fileno(stdin));
	    }
	} else {
	    bu_vls_trunc(&input_str, 0);
	    bu_vls_strcpy(&mged_prompt, "\r? ");

	    /* Allow the user to hit ^C */
	    (void)signal( SIGINT, sig2 );
	}
	pr_prompt(); /* Print prompt for more input */
	input_str_index = 0;
	freshline = 1;
	escaped = bracketed = 0;
	break;
    case BACKSPACE:
    case DELETE:
	if (input_str_index <= 0) {
	    pr_beep();
	    break;
	}

	if (input_str_index == bu_vls_strlen(&input_str)) {
	    bu_log("\b \b");
	    bu_vls_trunc(&input_str, bu_vls_strlen(&input_str)-1);
	} else {
	    bu_vls_init(&temp);
	    bu_vls_strcat(&temp, bu_vls_addr(&input_str)+input_str_index);
	    bu_vls_trunc(&input_str, input_str_index-1);
	    bu_log("\b%S ", &temp);
	    pr_prompt();
	    bu_log("%S", &input_str);
	    bu_vls_vlscat(&input_str, &temp);
	    bu_vls_free(&temp);
	}
	--input_str_index;
	escaped = bracketed = 0;
	break;
    case CTRL_A:                    /* Go to beginning of line */
	pr_prompt();
	input_str_index = 0;
	escaped = bracketed = 0;
	break;
    case CTRL_E:                    /* Go to end of line */
	if (input_str_index < bu_vls_strlen(&input_str)) {
	    bu_log("%s", bu_vls_addr(&input_str)+input_str_index);
	    input_str_index = bu_vls_strlen(&input_str);
	}
	escaped = bracketed = 0;
	break;
    case CTRL_D:                    /* Delete character at cursor */
	if (input_str_index == bu_vls_strlen(&input_str)) {
	    pr_beep(); /* Beep if at end of input string */
	    break;
	}
	bu_vls_init(&temp);
	bu_vls_strcat(&temp, bu_vls_addr(&input_str)+input_str_index+1);
	bu_vls_trunc(&input_str, input_str_index);
	bu_log("%S ", &temp);
	pr_prompt();
	bu_log("%S", &input_str);
	bu_vls_vlscat(&input_str, &temp);
	bu_vls_free(&temp);
	escaped = bracketed = 0;
	break;
    case CTRL_U:                   /* Delete whole line */
	pr_prompt();
	bu_log("%*s", bu_vls_strlen(&input_str), SPACES);
	pr_prompt();
	bu_vls_trunc(&input_str, 0);
	input_str_index = 0;
	escaped = bracketed = 0;
	break;
    case CTRL_K:                    /* Delete to end of line */
	bu_log("%*s", bu_vls_strlen(&input_str)-input_str_index, SPACES);
	bu_vls_trunc(&input_str, input_str_index);
	pr_prompt();
	bu_log("%S", &input_str);
	escaped = bracketed = 0;
	break;
    case CTRL_L:                   /* Redraw line */
	bu_log("\n");
	pr_prompt();
	bu_log("%S", &input_str);
	if (input_str_index == bu_vls_strlen(&input_str))
	    break;
	pr_prompt();
	bu_log("%*S", input_str_index, &input_str);
	escaped = bracketed = 0;
	break;
    case CTRL_B:                   /* Back one character */
	if (input_str_index == 0) {
	    pr_beep();
	    break;
	}
	--input_str_index;
	bu_log("\b"); /* hopefully non-destructive! */
	escaped = bracketed = 0;
	break;
    case CTRL_F:                   /* Forward one character */
	if (input_str_index == bu_vls_strlen(&input_str)) {
	    pr_beep();
	    break;
	}

	bu_log("%c", bu_vls_addr(&input_str)[input_str_index]);
	++input_str_index;
	escaped = bracketed = 0;
	break;
    case CTRL_T:                  /* Transpose characters */
	if (input_str_index == 0) {
	    pr_beep();
	    break;
	}
	if (input_str_index == bu_vls_strlen(&input_str)) {
	    bu_log("\b");
	    --input_str_index;
	}
	ch = bu_vls_addr(&input_str)[input_str_index];
	bu_vls_addr(&input_str)[input_str_index] =
	    bu_vls_addr(&input_str)[input_str_index - 1];
	bu_vls_addr(&input_str)[input_str_index - 1] = ch;
	bu_log("\b%*s", 2, bu_vls_addr(&input_str)+input_str_index-1);
	++input_str_index;
	escaped = bracketed = 0;
	break;
    case CTRL_N:                  /* Next history command */
    case CTRL_P:                  /* Last history command */
	/* Work the history routines to get the right string */
        curr_cmd_list = &head_cmd_list;
	if (freshline) {
	    if (ch == CTRL_P) {
		vp = history_prev();
		if (vp == NULL) {
		    pr_beep();
		    break;
		}
		bu_vls_trunc(&scratchline, 0);
		bu_vls_vlscat(&scratchline, &input_str);
		freshline = 0;
	    } else {
		pr_beep();
		break;
	    }
	} else {
	    if (ch == CTRL_P) {
		vp = history_prev();
		if (vp == NULL) {
		    pr_beep();
		    break;
		}
	    } else {
		vp = history_next();
		if (vp == NULL) {
		    vp = &scratchline;
		    freshline = 1;
		}
	    }
	}
	pr_prompt();
	bu_log("%*s", bu_vls_strlen(&input_str), SPACES);
	pr_prompt();
	bu_vls_trunc(&input_str, 0);
	bu_vls_vlscat(&input_str, vp);
	if (bu_vls_addr(&input_str)[bu_vls_strlen(&input_str)-1] == '\n')
	    bu_vls_trunc(&input_str, bu_vls_strlen(&input_str)-1); /* del \n */
	bu_log("%S", &input_str);
	input_str_index = bu_vls_strlen(&input_str);
	escaped = bracketed = 0;
	break;
    case '[':
	if (escaped) {
	    bracketed = 1;
	    break;
	}
	/* Fall through if not escaped! */
    default:
	if (!isprint(ch))
	    break;
	if (input_str_index == bu_vls_strlen(&input_str)) {
	    bu_log("%c", (int)ch);
	    bu_vls_putc(&input_str, (int)ch);
	    ++input_str_index;
	} else {
	    bu_vls_init(&temp);
	    bu_vls_strcat(&temp, bu_vls_addr(&input_str)+input_str_index);
	    bu_vls_trunc(&input_str, input_str_index);
	    bu_log("%c%S", (int)ch, &temp);
	    pr_prompt();
	    bu_vls_putc(&input_str, (int)ch);
	    bu_log("%S", &input_str);
	    bu_vls_vlscat(&input_str, &temp);
	    ++input_str_index;
	    bu_vls_free(&temp);
	}
	
	escaped = bracketed = 0;
	break;
    }
	
}


/* Stuff a string to stdout while leaving the current command-line alone */
int
cmd_stuff_str(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  int i;

  if(argc != 2)
    return TCL_ERROR;

  bu_log("\r%s\n", argv[1]);
  pr_prompt();
  bu_log("%s", bu_vls_addr(&input_str));
  pr_prompt();
  for(i = 0; i < input_str_index; ++i)
    bu_log("%c", bu_vls_addr(&input_str)[i]);

  return TCL_OK;
}


/*
 *			E V E N T _ C H E C K
 *
 *  Check for events, and dispatch them.
 *  Eventually, this will be done entirely by generating commands
 *
 *  Returns - recommended new value for non_blocking
 */

int
event_check( non_blocking )
int	non_blocking;
{
    vect_t		knobvec;	/* knob slew */
    register struct dm_list *p;
    struct dm_list *save_dm_list;

    /* Let cool Tk event handler do most of the work */

    if (non_blocking) {

	/* When in non_blocking-mode, we want to deal with as many events
	   as possible before the next redraw (multiple keypresses, redraw
	   events, etc... */
	
	while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
    } else {
      Tcl_DoOneEvent(TCL_ALL_EVENTS);
    }
    
    non_blocking = 0;

    /*
     * Set up window so that drawing does not run over into the
     * status line area, and menu area (if present).
     */

    windowbounds[1] = XMIN;		/* XLR */
    if( illump != SOLID_NULL )
	windowbounds[1] = MENUXLIM;
    windowbounds[3] = TITLE_YBASE-TEXT1_DY;	/* YLR */

#if 0
    dmp->dm_setWinBounds(windowbounds);	/* hack */
#endif

    save_dm_list = curr_dm_list;
    for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
      if(!p->_owner)
	continue;

      curr_dm_list = p;

      /*********************************
       *  Handle rate-based processing *
       *********************************/
      if( edit_rateflag_rotate ) {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -i -e ax %f ay %f az %f\n",
		      edit_rate_rotate[X],
		      edit_rate_rotate[Y],
		      edit_rate_rotate[Z]);
	
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }

      if( edit_rateflag_tran ) {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -i -e aX %f aY %f aZ %f\n",
		      -edit_rate_tran[X] * 0.1,
		      -edit_rate_tran[Y] * 0.1,
		      -edit_rate_tran[Z] * 0.1);
	
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }

      if( edit_rateflag_scale ) {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -i -e aS %f\n", edit_rate_scale * 0.01);
	
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }

      if( rateflag_rotate )  {
#if 1
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -i -v ax %f ay %f az %f\n",
		      rate_rotate[X],
		      rate_rotate[Y],
		      rate_rotate[Z]);

	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
#else
	non_blocking++;

	/* Compute delta x,y,z parameters */
	usejoy( rate_rotate[X] * 6 * degtorad,
	        rate_rotate[Y] * 6 * degtorad,
	        rate_rotate[Z] * 6 * degtorad );
#endif
      }
      if( rateflag_slew )  {
#if 1
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -i -v aX %f aY %f aZ %f",
		      rate_slew[X] * 0.1,
		      rate_slew[Y] * 0.1,
		      rate_slew[Z] * 0.1);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
#else
	non_blocking++;

	/* slew 1/10th of the view per update */
	knobvec[X] = -rate_slew[X] / 10;
	knobvec[Y] = -rate_slew[Y] / 10;
	knobvec[Z] = -rate_slew[Z] / 10;
	slewview( knobvec );
#endif
      }
      if( rateflag_zoom )  {
#if 1
#if 1
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "zoom %f",
		      1.0 / (1.0 - (rate_zoom / 10.0)));
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
#else
	int status;
	struct bu_vls vls;
	char *av[3];

	av[0] = "zoom";
	av[2] = NULL;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%f", 1.0 / (1.0 - (rate_zoom / 10.0)));
	av[1] = bu_vls_addr(&vls);
	status = f_zoom((ClientData)NULL, interp, 2, av);
	bu_vls_free(&vls);

	if( status == TCL_OK )
	  non_blocking++;
#endif
#else
	fastf_t	factor;
	mat_t scale_mat;

#define MINVIEW		0.001	/* smallest view.  Prevents runaway zoom */

	factor = 1.0 - (rate_zoom / 10);
	Viewscale *= factor;
	if( Viewscale < MINVIEW )
	    Viewscale = MINVIEW;
	else  {
	    non_blocking++;
	}

	/* Scaling (zooming) takes place around view center */
	mat_idn( scale_mat );
	scale_mat[15] = 1/factor;

	wrt_view( ModelDelta, scale_mat, ModelDelta );
	new_mats();
#endif
      }
      
      curr_dm_list = save_dm_list;
    }

    return( non_blocking );
}

/*			R E F R E S H
 *
 * NOTE that this routine is not to be casually used to
 * refresh the screen.  The normal procedure for screen
 * refresh is to manipulate the necessary global variables,
 * and wait for refresh to be called at the bottom of the while loop
 * in main().  However, when it is absolutely necessary to
 * flush a change in the solids table out to the display in
 * the middle of a routine somewhere (such as the "B" command
 * processor), then this routine may be called.
 *
 * If you don't understand the ramifications of using this routine,
 * then you don't want to call it.
 */
void
refresh()
{
  register struct dm_list *p;
  struct dm_list *save_dm_list;

  save_dm_list = curr_dm_list;
  for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
    /*
     * if something has changed, then go update the display.
     * Otherwise, we are happy with the view we have
     */

    curr_dm_list = p;
    dmp->dm_setWinBounds(dmp, windowbounds);

    if(update_views || dmaflag || dirty) {
      double	elapsed_time;

      /* XXX VR hack */
      if( viewpoint_hook )  (*viewpoint_hook)();

      rt_prep_timer();

      if( mged_variables.predictor )
	predictor_frame();

      dmp->dm_drawBegin(dmp);	/* update displaylist prolog */

      /*  Draw each solid in it's proper place on the screen
       *  by applying zoom, rotation, & translation.
       *  Calls dmp->dm_newrot() and dmp->dm_drawVList().
       */
      if( mged_variables.eye_sep_dist <= 0 )  {
	/* Normal viewing */
	dozoom(0);
      } else {
	/* Stereo viewing */
	dozoom(1);
	dozoom(2);
      }

      /* Restore to non-rotated, full brightness */
      dmp->dm_normal(dmp);

      /* Compute and display angle/distance cursor */
      if (mged_variables.adcflag)
	adcursor();

      /* Display titles, etc., if desired */
      dotitles(mged_variables.faceplate);

      /* Draw center dot */
      dmp->dm_setColor(dmp, DM_YELLOW, 1);
      dmp->dm_drawVertex2D(dmp, 0, 0);

      dmp->dm_drawEnd(dmp);

      (void)rt_get_timer( (struct bu_vls *)0, &elapsed_time );
      /* Only use reasonable measurements */
      if( elapsed_time > 1.0e-5 && elapsed_time < 30 )  {
	/* Smoothly transition to new speed */
	frametime = 0.9 * frametime + 0.1 * elapsed_time;
      }

      dirty = 0;
    }
  }

  for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
    curr_dm_list = p;
    dmaflag = 0;
  }

  curr_dm_list = save_dm_list;
  update_views = 0;
}

/*
 *			F _ V R O T _ C E N T E R
 *
 *  Set the center of rotation, either in model coordinates, or
 *  in view (+/-1) coordinates.
 *  The default is to rotate around the view center: v=(0,0,0).
 */
int
f_vrot_center(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  Tcl_AppendResult(interp, "Not ready until tomorrow.\n", (char *)NULL);
  return TCL_OK;
}

/*
 *			U S E J O Y
 *
 *  Apply the "joystick" delta rotation to the viewing direction,
 *  where the delta is specified in terms of the *viewing* axes.
 *  Rotation is performed about the view center, for now.
 *  Angles are in radians.
 */
void
usejoy( xangle, yangle, zangle )
double	xangle, yangle, zangle;
{
	mat_t	newrot;		/* NEW rot matrix, from joystick */

	if( state == ST_S_EDIT )  {
		if( sedit_rotate( xangle, yangle, zangle ) > 0 )
			return;		/* solid edit claimed event */
	} else if( state == ST_O_EDIT )  {
		if( objedit_rotate( xangle, yangle, zangle ) > 0 )
			return;		/* object edit claimed event */
	}

	/* NORMAL CASE.
	 * Apply delta viewing rotation for non-edited parts.
	 * The view rotates around the VIEW CENTER.
	 */
	mat_idn( newrot );
	buildHrot( newrot, xangle, yangle, zangle );

	mat_mul2( newrot, Viewrot );
	{
		mat_t	newinv;
		mat_inv( newinv, newrot );
		wrt_view( ModelDelta, newinv, ModelDelta );
	}
	new_mats();
}

/*
 *			A B S V I E W _ V
 *
 *  The "angle" ranges from -1 to +1.
 *  Assume rotation around view center, for now.
 */
void
absview_v( ang )
CONST point_t	ang;
{
	point_t	rad;

	VSCALE( rad, ang, rt_pi );	/* range from -pi to +pi */
	buildHrot( Viewrot, rad[X], rad[Y], rad[Z] );
	new_mats();
}

/*
 *			S E T V I E W
 *
 * Set the view.  Angles are DOUBLES, in degrees.
 *
 * Given that viewvec = scale . rotate . (xlate to view center) . modelvec,
 * we just replace the rotation matrix.
 * (This assumes rotation around the view center).
 */
void
setview( a1, a2, a3 )
double a1, a2, a3;		/* DOUBLE angles, in degrees */
{
  point_t model_pos;
  point_t temp;

  buildHrot( Viewrot, a1 * degtorad, a2 * degtorad, a3 * degtorad );
  new_mats();

  if(absolute_slew[X] != 0.0 ||
     absolute_slew[Y] != 0.0 ||
     absolute_slew[Z] != 0.0){
    VSET(temp, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
    MAT4X3PNT(absolute_slew, model2view, temp);
  }

  if(BU_LIST_NON_EMPTY(&head_cmd_list.l))
    Tcl_Eval(interp, "set_sliders");
}

/*
 *			S L E W V I E W
 *
 *  Given a position in view space,
 *  make that point the new view center.
 */
void
slewview( view_pos )
vect_t view_pos;
{
  point_t old_model_center;
  point_t new_model_center;
  vect_t diff;
  vect_t temp;
  mat_t	delta;

  MAT_DELTAS_GET_NEG( old_model_center, toViewcenter );

  MAT4X3PNT( new_model_center, view2model, view_pos );
  MAT_DELTAS_VEC_NEG( toViewcenter, new_model_center );

  VSUB2( diff, new_model_center, old_model_center );
  mat_idn( delta );
  MAT_DELTAS_VEC( delta, diff );
  mat_mul2( delta, ModelDelta );
  new_mats();

  VSET(temp, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
  MAT4X3PNT(absolute_slew, model2view, temp);

  if(BU_LIST_NON_EMPTY(&head_cmd_list.l))
    (void)Tcl_Eval(interp, "set_sliders");
}

/*
 *			L O G _ E V E N T
 *
 * Logging routine
 */
static void
log_event( event, arg )
char *event;
char *arg;
{
	char line[128];
	time_t now;
	char *timep;
	int logfd;

	(void)time( &now );
	timep = ctime( &now );	/* returns 26 char string */
	timep[24] = '\0';	/* Chop off \n */

	(void)sprintf(line, "%s [%s] time=%ld uid=%d (%s) %s\n",
		      event,
		      dmp->dm_name,
		      now,
		      getuid(),
		      timep,
		      arg
	);

	if( (logfd = open( LOGFILE, O_WRONLY|O_APPEND )) >= 0 )  {
		(void)write( logfd, line, (unsigned)strlen(line) );
		(void)close( logfd );
	}
}

/*
 *			F I N I S H
 *
 * This routine should be called in place of exit() everywhere
 * in GED, to permit an accurate finish time to be recorded in
 * the (ugh) logfile, also to remove the device access lock.
 */
void
mged_finish( exitcode )
int	exitcode;
{
	char place[64];
	register struct dm_list *p;

	(void)sprintf(place, "exit_status=%d", exitcode );
	log_event( "CEASE", place );

	for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
	  curr_dm_list = p;

	  dmp->dm_close(dmp);
#if 0
	  dmp->dm_light( dmp, LIGHT_RESET, 0 );	/* turn off the lights */
#endif
	}

	if (cbreak_mode > 0)
	    reset_Tty(fileno(stdin)); 
	    
	exit( exitcode );
}

/*
 *			Q U I T
 *
 * Handles finishing up.  Also called upon EOF on STDIN.
 */
void
quit()
{
	mged_finish(0);
	/* NOTREACHED */
}

/*
 *  			S I G 2
 */
void
sig2()
{
  /* Truncate input string */
  bu_vls_trunc(&input_str, 0);
  bu_vls_trunc(&input_str_prefix, 0);
  bu_vls_trunc(&curr_cmd_list->more_default, 0);
  input_str_index = 0;

  bu_vls_strcpy(&mged_prompt, MGED_PROMPT);
  bu_log("\n");
  pr_prompt();

  (void)signal( SIGINT, SIG_IGN );
}

void
sig3()
{
  (void)signal( SIGINT, SIG_IGN );
  longjmp( jmp_env, 1 );
}


/*
 *  			N E W _ M A T S
 *  
 *  Derive the inverse and editing matrices, as required.
 *  Centralized here to simplify things.
 */
void
new_mats()
{
	mat_mul( model2view, Viewrot, toViewcenter );
	model2view[15] = Viewscale;
	mat_inv( view2model, model2view );

#if 1
	{
	  vect_t work, work1;
	  vect_t temp, temp1;

	  /* Find current azimuth, elevation, and twist angles */
	  VSET( work , 0 , 0 , 1 );       /* view z-direction */
	  MAT4X3VEC( temp , view2model , work );
	  VSET( work1 , 1 , 0 , 0 );      /* view x-direction */
	  MAT4X3VEC( temp1 , view2model , work1 );

	  /* calculate angles using accuracy of 0.005, since display
	   * shows 2 digits right of decimal point */
	  mat_aet_vec( &curr_dm_list->s_info->azimuth,
		       &curr_dm_list->s_info->elevation,
		       &curr_dm_list->s_info->twist,
		       temp , temp1 , (fastf_t)0.005 );
	}
#endif

	if( state != ST_VIEW ) {
	    register struct dm_list *p;
	    struct dm_list *save_dm_list;

	    save_dm_list = curr_dm_list;
	    for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
	      if(!p->_owner)
		continue;

	      curr_dm_list = p;

	      mat_mul( model2objview, model2view, modelchanges );
	      mat_inv( objview2model, model2objview );
	    }

	    curr_dm_list = save_dm_list;
	    update_views = 1;
	}else
	  dmaflag = 1;
}

/*
 *			D O _ R C
 *
 *  If an mgedrc file exists, open it and process the commands within.
 *  Look first for a Shell environment variable, then for a file in
 *  the user's home directory, and finally in the current directory.
 *
 *  Returns -
 *	-1	FAIL
 *	 0	OK
 */
static int
do_rc()
{
	FILE	*fp;
	char	*path;
	int 	found;
	struct	bu_vls str;
	int bogus;

	found = 0;
	bu_vls_init( &str );

#define ENVRC	"MGED_RCFILE"
#define RCFILE	".gedrc"

	if( (path = getenv(ENVRC)) != (char *)NULL ) {
		if ((fp = fopen(path, "r")) != NULL ) {
			bu_vls_strcpy( &str, path );
			found = 1;
		}
	}

	if( !found ) {
		if( (path = getenv("HOME")) != (char *)NULL )  {
			bu_vls_strcpy( &str, path );
			bu_vls_strcat( &str, "/" );
			bu_vls_strcat( &str, RCFILE );
		}
		if( (fp = fopen(bu_vls_addr(&str), "r")) != NULL )
			found = 1;
	}

	if( !found ) {
		if( (fp = fopen( RCFILE, "r" )) != NULL )  {
			bu_vls_strcpy( &str, RCFILE );
			found = 1;
		}
	}

    /* At this point, if none of the above attempts panned out, give up. */

	if( !found )
		return -1;

	bogus = 0;
	while( !feof(fp) ) {
	    char buf[80];

	    /* Get beginning of line */
	    fgets( buf, 80, fp );
    /* If the user has a set command with an equal sign, remember to warn */
	    if( strstr(buf, "set") != NULL )
		if( strchr(buf, '=') != NULL )
		    bogus = 1;
	}
	
	fclose( fp );
	if( bogus ) {
	    bu_log("\nWARNING: The new format of the \"set\" command is:\n");
	    bu_log("    set varname value\n");
	    bu_log("If you are setting variables in your %s, you will ", RCFILE);
	    bu_log("need to change those\ncommands.\n\n");
	}
	if (Tcl_EvalFile( interp, bu_vls_addr(&str) ) == TCL_ERROR) {
	    bu_log("Error reading %s: %s\n", RCFILE, interp->result);
	}

	return 0;

}

/*
 *			F _ O P E N D B
 *
 *  Close the current database, if open, and then open a new database.
 *  May also open a display manager, if interactive and none selected yet.
 *
 *  argv[1] is the filename.
 *
 *  There are two invocations:
 *	main()
 *	cmdline()		Only one arg is permitted.
 */
int
f_opendb(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  static int first = 1;

	if( dbip )  {
	  char *av[2];

	  av[0] = "zap";
	  av[1] = NULL;

	  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	    return TCL_ERROR;

	  /* Clear out anything in the display */
	  f_zap(clientData, interp, 1, av);

	  /* Close current database.  Releases MaterHead, etc. too. */
	  db_close(dbip);
	  dbip = DBI_NULL;

	  log_event( "CEASE", "(close)" );
	}

	/* Get input file */
	if( ((dbip = db_open( argv[1], "r+w" )) == DBI_NULL ) &&
	    ((dbip = db_open( argv[1], "r"   )) == DBI_NULL ) )  {
	  char line[128];

	  if( isatty(0) ) {
	    if(first){
	      perror( argv[1] );
	      bu_log("Create new database (y|n)[n]? ");
	      (void)fgets(line, sizeof(line), stdin);
	      if( line[0] != 'y' && line[0] != 'Y' )
		exit(0);                /* NOT finish() */
	    } else {
	      if(argc == 2){
		perror( argv[1] );

		Tcl_AppendResult(interp, MORE_ARGS_STR, "Create new database (y|n)[n]? ",
				 (char *)NULL);
		bu_vls_printf(&curr_cmd_list->more_default, "n");

		dmaflag = 0;
		update_views = 0;
		return TCL_ERROR;
	      }

	      if( *argv[2] != 'y' && *argv[2] != 'Y' )
		exit(0);		/* NOT finish() */
	    }
	  }

	  Tcl_AppendResult(interp, "Creating new database \"", argv[1],
			   "\"\n", (char *)NULL);

	  if( (dbip = db_create( argv[1] )) == DBI_NULL )  {
	    perror( argv[1] );
	    exit(2);		/* NOT finish() */
	  }
	}
	if( dbip->dbi_read_only )
	  Tcl_AppendResult(interp, dbip->dbi_filename, ":  READ ONLY\n", (char *)NULL);

 	/* Quick -- before he gets away -- write a logfile entry! */
	log_event( "START", argv[1] );

	if(first){
	  first = 0;
#if 0
	  Tcl_AppendResult(interp, "Note: the attach command can be used\n",
			   "      to open a display window.\n\n", (char *)NULL);
#endif
	}

	/* --- Scan geometry database and build in-memory directory --- */
	db_scan( dbip, (int (*)())db_diradd, 1);
	/* XXX - save local units */
#if 0
	localunit = dbip->dbi_localunit;
	local2base = dbip->dbi_local2base;
	base2local = dbip->dbi_base2local;
#endif
	/* Print title/units information */
	if( interactive )
	  Tcl_AppendResult(interp, dbip->dbi_title, " (units=",
			   units_str[dbip->dbi_localunit], ")\n", (char *)NULL);
	

	return TCL_OK;
}
