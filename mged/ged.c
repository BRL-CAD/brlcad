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
static const char RCSid[] = "@(#)$Header$ (BRL)";
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
#include <sys/errno.h>

#include "tk.h"

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "mater.h"
#include "libtermio.h"
#include "./ged.h"
#include "./titles.h"
#include "./mged_solid.h"
#include "./sedit.h"
#include "./mged_dm.h"

#ifndef	LOGFILE
#define LOGFILE	"/vld/lib/gedlog"	/* usage log */
#endif

extern void view_ring_init(); /* defined in chgview.c */

extern void draw_e_axes();
extern void draw_m_axes();
extern void draw_v_axes();

extern void fb_tclInit();  /* from in libfb/tcl.c */
extern int fb_refresh();

extern void draw_grid();		/* grid.c */

extern void draw_rect();		/* rect.c */
extern void paint_rect_area();

/* defined in predictor.c */
extern void predictor_init();

/* defined in cmd.c */
extern Tcl_Interp *interp;
extern Tk_Window tkwin;

/* defined in attach.c */
extern int mged_link_vars();

/* defined in chgmodel.c */
extern void set_localunit_TclVar();

/* defined in dodraw.c */
extern unsigned char geometry_default_color[];

/* defined in libdm/dm-Null.c */
extern struct dm dm_Null;

/* defined in set.c */
extern struct _mged_variables default_mged_variables;

/* defined in color_scheme.c */
extern struct _color_scheme default_color_scheme;

/* defined in grid.c */
extern struct _grid_state default_grid_state;

/* defined in axes.c */
extern struct _axes_state default_axes_state;

/* defined in rect.c */
extern struct _rubber_band default_rubber_band;

int pipe_out[2];
int pipe_err[2];
struct db_i *dbip = DBI_NULL;	/* database instance pointer */
struct rt_wdb *wdbp = RT_WDB_NULL;
int update_views = 0;
int (*cmdline_hook)() = NULL;
jmp_buf	jmp_env;		/* For non-local gotos */
double frametime;		/* time needed to draw last frame */

int             cmd_stuff_str();
void		(*cur_sigint)();	/* Current SIGINT status */
void		sig2(), sig3();
void		reset_input_strings();
void		new_mats();
void		usejoy();
void            slewview();
int		interactive = 0;	/* >0 means interactive */
int             cbreak_mode = 0;        /* >0 means in cbreak_mode */
int		classic_mged=0;
char		*dpy_string = (char *)NULL;
static int	mged_init_flag = 1;	/* >0 means in initialization stage */

struct bu_vls input_str, scratchline, input_str_prefix;
int input_str_index = 0;

static void     mged_insert_char();
static void	mged_process_char();
static int	do_rc();
static void	log_event();
extern char	version[];		/* from vers.c */

struct bn_tol	mged_tol;		/* calculation tolerance */

struct bu_vls mged_prompt;
void pr_prompt(), pr_beep();
int mged_bomb_hook();

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
	int	rateflag = 0;
	int	c;
	int	read_only_flag=0;

	while ((c = bu_getopt(argc, argv, "d:hicnrx:X:")) != EOF)
	{
		switch( c )
		{
			case 'd':
				dpy_string = bu_optarg;
				break;
			case 'r':
				read_only_flag = 1;
				break;
			case 'n':		/* "not new" == "classic" */
			case 'c':
				classic_mged = 1;
				break;
			case 'x':
				sscanf( bu_optarg, "%x", &rt_g.debug );
				break;
			case 'X':
	                        sscanf( bu_optarg, "%x", &bu_debug );
				break;
			default:
				fprintf( stdout, "Unrecognized option (%c)\n", c );
				/* Fall through to help */
			case 'h':
				fprintf(stdout, "Usage:  %s [-c] [-d display] [-h] [-r] [-x#] [-X#] [database [command]]\n", argv[0]);
				fflush(stdout);
				return(1);
		}
	}

	argc -= (bu_optind - 1);
	argv += (bu_optind - 1);

	/* Identify ourselves if interactive */
	if( argc <= 2 )  {
	  if( isatty(fileno(stdin)) && isatty(fileno(stdout)) )
	    interactive = 1;

	  if(interactive && classic_mged){
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

	(void)signal( SIGPIPE, SIG_IGN );

	/*
	 *  Sample and hold current SIGINT setting, so any commands that
	 *  might be run (e.g., by .mgedrc) which establish cur_sigint
	 *  as their signal handler get the initial behavior.
	 *  This will change after setjmp() is called, below.
	 */
	cur_sigint = signal( SIGINT, SIG_IGN );		/* sample */
	(void)signal( SIGINT, cur_sigint );		/* restore */

#if 1
	/* If multiple processors might be used, initialize for it.
	 * Do not run any commands before here.
	 * Do not use bu_log() or bu_malloc() before here.
	 */
	if( bu_avail_cpus() > 1 )  {
	  rt_g.rtg_parallel = 1;
	  bu_semaphore_init( RT_SEM_LAST );
	}
#endif

	/* Set up linked lists */
	BU_LIST_INIT(&HeadSolid.l);
	BU_LIST_INIT(&FreeSolid.l);
	BU_LIST_INIT(&rt_g.rtg_vlfree);
	BU_LIST_INIT(&rt_g.rtg_headwdb.l);
	BU_LIST_INIT(&head_run_rt.l);

	bzero((void *)&head_cmd_list, sizeof(struct cmd_list));
	BU_LIST_INIT(&head_cmd_list.l);
	bu_vls_init(&head_cmd_list.cl_name);
	bu_vls_init(&head_cmd_list.cl_more_default);
	bu_vls_strcpy(&head_cmd_list.cl_name, "mged");
	curr_cmd_list = &head_cmd_list;

	bzero((void *)&head_dm_list, sizeof(struct dm_list));
	BU_LIST_INIT( &head_dm_list.l );

	BU_GETSTRUCT(curr_dm_list, dm_list);
	BU_LIST_APPEND(&head_dm_list.l, &curr_dm_list->l);
	netfd = -1;

	/* initialize predictor stuff */
	BU_LIST_INIT(&curr_dm_list->dml_p_vlist);
	predictor_init();

	BU_GETSTRUCT(dmp, dm);
	*dmp = dm_Null;
	bu_vls_init(&pathName);
	bu_vls_init(&tkName);
	bu_vls_init(&dName);
	bu_vls_strcpy(&pathName, "nu");
	bu_vls_strcpy(&tkName, "nu");

	BU_GETSTRUCT(rubber_band, _rubber_band);
	*rubber_band = default_rubber_band;		/* struct copy */

	BU_GETSTRUCT(mged_variables, _mged_variables);
	*mged_variables = default_mged_variables;	/* struct copy */

	BU_GETSTRUCT(color_scheme, _color_scheme);
	*color_scheme = default_color_scheme;		/* struct copy */

	BU_GETSTRUCT(grid_state, _grid_state);
	*grid_state = default_grid_state;		/* struct copy */

	BU_GETSTRUCT(axes_state, _axes_state);
	*axes_state = default_axes_state;		/* struct copy */

	BU_GETSTRUCT(adc_state, _adc_state);
	adc_state->adc_rc = 1;
	adc_state->adc_a1 = adc_state->adc_a2 = 45.0;

	BU_GETSTRUCT(menu_state, _menu_state);
	menu_state->ms_rc = 1;

	BU_GETSTRUCT(dlist_state, _dlist_state);
	dlist_state->dl_rc = 1;

	BU_GETSTRUCT(view_state, _view_state);
	view_state->vs_rc = 1;
	view_ring_init(curr_dm_list->dml_view_state, (struct _view_state *)NULL);
	/* init rotation matrix */
	view_state->vs_Viewscale = 500;		/* => viewsize of 1000mm (1m) */
	bn_mat_idn( view_state->vs_Viewrot );
	bn_mat_idn( view_state->vs_toViewcenter );
	bn_mat_idn( view_state->vs_ModelDelta );
	MAT_DELTAS_GET_NEG(view_state->vs_orig_pos, view_state->vs_toViewcenter);
	view_state->vs_i_Viewscale = view_state->vs_Viewscale;

	am_mode = AMM_IDLE;
	owner = 1;
	frametime = 1;

	bn_mat_idn( identity );		/* Handy to have around */
	bn_mat_idn( modelchanges );
	bn_mat_idn( acc_rot_sol );

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

	rt_init_resource( &rt_uniresource, 0, NULL );

	rt_prep_timer();		/* Initialize timer */

	new_mats();

	es_edflag = -1;		/* no solid editing just now */

	bu_vls_init(&input_str);
	bu_vls_init(&input_str_prefix);
	bu_vls_init(&scratchline);
	bu_vls_init(&mged_prompt);
	input_str_index = 0;

	/* Get set up to use Tcl */
	mged_setup();
	mmenu_init();
	btn_head_menu(0,0,0);
	mged_link_vars(curr_dm_list);

	{
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "set version \"%s\"", version+5);
	  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	}

	setview( 0.0, 0.0, 0.0 );

	if(dpy_string == (char *)NULL)
	  dpy_string = getenv("DISPLAY");

	if(interactive && !classic_mged){
	  int status;
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  if(dpy_string != (char *)NULL)
	    bu_vls_printf(&vls, "loadtk %s", dpy_string);
	  else
	    bu_vls_strcpy(&vls, "loadtk");

	  status = Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);

	  if (status != TCL_OK) {
	    bu_log("%s", interp->result);
	    exit(1);
	  }
	}

	if(argc >= 2){
	  /* Open the database, attach a display manager */
	  /* Command line may have more than 2 args, opendb only wants 2 */
	  if(f_opendb( (ClientData)NULL, interp, 2, argv ) == TCL_ERROR)
	    mged_finish(1);
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
#if 1
	  /*
	   * Initialze variables here in case the user specified changes
	   * to the defaults in their .mgedrc file.
	   */
#endif

	  if(classic_mged)
	    get_attached();
	  else{

	    if ((fork()) == 0){
	      struct bu_vls vls;
	      int status;

	      /* make this a process group leader */
	      setpgid(0, 0);

	      bu_vls_init(&vls);
	      bu_vls_strcpy(&vls, "gui");
	      status = Tcl_Eval(interp, bu_vls_addr(&vls));
	      bu_vls_free(&vls);

	      if (status != TCL_OK) {
		bu_log("%s", interp->result);
		exit(1);
	      }

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

#if 0
	      /* close stdin */
	      (void)close(0);
#endif
	    }else{
	      exit(0);
	    }

	    bu_add_hook(&bu_bomb_hook_list, mged_bomb_hook, GENPTR_NULL);
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

	if(classic_mged || !interactive){
	  Tcl_CreateFileHandler(STDIN_FILENO, TCL_READABLE,
				stdin_input, (ClientData)STDIN_FILENO);
	  (void)signal( SIGINT, SIG_IGN );

	  bu_vls_strcpy(&mged_prompt, MGED_PROMPT);
	  pr_prompt();

	  if (cbreak_mode) {
	    set_Cbreak(fileno(stdin));
	    clr_Echo(fileno(stdin));
	  }
	}else{
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "output_hook output_callback");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);

	  /* to catch output from routines that do not use bu_log */
	  Tcl_CreateFileHandler(pipe_out[0], TCL_READABLE,
				std_out_or_err, (ClientData)pipe_out[0]);
	  Tcl_CreateFileHandler(pipe_err[0], TCL_READABLE,
				std_out_or_err, (ClientData)pipe_err[0]);
	}

	mged_init_flag = 0;	/* all done with initialization */

	/****************  M A I N   L O O P   *********************/
	while(1) {
		/* This test stops optimizers from complaining about an infinite loop */
		if( (rateflag = event_check( rateflag )) < 0 )  break;

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
    struct bu_vls temp;
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
	if(!bu_vls_strlen(&input_str) && bu_vls_strlen(&curr_cmd_list->cl_more_default))
	  bu_vls_printf(&input_str_prefix, "%s%S\n",
			bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
			&curr_cmd_list->cl_more_default);
	else
	  bu_vls_printf(&input_str_prefix, "%s%S\n",
			bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
			&input_str);

	bu_vls_trunc(&curr_cmd_list->cl_more_default, 0);

	/* If a complete line was entered, attempt to execute command. */
	
	if (Tcl_CommandComplete(bu_vls_addr(&input_str_prefix))) {
	    curr_cmd_list = &head_cmd_list;
	    if(curr_cmd_list->cl_tie)
	      curr_dm_list = curr_cmd_list->cl_tie;
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

#ifdef TRY_STDIN_INPUT_HACK
    /* Process everything in buf */
    for(index = 0, ch = buf[index]; index < count; ch = buf[++index]){
#endif
      mged_process_char(ch);
#ifdef TRY_STDIN_INPUT_HACK
    }
    }
#endif	
}

/* Process character */
static void
mged_process_char(ch)
char ch;
{
  struct bu_vls *vp;
  struct bu_vls temp;
  static int escaped = 0;
  static int bracketed = 0;
  static int freshline = 1;

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
    if(!bu_vls_strlen(&input_str) && bu_vls_strlen(&curr_cmd_list->cl_more_default))
      bu_vls_printf(&input_str_prefix, "%s%S\n",
		    bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
		    &curr_cmd_list->cl_more_default);
    else {
      if (curr_cmd_list->cl_quote_string)
	bu_vls_printf(&input_str_prefix, "%s\"%S\"\n",
		      bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
		      &input_str);
      else
	bu_vls_printf(&input_str_prefix, "%s%S\n",
		      bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
		      &input_str);
    }

    curr_cmd_list->cl_quote_string = 0;
    bu_vls_trunc(&curr_cmd_list->cl_more_default, 0);

    /* If this forms a complete command (as far as the Tcl parser is
       concerned) then execute it. */
	
    if (Tcl_CommandComplete(bu_vls_addr(&input_str_prefix))) {
      curr_cmd_list = &head_cmd_list;
      if(curr_cmd_list->cl_tie)
	curr_dm_list = curr_cmd_list->cl_tie;
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

  if(argc != 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helpdevel stuff_str");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

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
  int fd = (int)((long)clientData & 0xFFFF);	/* fd's will be small */
  int count;
  struct bu_vls vls;
  char line[MAXLINE];

  /* Get data from stdout or stderr */
#if 1
  if((count = read((int)fd, line, MAXLINE)) == 0)
#else
  if((count = read((int)fd, line, 5120)) == 0)
#endif
    return;

  line[count] = '\0';

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "output_callback {%s}", line);
  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
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
event_check( int non_blocking )
{
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
      char save_coords;

      curr_dm_list = edit_rate_mr_dm_list;
      save_coords = mged_variables->mv_coords;
      mged_variables->mv_coords = 'm';

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

      mged_variables->mv_coords = save_coords;

      if(state == ST_S_EDIT)
	es_edflag = save_edflag;
      else
	edobj = save_edflag;
    }
    if( edit_rateflag_object_rotate ) {
      struct bu_vls vls;
      char save_coords;

      curr_dm_list = edit_rate_or_dm_list;
      save_coords = mged_variables->mv_coords;
      mged_variables->mv_coords = 'o';

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

      mged_variables->mv_coords = save_coords;

      if(state == ST_S_EDIT)
	es_edflag = save_edflag;
      else
	edobj = save_edflag;
    }
    if( edit_rateflag_view_rotate ) {
      struct bu_vls vls;
      char save_coords;

      curr_dm_list = edit_rate_vr_dm_list;
      save_coords = mged_variables->mv_coords;
      mged_variables->mv_coords = 'v';

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

      mged_variables->mv_coords = save_coords;

      if(state == ST_S_EDIT)
	es_edflag = save_edflag;
      else
	edobj = save_edflag;
    }
    if( edit_rateflag_model_tran ) {
      char save_coords;
      struct bu_vls vls;

      curr_dm_list = edit_rate_mt_dm_list;
      save_coords = mged_variables->mv_coords;
      mged_variables->mv_coords = 'm';

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
		    edit_rate_model_tran[X] * 0.05 * view_state->vs_Viewscale * base2local,
		    edit_rate_model_tran[Y] * 0.05 * view_state->vs_Viewscale * base2local,
		    edit_rate_model_tran[Z] * 0.05 * view_state->vs_Viewscale * base2local);
	
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      mged_variables->mv_coords = save_coords;

      if(state == ST_S_EDIT)
	es_edflag = save_edflag;
      else
	edobj = save_edflag;
    }
    if( edit_rateflag_view_tran ) {
      char save_coords;
      struct bu_vls vls;

      curr_dm_list = edit_rate_vt_dm_list;
      save_coords = mged_variables->mv_coords;
      mged_variables->mv_coords = 'v';

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
		    edit_rate_view_tran[X] * 0.05 * view_state->vs_Viewscale * base2local,
		    edit_rate_view_tran[Y] * 0.05 * view_state->vs_Viewscale * base2local,
		    edit_rate_view_tran[Z] * 0.05 * view_state->vs_Viewscale * base2local);
	
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      mged_variables->mv_coords = save_coords;

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
      if(!p->dml_owner)
	continue;

      curr_dm_list = p;

      if( view_state->vs_rateflag_model_rotate ) {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -o %c -i -m ax %f ay %f az %f\n",
		      view_state->vs_rate_model_origin,
		      view_state->vs_rate_model_rotate[X],
		      view_state->vs_rate_model_rotate[Y],
		      view_state->vs_rate_model_rotate[Z]);
	
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }
      if( view_state->vs_rateflag_model_tran ) {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -i -m aX %f aY %f aZ %f\n",
		      view_state->vs_rate_model_tran[X] * 0.05 * view_state->vs_Viewscale * base2local,
		      view_state->vs_rate_model_tran[Y] * 0.05 * view_state->vs_Viewscale * base2local,
		      view_state->vs_rate_model_tran[Z] * 0.05 * view_state->vs_Viewscale * base2local);

	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }
      if( view_state->vs_rateflag_rotate )  {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -o %c -i -v ax %f ay %f az %f\n",
		      view_state->vs_rate_origin,
		      view_state->vs_rate_rotate[X],
		      view_state->vs_rate_rotate[Y],
		      view_state->vs_rate_rotate[Z]);

	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }
      if( view_state->vs_rateflag_tran )  {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "knob -i -v aX %f aY %f aZ %f",
		      view_state->vs_rate_tran[X] * 0.05 * view_state->vs_Viewscale * base2local,
		      view_state->vs_rate_tran[Y] * 0.05 * view_state->vs_Viewscale * base2local,
		      view_state->vs_rate_tran[Z] * 0.05 * view_state->vs_Viewscale * base2local);

	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
      }
      if( view_state->vs_rateflag_scale )  {
	struct bu_vls vls;

	non_blocking++;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "zoom %f",
		      1.0 / (1.0 - (view_state->vs_rate_scale / 10.0)));
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
  double elapsed_time;
  int do_time = 0;

  rt_prep_timer();

  FOR_ALL_DISPLAYS(p, &head_dm_list.l)
	  if (update_views || p->dml_view_state->vs_flag)
		  p->dml_dirty = 1;

  /*
   * This needs to be done separately
   * because dml_view_state may be shared.
   */
  FOR_ALL_DISPLAYS(p, &head_dm_list.l)
	  p->dml_view_state->vs_flag = 0;

  update_views = 0;

  save_dm_list = curr_dm_list;
  FOR_ALL_DISPLAYS(p, &head_dm_list.l){
    /*
     * if something has changed, then go update the display.
     * Otherwise, we are happy with the view we have
     */
    curr_dm_list = p;
    if (mapped && dirty) {
      dirty = 0;
      do_time = 1;
      VMOVE(geometry_default_color,color_scheme->cs_geo_def);

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

      if( mged_variables->mv_predictor )
	predictor_frame();

      DM_DRAW_BEGIN(dmp);	/* update displaylist prolog */

      if (dbip != DBI_NULL) {
	      /* do framebuffer underlay */
	      if (mged_variables->mv_fb && !mged_variables->mv_fb_overlay) {
		      if (mged_variables->mv_fb_all)
			      fb_refresh(fbp, 0, 0, dmp->dm_width, dmp->dm_height);
		      else if (mged_variables->mv_mouse_behavior != 'z')
			      paint_rect_area();
	      }

	      /* do framebuffer overlay for entire window */
	      if (mged_variables->mv_fb &&
		  mged_variables->mv_fb_overlay &&
		  mged_variables->mv_fb_all) {
		      fb_refresh(fbp, 0, 0, dmp->dm_width, dmp->dm_height);
	      } else {
		      /*  Draw each solid in it's proper place on the screen
		       *  by applying zoom, rotation, & translation.
		       *  Calls DM_LOADMATRIX() and DM_DRAW_VLIST().
		       */

		      if (dmp->dm_stereo == 0 ||
			  mged_variables->mv_eye_sep_dist <= 0) {
			      /* Normal viewing */
			      dozoom(0);
		      } else {
			      /* Stereo viewing */
			      dozoom(1);
			      dozoom(2);
		      }

		      /* do framebuffer overlay in rectangular area */
		      if (mged_variables->mv_fb &&
			  mged_variables->mv_fb_overlay &&
			  mged_variables->mv_mouse_behavior != 'z')
			      paint_rect_area();
	      }


	      /* Restore to non-rotated, full brightness */
	      DM_NORMAL(dmp);

	      /* only if not doing overlay */
	      if (!mged_variables->mv_fb ||
		  mged_variables->mv_fb_overlay != 2) {
		      if (rubber_band->rb_active || rubber_band->rb_draw)
			      draw_rect();

		      if (grid_state->gr_draw)
			      draw_grid();

		      /* Compute and display angle/distance cursor */
		      if (adc_state->adc_draw)
			      adcursor();

		      if (axes_state->ax_view_draw)
			      draw_v_axes();

		      if (axes_state->ax_model_draw)
			      draw_m_axes();

		      if (axes_state->ax_edit_draw &&
			  (state == ST_S_EDIT || state == ST_O_EDIT))
			      draw_e_axes();

		      /* Display titles, etc., if desired */
		      bu_vls_strcpy(&tmp_vls, bu_vls_addr(&overlay_vls));
		      dotitles(&tmp_vls);
		      bu_vls_trunc(&tmp_vls, 0);
	      }
      }

      /* only if not doing overlay */
      if (!mged_variables->mv_fb ||
	  mged_variables->mv_fb_overlay != 2) {
	      /* Draw center dot */
	      DM_SET_FGCOLOR(dmp,
			     color_scheme->cs_center_dot[0],
			     color_scheme->cs_center_dot[1],
			     color_scheme->cs_center_dot[2], 1);
	      DM_DRAW_POINT_2D(dmp, 0.0, 0.0);
      }

      DM_DRAW_END(dmp);
    }
  }

  /* a frame was drawn */
  if(do_time){
    (void)rt_get_timer( (struct bu_vls *)0, &elapsed_time );
    /* Only use reasonable measurements */
    if( elapsed_time > 1.0e-5 && elapsed_time < 30 )  {
      /* Smoothly transition to new speed */
      frametime = 0.9 * frametime + 0.1 * elapsed_time;
    }
  }

  curr_dm_list = save_dm_list;

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
	struct bu_vls line;
	time_t now;
	char *timep;
	int logfd;

	(void)time( &now );
	timep = ctime( &now );	/* returns 26 char string */
	timep[24] = '\0';	/* Chop off \n */

	bu_vls_init(&line);
	bu_vls_printf(&line, "%s [%s] time=%ld uid=%d (%s) %s\n",
		      event,
		      dmp->dm_name,
		      (long)now,
		      getuid(),
		      timep,
		      arg
	);

	if( (logfd = open( LOGFILE, O_WRONLY|O_APPEND )) >= 0 )  {
		(void)write( logfd, bu_vls_addr(&line), (unsigned)bu_vls_strlen(&line) );
		(void)close( logfd );
	}

	bu_vls_free(&line);
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

	/* Release all displays */
	FOR_ALL_DISPLAYS(p, &head_dm_list.l){
	  curr_dm_list = p;

	  DM_CLOSE(dmp);
	}

	/* Be certain to close the database cleanly before exiting */
	Tcl_Eval(interp, "db close; .inmem close");
	if( wdbp )  wdb_close(wdbp);
	if( dbip )  db_close(dbip);

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
  reset_input_strings();

  (void)signal( SIGINT, SIG_IGN );
}

void
sig3()
{
  (void)signal( SIGINT, SIG_IGN );
  longjmp( jmp_env, 1 );
}

void
reset_input_strings()
{
  if(BU_LIST_IS_HEAD(curr_cmd_list, &head_cmd_list.l)){
    /* Truncate input string */
    bu_vls_trunc(&input_str, 0);
    bu_vls_trunc(&input_str_prefix, 0);
    bu_vls_trunc(&curr_cmd_list->cl_more_default, 0);
    input_str_index = 0;

    curr_cmd_list->cl_quote_string = 0;
    bu_vls_strcpy(&mged_prompt, MGED_PROMPT);
    bu_log("\n");
    pr_prompt();
  }else{
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "reset_input_strings");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
  }
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
	bn_mat_mul( view_state->vs_model2view, view_state->vs_Viewrot, view_state->vs_toViewcenter );
	view_state->vs_model2view[15] = view_state->vs_Viewscale;
	bn_mat_inv( view_state->vs_view2model, view_state->vs_model2view );

#if 1
	{
	  vect_t work, work1;
	  vect_t temp, temp1;

	  /* Find current azimuth, elevation, and twist angles */
	  VSET( work , 0.0, 0.0, 1.0 );       /* view z-direction */
	  MAT4X3VEC( temp , view_state->vs_view2model , work );
	  VSET( work1 , 1.0, 0.0, 0.0 );      /* view x-direction */
	  MAT4X3VEC( temp1 , view_state->vs_view2model , work1 );

	  /* calculate angles using accuracy of 0.005, since display
	   * shows 2 digits right of decimal point */
	  bn_aet_vec( &view_state->vs_azimuth,
		       &view_state->vs_elevation,
		       &view_state->vs_twist,
		       temp , temp1 , (fastf_t)0.005 );
#if 1
	  /* Force azimuth range to be [0,360] */
	  if((NEAR_ZERO(view_state->vs_elevation - 90.0,(fastf_t)0.005) ||
	     NEAR_ZERO(view_state->vs_elevation + 90.0,(fastf_t)0.005)) &&
	     view_state->vs_azimuth < 0 &&
	     !NEAR_ZERO(view_state->vs_azimuth,(fastf_t)0.005))
	    view_state->vs_azimuth += 360.0;
	  else if(NEAR_ZERO(view_state->vs_azimuth,(fastf_t)0.005))
	    view_state->vs_azimuth = 0.0;
#else
	  /* Force azimuth range to be [-180,180] */
	  if(!NEAR_ZERO(view_state->vs_elevation - 90.0,(fastf_t)0.005) &&
	      !NEAR_ZERO(view_state->vs_elevation + 90.0,(fastf_t)0.005))
	    view_state->vs_azimuth -= 180;
#endif
	}
#endif

	if( state != ST_VIEW ) {
	  bn_mat_mul( view_state->vs_model2objview, view_state->vs_model2view, modelchanges );
	  bn_mat_inv( view_state->vs_objview2model, view_state->vs_model2objview );
	}
	view_state->vs_flag = 1;
}

#ifdef DO_NEW_EDIT_MATS
void
new_edit_mats()
{
  register struct dm_list *p;
  struct dm_list *save_dm_list;

  save_dm_list = curr_dm_list;
  FOR_ALL_DISPLAYS(p, &head_dm_list.l){
    if(!p->dml_owner)
      continue;

    curr_dm_list = p;
    bn_mat_mul( view_state->vs_model2objview, view_state->vs_model2view, modelchanges );
    bn_mat_inv( view_state->vs_objview2model, view_state->vs_model2objview );
    view_state->vs_flag = 1;
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
	FILE	*fp = NULL;
	char	*path;
	struct	bu_vls str;
	int bogus;

	bu_vls_init( &str );

#define ENVRC	"MGED_RCFILE"
#define RCFILE	".mgedrc"

	if( (path = getenv(ENVRC)) != (char *)NULL ) {
		if ((fp = fopen(path, "r")) != NULL ) {
			bu_vls_strcpy( &str, path );
		}
	}

	if( !fp ) {
		if( (path = getenv("HOME")) != (char *)NULL )  {
			bu_vls_strcpy( &str, path );
			bu_vls_strcat( &str, "/" );
			bu_vls_strcat( &str, RCFILE );

			fp = fopen(bu_vls_addr(&str), "r");
		}
	}

	if( !fp ) {
		if( (fp = fopen( RCFILE, "r" )) != NULL )  {
			bu_vls_strcpy( &str, RCFILE );
		}
	}

    /* At this point, if none of the above attempts panned out, give up. */

	if( !fp ){
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
f_opendb(
	ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
	struct db_i *save_dbip;
	struct mater *save_materp;
	struct bu_vls vls;
	struct bu_vls msg;	/* use this to hold returned message */

	bu_vls_init(&vls);
	bu_vls_init(&msg);

	if( argc <= 1 )  {
		bu_vls_free(&vls);
		bu_vls_free(&msg);

		/* Invoked without args, return name of current database */
		if( dbip != DBI_NULL )  {
			Tcl_AppendResult(interp, dbip->dbi_filename, (char *)NULL);
			return TCL_OK;
		}

		Tcl_AppendResult(interp, "", (char *)NULL);
		return TCL_OK;
	}

	if(3 < argc || (strlen(argv[1]) == 0)){
		bu_vls_printf(&vls, "help opendb");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		bu_vls_free(&msg);
		return TCL_ERROR;
	}

	if(argc == 3 &&
	    strcmp("y", argv[2]) && strcmp("Y", argv[2]) &&
	    strcmp("n", argv[2]) && strcmp("N", argv[2])){
		bu_vls_printf(&vls, "help opendb");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		bu_vls_free(&msg);
		return TCL_ERROR;
	}

	save_dbip = dbip;
	dbip = DBI_NULL;
	save_materp = rt_material_head;
	rt_material_head = MATER_NULL;

	/* Get input file */
	if( ((dbip = db_open( argv[1], "r+w" )) == DBI_NULL ) &&
	    ((dbip = db_open( argv[1], "r"   )) == DBI_NULL ) )  {
		char line[128];

		/*
	         * Check to see if we can access the database
	         */
		if (access(argv[1], R_OK|W_OK) != 0 && errno != ENOENT) {
			perror(argv[1]);
			return TCL_ERROR;
		}

	    	/* File does not exist */
		if (interactive) {
			if(mged_init_flag){
				if(classic_mged){
					bu_log("Create new database (y|n)[n]? ");
					(void)fgets(line, sizeof(line), stdin);
					if( line[0] != 'y' && line[0] != 'Y' ) {
						bu_log("Warning: no database is currently opened!\n");
						bu_vls_free(&vls);
						bu_vls_free(&msg);
						return TCL_OK;
					}
				} else{
					int status;

					if(dpy_string != (char *)NULL)
						bu_vls_printf(&vls, "cad_dialog .createdb %s \"Create New Database?\" \"Create new database named %s?\" \"\" 0 Yes No Quit",
						    dpy_string, argv[1]);
					else
						bu_vls_printf(&vls, "cad_dialog .createdb :0 \"Create New Database?\" \"Create new database named %s?\" \"\" 0 Yes No Quit",
						    argv[1]);

					status = Tcl_Eval(interp, bu_vls_addr(&vls));

					if(status != TCL_OK || interp->result[0] == '2') {
						mged_finish(0);
					}

					if(interp->result[0] == '1') {
						bu_log("opendb: no database is currently opened!\n");
						bu_vls_free(&vls);
						bu_vls_free(&msg);
						return TCL_OK;
					}
				}
			} else { /* not initializing mged */
				if(argc == 2){
					/* need to reset this before returning */
					dbip = save_dbip;
					rt_material_head = save_materp;
					Tcl_AppendResult(interp, MORE_ARGS_STR, "Create new database (y|n)[n]? ",
					    (char *)NULL);
					bu_vls_printf(&curr_cmd_list->cl_more_default, "n");
					bu_vls_free(&vls);
					bu_vls_free(&msg);
					return TCL_ERROR;
				}

				if( *argv[2] != 'y' && *argv[2] != 'Y' ){
					dbip = save_dbip; /* restore previous database */
					rt_material_head = save_materp;
					bu_vls_free(&vls);
					bu_vls_free(&msg);
					return TCL_OK;
				}
			}
		}

	    	/* File does not exist, and should be created */
		if( (dbip = db_create( argv[1], 5 )) == DBI_NULL )  {
			dbip = save_dbip; /* restore previous database */
			rt_material_head = save_materp;
			bu_vls_free(&vls);
			bu_vls_free(&msg);

			if (mged_init_flag) {
				/* we need to use bu_log here */
				bu_log("opendb: failed to create %s\n", argv[1]);
				bu_log("opendb: no database is currently opened!\n");
				return TCL_OK;
			}

			Tcl_AppendResult(interp, "opendb: failed to create ", argv[1], "\n",\
		       (char *)NULL);
			if (dbip == DBI_NULL)
				Tcl_AppendResult(interp, "opendb: no database is currently opened!", \
			 (char *)NULL);

			return TCL_ERROR;
		}
	    	/* New database has already had db_dirbuild() by here */

		bu_vls_printf(&msg, "The new database %s was successfully created.\n", argv[1]);
	} else {
		/* Opened existing database file */

		/* Scan geometry database and build in-memory directory */
		(void)db_dirbuild( dbip );
	}

	if( save_dbip )  {
		char *av[2];
		struct db_i *new_dbip;
		struct mater *new_materp;

		av[0] = "zap";
		av[1] = NULL;

		new_dbip = dbip;
		dbip = save_dbip;
		new_materp = rt_material_head;
		rt_material_head = save_materp;

		/* Clear out anything in the display */
		f_zap(clientData, interp, 1, av);

		/* Close the Tcl database objects */
		Tcl_Eval(interp, "db close; .inmem close");
		wdb_close(wdbp);

		/* Close current database.  Releases MaterHead, etc. too. */
		db_close(dbip);
		dbip = new_dbip;
		rt_material_head = new_materp;

		log_event( "CEASE", "(close)" );
	}

	if( dbip->dbi_read_only )
		bu_vls_printf(&msg, "%s: READ ONLY\n", dbip->dbi_filename);

	/* Quick -- before he gets away -- write a logfile entry! */
	log_event( "START", argv[1] );

	/* Provide LIBWDB C access to the on-disk database */
	if( (wdbp = wdb_dbopen( dbip, RT_WDB_TYPE_DB_DISK )) == RT_WDB_NULL )  {
		Tcl_AppendResult(interp, "wdb_dbopen() failed?\n", (char *)NULL);
		return TCL_ERROR;
	}
	/* Establish LIBWDB TCL access to both disk and in-memory databases */
	/* This creates "db" and ".inmem" Tcl objects */
	bu_vls_strcpy(&vls, "set wdbp [wdb_open db disk [get_dbip]]; wdb_open .inmem inmem [get_dbip]");
	if( Tcl_Eval( interp, bu_vls_addr(&vls) ) != TCL_OK )  {
		bu_vls_printf(&msg, "%s\n%s\n",
		    interp->result,
		    Tcl_GetVar(interp,"errorInfo", TCL_GLOBAL_ONLY) );
		Tcl_AppendResult(interp, bu_vls_addr(&msg), (char *)NULL);
		bu_vls_free(&vls);
		bu_vls_free(&msg);
		return TCL_ERROR;
	}

	/* Perhaps do something special with the GUI */
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "opendb_callback %s", dbip->dbi_filename);
	(void)Tcl_Eval(interp, bu_vls_addr(&vls));

	bu_vls_strcpy(&vls, "local2base");
	Tcl_UnlinkVar(interp, bu_vls_addr(&vls));
	Tcl_LinkVar(interp, bu_vls_addr(&vls), (char *)&local2base,
	    TCL_LINK_DOUBLE|TCL_LINK_READ_ONLY);

	bu_vls_strcpy(&vls, "base2local");
	Tcl_UnlinkVar(interp, bu_vls_addr(&vls));
	Tcl_LinkVar(interp, bu_vls_addr(&vls), (char *)&base2local,
	    TCL_LINK_DOUBLE|TCL_LINK_READ_ONLY);

	set_localunit_TclVar();

	/* Print title/units information */
	if( interactive )
		bu_vls_printf(&msg, "%s (units=%s)\n", dbip->dbi_title,
		    bu_units_string(dbip->dbi_local2base) );

	Tcl_ResetResult( interp );
	Tcl_AppendResult(interp, bu_vls_addr(&msg), (char *)NULL);

	bu_vls_free(&vls);
	bu_vls_free(&msg);
	return TCL_OK;
}

int
mged_bomb_hook(clientData, str)
     genptr_t clientData;
     genptr_t str;
{
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "set mbh_dialog [Dialog .#auto -modality application];");
	bu_vls_printf(&vls, "$mbh_dialog hide 1; $mbh_dialog hide 2; $mbh_dialog hide 3;");
	bu_vls_printf(&vls, "label [$mbh_dialog childsite].l -text {%s};", str);
	bu_vls_printf(&vls, "pack [$mbh_dialog childsite].l;");
	bu_vls_printf(&vls, "update; $mbh_dialog activate");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	
	return TCL_OK;
}
	     
