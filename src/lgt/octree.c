/*                        O C T R E E . C
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
/** @file lgt/octree.c
    Author:		Gary S. Moss
*/
/*
  Originally extracted from SCCS archive:
  SCCS id:	@(#) octree.c	2.1
  Modified: 	12/10/86 at 16:03:58	G S M
  Retrieved: 	2/4/87 at 08:53:45
  SCCS archive:	/vld/moss/src/lgt/s.octree.c
*/

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./tree.h"


/* Error incurred while converting from double to float and back.	*/
#define F2D_EPSILON	1.0e-1
#define FABS(a)		((a) > 0 ? (a) : -(a))
#define SamePoint( p, q, e ) \
		(	FABS((p)[X]-(q)[X]) < (e) \
		    &&	FABS((p)[Y]-(q)[Y]) < (e) \
		    &&	FABS((p)[Z]-(q)[Z]) < (e) \
		)
#define NewPoint( p ) \
		(((p) = (PtList *) malloc(sizeof(PtList))) != PTLIST_NULL)
#define NewOctree( p ) \
		(((p) = (Octree *) malloc(sizeof(Octree))) != OCTREE_NULL)
static int	subdivide_Octree(Octree *parentp, int level);

Octree	*
new_Octant(Octree *parentp, Octree **childpp, int bitv, int level)
{
    Octree	*childp;
    fastf_t	delta = modl_radius / pow_Of_2( level );
    float	*origin = parentp->o_points->c_point;
    /* Create child node, filling in parent's pointer.		*/
    if ( ! NewOctree( *childpp ) )
    {
	Malloc_Bomb(sizeof(Octree));
	fatal_error = TRUE;
	return	OCTREE_NULL;
    }
    /* Fill in fields in child node.				*/
    childp = *childpp;
    childp->o_bitv = bitv;
    childp->o_temp = ABSOLUTE_ZERO;   /* Unclaimed by temperature.	*/
    childp->o_triep = TRIE_NULL;	  /* Unclaimed by region.	*/
    childp->o_sibling = OCTREE_NULL;  /* End of sibling chain.	*/
    childp->o_child = OCTREE_NULL;	  /* No children yet.		*/

    /* Create list node for origin of leaf octant.			*/
    if ( ! NewPoint( childp->o_points ) )
    {
	Malloc_Bomb(sizeof(PtList));
	fatal_error = TRUE;
	return	OCTREE_NULL;
    }
    childp->o_points->c_next = PTLIST_NULL; /* End of pt. chain.	*/
    /* Compute origin relative to parent, based on bit vector.	*/
    if ( bitv & 1<<X )
	childp->o_points->c_point[X] = origin[X] + delta;
    else
	childp->o_points->c_point[X] = origin[X] - delta;
    if ( bitv & 1<<Y )
	childp->o_points->c_point[Y] = origin[Y] + delta;
    else
	childp->o_points->c_point[Y] = origin[Y] - delta;
    if ( bitv & 1<<Z )
	childp->o_points->c_point[Z] = origin[Z] + delta;
    else
	childp->o_points->c_point[Z] = origin[Z] - delta;
    return	childp;
}

/*	f i n d _ O c t a n t ( )
	Starting at "parentp" and descending, locate octree node suitable
	for insertion of "pt".  Implicit return of tree level in "levelp".
	Return node's address.
*/
Octree	*
find_Octant(Octree *parentp, fastf_t *pt, int *levelp)
{
    if ( parentp == OCTREE_NULL )
    {
	bu_log( "find_Octant() parent node is NULL\n" );
	return	OCTREE_NULL;
    }
    do
    {
	int	bitv = 0;
	Octree	**childpp;
	float	*origin = parentp->o_points->c_point;
	/* Build bit vector to determine target octant.		*/
	bitv |= (pt[X] > origin[X]) << X;
	bitv |= (pt[Y] > origin[Y]) << Y;
	bitv |= (pt[Z] > origin[Z]) << Z;
	/* Search linked-list for target bit vector.		*/
	for (	childpp = &parentp->o_child;
		*childpp != OCTREE_NULL && (*childpp)->o_bitv != bitv;
		childpp = &(*childpp)->o_sibling
	    )
	    ;
	if ( *childpp != OCTREE_NULL )
	{
	    /* Found target octant, go next level.	*/
	    parentp = *childpp;
	    (*levelp)++;
	}
	else	/* Target octant doesn't exist yet.		*/
	    return	new_Octant( parentp, childpp, bitv, ++(*levelp) );
    }
    while ( parentp->o_child != OCTREE_NULL );
    return	parentp;	/* Returning leaf node.			*/
}

Octree	*
add_Region_Octree(Octree *parentp, fastf_t *pt, Trie *triep, int temp, int level)
{
    Octree	*newp;
    /* Traverse to octant leaf node containing "pt".		*/
    if ( (newp = find_Octant( parentp, pt, &level )) == OCTREE_NULL )
    {
	bu_log( "find_Octant() returned NULL!\n" );
	return	OCTREE_NULL;
    }

    /* Decide where to put datum.					*/
    if ( newp->o_points->c_next == PTLIST_NULL )
    {
	/* Octant empty, so place region here.		*/
	newp->o_triep = triep;
	if ( ! NewPoint( newp->o_points->c_next ) )
	{
	    Malloc_Bomb(sizeof(PtList));
	    fatal_error = TRUE;
	    return	OCTREE_NULL;
	}
	VMOVE( newp->o_points->c_next->c_point, pt );
	newp->o_points->c_next->c_next = PTLIST_NULL;
	if ( temp != AMBIENT-1 )
	    newp->o_temp = temp;
	return	newp;
    }
    else	  /* Octant occupied.					*/
	if ( triep != newp->o_triep )
	{
	    /* Region collision, must subdivide octant.		*/
	    if ( ! subdivide_Octree(	newp, level ))
		return	OCTREE_NULL;
	    return	add_Region_Octree( newp, pt, triep, temp, level );
	}
	else
	    if ( temp != AMBIENT-1 )
	    {
		/* We are assigning a temperature.			*/
		if ( newp->o_temp < AMBIENT )
		{
		    /* Temperature not assigned yet.		*/
		    newp->o_temp = temp;
		    if ( ! append_PtList( pt, newp->o_points ) )
			return	OCTREE_NULL;
		}
		else
		    if ( abs(newp->o_temp - temp) < ir_noise )
		    {
			/* Temperatures close enough.			*/
			if ( ! append_PtList( pt, newp->o_points ) )
			    return	OCTREE_NULL;
		    }
		    else
			if (	newp->o_points->c_next->c_next == PTLIST_NULL
				&&	SamePoint(	newp->o_points->c_next->c_point,
							pt,
							F2D_EPSILON
				    )
			    ) /* Only point in leaf node is this point.	*/
			    newp->o_temp = temp;
			else
			{
			    /* Temperature collision, must subdivide.	*/
			    if ( ! subdivide_Octree(	newp, level ) )
				return	OCTREE_NULL;
			    return	add_Region_Octree( newp, pt, triep, temp, level );
			}
	    }
	    else	/* Region pointers match, so append coordinate to list.	*/
		if ( ! append_PtList( pt, newp->o_points ) )
		    return	OCTREE_NULL;
    return	newp;
}
int
append_PtList(fastf_t *pt, PtList *ptlist)
{
    for (; ptlist->c_next != PTLIST_NULL; ptlist = ptlist->c_next )
    {
	if ( SamePoint( ptlist->c_next->c_point, pt, F2D_EPSILON ) )
	{
	    /* Point already in list.		*/
	    return	1;
	}
    }
    if ( ! NewPoint( ptlist->c_next ) )
    {
	Malloc_Bomb(sizeof(PtList));
	fatal_error = TRUE;
	return	0;
    }
    ptlist = ptlist->c_next;
    VMOVE( ptlist->c_point, pt );
    ptlist->c_next = PTLIST_NULL;
    return	1;
}

void
delete_PtList(PtList **ptlistp)
{
    PtList	*pp = *ptlistp, *np;
    *ptlistp = PTLIST_NULL;
    for (; pp != PTLIST_NULL; pp = np )
    {
	np = pp->c_next;
	free( (char *) pp );
    }
}

#define L_MAX_POWER_TWO		31

static int
subdivide_Octree(Octree *parentp, int level)
{
    PtList		*points = parentp->o_points->c_next;
    Trie		*triep = parentp->o_triep;
    int		temp = parentp->o_temp;
    /* Ward against integer overflow in 2^level.			*/
    if ( level > L_MAX_POWER_TWO )
    {
	bu_log( "Can not subdivide, level = %d\n", level );
	prnt_Octree( &ir_octree, 0 );
	return	0;
    }
    /* Remove datum from parent, it only belongs in leaves.		*/
    parentp->o_triep = TRIE_NULL;
    parentp->o_temp = ABSOLUTE_ZERO;
    parentp->o_points->c_next = PTLIST_NULL;
    /* Delete reference in trie tree to parent node.		*/
    delete_Node_OcList( &triep->l.t_octp, parentp );
    {
	PtList	*cp;
	/* Shove data down to sub-levels.				*/
	for ( cp = points; cp != PTLIST_NULL; cp = cp->c_next )
	{
	    fastf_t	c_point[3];
	    Octree	*octreep;
	    VMOVE( c_point, cp->c_point );
	    if (	(octreep =
			 add_Region_Octree( parentp, c_point, triep, temp, level )
			    ) != OCTREE_NULL
		)
		append_Octp( triep, octreep );
	    else
		return	0;
	}
    }
    delete_PtList( &points );
    return	1;
}

fastf_t
pow_Of_2(int power)
{
    long	value = 1;
    for (; power > 0; power-- )
	value *= 2;
    return	(fastf_t) value;
}

void
prnt_Node_Octree(Octree *parentp, int level)
{
    PtList	*ptp;
    int ptcount = 0;
    bu_log( "%s[%2d](%8.3f,%8.3f,%8.3f)bits=0%o temp=%04d trie=%p sibling=%p child=%p\n",
	    parentp->o_child != OCTREE_NULL ? "NODE" : "LEAF",
	    level,
	    parentp->o_points->c_point[X],
	    parentp->o_points->c_point[Y],
	    parentp->o_points->c_point[Z],
	    parentp->o_bitv,
	    parentp->o_temp,
	    parentp->o_triep,
	    parentp->o_sibling,
	    parentp->o_child
	);
    for ( ptp = parentp->o_points->c_next; ptp != PTLIST_NULL; ptp = ptp->c_next )
    {
	if ( RT_G_DEBUG )
	    bu_log( "\t%8.3f,%8.3f,%8.3f\n",
		    ptp->c_point[X],
		    ptp->c_point[Y],
		    ptp->c_point[Z]
		);
	ptcount++;
    }
    bu_log( "\t%d points\n", ptcount );
    return;
}

void
prnt_Octree(Octree *parentp, int level)
{
    Octree	*siblingp;
    /* Print each octant at this level.				*/
    for (	siblingp = parentp;
		siblingp != OCTREE_NULL;
		siblingp = siblingp->o_sibling
	)
	prnt_Node_Octree( siblingp, level );
    /* Print each octant at this level.				*/
    for (	siblingp = parentp;
		siblingp != OCTREE_NULL;
		siblingp = siblingp->o_sibling
	)
	prnt_Octree( siblingp->o_child, level+1 );
    return;
}

int
write_Octree(Octree *parentp, FILE *fp)
{
    PtList	*ptp;
    F_Hdr_Ptlist	hdr_ptlist;
    long		addr = ftell( fp );
    /* Write temperature and bogus number of points for this leaf.	*/
    hdr_ptlist.f_temp = parentp->o_temp;
    hdr_ptlist.f_length = 0;
    if ( fwrite( (char *) &hdr_ptlist, sizeof(F_Hdr_Ptlist), 1, fp ) != 1 )
    {
	bu_log( "\"%s\"(%d) Write failed!\n", __FILE__, __LINE__ );
	return	0;
    }
    /* Write out list of points.					*/
    for ( ptp = parentp->o_points->c_next; ptp != PTLIST_NULL; ptp = ptp->c_next )
    {
	hdr_ptlist.f_length++;
	if ( fwrite( (char *) ptp->c_point, sizeof(ptp->c_point), 1, fp )
	     != 1
	    )
	{
	    bu_log( "\"%s\"(%d) Write failed.\n", __FILE__, __LINE__ );
	    return	0;
	}
    }
    if ( hdr_ptlist.f_length > 0 )
    {
	/* Go back and fudge point count.			*/
	if ( fseek( fp, addr, 0 ) )
	{
	    bu_log( "\"%s\"(%d) Fseek failed.\n", __FILE__, __LINE__ );
	    return	0;
	}
	if ( fwrite( (char *) &hdr_ptlist, sizeof(hdr_ptlist), 1, fp )
	     != 1
	    )
	{
	    bu_log( "\"%s\"(%d) Write failed!\n", __FILE__, __LINE__ );
	    return	0;
	}
	/* Re-position write pointer to end-of-file.		*/
	if ( fseek( fp, 0L, 2 ) )
	{
	    bu_log( "\"%s\"(%d) Fseek failed.\n", __FILE__, __LINE__ );
	    return	0;
	}
    }
    return	1;
}

static void
hit_octant(struct application *ap, Octree *op, Octree **lpp, fastf_t *inv_dir, int level)
{
    for (; op != OCTREE_NULL; op = op->o_sibling )
    {
	fastf_t	octnt_min[3], octnt_max[3];
	fastf_t	delta = modl_radius / pow_Of_2( level );
	/* See if ray hits the octant RPP.			*/
	octnt_min[X] = op->o_points->c_point[X] - delta;
	octnt_min[Y] = op->o_points->c_point[Y] - delta;
	octnt_min[Z] = op->o_points->c_point[Z] - delta;
	octnt_max[X] = op->o_points->c_point[X] + delta;
	octnt_max[Y] = op->o_points->c_point[Y] + delta;
	octnt_max[Z] = op->o_points->c_point[Z] + delta;
	if ( rt_in_rpp( &ap->a_ray, inv_dir, octnt_min, octnt_max ) )
	{
	    /* Hit octant.				*/
	    if ( op->o_child == OCTREE_NULL )
	    {
		/* We are at a leaf node.		*/
		if ( ap->a_uvec[0] > ap->a_ray.r_min )
		{
		    /* Closest, so far.		*/
		    ap->a_uvec[0] = ap->a_ray.r_min;
		    ap->a_level = level;
		    *lpp = op;
		}
	    }
	    else	  /* We must descend to lower level.	*/
		hit_octant( ap, op->o_child, lpp, inv_dir, level+1 );
	}
    }
    /* No more octants at this level.				*/
    return;
}
int
ir_shootray_octree(struct application *ap)
{
    vect_t	inv_dir;	/* Inverses of ap->a_ray.r_dir	*/
    Octree	*leafp = NULL;	/* Intersected octree leaf.	*/
    inv_dir[X] = inv_dir[Y] = inv_dir[Z] = INFINITY;
    if ( !ZERO(ap->a_ray.r_dir[X]) )
	inv_dir[X] = 1.0 / ap->a_ray.r_dir[X];
    if ( !ZERO(ap->a_ray.r_dir[Y]) )
	inv_dir[Y] = 1.0 / ap->a_ray.r_dir[Y];
    if ( !ZERO(ap->a_ray.r_dir[Z]) )
	inv_dir[Z] = 1.0 / ap->a_ray.r_dir[Z];
    /* Descend octree from root to find the closest intersected leaf node.
       Store minimum hit distance in "a_uvec[0]" field of application
       structure.  Implicitly return the leaf node in "leafp".
    */
    ap->a_uvec[0] = INFINITY; /* Minimum hit point, safe distance.	*/
    hit_octant( ap, &ir_octree, &leafp, inv_dir, 0 );
    if ( leafp != OCTREE_NULL )
	/* Hit model.						*/
	/* a_hit is f_IR_Model(), uses 2nd arg as (Octree *) */
	return	ap->a_hit( ap, (struct partition *)leafp, RT_SEG_NULL );
    else	/* Missed it.						*/
	return	ap->a_miss( ap );
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
