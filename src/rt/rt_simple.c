/*                     R T _ S I M P L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file rt_simple.c
 *
 * a minimal raytracing application in BRLCAD.
 *
 * The primary BRL-CAD ray-tracing API consists of calls to the function
 * rt_shootray().  This function takes a single argument, the address of a
 * data structure called the "application" structure.  This data structure
 * contains crucial information, such as the origin and direction of the ray
 * to be traced, what to do if the ray hits geometry, or misses everything.
 * While the application struct is a large and somewhat complex looking one,
 * (it is defined in h/raytrace.h) there are really very few items in it
 * which the application programmer must know about. These are:
 *
 *	a_rt_i		The "rt instance" from a call to rt_dirbuild()
 *	a_ray		The ray origin and direction
 *	a_hit		A routine that is called when the ray overlaps geometry
 *	a_miss		A routine that is called when the ray misses everything
 *
 * Most of the work an application performs will be done in the "hit" routine.
 * This user-supplied routine gets called deep inside the raytracing library
 * under rt_shootray().  It is provided with 3 parameters:
 *
 *	ap		A pointer to the application structure passed to
 *				rt_shootray()
 *	PartHeadp	A list of ray "partitions" which overlap geometry
 *	segp		A list of ray "segments" which were used to create the
 *				partitions
 *
 * Most applications can ignore the last parameter.  The PartHeadp parameter
 * is a linked-list of "partition" structures (defined in "raytrace.h").
 * It is the job of the "hit" routine to process these ray/object intersections
 * to do the work of the application.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"


/* these will be defined later */

int hit(struct application *ap, struct partition *PartHeadp, struct seg *segp);
int miss(register struct application *ap);


/*	M A I N
 *
 *	This is where it all gets started
 */
int main(int argc, char **argv)
{
    /* the "ray tracing instance" structure contains definitions for librt
     * which are specific to the particular model being processed.  One
     * copy exists for each model.  Defined in "raytrace.h"
     */
    static struct rt_i *rtip;

    /* the "application" structure carries information about how the
     * ray-casting should be performed.  Defined in "raytrace.h"
     */
    struct application ap;

    /* by default, all values in application struct should be zeroed */
    RT_APPLICATION_INIT(&ap);

    /* Check for command-line arguments.  Make sure we have at least a
     * geometry file and one geometry object on the command line.
     */
    if (argc < 2) {
	fprintf(stderr,
		"Usage: %s geometry_file.g geom_object [geom_object...]\n",
		*argv);
	return -1;
    }


    /* First we need to build a directory (or table of contents) for
     * the objects in the database file.  That is the job of rt_dirbuild()
     */
    if( (rtip=rt_dirbuild(argv[1], (char *)NULL, 0)) == RTI_NULL ) {
	fprintf(stderr,"rt_dirbuild failure\n");
	return 2;
    }

    /* skip over the program name, and database name, leaving only the
     * list of objects in the database
     */
    argc -= 2;
    argv += 2;

    /* Add objects to the "active set".  These are the objects we are
     * interested in intersecting our ray with.
     */
    if( rt_gettrees_and_attrs( rtip, (const char **)NULL,
			       argc, (const char **)argv, 1 ) ) {
	fprintf(stderr,"rt_gettrees FAILED\n");
	return 1;
    }

    /* prepare objects for ray-tracing */
    rt_prep(rtip);

    /* set up the ray to be shot */
    VSET(ap.a_ray.r_pt, 0.0, 0.0, 0.0);
    VSET(ap.a_ray.r_dir, 0.0, 0.0, 1.0);

    /* register the hit/miss routines, and record the rtip */
    ap.a_hit = hit;
    ap.a_miss = miss;
    ap.a_rt_i = rtip;

    ap.a_purpose = "main ray";	/* a text string for informational purposes */

    /* shoot the ray */
    (void)rt_shootray( &ap );

    return(0);
}

/*	H I T
 *
 *	Routine that is called at the end of each ray which encounters at
 *	least one object
 */
int hit(struct application *ap,		/* application struct from main() */
	struct partition *PartHeadp, 	/* linked list of "partitions" */
	struct seg *segp)		/* segment list */
{
    /* partitions are spans of a ray which intersect material objects */
    struct partition *pp;
    point_t	pt;

    fprintf(stderr, "hit\n");

    for (pp=PartHeadp->pt_forw ; pp != PartHeadp ; pp = pp->pt_forw ) {

	/* construct the actual hit-point from the ray and the distance
	 * to the intersection point
	 */
	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);

	fprintf(stderr, "\"%s\" hit at %g %g %g\n",
		ap->a_purpose, /* this was the string we set in main() */
		V3ARGS(pt));
    }
    return 1;
}


/*	M I S S
 *
 *	Routine that is called if a ray does not intersect ANY geometry
 */
int miss(register struct application *ap)
{
    fprintf(stderr, "%s missed\n", ap->a_purpose);
    return 0;
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
