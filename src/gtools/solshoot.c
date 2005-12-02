/*                      S O L S H O O T . C
 * BRL-CAD
 *
 * Copyright (C) 1995-2005 United States Government as represented by
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
/** @file solshoot.c
 *
 *  Author -
 *	Paul J. Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "rtstring.h"
#include "rtlist.h"
#include "raytrace.h"
#include "../librt/debug.h"

#define	TITLE_LEN		80
#define	ORDER_BY_NAME		 0
#define	ORDER_BY_DISTANCE	 1
#define	made_it()		bu_log("Made it to %s:%d\n",	\
					__FILE__, __LINE__);

/*
 *		S O L _ N A M E _ D I S T
 *
 *	Little pair for storing the name and distance of a solid
 */
struct sol_name_dist
{
    long	magic;
    char	*name;
    fastf_t	dist;
};
#define	SOL_NAME_DIST_MAGIC	0x736c6e64

/*
 *		S O L _  C O M P _ N A M E
 *
 *	The function to order solids alphabetically by name
 */
int sol_comp_name (void *v1, void *v2)
{
    struct sol_name_dist	*s1 = v1;
    struct sol_name_dist	*s2 = v2;

    RT_CKMAG(s1, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");
    RT_CKMAG(s2, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    return(strcmp(s1 -> name, s2 -> name));
}

/*
 *		S O L _  C O M P _ D I S T
 *
 *	The function to order solids by distance along the ray
 */
int sol_comp_dist (void *v1, void *v2)
{
    struct sol_name_dist	*s1 = v1;
    struct sol_name_dist	*s2 = v2;

    RT_CKMAG(s1, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");
    RT_CKMAG(s2, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    if (s1 -> dist > s2 -> dist)
	return (1);
    else if (s1 -> dist == s2 -> dist)
	return (0);
    else /* (s1 -> dist < s2 -> dist) */
	return (-1);
}

/*
 *			M K _ S O L I D
 */
struct sol_name_dist *mk_solid (char *name, fastf_t dist)
{
    struct sol_name_dist	*sp;

    sp = (struct sol_name_dist *)
	    bu_malloc(sizeof(struct sol_name_dist), "solid name-and_dist");
    sp -> magic = SOL_NAME_DIST_MAGIC;
    sp -> name = name;
    sp -> dist = dist;
    bu_log("Created solid (%s, %g)\n", sp -> name, sp -> dist);
    return (sp);
}

/*
 *			F R E E _ S O L I D
 */
void free_solid (char *vp)
{
    struct sol_name_dist	*sol = (struct sol_name_dist *) vp;

    RT_CKMAG(sol, SOL_NAME_DIST_MAGIC, "solid name-and-dist");

    bu_log("freeing solid (%s, %g)...\n", sol -> name, sol -> dist);
    bu_free((char *) sol, "solid name-and-dist");
}

/*
 *			P R I N T _ S O L I D
 */
void print_solid (void *vp, int depth)
{
    struct sol_name_dist	*sol = vp;

    RT_CKMAG(sol, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");
    bu_log("solid %s at distance %g along ray\n", sol -> name, sol -> dist);
}

/*			R P T _ H I T
 *
 *	Hit handler for use by rt_shootray().
 *
 *	Does nothing.  Returns 1.
 */
static int rpt_hit (struct application *ap, struct partition *ph, struct seg *segs)
{
    struct partition		*pp;
    struct seg			*sh;
    struct seg			*sp;
    bu_rb_tree			*solids;
    struct sol_name_dist	*old_sol;
    struct sol_name_dist	*sol;
    static int			(*orders[])() =
				{
				    sol_comp_name,
				    sol_comp_dist
				};

    bu_log("I hit it!\n");
    /*
     *	Initialize the solid list
     */
    if ((solids = bu_rb_create("Solid list", 2, orders)) == RB_TREE_NULL)
    {
	bu_log("%s: %d: bu_rb_create() bombed\n", __FILE__, __LINE__);
	exit (1);
    }
    solids -> rbt_print = print_solid;
    bu_rb_uniq_on(solids, ORDER_BY_NAME);

    /*
     *	Get the list of segments along this ray
     *	and seek to its head
     */
    RT_CKMAG(ph, PT_HD_MAGIC, "partition head");
    pp = ph -> pt_forw;
    RT_CKMAG(pp, PT_MAGIC, "partition structure");
    for (sh = pp -> pt_inseg;
	    *((long *) sh) != RT_LIST_HEAD_MAGIC;
	    sh = (struct seg *) (sh -> l.forw))
	RT_CKMAG(sh, RT_SEG_MAGIC, "segment structure");

    /*
     *	March down the list of segments
     */
    for (sp = (struct seg *) (sh -> l.forw);
	    sp != sh;
	    sp = (struct seg *) sp -> l.forw)
    {
	RT_CKMAG(sp, RT_SEG_MAGIC, "seg structure");
	bu_log("I saw solid %s at distance %g\n",
	    sp -> seg_stp -> st_name,
	    sp -> seg_in.hit_dist);

	sol = mk_solid(sp -> seg_stp -> st_name, sp -> seg_in.hit_dist);
	if (bu_rb_insert(solids, (void *) sol) < 0)
	{
	    old_sol = (struct sol_name_dist *) bu_rb_curr(solids, ORDER_BY_NAME);
	    RT_CKMAG(old_sol, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");
	    if (sol -> dist >= old_sol -> dist)
		free_solid((char *) sol);
	    else
	    {
		bu_rb_delete(solids, ORDER_BY_NAME);
		bu_rb_insert(solids, sol);
		free_solid((char *) old_sol);
	    }
	}
    }
    bu_log("\n- - - Solids along the ray - - -\n");
    bu_rb_walk(solids, ORDER_BY_DISTANCE, print_solid, INORDER);

    bu_prmem("Before bu_rb_free()...");
    bu_rb_diagnose_tree(solids, ORDER_BY_NAME, INORDER);
#if 0
    bu_rb_free(solids, RB_RETAIN_DATA);
#else
    bu_rb_free(solids, free_solid);
#endif
    bu_prmem("After bu_rb_free()...");

    return (1);
}

/*			N O _ O P
 *
 *	Null event handler for use by rt_shootray().
 *
 *	Does nothing.  Returns 1.
 */
static int no_op (struct application *ap)
{
    return (1);
}

main (int argc, char **argv)
{
    struct application	ap;
    char		db_title[TITLE_LEN+1];
    struct rt_i		*rtip;

    if (--argc < 2)
    {
	bu_log("Usage: 'solshoot model.g obj [obj...]'\n");
	exit (1);
    }

    /* Read in the geometry model */
    bu_log("Database file:  '%s'\n", *++argv);
    bu_log("Building the directory... ");
    if ((rtip = rt_dirbuild(*argv , db_title, TITLE_LEN)) == RTI_NULL)
    {
	bu_log("Could not build directory for file '%s'\n", *argv);
	exit(1);
    }
    rtip -> useair = 1;
    bu_log("\nPreprocessing the geometry... ");
    while (--argc > 0)
    {
	if (rt_gettree(rtip, *++argv) == -1)
	    exit (1);
	bu_log("\nObject '%s' processed", *argv);
    }
    bu_log("\nPrepping the geometry... ");
    rt_prep(rtip);
    bu_log("\n");

    rt_g.debug = DEBUG_MEM_FULL;

    /* Initialize the application structure */
    RT_APPLICATION_INIT(&ap);
    ap.a_hit = rpt_hit;
    ap.a_miss = no_op;
    ap.a_resource = RESOURCE_NULL;
    ap.a_overlap = no_op;
    ap.a_onehit = 0;		/* Don't stop at first partition */
    ap.a_rt_i = rtip;
    ap.a_zero1 = 0;		/* Sanity checks for LIBRT(3) */
    ap.a_zero2 = 0;
    ap.a_purpose = "Determine which segments get reported";

#if 1
    VSET(ap.a_ray.r_pt, 7.0, 7.0, 0.0);
    VSET(ap.a_ray.r_dir, -0.7071067812, -0.7071067812, 0.0);
#else
    VSET(ap.a_ray.r_pt, 20.0, 0.0, 0.0);
    VSET(ap.a_ray.r_dir, -1.0, 0.0, 0.0);
#endif
    (void) rt_shootray(&ap);
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
