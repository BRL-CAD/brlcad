/*
 *			R T E X A M P L E . C
 *
 *  A trivial example of a program that uses librt.  With comments.
 *
 *    cc -I/usr/include/brlcad -o rtexample rtexample.c librt.a -lm
 */
#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"		/* machine specific definitions */
#include "vmath.h"		/* vector math macros */
#include "raytrace.h"		/* librt interface definitions */

/* every application needs one of these */
struct application	ap;

/* routines for shootray() to call on hit or miss */
extern int hit(), miss();

char	usage[] = "\
Usage:  rtexample model.g objects...\n";

main(argc, argv)
int argc;
char **argv;
{
	static struct rt_i *rtip;	/* rt_dirbuild returns this */
	char idbuf[132];		/* First ID record info */

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
	rt_prep(rtip);

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
hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	/* see raytrace.h for all of these guys */
	register struct partition *pp;
	register struct hit *hitp;
	register struct soltab *stp;
	struct curvature cur;
	struct hit	ihit;
	struct hit	ohit;

	/* examine each partition until we get back to the head */
	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
		rt_log("\n--- Hit region %s (in %s, out %s)\n",
			pp->pt_regionp->reg_name,
			pp->pt_inseg->seg_stp->st_name,
			pp->pt_outseg->seg_stp->st_name );

		/* inhit info */
		hitp = pp->pt_inhit;
		stp = pp->pt_inseg->seg_stp;
		/*
		 * This next macro fills in the normal and hit_point in hitp.
		 * If you just want the hit point, get it from the hit
		 * distance by:
		 * VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
		 */
		RT_HIT_NORM( hitp, stp, &(ap->a_ray) );
		/*
		 * If the flip flag is on, it means that the outward
		 * pointing normal actually points the other way so we
		 * need to reverse it.  The application is responsible
		 * for doing this because it's just a reference to the
		 * actual hit point on the segment, and may be referred to
		 * in it's non-flipped sense in the next (or previous)
		 * partition.  It is this sharing which requires the
		 * application to make it's own copy of the hit, too.
		 */
		ihit = *hitp;	/* struct copy */
		if( pp->pt_inflip ) {
			VREVERSE( ihit.hit_normal, ihit.hit_normal );
		}
		rt_pr_hit( "  In", &ihit );
		/*
		 * This next macro fills in the curvature information
		 * which consists on a principle direction vector, and
		 * the inverse radii of curvature along that direction
		 * and perpendicular to it.  Positive curvature bends
		 * toward the outward pointing normal.
		 */
		RT_CURVATURE( &cur, &ihit, pp->pt_inflip, stp );
		VPRINT("PDir", cur.crv_pdir );
		rt_log(" c1=%g\n", cur.crv_c1);
		rt_log(" c2=%g\n", cur.crv_c2);

		/* outhit info */
		hitp = pp->pt_outhit;
		stp = pp->pt_outseg->seg_stp;
		RT_HIT_NORM( hitp, stp, &(ap->a_ray) );
		ohit = *hitp;		/* struct copy */
		if( pp->pt_outflip ) {
			VREVERSE( ohit.hit_normal, ohit.hit_normal );
		}
		rt_pr_hit( " Out", &ohit );
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
miss( ap )
register struct application *ap;
{
	rt_log("missed\n");
	return(0);
}
