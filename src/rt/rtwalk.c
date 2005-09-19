/*                        R T W A L K . C
 * BRL-CAD
 *
 * Copyright (C) 1987-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file rtwalk.c
 *
 *  Demonstration Ray Tracing main program, using RT library.
 *  Walk a path *without running into any geometry*,
 *  given the start and goal points.
 *
 *  Authors -
 *	Michael John Muuss
 *	Robert J. Reschly, Jr.
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSrt[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"
#include "../librt/debug.h"
#include "plot3.h"


char	usage[] = "\
Usage:  rtwalk [options] startXYZ destXYZ model.g objects...\n\
 -X #		Set debug flags\n\
	1	plots on stdout\n\
	2	plots, attempts in red\n\
	3	all plots, plus printing\n\
 -x #		Set librt debug flags\n\
 -n #		Number of steps\n\
 -v #		Viewsize\n\
(output is rtwalk.mats)\n\
";

double		viewsize = 42;

/*
 *	0 - off
 *	1 - plots
 *	2 - plots with attempted rays in red
 *	3 - lots of printfs too
 */

int		npsw = 1;		/* Run serially */
int		interactive = 0;	/* human is watching results */

struct application	ap;

point_t		start_point;
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

extern int hit(struct application *ap, struct partition *PartHeadp, struct seg *segp);
extern int miss(register struct application *ap);

FILE		*plotfp;
FILE		*outfp = NULL;

void		proj_goal(void);
void		write_matrix(int frame);

/*
 *			G E T _ A R G S
 */
int
get_args(int argc, register char **argv)
{
	register int c;

	while( (c=bu_getopt( argc, argv, "x:X:n:v:" )) != EOF )  {
		switch( c )  {
		case 'x':
			sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.debug );
			fprintf(stderr,"librt rt_g.debug=x%x\n", rt_g.debug);
			break;
		case 'X':
			sscanf( bu_optarg, "%x", (unsigned int *)&rdebug );
			fprintf(stderr,"rt rdebug=x%x\n", rdebug);
			break;

		case 'n':
			nsteps = atoi( bu_optarg );
			break;
		case 'v':
			viewsize = atof( bu_optarg );
			break;

		default:		/* '?' */
			fprintf(stderr,"unknown option %c\n", c);
			return(0);	/* BAD */
		}
	}
	return(1);			/* OK */
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	static struct rt_i *rtip;
	char	*title_file;
	char	idbuf[RT_BUFSIZE] = {0};		/* First ID record info */
	int	curstep;
	vect_t	first_dir;		/* First dir chosen on a step */
	int	i;

	bu_semaphore_init( RT_SEM_LAST );

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit(1);
	}
	if( bu_optind+7 >= argc )  {
		(void)fputs(usage, stderr);
		exit(1);
	}

	/* Start point */
	start_point[X] = atof( argv[bu_optind] );
	start_point[Y] = atof( argv[bu_optind+1] );
	start_point[Z] = atof( argv[bu_optind+2] );

	/* Destination point */
	goal_point[X] = atof( argv[bu_optind+3] );
	goal_point[Y] = atof( argv[bu_optind+4] );
	goal_point[Z] = atof( argv[bu_optind+5] );
	bu_optind += 6;

	VSUB2( first_dir, goal_point, start_point );
	incr_dist = MAGNITUDE(first_dir) / nsteps;
	VMOVE( ap.a_ray.r_pt, start_point );
	VMOVE( ap.a_ray.r_dir, first_dir );
	VUNITIZE( ap.a_ray.r_dir );	/* initial dir, for dir_prev_step */

	fprintf(stderr,"nsteps = %d, incr_dist = %gmm\n", nsteps, incr_dist );
	fprintf(stderr,"viewsize = %gmm\n", viewsize);

	/* Load database */
	title_file = argv[bu_optind++];
	if( (rtip=rt_dirbuild(title_file, idbuf, sizeof(idbuf))) == RTI_NULL ) {
		fprintf(stderr,"rtwalk:  rt_dirbuild failure\n");
		exit(2);
	}
	ap.a_rt_i = rtip;
	fprintf(stderr, "db title:  %s\n", idbuf);

	/* Walk trees */
	for( i=bu_optind; i < argc; i++ )  {
		if( rt_gettree(rtip, argv[bu_optind]) < 0 )
			fprintf(stderr,"rt_gettree(%s) FAILED\n", argv[bu_optind]);
		bu_optind++;
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
	plotfp = stdout;

	/* Plot all of the solids */
	if( R_DEBUG > 0 )  {
		pl_color( plotfp, 150, 150, 150 );
		rt_plot_all_solids( plotfp, rtip, &rt_uniresource );
	}

	/* Take a walk */
	for( curstep = 0; curstep < nsteps*4; curstep++ )  {
		mat_t	mat;
		int	failed_try;

		/*
		 *  In order to be able to compute deltas from the
		 *  previous to the current step, here we handle
		 *  the results of the last iteration.
		 *  The first and last iterations result in no output
		 */
		if(R_DEBUG>=3) {
			VPRINT("pos", ap.a_ray.r_pt);
		}
		if( curstep > 0 )  {
			if( R_DEBUG > 0 )  {
				if( curstep&1 )
					pl_color( plotfp, 0, 255, 0 );
				else
					pl_color( plotfp, 0, 0, 255 );
				pdv_3line( plotfp,
					pt_prev_step, ap.a_ray.r_pt );
			}
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
			int	i;

			/* Shoot Ray */
			if(R_DEBUG>=3)fprintf(stderr,"try=%d, maxtogo=%g  ",
				failed_try, max_dist_togo);
			ap.a_hit = hit;
			ap.a_miss = miss;
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

			if(R_DEBUG > 2 )   {
				/* Log attempted ray in Red */
				VJOIN1( out, ap.a_ray.r_pt,
					incr_dist*4, ap.a_ray.r_dir );
				pl_color( plotfp, 255, 0, 0 );
				pdv_3line( plotfp,
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
					if(R_DEBUG>=3)fprintf(stderr,
						"Try prev dir\n");
					VMOVE( ap.a_ray.r_dir, dir_prev_step );
					continue;
				}
				if(R_DEBUG>=3)fprintf(stderr,"Try tangent\n");
				proj_goal();
				continue;
			} else if( failed_try <= 7 )  {
				/* Try 7 azimuthal escapes, 1..7 */
				i = failed_try-1+1;	/*  1..7 */
				if(R_DEBUG>=3)fprintf(stderr,"Try az %d\n", i);
				bn_mat_ae( mat, i*45.0, 0.0 );
			} else if( failed_try <= 14 ) {
				/* Try 7 Elevations to escape, 8..14 */
				i = failed_try-8+1;	/*     1..7 */
				if(R_DEBUG>=3)fprintf(stderr,"Try el %d\n", i);
				bn_mat_ae( mat, 0.0, i*45.0 );
			} else {
				fprintf(stderr,"trapped, giving up on escape\n");
				exit(1);
			}
			MAT4X3VEC( ap.a_ray.r_dir, mat, first_dir );

			/*
			 *  If new ray is nearly perpendicular to
			 *  the tangent plane, it is doomed to failure;
			 *  pick any tangent and use that instead.
			 */
			if( (VDOT( ap.a_ray.r_dir, norm_cur_try )) < -0.9995 )  {
				vect_t	olddir;

				VMOVE( olddir, ap.a_ray.r_dir );
				VCROSS( ap.a_ray.r_dir, olddir, norm_cur_try );
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

int hit(register struct application *ap, struct partition *PartHeadp, struct seg *segp)
{
	register struct partition *pp;
	register struct soltab *stp;
	register struct hit *hitp;

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist >= 0.0 )  break;
	if( pp == PartHeadp )  {
		bu_log("hit:  no hit out front?\n");
		return(0);
	}
	hitp = pp->pt_inhit;
	stp = pp->pt_inseg->seg_stp;
	VJOIN1( hit_cur_try, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir );
	RT_HIT_NORMAL( norm_cur_try, hitp, stp, &(ap->a_ray), pp->pt_inflip );
	RT_CURVATURE( &curve_cur, hitp, pp->pt_inflip, stp );

	clear_dist = hitp->hit_dist - 1;	/* Decrease 1mm */

	return(1);	/* HIT */
}

int
miss(register struct application *ap)
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
void
proj_goal(void)
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
void
write_matrix(int frame)
{
	fprintf(outfp, "start %d;\n", frame);
	fprintf(outfp, "clean;\n");
	fprintf(outfp, "viewsize %g;\n", viewsize);
	fprintf(outfp, "eye_pt %g %g %g;\n", V3ARGS(pt_prev_step) );
	fprintf(outfp, "lookat_pt %g %g %g  0;\n", V3ARGS(goal_point) );
	fprintf(outfp, "end;\n\n" );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
