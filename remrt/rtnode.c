/*
 *			R T N O D E . C
 *
 *  The per-node ray-tracing engine for the real-time ray-tracing project.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#if IRIX == 4
#define _BSD_COMPAT	1
#endif

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef HAVE_STDARG_H
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include <sys/time.h>

#ifndef SYSV
# include <sys/ioctl.h>
# include <sys/resource.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#include <signal.h>

#undef	VMIN
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "bu.h"
#include "externs.h"
#include "raytrace.h"
#include "pkg.h"
#include "fb.h"
#include "tcl.h"

#include "../librt/debug.h"
#include "../rt/material.h"
#include "../rt/ext.h"
#include "../rt/rdebug.h"

/***** Variables shared with viewing model *** */
FBIO		*fbp = FBIO_NULL;	/* Framebuffer handle */
FILE		*outfp = NULL;		/* optional pixel output file */
mat_t		view2model;
mat_t		model2view;
/***** end of sharing with viewing model *****/

extern void grid_setup();
extern void worker();

/***** variables shared with worker() ******/
struct application ap;
vect_t		left_eye_delta;
/***** end variables shared with worker() *****/

/***** variables shared with do.c *****/
char		*beginptr;		/* sbrk() at start of program */
/***** end variables shared with do.c *****/

/* Variables shared within mainline pieces */
extern fastf_t	rt_dist_tol;		/* Value for rti_tol.dist */
extern fastf_t	rt_perp_tol;		/* Value for rti_tol.perp */
int		rdebug;			/* RT program debugging (not library) */

/* State flags */
static int	seen_dirbuild;
static int	seen_gettrees;
static int	seen_matrix;

static char *title_file, *title_obj;	/* name of file and first object */

#define MAX_WIDTH	(16*1024)

static int	nlines_line;		/* how many scanlines worth in red_line */
static unsigned char	*red_line;
static unsigned char	*grn_line;
static unsigned char	*blu_line;
extern int	curframe;		/* shared with do.c */

static int	avail_cpus;		/* # of cpus avail on this system */
static int	max_cpus;		/* max # cpus for use, <= avail_cpus */

Tcl_Interp	*interp = NULL;

int print_on = 1;

/*
 *  Package handlers for the RTSYNC protocol.
 *  Numbered differently, to prevent confusion with other PKG protocols.
 */
#define RTSYNCMSG_PRINT	 999	/* StoM:  Diagnostic message */
#define RTSYNCMSG_ALIVE	1001	/* StoM:  protocol version, # of processors */
#define RTSYNCMSG_OPENFB 1002	/* both:  width height framebuffer */
#define RTSYNCMSG_DIRBUILD 1003	/* both:  database */
#define RTSYNCMSG_GETTREES 1004	/* both:  treetop(s) */
#define RTSYNCMSG_CMD	1006	/* MtoS:  Any Tcl command */
#define RTSYNCMSG_POV	1007	/* MtoS:  pov, min_res, start&end lines */
#define RTSYNCMSG_HALT	1008	/* MtoS:  abandon frame & xmit, NOW */
#define RTSYNCMSG_DONE	1009	/* StoM:  halt=0/1, res, elapsed, etc... */

void	ph_default();
void	rtsync_ph_pov();
void	rtsync_ph_openfb();
void	rtsync_ph_dirbuild();
void	rtsync_ph_gettrees();
void	rtsync_ph_halt();
void	rtsync_ph_cmd();
static struct pkg_switch rtsync_pkgswitch[] = {
	{ RTSYNCMSG_POV,	rtsync_ph_pov, "POV" },
	{ RTSYNCMSG_ALIVE,	ph_default,	"ALIVE" },
	{ RTSYNCMSG_HALT,	rtsync_ph_halt, "HALT" },
	{ RTSYNCMSG_CMD,	rtsync_ph_cmd, "CMD" },
	{ RTSYNCMSG_OPENFB,	rtsync_ph_openfb, "RTNODE open(ed) fb" },
	{ RTSYNCMSG_GETTREES,	rtsync_ph_gettrees, "RTNODE prep(ed) db" },
	{ RTSYNCMSG_DIRBUILD,	rtsync_ph_dirbuild, "RTNODE dirbuilt/built" },
	{ RTSYNCMSG_DONE,	ph_default,	"DONE" },
	{ RTSYNCMSG_PRINT,	ph_default,	"Log Message" },
	{ 0,			0,		(char *)0 }
};

struct pkg_conn *pcsrv;		/* PKG connection to server */
char		*control_host;	/* name of host running controller */
char		*tcp_port;	/* TCP port on control_host */

int debug = 0;		/* 0=off, 1=debug, 2=verbose */

int test_fb_speed = 0;
char	*framebuffer_name;

char srv_usage[] = "Usage: rtnode [-d] control-host tcp-port [cmd]\n";

/*
 *			C M D _ G E T _ P T R
 *
 *  Returns an appropriately-formatted string that can later be reinterpreted
 *  (using atol() and a cast) as a a pointer.
 */

int
cmd_get_ptr(clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	char buf[128];

	sprintf( buf, "%ld", (long)(*((void **)clientData)) );
	Tcl_AppendResult( interp, buf, (char *)NULL );
	return TCL_OK;
}

/*
 *			M A I N
 */
int
main(argc, argv)
int argc;
char **argv;
{
	register int	n;
	FILE		*fp;
	double		load = 0;

	use_air = 1;	/* air & clouds are generally desired */
	if( argc < 2 )  {
		fprintf(stderr, srv_usage);
		exit(1);
	}
	while( argv[1][0] == '-' )  {
		if( strcmp( argv[1], "-d" ) == 0 )  {
			debug++;
		} else if( strcmp( argv[1], "-x" ) == 0 )  {
			sscanf( argv[2], "%x", &rt_g.debug );
			argc--; argv++;
		} else if( strcmp( argv[1], "-X" ) == 0 )  {
			sscanf( argv[2], "%x", &rdebug );
			argc--; argv++;
		} else if( strcmp( argv[1], "-U" ) == 0 )  {
			sscanf( argv[2], "%d", &use_air );
			argc--; argv++;
		} else {
			fprintf(stderr, srv_usage);
			exit(3);
		}
		argc--; argv++;
	}
	if( argc != 3 && argc != 4 )  {
		fprintf(stderr, srv_usage);
		exit(2);
	}

	control_host = argv[1];
	tcp_port = argv[2];

	/* Note that the LIBPKG error logger can not be
	 * "bu_log", as that can cause bu_log to be entered recursively.
	 * Given the special version of bu_log in use here,
	 * that will result in a deadlock in RES_ACQUIRE(res_syscall)!
	 *  libpkg will default to stderr via pkg_errlog(), which is fine.
	 */
	pcsrv = pkg_open( control_host, tcp_port, "tcp", "", "",
		rtsync_pkgswitch, NULL );
	if( pcsrv == PKC_ERROR )  {
		fprintf(stderr, "rtnode: unable to contact %s, port %s\n",
			control_host, tcp_port);
		exit(1);
	}

	if( argc == 4 )  {
#if 0
		/* Slip one command to dispatcher */
		(void)pkg_send( MSG_CMD, argv[3], strlen(argv[3])+1, pcsrv );

		/* Prevent chasing the package with an immediate TCP close */
		sleep(1);

#endif
		pkg_close( pcsrv );
		exit(0);
	}

#if BSD == 43
	{
		int	val = 32767;
		n = setsockopt( pcsrv->pkc_fd, SOL_SOCKET,
			SO_SNDBUF, (char *)&val, sizeof(val) );
		if( n < 0 )  perror("setsockopt: SO_SNDBUF");
	}
#endif

	if( !debug )  {
		/* A fresh process */
		if (fork())
			exit(0);

		/* Go into our own process group */
		n = getpid();

		/* SysV uses setpgrp with no args and it can't fail */
#if (defined(__STDC__) || defined(SYSV)) && !defined(_BSD_COMPAT)
		if( setpgid( n, n ) < 0 )
			perror("setpgid");
#else
		if( setpgrp( n, n ) < 0 )
			perror("setpgrp");
#endif

		/* Deal with CPU limits on "those kinds" of systems */
		if( bu_cpulimit_get() > 0 )  {
			bu_cpulimit_set( 9999999 );
		}

		/* Close off the world */
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);

		(void)close(0);
		(void)close(1);
		(void)close(2);

		/* For stdio & perror safety, reopen 0,1,2 */
		(void)open("/dev/null", 0);	/* to fd 0 */
		(void)dup(0);			/* to fd 1 */
		(void)dup(0);			/* to fd 2 */

#ifndef SYSV
		n = open("/dev/tty", 2);
		if (n >= 0) {
			(void)ioctl(n, TIOCNOTTY, 0);
			(void)close(n);
		}
#endif
	}

	/*
	 *  Now that the fork() has been done, it is safe to initialize
	 *  the parallel processing support.
	 */

	beginptr = (char *) sbrk(0);

#define PUBLIC_CPUS	"/usr/tmp/public_cpus"
	max_cpus = avail_cpus = bu_avail_cpus();
	if( (fp = fopen(PUBLIC_CPUS, "r")) != NULL )  {
		(void)fscanf( fp, "%d", &max_cpus );
		fclose(fp);
		if( max_cpus < 0 )  max_cpus = avail_cpus + max_cpus;
		if( max_cpus > avail_cpus )  max_cpus = avail_cpus;
	} else {
		(void)unlink(PUBLIC_CPUS);
		if( (fp = fopen(PUBLIC_CPUS, "w")) != NULL )  {
			fprintf(fp, "%d\n", avail_cpus);
			fclose(fp);
			(void)chmod(PUBLIC_CPUS, 0666);
		}
	}

	/* Be nice on loaded machines */
	if( (debug&1) == 0 )  {
		FILE	*fp;
		fp = popen("PATH=/bin:/usr/ucb:/usr/bsd; export PATH; uptime|sed -e 's/.*average: //' -e 's/,.*//' ", "r");
		if( fp )  {
			int	iload;

			fscanf( fp, "%lf", &load );
			fclose(fp);

			iload = (int)(load + 0.5);	/* round up */
			max_cpus -= iload;
			if( max_cpus <= 0 )  {
				bu_log("This machine is overloaded, load=%g, aborting.\n", load);
				exit(9);
			}

			while( wait(NULL) != -1 )  ;	/* NIL */
		}
	}

	/* Need to set rtg_parallel non_zero here for RES_INIT to work */
	npsw = max_cpus;
	if( npsw > 1 )  {
		rt_g.rtg_parallel = 1;
	} else
		rt_g.rtg_parallel = 0;
	RES_INIT( &rt_g.res_syscall );
	RES_INIT( &rt_g.res_worker );
	RES_INIT( &rt_g.res_stats );
	RES_INIT( &rt_g.res_results );
	RES_INIT( &rt_g.res_model );
	/* DO NOT USE bu_log() before this point! */

	bu_log("load average = %f, using %d of %d cpus\n",
		load,
		npsw, avail_cpus );
	if( max_cpus <= 0 )  {
		pkg_close(pcsrv);
		exit(0);
	}

	/* Initialize the Tcl interpreter */
	interp = Tcl_CreateInterp();
	/* This runs the init.tcl script */
bu_log("before Tcl_Init\n");
	if( Tcl_Init(interp) == TCL_ERROR )  {
		bu_log("Tcl_Init error %s\n", interp->result);
		bu_bomb("rtnode: Unable to initialize TCL.  Run 'new_tk'?\n");
	}
bu_log("after Tcl_Init\n");
	bn_tcl_setup(interp);
	rt_tcl_setup(interp);
	sh_tcl_setup(interp);
	/* Don't allow unknown commands to be fed to the shell */
	Tcl_SetVar( interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY );

	/* Send our version string */
#define PROTOCOL_VERSION	"Version1.0"
	{
		char buf[512];
		sprintf(buf, "%d %s", npsw, PROTOCOL_VERSION );
		if( pkg_send( RTSYNCMSG_ALIVE, buf, strlen(buf)+1, pcsrv ) < 0 )  {
			fprintf(stderr,"pkg_send RTSYNCMSG_ALIVE error\n");
			exit(1);
		}
	}
	if( debug&2 )  fprintf(stderr, "PROTOCOL_VERSION='%s'\n", PROTOCOL_VERSION );


	for(;;)  {
		register struct pkg_queue	*lp;
		fd_set ifds;
		struct timeval tv;

		/* First, process any packages in library buffers */
		if( pkg_process( pcsrv ) < 0 )  {
			bu_log("pkg_get error\n");
			break;
		}

		/* Second, see if any input to read */
		FD_ZERO(&ifds);
		FD_SET(pcsrv->pkc_fd, &ifds);
		tv.tv_sec = 5;
		tv.tv_usec = 0L;

		/* XXX fbp */

		if( select(pcsrv->pkc_fd+1, &ifds, (fd_set *)0, (fd_set *)0,
			&tv ) != 0 )  {
			n = pkg_suckin(pcsrv);
			if( n < 0 )  {
				bu_log("pkg_suckin error\n");
				break;
			} else if( n == 0 )  {
				/* EOF detected */
				break;
			} else {
				/* All is well */
			}
		}

		/* Third, process any new packages in library buffers */
		if( pkg_process( pcsrv ) < 0 )  {
			bu_log("pkg_get error\n");
			break;
		}

		/* Finally, more work may have just arrived, check our list */
	}
}



void
ph_cd(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	if(debug)fprintf(stderr,"ph_cd %s\n", buf);
	if( chdir( buf ) < 0 )  {
		bu_log("ph_cd: chdir(%s) failure\n", buf);
		exit(1);
	}
	(void)free(buf);
}

void
ph_restart(pc, buf)
register struct pkg_conn *pc;
char *buf;
{

	if(debug)fprintf(stderr,"ph_restart %s\n", buf);
	bu_log("Restarting\n");
	pkg_close(pcsrv);
	execlp( "rtnode", "rtnode", control_host, tcp_port, (char *)0);
	perror("rtnode");
	exit(1);
}

/* -------------------- */

void
rtsync_timeout(foo)
int	foo;
{
	bu_log("rtnode: fb_open(%s) timeout -- unable to open remote framebuffer.\n",
		framebuffer_name ? framebuffer_name : "NULL" );
	bu_bomb("rtnode: Ensure BRL-CAD Release 5.0 fbserv is running. Aborting.\n");
}

/*
 *			R T S Y N C _ P H _ O P E N F B
 *
 *  Format:  width, height, framebuffer
 */
void
rtsync_ph_openfb(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	char	*hp;
	int	w, h;

	if( debug )  fprintf(stderr, "rtsync_ph_openfb: %s\n", buf );

	hp = strchr(buf, ' ');
	*hp++ = '\0';
	framebuffer_name = strchr(hp, ' ');
	*framebuffer_name++ = '\0';

	w = atoi(buf);
	h = atoi(hp);

	if( debug )  fprintf(stderr, "rtsync_ph_openfb: %d %d %s\n",
		w, h, framebuffer_name );

	(void)signal( SIGALRM, rtsync_timeout );
	alarm(7);
	if( (fbp = fb_open( framebuffer_name, w, h ) ) == 0 )  {
		bu_log("rtnode: fb_open(%s, %d, %d) failed\n", framebuffer_name, w, h );
		exit(1);
	}
	alarm(0);

	if( w <= 0 || fb_getwidth(fbp) < w )
		width = fb_getwidth(fbp);
	else
		width = w;
	if( h <= 0 || fb_getheight(fbp) < h )
		height = fb_getheight(fbp);
	else
		height = h;

	if( pkg_send( RTSYNCMSG_OPENFB, NULL, 0,
	    pcsrv ) < 0 )
		fprintf(stderr,"RTSYNCMSG_OPENFB reply error\n");

	/* Build test-pattern scanlines, sized to fit */
	nlines_line = 512;
	red_line = bu_calloc( nlines_line*(width+1), 3, "red_line" );
	grn_line = bu_calloc( nlines_line*(width+1), 3, "grn_line" );
	blu_line = bu_calloc( nlines_line*(width+1), 3, "blu_line" );
	for( w = width*nlines_line-1; w >= 0; w-- )  {
		red_line[w*3+0] = 255;
		grn_line[w*3+1] = 255;
		blu_line[w*3+2] = 255;
	}

	/* Do NOT free 'buf', it contains *framebuffer_name string */
}

/*
 *			R T S Y N C _ P H _ D I R B U I L D
 *
 *  The only argument is the name of the database file.
 */
void
rtsync_ph_dirbuild(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
#define MAXARGS 1024
	char	*argv[MAXARGS+1];
	struct rt_i *rtip;

	if( debug )  fprintf(stderr, "rtsync_ph_dirbuild: %s\n", buf );

	if( (rt_split_cmd( argv, MAXARGS, buf )) <= 0 )  {
		/* No words in input */
		(void)free(buf);
		return;
	}

	if( seen_dirbuild )  {
		bu_log("rtsync_ph_dirbuild:  MSG_DIRBUILD already seen, ignored\n");
		(void)free(buf);
		return;
	}

	title_file = bu_strdup(argv[0]);

	/* Build directory of GED database */
	if( (rtip=rt_dirbuild( title_file, NULL, 0 )) == RTI_NULL )  {
		bu_log("rtsync_ph_dirbuild:  rt_dirbuild(%s) failure\n", title_file);
		exit(2);
	}
	ap.a_rt_i = rtip;
	seen_dirbuild = 1;

	if( pkg_send( RTSYNCMSG_DIRBUILD,
	    rtip->rti_dbip->dbi_title, strlen(rtip->rti_dbip->dbi_title)+1, pcsrv ) < 0 )
		fprintf(stderr,"RTSYNCMSG_DIRBUILD reply error\n");

	(void)Tcl_CreateCommand(interp, "get_dbip", cmd_get_ptr,
		(ClientData)&ap.a_rt_i->rti_dbip, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "get_rtip", cmd_get_ptr,
		(ClientData)&ap.a_rt_i, (Tcl_CmdDeleteProc *)NULL);

	{
		struct bu_vls	cmd;
		bu_vls_init(&cmd);
		bu_vls_strcpy(&cmd, "wdb_open .inmem inmem [get_dbip]]");
		/* Tcl interpreter will write nulls into cmd string */
		if( Tcl_Eval(interp, bu_vls_addr(&cmd) ) != TCL_OK )  {
			bu_log("%s\n%s\n",
		    		interp->result,
				Tcl_GetVar(interp,"errorInfo", TCL_GLOBAL_ONLY) );
		}
		bu_vls_free(&cmd);
	}

	Tcl_LinkVar(interp, "test_fb_speed", (char *)&test_fb_speed, TCL_LINK_INT);
	Tcl_LinkVar(interp, "curframe", (char *)&curframe, TCL_LINK_INT);
}

/*
 *			R T S Y N C _ P H _ G E T T R E E S
 *
 *  Each word in the command buffer is the name of a treetop.
 */
void
rtsync_ph_gettrees(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
#define MAXARGS 1024
	char	*argv[MAXARGS+1];
	int	argc;
	struct rt_i *rtip = ap.a_rt_i;

	RT_CK_RTI(rtip);

	if( debug )  fprintf(stderr, "rtsync_ph_gettrees: %s\n", buf );

	/* Copy values from command line options into rtip */
	rtip->useair = use_air;
	if( rt_dist_tol > 0 )  {
		rtip->rti_tol.dist = rt_dist_tol;
		rtip->rti_tol.dist_sq = rt_dist_tol * rt_dist_tol;
	}
	if( rt_perp_tol > 0 )  {
		rtip->rti_tol.perp = rt_perp_tol;
		rtip->rti_tol.para = 1 - rt_perp_tol;
	}

	if( (argc = rt_split_cmd( argv, MAXARGS, buf )) <= 0 )  {
		/* No words in input */
		(void)free(buf);
		return;
	}
	title_obj = bu_strdup(argv[0]);

	if( rtip->needprep == 0 )  {
		/* First clean up after the end of the previous frame */
		if(debug)bu_log("Cleaning previous model\n");
		/* Allow lighting model to clean up (e.g. lights, materials, etc) */
		view_cleanup( rtip );
		rt_clean(rtip);
		if(rdebug&RDEBUG_RTMEM_END)
			bu_prmem( "After rt_clean" );
	}

	/* Load the desired portion of the model */
	if( rt_gettrees(rtip, argc, (CONST char **)argv, npsw) < 0 )
		fprintf(stderr,"rt_gettrees(%s) FAILED\n", argv[0]);

	/* In case it changed from startup time via an OPT command */
	if( npsw > 1 )  {
		rt_g.rtg_parallel = 1;
	} else
		rt_g.rtg_parallel = 0;

	beginptr = (char *) sbrk(0);

	seen_gettrees = 1;
	(void)free(buf);

	/*
	 * initialize application -- it will allocate 1 line and
	 * set buf_mode=1, as well as do mlib_init().
	 */
	(void)view_init( &ap, title_file, title_obj, 0 );

	do_prep( rtip );

	if( rtip->nsolids <= 0 )  {
		bu_log("ph_matrix: No solids remain after prep.\n");
		exit(3);
	}

	/* Acknowledge that we are ready */
	if( pkg_send( RTSYNCMSG_GETTREES,
	    title_obj, strlen(title_obj)+1, pcsrv ) < 0 )
		fprintf(stderr,"RTSYNCMSG_GETTREES reply error\n");
}

/*
 *			R T S Y N C _ P H _ P O V
 *
 *
 *  Format:  min_res, start_line, end_line, pov...
 */
void
rtsync_ph_pov(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	register struct rt_i *rtip = ap.a_rt_i;
	char	*argv_buf[MAXARGS+1];
	char	**argv = argv_buf;
	int	argc;
	int	min_res;
	int	start_line;
	int	end_line;
	quat_t	orient;
	mat_t	toViewcenter;
	fastf_t	viewscale;
	point_t	viewcenter_model;
	point_t	eye_screen;
	int	saved_print_on = print_on;

	RT_CK_RTI(rtip);

	if( debug )  fprintf(stderr, "rtsync_ph_pov: %s\n", buf );

	if( (argc = rt_split_cmd( argv_buf, MAXARGS, buf )) <= 0 )  {
		/* No words in input */
		bu_log("Null POV command\n");
		(void)free(buf);
		return;
	}

	/* Start options in a known state */
	AmbientIntensity = 0.4;
	hypersample = 0;
	jitter = 0;
	eye_backoff = 1.414;
	aspect = 1;
	stereo = 0;
#if 0
	width = height = 0;
	cell_width = cell_height = 0;
	lightmodel = 0;
	incr_mode = 0;
	rt_dist_tol = 0;
	rt_perp_tol = 0;

	process_cmd( buf );
#endif

	min_res = atoi(argv[0]);
	start_line = atoi(argv[1]);
	end_line = atoi(argv[2]);

	VSET( viewcenter_model, atof(argv[3]), atof(argv[4]), atof(argv[5]) );
	VSET( orient, atof(argv[6]), atof(argv[7]), atof(argv[8]) );
	orient[3] = atof(argv[9]);
	viewscale = atof(argv[10]);
	/* 11...13 for eye_pos_screen[] */
	if( argc <= 14 )  {
		static int first = 1;
		if(first)  {
			bu_log("rtnode: old format POV message received, no perspective\n");
			first = 0;
		}
		rt_perspective = 0;
	} else {
		rt_perspective = atof(argv[14]);
	}
	if( rt_perspective < 0 || rt_perspective > 179 )  rt_perspective = 0;

	curframe++;

	viewsize = 2 * viewscale;
	bn_mat_idn( Viewrotscale );
	quat_quat2mat( Viewrotscale, orient );

	bn_mat_idn( toViewcenter );
	MAT_DELTAS_VEC_NEG( toViewcenter, viewcenter_model );
	bn_mat_mul( model2view, Viewrotscale, toViewcenter );
	Viewrotscale[15] = viewscale;
	model2view[15] = viewscale;
	bn_mat_inv( view2model, model2view );

	VSET( eye_screen, 0, 0, 1 );
	MAT4X3PNT( eye_model, view2model, eye_screen );

	seen_matrix = 1;

	grid_setup();

	/* initialize lighting */
	print_on = 0;
	view_2init( &ap );
	print_on = saved_print_on;

	rtip->nshots = 0;
	rtip->nmiss_model = 0;
	rtip->nmiss_tree = 0;
	rtip->nmiss_solid = 0;
	rtip->nmiss = 0;
	rtip->nhits = 0;
	rtip->rti_nrays = 0;

	if( test_fb_speed )  {
		unsigned char	*buf;
		int	y;

		/* Write out colored lines. */
		switch( curframe%3 )  {
		case 0:
			buf = red_line;
			break;
		case 1:
			buf = grn_line;
			break;
		case 2:
			buf = blu_line;
			break;
		}
		for( y=start_line; y <= end_line; )  {
			int	todo;
			todo = end_line - y + 1;
			if( todo > nlines_line )  todo = nlines_line;
			fb_writerect( fbp, 0, y, width, todo, buf );
			y += todo;
		}
	} else {
		rt_prep_timer();
		do_run( start_line*width, end_line*width+width-1 );
		(void)rt_read_timer( (char *)0, 0 );
		view_end(&ap);
	}

	/*
	 *  Ensure all scanlines have made it to display server,
	 *  by requesting a cheap LIBFB service which requires a reply.
	 */
	{
		int	xcent, ycent, xzoom, yzoom;
		(void)fb_getview( fbp, &xcent, &ycent, &xzoom, &yzoom );
	}

	if(debug) bu_log("done!\n");

	if( pkg_send( RTSYNCMSG_DONE, "1", 2, pcsrv ) < 0 )  {
		fprintf(stderr,"pkg_send RTSYNCMSG_DONE failed\n");
		exit(12);
	}

	/* Signal done */

	free(buf);
}

/*
 *			P H _ H A L T
 */
void
rtsync_ph_halt(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	free(buf);
}

/*
 *			P H _ C M D
 *
 *  Run the buffer as a Tcl script.  There is no return,
 *  except for a log message on error.
 */
void
rtsync_ph_cmd(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	if( Tcl_Eval(interp, buf) != TCL_OK )  {
		bu_log("%s\n",
			Tcl_GetVar(interp,"errorInfo", TCL_GLOBAL_ONLY) );
	} else {
		if(interp->result[1] != '\0' )
			bu_log("%s\n", interp->result);	/* may be noisy */
	}
	free(buf);
}


/*
 *			R T L O G
 *
 *  Log an error.
 *  This version buffers a full line, to save network traffic.
 */
#if (__STDC__ && !apollo)
void
bu_log( char *fmt, ... )
{
	va_list ap;
	static char buf[512];		/* a generous output line */
	static char *cp = buf+1;

	if( print_on == 0 )  return;
	RES_ACQUIRE( &rt_g.res_syscall );
	va_start( ap, fmt );
	(void)vsprintf( cp, fmt, ap );
	va_end(ap);

	while( *cp++ )  ;		/* leaves one beyond null */
	if( cp[-2] != '\n' )
		goto out;
	if( pcsrv == PKC_NULL || pcsrv == PKC_ERROR )  {
		fprintf(stderr, "%s", buf+1);
		goto out;
	}
	if(debug) fprintf(stderr, "%s", buf+1);
	if( pkg_send( RTSYNCMSG_PRINT, buf+1, strlen(buf+1)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"pkg_send RTSYNCMSG_PRINT failed\n");
		exit(12);
	}
	cp = buf+1;
out:
	RES_RELEASE( &rt_g.res_syscall );
}
#else /* __STDC__ */

#if defined(sgi) && !defined(mips)
# define _sgi3d	1
#endif

#if (defined(BSD) && !defined(_sgi3d)) || defined(mips) || defined(CRAY2)
/*
 *  			B U _ L O G
 *  
 *  Replacement for the LIBBU routine.
 *  Log an RT library event using the Berkeley _doprnt() routine.
 */
/* VARARGS */
void
bu_log(va_alist)
va_dcl
{
	va_list		ap;
	char		*fmt;
	static char	buf[512];
	static char	*cp;
	FILE		strbuf;

	if( print_on == 0 )  return;
	if( cp == (char *)0 )  cp = buf+1;

	RES_ACQUIRE( &rt_g.res_syscall );		/* lock */
	va_start(ap);
	fmt = va_arg(ap,char *);
#if defined(mips) || (defined(alliant) && defined(i860))
	(void) vsprintf( cp, fmt, ap );
#else
	strbuf._flag = _IOWRT|_IOSTRG;
#if defined(sun)
	strbuf._ptr = (unsigned char *)cp;
#else
	strbuf._ptr = cp;
#endif
	strbuf._cnt = sizeof(buf)-(cp-buf);
	(void) _doprnt( fmt, ap, &strbuf );
	putc( '\0', &strbuf );
#endif
	va_end(ap);

	if(debug) fprintf(stderr, "%s", buf+1);
	while( *cp++ )  ;		/* leaves one beyond null */
	if( cp[-2] != '\n' )
		goto out;
	if( pcsrv == PKC_NULL || pcsrv == PKC_ERROR )  {
		fprintf(stderr, "%s", buf+1);
		goto out;
	}
	if( pkg_send( RTSYNCRTSYNCMSG_PRINT, buf+1, strlen(buf+1)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"pkg_send RTSYNCRTSYNCMSG_PRINT failed\n");
		exit(12);
	}
	cp = buf+1;
out:
	RES_RELEASE( &rt_g.res_syscall );		/* unlock */
}
#else
/* VARARGS */
void
bu_log( str, a, b, c, d, e, f, g, h )
char	*str;
int	a, b, c, d, e, f, g, h;
{
	static char buf[512];		/* a generous output line */
	static char *cp = buf+1;

	if( print_on == 0 )  return;
	RES_ACQUIRE( &rt_g.res_syscall );
	(void)sprintf( cp, str, a, b, c, d, e, f, g, h );
	while( *cp++ )  ;		/* leaves one beyond null */
	if( cp[-2] != '\n' )
		goto out;
	if( pcsrv == PKC_NULL || pcsrv == PKC_ERROR )  {
		fprintf(stderr, "%s", buf+1);
		goto out;
	}
	if(debug) fprintf(stderr, "%s", buf+1);
	if( pkg_send( RTSYNCRTSYNCMSG_PRINT, buf+1, strlen(buf+1)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"pkg_send RTSYNCRTSYNCMSG_PRINT failed\n");
		exit(12);
	}
	cp = buf+1;
out:
	RES_RELEASE( &rt_g.res_syscall );
}
#endif /* not BSD */
#endif /* not __STDC__ */


/*
 *			B U _ B O M B
 *  
 *  Replacement for LIBBU routine of same name.
 */
void
bu_bomb(str)
CONST char *str;
{
	char	*bomb = "RTSRV terminated by rt_bomb()\n";

	if( pkg_send( RTSYNCMSG_PRINT, str, strlen(str)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"rt_bomb RTSYNCMSG_PRINT failed\n");
	}
	if( pkg_send( RTSYNCMSG_PRINT, bomb, strlen(bomb)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"rt_bomb RTSYNCMSG_PRINT failed\n");
	}

	if(debug)  fprintf(stderr,"\n%s\n", str);
	fflush(stderr);
	if( rt_g.debug || rt_g.NMG_debug || bu_debug || debug )
		abort();	/* should dump */
	exit(12);
}


/*
 *			P H _ D E F A U L T
 */
void
ph_default(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	register int i;

	for( i=0; pc->pkc_switch[i].pks_handler != NULL; i++ )  {
		if( pc->pkc_switch[i].pks_type == pc->pkc_type )  break;
	}
	bu_log("ctl: unable to handle %s message: len %d",
		pc->pkc_switch[i].pks_title, pc->pkc_len);
	*buf = '*';
	(void)free(buf);
}
