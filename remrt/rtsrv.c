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

#include "conf.h"

#if IRIX == 4
#define _BSD_COMPAT	1
#endif

#include <stdio.h>
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

#undef	VMIN
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "rtstring.h"
#include "externs.h"
#include "raytrace.h"
#include "pkg.h"
#include "fb.h"

#include "../librt/debug.h"
#include "../rt/material.h"
#include "../rt/ext.h"
#include "../rt/rdebug.h"

#include "./protocol.h"

struct bu_list	WorkHead;

struct pkg_queue {
	struct bu_list	l;
	unsigned short	type;
	char		*buf;
};

/***** Variables shared with viewing model *** */
FBIO		*fbp = FBIO_NULL;	/* Framebuffer handle */
FILE		*outfp = NULL;		/* optional pixel output file */
mat_t		view2model;
mat_t		model2view;
int		srv_startpix;		/* offset for view_pixel */
int		srv_scanlen = REMRT_MAX_PIXELS;	/* max assignment */
char		*scanbuf;
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

static char idbuf[132];			/* First ID record info */

/* State flags */
static int	seen_dirbuild;
static int	seen_gettrees;
static int	seen_matrix;

static char *title_file, *title_obj;	/* name of file and first object */

#define MAX_WIDTH	(16*1024)

static int	avail_cpus;		/* # of cpus avail on this system */
static int	max_cpus;		/* max # cpus for use, <= avail_cpus */

/*
 * Package Handlers.
 */
void	ph_unexp();	/* foobar message handler */
void	ph_enqueue();	/* Addes message to linked list */
void	ph_dirbuild();
void	ph_gettrees();
void	ph_matrix();
void	ph_options();
void	ph_lines();
void	ph_end();
void	ph_restart();
void	ph_loglvl();
void	ph_cd();

void	prepare();

struct pkg_switch pkgswitch[] = {
	{ MSG_DIRBUILD,	ph_dirbuild,	"DirBuild" },
	{ MSG_GETTREES,	ph_enqueue,	"Get Trees" },
	{ MSG_MATRIX,	ph_enqueue,	"Set Matrix" },
	{ MSG_OPTIONS,	ph_enqueue,	"Options" },
	{ MSG_LINES,	ph_enqueue,	"Compute lines" },
	{ MSG_END,	ph_end,		"End" },
	{ MSG_PRINT,	ph_unexp,	"Log Message" },
	{ MSG_LOGLVL,	ph_loglvl,	"Change log level" },
	{ MSG_RESTART,	ph_restart,	"Restart" },
	{ MSG_CD,	ph_cd,		"Change Dir" },
	{ 0,		0,		(char *)0 }
};

struct pkg_conn *pcsrv;		/* PKG connection to server */
char		*control_host;	/* name of host running controller */
char		*tcp_port;	/* TCP port on control_host */

int debug = 0;		/* 0=off, 1=debug, 2=verbose */

char srv_usage[] = "Usage: rtsrv [-d] control-host tcp-port [cmd]\n";

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
		pkgswitch, NULL );
	if( pcsrv == PKC_ERROR )  {
		fprintf(stderr, "rtsrv: unable to contact %s, port %s\n",
			control_host, tcp_port);
		exit(1);
	}

	if( argc == 4 )  {
		/* Slip one command to dispatcher */
		(void)pkg_send( MSG_CMD, argv[3], strlen(argv[3])+1, pcsrv );

		/* Prevent chasing the package with an immediate TCP close */
		sleep(1);

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

		/*
		 *  Unless controller process has specificially said
		 *  that this is an interactive session, eg, for a demo,
		 *  drop to the lowest sensible priority.
		 */
		if( !interactive )  {
#ifdef CRAY
			bu_nice_set(6);		/* highest "free" priority */
#else
			bu_nice_set(19);		/* lowest priority */
#endif
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

	/* Send our version string */
	if( pkg_send( MSG_VERSION,
	    PROTOCOL_VERSION, strlen(PROTOCOL_VERSION)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"pkg_send MSG_VERSION error\n");
		exit(1);
	}
	if( debug )  fprintf(stderr, "PROTOCOL_VERSION='%s'\n", PROTOCOL_VERSION );

	/*
	 *  Now that the fork() has been done, it is safe to initialize
	 *  the parallel processing support.
	 */

	beginptr = (char *) sbrk(0);

	avail_cpus = bu_avail_cpus();
	max_cpus = bu_get_public_cpus();

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

	bu_log("using %d of %d cpus\n",
		npsw, avail_cpus );
	if( max_cpus <= 0 )  {
		pkg_close(pcsrv);
		exit(0);
	}

	BU_LIST_INIT( &WorkHead );

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
		tv.tv_sec = BU_LIST_NON_EMPTY( &WorkHead ) ? 0L : 9999L;
		tv.tv_usec = 0L;

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
		if( BU_LIST_NON_EMPTY( &WorkHead ) )  {
			lp = BU_LIST_FIRST( pkg_queue, &WorkHead );
			BU_LIST_DEQUEUE( &lp->l );
			switch( lp->type )  {
			case MSG_MATRIX:
				ph_matrix( (struct pkg_conn *)0, lp->buf );
				break;
			case MSG_LINES:
				ph_lines( (struct pkg_conn *)0, lp->buf );
				break;
			case MSG_OPTIONS:
				ph_options( (struct pkg_conn *)0, lp->buf );
				break;
			case MSG_GETTREES:
				ph_gettrees( (struct pkg_conn *)0, lp->buf );
				break;
			default:
				bu_log("bad list element, type=%d\n", lp->type );
				exit(33);
			}
			rt_free( (char *)lp, "struct pkg_queue" );
		}
	}

	return(0);		/* exit(0) */
}

/*
 *			P H _ E N Q U E U E
 *
 *  Generic routine to add a newly arrived PKG to a linked list,
 *  for later processing.
 *  Note that the buffer will be freed when the list element is processed.
 *  Presently used for MATRIX and LINES messages.
 */
void
ph_enqueue(pc, buf)
register struct pkg_conn *pc;
char	*buf;
{
	register struct pkg_queue	*lp;

	if( debug )  fprintf(stderr, "ph_enqueue: %s\n", buf );

	BU_GETSTRUCT( lp, pkg_queue );
	lp->type = pc->pkc_type;
	lp->buf = buf;
	BU_LIST_INSERT( &WorkHead, &lp->l );
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
	execlp( "rtsrv", "rtsrv", control_host, tcp_port, (char *)0);
	perror("rtsrv");
	exit(1);
}

/*
 *			P H _ D I R B U I L D
 *
 *  The only argument is the name of the database file.
 */
void
ph_dirbuild(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
#define MAXARGS 1024
	char	*argv[MAXARGS+1];
	struct rt_i *rtip;

	if( debug )  fprintf(stderr, "ph_dirbuild: %s\n", buf );

	if( (rt_split_cmd( argv, MAXARGS, buf )) <= 0 )  {
		/* No words in input */
		(void)free(buf);
		return;
	}

	if( seen_dirbuild )  {
		bu_log("ph_dirbuild:  MSG_DIRBUILD already seen, ignored\n");
		(void)free(buf);
		return;
	}

	title_file = bu_strdup(argv[0]);

	/* Build directory of GED database */
	if( (rtip=rt_dirbuild( title_file, idbuf, sizeof(idbuf) )) == RTI_NULL )  {
		bu_log("ph_dirbuild:  rt_dirbuild(%s) failure\n", title_file);
		exit(2);
	}
	ap.a_rt_i = rtip;
	seen_dirbuild = 1;

	if( pkg_send( MSG_DIRBUILD_REPLY,
	    idbuf, strlen(idbuf)+1, pcsrv ) < 0 )
		fprintf(stderr,"MSG_DIRBUILD_REPLY error\n");
}

/*
 *			P H _ G E T T R E E S
 *
 *  Each word in the command buffer is the name of a treetop.
 */
void
ph_gettrees(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
#define MAXARGS 1024
	char	*argv[MAXARGS+1];
	int	argc;
	struct rt_i *rtip = ap.a_rt_i;

	RT_CK_RTI(rtip);

	if( debug )  fprintf(stderr, "ph_gettrees: %s\n", buf );

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
		view_end( &ap );
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

	prepare();

	/* Acknowledge that we are ready */
	if( pkg_send( MSG_GETTREES_REPLY,
	    title_obj, strlen(title_obj)+1, pcsrv ) < 0 )
		fprintf(stderr,"MSG_START error\n");
}

/*
 *			P R O C E S S _ C M D
 */
void
process_cmd( buf )
char	*buf;
{
	register char	*cp;
	register char	*sp;
	register char	*ep;
	int		len;
	extern struct command_tab rt_cmdtab[];	/* from do.c */

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
		if( debug )  bu_log("process_cmd '%s'\n", sp);
		if( rt_do_cmd( ap.a_rt_i, sp, rt_cmdtab ) < 0 )  {
			bu_log("process_cmd: error on '%s'\n", sp );
			exit(1);
		}
		sp = cp;
	}
}

void
ph_options( pc, buf )
register struct pkg_conn *pc;
char	*buf;
{

	if( debug )  fprintf(stderr, "ph_options: %s\n", buf );

	process_cmd( buf );

	/* Just in case command processed was "opt -P" */
	if( npsw < 0 )  {
		/* Negative number means "all but" npsw */
		npsw = max_cpus + npsw;
	}
	if( npsw > MAX_PSW )  npsw = MAX_PSW;

	if( width <= 0 || height <= 0 )  {
		bu_log("ph_options:  width=%d, height=%d\n", width, height);
		exit(3);
	}
	(void)free(buf);
}

void
ph_matrix(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	register struct rt_i *rtip = ap.a_rt_i;

	RT_CK_RTI(rtip);

	if( debug )  fprintf(stderr, "ph_matrix: %s\n", buf );

	/* Start options in a known state */
	AmbientIntensity = 0.4;
	hypersample = 0;
	jitter = 0;
	rt_perspective = 0;
	eye_backoff = 1.414;
	aspect = 1;
	stereo = 0;
	use_air = 0;
	width = height = 0;
	cell_width = cell_height = 0;
	lightmodel = 0;
	incr_mode = 0;
	rt_dist_tol = 0;
	rt_perp_tol = 0;

	process_cmd( buf );
	free(buf);

	seen_matrix = 1;
}

void
prepare()
{
	register struct rt_i *rtip = ap.a_rt_i;

	RT_CK_RTI(rtip);

	if( debug )  fprintf(stderr, "prepare()\n");

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

}

/* 
 *			P H _ L I N E S
 *
 *
 *  Process pixels from 'a' to 'b' inclusive.
 *  The results are sent back all at once.
 *  Limitation:  may not do more than 'width' pixels at once,
 *  because that is the size of the buffer (for now).
 */
void
ph_lines(pc, buf)
struct pkg_conn *pc;
char *buf;
{
	auto int		a,b, fr;
	struct line_info	info;
	register struct rt_i	*rtip = ap.a_rt_i;
	struct	bu_external	ext;

	RT_CK_RTI(rtip);

	if( debug > 1 )  fprintf(stderr, "ph_lines: %s\n", buf );
	if( !seen_gettrees )  {
		bu_log("ph_lines:  no MSG_GETTREES yet\n");
		return;
	}
	if( !seen_matrix )  {
		bu_log("ph_lines:  no MSG_MATRIX yet\n");
		return;
	}

	a=0;
	b=0;
	fr=0;
	if( sscanf( buf, "%d %d %d", &a, &b, &fr ) != 3 )  {
		bu_log("ph_lines:  %s conversion error\n", buf );
		exit(2);
	}

	srv_startpix = a;		/* buffer un-offset for view_pixel */
	if( b-a+1 > srv_scanlen )  b = a + srv_scanlen - 1;

	rtip->rti_nrays = 0;
	info.li_startpix = a;
	info.li_endpix = b;
	info.li_frame = fr;

	rt_prep_timer();
	do_run( a, b );
	info.li_nrays = rtip->rti_nrays;
	info.li_cpusec = rt_read_timer( (char *)0, 0 );
	info.li_percent = 42.0;	/* for now */

	if (!bu_struct_export( &ext, (genptr_t)&info, desc_line_info ) ) {
		bu_log("ph_lines: bu_struct_export failure\n");
		exit(98);
	}

	if(debug)  {
		fprintf(stderr,"PIXELS fr=%d pix=%d..%d, rays=%d, cpu=%g\n",
			info.li_frame,
			info.li_startpix, info.li_endpix,
			info.li_nrays, info.li_cpusec);
	}
	if( pkg_2send( MSG_PIXELS, ext.ext_buf, ext.ext_nbytes, scanbuf, (b-a+1)*3, pcsrv ) < 0 )  {
		fprintf(stderr,"MSG_PIXELS send error\n");
		db_free_external(&ext);
	}

	db_free_external(&ext);
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
 *			B U _ L O G
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
	if( pkg_send( MSG_PRINT, buf+1, strlen(buf+1)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"pkg_send MSG_PRINT failed\n");
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
 *  			R T _ L O G
 *  
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
	if( pkg_send( MSG_PRINT, buf+1, strlen(buf+1)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"pkg_send MSG_PRINT failed\n");
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
	if( pkg_send( MSG_PRINT, buf+1, strlen(buf+1)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"pkg_send MSG_PRINT failed\n");
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
 *  Replacement for the LIBBU routine of the same name.
 */
void
bu_bomb(str)
CONST char *str;
{
	char	*bomb = "RTSRV terminated by rt_bomb()\n";

	if( pkg_send( MSG_PRINT, str, strlen(str)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"rt_bomb MSG_PRINT failed\n");
	}
	if( pkg_send( MSG_PRINT, bomb, strlen(bomb)+1, pcsrv ) < 0 )  {
		fprintf(stderr,"rt_bomb MSG_PRINT failed\n");
	}

	if(debug)  fprintf(stderr,"\n%s\n", str);
	fflush(stderr);
	if( rt_g.debug || rt_g.NMG_debug || bu_debug || debug )
		abort();	/* should dump */
	exit(12);
}

void
ph_unexp(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	register int i;

	if(debug) fprintf(stderr, "ph_unexp %s\n", buf);

	for( i=0; pc->pkc_switch[i].pks_handler != NULL; i++ )  {
		if( pc->pkc_switch[i].pks_type == pc->pkc_type )  break;
	}
	bu_log("ph_unexp: unable to handle %s message: len %d",
		pc->pkc_switch[i].pks_title, pc->pkc_len);
	*buf = '*';
	(void)free(buf);
}

/*
 *			P H _ E N D
 */
void
ph_end(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	if( debug )  fprintf(stderr, "ph_end\n");
	pkg_close(pcsrv);
	exit(0);
}

/*
 *			P H _ P R I N T
 */
void
ph_print(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	fprintf(stderr,"msg: %s\n", buf);
	(void)free(buf);
}
