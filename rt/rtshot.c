/*
 *			R T S H O T . C 
 *
 *  Demonstration Ray Tracing main program, using RT library.
 *  Fires a single ray, given any two of these three parameters:
 *	start point
 *	at point
 *	direction vector
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
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSrt[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "raytrace.h"
#include "./rdebug.h"
#include "../librt/debug.h"

char	usage[] = "\
Usage:  rtshot [options] model.g objects...\n\
 -U #		Set use_air flag\n\
 -u #		Set libbu debug flag\n\
 -x #		Set librt debug flags\n\
 -X #		Set rt program debug flags\n\
 -N #		Set NMG debug flags\n\
 -d # # #	Set direction vector\n\
 -p # # #	Set starting point\n\
 -a # # #	Set shoot-at point\n";

int		rdebug;			/* RT program debugging (not library) */
static FILE	*plotfp;		/* For plotting into */

struct application	ap;

int		set_dir = 0;
int		set_pt = 0;
int		set_at = 0;
vect_t		at_vect;
int		use_air = 0;		/* Handling of air */

extern int hit(), miss();

/*
 *			M A I N
 */
main(argc, argv)
int argc;
char **argv;
{
	static struct rt_i *rtip;
	char *title_file;
	char idbuf[132];		/* First ID record info */

	if( argc < 3 )  {
		(void)fputs(usage, stderr);
		exit(1);
	}
	argc--;
	argv++;

	while( argv[0][0] == '-' ) switch( argv[0][1] )  {
	case 'U':
		sscanf( argv[1], "%d", &use_air );
		argc -= 2;
		argv += 2;
		break;
	case 'u':
		sscanf( argv[1], "%x", &bu_debug );
		fprintf(stderr,"librt bu_debug=x%x\n", bu_debug);
		argc -= 2;
		argv += 2;
		break;
	case 'x':
		sscanf( argv[1], "%x", &rt_g.debug );
		fprintf(stderr,"librt rt_g.debug=x%x\n", rt_g.debug);
		argc -= 2;
		argv += 2;
		break;
	case 'X':
		sscanf( argv[1], "%x", &rdebug );
		fprintf(stderr,"rdebug=x%x\n", rdebug);
		argc -= 2;
		argv += 2;
		break;
	case 'N':
		sscanf( argv[1], "%x", &rt_g.NMG_debug);
		fprintf(stderr,"librt rt_g.NMG_debug=x%x\n", rt_g.NMG_debug);
		argc -= 2;
		argv += 2;
		break;
	case 'd':
		if( argc < 4 )  goto err;
		ap.a_ray.r_dir[X] = atof( argv[1] );
		ap.a_ray.r_dir[Y] = atof( argv[2] );
		ap.a_ray.r_dir[Z] = atof( argv[3] );
		set_dir = 1;
		argc -= 4;
		argv += 4;
		continue;

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
		at_vect[X] = atof( argv[1] );
		at_vect[Y] = atof( argv[2] );
		at_vect[Z] = atof( argv[3] );
		set_at = 1;
		argc -= 4;
		argv += 4;
		continue;
	default:
err:
		(void)fputs(usage, stderr);
		exit(1);
	}
	if( argc < 2 )  {
		fprintf(stderr,"rtshot: MGED database not specified\n");
		(void)fputs(usage, stderr);
		exit(1);
	}

	if( set_dir + set_pt + set_at != 2 )  goto err;

	/* Load database */
	title_file = argv[0];
	argv++;
	argc--;
	if( (rtip=rt_dirbuild(title_file, idbuf, sizeof(idbuf))) == RTI_NULL ) {
		fprintf(stderr,"rtshot:  rt_dirbuild failure\n");
		exit(2);
	}
	ap.a_rt_i = rtip;
	fprintf(stderr, "db title:  %s\n", idbuf);
	rtip->useair = use_air;

	/* Walk trees */
	while( argc > 0 )  {
		if( rt_gettree(rtip, argv[0]) < 0 )
			fprintf(stderr,"rt_gettree(%s) FAILED\n", argv[0]);
		argc--;
		argv++;
	}
	rt_prep(rtip);

	if( rdebug&RDEBUG_RAYPLOT )  {
		if( (plotfp = fopen("rtshot.plot", "w")) == NULL )  {
			perror("rtshot.plot");
			exit(1);
		}
		pdv_3space( plotfp, rtip->rti_pmin, rtip->rti_pmax );
	}

	/* Compute r_dir and r_pt from the inputs */
	if( set_at )  {
		if( set_dir ) {
			vect_t	diag;
			fastf_t	viewsize;
			VSUB2( diag, rtip->mdl_max, rtip->mdl_min );
			viewsize = MAGNITUDE( diag );
			VJOIN1( ap.a_ray.r_pt, at_vect,
				-viewsize/2.0, ap.a_ray.r_dir );
		} else {
			/* set_pt */
			VSUB2( ap.a_ray.r_dir, at_vect, ap.a_ray.r_pt );
		}
	}
	VUNITIZE( ap.a_ray.r_dir );

	VPRINT( "Pnt", ap.a_ray.r_pt );
	VPRINT( "Dir", ap.a_ray.r_dir );

	/* Shoot Ray */
	ap.a_hit = hit;
	ap.a_miss = miss;
	(void)rt_shootray( &ap );

	return(0);
}

hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp;
	register struct hit *hitp;
	register struct soltab *stp;
	struct curvature cur;
	fastf_t out;
	point_t inpt, outpt;
	vect_t	inormal, onormal;

	if( (pp=PartHeadp->pt_forw) == PartHeadp )
		return(0);		/* Nothing hit?? */

	/* First, plot ray start to inhit */
	if( rdebug&RDEBUG_RAYPLOT )  {
		if( pp->pt_inhit->hit_dist > 0.0001 )  {
			VJOIN1( inpt, ap->a_ray.r_pt,
				pp->pt_inhit->hit_dist, ap->a_ray.r_dir );
			pl_color( plotfp, 0, 0, 255 );
			pdv_3line( plotfp, ap->a_ray.r_pt, inpt );
		}
	}
	for( ; pp != PartHeadp; pp = pp->pt_forw )  {
		rt_log("\n--- Hit region %s (in %s, out %s)\n",
			pp->pt_regionp->reg_name,
			pp->pt_inseg->seg_stp->st_name,
			pp->pt_outseg->seg_stp->st_name );

		/* inhit info */
		stp = pp->pt_inseg->seg_stp;
		VJOIN1( inpt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir );
		RT_HIT_NORMAL( inormal, pp->pt_inhit, stp, &(ap->a_ray), pp->pt_inflip );
		RT_CURVATURE( &cur, pp->pt_inhit, pp->pt_inflip, stp );

		rt_pr_hit( "  In", pp->pt_inhit );
		VPRINT(    "  Ipoint", inpt );
		VPRINT(    "  Inormal", inormal );
		rt_log(    "   PDir (%g, %g, %g) c1=%g, c2=%g\n",
			V3ARGS(cur.crv_pdir), cur.crv_c1, cur.crv_c2);

		/* outhit info */
		stp = pp->pt_outseg->seg_stp;
		VJOIN1( outpt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir );
		RT_HIT_NORMAL( onormal, pp->pt_outhit, stp, &(ap->a_ray), pp->pt_outflip );
		RT_CURVATURE( &cur, pp->pt_outhit, pp->pt_outflip, stp );

		rt_pr_hit( "  Out", pp->pt_outhit );
		VPRINT(    "  Opoint", outpt );
		VPRINT(    "  Onormal", onormal );
		rt_log(    "   PDir (%g, %g, %g) c1=%g, c2=%g\n",
			V3ARGS(cur.crv_pdir), cur.crv_c1, cur.crv_c2);

		/* Plot inhit to outhit */
		if( rdebug&RDEBUG_RAYPLOT )  {
			if( (out = pp->pt_outhit->hit_dist) >= INFINITY )
				out = 10000;	/* to imply the direction */

			VJOIN1( outpt,
				ap->a_ray.r_pt, out,
				ap->a_ray.r_dir );
			pl_color( plotfp, 0, 255, 255 );
			pdv_3line( plotfp, inpt, outpt );
		}
	}
	return(1);
}

miss( ap )
register struct application *ap;
{
	rt_log("missed\n");
	if( rdebug&RDEBUG_RAYPLOT )  {
		vect_t	out;

		VJOIN1( out, ap->a_ray.r_pt,
			10000, ap->a_ray.r_dir );	/* to imply direction */
		pl_color( plotfp, 190, 0, 0 );
		pdv_3line( plotfp, ap->a_ray.r_pt, out );
	}
	return(0);
}
