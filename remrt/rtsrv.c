/*
 *			R T S R V
 *
 *  Ray Tracing service program, using RT library.
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
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/raytrace.h"
#include "../librt/debug.h"
#include "pkg.h"
#include "rtsrv.h"

extern double atof();

/***** Variables shared with viewing model *** */
double AmbientIntensity = 0.4;	/* Ambient light intensity */
double azimuth, elevation;
int lightmodel;		/* Select lighting model */
mat_t view2model;
mat_t model2view;
/***** end of sharing with viewing model *****/

/***** variables shared with worker() */
static int	perspective=0;	/* perspective view -vs- parallel view */
static vect_t	dx_model;	/* view delta-X as model-space vector */
static vect_t	dy_model;	/* view delta-Y as model-space vector */
static point_t	eye_model;	/* model-space location of eye */
static point_t	viewbase_model;	/* model-space location of viewplane corner */
static int npts;	/* # of points to shoot: x,y */
int npsw = 1;		/* HEP: number of worker PSWs to run */
void worker();
int work_word;		/* semaphored (x<<16)|y */

#define LINENO_WIDTH	2
/* We don't know exactly how much we will need */
char scanbuf[(8*1024*3) + LINENO_WIDTH + 8];/* scan line, line # as header */
/***** end variables shared with worker() */

/* Variables shared within mainline pieces */
static struct application ap;
static float	zoomout=1;	/* >0 zoom out, 0..1 zoom in */
static char outbuf[132];
static char idbuf[132];		/* First ID record info */
static mat_t Viewrotscale;
static mat_t toEye;
static fastf_t viewsize;

static int seen_start, seen_matrix;	/* state flags */

/*
 * Package Handlers.
 */
int ph_unexp();	/* foobar message handler */
int ph_start();
int ph_matrix();
int ph_options();
int ph_lines();
int ph_end();
int ph_restart();
int ph_loglvl();
struct pkg_switch pkg_switch[] = {
	{ MSG_START, ph_start, "Startup" },
	{ MSG_MATRIX, ph_matrix, "Set Matrix" },
	{ MSG_OPTIONS, ph_options, "Set options" },
	{ MSG_LINES, ph_lines, "Compute lines" },
	{ MSG_END, ph_end, "End" },
	{ MSG_PRINT, ph_unexp, "Log Message" },
	{ MSG_LOGLVL, ph_loglvl, "Change log level" },
	{ MSG_RESTART, ph_restart, "Restart" }
};
int pkg_swlen = sizeof(pkg_switch)/sizeof(struct pkg_switch);

#define MAXARGS 48
char *cmd_args[MAXARGS];
int numargs;

struct pkg_conn *pcsrv;		/* PKG connection to server */
char *control_host;	/* name of host running controller */


/*
 *			M A I N
 */
main(argc, argv)
int argc;
char **argv;
{
	register int n;

	if( argc != 2 )  {
		fprintf(stderr, "Usage: rtsrv control-host\n");
		exit(1);
	}

	control_host = argv[1];
	if( (pcsrv = pkg_open( control_host, "rtsrv" )) == 0 &&
	    (pcsrv = pkg_open( control_host, "4446" )) == 0 )  {
		fprintf(stderr, "unable to contact %s\n", control_host);
		exit(1);
	}
	for( n=0; n < 20; n++ )
		if( n != pcsrv->pkc_fd )
			(void)close(n);
	(void)open("/dev/null", 0);
	(void)dup2(0, 1);
	(void)dup2(0, 2);
	n = open("/dev/tty", 2);
	if (n > 0) {
		ioctl(n, TIOCNOTTY, 0);
		close(n);
	}

	/* A fresh process */
	if (fork())
		exit(0);

	/* Go into our own process group */
	n = getpid();
	if( setpgrp( n, n ) < 0 )
		perror("setpgrp");

	(void)setpriority( PRIO_PROCESS, getpid(), 20 );

	while( pkg_block(pcsrv) >= 0 )
		;
}

ph_restart(pc, buf)
register struct pkg_comm *pc;
char *buf;
{
	register int i;

	rt_log("Restarting\n");
	pkg_close(pcsrv);
	execlp( "rtsrv", "rtsrv", control_host, (char *)0);
	perror("rtsrv");
	exit(1);
}

ph_options(pc, buf)
register struct pkg_comm *pc;
char *buf;
{
	register char *cp;

	/* Start options in a known state */
	perspective = 0;

	cp = buf;
	while( *cp == '-' )  {
		switch( cp[1] )  {
		case 'A':
			AmbientIntensity = atof( &cp[2] );
			break;
		case 'x':
			sscanf( &cp[2], "%x", &rt_g.debug );
			rt_log("debug=x%x\n", rt_g.debug);
			break;
		case 'f':
			/* "Fast" -- just a few pixels.  Or, arg's worth */
			npts = atoi( &cp[2] );
			if( npts < 2 || npts > 1024 )  {
				npts = 50;
			}
			break;
		case 'l':
			/* Select lighting model # */
			lightmodel = atoi( &cp[2] );
			break;
		case 'p':
			perspective = 1;
			if( cp[2] != '\0' )
				zoomout = atof( &cp[2] );
			if( zoomout <= 0 )  zoomout = 1;
			break;
		case 'P':
			/* Number of parallel workers */
			npsw = atoi( &cp[2] );
			break;
		default:
			rt_log("Option '%c' unknown\n", cp[1]);
			break;
		}
		while( *cp && *cp++ != ' ' )
			;
		while( *cp == ' ' )  cp++;
	}
	(void)free(buf);
}

ph_start(pc, buf)
register struct pkg_comm *pc;
char *buf;
{
	char *title_file, *title_obj;	/* name of file and first object */
	struct rt_i *rtip;
	register int i;

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

	title_file = cmd_args[0];
	title_obj = cmd_args[1];

	RES_RELEASE( &res_pt );
	RES_RELEASE( &res_seg );
	RES_RELEASE( &res_malloc );
	RES_RELEASE( &res_bitv );

	rt_prep_timer();		/* Start timing preparations */

	/* Build directory of GED database */
	if( (rtip=rt_dirbuild( title_file, idbuf, sizeof(idbuf) )) == RTI_NULL )  {
		rt_log("rt:  rt_dirbuild(%s) failure\n", title_file);
		exit(2);
	}
	ap.a_rt_i = rtip;
	rt_log( "db title:  %s\n", idbuf);

	(void)rt_read_timer( outbuf, sizeof(outbuf) );
	rt_log("DB TOC: %s\n", outbuf);
	rt_prep_timer();

	/* Load the desired portion of the model */
	for( i=1; i<numargs; i++ )  {
		(void)rt_gettree(rtip, cmd_args[i]);
	}
	(void)rt_read_timer( outbuf, sizeof(outbuf) );
	rt_log("DB WALK: %s\n", outbuf);
	rt_prep_timer();

	/* Allow library to prepare itself */
	rt_prep(rtip);

	/* initialize application (claim output to file) */
	view_init( &ap, title_file, title_obj, npts, 1 );

	(void)rt_read_timer( outbuf, sizeof(outbuf) );
	rt_log( "PREP: %s\n", outbuf );

	if( rt_i.HeadSolid == SOLTAB_NULL )  {
		rt_log("rt: No solids remain after prep.\n");
		exit(3);
	}

	seen_start = 1;
	(void)free(buf);

	/* Acknowledge that we are ready */
	pkg_send( MSG_START, "", 0, pcsrv );
}

ph_matrix(pc, buf)
register struct pkg_comm *pc;
char *buf;
{
	static vect_t temp;

	/* Visible part is from -1 to +1 in view space */
	if( sscanf( buf,
		"%e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e %e",
		&viewsize,
		&eye_model[X],
		&eye_model[Y],
		&eye_model[Z],
		&Viewrotscale[0], 
		&Viewrotscale[1], 
		&Viewrotscale[2], 
		&Viewrotscale[3], 
		&Viewrotscale[4], 
		&Viewrotscale[5], 
		&Viewrotscale[6], 
		&Viewrotscale[7], 
		&Viewrotscale[8], 
		&Viewrotscale[9], 
		&Viewrotscale[10], 
		&Viewrotscale[11], 
		&Viewrotscale[12], 
		&Viewrotscale[13], 
		&Viewrotscale[14], 
		&Viewrotscale[15] ) != 20 )  {
		rt_log("matrix: scanf failure\n");
		exit(2);
	}

	/* model2view takes us to eye_model location & orientation */
	mat_idn( toEye );
	toEye[MDX] = -eye_model[X];
	toEye[MDY] = -eye_model[Y];
	toEye[MDZ] = -eye_model[Z];
	Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
	mat_mul( model2view, Viewrotscale, toEye );
	mat_inv( view2model, model2view );

	/* Chop -1.0..+1.0 range into npts parts */
	VSET( temp, 2.0/npts, 0, 0 );
	MAT4X3VEC( dx_model, view2model, temp );
	VSET( temp, 0, 2.0/npts, 0 );
	MAT4X3VEC( dy_model, view2model, temp );

	/* "Upper left" corner of viewing plane */
	if( perspective )  {
		VSET( temp, -1, -1, -zoomout );	/* viewing plane */
	}  else  {
		VSET( temp, 0, 0, -1 );
		MAT4X3VEC( ap.a_ray.r_dir, view2model, temp );
		VUNITIZE( ap.a_ray.r_dir );

		VSET( temp, -1, -1, 0 );	/* eye plane */
	}
	MAT4X3PNT( viewbase_model, view2model, temp );

	/* initialize lighting */
	view_2init( &ap, -1 );

	seen_matrix = 1;
	(void)free(buf);
}

ph_lines(pc, buf)
register struct pkg_comm *pc;
char *buf;
{
	register int x,y;
	int a,b;

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

	for( y = a; y <= b; y++)  {
		for( x = 0; x < npts; x++)  {
			work_word = (x<<16) | y;
			worker( &ap );
		}
		scanbuf[0] = y&0xFF;
		scanbuf[1] = (y>>8);
		pkg_send( MSG_PIXELS, scanbuf, LINENO_WIDTH+npts*3, pcsrv );
	}
	(void)free(buf);
}

/*
 *  			W O R K E R
 *  
 *  Compute one pixel, and store it.
 */
void
worker( ap )
register struct application *ap;
{
	LOCAL struct application a;
	LOCAL vect_t tempdir;
	register char *pixelp;
	register int r,g,b;
	register int com;

	a.a_onehit = 1;
	{
		com = work_word;
		a.a_x = (com>>16)&0xFFFF;
		a.a_y = (com&0xFFFF);
		a.a_hit = ap->a_hit;
		a.a_miss = ap->a_miss;
		a.a_rt_i = ap->a_rt_i;
		VJOIN2( a.a_ray.r_pt, viewbase_model,
			a.a_x, dx_model, 
			(npts-a.a_y-1), dy_model );
		if( perspective )  {
			VSUB2( a.a_ray.r_dir,
				a.a_ray.r_pt, eye_model );
			VUNITIZE( a.a_ray.r_dir );
			VMOVE( a.a_ray.r_pt, eye_model );
		} else {
		 	VMOVE( a.a_ray.r_dir, ap->a_ray.r_dir );
		}

		a.a_level = 0;		/* recursion level */
		rt_shootray( &a );

		pixelp = scanbuf+LINENO_WIDTH+(a.a_x*3);
		r = a.a_color[0]*255.;
		g = a.a_color[1]*255.;
		b = a.a_color[2]*255.;
		/* Truncate glints, etc */
		if( r > 255 )  r=255;
		if( g > 255 )  g=255;
		if( b > 255 )  b=255;
		if( r<0 || g<0 || b<0 )  {
			rt_log("Negative RGB %d,%d,%d\n", r, g, b );
			r = 0x80;
			g = 0xFF;
			b = 0x80;
		}
		*pixelp++ = r ;
		*pixelp++ = g ;
		*pixelp++ = b ;
	}
}

int print_on = 1;
ph_loglvl(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
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
	(void)sprintf( cp, str, a, b, c, d, e, f, g, h );
	while( *cp++ )  ;		/* leave at null */
	if( cp[-1] != '\n' )
		return;
	if( pkg_send( MSG_PRINT, buf+1, strlen(buf+1)+1, pcsrv ) < 0 )
		exit(12);
	cp = buf+1;
}

void
rt_bomb(str)
char *str;
{
	rt_log("rt:  Fatal Error %s, aborting\n", str);
	abort();	/* should dump */
	exit(12);
}

ph_unexp(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	register int i;

	for( i=0; i<pkg_swlen; i++ )  {
		if( pkg_switch[i].pks_type == pc->pkc_type )  break;
	}
	rt_log("rtsrv: unable to handle %s message: len %d",
		pkg_switch[i].pks_title, pc->pkc_len);
	*buf = '*';
	(void)free(buf);
}

ph_end(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	pkg_close(pcsrv);
	exit(0);
}

ph_print(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	printf("msg: %s\n", buf);
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
