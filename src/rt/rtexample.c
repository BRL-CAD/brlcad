/*                     R T E X A M P L E . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file rtexample.c
 *
 *  A trivial example of a program that uses librt.  With comments.
 *
 *    cc -I/usr/include/brlcad -o rtexample rtexample.c librt.a -lm
 */
#include "common.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"		/* machine specific definitions */
#include "vmath.h"		/* vector math macros */
#include "raytrace.h"		/* librt interface definitions */
#include "rtprivate.h"


/* routines for shootray() to call on hit or miss */
extern int hit(struct application *ap, struct partition *PartHeadp, struct seg *segs);
extern int miss(register struct application *ap);


char	usage[] = "\
Usage:  rtexample model.g objects...\n";

main(int argc, char **argv)
{
    /* every application needs one of these */
    struct application	ap;

    static struct rt_i *rtip;	/* rt_dirbuild returns this */
    char idbuf[RT_BUFSIZE] = {0};		/* First ID record info */

    if( argc < 3 )  {
	(void)fputs(usage, stderr);
	exit(1);
    }

    /*
     *  Load database.
     *  rt_dirbuild() returns an "instance" pointer which describes
     *  the database to be ray traced.  It also gives you back the
     *  title string in the header (ID) record.
     */
    if( (rtip=rt_dirbuild(argv[1], idbuf, sizeof(idbuf))) == RTI_NULL ) {
	fprintf(stderr,"rtexample: rt_dirbuild failure\n");
	exit(2);
    }

    /* intialize the application structure to all zeros */
    RT_APPLICATION_INIT(&ap);

    ap.a_rt_i = rtip;	/* your application uses this instance */
    fprintf(stderr, "db title: %s\n", idbuf);

    /* Walk trees.
     * Here you identify any object trees in the database that you
     * want included in the ray trace.
     */
    while( argc > 2 )  {
	if( rt_gettree(rtip, argv[2]) < 0 )
	    fprintf(stderr,"rt_gettree(%s) FAILED\n", argv[0]);
	argc--;
	argv++;
    }
    /*
     * This next call gets the database ready for ray tracing.
     * (it precomputes some values, sets up space partitioning, etc.)
     */
    rt_prep_parallel(rtip,1);

    /*
     * Set the ray start point and direction
     * rt_shootray() uses these two to determine what ray to fire.
     * In this case we simply shoot down the z axis toward the
     * origin from 10 meters away [librt assumes units of millimeters.
     * not that is really maters here, but an MGED database made with
     * units=mm will have the same values in the file (and thus in
     * librt) that you see displayed by MGED.
     */
    VSET( ap.a_ray.r_pt, 0, 0, 10000 );
    VSET( ap.a_ray.r_dir, 0, 0, -1 );

    VPRINT( "Pnt", ap.a_ray.r_pt );
    VPRINT( "Dir", ap.a_ray.r_dir );

    /* Shoot Ray */
    ap.a_hit = hit;			/* where to go on a hit */
    ap.a_miss = miss;		/* where to go on a miss */
    (void)rt_shootray( &ap );	/* do it */

    /*
     * A real application would probably set up another
     * ray and fire again.
     */

    return(0);
}


/*
 *  rt_shootray() was told to call this on a hit.  He gives up the
 *  application structure which describes the state of the world
 *  (see raytrace.h), and a circular linked list of partitions,
 *  each one describing one in and out segment of one region.
 */
hit(register struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    /* see raytrace.h for all of these guys */
    register struct partition *pp;
    register struct hit *hitp;
    register struct soltab *stp;
    struct curvature cur;
    point_t		pt;
    vect_t		inormal;
    vect_t		onormal;

    /* examine each partition until we get back to the head */
    for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
	bu_log("\n--- Hit region %s (in %s, out %s)\n",
	       pp->pt_regionp->reg_name,
	       pp->pt_inseg->seg_stp->st_name,
	       pp->pt_outseg->seg_stp->st_name );

	/* inhit info */
	hitp = pp->pt_inhit;
	stp = pp->pt_inseg->seg_stp;

	VJOIN1( pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir );

	/* This macro takes care of the flip flag and all that */
	RT_HIT_NORMAL( inormal, hitp, stp, &(ap->a_ray), pp->pt_inflip );

	rt_pr_hit( "  In", hitp );
	VPRINT(    "  Ipoint", pt );
	VPRINT(    "  Inormal", inormal );
	/*
	 * This next macro fills in the curvature information
	 * which consists on a principle direction vector, and
	 * the inverse radii of curvature along that direction
	 * and perpendicular to it.  Positive curvature bends
	 * toward the outward pointing normal.
	 */
	RT_CURVATURE( &cur, hitp, pp->pt_inflip, stp );
	VPRINT("PDir", cur.crv_pdir );
	bu_log(" c1=%g\n", cur.crv_c1);
	bu_log(" c2=%g\n", cur.crv_c2);

	/* outhit info */
	hitp = pp->pt_outhit;
	stp = pp->pt_outseg->seg_stp;
	VJOIN1( pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir );
	RT_HIT_NORMAL( onormal, hitp, stp, &(ap->a_ray), pp->pt_outflip );

	rt_pr_hit( "  Out", hitp );
	VPRINT(    "  Opoint", pt );
	VPRINT(    "  Onormal", onormal );
    }

    /*
     * A more complicated application would probably fill in a
     * new local application structure and describe say a reflected
     * or refracted ray, and then call rt_shootray with it.
     */

    /*
     * This value is returned by rt_shootray
     * a hit usually returns 1, miss 0.
     */
    return(1);
}


/*
 * rt_shootray() was told to call this on a miss.
 */
miss(register struct application *ap)
{
    bu_log("missed\n");
    return(0);
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
