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
#include "vmath.h"
#include "db.h"
#include "rtlist.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./solid.h"
#include "./sedit.h"
#include "./dm.h"

#ifdef XMGED

extern int      (*editline_hook)();
extern int	(*fgetc_hook)();
extern int	(*fputc_hook)();
extern int	(*fputs_hook)();
extern char	*(*fgets_hook)();
extern char	*(*gets_hook)();
extern void	(*fprintf_hook)();
extern int	(*button_hook)();
extern int	(*slider_hook)();
extern int	(*openw_hook)();
extern int	(*closew_hook)();
extern int	(*adc_hook)();
extern void	(*wait_hook)();
extern void	(*dotitles_hook)();
extern int     (*tran_hook)();
extern int     (*rot_hook)(), (*set_tran_hook)();
extern void set_tran();
extern point_t orig_pos;
extern int tran_set;
extern double tran_x;
extern double tran_y;
extern double tran_z;
extern int journal;
#endif

#ifndef	LOGFILE
#define LOGFILE	"/vld/lib/gedlog"	/* usage log */
#endif

struct db_i	*dbip;			/* database instance pointer */

struct device_values dm_values;		/* Dev Values, filled by dm-XX.c */

int		dmaflag;		/* Set to 1 to force new screen DMA */
double		frametime = 1.0;	/* time needed to draw last frame */
mat_t		ModelDelta;		/* Changes to Viewrot this frame */

int             (*knob_hook)() = NULL;
int             (*cue_hook)() = NULL;
int             (*zclip_hook)() = NULL;
int             (*zbuffer_hook)() = NULL;
int             (*light_hook)() = NULL;
int             (*perspective_hook)() = NULL;
int		(*cmdline_hook)() = NULL;
void		(*viewpoint_hook)() = NULL;
void            (*axis_color_hook)() = NULL;

static int	windowbounds[6];	/* X hi,lo;  Y hi,lo;  Z hi,lo */

static jmp_buf	jmp_env;		/* For non-local gotos */
void		(*cur_sigint)();	/* Current SIGINT status */
void            (*cmdline_sig)();
void		sig2();
void		new_mats();
void		usejoy();
void            slewview();
int		interactive = 0;	/* >0 means interactive */
int             cbreak_mode = 0;        /* >0 means in cbreak_mode */

static struct rt_vls input_str, scratchline, input_str_prefix;
static int input_str_index = 0;

static int	do_rc();
static void	log_event();
extern char	version[];		/* from vers.c */

struct rt_tol	mged_tol;		/* calculation tolerance */

#ifdef XMGED
extern int    update_views;
extern int      already_scandb;
#endif

static char *units_str[] = {
	"none",
	"mm",
	"cm",
	"meters",
	"inches",
	"feet",
	"extra"
};

struct rt_vls mged_prompt;
void pr_prompt(), pr_beep();

#ifdef USE_PROTOTYPES
Tk_FileProc stdin_input;
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

#ifdef XMGED
	editline_hook = NULL;
	fputc_hook = NULL;
	fputs_hook = NULL;
	fprintf_hook = NULL;
	fgets_hook = NULL;
	gets_hook = NULL;
	fgetc_hook = NULL;
	button_hook = NULL;
	slider_hook = NULL;
	openw_hook = NULL;
	closew_hook = NULL;
	adc_hook = NULL;
	dotitles_hook = NULL;
	wait_hook = NULL;
	tran_hook = NULL;
	rot_hook = NULL;
	set_tran_hook = NULL;
#endif

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
	 * Do not use rt_log() or rt_malloc() before here.
	 */
	if( rt_avail_cpus() > 1 )  {
		rt_g.rtg_parallel = 1;
		RES_INIT( &rt_g.res_syscall );
		RES_INIT( &rt_g.res_worker );
		RES_INIT( &rt_g.res_stats );
		RES_INIT( &rt_g.res_results );
		RES_INIT( &rt_g.res_model );
	}

	/* Set up linked lists */
	HeadSolid.s_forw = &HeadSolid;
	HeadSolid.s_back = &HeadSolid;
	FreeSolid = SOLID_NULL;
	RT_LIST_INIT( &rt_g.rtg_vlfree );

	state = ST_VIEW;
	es_edflag = -1;
	inpara = newedge = 0;

	/* init rotation matrix */
	mat_idn( identity );		/* Handy to have around */
	Viewscale = 500;		/* => viewsize of 1000mm (1m) */
	mat_idn( Viewrot );
	mat_idn( toViewcenter );
	mat_idn( modelchanges );
	mat_idn( ModelDelta );

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

	rt_vls_init( &dm_values.dv_string );
	rt_vls_init(&input_str);
	rt_vls_init(&input_str_prefix);
	rt_vls_init(&scratchline);
	rt_vls_init(&mged_prompt);
	input_str_index = 0;

	/* Perform any necessary initializations for the command parser */
	cmd_setup(interactive);

	/* Initialize the menu mechanism to be off, but ready. */
	mmenu_init();
	btn_head_menu(0,0,0);		/* unlabeled menu */

	windowbounds[0] = XMAX;		/* XHR */
	windowbounds[1] = XMIN;		/* XLR */
	windowbounds[2] = YMAX;		/* YHR */
	windowbounds[3] = YMIN;		/* YLR */
	windowbounds[4] = 2047;		/* ZHR */
	windowbounds[5] = -2048;	/* ZLR */

	dmaflag = 1;

	/* Open the database, attach a display manager */
	f_opendb( argc, argv );

	dmp->dmr_window(windowbounds);

	/* --- Now safe to process commands. --- */
	if( interactive )  {
		/* This is an interactive mged, process .mgedrc */
		do_rc();
	}

	/* --- Now safe to process geometry. --- */

	/* If this is an argv[] invocation, do it now */
	if( argc > 2 )  {
#if 0
		mged_cmd( argc-2, argv+2, (struct funtab *)NULL );
#else
		/*
		   Call cmdline instead of calling mged_cmd directly
		   so that access to tcl is possible.
	        */
		for(argc -= 2, argv += 2; argc; --argc, ++argv)
		  rt_vls_strcat(&input_str, *argv);

		cmdline(&input_str, TRUE);
		rt_vls_free(&input_str);
#endif
		f_quit(0, NULL);
		/* NOTREACHED */
	}

	refresh();			/* Put up faceplate */

	/* Reset the lights */
	dmp->dmr_light( LIGHT_RESET, 0 );

	Tk_CreateFileHandler(fileno(stdin), TK_READABLE, stdin_input,
			     (ClientData)NULL);

	/* Caught interrupts take us back here, via longjmp() */
	if( setjmp( jmp_env ) == 0 )  {
		/* First pass, determine SIGINT handler for interruptable cmds */
		if( cur_sigint == (void (*)())SIG_IGN )
			cur_sigint = (void (*)())SIG_IGN; /* detached? */
		else
			cur_sigint = sig2;	/* back to here w/!0 return */
	} else {
		rt_log("\nAborted.\n");
		/* If parallel routine was interrupted, need to reset */
		RES_RELEASE( &rt_g.res_syscall );
		RES_RELEASE( &rt_g.res_worker );
		RES_RELEASE( &rt_g.res_stats );
		RES_RELEASE( &rt_g.res_results );
		/* Truncate input string */
		rt_vls_trunc(&input_str, 0);
		rt_vls_trunc(&input_str_prefix, 0);
		input_str_index = 0;
	}

	rt_vls_strcpy(&mged_prompt, MGED_PROMPT);
	pr_prompt();

	/****************  M A I N   L O O P   *********************/

	if (cbreak_mode) {
	    set_Cbreak(fileno(stdin));
	    clr_Echo(fileno(stdin));
	}

	cmdline_sig = SIG_IGN;

	while(1) {
		(void)signal( SIGINT, cmdline_sig );

		/* This test stops optimizers from complaining about an infinite loop */
		if( (rateflag = event_check( rateflag )) < 0 )  break;

#ifdef XMGED
		/* now being done in cmd.c/mged_cmd() */
#else
		/* apply solid editing changes if necessary */
		if( sedraw > 0) {
			sedit();
			sedraw = 0;
			dmaflag = 1;
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
		rt_log("%S", &mged_prompt);
}

void
pr_beep()
{
    rt_log("%c", 7);
}

/*
 * standard input handling
 *
 * When the Tk event handler sees input on standard input, it calls the
 * routine "stdin_input" (registered with the Tk_CreateFileHandler call).
 * This routine simply appends the new input to a growing string until the
 * command is complete (it is assumed that the routine gets a fill line.)
 *
 * If the command is incomplete, then allow the user to hit ^C to start over,
 * by setting up the multi_line_sig routine as the SIGINT handler.
 */

extern struct rt_vls *history_prev(), *history_cur(), *history_next();

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
    struct rt_vls *vp;
    struct rt_vls temp;
    static int escaped = 0;
    static int bracketed = 0;
    static int freshline = 1;

    /* When not in cbreak mode, just process an entire line of input, and
       don't do any command-line manipulation. */

    if (!cbreak_mode) {
	rt_vls_init(&temp);

	/* Get line from stdin */
	if( rt_vls_gets(&temp, stdin) < 0 )
    		quit();				/* does not return */
	rt_vls_vlscat(&input_str, &temp);

	/* If there are any characters already in the command string (left
	   over from a CMD_MORE), then prepend them to the new input. */

	rt_vls_trunc(&temp, 0);
	rt_vls_vlscat(&temp, &input_str_prefix);  /* Make a backup copy */
	rt_vls_printf(&input_str_prefix, "%s%S\n",
		      rt_vls_strlen(&input_str_prefix) > 0 ? " " : "",
		      &input_str);

	/* If a complete line was entered, attempt to execute command. */
	
	if (Tcl_CommandComplete(rt_vls_addr(&input_str_prefix))) {
	    cmdline_sig = SIG_IGN; /* Ignore interrupts when command finishes*/
	    if (cmdline_hook != NULL) {
		if ((*cmdline_hook)(&input_str))
		    pr_prompt();
		rt_vls_trunc(&input_str, 0);
		rt_vls_trunc(&input_str_prefix, 0);
		input_str_index = 0;
	    } else {
		if (cmdline(&input_str, TRUE) == CMD_MORE) {
		    /* Remove newline */
		    rt_vls_trunc(&input_str_prefix,
				 rt_vls_strlen(&input_str_prefix)-1);
		    rt_vls_trunc(&input_str, 0);
		    cmdline_sig = sig2;  /* Still in CMD_MORE; allow ^C. */
		} else {
		    rt_vls_trunc(&input_str_prefix, 0);
		    rt_vls_trunc(&input_str, 0);
		}
		pr_prompt();
	    }
	    input_str_index = 0;
	    freshline = 1;
	} else {
	    rt_vls_trunc(&input_str, 0);
	    /* Allow the user to hit ^C. */
	    cmdline_sig = sig2;
	}
	rt_vls_free(&temp);
	return;
    }

    /* Grab single character from stdin */

    count = read(fileno(stdin), (void *)&ch, 1);
    if (count <= 0 && feof(stdin))
	f_quit(0, NULL);

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
	rt_log("\n");   /* Display newline */

	/* If there are any characters already in the command string (left
	   over from a CMD_MORE), then prepend them to the new input. */

	rt_vls_init(&temp);
	
	rt_vls_trunc(&temp, 0);
	rt_vls_vlscat(&temp, &input_str_prefix);  /* Make a backup copy */
	rt_vls_printf(&input_str_prefix, "%s%S\n",
		      rt_vls_strlen(&input_str_prefix) > 0 ? " " : "",
		      &input_str);

	/* If this forms a complete command (as far as the Tcl parser is
	   concerned) then execute it. */
	
	if (Tcl_CommandComplete(rt_vls_addr(&input_str_prefix))) {
	    cmdline_sig = SIG_IGN;
	    if (cmdline_hook) {  /* Command-line hooks don't do CMD_MORE */
		reset_Tty(fileno(stdin));
		if ((*cmdline_hook)(&input_str_prefix))
		    pr_prompt();
		set_Cbreak(fileno(stdin));
		clr_Echo(fileno(stdin));
		rt_vls_trunc(&input_str, 0);
		rt_vls_trunc(&input_str_prefix, 0);
	    } else {
		reset_Tty(fileno(stdin)); /* Backwards compatibility */
		if (cmdline(&input_str_prefix, TRUE) == CMD_MORE) {
		    /* Remove newline */
		    rt_vls_trunc(&input_str_prefix,
				 rt_vls_strlen(&input_str_prefix)-1);
		    rt_vls_trunc(&input_str, 0);
		    cmdline_sig = sig2; /* Allow ^C */
       /* *** The mged_prompt vls now contains prompt for more input. *** */
		} else {
		    rt_vls_trunc(&input_str_prefix, 0);
		    rt_vls_trunc(&input_str, 0);
		    /* All done; clear all strings. */
		}
		set_Cbreak(fileno(stdin)); /* Back to single-character mode */
		clr_Echo(fileno(stdin));
	    }
	} else {
	    rt_vls_trunc(&input_str, 0);
#if 0
	    rt_vls_strcpy(&mged_prompt, "? ");
#else
	    rt_vls_strcpy(&mged_prompt, "\r? ");
#endif
	    /* Allow the user to hit ^C */
	    cmdline_sig = sig2;
	}
	pr_prompt(); /* Print prompt for more input */
	input_str_index = 0;
	freshline = 1;
	escaped = bracketed = 0;
	rt_vls_free(&temp);
	break;
    case BACKSPACE:
    case DELETE:
	if (input_str_index <= 0) {
	    pr_beep();
	    break;
	}

	if (input_str_index == rt_vls_strlen(&input_str)) {
	    rt_log("\b \b");
	    rt_vls_trunc(&input_str, rt_vls_strlen(&input_str)-1);
	} else {
	    rt_vls_init(&temp);
	    rt_vls_strcat(&temp, rt_vls_addr(&input_str)+input_str_index);
	    rt_vls_trunc(&input_str, input_str_index-1);
	    rt_log("\b%S ", &temp);
	    pr_prompt();
	    rt_log("%S", &input_str);
	    rt_vls_vlscat(&input_str, &temp);
	    rt_vls_free(&temp);
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
	if (input_str_index < rt_vls_strlen(&input_str)) {
	    rt_log("%s", rt_vls_addr(&input_str)+input_str_index);
	    input_str_index = rt_vls_strlen(&input_str);
	}
	escaped = bracketed = 0;
	break;
    case CTRL_D:                    /* Delete character at cursor */
	if (input_str_index == rt_vls_strlen(&input_str)) {
	    pr_beep(); /* Beep if at end of input string */
	    break;
	}
	rt_vls_init(&temp);
	rt_vls_strcat(&temp, rt_vls_addr(&input_str)+input_str_index+1);
	rt_vls_trunc(&input_str, input_str_index);
	rt_log("%S ", &temp);
	pr_prompt();
	rt_log("%S", &input_str);
	rt_vls_vlscat(&input_str, &temp);
	rt_vls_free(&temp);
	escaped = bracketed = 0;
	break;
    case CTRL_U:                   /* Delete whole line */
	pr_prompt();
	rt_log("%*s", rt_vls_strlen(&input_str), SPACES);
	pr_prompt();
	rt_vls_trunc(&input_str, 0);
	input_str_index = 0;
	escaped = bracketed = 0;
	break;
    case CTRL_K:                    /* Delete to end of line */
	rt_log("%*s", rt_vls_strlen(&input_str)-input_str_index, SPACES);
	rt_vls_trunc(&input_str, input_str_index);
	pr_prompt();
	rt_log("%S", &input_str);
	escaped = bracketed = 0;
	break;
    case CTRL_L:                   /* Redraw line */
	rt_log("\n");
	pr_prompt();
	rt_log("%S", &input_str);
	if (input_str_index == rt_vls_strlen(&input_str))
	    break;
	pr_prompt();
	rt_log("%*S", input_str_index, &input_str);
	escaped = bracketed = 0;
	break;
    case CTRL_B:                   /* Back one character */
	if (input_str_index == 0) {
	    pr_beep();
	    break;
	}
	--input_str_index;
	rt_log("\b"); /* hopefully non-destructive! */
	escaped = bracketed = 0;
	break;
    case CTRL_F:                   /* Forward one character */
	if (input_str_index == rt_vls_strlen(&input_str)) {
	    pr_beep();
	    break;
	}

	rt_log("%c", rt_vls_addr(&input_str)[input_str_index]);
	++input_str_index;
	escaped = bracketed = 0;
	break;
    case CTRL_T:                  /* Transpose characters */
	if (input_str_index == 0) {
	    pr_beep();
	    break;
	}
	if (input_str_index == rt_vls_strlen(&input_str)) {
	    rt_log("\b");
	    --input_str_index;
	}
	ch = rt_vls_addr(&input_str)[input_str_index];
	rt_vls_addr(&input_str)[input_str_index] =
	    rt_vls_addr(&input_str)[input_str_index - 1];
	rt_vls_addr(&input_str)[input_str_index - 1] = ch;
	rt_log("\b%*s", 2, rt_vls_addr(&input_str)+input_str_index-1);
	++input_str_index;
	escaped = bracketed = 0;
	break;
    case CTRL_N:                  /* Next history command */
    case CTRL_P:                  /* Last history command */
	/* Work the history routines to get the right string */

	if (freshline) {
	    if (ch == CTRL_P) {
		vp = history_prev();
		if (vp == NULL) {
		    pr_beep();
		    break;
		}
		rt_vls_trunc(&scratchline, 0);
		rt_vls_vlscat(&scratchline, &input_str);
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
	rt_log("%*s", rt_vls_strlen(&input_str), SPACES);
	pr_prompt();
	rt_vls_trunc(&input_str, 0);
	rt_vls_vlscat(&input_str, vp);
	if (rt_vls_addr(&input_str)[rt_vls_strlen(&input_str)-1] == '\n')
	    rt_vls_trunc(&input_str, rt_vls_strlen(&input_str)-1); /* del \n */
	rt_log("%S", &input_str);
	input_str_index = rt_vls_strlen(&input_str);
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
	if (input_str_index == rt_vls_strlen(&input_str)) {
	    rt_log("%c", (int)ch);
	    rt_vls_putc(&input_str, (int)ch);
	    ++input_str_index;
	} else {
	    rt_vls_init(&temp);
	    rt_vls_strcat(&temp, rt_vls_addr(&input_str)+input_str_index);
	    rt_vls_trunc(&input_str, input_str_index);
	    rt_log("%c%S", (int)ch, &temp);
	    pr_prompt();
	    rt_vls_putc(&input_str, (int)ch);
	    rt_log("%S", &input_str);
	    rt_vls_vlscat(&input_str, &temp);
	    ++input_str_index;
	    rt_vls_free(&temp);
	}
	
	escaped = bracketed = 0;
	break;
    }
	
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

    /* Let cool Tk event handler do most of the work */

    if (non_blocking) {

	/* When in non_blocking-mode, we want to deal with as many events
	   as possible before the next redraw (multiple keypresses, redraw
	   events, etc... */
	
	while (Tk_DoOneEvent(TK_ALL_EVENTS|TK_DONT_WAIT)) {
	    if( cmdline_hook )  
		(*cmdline_hook)( &dm_values.dv_string ); 
	    else {
		/* Some commands (e.g. mouse events) queue further events. */
		int oldlen;
loopagain:
		oldlen = rt_vls_strlen( &dm_values.dv_string );
		(void)cmdline( &dm_values.dv_string, FALSE );
		if( rt_vls_strlen( &dm_values.dv_string ) > oldlen ) {
		    /* Remove cmds already done, and go again */
		    rt_vls_nibble( &dm_values.dv_string, oldlen );
		    goto loopagain;
		}
	    }
	    rt_vls_trunc( &dm_values.dv_string, 0 );
	}
    } else {
	Tk_DoOneEvent(TK_ALL_EVENTS);
    }
    
    non_blocking = 0;

    /*
     *  Process any "string commands" sent to us by the display manager.
     *  (Or "invented" here, for compatability with old dm's).
     *  Each one is expected to be newline terminated.
     */

    if( cmdline_hook )  
	(*cmdline_hook)(&dm_values.dv_string); 
    else {
	/* Some commands (e.g. mouse events) queue further events. */
	int oldlen;
again:
	oldlen = rt_vls_strlen( &dm_values.dv_string );
	(void)cmdline( &dm_values.dv_string, FALSE );
	if( rt_vls_strlen( &dm_values.dv_string ) > oldlen ) {
	    /* Remove cmds already done, and go again */
	    rt_vls_nibble( &dm_values.dv_string, oldlen );
	    goto again;
	}
    }

    rt_vls_trunc( &dm_values.dv_string, 0 );
    
    /*
     * Set up window so that drawing does not run over into the
     * status line area, and menu area (if present).
     */

    windowbounds[1] = XMIN;		/* XLR */
    if( illump != SOLID_NULL )
	windowbounds[1] = MENUXLIM;
    windowbounds[3] = TITLE_YBASE-TEXT1_DY;	/* YLR */
    dmp->dmr_window(windowbounds);	/* hack */

    /*********************************
     *  Handle rate-based processing *
     *********************************/
    if( rateflag_rotate )  {
	non_blocking++;

	/* Compute delta x,y,z parameters */
	usejoy( rate_rotate[X] * 6 * degtorad,
	        rate_rotate[Y] * 6 * degtorad,
	        rate_rotate[Z] * 6 * degtorad );
    }
    if( rateflag_slew )  {
	non_blocking++;

	/* slew 1/10th of the view per update */
	knobvec[X] = -rate_slew[X] / 10;
	knobvec[Y] = -rate_slew[Y] / 10;
	knobvec[Z] = -rate_slew[Z] / 10;
	slewview( knobvec );
    }
    if( rateflag_zoom )  {
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
	dmaflag = 1;
	new_mats();
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
	/*
	 * if something has changed, then go update the display.
	 * Otherwise, we are happy with the view we have
	 */
	if( dmaflag )  {
		double	elapsed_time;

		/* XXX VR hack */
		if( viewpoint_hook )  (*viewpoint_hook)();

		rt_prep_timer();

		if( mged_variables.predictor )
			predictor_frame();

		dmp->dmr_prolog();	/* update displaylist prolog */

		/*  Draw each solid in it's proper place on the screen
		 *  by applying zoom, rotation, & translation.
		 *  Calls dmp->dmr_newrot() and dmp->dmr_object().
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
		dmp->dmr_normal();

		/* Compute and display angle/distance cursor */
		if (adcflag)
			adcursor();

		/* Display titles, etc., if desired */
		dotitles(mged_variables.faceplate);
		
		dmp->dmr_epilog();

		(void)rt_get_timer( (struct rt_vls *)0, &elapsed_time );
		/* Only use reasonable measurements */
		if( elapsed_time > 1.0e-5 && elapsed_time < 30 )  {
			/* Smoothly transition to new speed */
			frametime = 0.9 * frametime + 0.1 * elapsed_time;
		}
	} else {
		/* For displaylist machines??? */
		dmp->dmr_prolog();	/* update displaylist prolog */
		dmp->dmr_update();
	}

	dmaflag = 0;
}

/*
 *			F _ V R O T _ C E N T E R
 *
 *  Set the center of rotation, either in model coordinates, or
 *  in view (+/-1) coordinates.
 *  The default is to rotate around the view center: v=(0,0,0).
 */
int
f_vrot_center( argc, argv )
int	argc;
char	**argv;
{
	rt_log("Not ready until tomorrow.\n");
	return CMD_OK;
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

	dmaflag = 1;

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
#ifdef XMGED
  point_t old_pos;
  point_t new_pos;
  point_t new_pos2;
  point_t diff;
  vect_t view_pos;
  
  MAT_DELTAS_GET(old_pos, toViewcenter);

  if(state == ST_S_EDIT || state == ST_O_EDIT){
    VSET(view_pos, tran_x, tran_y, tran_z);
    MAT4X3PNT( new_pos2, view2model, view_pos );
  }
#endif

	buildHrot( Viewrot, a1 * degtorad, a2 * degtorad, a3 * degtorad );
	new_mats();

#ifdef XMGED
  MAT_DELTAS_GET_NEG(new_pos, toViewcenter);

  if(state == ST_S_EDIT || state == ST_O_EDIT){
    MAT4X3PNT(view_pos, model2view, new_pos2);
    tran_x = view_pos[X];
    tran_y = view_pos[Y];
    tran_z = view_pos[Z];
  }else{
    VSUB2(diff, new_pos, orig_pos);
    VADD2(new_pos, old_pos, diff);
    VSET(view_pos, new_pos[X], new_pos[Y], new_pos[Z]);
    MAT4X3PNT( new_pos, model2view, view_pos);
    tran_x = new_pos[X];
    tran_y = new_pos[Y];
    tran_z = new_pos[Z];
  }

  if(tran_hook)
          (*tran_hook)();
#endif
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
	point_t	old_model_center;
	point_t	new_model_center;
	vect_t	diff;
	mat_t	delta;

	MAT_DELTAS_GET_NEG( old_model_center, toViewcenter );

	MAT4X3PNT( new_model_center, view2model, view_pos );
	MAT_DELTAS_VEC_NEG( toViewcenter, new_model_center );

	VSUB2( diff, new_model_center, old_model_center );
	mat_idn( delta );
	MAT_DELTAS_VEC( delta, diff );
	mat_mul2( delta, ModelDelta );
	new_mats();

#ifdef XMGED
	if(!tran_set){
	  tran_x -= view_pos[X];
	  tran_y -= view_pos[Y];
	  tran_z -= view_pos[Z];
	}

	if(tran_hook)
	  (*tran_hook)();
#endif
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
			dmp->dmr_name,
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

	(void)sprintf(place, "exit_status=%d", exitcode );
	log_event( "CEASE", place );
	dmp->dmr_light( LIGHT_RESET, 0 );	/* turn off the lights */
	dmp->dmr_close();

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
    rt_vls_strcpy(&mged_prompt, MGED_PROMPT);
    longjmp( jmp_env, 1 );
	/* NOTREACHED */
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
	if( state != ST_VIEW )  {
		mat_mul( model2objview, model2view, modelchanges );
		mat_inv( objview2model, model2objview );
	}
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
	struct	rt_vls str;
	int bogus;

	found = 0;
	rt_vls_init( &str );

#define ENVRC	"MGED_RCFILE"
#define RCFILE	".gedrc"

	if( (path = getenv(ENVRC)) != (char *)NULL ) {
		if ((fp = fopen(path, "r")) != NULL ) {
			rt_vls_strcpy( &str, path );
			found = 1;
		}
	}

	if( !found ) {
		if( (path = getenv("HOME")) != (char *)NULL )  {
			rt_vls_strcpy( &str, path );
			rt_vls_strcat( &str, "/" );
			rt_vls_strcat( &str, RCFILE );
		}
		if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )
			found = 1;
	}

	if( !found ) {
		if( (fp = fopen( RCFILE, "r" )) != NULL )  {
			rt_vls_strcpy( &str, RCFILE );
			found = 1;
		}
	}

    /* At this point, if none of the above attempts panned out, give up. */

	if( !found )
		return -1;

#ifndef XMGED
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
	    rt_log("\nWARNING: The new format of the \"set\" command is:\n");
	    rt_log("    set varname value\n");
	    rt_log("If you are setting variables in your %s, you will ", RCFILE);
	    rt_log("need to change those\ncommands.\n\n");
	}
	if (Tcl_EvalFile( interp, rt_vls_addr(&str) ) == TCL_ERROR) {
	    rt_log("Error reading %s: %s\n", RCFILE, interp->result);
	}
#else
	mged_source_file( fp );
	fclose( fp );
#endif
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
f_opendb( argc, argv )
int	argc;
char	**argv;
{
#ifdef XMGED
        struct db_i *o_dbip;

	o_dbip = dbip;  /* save pointer to old database */

	/* Get input file */
	if( ((dbip = db_open( argv[1], "r+w" )) == DBI_NULL ) &&
	    ((dbip = db_open( argv[1], "r"   )) == DBI_NULL ) )  {
		char line[128];

		dbip = o_dbip; /* reinitialize in case of control-C */

		if( isatty(0) ) {
		    perror( argv[1] );
		    rt_log("Create new database (y|n)[n]? ");
		    (void)mged_fgets(line, sizeof(line), stdin);
		    if( line[0] != 'y' && line[0] != 'Y' ){
		      if(dbip != DBI_NULL){
			rt_log( "Still using %s\n", dbip->dbi_title);
			return(CMD_OK);
		      }

		      exit(0);		/* NOT finish() */
		    }
		} else
		  rt_log("Creating new database \"%s\"\n", argv[1]);

		if( (dbip = db_create( argv[1] )) == DBI_NULL )  {
			perror( argv[1] );
			exit(2);		/* NOT finish() */
		}
	}

	already_scandb = 0;
	if( o_dbip )  {
		/* Clear out anything in the display */
		f_zap( 0, (char **)NULL );

		/* Close current database.  Releases rt_material_head, etc. too. */
		db_close(o_dbip);

		log_event( "CEASE", "(close)" );
	}

	if( dbip->dbi_read_only )
		(void)rt_log( "%s:  READ ONLY\n", dbip->dbi_filename );

	/* Quick -- before he gets away -- write a logfile entry! */
	log_event( "START", argv[1] );

	if( interactive && is_dm_null() )  {
		/*
		 * This is an interactive mged, with no display yet.
		 * Ask which DM, and fire up the display manager.
		 * Ask this question BEFORE the db_scan, because
		 * that can take a long time for large models.
		 */
		get_attached();
	}

	/* --- Scan geometry database and build in-memory directory --- */
	if(!already_scandb){	/* need this because the X display manager may
					scan the database */
		db_scan( dbip, (int (*)())db_diradd, 1);
		already_scandb = 1;
	}

	/* XXX - save local units */
	localunit = dbip->dbi_localunit;
	local2base = dbip->dbi_local2base;
	base2local = dbip->dbi_base2local;

	/* Print title/units information */
	if( interactive )
		(void)rt_log( "%s (units=%s)\n", dbip->dbi_title,
			units_str[dbip->dbi_localunit] );

	update_views = 1;
	return(CMD_OK);
#else
	if( dbip )  {
		/* Clear out anything in the display */
		f_zap( 0, (char **)NULL );

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
		    perror( argv[1] );
		    rt_log("Create new database (y|n)[n]? ");
		    (void)fgets(line, sizeof(line), stdin);
		    if( line[0] != 'y' && line[0] != 'Y' )
			exit(0);		/* NOT finish() */
		} else
		    rt_log("Creating new database \"%s\"\n", argv[1]);
			

		if( (dbip = db_create( argv[1] )) == DBI_NULL )  {
			perror( argv[1] );
			exit(2);		/* NOT finish() */
		}
	}
	if( dbip->dbi_read_only )
		rt_log("%s:  READ ONLY\n", dbip->dbi_filename );

	/* Quick -- before he gets away -- write a logfile entry! */
	log_event( "START", argv[1] );

	if( interactive && is_dm_null() )  {
		/*
		 * This is an interactive mged, with no display yet.
		 * Ask which DM, and fire up the display manager.
		 * Ask this question BEFORE the db_scan, because
		 * that can take a long time for large models.
		 */
		get_attached();
	}

	/* --- Scan geometry database and build in-memory directory --- */
	db_scan( dbip, (int (*)())db_diradd, 1);
	/* XXX - save local units */
	localunit = dbip->dbi_localunit;
	local2base = dbip->dbi_local2base;
	base2local = dbip->dbi_base2local;

	/* Print title/units information */
	if( interactive )
		rt_log("%s (units=%s)\n", dbip->dbi_title,
			units_str[dbip->dbi_localunit] );
	
	return CMD_OK;
#endif
}
