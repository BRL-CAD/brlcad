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
#include "bu.h"
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
static int
sol_comp_name(v1, v2)
void *v1, *v2;    
{
    struct sol_name_dist	*s1 = v1;
    struct sol_name_dist	*s2 = v2;

    BU_CKMAG(s1, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");
    BU_CKMAG(s2, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    return(strcmp(s1 -> name, s2 -> name));
}

/*
 *		S O L _  C O M P _ D I S T
 *
 *	The function to order solids by distance along the ray
 */
static int
sol_comp_dist(v1, v2)
void *v1, *v2;    
{
    struct sol_name_dist	*s1 = v1;
    struct sol_name_dist	*s2 = v2;

    BU_CKMAG(s1, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");
    BU_CKMAG(s2, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

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
static struct sol_name_dist *
mk_solid(name, dist)
char	*name;
fastf_t	dist;
{
    struct sol_name_dist	*sp;

    sp = (struct sol_name_dist *)
	    bu_malloc(sizeof(struct sol_name_dist), "solid");
    sp -> magic = SOL_NAME_DIST_MAGIC;
    sp -> name = (char *)
	    bu_malloc(strlen(name) + 1, "solid name");
    (void) strcpy(sp -> name, name);
    sp -> dist = dist;
    return (sp);
}

/*
 *			F R E E _ S O L I D
 *
 *	This function has two parameters: the solid to free and
 *	an indication whether the name member of the solid should
 *	also be freed.
 */
static void
free_solid(sol, free_name)
struct sol_name_dist	*sol;
int			free_name;

{
    BU_CKMAG(sol, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    if (free_name)
	bu_free((char *) sol -> name, "solid name");
    bu_free((char *) sol, "solid");
}

/*
 *			P R I N T _ S O L I D
 */
static void
print_solid(vp, depth)
void	*vp;
int	depth;
{
    struct sol_name_dist	*sol = vp;
    struct bu_vls tmp_vls;

    BU_CKMAG(sol, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "solid %s at distance %g along ray\n",
		  sol -> name, sol -> dist);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
}

/*
 *			    N O _ O P
 *
 *	    Null event handler for use by rt_shootray().
 *
 *	Does nothing.  Returns 1.
 */
static int
no_op(ap, ph)
struct application	*ap;
struct partition	*ph;
{
    return (1);
}

/*
 *	    B U I L D _ P A T H _ N A M E _ O F _ S O L I D ( )
 *
 *	Builds the slash-separated path name for a struct solid.
 */
void
build_path_name_of_solid(vp, sp)
struct bu_vls	*vp;
struct solid	*sp;
{
    int		i;

    bu_vls_trunc(vp, 0);
    for (i = 0; i < sp -> s_last; ++i)
    {
	bu_vls_strcat(vp, sp -> s_path[i] -> d_namep);
	bu_vls_strcat(vp, "/");
    }
    bu_vls_strcat(vp, sp -> s_path[sp -> s_last] -> d_namep);
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
 *	of the sorted solid names.  If ap->a_user is nonzero,
 *	rpt_solids() stashes the complete path name for each solid,
 *	otherwise, just its basename.  It returns 1.
 */

static int
rpt_solids(ap, ph)
struct application	*ap;
struct partition	*ph;
{
    char			**result;
    struct db_full_path		*fp;
    int				i;
    int				full_path; /* Get full path, not base? */
    struct partition		*pp;
    rb_tree			*solids;
    struct seg			*segh;
    struct seg			*segp;
    struct solid		*sp;
    struct sol_name_dist	*old_sol;
    struct sol_name_dist	*sol;
    struct bu_vls		sol_path_name;
    static int			(*orders[])() =
				{
				    sol_comp_name,
				    sol_comp_dist
				};

    full_path = ap -> a_user;

    /*
     *	Initialize the solid list
     */
    if ((solids = rb_create("Solid list", 2, orders)) == RB_TREE_NULL)
    {
	bu_log("%s: %d: rb_create() bombed\n", __FILE__, __LINE__);
	exit (1);
    }
    solids -> rbt_print = print_solid;
    rb_uniq_on(solids, ORDER_BY_NAME);
    printf("HELLO %s:%d\n", __FILE__, __LINE__);

    if (1)
    {
	printf("HELLO %s:%d\n", __FILE__, __LINE__);
	bu_vls_init(&sol_path_name);

	/*
	 *	Get the list of segments along this ray
	 *	and seek to its head
	 */
	BU_CKMAG(ph, PT_HD_MAGIC, "partition head");
	pp = ph -> pt_forw;
	BU_CKMAG(pp, PT_MAGIC, "partition structure");
	for (segh = pp -> pt_inseg;
		*((long *) segh) != RT_LIST_HEAD_MAGIC;
		segh = (struct seg *) (segh -> l.forw))
	    BU_CKMAG(segh, RT_SEG_MAGIC, "segment structure");

	/*
	 *	March down the segment list
	 */
	for (segp = (struct seg *) (segh -> l.forw);
		segp != segh;
		segp = (struct seg *) segp -> l.forw)
	{
	    BU_CKMAG(segp, RT_SEG_MAGIC, "seg structure");
	    
	    bu_vls_trunc(&sol_path_name, 0);
	    fp = &(segp -> seg_stp -> st_path);
	    printf("At line %d, sol_path_name contains '%s'\n",
		    __LINE__, bu_vls_addr(&sol_path_name));
	    if (fp -> magic)
		bu_vls_strcpy(&sol_path_name, db_path_to_string(fp));
	    printf("At line %d, sol_path_name contains '%s'\n",
		    __LINE__, bu_vls_addr(&sol_path_name));
	    bu_vls_strcat(&sol_path_name, segp -> seg_stp -> st_name);
	    printf("At line %d, sol_path_name contains '%s'\n",
		    __LINE__, bu_vls_addr(&sol_path_name));
	    sol = mk_solid(bu_vls_addr(&sol_path_name),
			segp -> seg_in.hit_dist);
	    printf("and segp -> seg_stp = %x\n", segp -> seg_stp);
	    /*
	     *	Attempt to record the new solid.
	     *	If it shares its name with a previously recorded solid,
	     *	then retain the one that appears earlier on the ray.
	     */
	    if (rb_insert(solids, (void *) sol) < 0)
	    {
		old_sol = (struct sol_name_dist *)
			    rb_curr(solids, ORDER_BY_NAME);
		BU_CKMAG(old_sol, SOL_NAME_DIST_MAGIC,
		    "sol_name_dist structure");
		if (sol -> dist >= old_sol -> dist)
		    free_solid(sol, 1);
		else
		{
		    rb_delete(solids, ORDER_BY_NAME);
		    rb_insert(solids, sol);
		    free_solid(old_sol, 1);
		}
	    }
	}
    }
    else
    {
	printf("HELLO %s:%d\n", __FILE__, __LINE__);
	bu_vls_init(&sol_path_name);
	bu_log("Path names for all solids...\n");
	FOR_ALL_SOLIDS(sp)
	{
	    build_path_name_of_solid (&sol_path_name, sp);
	    printf("%s\n", bu_vls_addr(&sol_path_name));
	}
	bu_log("...the end\n");

	/*
	 *	Get the list of segments along this ray
	 *	and seek to its head
	 */
	BU_CKMAG(ph, PT_HD_MAGIC, "partition head");
	pp = ph -> pt_forw;
	BU_CKMAG(pp, PT_MAGIC, "partition structure");
	for (segh = pp -> pt_inseg;
		*((long *) segh) != RT_LIST_HEAD_MAGIC;
		segh = (struct seg *) (segh -> l.forw))
	    BU_CKMAG(segh, RT_SEG_MAGIC, "segment structure");

	/*
	 *	March down the segment list
	 */
	for (segp = (struct seg *) (segh -> l.forw);
		segp != segh;
		segp = (struct seg *) segp -> l.forw)
	{
	    BU_CKMAG(segp, RT_SEG_MAGIC, "seg structure");
	    
	    sol = mk_solid(segp -> seg_stp -> st_name, segp -> seg_in.hit_dist);
	    if (rb_insert(solids, (void *) sol) < 0)
	    {
		old_sol = (struct sol_name_dist *)
			    rb_curr(solids, ORDER_BY_NAME);
		BU_CKMAG(old_sol, SOL_NAME_DIST_MAGIC,
		    "sol_name_dist structure");
		if (sol -> dist >= old_sol -> dist)
		    free_solid(sol, 1);
		else
		{
		    rb_delete(solids, ORDER_BY_NAME);
		    rb_insert(solids, sol);
		    free_solid(old_sol, 1);
		}
	    }
	}
    }
    printf("HELLO %s:%d\n", __FILE__, __LINE__);

    result = (char **)
		bu_malloc((solids -> rbt_nm_nodes + 1) * sizeof(char *),
			  "names of solids on ray");
    for (sol = (struct sol_name_dist *) rb_min(solids, ORDER_BY_DISTANCE),
		i=0;
	 sol != NULL;
	 sol = (struct sol_name_dist *) rb_succ(solids, ORDER_BY_DISTANCE),
		++i)
    {
	result[i] = sol -> name;
	free_solid(sol, 0);
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

static int
rpt_miss(ap, ph)
struct application	*ap;
struct partition	*ph;
{
    ap -> a_uptr = bu_malloc(sizeof(char *), "names of solids on ray");
    *((char **) (ap -> a_uptr)) = 0;

    return (0);
}

/*
 *		    S K E W E R _ S O L I D S
 *
 *	Fire a ray at some geometry and obtain a list of
 *	the solids encountered, sorted by first intersection.
 *
 *	The function has five parameters: the model and objects at which
 *	to fire (in an argc/argv pair) the origination point and direction
 *	for the ray, and a result-format specifier.  So long as it could
 *	find the objects in the model, skewer_solids() returns a null-
 *	terminated array of solid names.  Otherwise, it returns 0.  If
 *	full_path is nonzero, then the entire path for each solid is
 *	recorded.  Otherwise, only the basename is recorded.
 *
 *	N.B. - It is the caller's responsibility to free the array
 *	of solid names.
 */
char **skewer_solids (argc, argv, ray_orig, ray_dir, full_path)

int		argc;
CONST char	**argv;
point_t		ray_orig;
vect_t		ray_dir;
int		full_path;

{
    struct application	ap;
    struct rt_i		*rtip;
    struct rt_list	sol_list;
    struct sol_name_dist	*sol;

    if (argc <= 0) {
	char **ptr;
	ptr = (char **)bu_malloc(sizeof(char *), "empty solid list");
	*ptr = 0;
	return ptr;
    }
	
    if ((rtip = rt_dirbuild(dbip -> dbi_filename, (char *) 0, 0)) == RTI_NULL)
    {
      Tcl_AppendResult(interp, "Cannot build directory for file '",
		       dbip -> dbi_filename, "'\n", (char *)NULL);
      return ((char **) 0);
    }
    rtip -> useair = 1;
    /*
     *	XXX	I've hardwired in here to use a single CPU.
     *		Should that be bu_avail_cpus()?
     */
    if (rt_gettrees(rtip, argc, argv, 1) == -1) {
	return ((char **) 0);
    }
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
    ap.a_user = 1;	/* Requests full paths to solids, not just basenames */
    ap.a_rt_i = rtip;
    ap.a_zero1 = ap.a_zero2 = 0;
    ap.a_purpose = "skewer_solids()";
    VMOVE(ap.a_ray.r_pt, ray_orig);
    VMOVE(ap.a_ray.r_dir, ray_dir);

    (void) rt_shootray(&ap);

    return ((char **) ap.a_uptr);
}
