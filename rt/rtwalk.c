/*
 *			R T W A L K . C 
 *
 *  Demonstration Ray Tracing main program, using RT library.
 *  Walk a path *without running into any geometry*,
 *  given any two of these three parameters:
 *	start point
 *	at point
 *
 *  Authors -
 *	Michael John Muuss
 *	Robert J. Reschly, Jr.
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSrt[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./rdebug.h"
#include "../librt/debug.h"

extern int	getopt();
extern char	*optarg;
extern int	optind;

char	usage[] = "\
Usage:  rtwalk [options] model.g objects...\n\
 -x #		Set librt debug flags\n\
 -p # # #	Set starting point\n\
 -a # # #	Set shoot-at point (destination)\n\
 -n #		Number of steps\n\
 -v #		Viewsize\n\
";

extern double	atof();
extern char	*sbrk();

double		viewsize = 42;

/*
 *	0 - off
 *	1 - plots
 *	2 - plots with attempted rays in red
 *	3 - lots of printfs too
 */
int		debug;			/* program debugging (not library) */

int		npsw = 1;		/* Run serially */
int		parallel = 0;
int		interactive = 0;	/* human is watching results */

struct resource		resource;
struct application	ap;

int		set_pt = 0;
int		set_at = 0;
point_t		goal_point;

vect_t		dir_prev_step;		/* Dir used on last step */
point_t		pt_prev_step;
vect_t		norm_prev_step = {0, 0, 0};

vect_t		norm_cur_try;		/* normal vector at current hit pt */
point_t		hit_cur_try;
struct curvature curve_cur;

int		nsteps = 10;
fastf_t		incr_dist;
double		clear_dist = 0.0;
double		max_dist_togo;

extern int hit(), miss();

static double deg2rad = 57.29577951125853957129;

FILE		*plotfp = stdout;
FILE		*outfp = NULL;

/*
 *			M A I N
 */
main(argc, argv)
int argc;
char **argv;
{
	static struct rt_i *rtip;
	static vect_t temp;
	char	*title_file;
	char	idbuf[132];		/* First ID record info */
	int	curstep;
	vect_t	first_dir;	/* First dir chosen on a step */

	RES_INIT( &rt_g.res_syscall );
	RES_INIT( &rt_g.res_worker );
	RES_INIT( &rt_g.res_stats );
	RES_INIT( &rt_g.res_results );

	if( argc < 3 )  {
		(void)fputs(usage, stderr);
		exit(1);
	}
	argc--;
	argv++;

	while( argv[0][0] == '-' ) switch( argv[0][1] )  {
	case 'x':
		sscanf( argv[1], "%x", &rt_g.debug );
		fprintf(stderr,"librt rt_g.debug=x%x\n", rt_g.debug);
		argc -= 2;
		argv += 2;
		break;


	case 'p':
		if( argc < 4 )  goto err;
		ap.a_ray.r_pt[X] = atof( argv[1] );
		ap.a_ray.r_pt[Y] = atof( argv[2] );
		ap.a_ray.r_pt[Z] = atof( argv[3] );
		set_pt = 1;
		argc -= 4;
		argv += 4;
		continue;

	case 'a':
		if( argc < 4 )  goto err;
		goal_point[X] = atof( argv[1] );
		goal_point[Y] = atof( argv[2] );
		goal_point[Z] = atof( argv[3] );
		set_at = 1;
		argc -= 4;
		argv += 4;
		continue;

	case 'n':
		if( argc < 2 )  goto err;
		nsteps = atoi( argv[1] );
		argc -= 2;
		argv += 2;
		continue;

	case 'v':
		if( argc < 2 )  goto err;
		viewsize = atof( argv[1] );
		argc -= 2;
		argv += 2;
		continue;

	default:
err:
		(void)fputs(usage, stderr);
		exit(1);
	}
	if( argc < 2 )  {
		fprintf(stderr,"rtwalk: MGED database not specified\n");
		(void)fputs(usage, stderr);
		exit(1);
	}

	if( set_pt + set_at != 2 )  goto err;

	VSUB2( first_dir, goal_point, ap.a_ray.r_pt );
	incr_dist = MAGNITUDE(first_dir) / nsteps;
	VMOVE( ap.a_ray.r_dir, first_dir );
	VUNITIZE( ap.a_ray.r_dir );	/* initial dir, for dir_prev_step */

	fprintf(stderr,"nsteps = %d, incr_dist = %gmm\n", nsteps, incr_dist );
	fprintf(stderr,"viewsize = %gmm\n", viewsize);

	/* Load database */
	title_file = argv[0];
	argv++;
	argc--;
	if( (rtip=rt_dirbuild(title_file, idbuf, sizeof(idbuf))) == RTI_NULL ) {
		fprintf(stderr,"rtwalk:  rt_dirbuild failure\n");
		exit(2);
	}
	ap.a_rt_i = rtip;
	fprintf(stderr, "db title:  %s\n", idbuf);

	/* Walk trees */
	while( argc > 0 )  {
		if( rt_gettree(rtip, argv[0]) < 0 )
			fprintf(stderr,"rt_gettree(%s) FAILED\n", argv[0]);
		argc--;
		argv++;
	}

	/* Prep finds the model RPP, needed for the plotting step */
	rt_prep(rtip);

	/*
	 *  With stdout for the plot file, and stderr for
	 *  remarks, the output must go into a file.
	 */
	if( (outfp=fopen("rtwalk.mats", "w")) == NULL )  {
		perror("rtwalk.mats");
		exit(1);
	}

	/* Plot all of the solid RPPs in a light grey */
	pl_3space( plotfp, 0,0,0, 4096, 4096, 4096);
	pl_color( plotfp, 150, 150, 150 );
	{
		register struct soltab *stp;
		for(stp=rtip->HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw)  {
			if( stp->st_aradius >= INFINITY )
				continue;
			rt_draw_box( plotfp, rtip, stp->st_min, stp->st_max );
		}
	}

	/* Take a walk */
	for( curstep = 0; curstep < nsteps*4; curstep++ )  {
		mat_t	mat;
		int	failed_try;

		/* Record this step */
		if(debug>=3) {VPRINT("pos", ap.a_ray.r_pt);}
		if( curstep > 0 )  {
			if( curstep&1 )
				pl_color( plotfp, 0, 255, 0 );
			else
				pl_color( plotfp, 0, 0, 255 );
			rt_drawvec( plotfp, ap.a_rt_i,
				pt_prev_step, ap.a_ray.r_pt );
			write_matrix(curstep);
		}
		VMOVE( pt_prev_step, ap.a_ray.r_pt );
		VMOVE( dir_prev_step, ap.a_ray.r_dir );
		VSETALL( norm_cur_try, 0 );	/* sanity */

		/* See if goal has been reached */
		VSUB2( first_dir, goal_point, ap.a_ray.r_pt );
		if( (max_dist_togo=MAGNITUDE(first_dir)) < 1.0 )  {
			fprintf(stderr,"Complete in %d steps\n", curstep);
			exit(0);
		}

		/*  See if there is significant clear space ahead
		 *  Avoid taking small steps.
		 */
		if( clear_dist < incr_dist * 0.25 )
			clear_dist = 0.0;
		if( clear_dist > 0.0 )
			goto advance;

		/*
		 * Initial direction:  Head directly towards the goal.
		 */
		VUNITIZE( first_dir );
		VMOVE( ap.a_ray.r_dir, first_dir );

		for( failed_try=0; failed_try<100; failed_try++ )  {
			vect_t	out;
			fastf_t	dot;
			int	i;

			/* Shoot Ray */
			if(debug>=3)fprintf(stderr,"try=%d, maxtogo=%g  ",
				failed_try, max_dist_togo);
			ap.a_hit = hit;
			ap.a_miss = miss;
			ap.a_resource = &resource;
			ap.a_onehit = 1;
			if( rt_shootray( &ap ) == 0 )  {
				/* A miss, the way is clear all the way */
				clear_dist = max_dist_togo*2;
				break;
			}
			/* Hit, check distance to closest obstacle */
			if( clear_dist >= incr_dist )  {
				/* Clear for at least one more step.
				 * Zap memory of prev normal --
				 * this probe ahead does not count.
				 * Current normal has no significance,
				 * because object is more than one step away.
				 */
				VSETALL( norm_cur_try, 0 );
				break;
			}
			/*
			 * Failed, try another direction
			 */

			if(debug > 2 )   {
				/* Log attempted ray in Red */
				VJOIN1( out, ap.a_ray.r_pt,
					incr_dist*4, ap.a_ray.r_dir );
				pl_color( plotfp, 255, 0, 0 );
				rt_drawvec( plotfp, ap.a_rt_i,
					pt_prev_step, out );
			}

			/* Initial try was in direction of goal, it failed. */
			if( failed_try == 0 )  {
				/*  First recovery attempt.
				 *  If hit normal has not changed, continue
				 *  the direction of the last step.
				 *  Otherwise, head on tangent plane.
				 */
				if( VEQUAL( norm_cur_try, norm_prev_step ) )  {
					if(debug>=3)fprintf(stderr,
						"Try prev dir\n");
					VMOVE( ap.a_ray.r_dir, dir_prev_step );
					continue;
				}
				if(debug>=3)fprintf(stderr,"Try tangent\n");
				proj_goal();
				continue;
			} else if( failed_try <= 7 )  {
				/* Try 7 azimuthal escapes, 1..7 */
				i = failed_try-1+1;	/*  1..7 */
				if(debug>=3)fprintf(stderr,"Try az %d\n", i);
				mat_ae( mat, i*45.0, 0.0 );
			} else if( failed_try <= 14 ) {
				/* Try 7 Elevations to escape, 8..14 */
				i = failed_try-8+1;	/*     1..7 */
				if(debug>=3)fprintf(stderr,"Try el %d\n", i);
				mat_ae( mat, 0.0, i*45.0 );
			} else {
				fprintf(stderr,"giving up on escape\n");
				exit(1);
			}
			MAT4X3VEC( ap.a_ray.r_dir, mat, first_dir );

			/* Limit new direction to lie "above" or on
			 * the tangent plane at the current hit point,
			 * to prevent constantly banging into the
			 * object we just hit.
			 */
			if( (dot=VDOT( ap.a_ray.r_dir, norm_cur_try )) < 0 )  {
				vect_t	olddir;

				if( dot <= -0.9995 )  {
					/* Pick any tangent */
					VMOVE( olddir, ap.a_ray.r_dir );
					VCROSS( ap.a_ray.r_dir, olddir, norm_cur_try );
				} else {
					proj_goal();
				}
			}
		}
		if( failed_try > 0 )  {
			/* Extra trys were required, prevent runaways */
			if( clear_dist > incr_dist )
				clear_dist = incr_dist;
		}

		/* One simple attempt at not overshooting the goal.
		 * Really should measure distance point-to-line
		 */
		if( clear_dist > max_dist_togo )
			clear_dist = max_dist_togo;

		/* Advance position along ray */
advance:	;
		if( clear_dist > 0.0 )  {
			fastf_t	step;
			if( clear_dist < incr_dist )
				step = clear_dist;
			else
				step = incr_dist;
			VJOIN1( ap.a_ray.r_pt, ap.a_ray.r_pt,
				step, ap.a_ray.r_dir );
			clear_dist -= step;
		}

		/* Save status */
		VMOVE( norm_prev_step, norm_cur_try );
	}
	fprintf(stderr,"%d steps used without reaching goal by %gmm\n", curstep, max_dist_togo);
	exit(1);
}

hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp;
	register struct soltab *stp;
	register struct hit *hitp;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		rt_log("hit:  no hit out front?\n");
		return(0);
	}
	hitp = pp->pt_inhit;
	stp = pp->pt_inseg->seg_stp;
	RT_HIT_NORM( hitp, stp, &(ap->a_ray) );
	RT_CURVE( &curve_cur, hitp, stp );
	VMOVE( hit_cur_try, hitp->hit_point );
	VMOVE( norm_cur_try, hitp->hit_normal );

	clear_dist = hitp->hit_dist - 1;	/* Decrease 1mm */

	return(1);	/* HIT */
}

miss()
{
	return(0);
}

/*
 *			P R O J _ G O A L
 *
 *  When progress towards the goal is blocked by an object,
 *  head off "towards the side" to try to get around.
 *  Project the goal point onto the plane tangent to the object
 *  at the hit point.  Head for the projection of the goal point,
 *  which should keep things moving in the right general direction,
 *  except perhaps for concave objects.
 */
proj_goal()
{
	vect_t	goal_dir;
	vect_t	goal_proj;
	vect_t	newdir;
	fastf_t	k;

	if( VDOT( ap.a_ray.r_dir, norm_cur_try ) < -0.9995 )  {
		/* Projected goal will be right where we are now.
		 * Pick any tangent at all.
		 * Use principle dir of curvature.
		 */
		VMOVE( ap.a_ray.r_dir, curve_cur.crv_pdir );
		return;
	}

	VSUB2( goal_dir, hit_cur_try, goal_point );
	k = VDOT( goal_dir, norm_cur_try );
	VJOIN1( goal_proj, goal_point,
		k, norm_cur_try );
	VSUB2( newdir, goal_proj, hit_cur_try );
	VUNITIZE( newdir );
	VMOVE( ap.a_ray.r_dir, newdir );
}

/*
 *			W R I T E _ M A T R I X
 */
write_matrix(frame)
{
	mat_t	viewrot;
	int	i;
	vect_t	model;
	vect_t	view;

	/* Build viewrot matrix */
/*	VSUB2( model, hit_cur_try, pt_prev_step );	/* Look to next step */
	VSUB2( model, goal_point, pt_prev_step );
	VUNITIZE( model );

	mat_lookat( viewrot, model );

/*	fprintf(outfp, "start %d;\n", frame); */
	fprintf(outfp, "%g\n", viewsize);
	fprintf(outfp, "%g %g %g",
		pt_prev_step[X],
		pt_prev_step[Y],
		pt_prev_step[Z]);

	for( i=0; i < 16; i++ ) {
		if( (i%4) == 0 )  (void)fprintf(outfp, "\n");
		(void)fprintf( outfp, "%.9e ", viewrot[i] );
	}
	(void)fprintf(outfp,"\n\n");
}

#if defined(SYSV)
#if !defined(bcopy)
bcopy(from,to,count)
{
	memcpy( to, from, count );
}
#endif
#if !defined(bzero)
bzero(str,n)
{
	memset( str, '\0', n );
}
#endif
#endif

/* wrapper for atan2.  On SGI (and perhaps others), x==0 returns infinity */
double
xatan2(y,x)
double	y,x;
{
	if( x > -1.0e-20 && x < 1.0e-20 )  {
		/* X is equal to zero, check Y */
		if( y < -1.0e-20 )  return( -3.14159265358979323/2 );
		if( y >  1.0e-20 )  return(  3.14159265358979323/2 );
		return(0.0);
	}
	return( atan2( y, x ) );
}
/*
 *			M A T _ F R O M T O
 *
 *  Given two vectors, compute a rotation matrix that will transform
 *  space by the angle between the two.  Since there are many
 *  candidate matricies, the method used here is to convert the vectors
 *  to azimuth/elevation form (azimuth is +X, elevation is +Z),
 *  take the difference, and form the rotation matrix.
 *  See mat_ae for that algorithm.
 *
 *  The input 'from' and 'to' vectors must be unit length.
 */
mat_fromto( m, from, to )
mat_t	m;
vect_t	from;
vect_t	to;
{
	double	az, el;
	LOCAL double sin_az, sin_el;
	LOCAL double cos_az, cos_el;

	az = xatan2( to[Y], to[X] ) - xatan2( from[Y], from[X] );
	el = asin( to[Z] ) - asin( from[Z] );

	sin_az = sin(az);
	cos_az = cos(az);
	sin_el = sin(el);
	cos_el = cos(el);

	m[0] = cos_el * cos_az;
	m[1] = -sin_az;
	m[2] = -sin_el * cos_az;
	m[3] = 0;

	m[4] = cos_el * sin_az;
	m[5] = cos_az;
	m[6] = -sin_el * sin_az;
	m[7] = 0;

	m[8] = sin_el;
	m[9] = 0;
	m[10] = cos_el;
	m[11] = 0;

	m[12] = m[13] = m[14] = 0;
	m[15] = 1.0;
}

/*
 *			M A T _ L O O K A T
 *
 *  Given a direction vector D, product a matrix suitable for use
 *  as a "model2view" matrix that transforms the vector D
 *  into the -Z ("view") axis.
 *
 *  Note that due to the special property of mat_fromto()
 *  that prevents "twist" on the vector by orienting on the X-Y
 *  plane, we must first find the transformation that maps
 *  D into the +X axis, and then rotate to the -Z axis.
 */
mat_lookat( rot, dir )
mat_t rot;
vect_t dir;
{
	mat_t	second;
	mat_t	first;
	vect_t	x;

	/* Rotate from Dir to +X */
	VSET( x, 1, 0, 0 );
	mat_fromto( first, dir, x );

	/* Rotate so that +X is now -Z axis */
	mat_angles( second, -90.0, 0.0, 90.0 );
	mat_mul( rot, second, first );
}
