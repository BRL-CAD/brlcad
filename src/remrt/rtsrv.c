/*                         R T S R V . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file rtsrv.c
 *
 *  Remote Ray Tracing service program, using RT library.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#  include <sys/resource.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include "bselect.h"
#include "bio.h"

#ifdef VMIN
#  undef VMIN
#endif

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "pkg.h"
#include "fb.h"
#include "icv.h"

#include "../rt/rtuif.h"
#include "../rt/ext.h"
#include "./protocol.h"



struct bu_list	WorkHead;

struct pkg_queue {
    struct bu_list	l;
    unsigned short	type;
    char		*buf;
};

/***** Variables shared with viewing model *** */
FBIO *fbp = FBIO_NULL;	/* Framebuffer handle */
FILE *outfp = NULL;		/* optional pixel output file */
mat_t view2model;
mat_t model2view;
int srv_startpix;		/* offset for view_pixel */
int srv_scanlen = REMRT_MAX_PIXELS;	/* max assignment */
unsigned char *scanbuf;
/***** end of sharing with viewing model *****/

extern void grid_setup();
extern void worker();

/***** variables shared with worker() ******/
struct application APP;
vect_t		left_eye_delta;
int		report_progress;	/* !0 = user wants progress report */
/***** end variables shared with worker() *****/

/* Variables shared within mainline pieces */
extern fastf_t	rt_dist_tol;		/* Value for rti_tol.dist */
extern fastf_t	rt_perp_tol;		/* Value for rti_tol.perp */
extern int	rdebug;			/* RT program debugging (not library) */
extern int	rt_verbosity;		/* from liboptical */
static char idbuf[132];			/* First ID record info */

/* State flags */
static int	seen_dirbuild;
static int	seen_gettrees;
static int	seen_matrix;

static char *title_file, *title_obj;	/* name of file and first object */

#define MAX_WIDTH	(16*1024)

static int	avail_cpus;		/* # of cpus avail on this system */

int	save_overlaps=0;

struct icv_image_file *bif = NULL;

/*
 * Package Handlers.
 */
void	ph_unexp(struct pkg_conn *pc, char *buf);	/* foobar message handler */
void	ph_enqueue(struct pkg_conn *pc, char *buf);	/* Addes message to linked list */
void	ph_dirbuild(struct pkg_conn *pc, char *buf);
void	ph_gettrees(struct pkg_conn *pc, char *buf);
void	ph_matrix(struct pkg_conn *pc, char *buf);
void	ph_options(struct pkg_conn *pc, char *buf);
void	ph_lines(struct pkg_conn *pc, char *buf);
void	ph_end(struct pkg_conn *pc, char *buf);
void	ph_restart(struct pkg_conn *pc, char *buf);
void	ph_loglvl(struct pkg_conn *pc, char *buf);
void	ph_cd(struct pkg_conn *pc, char *buf);

void	prepare(void);

struct pkg_switch pkgswitch[] = {
    { MSG_DIRBUILD,	ph_dirbuild,	"DirBuild", NULL },
    { MSG_GETTREES,	ph_enqueue,	"Get Trees", NULL },
    { MSG_MATRIX,	ph_enqueue,	"Set Matrix", NULL },
    { MSG_OPTIONS,	ph_enqueue,	"Options", NULL },
    { MSG_LINES,	ph_enqueue,	"Compute lines", NULL },
    { MSG_END,		ph_end,		"End", NULL },
    { MSG_PRINT,	ph_unexp,	"Log Message", NULL },
    { MSG_LOGLVL,	ph_loglvl,	"Change log level", NULL },
    { MSG_RESTART,	ph_restart,	"Restart", NULL },
    { MSG_CD,		ph_cd,		"Change Dir", NULL },
    { 0,		0,		NULL, NULL }
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
main(int argc, char **argv)
{
    int	n;

    if ( argc < 2 )  {
	fprintf(stderr, "%s", srv_usage);
	return 1;
    }
    while ( argv[1][0] == '-' )  {
	if ( BU_STR_EQUAL( argv[1], "-d" ) )  {
	    debug++;
	} else if ( BU_STR_EQUAL( argv[1], "-x" ) )  {
	    sscanf( argv[2], "%x", (unsigned int *)&rt_g.debug );
	    argc--; argv++;
	} else if ( BU_STR_EQUAL( argv[1], "-X" ) )  {
	    sscanf( argv[2], "%x", (unsigned int *)&rdebug );
	    argc--; argv++;
	} else {
	    fprintf(stderr, "%s", srv_usage);
	    return 3;
	}
	argc--; argv++;
    }
    if ( argc != 3 && argc != 4 )  {
	fprintf(stderr, "%s", srv_usage);
	return 2;
    }

    control_host = argv[1];
    tcp_port = argv[2];

    /* Note that the LIBPKG error logger can not be
     * "bu_log", as that can cause bu_log to be entered recursively.
     * Given the special version of bu_log in use here,
     * that will result in a deadlock in bu_semaphore_acquire(res_syscall)!
     *  libpkg will default to stderr via pkg_errlog(), which is fine.
     */
    pcsrv = pkg_open( control_host, tcp_port, "tcp", "", "",
		      pkgswitch, NULL );
    if ( pcsrv == PKC_ERROR )  {
	fprintf(stderr, "rtsrv: unable to contact %s, port %s\n",
		control_host, tcp_port);
	return 1;
    }

    if ( argc == 4 )  {
	/* Slip one command to dispatcher */
	(void)pkg_send( MSG_CMD, argv[3], strlen(argv[3])+1, pcsrv );

	/* Prevent chasing the package with an immediate TCP close */
	sleep(1);

	pkg_close( pcsrv );
	return 0;
    }

#ifdef SO_SNDBUF
    /* increase the default send buffer size to 32k since we're
     * sending pixels more than likely.
     */
    {
	int val = 32767;
	n = setsockopt( pcsrv->pkc_fd, SOL_SOCKET, SO_SNDBUF, (const void *)&val, sizeof(val) );
	if ( n < 0 )  perror("setsockopt: SO_SNDBUF");
    }
#endif

    if ( !debug )  {
	/* A fresh process */
	if (fork())
	    return 0;

	/* Go into our own process group */
	n = bu_process_id();

#ifdef HAVE_SETPGID
	if ( setpgid( n, n ) < 0 )
	    perror("setpgid");
#else
	/* SysV uses setpgrp with no args and it can't fail,
	 * obsoleted by setpgid.
	 */
	setpgrp();
#endif

	/* Deal with CPU limits on "those kinds" of systems */
	if ( bu_cpulimit_get() > 0 )  {
	    bu_cpulimit_set( 9999999 );
	}

	/*
	 *  Unless controller process has specificially said
	 *  that this is an interactive session, eg, for a demo,
	 *  drop to the lowest sensible priority.
	 */
	if ( !interactive )  {
	    bu_nice_set(19);		/* lowest priority */
	}

	/* Close off the world */
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	(void)close(0);
	(void)close(1);
	(void)close(2);

	/* For stdio & perror safety, reopen 0, 1, 2 */
	(void)open("/dev/null", 0);	/* to fd 0 */
	n = dup(0);			/* to fd 1 */
	if (n == -1)
	    perror("dup");
	n = dup(0);			/* to fd 2 */
	if (n == -1)
	    perror("dup");

#if defined(HAVE_SYS_IOCTL_H) && defined(TIOCNOTTY)
	n = open("/dev/tty", 2);
	if (n >= 0) {
	    (void)ioctl(n, TIOCNOTTY, 0);
	    (void)close(n);
	}
#endif
    }

    /* Send our version string */
    if ( pkg_send( MSG_VERSION,
		   PROTOCOL_VERSION, strlen(PROTOCOL_VERSION)+1, pcsrv ) < 0 )  {
	fprintf(stderr, "pkg_send MSG_VERSION error\n");
	return 1;
    }
    if ( debug )  fprintf(stderr, "PROTOCOL_VERSION='%s'\n", PROTOCOL_VERSION );

    /*
     *  Now that the fork() has been done, it is safe to initialize
     *  the parallel processing support.
     */

    avail_cpus = bu_avail_cpus();

    /* Need to set rtg_parallel non_zero here for RES_INIT to work */
    npsw = avail_cpus;
    if ( npsw > 1 )  {
	rt_g.rtg_parallel = 1;
    } else
	rt_g.rtg_parallel = 0;
    bu_semaphore_init( RT_SEM_LAST );

    bu_log("using %d of %d cpus\n",
	   npsw, avail_cpus );

    /*
     *  Initialize the non-parallel memory resource.
     *  The parallel guys are initialized after the rt_dirbuild().
     */
    rt_init_resource( &rt_uniresource, MAX_PSW, NULL );
    bn_rand_init( rt_uniresource.re_randptr, MAX_PSW );

    BU_LIST_INIT( &WorkHead );

    for (;;)  {
	struct pkg_queue	*lp;
	fd_set ifds;
	struct timeval tv;

	/* First, process any packages in library buffers */
	if ( pkg_process( pcsrv ) < 0 )  {
	    bu_log("pkg_get error\n");
	    break;
	}

	/* Second, see if any input to read */
	FD_ZERO(&ifds);
	FD_SET(pcsrv->pkc_fd, &ifds);
	tv.tv_sec = BU_LIST_NON_EMPTY( &WorkHead ) ? 0L : 9999L;
	tv.tv_usec = 0L;

	if ( select(pcsrv->pkc_fd+1, &ifds, (fd_set *)0, (fd_set *)0,
		    &tv ) != 0 )  {
	    n = pkg_suckin(pcsrv);
	    if ( n < 0 )  {
		bu_log("pkg_suckin error\n");
		break;
	    } else if ( n == 0 )  {
		/* EOF detected */
		break;
	    } else {
		/* All is well */
	    }
	}

	/* Third, process any new packages in library buffers */
	if ( pkg_process( pcsrv ) < 0 )  {
	    bu_log("pkg_get error\n");
	    break;
	}

	/* Finally, more work may have just arrived, check our list */
	if ( BU_LIST_NON_EMPTY( &WorkHead ) )  {
	    lp = BU_LIST_FIRST( pkg_queue, &WorkHead );
	    BU_LIST_DEQUEUE( &lp->l );
	    switch ( lp->type )  {
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
		    return 33;
	    }
	    bu_free( (char *)lp, "struct pkg_queue" );
	}
    }

    return 0;		/* bu_exit(0, NULL) */
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
ph_enqueue(struct pkg_conn *pc, char *buf)
{
    struct pkg_queue	*lp;

    if ( debug )  fprintf(stderr, "ph_enqueue: %s\n", buf );

    BU_GETSTRUCT( lp, pkg_queue );
    lp->type = pc->pkc_type;
    lp->buf = buf;
    BU_LIST_INSERT( &WorkHead, &lp->l );
}

void
ph_cd(struct pkg_conn *UNUSED(pc), char *buf)
{
    if (debug)fprintf(stderr, "ph_cd %s\n", buf);
    if ( chdir( buf ) < 0 )
	bu_exit(1, "ph_cd: chdir(%s) failure\n", buf);
    (void)free(buf);
}

void
ph_restart(struct pkg_conn *UNUSED(pc), char *buf)
{

    if (debug)fprintf(stderr, "ph_restart %s\n", buf);
    bu_log("Restarting\n");
    pkg_close(pcsrv);
    execlp( "rtsrv", "rtsrv", control_host, tcp_port, (char *)0);
    perror("rtsrv");
    bu_exit(1, NULL);
}

/*
 *			P H _ D I R B U I L D
 *
 *  The only argument is the name of the database file.
 */
void
ph_dirbuild(struct pkg_conn *UNUSED(pc), char *buf)
{
    long max_argc = 0;
    char **argv = NULL;
    struct rt_i *rtip = NULL;
    size_t n = 0;

    if ( debug )  fprintf(stderr, "ph_dirbuild: %s\n", buf );

    for (n = 0; n < strlen(buf); n++) {
	if (isspace(buf[n]))
	    max_argc++;
    }
    argv = bu_calloc(max_argc+1, sizeof(char *), "alloc argv");

    if ( (bu_argv_from_string( argv, max_argc, buf )) <= 0 )  {
	/* No words in input */
	(void)free(buf);
	bu_free(argv, "free argv");
	return;
    }

    if ( seen_dirbuild )  {
	bu_log("ph_dirbuild:  MSG_DIRBUILD already seen, ignored\n");
	(void)free(buf);
	bu_free(argv, "free argv");
	return;
    }

    title_file = bu_strdup(argv[0]);
    bu_free(argv, "free argv");

    /* Build directory of GED database */
    if ( (rtip=rt_dirbuild( title_file, idbuf, sizeof(idbuf) )) == RTI_NULL )
	bu_exit(2, "ph_dirbuild:  rt_dirbuild(%s) failure\n", title_file);
    APP.a_rt_i = rtip;
    seen_dirbuild = 1;

    /*
     *  Initialize all the per-CPU memory resources.
     *  Go for the max, as TCL interface may change npsw as we run.
     */
    for ( n=0; n < MAX_PSW; n++ )  {
	rt_init_resource( &resource[n], n, rtip );
	bn_rand_init( resource[n].re_randptr, n );
    }

    if ( pkg_send( MSG_DIRBUILD_REPLY,
		   idbuf, strlen(idbuf)+1, pcsrv ) < 0 )
	fprintf(stderr, "MSG_DIRBUILD_REPLY error\n");
}

/*
 *			P H _ G E T T R E E S
 *
 *  Each word in the command buffer is the name of a treetop.
 */
void
ph_gettrees(struct pkg_conn *UNUSED(pc), char *buf)
{
    size_t n = 0;
    long max_argc = 0;
    char **argv = NULL;
    int	argc = 0;
    struct rt_i *rtip = APP.a_rt_i;

    RT_CK_RTI(rtip);

    if ( debug )  fprintf(stderr, "ph_gettrees: %s\n", buf );

    /* Copy values from command line options into rtip */
    rtip->useair = use_air;
    if ( rt_dist_tol > 0 )  {
	rtip->rti_tol.dist = rt_dist_tol;
	rtip->rti_tol.dist_sq = rt_dist_tol * rt_dist_tol;
    }
    if ( rt_perp_tol > 0 )  {
	rtip->rti_tol.perp = rt_perp_tol;
	rtip->rti_tol.para = 1 - rt_perp_tol;
    }

    for (n = 0; n < strlen(buf); n++) {
	if (isspace(buf[n]))
	    max_argc++;
    }
    argv = bu_calloc(max_argc+1, sizeof(char *), "alloc argv");

    if ( (argc = bu_argv_from_string( argv, max_argc, buf )) <= 0 )  {
	/* No words in input */
	(void)free(buf);
	bu_free(argv, "free argv");
	return;
    }
    title_obj = bu_strdup(argv[0]);

    if ( rtip->needprep == 0 )  {
	/* First clean up after the end of the previous frame */
	if (debug)bu_log("Cleaning previous model\n");
	view_end( &APP );
	view_cleanup( rtip );
	rt_clean(rtip);
	if (rdebug&RDEBUG_RTMEM_END)
	    bu_prmem( "After rt_clean" );
    }

    /* Load the desired portion of the model */
    if ( rt_gettrees(rtip, argc, (const char **)argv, npsw) < 0 )
	fprintf(stderr, "rt_gettrees(%s) FAILED\n", argv[0]);
    bu_free(argv, "free argv");

    /* In case it changed from startup time via an OPT command */
    if ( npsw > 1 )  {
	rt_g.rtg_parallel = 1;
    } else
	rt_g.rtg_parallel = 0;

    seen_gettrees = 1;
    (void)free(buf);

    prepare();

    /* Acknowledge that we are ready */
    if ( pkg_send( MSG_GETTREES_REPLY,
		   title_obj, strlen(title_obj)+1, pcsrv ) < 0 )
	fprintf(stderr, "MSG_START error\n");
}

/*
 *			P R O C E S S _ C M D
 */
void
process_cmd(char *buf)
{
    char	*cp;
    char	*sp;
    char	*ep;
    int		len;
    extern struct command_tab rt_cmdtab[];	/* from do.c */

    /* Parse the string */
    len = strlen(buf);
    ep = buf+len;
    sp = buf;
    cp = buf;
    while ( sp < ep )  {
	/* Find next semi-colon */
	while ( *cp && *cp != ';' )  cp++;
	*cp++ = '\0';
	/* Process this command */
	if ( debug )  bu_log("process_cmd '%s'\n", sp);
	if ( rt_do_cmd( APP.a_rt_i, sp, rt_cmdtab ) < 0 )
	    bu_exit(1, "process_cmd: error on '%s'\n", sp );
	sp = cp;
    }
}

void
ph_options(struct pkg_conn *UNUSED(pc), char *buf)
{

    if ( debug )  fprintf(stderr, "ph_options: %s\n", buf );

    process_cmd( buf );

    /* Just in case command processed was "opt -P" */
    if ( npsw < 0 )  {
	/* Negative number means "all but #" available */
	npsw = avail_cpus - npsw;
    }

    /* basic bounds sanity */
    if (npsw > MAX_PSW)
	npsw = MAX_PSW;
    if (npsw <= 0)
	npsw = 1;

    if ( width <= 0 || height <= 0 )
	bu_exit(3, "ph_options:  width=%zu, height=%zu\n", width, height);
    (void)free(buf);
}

void
ph_matrix(struct pkg_conn *UNUSED(pc), char *buf)
{
#ifndef NO_MAGIC_CHECKING
    struct rt_i *rtip = APP.a_rt_i;

    RT_CK_RTI(rtip);
#endif

    if ( debug )  fprintf(stderr, "ph_matrix: %s\n", buf );

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
prepare(void)
{
    struct rt_i *rtip = APP.a_rt_i;

    RT_CK_RTI(rtip);

    if ( debug )  fprintf(stderr, "prepare()\n");

    /*
     * initialize application -- it will allocate 1 line and
     * set buf_mode=1, as well as do mlib_init().
     */
    (void)view_init( &APP, title_file, title_obj, 0, 0);

    do_prep( rtip );

    if ( rtip->nsolids <= 0 )
	bu_exit(3, "ph_matrix: No solids remain after prep.\n");

    grid_setup();

    /* initialize lighting */
    view_2init( &APP, NULL );

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
ph_lines(struct pkg_conn *UNUSED(pc), char *buf)
{
    auto int		a, b, fr;
    struct line_info	info;
    struct rt_i	*rtip = APP.a_rt_i;
    struct	bu_external	ext;

    RT_CK_RTI(rtip);

    if ( debug > 1 )  fprintf(stderr, "ph_lines: %s\n", buf );
    if ( !seen_gettrees )  {
	bu_log("ph_lines:  no MSG_GETTREES yet\n");
	return;
    }
    if ( !seen_matrix )  {
	bu_log("ph_lines:  no MSG_MATRIX yet\n");
	return;
    }

    a=0;
    b=0;
    fr=0;
    if ( sscanf( buf, "%d %d %d", &a, &b, &fr ) != 3 )
	bu_exit(2, "ph_lines:  %s conversion error\n", buf );

    srv_startpix = a;		/* buffer un-offset for view_pixel */
    if ( b-a+1 > srv_scanlen )  b = a + srv_scanlen - 1;

    rtip->rti_nrays = 0;
    info.li_startpix = a;
    info.li_endpix = b;
    info.li_frame = fr;

    rt_prep_timer();
    do_run( a, b );
    info.li_nrays = rtip->rti_nrays;
    info.li_cpusec = rt_read_timer( (char *)0, 0 );
    info.li_percent = 42.0;	/* for now */

    if (!bu_struct_export( &ext, (genptr_t)&info, desc_line_info ) )
	bu_exit(98, "ph_lines: bu_struct_export failure\n");

    if (debug)  {
	fprintf(stderr, "PIXELS fr=%d pix=%d..%d, rays=%d, cpu=%g\n",
		info.li_frame,
		info.li_startpix, info.li_endpix,
		info.li_nrays, info.li_cpusec);
    }
    if ( pkg_2send( MSG_PIXELS, (const char *)ext.ext_buf, ext.ext_nbytes, (const char *)scanbuf, (b-a+1)*3, pcsrv ) < 0 )  {
	fprintf(stderr, "MSG_PIXELS send error\n");
	bu_free_external(&ext);
    }

    bu_free_external(&ext);
}

int print_on = 1;

void
ph_loglvl(struct pkg_conn *UNUSED(pc), char *buf)
{
    if (debug) fprintf(stderr, "ph_loglvl %s\n", buf);
    if ( buf[0] == '0' )
	print_on = 0;
    else	print_on = 1;
    (void)free(buf);
}

/**** Other replacement routines from libbu/log.c ****/
int	bu_log_indent_cur_level = 0; /* formerly rt_g.rtg_logindent */
/*
 *			B U _ L O G _ I N D E N T _ D E L T A
 *
 *  Change indentation level by indicated number of characters.
 *  Call with a large negative number to cancel all indentation.
 */
void
bu_log_indent_delta(int delta)
{
    if ( (bu_log_indent_cur_level += delta) < 0 )
	bu_log_indent_cur_level = 0;
}

/*
 *			B U _ L O G _ I N D E N T _ V L S
 *
 *  For multi-line vls generators, honor logindent level like bu_log() does,
 *  and prefix the proper number of spaces.
 *  Should be called at the front of each new line.
 */
void
bu_log_indent_vls(struct bu_vls *v)
{
    bu_vls_spaces( v, bu_log_indent_cur_level );
}


/*
 *			B U _ L O G
 *
 *  Log an error.
 *  This version buffers a full line, to save network traffic.
 */
void
bu_log( const char *fmt, ... )
{
    va_list vap;
    char buf[512];		/* a generous output line.  Must be AUTO, else non-PARALLEL. */

    if ( print_on == 0 )  return;
    bu_semaphore_acquire( BU_SEM_SYSCALL );
    va_start( vap, fmt );
    (void)vsprintf( buf, fmt, vap );
    va_end(vap);

    if ( pcsrv == PKC_NULL || pcsrv == PKC_ERROR )  {
	fprintf(stderr, "%s", buf);
	goto out;
    }
    if (debug) fprintf(stderr, "%s", buf);
    if ( pkg_send( MSG_PRINT, buf, strlen(buf)+1, pcsrv ) < 0 )  {
	fprintf(stderr, "pkg_send MSG_PRINT failed\n");
	bu_exit(12, NULL);
    }
 out:
    bu_semaphore_release( BU_SEM_SYSCALL );
}


/*
 *			B U _ B O M B
 *
 *  Replacement for the LIBBU routine of the same name.
 */
int		bu_setjmp_valid = 0;	/* !0 = bu_jmpbuf is valid */
jmp_buf		bu_jmpbuf;		/* for BU_SETJMP() */

void
bu_bomb(const char *str)
{
    char	*bomb = "RTSRV terminated by bu_bomb()\n";

    if ( pkg_send( MSG_PRINT, (char *)str, strlen(str)+1, pcsrv ) < 0 )  {
	fprintf(stderr, "bu_bomb MSG_PRINT failed\n");
    }
    if ( pkg_send( MSG_PRINT, bomb, strlen(bomb)+1, pcsrv ) < 0 )  {
	fprintf(stderr, "bu_bomb MSG_PRINT failed\n");
    }

    if (debug)  fprintf(stderr, "\n%s\n", str);
    fflush(stderr);
    if ( RT_G_DEBUG || rt_g.NMG_debug || bu_debug || debug )
	abort();	/* should dump */
    bu_exit(12, NULL);
}

void
ph_unexp(struct pkg_conn *pc, char *buf)
{
    int i;

    if (debug) fprintf(stderr, "ph_unexp %s\n", buf);

    for ( i=0; pc->pkc_switch[i].pks_handler != NULL; i++ )  {
	if ( pc->pkc_switch[i].pks_type == pc->pkc_type )  break;
    }
    bu_log("ph_unexp: unable to handle %s message: len %zu",
	   pc->pkc_switch[i].pks_title, pc->pkc_len);
    *buf = '*';
    (void)free(buf);
}

/*
 *			P H _ E N D
 */
void
ph_end(struct pkg_conn *UNUSED(pc), char *UNUSED(buf))
{
    if ( debug )  fprintf(stderr, "ph_end\n");
    pkg_close(pcsrv);
    bu_exit(0, NULL);
}

/*
 *			P H _ P R I N T
 */
void
ph_print(struct pkg_conn *UNUSED(pc), char *buf)
{
    fprintf(stderr, "msg: %s\n", buf);
    (void)free(buf);
}

/* Stub for do.c */
void
memory_summary(void)
{
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
