/*
 *		S O L I D S _ O N _ R A Y . C
 *
 *	Routines to implement the click-to-pick-an-edit-solid feature.
 *
 *  Functions -
 *	skewer_solids		fire a ray and list the solids hit
 *
 *  Author -
 *	Paul Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>

#include "tcl.h"
#include "tk.h"
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "rtstring.h"
#include "rtlist.h"
#include "redblack.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"

#define	ORDER_BY_NAME		 0
#define	ORDER_BY_DISTANCE	 1

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
static int sol_comp_name (void *v1, void *v2)
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
static int sol_comp_dist (void *v1, void *v2)
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
static struct sol_name_dist *mk_solid (name, dist)

char	*name;
fastf_t	dist;

{
    struct sol_name_dist	*sp;

    sp = (struct sol_name_dist *)
	    rt_malloc(sizeof(struct sol_name_dist), "solid");
    sp -> magic = SOL_NAME_DIST_MAGIC;
    sp -> name = name;
    sp -> dist = dist;
    rt_log("Created solid (%s, %g)\n", sp -> name, sp -> dist);
    return (sp);
}

/*
 *			F R E E _ S O L I D
 */
static void free_solid (sol)

struct sol_name_dist	*sol;

{
    RT_CKMAG(sol, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    rt_log("freeing solid (%s, %g)...\n", sol -> name, sol -> dist);
    rt_free((char *) sol, "solid");
}

/*
 *			P R I N T _ S O L I D
 */
static void print_solid (vp, depth)

void	*vp;
int	depth;

{
    struct sol_name_dist	*sol = vp;

    RT_CKMAG(sol, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");
    rt_log("solid %s at distance %g along ray\n", sol -> name, sol -> dist);
}

/*
 *			    N O _ O P
 *
 *	    Null event handler for use by rt_shootray().
 *
 *	Does nothing.  Returns 1.
 */
static int no_op (ap, ph)

struct application	*ap;
struct partition	*ph;

{
    return (1);
}

/*
 *			R P T _ S O L I D S
 *
 *		Hit handler for use by rt_shootray().
 *
 *	Grabs the first partition it sees, extracting thence
 *	the segment list.  Rpt_solids() sorts the solids along
 *	the ray by first encounter.  As a side-effect, rpt_solids()
 *	stores in ap->a_uptr the address of a null-terminated array
 *	of the sorted solid names.  It returns 1.
 */

static int rpt_solids (ap, ph)

struct application	*ap;
struct partition	*ph;

{
    char			**result;
    int				i;
    struct partition		*pp;
    rb_tree			*solids;
    struct seg			*sh;
    struct seg			*sp;
    struct sol_name_dist	*old_sol;
    struct sol_name_dist	*sol;
    static int			(*orders[])() =
				{
				    sol_comp_name,
				    sol_comp_dist
				};

    rt_log("I hit it!\n");
    /*
     *	Initialize the solid list
     */
    if ((solids = rb_create("Solid list", 2, orders)) == RB_TREE_NULL)
    {
	rt_log("%s: %d: rb_create() bombed\n", __FILE__, __LINE__);
	exit (1);
    }
    solids -> rbt_print = print_solid;
    rb_uniq_on(solids, ORDER_BY_NAME);

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
	rt_log("I saw solid %s at distance %g\n",
	    sp -> seg_stp -> st_name,
	    sp -> seg_in.hit_dist);
	
	sol = mk_solid(sp -> seg_stp -> st_name, sp -> seg_in.hit_dist);
	if (rb_insert(solids, (void *) sol) < 0)
	{
	    old_sol = (struct sol_name_dist *) rb_curr(solids, ORDER_BY_NAME);
	    RT_CKMAG(old_sol, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");
	    if (sol -> dist >= old_sol -> dist)
		free_solid(sol);
	    else
	    {
		rb_delete(solids, ORDER_BY_NAME);
		rb_insert(solids, sol);
		free_solid(old_sol);
	    }
	}
    }
    rt_log("\n- - - Solids along the ray - - -\n");
    rb_walk(solids, ORDER_BY_DISTANCE, print_solid, INORDER);

    result = (char **)
		rt_malloc((solids -> rbt_nm_nodes + 1) * sizeof(char *),
			  "names of solids on ray");
    for (sol = (struct sol_name_dist *) rb_min(solids, ORDER_BY_DISTANCE), i=0;
	 sol != NULL;
	 sol = (struct sol_name_dist *) rb_succ(solids, ORDER_BY_DISTANCE), ++i)
    {
	result[i] = sol -> name;
	rt_log("before free_solid(%x)... '%s'\n", sol, result[i]);
	free_solid(sol);
	rt_log("after free_solid(%x)... '%s'\n", sol, result[i]);
    }
    result[i] = 0;
    ap -> a_uptr = (char *) result;

    rb_free(solids, RB_RETAIN_DATA);
    return (1);
}

/*
 *			R P T _ M I S S
 *
 *		Miss handler for use by rt_shootray().
 *
 *	Stuffs the address of a null string in ap->a_uptr and returns 0.
 */

static int rpt_miss (ap, ph)

struct application	*ap;
struct partition	*ph;

{
    rt_log("I missed!\n");
    ap -> a_uptr = rt_malloc(sizeof(char *), "names of solids on ray");
    *((char **) (ap -> a_uptr)) = 0;

    return (0);
}

/*
 *		S K E W E R _ S O L I D S
 *
 *	Fire a ray at some geometry and obtain a list of
 *	the solids encountered, sorted by first intersection.
 *
 *	The function has four parameters: the model and objects
 *	at which to fire (in an argc/argv pair) and the origination
 *	point and direction for the ray.  So long as it could find
 *	the objects in the model, skewer_solids() returns a
 *	null-terminated array of solid names.  Otherwise, it returns 0.
 *
 *	N.B. - It is the caller's responsibility to free the array
 *	of solid names.
 */
char **skewer_solids (argc, argv, ray_orig, ray_dir)

int		argc;
CONST char	**argv;
point_t		ray_orig;
vect_t		ray_dir;

{
    struct application	ap;
    struct rt_i		*rtip;
    struct rt_list	sol_list;
    struct sol_name_dist	*sol;
	
    if ((rtip = rt_dirbuild(dbip -> dbi_filename, (char *) 0, 0)) == RTI_NULL)
    {
	rt_log("Cannot build directory for file '%s'\n",
	    dbip -> dbi_filename);
	return ((char **) 0);
    }
    rtip -> useair = 1;
    /*
     *	XXX	I've hardwired in here to use a single CPU.
     *		Should that be rt_avail_cpus()?
     */
    if (rt_gettrees(rtip, argc, argv, 1) == -1)
	return ((char **) 0);
    rt_prep(rtip);

    RT_LIST_INIT(&sol_list);

    /*
     *	Initialize the application
     */
    ap.a_hit = rpt_solids;
    ap.a_miss = rpt_miss;
    ap.a_resource = RESOURCE_NULL;
    ap.a_overlap = no_op;
    ap.a_onehit = 0;
    ap.a_uptr = (genptr_t) &sol_list;
    ap.a_rt_i = rtip;
    ap.a_zero1 = ap.a_zero2 = 0;
    ap.a_purpose = "skewer_solids()";
    VMOVE(ap.a_ray.r_pt, ray_orig);
    VMOVE(ap.a_ray.r_dir, ray_dir);

    (void) rt_shootray(&ap);

    return ((char **) ap.a_uptr);
}
