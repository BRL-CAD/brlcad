/*                     S H O T L I N E S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file rt/reshoot.c
 *
 * A program to shoot reshoot and compare results to previous runs.
 *
 * This program is geared towards performing regression tests of the
 * BRL-CAD libraries.  A candidate application is run with
 * the RT_G_DEBUG flag DEBUG_ALLHITS set.  This causes the application
 * to log all calls to the application hit() routine, and print the
 * contents of the struct partition.  The log is processed to
 * eliminate extraneous information to produce the input to this
 * program.  The following awk program, will suffice:  @verbatim

 /Pnt/ { START=index($0, "(") + 1
 STR=substr($0, START, index($0, ")") - START)
 gsub(  ", ", ",", STR)
 printf "Pnt=%s\n", STR

 }


 /Dir/ { START=index($0, "(") + 1
 STR=substr($0, START, index($0, ")") - START)
 gsub(  ", ", ",", STR)
 printf "Dir=%s\n", STR
 }
 /PT/  { PARTIN=$3
 PARTOUT=$5
 }
 /InHIT/ { INHIT=$2 }
 /OutHIT/ { OUTHIT=$2 }
 /Region/ { printf "\tregion=%s in=%s in%s out=%s out%s\n", $2, PARTIN, INHIT, PARTOUT, OUTHIT}

 @endverbatim
 * If this awk program is stored in the file p.awk then: @verbatim
 awk -f p.awk < logfile > inputfile
 @endverbatim
 * will produce a suitable input file for this program.  The form is as
 * follows: @verbatim
 Pnt=1, 2, 3
 Dir=4, 5, 6
 region=/all.g/platform.r in=platform.s indist=10016.8 out=platform.s outdist=10023.8
 @endverbatim
 * where the line begining with "region" may be repeated any number of times, representing each
 * region encountered along the ray.
 * now run this program as follows: @verbatim
 reshoot geom.g obj [obj...] < inputfile
 @endverbatim
 * and this  will re-shoot all of the rays that the original program
 * shot, and compare the results.
 *
 * One of the basic assumptions is that the application structure
 * field a_onehit is set to zero in the original application, causing
 * all rays to be shot all the way through the geometry
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

#include "vmath.h"		/* vector math macros */
#include "bu.h"
#include "raytrace.h"		/* librt interface definitions */

#include "./rtuif.h"


char *progname = "(noname)";

/**
 * @struct shot
 * A description of a single shotline, and all the regions that were
 * hit by the shotline.
 */
struct shot {
    point_t pt;
    vect_t dir;
    struct bu_list regions;
};

/**
 * @struct shot_sp
 * The parse table for a struct shot
 */
static const struct bu_structparse shot_sp[] = {
    { "%f", 3, "Pnt", bu_offsetofarray(struct shot, pt), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    { "%f", 3, "Dir", bu_offsetofarray(struct shot, dir), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

/**
 * @struct reg_hit
 *
 * This describes the interaction of the ray with a single region
 */
struct reg_hit {
    struct bu_list l; /* linked list membership */
    struct bu_vls regname; /* name of the region */
    struct bu_vls in_primitive; /* name of the primitive for the inbound hit */
    double indist; /* distance along ray to the inbound hit */
    struct bu_vls out_primitive; /* name of the primitive for the outbound hit */
    double outdist; /* distance along ray to the outbound hit */
};

static const struct bu_structparse reg_sp[] = {
    {"%V", 1, "region", bu_offsetof(struct reg_hit, regname), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%V", 1, "in", bu_offsetof(struct reg_hit, in_primitive), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%V", 1, "out", bu_offsetof(struct reg_hit, out_primitive), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 1, "indist", bu_offsetof(struct reg_hit, indist), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 1, "outdist", bu_offsetof(struct reg_hit, outdist), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/**
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void
usage(char *s)
{
    if (s) (void)fputs(s, stderr);
    bu_exit(1, "Usage: %s geom.g obj [obj...] < rayfile \n", progname);
}


/**
 *  Process a single ray intersection list.
 *  A pointer to this function is in the application structure a_hit field.
 *  rt_shootray() calls this if geometry is hit by the ray.  It passes the
 *  application structure which describes the state of the world
 *  (see raytrace.h), and a circular linked list of partitions,
 *  each one describing one in and out segment of one region.
 * @return
 *	integer value, typically 1.  This value becomes the return value to rtshootray()
 */
int
hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    /* see raytrace.h for all of these guys */
    register struct partition *pp;
    struct shot *sh = (struct shot *)ap->a_uptr;
    int status = 0;
    struct reg_hit *rh;
    struct bu_vls v;
    struct bu_vls result;
    struct valstruct {
	double val;
    } vs;
    static struct bu_structparse val_sp[] = {
	{"%f", 1, "val", bu_offsetof(struct valstruct, val), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    };

    bu_vls_init(&v);
    bu_vls_init(&result);

    /* examine each partition until we get back to the head */
    rh = BU_LIST_FIRST(reg_hit, &sh->regions);
    for ( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {

	bu_vls_trunc(&result, 0);

	/* since the values were printed out using a %g format,
	 * we have to do the same thing to the result to compare them
	 */
	bu_vls_trunc(&v, 0);
	bu_vls_printf(&v, "val=%g", pp->pt_inhit->hit_dist);
	bu_struct_parse(&v, val_sp, (const char *)&vs);

	if (!EQUAL(vs.val, rh->indist)) {
	    bu_vls_printf(&result, "\tinhit mismatch %g %g\n", pp->pt_inhit->hit_dist, rh->indist);
	    status = 1;
	}

	bu_vls_trunc(&v, 0);
	bu_vls_printf(&v, "val=%g", pp->pt_outhit->hit_dist);
	bu_struct_parse(&v, val_sp, (const char *)&vs);


	if (!EQUAL(vs.val, rh->outdist)) {
	    bu_vls_printf(&result, "\touthit mismatch %g %g\n", pp->pt_outhit->hit_dist, rh->outdist);
	    status = 1;
	}

	/* check the region name */
	if (!BU_STR_EQUAL( pp->pt_regionp->reg_name, bu_vls_addr(&rh->regname) )) {
	    /* region names don't match */
	    bu_vls_printf(&result, "\tregion name mismatch %s %s\n", pp->pt_regionp->reg_name, bu_vls_addr(&rh->regname) );
	    status = 1;
	}

	if ( !BU_STR_EQUAL(pp->pt_inseg->seg_stp->st_dp->d_namep, bu_vls_addr(&rh->in_primitive))) {
	    bu_vls_printf(&result, "\tin primitive name mismatch %s %s\n", pp->pt_inseg->seg_stp->st_dp->d_namep, bu_vls_addr(&rh->in_primitive));
	    status = 1;
	}
	if ( !BU_STR_EQUAL(pp->pt_outseg->seg_stp->st_dp->d_namep, bu_vls_addr(&rh->out_primitive))) {
	    bu_vls_printf(&result, "\tout primitive name mismatch %s %s\n", pp->pt_outseg->seg_stp->st_dp->d_namep, bu_vls_addr(&rh->out_primitive));
	    status = 1;
	}
	if (bu_vls_strlen(&result) > 0) {
	    bu_log("Ray Pt %g,%g,%g Dir %g,%g,%g\n%V",
		   V3ARGS(sh->pt),
		   V3ARGS(sh->dir),
		   &result);
	}

	rh = BU_LIST_NEXT(reg_hit, &rh->l);
    }

    /*
     * This value is returned by rt_shootray
     * a hit usually returns 1, miss 0.
     */
    return status;
}


/**
 * Function called when ray misses all geometry
 * A pointer to this function is stored in the application structure
 * field a_miss.  rt_shootray() will call this when the ray misses all geometry.
 * it passees the application structure.
 * @return
 *	Typically 0, and becomes the return value from rt_shootray()
 */
int
miss(register struct application *UNUSED(ap))
{
    return 0;
}

/**
 * Print a shot.  Mostly a debugging routine.
 *
 */
void
pr_shot(struct shot *sh)
{
    struct reg_hit *rh;
    /* shoot the old ray */
    bu_struct_print("shooting", shot_sp, (const char *)sh);
    for (BU_LIST_FOR(rh, reg_hit, &sh->regions)) {
	bu_struct_print("", reg_sp, (const char *)rh);
    }
}

/**
 *	Re-shoot a ray
 *	The ray described by the parametry sh
 *
 *	@param sh  a pointer to a struct shot describing the ray to shoot and expected results
 *	@param ap  a pointer to a struct application
 *	@return
 *	integer value indicating if there was a difference
 */
int
do_shot(struct shot *sh, struct application *ap)
{
    struct reg_hit *rh;
    int status;


    VMOVE(ap->a_ray.r_pt, sh->pt);
    VMOVE(ap->a_ray.r_dir, sh->dir);
    ap->a_uptr = (genptr_t)sh;

    ap->a_hit = hit;
    ap->a_miss = miss;
    status = rt_shootray( ap );	/* do it */

    /* clean up */
    while (BU_LIST_WHILE(rh, reg_hit, &sh->regions)) {
	BU_LIST_DEQUEUE( &(rh->l) );
	bu_vls_free(&rh->regname);
	bu_vls_free(&rh->in_primitive);
	bu_vls_free(&rh->out_primitive);
	bu_free(rh, "");
    }
    return status;
}


/**
 *	Load the database, parse the input, and shoot the rays
 * @return
 * integer flag value indicating whether any differences were detected.  0 = none, !0 = different
 */
int
main(int argc, char **argv)
{
    /* every application needs one of these */
    struct application	ap;
    static struct rt_i *rtip;	/* rt_dirbuild returns this */
    char idbuf[2048] = {0};	/* First ID record info */

    int status = 0;
    struct bu_vls buf;
    struct shot sh;


    progname = argv[0];

    if (argc < 3) {
	usage("insufficient args\n");
    }


    /*
     *  Load database.
     *  rt_dirbuild() returns an "instance" pointer which describes
     *  the database to be ray traced.  It also gives you back the
     *  title string in the header (ID) record.
     */
    if ( (rtip=rt_dirbuild(argv[1], idbuf, sizeof(idbuf))) == RTI_NULL ) {
	bu_exit(2, "rtexample: rt_dirbuild failure\n");
    }

    /* intialize the application structure to all zeros */
    RT_APPLICATION_INIT(&ap);

    ap.a_rt_i = rtip;	/* your application uses this instance */

    /* Walk trees.
     * Here you identify any object trees in the database that you
     * want included in the ray trace.
     */
    while (argc > 2) {
	if ( rt_gettree(rtip, argv[2]) < 0 )
	    fprintf(stderr, "rt_gettree(%s) FAILED\n", argv[0]);
	argc--;
	argv++;
    }
    /*
     * This next call gets the database ready for ray tracing.
     * (it precomputes some values, sets up space partitioning, etc.)
     */
    rt_prep_parallel(rtip, 1);


    bu_vls_init(&buf);


    memset((void *)&sh, 0, sizeof(sh));
    BU_LIST_INIT(&sh.regions);

    while (bu_vls_gets(&buf, stdin) >= 0) {
	char *p = bu_vls_addr(&buf);

	switch (*p) {
	    case 'P' :
	    {

		if (BU_LIST_NON_EMPTY(&sh.regions)) {
		    status |= do_shot(&sh, &ap);
		}

		if (bu_struct_parse(&buf, shot_sp, (const char *)&sh)) {
		    bu_exit(EXIT_FAILURE, "error parsing pt");
		}

		break;
	    }
	    case 'D' :
	    {
		if (bu_struct_parse(&buf, shot_sp, (const char *)&sh)) {
		    bu_exit(EXIT_FAILURE, "error parsing dir");
		}
		break;
	    }

	    default:
	    {
		struct reg_hit *rh = bu_calloc(1, sizeof (struct reg_hit), "");


		if (bu_struct_parse(&buf, reg_sp, (const char *)rh)) {
		    bu_log("Error parsing region %s\nSkipping to next line\n",
			   bu_vls_addr(&buf));
		}
		BU_LIST_APPEND(&sh.regions, &rh->l);

		break;
	    }
	}
	bu_vls_trunc(&buf, 0);
    }
    status |= do_shot(&sh, &ap);


    return status;
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
