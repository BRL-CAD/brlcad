/*                 S O L I D S _ O N _ R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2006 United States Government as represented by
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
/** @file solids_on_ray.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include <signal.h>

#include "tcl.h"
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "redblack.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

#define	ORDER_BY_NAME		 0
#define	ORDER_BY_DISTANCE	 1

#define	made_it()		printf("Made it to %s%d\n", \
					__FILE__, __LINE__); \
				fflush(stdout)

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

#if OLD_RPT
/*
 *		S O L _  C O M P _ N A M E
 *
 *	The function to order solids alphabetically by name
 */
static int
sol_comp_name(void *v1, void *v2)
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
sol_comp_dist(void *v1, void *v2)
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
mk_solid(char *name, fastf_t dist)
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
free_solid(struct sol_name_dist *sol, int free_name)
{
    BU_CKMAG(sol, SOL_NAME_DIST_MAGIC, "sol_name_dist structure");

    if (free_name)
	bu_free((genptr_t) sol -> name, "solid name");
    bu_free((genptr_t) sol, "solid");
}

/*
 *			P R I N T _ S O L I D
 */
static void
print_solid(void *vp, int depth)
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
#endif /* OLD_RPT */


/*
 *			    N O _ O P
 *
 *	    Null event handler for use by rt_shootray().
 *
 *	Does nothing.  Returns 1.
 */
static int
no_op(struct application *ap, struct partition *ph)
{
    return (1);
}

/*
 *	    B U I L D _ P A T H _ N A M E _ O F _ S O L I D ( )
 *
 *	Builds the slash-separated path name for a struct solid.
 */
void
build_path_name_of_solid(struct bu_vls *vp, struct solid *sp)
{
    bu_vls_trunc(vp, 0);
    db_path_to_vls(vp, &sp->s_fullpath);
}

#if OLD_RPT
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
/* If this is defined inside the body of rpt_solids(), it causes
 * the IRIX 6 compilers to segmentation fault in WHIRL file phase. Ugh. */
static int			(*rpt_solids_orders[])() =
				{
				    sol_comp_name,
				    sol_comp_dist
				};

static int
rpt_solids(struct application *ap, struct partition *ph, struct seg *finished_segs)
{
    char			**result;
    struct db_full_path		*fp;
    int				i;
    struct partition		*pp;
    bu_rb_tree			*solids;
    struct seg			*segp;
    struct sol_name_dist	*old_sol;
    struct sol_name_dist	*sol;
    struct soltab		*stp;
    struct bu_vls		sol_path_name;
    int				index;

    /*
     *	Initialize the solid list
     */
    if ((solids = bu_rb_create("Primitive list", 2, rpt_solids_orders)) == BU_RB_TREE_NULL)
    {
	bu_log("%s: %d: bu_rb_create() bombed\n", __FILE__, __LINE__);
	exit (1);
    }
    solids -> rbt_print = print_solid;
    bu_rb_uniq_on(solids, ORDER_BY_NAME);

    bu_vls_init(&sol_path_name);

    /*
     *	Get the list of segments along this ray
     *	and seek to its head
     */
    BU_CKMAG(ph, PT_HD_MAGIC, "partition head");
    pp = ph -> pt_forw;
    BU_CKMAG(pp, PT_MAGIC, "partition structure");
    if (BU_LIST_MAGIC_WRONG((struct bu_list *) finished_segs,
			    RT_SEG_MAGIC))
	BU_CKMAG(finished_segs, BU_LIST_HEAD_MAGIC, "list head");

    /*
     *	New stuff
     */

    RT_CK_LIST_HEAD(&finished_segs->l);

    for (RT_LIST_FOR(pp, partition, (struct bu_list *) &ph -> pt_magic))
    {
	BU_CKMAG(pp, PT_MAGIC, "partition");
	BU_CKMAG(pp -> pt_regionp, RT_REGION_MAGIC, "region");
	printf("    Partition <x%lx> is '%s' ",
	    (long)pp, pp -> pt_regionp -> reg_name);

	printf("\n--- Primitives hit on this partition ---\n");
	for (i = 0; i < (pp -> pt_seglist).end; ++i)
	{
	    stp = ((struct seg *)BU_PTBL_GET(&pp->pt_seglist, i))->seg_stp;
	    RT_CK_SOLTAB(stp);
	    bu_vls_trunc(&sol_path_name, 0);
	    fp = &(stp -> st_path);
	    if (fp -> magic != 0)
	    {
		printf(" full path... ");fflush(stdout);
		RT_CK_FULL_PATH(fp);
		bu_vls_strcpy(&sol_path_name, db_path_to_string(fp));
	    }
	    else
	    {
		printf(" dir-entry name... ");fflush(stdout);
		BU_CKMAG(stp -> st_dp, RT_DIR_MAGIC,
		    "directory");
		bu_vls_strcpy(&sol_path_name, stp -> st_name);
	    }
	    printf("'%s'\n", bu_vls_addr(&sol_path_name));fflush(stdout);
	}
	printf("------------------------------------\n");

    /*
     *	Look at each segment that participated in this ray partition.
     */
    for( index = 0; index < BU_PTBL_END(&pp->pt_seglist); index++ )  {
        segp = (struct seg *)BU_PTBL_GET(&pp->pt_seglist, index);
	RT_CK_SEG(segp);
	RT_CK_SOLTAB(segp -> seg_stp);

	printf("Primitive #%d in this partition is ", index);fflush(stdout);
	bu_vls_trunc(&sol_path_name, 0);
	fp = &(segp -> seg_stp -> st_path);
	if (fp -> magic != 0)
	{
		printf(" full path... ");fflush(stdout);
		RT_CK_FULL_PATH(fp);
		bu_vls_strcpy(&sol_path_name, db_path_to_string(fp));
	}
	else
	{
		printf(" dir-entry name... ");fflush(stdout);
		BU_CKMAG(segp -> seg_stp -> st_dp, RT_DIR_MAGIC,
		    "directory");
		bu_vls_strcpy(&sol_path_name, segp -> seg_stp -> st_name);
	}
	printf("'%s'\n", bu_vls_addr(&sol_path_name));

	/*
	 *	Attempt to record the new solid.
	 *	If it shares its name with a previously recorded solid,
	 *	then retain the one that appears earlier on the ray.
	 */
	sol = mk_solid(bu_vls_addr(&sol_path_name),
		    segp -> seg_in.hit_dist);
	if (bu_rb_insert(solids, (void *) sol) < 0)
	{
	    old_sol = (struct sol_name_dist *)
			bu_rb_curr(solids, ORDER_BY_NAME);
	    BU_CKMAG(old_sol, SOL_NAME_DIST_MAGIC,
		"sol_name_dist structure");
	    if (sol -> dist >= old_sol -> dist)
		free_solid(sol, 1);
	    else
	    {
		bu_rb_delete(solids, ORDER_BY_NAME);
		bu_rb_insert(solids, sol);
		free_solid(old_sol, 1);
	    }
	}
    }
    }

    /*
     *	Record the resulting list of solid names
     *	for use by the calling function
     */
    result = (char **)
		bu_malloc((solids -> rbt_nm_nodes + 1) * sizeof(char *),
			  "names of solids on ray");
    for (sol = (struct sol_name_dist *) bu_rb_min(solids, ORDER_BY_DISTANCE),
		i=0;
	 sol != NULL;
	 sol = (struct sol_name_dist *) bu_rb_succ(solids, ORDER_BY_DISTANCE),
		++i)
    {
	result[i] = sol -> name;
	free_solid(sol, 0);
    }
    result[i] = 0;
    ap -> a_uptr = (char *) result;

    bu_rb_free(solids, BU_RB_RETAIN_DATA);

    return 1;
    /*
     *	End new stuff
     */
#if 0

    for (segh = pp -> pt_inseg;
	    *((long *) segh) != BU_LIST_HEAD_MAGIC;
	    segh = (struct seg *) (segh -> l.forw))
	BU_CKMAG(segh, RT_SEG_MAGIC, "segment structure");

    /*
     *	Let's see what final_segs contains...
     */
    RT_CHECK_SEG(final_segs -> seg_stp);
    bu_vls_trunc(&sol_path_name, 0);
    fp = &(final_segs -> seg_stp -> st_path);
    bu_vls_strcpy(&sol_path_name, db_path_to_string(fp));
    printf("At line %d, sol_path_name contains '%s'\n",
	    __LINE__, bu_vls_addr(&sol_path_name));

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
	if (bu_rb_insert(solids, (void *) sol) < 0)
	{
	    old_sol = (struct sol_name_dist *)
			bu_rb_curr(solids, ORDER_BY_NAME);
	    BU_CKMAG(old_sol, SOL_NAME_DIST_MAGIC,
		"sol_name_dist structure");
	    if (sol -> dist >= old_sol -> dist)
		free_solid(sol, 1);
	    else
	    {
		bu_rb_delete(solids, ORDER_BY_NAME);
		bu_rb_insert(solids, sol);
		free_solid(old_sol, 1);
	    }
	}
    }
    printf("HELLO %s:%d\n", __FILE__, __LINE__);

    result = (char **)
		bu_malloc((solids -> rbt_nm_nodes + 1) * sizeof(char *),
			  "names of solids on ray");
    for (sol = (struct sol_name_dist *) bu_rb_min(solids, ORDER_BY_DISTANCE),
		i=0;
	 sol != NULL;
	 sol = (struct sol_name_dist *) bu_rb_succ(solids, ORDER_BY_DISTANCE),
		++i)
    {
	result[i] = sol -> name;
	free_solid(sol, 0);
    }
    result[i] = 0;
    ap -> a_uptr = (char *) result;

    bu_rb_free(solids, RB_RETAIN_DATA);
    return (1);
#endif
}
#endif

/*
 *			R P T _ H I T S _ M I K E
 *
 *  Each partition represents a segment, i.e. a single solid.
 *  Boolean operations have not been performed.
 *  The partition list is sorted by ascending inhit distance.
 *  This code does not attempt to eliminate duplicate segs,
 *  e.g. from piercing the torus twice.
 */
static int
rpt_hits_mike(struct application *ap, struct partition *PartHeadp, struct seg *segp)
{
	register struct partition	*pp;
	int		len;
	char		**list;
	int		i;

	len = rt_partition_len(PartHeadp) + 2;
	list = (char **)bu_calloc( len, sizeof(char *), "hit list[]");

	i = 0;
	for( pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
		RT_CK_PT(pp);
		list[i++] = db_path_to_string( &(pp->pt_inseg->seg_stp->st_path) );
	}
	list[i++] = NULL;
	if( i > len )  bu_bomb("rpt_hits_mike: array overflow\n");

	ap->a_uptr = (genptr_t)list;
	return len;
}

/*
 *			R P T _ M I S S
 *
 *		Miss handler for use by rt_shootray().
 *
 *	Stuffs the address of a null string in ap->a_uptr and returns 0.
 */

static int
rpt_miss(struct application *ap)
{
	ap->a_uptr = NULL;

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
char **skewer_solids (int argc, const char **argv, fastf_t *ray_orig, fastf_t *ray_dir, int full_path)
{
    struct application	ap;
    struct rt_i		*rtip;
    struct bu_list	sol_list;

	if (argc <= 0) {
		Tcl_AppendResult( interp, "skewer_solids argc<=0\n", (char *)NULL );
		return ((char **) 0);
	}

	/* .inmem rt_gettrees .rt -i -u [who] */
	rtip = rt_new_rti( dbip );
	rtip->useair = 1;
	rtip->rti_dont_instance = 1;	/* full paths to solids, too. */
	if (rt_gettrees(rtip, argc, argv, 1) == -1) {
		Tcl_AppendResult( interp, "rt_gettrees() failed\n", (char *)NULL );
		rt_clean(rtip);
		bu_free((genptr_t)rtip, "struct rt_i");
		return ((char **) 0);
	}

	/* .rt prep 1 */
	rtip->rti_hasty_prep = 1;
	rt_prep(rtip);

    BU_LIST_INIT(&sol_list);

    /*
     *	Initialize the application
     */
    RT_APPLICATION_INIT(&ap);
    ap.a_magic = RT_AP_MAGIC;
    ap.a_ray.magic = RT_RAY_MAGIC;
    ap.a_hit = rpt_hits_mike;
    ap.a_miss = rpt_miss;
    ap.a_resource = RESOURCE_NULL;
    ap.a_overlap = no_op;
    ap.a_onehit = 0;
    ap.a_user = 1;	/* Requests full paths to solids, not just basenames */
    ap.a_rt_i = rtip;
    ap.a_zero1 = ap.a_zero2 = 0;
    ap.a_purpose = "skewer_solids()";
    ap.a_no_booleans = 1;		/* full paths, no booleans */
    VMOVE(ap.a_ray.r_pt, ray_orig);
    VMOVE(ap.a_ray.r_dir, ray_dir);

    (void) rt_shootray(&ap);

	rt_clean(rtip);
	bu_free((genptr_t)rtip, "struct rt_i");

    return ((char **) ap.a_uptr);
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
