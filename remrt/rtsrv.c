/*
 *			R T S R V . C
 *
 *  Remote Ray Tracing service program, using RT library.
 *
 *  Author -
 *	Michael John Muuss
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
static char RCSid[] = "@(#)$Header$ (BRL)";

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>

#undef	VMIN
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "pkg.h"
#include "fb.h"
#include "../librt/debug.h"
#include "../rt/material.h"
#include "../rt/mathtab.h"
#include "../rt/rdebug.h"

#include "./list.h"
#include "./rtsrv.h"
#include "./inout.h"
#include "./protocol.h"

struct list	*FreeList;
struct list	WorkHead;

extern char	*sbrk();
extern double atof();

/***** Variables shared with viewing model *** */
FBIO		*fbp = FBIO_NULL;	/* Framebuffer handle */
FILE		*outfp = NULL;		/* optional pixel output file */
extern int	hex_out;		/* Binary or Hex .pix output file */
extern double	AmbientIntensity;	/* Ambient light intensity */
extern double	azimuth, elevation;
extern int	lightmodel;		/* Select lighting model */
mat_t		view2model;
mat_t		model2view;
extern int	use_air;		/* Handling of air in librt */
/***** end of sharing with viewing model *****/

extern void grid_setup();
extern void worker();

/***** variables shared with worker() ******/
struct application ap;
extern int	stereo;			/* stereo viewing */
vect_t		left_eye_delta;
extern int	hypersample;		/* number of extra rays to fire */
extern int	jitter;			/* jitter ray starting positions */
extern double	rt_perspective;		/* perspective view -vs- parallel */
extern fastf_t	aspect;			/* aspect ratio Y/X */
extern vect_t	dx_model;		/* view delta-X as model-space vect */
extern vect_t	dy_model;		/* view delta-Y as model-space vect */
extern point_t	eye_model;		/* model-space location of eye */
extern fastf_t	eye_backoff;		/* dist from eye to center */
extern int	width;			/* # of pixels in X */
extern int	height;			/* # of lines in Y */
extern mat_t	Viewrotscale;
extern fastf_t	viewsize;
extern char	*scanbuf;		/* For optional output buffering */
extern int	incr_mode;		/* !0 for incremental resolution */
extern int	incr_level;		/* current incremental level */
extern int	incr_nlevel;		/* number of levels */
extern int	npsw;			/* number of worker PSWs to run */
extern struct resource	resource[MAX_PSW];	/* memory resources */
/***** end variables shared with worker() *****/

/***** variables shared with do.c *****/
extern int	pix_start;		/* pixel to start at */
extern int	pix_end;		/* pixel to end at */
extern int	nobjs;			/* Number of cmd-line treetops */
extern char	**objtab;		/* array of treetop strings */
char		*beginptr;		/* sbrk() at start of program */
extern int	matflag;		/* read matrix from stdin */
extern int	desiredframe;		/* frame to start at */
extern int	curframe;		/* current frame number */
extern char	*outputfile;		/* name of base of output file */
extern int	interactive;		/* human is watching results */
/***** end variables shared with do.c *****/

/* Variables shared within mainline pieces */
int		rdebug;			/* RT program debugging (not library) */

static char outbuf[132];
static char idbuf[132];			/* First ID record info */

static int seen_start, seen_matrix;	/* state flags */

static char *title_file, *title_obj;	/* name of file and first object */

#define MAX_WIDTH	(16*1024)

/*
 * Package Handlers.
 */
extern int pkg_nochecking;
void	ph_unexp();	/* foobar message handler */
void	ph_start();
void	ph_matrix();
void	ph_options();
void	ph_lines();
void	ph_end();
void	ph_restart();
void	ph_loglvl();
struct pkg_switch pkgswitch[] = {
	{ MSG_START, ph_start, "Startup" },
	{ MSG_MATRIX, ph_matrix, "Set Matrix" },
	{ MSG_OPTIONS, ph_options, "Options" },
	{ MSG_LINES, ph_lines, "Compute lines" },
	{ MSG_END, ph_end, "End" },
	{ MSG_PRINT, ph_unexp, "Log Message" },
	{ MSG_LOGLVL, ph_loglvl, "Change log level" },
	{ MSG_RESTART, ph_restart, "Restart" },
	{ 0, 0, (char *)0 }
};

#define MAXARGS 48
char *cmd_args[MAXARGS];
int numargs;

struct pkg_conn *pcsrv;		/* PKG connection to server */
char		*control_host;	/* name of host running controller */
char		*tcp_port;	/* TCP port on control_host */

int debug = 0;		/* 0=off, 1=debug, 2=verbose */

char srv_usage[] = "Usage: rtsrv [-d] control-host tcp-port\n";

/*
 *			M A I N
 */
main(argc, argv)
int argc;
char **argv;
{
	register int	n;
	struct timeval	nowait;
	auto int	ibits;

	if( argc < 2 )  {
		fprintf(stderr, srv_usage);
		exit(1);
	}
	if( strcmp( argv[1], "-d" ) == 0 )  {
		argc--; argv++;
		debug++;
	}
	if( argc != 3 )  {
		fprintf(stderr, srv_usage);
		exit(2);
	}

	beginptr = sbrk(0);


	/* Need to set rtg_parallel non_zero here for RES_INIT to work */
	if( npsw > 1 )  {
		rt_g.rtg_parallel = 1;
	} else
		rt_g.rtg_parallel = 0;
	RES_INIT( &rt_g.res_syscall );
	RES_INIT( &rt_g.res_worker );
	RES_INIT( &rt_g.res_stats );
	RES_INIT( &rt_g.res_results );

	pkg_nochecking = 1;
	control_host = argv[1];
	tcp_port = argv[2];
	pcsrv = pkg_open( control_host, tcp_port, "tcp", "", "",
		pkgswitch, rt_log );
	if( pcsrv == PKC_ERROR )  {
		fprintf(stderr, "rtsrv: unable to contact %s, port %s\n",
			control_host, tcp_port);
		exit(1);
	}
	if( !debug )  {
		/* Close off the world */
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);

		for( n=0; n < 20; n++ )
			if( n != pcsrv->pkc_fd )
				(void)close(n);
		(void)open("/dev/null", 0);
		(void)dup2(0, 1);
		(void)dup2(0, 2);
#ifndef SYSV
		n = open("/dev/tty", 2);
		if (n > 0) {
			ioctl(n, TIOCNOTTY, 0);
			close(n);
		}
#endif SYSV

		/* A fresh process */
		if (fork())
			exit(0);

		/* Go into our own process group */
		n = getpid();
		if( setpgrp( n, n ) < 0 )
			perror("setpgrp");

#ifndef SYSV
		(void)setpriority( PRIO_PROCESS, getpid(), 20 );
#endif
	}

	rt_log("%s\n", RCSid+13 );

	WorkHead.li_forw = WorkHead.li_back = &WorkHead;
	nowait.tv_sec = 0;
	nowait.tv_usec = 0;

	for(;;)  {
		ibits = 1 << pcsrv->pkc_fd;
		n = select(32, (char *)&ibits, (char *)0, (char *)0,
			WorkHead.li_forw != &WorkHead ? 
				&nowait : (struct timeval *)0 );
		if( n < 0 )  {
			perror("select");
			rt_log("select failure\n");
			exit(9);
		}
		if( ibits )  {
			if( pkg_get(pcsrv) < 0 )
				break;
		}
		/* More work may have just arrived, recheck queue */
		if( WorkHead.li_forw != &WorkHead )  {
			do_work();
		}
	}

	exit(0);
}

void
ph_restart(pc, buf)
register struct pkg_comm *pc;
char *buf;
{
	register int i;

	if(debug)fprintf(stderr,"ph_restart %s\n", buf);
	rt_log("Restarting\n");
	pkg_close(pcsrv);
	execlp( "rtsrv", "rtsrv", control_host, tcp_port, (char *)0);
	perror("rtsrv");
	exit(1);
}

void
ph_start(pc, buf)
register struct pkg_comm *pc;
char *buf;
{
	struct rt_i *rtip;
	register int i;

	if( debug )  fprintf(stderr, "ph_start: %s\n", buf );
	if( parse_cmd( buf ) > 0 )  {
		(void)free(buf);
		return;	/* was nop */
	}
	if( numargs < 2 )  {
		rt_log("ph_start:  no objects? %s\n", buf);
		(void)free(buf);
		return;
	}
	if( seen_start )  {
		rt_log("ph_start:  start already seen, ignored\n");
		(void)free(buf);
		return;
	}

	title_file = rt_strdup(cmd_args[0]);
	title_obj = rt_strdup(cmd_args[1]);

	rt_prep_timer();		/* Start timing preparations */

	/* Build directory of GED database */
	if( (rtip=rt_dirbuild( title_file, idbuf, sizeof(idbuf) )) == RTI_NULL )  {
		rt_log("rt:  rt_dirbuild(%s) failure\n", title_file);
		exit(2);
	}
	ap.a_rt_i = rtip;
	rt_log( "db title:  %s\n", idbuf);

	/* Load the desired portion of the model */
	for( i=1; i<numargs; i++ )  {
		(void)rt_gettree(rtip, cmd_args[i]);
	}

	/* In case it changed from startup time */
	if( npsw > 1 )  {
		rt_g.rtg_parallel = 1;
		rt_log("rtsrv:  running with %d processors\n", npsw );
	} else
		rt_g.rtg_parallel = 0;

	beginptr = sbrk(0);

	seen_start = 1;
	(void)free(buf);

	/* Acknowledge that we are ready */
	if( pkg_send( MSG_START, "", 0, pcsrv ) < 0 )
		fprintf(stderr,"MSG_START error\n");
}

void
ph_options( pc, buf )
register struct pkg_comm *pc;
char	*buf;
{
	register char	*cp;
	register char	*sp;
	register char	*ep;
	int		len;
	extern struct command_tab rt_cmdtab[];	/* from do.c */

	if( debug )  fprintf(stderr, "ph_options: %s\n", buf );

	/* Parse the string */
	len = strlen(buf);
	ep = buf+len;
	sp = buf;
	cp = buf;
	while( sp < ep )  {
		/* Find next semi-colon */
		while( *cp && *cp != ';' )  cp++;
		*cp++ = '\0';
		/* Process this command */
		if( rt_do_cmd( ap.a_rt_i, sp, rt_cmdtab ) < 0 )  {
			rt_log("error on '%s'\n", sp );
			exit(1);
		}
		sp = cp;
	}

	if( width <= 0 || height <= 0 )  {
		rt_log("ph_matrix:  width=%d, height=%d\n", width, height);
		exit(3);
	}
	(void)free(buf);
}

void
ph_matrix(pc, buf)
register struct pkg_comm *pc;
char *buf;
{
	register int i;
	register struct rt_i *rtip = ap.a_rt_i;

	if( debug )  fprintf(stderr, "ph_matrix: %s\n", buf );

	/* Start options in a known state */
	hypersample = 0;
	jitter = 0;
	rt_perspective = 0;
	eye_backoff = 1.414;
	aspect = 1;
	stereo = 0;
	width = 0;
	height = 0;

	/* Simulate arrival of options message */
	ph_options( pc, buf );
	buf = (char *)0;

	/*
	 * initialize application -- it will allocate 1 line and
	 * set buf_mode=1, as well as do mlib_init().
	 */
	(void)view_init( &ap, title_file, title_obj, 0 );

	do_prep( rtip );

	if( rtip->HeadSolid == SOLTAB_NULL )  {
		rt_log("rt: No solids remain after prep.\n");
		exit(3);
	}

	grid_setup();

	/* initialize lighting */
	view_2init( &ap );

	rtip->nshots = 0;
	rtip->nmiss_model = 0;
	rtip->nmiss_tree = 0;
	rtip->nmiss_solid = 0;
	rtip->nmiss = 0;
	rtip->nhits = 0;
	rtip->rti_nrays = 0;

	seen_matrix = 1;
}

/* 
 *			P H _ L I N E S
 *
 *  Queue a request for some pixels.
 */
void
ph_lines(pc, buf)
struct pkg_comm *pc;
char *buf;
{
	register struct list	*lp;
	auto int a,b;

	if( debug > 1 )  fprintf(stderr, "ph_lines: %s\n", buf );
	if( !seen_start )  {
		rt_log("ph_lines:  no start yet\n");
		return;
	}
	if( !seen_matrix )  {
		rt_log("ph_lines:  no matrix yet\n");
		return;
	}

	a=0;
	b=0;
	if( sscanf( buf, "%d %d", &a, &b ) != 2 )  {
		rt_log("ph_lines:  %s conversion error\n", buf );
		exit(2);
	}

	GET_LIST( lp );
	lp->li_start = a;
	lp->li_stop = b;
	APPEND_LIST( lp, WorkHead.li_back );

	(void)free(buf);
}

/*
 *			D O _ W O R K
 *
 *  Process scanlines from 'a' to 'b', inclusive, sending each back
 *  as soon as it's done.
 */
do_work()
{
	register struct list	*lp;
	struct line_info	info;
	register struct rt_i	*rtip = ap.a_rt_i;
	register int	y;
	int	len;
	char	*cp;

	if( (lp = WorkHead.li_forw) == &WorkHead )
		return;

	DEQUEUE_LIST( lp );
	if(debug) fprintf(stderr,"do_work %d %d\n", lp->li_start, lp->li_stop);

	for( y = lp->li_start; y <= lp->li_stop; y++)  {

		rtip->rti_nrays = 0;
		rt_prep_timer();

		do_run( y*width + 0, y*width + width - 1 );

		info.li_y = y;
		info.li_nrays = rtip->rti_nrays;
		info.li_cpusec = rt_read_timer( (char *)0, 0 );

		len = 0;
		cp = struct_export( &len, (stroff_t)&info, desc_line_info );
		if( cp == (char *)0 )  {
			rt_log("struct_export failure\n");
			break;
		}

		if(debug)fprintf(stderr,"PIXELS y=%d, rays=%d, cpu=%g\n", info.li_y, info.li_nrays, info.li_cpusec);
		if( pkg_2send( MSG_PIXELS, cp, len,
			scanbuf, width*3, pcsrv ) < 0 )
			fprintf(stderr,"MSG_PIXELS send error\n");
		(void)free(cp);
	}

	FREE_LIST( lp );
}

int print_on = 1;

void
ph_loglvl(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	if(debug) fprintf(stderr, "ph_loglvl %s\n", buf);
	if( buf[0] == '0' )
		print_on = 0;
	else	print_on = 1;
	(void)free(buf);
}

/*
 *			R T L O G
 *
 *  Log an error.
 *  This version buffers a full line, to save network traffic.
 */
/* VARARGS */
void
rt_log( str, a, b, c, d, e, f, g, h )
char *str;
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
	if( pkg_send( MSG_PRINT, buf+1, strlen(buf+1)+1, pcsrv ) < 0 )
		exit(12);
	cp = buf+1;
out:
	RES_RELEASE( &rt_g.res_syscall );
}

void
rt_bomb(str)
char *str;
{
	rt_log("rt:  Fatal Error %s, aborting\n", str);
	abort();	/* should dump */
	exit(12);
}

void
ph_unexp(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	register int i;

	for( i=0; pc->pkc_switch[i].pks_handler != NULL; i++ )  {
		if( pc->pkc_switch[i].pks_type == pc->pkc_type )  break;
	}
	rt_log("rtsrv: unable to handle %s message: len %d",
		pc->pkc_switch[i].pks_title, pc->pkc_len);
	*buf = '*';
	(void)free(buf);
}

void
ph_end(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	if( debug )  fprintf(stderr, "ph_end\n");
	pkg_close(pcsrv);
	exit(0);
}

void
ph_print(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	fprintf(stderr,"msg: %s\n", buf);
	(void)free(buf);
}

parse_cmd( line )
char *line;
{
	register char *lp;
	register char *lp1;

	numargs = 0;
	lp = &line[0];
	cmd_args[0] = &line[0];

	if( *lp=='\0' || *lp == '\n' )
		return(1);		/* NOP */

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
					rt_log("More than %d args, excess flushed\n", MAXARGS);
					cmd_args[MAXARGS] = (char *)0;
					return(0);
				}
				cmd_args[numargs++] = lp1;
			}
		}
		/* Finally, a non-space char */
	}
	/* Null terminate pointer array */
	cmd_args[numargs] = (char *)0;
	return(0);
}
