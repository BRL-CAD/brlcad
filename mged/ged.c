/*
 *			G E D . C
 *
 * Mainline portion of the graphics editor
 *
 *  Functions -
 *	pr_prompt	print prompt
 *	main		Mainline portion of the graphics editor
 *	refresh		Internal routine to perform displaylist output writing
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

#include "tk.h"

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./mged_solid.h"
#include "./sedit.h"
#include "./mged_dm.h"

#ifndef	LOGFILE
#define LOGFILE	"/vld/lib/gedlog"	/* usage log */
#endif

#if 0
/* defined in libfb/tcl.c */
extern void fb_tclInit();
#endif

/* defined in predictor.c */
extern void predictor_init();

/* defined in cmd.c */
extern Tcl_Interp *interp;
extern Tk_Window tkwin;

/* defined in attach.c */
extern int mged_slider_link_vars();

extern struct dm dm_Null;
extern struct _mged_variables default_mged_variables;

int dm_pipe[2];
int pipe_out[2];
int pipe_err[2];
struct db_i *dbip = DBI_NULL;	/* database instance pointer */
int    update_views = 0;
int		(*cmdline_hook)() = NULL;
jmp_buf	jmp_env;		/* For non-local gotos */

int bit_bucket();
int             cmd_stuff_str();
void		(*cur_sigint)();	/* Current SIGINT status */
void		sig2(), sig3();
void		new_mats();
void		usejoy();
void            slewview();
int		interactive = 0;	/* >0 means interactive */
int             cbreak_mode = 0;        /* >0 means in cbreak_mode */
int		classic_mged=0;

static struct bu_vls input_str, scratchline, input_str_prefix;
static int input_str_index = 0;

static void     mged_insert_char();
static int	do_rc();
static void	log_event();
extern char	version[];		/* from vers.c */

struct bn_tol	mged_tol;		/* calculation tolerance */

static char *units_str[] = {
	"none",
	"mm",
	"um",
	"cm",
	"km",
	"meters",
	"inches",
	"feet",
	"yards",
	"miles",
	"extra"
};

struct bu_vls mged_prompt;
void pr_prompt(), pr_beep();

#ifdef USE_PROTOTYPES
Tcl_FileProc stdin_input;
Tcl_FileProc std_out_or_err;
#else
void stdin_input();
void std_out_or_err();
#endif

/* 
 *			M A I N
 */

int
main(argc,argv)
int argc;
char **argv;
{
	extern char *bu_optarg;
	extern int bu_optind, bu_opterr, bu_optopt;
	int	force_interactive = 0;
	int	rateflag = 0;
	int	c;
	int	read_only_flag=0;

#if 0
	/* Check for proper invocation */
	if( argc < 1 )  {
		fprintf(stdout, "Usage:  %s [-n] [-r] [database [command]]\n", argv[0]);
		fflush(stdout);
		return(1);		/* NOT finish() */
	}
#endif

	while ((c = bu_getopt(argc, argv, "hinrx:X:")) != EOF)
	{
		switch( c )
		{
			case 'i':
				force_interactive = 1;
				break;
			case 'r':
				read_only_flag = 1;
				break;
			case 'n':
				classic_mged = 1;
				break;
			case 'x':
				sscanf( bu_optarg, "%x", &rt_g.debug );
				break;
			case 'X':
	                        sscanf( bu_optarg, "%x", &bu_debug );
				break;
			case 'h':
				fprintf(stdout, "Usage:  %s [-h] [-i] [-n] [-r] [database [command]]\n", argv[0]);
				fflush(stdout);
				return(1);
			default:
				fprintf( stdout, "Unrecognized option (%c)\n", c );
				fflush(stdout);
				return( 1 );
		}
	}

	argc -= (bu_optind - 1);
	argv += (bu_optind - 1);

#if 0
	/* Check again for proper invocation */
	if( argc < 2 )  {
	  argv -= (bu_optind - 1);
	  fprintf(stdout, "Usage:  %s [-n] [-r] [database [command]]\n", argv[0]);
	  fflush(stdout);
	  return(1);		/* NOT finish() */
	}
#endif

	/* Identify ourselves if interactive */
	if( argc <= 2 )  {
	  if( isatty(fileno(stdin)) )
	    interactive = 1;

	  if(classic_mged){
	    fprintf(stdout, "%s\n", version+5);	/* skip @(#) */
	    fflush(stdout);
		
	    if (isatty(fileno(stdin)) && isatty(fileno(stdout))) {
#ifndef COMMAND_LINE_EDITING
#define COMMAND_LINE_EDITING 1
#endif
	      /* Set up for character-at-a-time terminal IO. */
	      cbreak_mode = COMMAND_LINE_EDITING;
	      save_Tty(fileno(stdin));
	    }
	  }
	}

	if( force_interactive )
		interactive = 1;

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

	  bu_semaphore_init( 5 );
	  bu_semaphore_init( 5 );
	  bu_semaphore_init( 5 );
	  bu_semaphore_init( 5 );
	  bu_semaphore_init( 5 );
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
	{
	  struct dm_list *dlp;

	  BU_GETSTRUCT(dlp, dm_list);
	  BU_LIST_APPEND(&head_dm_list.l, &dlp->l);
	  curr_dm_list = dlp;
	}

	/* initialize predictor stuff */
	BU_LIST_INIT(&curr_dm_list->p_vlist);
	predictor_init();

	BU_GETSTRUCT(dmp, dm);
	*dmp = dm_Null;
	bu_vls_init(&pathName);
	bu_vls_init(&tkName);
	bu_vls_init(&dName);
	bu_vls_strcpy(&pathName, "nu");
	bu_vls_strcpy(&tkName, "nu");
	BU_GETSTRUCT(curr_dm_list->s_info, shared_info);
	BU_GETSTRUCT(mged_variables, _mged_variables);
        *mged_variables = default_mged_variables;
	am_mode = AMM_IDLE;
	rc = 1;
	owner = 1;
	frametime = 1;
	adc_a1_deg = adc_a2_deg = 45.0;
	curr_dm_list->s_info->opp = &pathName;
	mged_view_init(curr_dm_list);
	BU_GETSTRUCT(curr_dm_list->menu_vars, menu_vars);

	bu_vls_init(&fps_name);
	bu_vls_printf(&fps_name, "mged_display(%S,fps)",
		      &curr_dm_list->_dmp->dm_pathName);

	bn_mat_idn( identity );		/* Handy to have around */
	/* init rotation matrix */
	Viewscale = 500;		/* => viewsize of 1000mm (1m) */
	bn_mat_idn( Viewrot );
	bn_mat_idn( toViewcenter );
	bn_mat_idn( modelchanges );
	bn_mat_idn( ModelDelta );
	bn_mat_idn( acc_rot_sol );

	MAT_DELTAS_GET_NEG(orig_pos, toViewcenter);
	i_Viewscale = Viewscale;

	state = ST_VIEW;
	es_edflag = -1;
	es_edclass = EDIT_CLASS_NULL;
	inpara = newedge = 0;

	/* These values match old GED.  Use 'tol' command to change them. */
	mged_tol.magic = BN_TOL_MAGIC;
	mged_tol.dist = 0.005;
	mged_tol.dist_sq = mged_tol.dist * mged_tol.dist;
	mged_tol.perp = 1e-6;
	mged_tol.para = 1 - mged_tol.perp;

	rt_prep_timer();		/* Initialize timer */

	new_mats();

	es_edflag = -1;		/* no solid editing just now */

	bu_vls_init( &curr_cmd_list->more_default );
	bu_vls_init(&input_str);
	bu_vls_init(&input_str_prefix);
	bu_vls_init(&scratchline);
	bu_vls_init(&mged_prompt);
	input_str_index = 0;

	/* Get set up to use Tcl */
	mged_setup();
	mmenu_init();
	btn_head_menu(0,0,0);
	mged_slider_link_vars(curr_dm_list);

	/* Initialize libdm */
	(void)dm_tclInit(interp);

#if 0
	/* Initialize libfb */
	(void)fb_tclInit(interp);
#endif

	setview( 0.0, 0.0, 0.0 );

	if(argc >= 2){
	  /* Open the database, attach a display manager */
	  /* Command line may have more than 2 args, opendb only wants 2 */
	  if(f_opendb( (ClientData)NULL, interp, 2, argv ) == TCL_ERROR)
	    mged_finish(1);
	  else
	    bu_log("%s", interp->result);
	}

	if( dbip != DBI_NULL && (read_only_flag || dbip->dbi_read_only) )
	{
		dbip->dbi_read_only = 1;
		bu_log( "Opened in READ ONLY mode\n" );
	}

	/* --- Now safe to process commands. --- */
	if(interactive){
	  /* This is an interactive mged, process .mgedrc */
	  do_rc();

	  if(classic_mged)
	    get_attached();
	  else{
	    int pid;

	    if ((pid = fork()) == 0){
	      struct bu_vls vls;

	      (void)pipe(pipe_out);
	      (void)pipe(pipe_err);

	      /* Redirect stdout */
	      (void)close(1);
	      (void)dup(pipe_out[1]);
	      (void)close(pipe_out[1]);

	      /* Redirect stderr */
	      (void)close(2);
	      (void)dup(pipe_err[1]);
	      (void)close(pipe_err[1]);

	      bu_vls_init(&vls);
	      bu_vls_strcpy(&vls, "openw");
	      (void)Tcl_Eval(interp, bu_vls_addr(&vls));
	      bu_vls_free(&vls);
#if 0
	      bu_add_hook(bit_bucket, (genptr_t)NULL);
#endif
	    }else{
	      exit(0);
	    }
	  }
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

	refresh();			/* Put up faceplate */

	if(classic_mged){
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

	  if (cbreak_mode) {
	    set_Cbreak(fileno(stdin));
	    clr_Echo(fileno(stdin));
	  }
	}else{
	  Tcl_CreateFileHandler(Tcl_GetFile((ClientData)pipe_out[0], TCL_UNIX_FD),
				TCL_READABLE, std_out_or_err, (ClientData)pipe_out[0]);
	  Tcl_CreateFileHandler(Tcl_GetFile((ClientData)pipe_err[0], TCL_UNIX_FD),
				TCL_READABLE, std_out_or_err, (ClientData)pipe_err[0]);
	}

	/****************  M A I N   L O O P   *********************/
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

/*XXXXX*/
#define TRY_STDIN_INPUT_HACK
#ifdef TRY_STDIN_INPUT_HACK
    /* Grab everything --- assuming everything is <= 4096 */
    {
      char buf[4096];
      int index;

      count = read((int)fd, (void *)buf, 4096);
#else
    /* Grab single character from stdin */
    count = read((int)fd, (void *)&ch, 1);
#endif

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
#define CTRL_W      '\027'
#define ESC         27
#define BACKSPACE   '\b'
#define DELETE      127

#define SPACES "                                                                                                                                                                                                                                                                                                           "
#ifdef TRY_STDIN_INPUT_HACK
    /* Process everything in buf */
    for(index = 0, ch = buf[index]; index < count; ch = buf[++index]){
#endif
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
	else {
	  if (curr_cmd_list->quote_string)
	    bu_vls_printf(&input_str_prefix, "%s\"%S\"\n",
			  bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
			  &input_str);
	  else
	    bu_vls_printf(&input_str_prefix, "%s%S\n",
			  bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
			  &input_str);
	}

	curr_cmd_list->quote_string = 0;
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
    case CTRL_W:                   /* backward-delete-word */
	{
	  char *start;
	  char *curr;
	  int len;

	  start = bu_vls_addr(&input_str);
	  curr = start + input_str_index - 1;

	  /* skip spaces */
	  while(curr > start && *curr == ' ')
	    --curr;

	  /* find next space */
	  while(curr > start && *curr != ' ')
	    --curr;

	  bu_vls_init(&temp);
	  bu_vls_strcat(&temp, start+input_str_index);

	  if(curr == start)
	    input_str_index = 0;
	  else
	    input_str_index = curr - start + 1;

	  len = bu_vls_strlen(&input_str);
	  bu_vls_trunc(&input_str, input_str_index);
	  pr_prompt();
	  bu_log("%S%S%*s", &input_str, &temp, len - input_str_index, SPACES);
	  pr_prompt();
	  bu_log("%S", &input_str);
	  bu_vls_vlscat(&input_str, &temp);
	  bu_vls_free(&temp);
	}

        escaped = bracketed = 0;
        break;
    case 'd':
      if (escaped) {                /* delete-word */
	char *start;
	char *curr;
	int i;

	start = bu_vls_addr(&input_str);
	curr = start + input_str_index;

	/* skip spaces */
	while(*curr != '\0' && *curr == ' ')
	  ++curr;

	/* find next space */
	while(*curr != '\0' && *curr != ' ')
	  ++curr;

	i = curr - start;
	bu_vls_init(&temp);
	bu_vls_strcat(&temp, curr);
	bu_vls_trunc(&input_str, input_str_index);
	pr_prompt();
	bu_log("%S%S%*s", &input_str, &temp, i - input_str_index, SPACES);
	pr_prompt();
	bu_log("%S", &input_str);
	bu_vls_vlscat(&input_str, &temp);
	bu_vls_free(&temp);
      }else
	mged_insert_char(ch);

      escaped = bracketed = 0;
      break;
    case 'f':
      if (escaped) {                /* forward-word */
	char *start;
	char *curr;

	start = bu_vls_addr(&input_str);
	curr = start + input_str_index;

	/* skip spaces */
	while(*curr != '\0' && *curr == ' ')
	  ++curr;

	/* find next space */
	while(*curr != '\0' && *curr != ' ')
	  ++curr;

	input_str_index = curr - start;
	bu_vls_init(&temp);
	bu_vls_strcat(&temp, start+input_str_index);
	bu_vls_trunc(&input_str, input_str_index);
	pr_prompt();
	bu_log("%S", &input_str);
	bu_vls_vlscat(&input_str, &temp);
	bu_vls_free(&temp);
      }else
	mged_insert_char(ch);

      escaped = bracketed = 0;
      break;
    case 'b':
      if (escaped) {                /* backward-word */
	char *start;
	char *curr;

	start = bu_vls_addr(&input_str);
	curr = start + input_str_index - 1;

	/* skip spaces */
	while(curr > start && *curr == ' ')
	  --curr;

	/* find next space */
	while(curr > start && *curr != ' ')
	  --curr;

	if(curr == start)
	  input_str_index = 0;
	else
	  input_str_index = curr - start + 1;

	bu_vls_init(&temp);
	bu_vls_strcat(&temp, start+input_str_index);
	bu_vls_trunc(&input_str, input_str_index);
	pr_prompt();
	bu_log("%S", &input_str);
	bu_vls_vlscat(&input_str, &temp);
	bu_vls_free(&temp);
      }else
	mged_insert_char(ch);

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

	mged_insert_char(ch);
	escaped = bracketed = 0;
	break;
    }
#ifdef TRY_STDIN_INPUT_HACK
    }
    }
#endif	
}

static void
mged_insert_char(ch)
char ch;
{
  if (input_str_index == bu_vls_strlen(&input_str)) {
    bu_log("%c", (int)ch);
    bu_vls_putc(&input_str, (int)ch);
    ++input_str_index;
  } else {
    struct bu_vls temp;

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

  if(classic_mged){
    bu_log("\r%s\n", argv[1]);
    pr_prompt();
    bu_log("%s", bu_vls_addr(&input_str));
    pr_prompt();
    for(i = 0; i < input_str_index; ++i)
      bu_log("%c", bu_vls_addr(&input_str)[i]);
  }

  return TCL_OK;
}

void
std_out_or_err(clientData, mask)
ClientData clientData;
int mask;
{
  int fd = (int)clientData;
  int count;
  struct bu_vls vls;
  char line[MAXLINE];

  /* Get data from stdout or stderr */
  if((count = read((int)fd, line, MAXLINE)) == 0)
    return;

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "distribute_text {} {} %*s", count, line);
  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  return;
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
    int save_edflag;

    /* Let cool Tk event handler do most of the work */

    if (non_blocking) {

	/* When in non_blocking-mode, we want to deal with as many events
	   as possible before the next redraw (multiple keypresses, redraw
	   events, etc... */
	
	while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
    } else {
      /* Wait for an event, then handle it */
      Tcl_DoOneEvent(TCL_ALL_EVENTS);

      /* Handle any other events in the queue */
      while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
    }
    
    non_blocking = 0;

    /*********************************
     *  Handle rate-based processing *
     *********************************/
    save_dm_list = curr_dm_list;
    if( edit_rateflag_model_rotate ) {
      struct bu_vls vls;
      char save_ecoords;

      curr_dm_list = edit_rate_mr_dm_list;
      save_ecoords = mged_variables->ecoords;
      mged_variables->ecoords = 'm';

      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_ROTATE)
	  es_edflag = SROT;
      }else{
	save_edflag = edobj;
	edobj = BE_O_ROTATE;
      }

      non_blocking++;
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "knob -o %c -i -e ax %f ay %f az %f\n",
		    edit_rate_model_origin,
		    edit_rate_model_rotate[X],
		    edit_rate_model_rotate[Y],
		    edit_rate_model_rotate[Z]);
	
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      mged_variables->ecoords = save_ecoords;

      if(state == ST_S_EDIT)
	es_edflag = save_edflag;
      else
	edobj = save_edflag;
    }
    if( edit_rateflag_object_rotate ) {
      struct bu_vls vls;
      char save_ecoords;

      curr_dm_list = edit_rate_or_dm_list;
      save_ecoords = mged_variables->ecoords;
      mged_variables->ecoords = 'o';

      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_ROTATE)
	  es_edflag = SROT;
      }else{
	save_edflag = edobj;
	edobj = BE_O_ROTATE;
      }

      non_blocking++;
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "knob -o %c -i -e ax %f ay %f az %f\n",
		    edit_rate_object_origin,
		    edit_rate_object_rotate[X],
		    edit_rate_object_rotate[Y],
		    edit_rate_object_rotate[Z]);
	
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      mged_variables->ecoords = save_ecoords;

      if(state == ST_S_EDIT)
	es_edflag = save_edflag;
      else
	edobj = save_edflag;
    }
    if( edit_rateflag_view_rotate ) {
      struct bu_vls vls;
      char save_ecoords;

      curr_dm_list = edit_rate_vr_dm_list;
      save_ecoords = mged_variables->ecoords;
      mged_variables->ecoords = 'v';

      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_ROTATE)
	  es_edflag = SROT;
      }else{
	save_edflag = edobj;
	edobj = BE_O_ROTATE;
      }

      non_blocking++;
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "knob -o %c -i -e ax %f ay %f az %f\n",
		    edit_rate_view_origin,
		    edit_rate_view_rotate[X],
		    edit_rate_view_rotate[Y],
		    edit_rate_view_rotate[Z]);
	
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      mged_variables->ecoords = save_ecoords;

      if(state == ST_S_EDIT)
	es_edflag = save_edflag;
      else
	edobj = save_edflag;
    }
    if( edit_rateflag_model_tran ) {
      char save_ecoords;
      struct bu_vls vls;

      curr_dm_list = edit_rate_mt_dm_list;
      save_ecoords = mged_variables->ecoords;
      mged_variables->ecoords = 'm';

      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_TRAN)
	  es_edflag = STRANS;
      }else{
	save_edflag = edobj;
	edobj = BE_O_XY;
      }

      non_blocking++;
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "knob -i -e aX %f aY %f aZ %f\n",
		    edit_rate_model_tran[X] * 0.05 * Viewscale * base2local,
		    edit_rate_model_tran[Y] * 0.05 * Viewscale * base2local,
		    edit_rate_model_tran[Z] * 0.05 * Viewscale * base2local);
	
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      mged_variables->ecoords = save_ecoords;

      if(state == ST_S_EDIT)
	es_edflag = save_edflag;
      else
	edobj = save_edflag;
    }
    if( edit_rateflag_view_tran ) {
      char save_ecoords;
      struct bu_vls vls;

      curr_dm_list = edit_rate_vt_dm_list;
      save_ecoords = mged_variables->ecoords;
      mged_variables->ecoords = 'v';

      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_TRAN)
	  es_edflag = STRANS;
      }else{
	save_edflag = edobj;
	edobj = BE_O_XY;
      }

      non_blocking++;
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "knob -i -e aX %f aY %f aZ %f\n",
		    edit_rate_view_tran[X] * 0.05 * Viewscale * base2local,
		    edit_rate_view_tran[Y] * 0.05 * Viewscale * base2local,
		    edit_rate_view_tran[Z] * 0.05 * Viewscale * base2local);
	
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      mged_variables->ecoords = save_ecoords;

      if(state == ST_S_EDIT)
	es_edflag = save_edflag;
      else
	edobj = save_edflag;
    }
    if( edit_rateflag_scale ) {
      struct bu_vls vls;

      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_SCALE)
	  es_edflag = SSCALE;
      }else{
	save_edflag = edobj;
	if(!OEDIT_SCALE)
	  edobj = BE_O_SCALE;
      }

      non_blocking++;
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "knob -i -e aS %f\n", edit_rate_scale * 0.01);
	
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      if(state == ST_S_EDIT)
	es_edflag = save_edflag;
      else
	edobj = save_edflag;
    }

    FOR_ALL_DISPLAYS(p, &head_dm_list.l){
      if(!p->_owner)
	continue;

      curr_dm_list = p;

      if( rateflag_model_rotate ) {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -o %c -i -m ax %f ay %f az %f\n",
		      rate_model_origin,
		      rate_model_rotate[X],
		      rate_model_rotate[Y],
		      rate_model_rotate[Z]);
	
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }
      if( rateflag_model_tran ) {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -i -m aX %f aY %f aZ %f\n",
		      rate_model_tran[X] * 0.05 * Viewscale * base2local,
		      rate_model_tran[Y] * 0.05 * Viewscale * base2local,
		      rate_model_tran[Z] * 0.05 * Viewscale * base2local);

	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }
      if( rateflag_rotate )  {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -o %c -i -v ax %f ay %f az %f\n",
		      rate_origin,
		      rate_rotate[X],
		      rate_rotate[Y],
		      rate_rotate[Z]);

	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }
      if( rateflag_tran )  {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -i -v aX %f aY %f aZ %f",
		      rate_tran[X] * 0.05 * Viewscale * base2local,
		      rate_tran[Y] * 0.05 * Viewscale * base2local,
		      rate_tran[Z] * 0.05 * Viewscale * base2local);

	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }
      if( rateflag_scale )  {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "zoom %f",
		      1.0 / (1.0 - (rate_scale / 10.0)));
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
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
  struct dm_list *p;
  struct dm_list *save_dm_list;
  struct bu_vls overlay_vls;
  struct bu_vls tmp_vls;
  register int do_overlay = 1;

  save_dm_list = curr_dm_list;
  FOR_ALL_DISPLAYS(p, &head_dm_list.l){
    double	elapsed_time;

    /*
     * if something has changed, then go update the display.
     * Otherwise, we are happy with the view we have
     */
    curr_dm_list = p;
    if(mapped && (update_views || dmaflag || dirty)) {
      if(dbip != DBI_NULL){
	if(do_overlay){
	  bu_vls_init(&overlay_vls);
	  bu_vls_init(&tmp_vls);
	  create_text_overlay(&overlay_vls);
	  do_overlay = 0;
	}

	/* XXX VR hack */
	if( viewpoint_hook )  (*viewpoint_hook)();
      }

      if( mged_variables->predictor )
	predictor_frame();

      rt_prep_timer();
      elapsed_time = -1;		/* timer running */

      DM_DRAW_BEGIN(dmp);	/* update displaylist prolog */

      if(dbip != DBI_NULL){
	/*  Draw each solid in it's proper place on the screen
	 *  by applying zoom, rotation, & translation.
	 *  Calls DM_LOADMATRIX() and DM_DRAW_VLIST().
	 */
	if( dmp->dm_stereo == 0 || mged_variables->eye_sep_dist <= 0 )  {
	  /* Normal viewing */
	  dozoom(0);
	} else {
	  /* Stereo viewing */
	  dozoom(1);
	  dozoom(2);
	}

	/* Restore to non-rotated, full brightness */
	DM_NORMAL(dmp);

	/* Compute and display angle/distance cursor */
	if (mged_variables->adcflag)
	  adcursor();

	/* Display titles, etc., if desired */
	bu_vls_strcpy(&tmp_vls, bu_vls_addr(&overlay_vls));
	dotitles(&tmp_vls);
	bu_vls_trunc(&tmp_vls, 0);
      }

      /* Draw center dot */
      DM_SET_COLOR(dmp, DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B, 1);
      DM_DRAW_POINT_2D(dmp, 0, 0);

      DM_DRAW_END(dmp);

      dirty = 0;

      if (elapsed_time < 0)  {
	(void)rt_get_timer( (struct bu_vls *)0, &elapsed_time );
	/* Only use reasonable measurements */
	if( elapsed_time > 1.0e-5 && elapsed_time < 30 )  {
	  /* Smoothly transition to new speed */
	  frametime = 0.9 * frametime + 0.1 * elapsed_time;
	}
      }
    }
  }

  FOR_ALL_DISPLAYS(p, &head_dm_list.l)
    p->s_info->_dmaflag = 0;

  curr_dm_list = save_dm_list;
  update_views = 0;

  if(!do_overlay){
    bu_vls_free(&overlay_vls);
    bu_vls_free(&tmp_vls);
  }
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

	FOR_ALL_DISPLAYS(p, &head_dm_list.l){
	  curr_dm_list = p;

	  DM_CLOSE(dmp);
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

  curr_cmd_list->quote_string = 0;

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
	bn_mat_mul( model2view, Viewrot, toViewcenter );
	model2view[15] = Viewscale;
	bn_mat_inv( view2model, model2view );

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
	  bn_aet_vec( &curr_dm_list->s_info->azimuth,
		       &curr_dm_list->s_info->elevation,
		       &curr_dm_list->s_info->twist,
		       temp , temp1 , (fastf_t)0.005 );
#if 1
	  /* Force azimuth range to be [0,360] */
	  if((NEAR_ZERO(curr_dm_list->s_info->elevation - 90.0,(fastf_t)0.005) ||
	     NEAR_ZERO(curr_dm_list->s_info->elevation + 90.0,(fastf_t)0.005)) &&
	     curr_dm_list->s_info->azimuth < 0 &&
	     !NEAR_ZERO(curr_dm_list->s_info->azimuth,(fastf_t)0.005))
	    curr_dm_list->s_info->azimuth += 360.0;
	  else if(NEAR_ZERO(curr_dm_list->s_info->azimuth,(fastf_t)0.005))
	    curr_dm_list->s_info->azimuth = 0.0;
#else
	  /* Force azimuth range to be [-180,180] */
	  if(!NEAR_ZERO(curr_dm_list->s_info->elevation - 90.0,(fastf_t)0.005) &&
	      !NEAR_ZERO(curr_dm_list->s_info->elevation + 90.0,(fastf_t)0.005))
	    curr_dm_list->s_info->azimuth -= 180;
#endif
	}
#endif

	if( state != ST_VIEW ) {
	  bn_mat_mul( model2objview, model2view, modelchanges );
	  bn_mat_inv( objview2model, model2objview );
	}
	dmaflag = 1;
}

#ifdef DO_NEW_EDIT_MATS
void
new_edit_mats()
{
  register struct dm_list *p;
  struct dm_list *save_dm_list;

  save_dm_list = curr_dm_list;
  FOR_ALL_DISPLAYS(p, &head_dm_list.l){
    if(!p->_owner)
      continue;

    curr_dm_list = p;
    bn_mat_mul( model2objview, model2view, modelchanges );
    bn_mat_inv( objview2model, model2objview );
    dmaflag = 1;
  }

  curr_dm_list = save_dm_list;
}
#endif

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
#define RCFILE	".mgedrc"

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

			if( (fp = fopen(bu_vls_addr(&str), "r")) != NULL )
			  found = 1;
		}
	}

	if( !found ) {
		if( (fp = fopen( RCFILE, "r" )) != NULL )  {
			bu_vls_strcpy( &str, RCFILE );
			found = 1;
		}
	}

    /* At this point, if none of the above attempts panned out, give up. */

	if( !found ){
	  bu_vls_free(&str);
	  return -1;
	}

	bogus = 0;
	while( !feof(fp) ) {
	    char buf[80];

	    /* Get beginning of line */
	    fgets( buf, 80, fp );
    /* If the user has a set command with an equal sign, remember to warn */
	    if( strstr(buf, "set") != NULL )
	      if( strchr(buf, '=') != NULL ){
		    bogus = 1;
		    break;
	      }
	}
	
	fclose( fp );
	if( bogus ) {
	    bu_log("\nWARNING: The new format of the \"set\" command is:\n");
	    bu_log("    set varname value\n");
	    bu_log("If you are setting variables in your %s, you will ", RCFILE);
	    bu_log("need to change those\ncommands.\n\n");
	}
	if (Tcl_EvalFile( interp, bu_vls_addr(&str) ) != TCL_OK) {
	    bu_log("Error reading %s:\n%s\n", RCFILE,
		Tcl_GetVar(interp,"errorInfo", TCL_GLOBAL_ONLY) );
	}

	bu_vls_free(&str);
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
  int force_new = 0;
  struct db_i *save_dbip;
  struct bu_vls vls;

  if( argc <= 1 )  {
    /* Invoked without args, return name of current database */
    if( dbip != DBI_NULL )  {
      Tcl_AppendResult(interp, dbip->dbi_filename, (char *)NULL);
      return TCL_OK;
    }
    Tcl_AppendResult(interp, "opendb: No database presently open\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(3 < argc || (strlen(argv[1]) == 0)){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help opendb");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(argc == 3 &&
     strcmp("y", argv[2]) && strcmp("Y", argv[2]) &&
     strcmp("n", argv[2]) && strcmp("N", argv[2])){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help opendb");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  save_dbip = dbip;
  dbip = DBI_NULL;

  /* Get input file */
  if( ((dbip = db_open( argv[1], "r+w" )) == DBI_NULL ) &&
      ((dbip = db_open( argv[1], "r"   )) == DBI_NULL ) )  {
    char line[128];

    if( isatty(0) ) {
      if(first && classic_mged){
	perror( argv[1] );
	bu_log("Create new database (y|n)[n]? ");
	(void)fgets(line, sizeof(line), stdin);
	if( line[0] != 'y' && line[0] != 'Y' )
	  exit(0);                /* NOT finish() */
      } else {
	if(argc == 2){
	  /* need to reset this before returning */
	  dbip = save_dbip;
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Create new database (y|n)[n]? ",
			   (char *)NULL);
	  bu_vls_printf(&curr_cmd_list->more_default, "n");
	  return TCL_ERROR;
	}

	if( *argv[2] != 'y' && *argv[2] != 'Y' ){
	  dbip = save_dbip;
	  return TCL_OK;
	}
      }
    }

    Tcl_AppendResult(interp, "Creating new database \"", argv[1],
		     "\"\n", (char *)NULL);

    if( (dbip = db_create( argv[1] )) == DBI_NULL )  {
      perror( argv[1] );
      exit(2);		/* NOT finish() */
    }
  }

  if( save_dbip )  {
    char *av[2];

    av[0] = "zap";
    av[1] = NULL;

    /* Clear out anything in the display */
    f_zap(clientData, interp, 1, av);

    /* Close current database.  Releases MaterHead, etc. too. */
    db_close(save_dbip);

    log_event( "CEASE", "(close)" );
  }

  if( dbip->dbi_read_only )
    Tcl_AppendResult(interp, dbip->dbi_filename, ":  READ ONLY\n", (char *)NULL);

  /* Quick -- before he gets away -- write a logfile entry! */
  log_event( "START", argv[1] );

  if(first)
    first = 0;

  /* --- Scan geometry database and build in-memory directory --- */
  db_scan( dbip, (int (*)())db_diradd, 1);

  /* Close previous databases, if any.  Ignore errors. */
  (void)Tcl_Eval( interp, "db close; .inmem close" );

  /* Establish TCL access to both disk and in-memory databases */
  if( Tcl_Eval( interp, "set wdbp [wdb_open db disk [get_dbip]]; wdb_open .inmem inmem [get_dbip]" ) != TCL_OK )  {
	bu_log("%s\n%s\n",
    		interp->result,
		Tcl_GetVar(interp,"errorInfo", TCL_GLOBAL_ONLY) );
	return TCL_ERROR;
  }
  Tcl_ResetResult( interp );

  bu_vls_init(&vls);

  bu_vls_strcpy(&vls, "local2base");
  Tcl_LinkVar(interp, bu_vls_addr(&vls), (char *)&local2base,
	      TCL_LINK_DOUBLE|TCL_LINK_READ_ONLY);

  bu_vls_strcpy(&vls, "base2local");
  Tcl_LinkVar(interp, bu_vls_addr(&vls), (char *)&base2local,
	      TCL_LINK_DOUBLE|TCL_LINK_READ_ONLY);

  bu_vls_free(&vls);

  /* Print title/units information */
  if( interactive )
    Tcl_AppendResult(interp, dbip->dbi_title, " (units=",
		     units_str[dbip->dbi_localunit], ")\n", (char *)NULL);

  return TCL_OK;
}

int
bit_bucket(clientdata, str)
genptr_t clientdata;
genptr_t str;
{
  /* do nothing */
  return( 0 );
}
