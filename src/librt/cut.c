/*                           C U T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup librt */
/*@{*/
/** @file cut.c
 *  Cut space into lots of small boxes (RPPs actually).
 *
 *  Call tree for default path through the code:
 *	rt_cut_it()
 *		rt_cut_extend() for all solids in model
 *		rt_ct_optim()
 *			rt_ct_old_assess()
 *			rt_ct_box()
 *				rt_ct_populate_box()
 *					rt_ck_overlap()
 *
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCScut[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nmg.h"
#include "plot3.h"
#include "./debug.h"

HIDDEN int		rt_ck_overlap BU_ARGS((const vect_t min,
					       const vect_t max,
					       const struct soltab *stp,
					       const struct rt_i *rtip));
HIDDEN int		rt_ct_box BU_ARGS((struct rt_i *rtip,
					   union cutter *cutp,
					   int axis, double where, int force));
HIDDEN void		rt_ct_optim BU_ARGS((struct rt_i *rtip,
					     union cutter *cutp, int depth));
HIDDEN void		rt_ct_free BU_ARGS((struct rt_i *rtip,
					    union cutter *cutp));
HIDDEN void		rt_ct_release_storage BU_ARGS((union cutter *cutp));

HIDDEN void		rt_ct_measure BU_ARGS((struct rt_i *rtip,
					       union cutter *cutp, int depth));
HIDDEN union cutter	*rt_ct_get BU_ARGS((struct rt_i *rtip));
void		rt_plot_cut BU_ARGS((FILE *fp, struct rt_i *rtip,
					     union cutter *cutp, int lvl));

BU_EXTERN(void		rt_pr_cut_info, (const struct rt_i *rtip,
					const char *str));
HIDDEN int		rt_ct_old_assess(register union cutter *,
					 register int,
					 double *,
					 double *);

#define AXIS(depth)	((depth)%3)	/* cuts: X, Y, Z, repeat */

/*
 *			R T _ C U T _ O N E _ A X I S
 *
 *  As a temporary aid until NUgrid is working, use NUgrid
 *  histogram to perform a preliminary partitioning of space,
 *  along a single axis.
 *  The tree built here is expected to be further refined.
 *  The bu_ptbl "boxes" contains a list of the boxnodes, for convenience.
 *
 *  Each span runs from [min].nu_spos to [max].nu_epos.
 */
union cutter *
rt_cut_one_axis(struct bu_ptbl *boxes, struct rt_i *rtip, int axis, int min, int max, struct nugridnode *nuginfop)
{
	register struct soltab *stp;
	union cutter	*box;
	int	cur;
	int	slice;

	BU_CK_PTBL(boxes);
	RT_CK_RTI(rtip);

	if( min == max )  {
		/* Down to one cell, generate a boxnode */
		slice = min;
		box = (union cutter *)bu_calloc( 1, sizeof(union cutter),
			"union cutter");
		box->bn.bn_type = CUT_BOXNODE;
		box->bn.bn_len = 0;
		box->bn.bn_maxlen = rtip->nsolids;
		box->bn.bn_list = (struct soltab **)bu_malloc(
			box->bn.bn_maxlen * sizeof(struct soltab *),
			"xbox boxnode []" );
		VMOVE( box->bn.bn_min, rtip->mdl_min );
		VMOVE( box->bn.bn_max, rtip->mdl_max );
		box->bn.bn_min[axis] = nuginfop->nu_axis[axis][slice].nu_spos;
		box->bn.bn_max[axis] = nuginfop->nu_axis[axis][slice].nu_epos;
		box->bn.bn_len = 0;

		/* Search all solids for those in this slice */
		RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
			RT_CHECK_SOLTAB(stp);
			if( !rt_ck_overlap( box->bn.bn_min, box->bn.bn_max,
					    stp, rtip ) )
				continue;
			box->bn.bn_list[box->bn.bn_len++] = stp;
		} RT_VISIT_ALL_SOLTABS_END

		bu_ptbl_ins( boxes, (long *)box );
		return box;
	}

	cur = (min + max + 1) / 2;
	/* Recurse on both sides, then build a cutnode */
	box = (union cutter *)bu_calloc( 1, sizeof(union cutter),
		"union cutter");
	box->cn.cn_type = CUT_CUTNODE;
	box->cn.cn_axis = axis;
	box->cn.cn_point = nuginfop->nu_axis[axis][cur].nu_spos;
	box->cn.cn_l = rt_cut_one_axis( boxes, rtip, axis, min, cur-1, nuginfop );
	box->cn.cn_r = rt_cut_one_axis( boxes, rtip, axis, cur, max, nuginfop );
	return box;
}

/*
 *			R T _ C U T _ O P T I M I Z E _ P A R A L L E L
 *
 *  Process all the nodes in the global array rtip->rti_cuts_waiting,
 *  until none remain.
 *  This routine is run in parallel.
 */
void
rt_cut_optimize_parallel(int cpu, genptr_t arg)
{
	struct rt_i	*rtip = (struct rt_i *)arg;
	union cutter	*cp;
	int		i;

	RT_CK_RTI(rtip);
	for(;;)  {

		bu_semaphore_acquire( RT_SEM_WORKER );
		i = rtip->rti_cuts_waiting.end--;	/* get first free index */
		bu_semaphore_release( RT_SEM_WORKER );
		i -= 1;				/* change to last used index */

		if( i < 0 )  break;

		cp = (union cutter *)BU_PTBL_GET( &rtip->rti_cuts_waiting, i );

		rt_ct_optim( rtip, cp, Z );
	}
}

#define CMP(_p1,_p2,_memb,_ind) \
	(*(const struct soltab **)(_p1))->_memb[_ind] < \
	(*(const struct soltab **)(_p2))->_memb[_ind] ? -1 : \
	(*(const struct soltab **)(_p1))->_memb[_ind] > \
	(*(const struct soltab **)(_p2))->_memb[_ind] ? 1 : 0

/* Functions for use with qsort */
HIDDEN int rt_projXmin_comp BU_ARGS((const void * p1, const void * p2));
HIDDEN int rt_projXmax_comp BU_ARGS((const void * p1, const void * p2));
HIDDEN int rt_projYmin_comp BU_ARGS((const void * p1, const void * p2));
HIDDEN int rt_projYmax_comp BU_ARGS((const void * p1, const void * p2));
HIDDEN int rt_projZmin_comp BU_ARGS((const void * p1, const void * p2));
HIDDEN int rt_projZmax_comp BU_ARGS((const void * p1, const void * p2));

HIDDEN int
rt_projXmin_comp(const void *p1, const void *p2)
{
	return CMP(p1,p2,st_min,X);
}

HIDDEN int
rt_projXmax_comp(const void *p1, const void *p2)
{
	return CMP(p1,p2,st_max,X);
}

HIDDEN int
rt_projYmin_comp(const void *p1, const void *p2)
{
	return CMP(p1,p2,st_min,Y);
}

HIDDEN int
rt_projYmax_comp(const void *p1, const void *p2)
{
	return CMP(p1,p2,st_max,Y);
}

HIDDEN int
rt_projZmin_comp(const void *p1, const void *p2)
{
	return CMP(p1,p2,st_min,Z);
}

HIDDEN int
rt_projZmax_comp(const void *p1, const void *p2)
{
	return CMP(p1,p2,st_max,Z);
}

HIDDEN struct cmp_pair {
	int (*cmp_min) BU_ARGS((const void *, const void *));
	int (*cmp_max) BU_ARGS((const void *, const void *));
} pairs[] = {
	{ rt_projXmin_comp, rt_projXmax_comp },
	{ rt_projYmin_comp, rt_projYmax_comp },
	{ rt_projZmin_comp, rt_projZmax_comp }
};

/*
 *			R T _ N U G R I D _ C U T
 *
 *   Makes a NUGrid node (CUT_NUGRIDNODE), filling the cells with solids
 *   from the given list.
 */

void
rt_nugrid_cut(register struct nugridnode *nugnp, register struct boxnode *fromp, struct rt_i *rtip, int just_collect_info, int depth)
{
#define USE_HIST 0
#if USE_HIST
	struct bu_hist start_hist[3];	/* where solid RPPs start */
	struct bu_hist end_hist[3];	/* where solid RPPs end */
#endif
	register struct soltab **stpp;

	int	nu_ncells;		/* # cells along one axis */
	int	nu_sol_per_cell;	/* avg # solids per cell */
	int	nu_max_ncells;		/* hard limit on nu_ncells */
	int	pseudo_depth;		/* "fake" depth to tell rt_ct_optim */
	register int	i;
	int	xp, yp, zp;
	vect_t	xmin, xmax, ymin, ymax, zmin, zmax;
	struct boxnode nu_xbox, nu_ybox, nu_zbox;

	if( nugnp->nu_type != CUT_NUGRIDNODE )
		rt_bomb( "rt_nugrid_cut: passed non-nugridnode" );

	/*
	 *  Build histograms of solid RPP extent distribution
	 *  when projected onto X, Y, and Z axes.
	 *  First stage in implementing the Gigante NUgrid algorithm.
	 *  (Proc. Ausgraph 1990, pg 157)
	 *  XXX For databases where the model diameter (diagonal through
	 *  XXX the model RPP) is small or huge, a histogram with
	 *  XXX floating-point ranges will be needed.
	 *  XXX Integers should suffice for now.
	 *
	 *  Goal is to keep the average number of solids per cell constant.
	 *  Assuming a uniform distribution, the number of cells along
	 *  each axis will need to be (nsol / desired_avg)**(1/3).
	 *  This is increased by two to err on the generous side,
	 *  resulting in a 3*3*3 grid even for small numbers of solids.
	 *
	 *  Presumably the caller will have set rtip->rti_nu_gfactor to
	 *  RT_NU_GFACTOR_DEFAULT (although it may change it depending on
	 *  the user's wishes or what it feels will be optimal for this
	 *  model.)  The default value was computed in the following fashion:
	 *  Suppose the ratio of the running times of ft_shot and
	 *  rt_advance_to_next_cell is K, i.e.,
	 *
	 *          avg_running_time( ft_shot )
	 *  K := ---------------------------------
	 *         avg_running_time( rt_advance )
	 *
	 *  Now suppose we divide each axis into G*n^R segments, yielding
	 *  G^3 n^(3R) cells.  We expect each cell to contain
	 *  G^(-3) n^(1-3R) solids, and hence the running time of firing
	 *  a single ray through the model will be proportional to
	 *
	 *  G*n^R * [ 1 + K G^(-3) n^(1-3R) ] = G*n^R + KG^(-2) n^(1-2R).
	 *
	 *  Since we wish for the running time of this algorithm to be low,
	 *  we wish to minimize the degree of this polynomial.  The optimal
	 *  choice for R is 1/3.  So the above equation becomes
	 *
	 *  G*n^(1/3) + KG^(-2) n^(1/3) = [ G + KG^(-2) ] n^(1/3).
	 *
	 *  We now wish to find an optimal value of G in terms of K.
	 *  Using basic calculus we see that
	 *
	 *                 G := (2K)^(1/3).
	 *
	 *  It has been experimentally observed that K ~ 5/3 when the model
	 *  is mostly arb8s.   Hence RT_NU_GFACTOR_DEFAULT := 1.5.
	 *
	 *  XXX More tests should be done for this.
	 */

	if( rtip->rti_nu_gfactor < 0.01 ||
	    rtip->rti_nu_gfactor > 10*RT_NU_GFACTOR_DEFAULT ) {
		rtip->rti_nu_gfactor = RT_NU_GFACTOR_DEFAULT;
		bu_log(
      "rt_nugrid_cut: warning: rtip->rti_nu_gfactor not set, setting to %g\n",
                        RT_NU_GFACTOR_DEFAULT );
	}

	nu_ncells = (int)ceil( 2.0 + rtip->rti_nu_gfactor *
			       pow( (double)fromp->bn_len, 1.0/3.0 ) );
	if( rtip->rti_nugrid_dimlimit > 0 &&
	    nu_ncells > rtip->rti_nugrid_dimlimit )
		nu_ncells = rtip->rti_nugrid_dimlimit;
	nu_sol_per_cell = (fromp->bn_len + nu_ncells - 1) / nu_ncells;
	nu_max_ncells = 2*nu_ncells + 8;
#if 0
	pseudo_depth = depth+(int)log((double)(nu_ncells*nu_ncells*nu_ncells));
#else
	pseudo_depth = depth;
#endif

	if( RT_G_DEBUG&DEBUG_CUT )
		bu_log(
	             "\nnu_ncells=%d, nu_sol_per_cell=%d, nu_max_ncells=%d\n",
		     nu_ncells, nu_sol_per_cell, nu_max_ncells );

#if USE_HIST
	for( i=0; i<3; i++ ) {
		bu_hist_init( &start_hist[i],
			      fromp->bn_min[i],
			      fromp->bn_max[i],
			      nu_ncells*100 );
		bu_hist_init( &end_hist[i],
			      fromp->bn_min[i],
			      fromp->bn_max[i],
			      nu_ncells*100 );
	}


	for( i = 0, stpp = fromp->bn_list;
	     i < fromp->bn_len;
	     i++, stpp++ ) {
		register struct soltab *stp = *stpp;
		RT_CK_SOLTAB( stp );
		if( stp->st_aradius <= 0 )  continue;
		if( stp->st_aradius >= INFINITY )  continue;
		for( j=0; j<3; j++ )  {
			BU_HIST_TALLY( &start_hist[j],stp->st_min[j] );
			BU_HIST_TALLY( &end_hist[j],  stp->st_max[j] );
		}
	}
#endif

	/* Allocate memory for nugrid axis parition. */

	for( i=0; i<3; i++ )
		nugnp->nu_axis[i] = (struct nu_axis *)bu_malloc(
			nu_max_ncells*sizeof(struct nu_axis), "NUgrid axis" );

#if USE_HIST
	/*
	 *  Next part of NUgrid algorithm:
	 *  Decide where cell boundaries should be, along each axis.
	 *  Each cell will be an interval across the histogram.
	 *  For each interval, track the number of new solids that
	 *  have *started* in the interval, and the number of existing solids
	 *  that have *ended* in the interval.
	 *  Continue widening the interval another histogram element
	 *  in width, until either the number of starts or endings
	 *  exceeds the goal for the average number of solids per
	 *  cell, nu_sol_per_cell = (nsolids / nu_ncells).
	 */

	for( i=0; i<3; i++ )  {
		fastf_t	pos;		/* Current interval start pos */
		int	nstart = 0;
		int	nend = 0;
		struct bu_hist	*shp = &start_hist[i];
		struct bu_hist	*ehp = &end_hist[i];
		int	hindex = 0;
		int	axi = 0;

		if( shp->hg_min != ehp->hg_min )
			rt_bomb("cut_it: hg_min error\n");
		pos = shp->hg_min;
		nugnp->nu_axis[i][axi].nu_spos = pos;
		for( hindex = 0; hindex < shp->hg_nbins; hindex++ )  {
			if( pos > shp->hg_max )  break;
			/* Advance interval one more histogram entry */
			/* NOTE:  Peeks into histogram structures! */
			nstart += shp->hg_bins[hindex];
			nend += ehp->hg_bins[hindex];
			pos += shp->hg_clumpsize;
#if 1
			if( nstart < nu_sol_per_cell &&
			    nend < nu_sol_per_cell ) continue;
#else
			if( nstart + nend < 2 * nu_sol_per_cell )
				continue;
#endif
			/* End current interval, start new one */
			nugnp->nu_axis[i][axi].nu_epos = pos;
			nugnp->nu_axis[i][axi].nu_width =
				pos - nugnp->nu_axis[i][axi].nu_spos;
			if( axi >= nu_max_ncells-1 )  {
				bu_log("NUgrid ran off end, axis=%d, axi=%d\n",
					i, axi);
				pos = shp->hg_max+1;
				break;
			}
			nugnp->nu_axis[i][++axi].nu_spos = pos;
			nstart = 0;
			nend = 0;
		}
		/* End final interval */
		nugnp->nu_axis[i][axi].nu_epos = pos;
		nugnp->nu_axis[i][axi].nu_width =
			pos - nugnp->nu_axis[i][axi].nu_spos;
		nugnp->nu_cells_per_axis[i] = axi+1;
	}

	/* Finished with temporary data structures */
	for( i=0; i<3; i++ ) {
		bu_hist_free( &start_hist[i] );
		bu_hist_free( &end_hist[i] );
	}
#else
	{
		struct soltab **list_min, **list_max;
		register struct soltab **l1, **l2;
		register int nstart, nend, axi, len = fromp->bn_len;
		register fastf_t pos;

		list_min = (struct soltab **)bu_malloc( len *
						      sizeof(struct soltab *),
							"min solid list" );
		list_max = (struct soltab **)bu_malloc( len *
						      sizeof(struct soltab *),
							"max solid list" );
		bcopy( fromp->bn_list, list_min, len*sizeof(struct soltab *) );
		bcopy( fromp->bn_list, list_max, len*sizeof(struct soltab *) );
		for( i=0; i<3; i++ ) {
			qsort( (genptr_t)list_min, len,
			       sizeof(struct soltab *), pairs[i].cmp_min );
			qsort( (genptr_t)list_max, len,
			       sizeof(struct soltab *), pairs[i].cmp_max );
			nstart = nend = axi = 0;
			l1 = list_min;
			l2 = list_max;

			pos = fromp->bn_min[i];
			nugnp->nu_axis[i][axi].nu_spos = pos;

			while( l1-list_min < len ||
			       l2-list_max < len ) {
				if( l2-list_max >= len ||
				    (l1-list_min < len &&
				     (*l1)->st_min[i] < (*l2)->st_max[i]) ) {
					pos = (*l1++)->st_min[i];
					++nstart;
				} else {
					pos = (*l2++)->st_max[i];
					++nend;
				}

#if 1
				if( nstart < nu_sol_per_cell &&
				    nend < nu_sol_per_cell )
#else
				if( nstart + nend < nu_sol_per_cell )
#endif
					continue;

				/* Don't make really teeny intervals. */
				if( pos <= nugnp->nu_axis[i][axi].nu_spos
#if 1
					   + 1.0
#endif
				           + rtip->rti_tol.dist )
					continue;

				/* don't make any more cuts if we've gone
				   past the end. */
				if( pos >= fromp->bn_max[i]
#if 1
					   - 1.0
#endif
					   - rtip->rti_tol.dist )
					continue;

				/* End current interval, start new one */
				nugnp->nu_axis[i][axi].nu_epos = pos;
				nugnp->nu_axis[i][axi].nu_width =
					pos - nugnp->nu_axis[i][axi].nu_spos;
				if( axi >= nu_max_ncells-1 )  {
					bu_log(
				      "NUgrid ran off end, axis=%d, axi=%d\n",
					       i, axi);
					rt_bomb( "rt_nugrid_cut: NUgrid ran off end" );
				}
				nugnp->nu_axis[i][++axi].nu_spos = pos;
				nstart = 0;
				nend = 0;
			}
			/* End final interval */
			if( pos < fromp->bn_max[i] ) pos = fromp->bn_max[i];
			nugnp->nu_axis[i][axi].nu_epos = pos;
			nugnp->nu_axis[i][axi].nu_width =
				pos - nugnp->nu_axis[i][axi].nu_spos;
			nugnp->nu_cells_per_axis[i] = axi+1;
		}
		bu_free( (genptr_t)list_min, "solid list min sort" );
		bu_free( (genptr_t)list_max, "solid list max sort" );
	}

#endif

	nugnp->nu_stepsize[X] = 1;
	nugnp->nu_stepsize[Y] = nugnp->nu_cells_per_axis[X] *
					nugnp->nu_stepsize[X];
	nugnp->nu_stepsize[Z] = nugnp->nu_cells_per_axis[Y] *
					nugnp->nu_stepsize[Y];

	/* For debugging */
	if( RT_G_DEBUG&DEBUG_CUT ) for( i=0; i<3; i++ ) {
		register int j;
		bu_log( "NUgrid %c axis:  %d cells\n", "XYZ*"[i],
			nugnp->nu_cells_per_axis[i] );
		for( j=0; j<nugnp->nu_cells_per_axis[i]; j++ ) {
			bu_log( "  %g .. %g, w=%g\n",
				nugnp->nu_axis[i][j].nu_spos,
				nugnp->nu_axis[i][j].nu_epos,
				nugnp->nu_axis[i][j].nu_width );
		}
	}

	/* If we were just asked to collect info, we are done.
	   The binary space partioning algorithm (sometimes) needs just this.*/

	if( just_collect_info ) return;

	/* For the moment, re-use "union cutter" */
	nugnp->nu_grid = (union cutter *)bu_malloc(
		nugnp->nu_cells_per_axis[X] *
		nugnp->nu_cells_per_axis[Y] *
		nugnp->nu_cells_per_axis[Z] * sizeof(union cutter),
		"3-D NUgrid union cutter []" );
	nu_xbox.bn_len = 0;
	nu_xbox.bn_maxlen = fromp->bn_len;
	nu_xbox.bn_list = (struct soltab **)bu_malloc(
		nu_xbox.bn_maxlen * sizeof(struct soltab *),
		"xbox boxnode []" );
	nu_ybox.bn_len = 0;
	nu_ybox.bn_maxlen = fromp->bn_len;
	nu_ybox.bn_list = (struct soltab **)bu_malloc(
		nu_ybox.bn_maxlen * sizeof(struct soltab *),
		"ybox boxnode []" );
	nu_zbox.bn_len = 0;
	nu_zbox.bn_maxlen = fromp->bn_len;
	nu_zbox.bn_list = (struct soltab **)bu_malloc(
		nu_zbox.bn_maxlen * sizeof(struct soltab *),
		"zbox boxnode []" );
	/* Build each of the X slices */
	for( xp = 0; xp < nugnp->nu_cells_per_axis[X]; xp++ ) {
		VMOVE( xmin, fromp->bn_min );
		VMOVE( xmax, fromp->bn_max );
		xmin[X] = nugnp->nu_axis[X][xp].nu_spos;
		xmax[X] = nugnp->nu_axis[X][xp].nu_epos;
		VMOVE( nu_xbox.bn_min, xmin );
		VMOVE( nu_xbox.bn_max, xmax );
		nu_xbox.bn_len = 0;

		/* Search all solids for those in this X slice */
		for( i = 0, stpp = fromp->bn_list;
		     i < fromp->bn_len;
		     i++, stpp++ ) {
			if( !rt_ck_overlap( xmin, xmax, *stpp, rtip ) )
				continue;
			nu_xbox.bn_list[nu_xbox.bn_len++] = *stpp;
		}

		/* Build each of the Y slices in this X slice */
		for( yp = 0; yp < nugnp->nu_cells_per_axis[Y]; yp++ ) {
			VMOVE( ymin, xmin );
			VMOVE( ymax, xmax );
			ymin[Y] = nugnp->nu_axis[Y][yp].nu_spos;
			ymax[Y] = nugnp->nu_axis[Y][yp].nu_epos;
			VMOVE( nu_ybox.bn_min, ymin );
			VMOVE( nu_ybox.bn_max, ymax );
			nu_ybox.bn_len = 0;
			/* Search X slice for membs of this Y slice */
			for( i=0; i<nu_xbox.bn_len; i++ )  {
				if( !rt_ck_overlap( ymin, ymax,
						    nu_xbox.bn_list[i],
						    rtip ) )
					continue;
				nu_ybox.bn_list[nu_ybox.bn_len++] =
					nu_xbox.bn_list[i];
			}
			/* Build each of the Z slices in this Y slice*/
			/* Each of these will be a final cell */
			for( zp = 0; zp < nugnp->nu_cells_per_axis[Z]; zp++ ) {
				register union cutter *cutp =
					&nugnp->nu_grid[
						zp*nugnp->nu_stepsize[Z] +
						yp*nugnp->nu_stepsize[Y] +
						xp*nugnp->nu_stepsize[X]];

				VMOVE( zmin, ymin );
				VMOVE( zmax, ymax );
				zmin[Z] = nugnp->nu_axis[Z][zp].nu_spos;
				zmax[Z] = nugnp->nu_axis[Z][zp].nu_epos;
				cutp->cut_type = CUT_BOXNODE;
				VMOVE( cutp->bn.bn_min, zmin );
				VMOVE( cutp->bn.bn_max, zmax );
				/* Build up a temporary list in nu_zbox first,
				 * then copy list over to cutp->bn */
				nu_zbox.bn_len = 0;
				/* Search Y slice for members of this Z slice*/
				for( i=0; i<nu_ybox.bn_len; i++ )  {
					if( !rt_ck_overlap( zmin, zmax,
							    nu_ybox.bn_list[i],
							    rtip ) )
						continue;
					nu_zbox.bn_list[nu_zbox.bn_len++] =
						nu_ybox.bn_list[i];
				}

				if( nu_zbox.bn_len <= 0 )  {
					/* Empty cell */
					cutp->bn.bn_list = (struct soltab **)0;
					cutp->bn.bn_len = 0;
					cutp->bn.bn_maxlen = 0;
					continue;
				}
				/* Allocate just enough space for list,
				 * and copy it in */

				cutp->bn.bn_list =
					(struct soltab **)bu_malloc(
						nu_zbox.bn_len *
						sizeof(struct soltab *),
						"NUgrid cell bn_list[]" );
				cutp->bn.bn_len = cutp->bn.bn_maxlen =
					nu_zbox.bn_len;
				bcopy( (char *)nu_zbox.bn_list,
				       (char *)cutp->bn.bn_list,
				       nu_zbox.bn_len *
				       sizeof(struct soltab *) );

				if( rtip->rti_nugrid_dimlimit > 0 ) {
#if 1
					rt_ct_optim( rtip, cutp, pseudo_depth);
#else
				/* Recurse, but only if we're cutting down on
				   the cellsize. */
					if( cutp->bn.bn_len > 5 &&
				    cutp->bn.bn_len < fromp->bn_len>>1 ) {

					/* Make a little NUGRID node here
					   to clean things up */
					union cutter temp;

					temp = *cutp;  /* union copy */
					cutp->cut_type = CUT_NUGRIDNODE;
					/* recursive call! */
					rt_nugrid_cut( &cutp->nugn,
						       &temp.bn, rtip, 0,
						       depth+1 );
					}
#endif
				}
			}
		}
	}

	bu_free( (genptr_t)nu_zbox.bn_list, "nu_zbox bn_list[]" );
	bu_free( (genptr_t)nu_ybox.bn_list, "nu_ybox bn_list[]" );
	bu_free( (genptr_t)nu_xbox.bn_list, "nu_xbox bn_list[]" );
}

int
rt_split_mostly_empty_cells( struct rt_i *rtip, union cutter *cutp )
{
	point_t max, min;
	struct soltab *stp;
	struct rt_piecelist pl;
	fastf_t range[3], empty[3], tmp;
	int upper_or_lower[3];
	fastf_t max_empty;
	int max_empty_dir;
	int i;
	int num_splits=0;

	switch( cutp->cut_type ) {
	case CUT_CUTNODE:
		num_splits += rt_split_mostly_empty_cells( rtip, cutp->cn.cn_l );
		num_splits += rt_split_mostly_empty_cells( rtip, cutp->cn.cn_r );
		break;
	case CUT_BOXNODE:
		/* find the actual bounds of stuff in this cell */
		if( cutp->bn.bn_len == 0 && cutp->bn.bn_piecelen == 0 ) {
			break;
		}
		VSETALL( min, MAX_FASTF );
		VREVERSE( max, min );

		for( i=0 ; i<cutp->bn.bn_len ; i++ ) {
			stp = cutp->bn.bn_list[i];
			VMIN( min, stp->st_min );
			VMAX( max, stp->st_max );
		}

		for( i=0 ; i<cutp->bn.bn_piecelen ; i++ ) {
			int j;

			pl = cutp->bn.bn_piecelist[i];
			for( j=0 ; j<pl.npieces ; j++ ) {
				int piecenum;

				piecenum = pl.pieces[j];
				VMIN( min, pl.stp->st_piece_rpps[piecenum].min );
				VMAX( max, pl.stp->st_piece_rpps[piecenum].max );
			}
		}

		/* clip min and max to the bounds of this cell */
		for( i=X ; i<=Z ; i++ ) {
			if( min[i] < cutp->bn.bn_min[i] ) {
				min[i] = cutp->bn.bn_min[i];
			}
			if( max[i] > cutp->bn.bn_max[i] ) {
				max[i] = cutp->bn.bn_max[i];
			}
		}

		/* min and max now have the real bounds of data in this cell */
		VSUB2( range, cutp->bn.bn_max, cutp->bn.bn_min );
		for( i=X ; i<=Z ; i++ ) {
			empty[i] = cutp->bn.bn_max[i] - max[i];
			upper_or_lower[i] = 1; /* upper section is empty */
			tmp = min[i] - cutp->bn.bn_min[i];
			if( tmp > empty[i] ) {
				empty[i] = tmp;
				upper_or_lower[i] = 0;	/* lower section is empty */
			}
		}
		max_empty = empty[X];
		max_empty_dir = X;
		if( empty[Y] > max_empty ) {
			max_empty = empty[Y];
			max_empty_dir = Y;
		}
		if( empty[Z] > max_empty ) {
			max_empty = empty[Z];
			max_empty_dir = Z;
		}
		if( max_empty / range[max_empty_dir] > 0.5 ) {
			/* this cell is over 50% empty in this direction, split it */

			fastf_t where;

			/* select cutting plane, but move it slightly off any geometry */
			if( upper_or_lower[max_empty_dir] ) {
			        where = max[max_empty_dir] + rtip->rti_tol.dist;
				if( where >= cutp->bn.bn_max[max_empty_dir] ) {
				       return( num_splits );
				}
			} else {
				where = min[max_empty_dir] - rtip->rti_tol.dist;
				if( where <= cutp->bn.bn_min[max_empty_dir] ) {
				       return( num_splits );
				}
			}
			if( where - cutp->bn.bn_min[max_empty_dir] < 2.0 ||
			    cutp->bn.bn_max[max_empty_dir] - where < 2.0 ) {
				/* will make a box too small */
				return( num_splits );
			}
			if( rt_ct_box( rtip, cutp, max_empty_dir, where, 1 ) ) {
				num_splits++;
				num_splits += rt_split_mostly_empty_cells( rtip, cutp->cn.cn_l );
				num_splits += rt_split_mostly_empty_cells( rtip, cutp->cn.cn_r );
			}
		}
		break;
	}

	return( num_splits );
}



/*
 *  			R T _ C U T _ I T
 *
 *  Go through all the solids in the model, given the model mins and maxes,
 *  and generate a cutting tree.  A strategy better than incrementally
 *  cutting each solid is to build a box node which contains everything
 *  in the model, and optimize it.
 *
 *  This is the main entry point into space partitioning from rt_prep().
 */
void
rt_cut_it(register struct rt_i *rtip, int ncpu)
{
	register struct soltab *stp;
	union cutter *finp;	/* holds the finite solids */
	FILE *plotfp;
	int num_splits=0;

	/* Make a list of all solids into one special boxnode, then refine. */
	BU_GETUNION( finp, cutter );
	finp->cut_type = CUT_BOXNODE;
	VMOVE( finp->bn.bn_min, rtip->mdl_min );
	VMOVE( finp->bn.bn_max, rtip->mdl_max );
	finp->bn.bn_len = 0;
	finp->bn.bn_maxlen = rtip->nsolids+1;
	finp->bn.bn_list = (struct soltab **)bu_malloc(
		finp->bn.bn_maxlen * sizeof(struct soltab *),
		"rt_cut_it: initial list alloc" );

	rtip->rti_inf_box.cut_type = CUT_BOXNODE;

	RT_VISIT_ALL_SOLTABS_START( stp, rtip ) {
		/* Ignore "dead" solids in the list.  (They failed prep) */
		if( stp->st_aradius <= 0 )  continue;

		/* Infinite and finite solids all get lumpped together */
		rt_cut_extend( finp, stp, rtip );

		if( stp->st_aradius >= INFINITY )  {
			/* Also add infinite solids to a special BOXNODE */
			rt_cut_extend( &rtip->rti_inf_box, stp, rtip );
		}
	} RT_VISIT_ALL_SOLTABS_END

	/* Dynamic decisions on tree limits.  Note that there will be
	 * (2**rtip->rti_cutdepth)*rtip->rti_cutlen potential leaf slots.
	 * Also note that solids will typically span several leaves.
	 */
	rtip->rti_cutlen = (int)log((double)rtip->nsolids);  /* ln ~= log2 */
	rtip->rti_cutdepth = 2 * rtip->rti_cutlen;
	if( rtip->rti_cutlen < 3 )  rtip->rti_cutlen = 3;
	if( rtip->rti_cutdepth < 12 )  rtip->rti_cutdepth = 12;
	if( rtip->rti_cutdepth > 24 )  rtip->rti_cutdepth = 24;     /* !! */
	if( RT_G_DEBUG&DEBUG_CUT )
		bu_log( "Before Space Partitioning: Max Tree Depth=%d, Cuttoff primitive count=%d\n",
			rtip->rti_cutdepth, rtip->rti_cutlen );

	bu_ptbl_init( &rtip->rti_cuts_waiting, rtip->nsolids,
		      "rti_cuts_waiting ptbl" );

	if( rtip->rti_hasty_prep )  {
		rtip->rti_space_partition = RT_PART_NUBSPT;
		rtip->rti_cutdepth = 6;
	}

	switch( rtip->rti_space_partition ) {
	case RT_PART_NUGRID:
		rtip->rti_CutHead.cut_type = CUT_NUGRIDNODE;
		rt_nugrid_cut( &rtip->rti_CutHead.nugn, &finp->bn, rtip, 0,0 );
		rt_fr_cut( rtip, finp ); /* done with finite solids box */
		break;
	case RT_PART_NUBSPT: {
#ifdef NEW_WAY
		struct nugridnode nuginfo;

		/* Collect statistics to assist binary space partition tree
		   construction */
		nuginfo.nu_type = CUT_NUGRIDNODE;
		rt_nugrid_cut( &nuginfo, &fin_box, rtip, 1, 0 );
#endif
		rtip->rti_CutHead = *finp;	/* union copy */
#ifdef NEW_WAY
		if( rtip->nsolids < 50000 )  {
#endif
		/* Old way, non-parallel */
		/* For some reason, this algorithm produces a substantial
		 * performance improvement over the parallel way, below.  The
		 * benchmark tests seem to be very sensitive to how the space
		 * partitioning is laid out.  Until we go to full NUgrid, this
		 * will have to do.  */
			rt_ct_optim( rtip, &rtip->rti_CutHead, 0 );
#ifdef NEW_WAY
		} else {

			XXX This hasnt been tested since massive
			    NUgrid changes were made

			/* New way, mostly parallel */
			union cutter	*head;
			int	i;

			head = rt_cut_one_axis( &rtip->rti_cuts_waiting, rtip,
			    Y, 0, nuginfo.nu_cells_per_axis[Y]-1, &nuginfo );
			rtip->rti_CutHead = *head;	/* struct copy */
			bu_free( (char *)head, "union cutter" );

			if(RT_G_DEBUG&DEBUG_CUTDETAIL)  {
				for( i=0; i<3; i++ )  {
					bu_log("\nNUgrid %c axis:  %d cells\n",
				     "XYZ*"[i], nuginfo.nu_cells_per_axis[i] );
				}
				rt_pr_cut( &rtip->rti_CutHead, 0 );
			}

			if( ncpu <= 1 )  {
				rt_cut_optimize_parallel(0, rtip);
			} else {
				bu_parallel( rt_cut_optimize_parallel, ncpu, rtip );
			}
		}
#endif
		/* one more pass to find cells that are mostly empty */
		num_splits = rt_split_mostly_empty_cells( rtip,  &rtip->rti_CutHead );

		if( RT_G_DEBUG&DEBUG_CUT ) {
			bu_log( "rt_split_mostly_empty_cells(): split %d cells\n", num_splits );
		}

		break; }
	default:
		rt_bomb( "rt_cut_it: unknown space partitioning method\n" );
	}

	bu_free( (genptr_t)finp, "finite solid box" );

	/* Measure the depth of tree, find max # of RPPs in a cut node */

	bu_hist_init( &rtip->rti_hist_cellsize, 0.0, 400.0, 400 );
	bu_hist_init( &rtip->rti_hist_cell_pieces, 0.0, 400.0, 400 );
	bu_hist_init( &rtip->rti_hist_cutdepth, 0.0,
		      (fastf_t)rtip->rti_cutdepth+1, rtip->rti_cutdepth+1 );
	bzero( rtip->rti_ncut_by_type, sizeof(rtip->rti_ncut_by_type) );
	rt_ct_measure( rtip, &rtip->rti_CutHead, 0 );
	if( RT_G_DEBUG&DEBUG_CUT )  {
		rt_pr_cut_info( rtip, "Cut" );
	}

	if( RT_G_DEBUG&DEBUG_CUTDETAIL ) {
		/* Produce a voluminous listing of the cut tree */
		rt_pr_cut( &rtip->rti_CutHead, 0 );
	}

	if( RT_G_DEBUG&DEBUG_PLOTBOX ) {
		/* Debugging code to plot cuts */
		if( (plotfp=fopen("rtcut.plot", "w"))!=NULL ) {
			pdv_3space( plotfp, rtip->rti_pmin, rtip->rti_pmax );
				/* Plot all the cutting boxes */
			rt_plot_cut( plotfp, rtip, &rtip->rti_CutHead, 0 );
			(void)fclose(plotfp);
		}
	}
}

/*
 *			R T _ C U T _ E X T E N D
 *
 *  Add a solid into a given boxnode, extending the lists there.
 *  This is used only for building the root node, which will then
 *  be subdivided.
 *
 *  Solids with pieces go onto a special list.
 */
void
rt_cut_extend(register union cutter *cutp, struct soltab *stp, const struct rt_i *rtip)
{
	RT_CK_SOLTAB(stp);
	RT_CK_RTI(rtip);

	BU_ASSERT( cutp->cut_type == CUT_BOXNODE );

	if(RT_G_DEBUG&DEBUG_CUTDETAIL)  {
		bu_log("rt_cut_extend(cutp=x%x) %s npieces=%d\n",
			cutp, stp->st_name, stp->st_npieces);
	}

	if( stp->st_npieces > 0 )  {
		struct rt_piecelist *plp;
		register int i;

		if( cutp->bn.bn_piecelist == NULL )  {
			/* Allocate enough piecelist's to hold all solids */
			BU_ASSERT( rtip->nsolids > 0 );
			cutp->bn.bn_piecelist = (struct rt_piecelist *) bu_malloc(
				sizeof(struct rt_piecelist) * (rtip->nsolids + 2),
				"rt_ct_box bn_piecelist (root node)" );
			cutp->bn.bn_piecelen = 0;	/* sanity */
			cutp->bn.bn_maxpiecelen = rtip->nsolids + 2;
		}
		plp = &cutp->bn.bn_piecelist[cutp->bn.bn_piecelen++];
		plp->magic = RT_PIECELIST_MAGIC;
		plp->stp = stp;

		/* List every index that this solid has */
		plp->npieces = stp->st_npieces;
		plp->pieces = (long *)bu_malloc(
			sizeof(long) * plp->npieces,
			"pieces[]" );
		for( i = stp->st_npieces-1; i >= 0; i-- )
			plp->pieces[i] = i;

		return;
	}

	/* No pieces, list the entire solid on bn_list */
	if( cutp->bn.bn_len >= cutp->bn.bn_maxlen )  {
		/* Need to get more space in list.  */
		if( cutp->bn.bn_maxlen <= 0 )  {
			/* Initial allocation */
			if( rtip->rti_cutlen > rtip->nsolids )
				cutp->bn.bn_maxlen = rtip->rti_cutlen;
			else
				cutp->bn.bn_maxlen = rtip->nsolids + 2;
			cutp->bn.bn_list = (struct soltab **)bu_malloc(
				cutp->bn.bn_maxlen * sizeof(struct soltab *),
				"rt_cut_extend: initial list alloc" );
		} else {
			cutp->bn.bn_maxlen *= 8;
			cutp->bn.bn_list = (struct soltab **) bu_realloc(
				(genptr_t)cutp->bn.bn_list,
				sizeof(struct soltab *) * cutp->bn.bn_maxlen,
				"rt_cut_extend: list extend" );
		}
	}
	cutp->bn.bn_list[cutp->bn.bn_len++] = stp;
}

/*
 *			R T _ C T _ P L A N
 *
 *  Attempt to make an "optimal" cut of the given boxnode.
 *  Consider cuts along all three axis planes, and choose
 *  the one with the smallest "offcenter" metric.
 *
 *  Returns -
 *	-1	No cut is possible
 *	 0	Node has been cut
 */
HIDDEN int
rt_ct_plan(struct rt_i *rtip, union cutter *cutp, int depth)
{
	int	axis;
	int	status[3];
	double	where[3];
	double	offcenter[3];
	int	best;
	double	bestoff;

	RT_CK_RTI(rtip);
	for( axis = X; axis <= Z; axis++ )  {
#if 0
 /* New way */
		status[axis] = rt_ct_assess(
			cutp, axis, &where[axis], &offcenter[axis] );
#else
 /* Old way */
		status[axis] = rt_ct_old_assess(
			cutp, axis, &where[axis], &offcenter[axis] );
#endif
	}

	for(;;)  {
		best = -1;
		bestoff = INFINITY;
		for( axis = X; axis <= Z; axis++ )  {
			if( status[axis] <= 0 )  continue;
			if( offcenter[axis] >= bestoff )  continue;
			/* This one is better than previous ones */
			best = axis;
			bestoff = offcenter[axis];
		}

		if( best < 0 )  return(-1);	/* No cut is possible */

		if( rt_ct_box( rtip, cutp, best, where[best], 0 ) > 0 )
			return(0);		/* OK */

		/*
		 *  This cut failed to reduce complexity on either side.
		 *  Mark this status as bad, and try the next-best
		 *  opportunity, if any.
		 */
		status[best] = 0;
	}
}

/*
 *			R T _ C T _ A S S E S S
 *
 *  Assess the possibility of making a cut along the indicated axis.
 *
 *  Returns -
 *	0	if a cut along this axis is not possible
 *	1	if a cut along this axis *is* possible, plus:
 *		*where		is proposed cut point, and
 *		*offcenter	is distance from "optimum" cut location.
 */
HIDDEN int
rt_ct_assess(register union cutter *cutp, register int axis, double *where_p, double *offcenter_p)
{
	register int	i;
	register double	val;
	register double	where;
	register double	offcenter;	/* Closest distance from midpoint */
	register double	middle;		/* midpoint */
	register double	left, right;

	if( cutp->bn.bn_len <= 1 )  return(0);		/* Forget it */

	/*
	 *  In absolute terms, each box must be at least 1mm wide after cut,
	 *  so there is no need subdividing anything smaller than twice that.
	 */
	if( (right=cutp->bn.bn_max[axis])-(left=cutp->bn.bn_min[axis]) <= 2.0 )
		return(0);

	/*
	 *  Split distance between min and max in half.
	 *  Find the closest edge of a solid's bounding RPP
	 *  to the mid-point, and split there.
	 *  This should ordinarily guarantee that at least one side of the
	 *  cut has one less item in it.
	 *
	 * XXX This should be much more sophisticated.
	 * XXX Consider making a list of candidate cut points
	 * (max and min of each bn_list[] element) with
	 * the subscript stored.
	 * Eliminaate candidates outside the current range.
	 * Sort the list.
	 * Eliminate duplicate candidates.
	 * The the element in the middle of the candidate list.
	 * Compute offcenter from middle of range as now.
	 */
	middle = (left + right) * 0.5;
	where = offcenter = INFINITY;
	for( i=0; i < cutp->bn.bn_len; i++ )  {
		/* left (min) edge */
		val = cutp->bn.bn_list[i]->st_min[axis];
		if( val > left && val < right )  {
			register double	d;
			if( (d = val - middle) < 0 )  d = (-d);
			if( d < offcenter )  {
				offcenter = d;
				where = val;
			}
		}
		/* right (max) edge */
		val = cutp->bn.bn_list[i]->st_max[axis];
		if( val > left && val < right )  {
			register double	d;
			if( (d = val - middle) < 0 )  d = (-d);
			if( d < offcenter )  {
				offcenter = d;
				where = val;
			}
		}
	}
	if( offcenter >= INFINITY )
		return(0);	/* no candidates? */
	if( where <= left || where >= right )
		return(0);	/* not reasonable */

	if( where - left <= 1.0 || right - where <= 1.0 )
		return(0);	/* cut will be too small */

	*where_p = where;
	*offcenter_p = offcenter;
	return(1);		/* OK */
}

#define PIECE_BLOCK	512

/*
 *			R T _ C T _ P O P U L A T E _ B O X
 *
 *  Given that 'outp' has been given a bounding box smaller than
 *  that of 'inp', copy over everything which still fits in the smaller box.
 *
 *  Returns -
 *	0	if outp has the same number of items as inp
 *	1	if outp has fewer items than inp
 */
HIDDEN int
rt_ct_populate_box(union cutter *outp, const union cutter *inp, struct rt_i *rtip)
{
	register int	i;
	int success = 0;
	const struct bn_tol *tol = &rtip->rti_tol;

	/* Examine the solids */
	outp->bn.bn_len = 0;
	outp->bn.bn_maxlen = inp->bn.bn_len;
	if( outp->bn.bn_maxlen > 0 )  {
		outp->bn.bn_list = (struct soltab **) bu_malloc(
			sizeof(struct soltab *) * outp->bn.bn_maxlen,
			"bn_list" );
		for( i = inp->bn.bn_len-1; i >= 0; i-- )  {
			struct soltab *stp = inp->bn.bn_list[i];
			if( !rt_ck_overlap(outp->bn.bn_min, outp->bn.bn_max,
			    stp, rtip))
				continue;
			outp->bn.bn_list[outp->bn.bn_len++] = stp;
		}
		if( outp->bn.bn_len < inp->bn.bn_len )  success = 1;
	} else {
		outp->bn.bn_list = (struct soltab **)NULL;
	}

	/* Examine the solid pieces */
	outp->bn.bn_piecelen = 0;
	if( inp->bn.bn_piecelen <= 0 )  {
		outp->bn.bn_piecelist = (struct rt_piecelist *)NULL;
		outp->bn.bn_maxpiecelen = 0;
		return success;
	}

	outp->bn.bn_piecelist = (struct rt_piecelist *) bu_malloc(
		sizeof(struct rt_piecelist) * inp->bn.bn_piecelen,
		"rt_piecelist" );
	outp->bn.bn_maxpiecelen = inp->bn.bn_piecelen;
#if 0
	for( i = inp->bn.bn_piecelen-1; i >= 0; i-- )  {
		struct rt_piecelist *plp = &inp->bn.bn_piecelist[i];	/* input */
		struct soltab *stp = plp->stp;
		struct rt_piecelist *olp = &outp->bn.bn_piecelist[outp->bn.bn_piecelen]; /* output */
		int j;

		RT_CK_PIECELIST(plp);
		RT_CK_SOLTAB(stp);
		olp->pieces = (long *)bu_malloc(
			sizeof(long) * plp->npieces,
			"olp->pieces[]" );
		olp->npieces = 0;

		/* Loop for every piece of this solid */
		for( j = plp->npieces-1; j >= 0; j-- )  {
			long indx = plp->pieces[j];
			struct bound_rpp *rpp = &stp->st_piece_rpps[indx];
			if( !V3RPP_OVERLAP_TOL(
			    outp->bn.bn_min, outp->bn.bn_max,
			    rpp->min, rpp->max, tol) )
				continue;
			olp->pieces[olp->npieces++] = indx;
		}
		if( olp->npieces > 0 )  {
			/* This solid contributed pieces to the output box */
			olp->magic = RT_PIECELIST_MAGIC;
			olp->stp = stp;
			outp->bn.bn_piecelen++;
			if( olp->npieces < plp->npieces ) success = 1;
		} else {
			bu_free( (char *)olp->pieces, "olp->pieces[]");
			olp->pieces = NULL;
		}
	}

#else
	for( i = inp->bn.bn_piecelen-1; i >= 0; i-- )  {
		struct rt_piecelist *plp = &inp->bn.bn_piecelist[i];	/* input */
		struct soltab *stp = plp->stp;
		struct rt_piecelist *olp = &outp->bn.bn_piecelist[outp->bn.bn_piecelen]; /* output */
		int j,k;
		long piece_list[PIECE_BLOCK];	/* array of pieces */
		long piece_count=0;		/* count of used slots in above array */
		long *more_pieces=NULL;		/* dynamically allocated array for overflow of above array */
		long more_piece_count=0;	/* number of slots used in dynamic array */
		long more_piece_len=0;		/* allocated length of dynamic array */

		RT_CK_PIECELIST(plp);
		RT_CK_SOLTAB(stp);

		/* Loop for every piece of this solid */
		for( j = plp->npieces-1; j >= 0; j-- )  {
			long indx = plp->pieces[j];
			struct bound_rpp *rpp = &stp->st_piece_rpps[indx];
			if( !V3RPP_OVERLAP_TOL(
			    outp->bn.bn_min, outp->bn.bn_max,
			    rpp->min, rpp->max, tol) )
				continue;
			if( piece_count < PIECE_BLOCK ) {
				piece_list[piece_count++] = indx;
			} else if( more_piece_count >= more_piece_len ) {
				/* this should be an extemely rare occurrence */
				more_piece_len += PIECE_BLOCK;
				more_pieces = (long *)bu_realloc( more_pieces, more_piece_len * sizeof( long ),
								  "more_pieces" );
				more_pieces[more_piece_count++] = indx;
			} else {
				more_pieces[more_piece_count++] = indx;
			}
		}
		olp->npieces = piece_count + more_piece_count;
		if( olp->npieces > 0 )  {
			/* This solid contributed pieces to the output box */
			olp->magic = RT_PIECELIST_MAGIC;
			olp->stp = stp;
			outp->bn.bn_piecelen++;
			olp->pieces = (long *)bu_malloc( sizeof(long) * olp->npieces, "olp->pieces[]" );
			for( j=0 ; j<piece_count ; j++ ) {
				olp->pieces[j] = piece_list[j];
			}
			k = piece_count;
			for( j=0 ; j<more_piece_count ; j++ ) {
				olp->pieces[k++] = more_pieces[j];
			}
			if( more_pieces ) {
				bu_free( (char *)more_pieces, "more_pieces" );
			}
			if( olp->npieces < plp->npieces ) success = 1;
		} else {
			olp->pieces = NULL;
			/*			if( plp->npieces > 0 ) success = 1; */
		}
	}
#endif

	return success;
}

/*
 *			R T _ C T _ B O X
 *
 *  Cut the given box node with a plane along the given axis,
 *  at the specified distance "where".
 *  Convert the caller's box node into a cut node, allocating two
 *  additional box nodes for the new leaves.
 *
 *  If, according to the classifier, both sides have the same number
 *  of solids, then nothing is changed, and an error is returned.
 *
 *  The storage strategy used is to make the maximum length of
 *  each of the two child boxnodes be the current length of the
 *  source node.
 *
 *  Returns -
 *	0	failure
 *	1	success
 */
HIDDEN int
rt_ct_box(struct rt_i *rtip, register union cutter *cutp, register int axis, double where, int force)
{
	register union cutter	*rhs, *lhs;
	int success = 0;

	RT_CK_RTI(rtip);
	if(RT_G_DEBUG&DEBUG_CUTDETAIL)  {
		bu_log("rt_ct_box(x%x, %c) %g .. %g .. %g\n",
			cutp, "XYZ345"[axis],
			cutp->bn.bn_min[axis],
			where,
			cutp->bn.bn_max[axis]);
	}

	/* LEFT side */
	lhs = rt_ct_get(rtip);
	lhs->bn.bn_type = CUT_BOXNODE;
	VMOVE( lhs->bn.bn_min, cutp->bn.bn_min );
	VMOVE( lhs->bn.bn_max, cutp->bn.bn_max );
	lhs->bn.bn_max[axis] = where;

	success = rt_ct_populate_box( lhs, cutp, rtip );

	/* RIGHT side */
	rhs = rt_ct_get(rtip);
	rhs->bn.bn_type = CUT_BOXNODE;
	VMOVE( rhs->bn.bn_min, cutp->bn.bn_min );
	VMOVE( rhs->bn.bn_max, cutp->bn.bn_max );
	rhs->bn.bn_min[axis] = where;

	success += rt_ct_populate_box( rhs, cutp, rtip );

	/* Check to see if complexity didn't decrease */
	if( success == 0 && !force )  {
		/*
		 *  This cut operation did no good, release storage,
		 *  and let caller attempt something else.
		 */
		if(RT_G_DEBUG&DEBUG_CUTDETAIL)  {
			static char axis_str[] = "XYZw";
			bu_log("rt_ct_box:  no luck, len=%d, axis=%c\n",
				cutp->bn.bn_len, axis_str[axis] );
		}
		rt_ct_free( rtip, rhs );
		rt_ct_free( rtip, lhs );
		return(0);		/* fail */
	}

	/* Success, convert callers box node into a cut node */
	rt_ct_release_storage( cutp );

	cutp->cut_type = CUT_CUTNODE;
	cutp->cn.cn_axis = axis;
	cutp->cn.cn_point = where;
	cutp->cn.cn_l = lhs;
	cutp->cn.cn_r = rhs;
	return(1);			/* success */
}

/*
 *			R T _ C K _ O V E R L A P
 *
 *  See if any part of the solid is contained within the bounding box (RPP).
 *
 *  If the solid RPP at least partly overlaps the bounding RPP,
 *  invoke the per-solid "classifier" method to perform a more
 *  rigorous check.
 *
 *  Returns -
 *	!0	if object overlaps box.
 *	0	if no overlap.
 */
HIDDEN int
rt_ck_overlap(register const fastf_t *min, register const fastf_t *max, register const struct soltab *stp, register const struct rt_i *rtip)
{
	RT_CHECK_SOLTAB(stp);
	if( RT_G_DEBUG&DEBUG_BOXING )  {
		bu_log("rt_ck_overlap(%s)\n",stp->st_name);
		VPRINT(" box min", min);
		VPRINT(" sol min", stp->st_min);
		VPRINT(" box max", max);
		VPRINT(" sol max", stp->st_max);
	}
	/* Ignore "dead" solids in the list.  (They failed prep) */
	if( stp->st_aradius <= 0 )  return(0);

	/* Only check RPP on finite solids */
	if( stp->st_aradius < INFINITY )  {
		if( V3RPP_DISJOINT( stp->st_min, stp->st_max, min, max ) )
			goto fail;
	}

	/* RPP overlaps, invoke per-solid method for detailed check */
	if( rt_functab[stp->st_id].ft_classify( stp, min, max,
			  &rtip->rti_tol ) == RT_CLASSIFY_OUTSIDE )  goto fail;

	if( RT_G_DEBUG&DEBUG_BOXING )  bu_log("rt_ck_overlap:  TRUE\n");
	return(1);
fail:
	if( RT_G_DEBUG&DEBUG_BOXING )  bu_log("rt_ck_overlap:  FALSE\n");
	return(0);
}

/*
 *			R T _ C T _ P I E C E C O U N T
 *
 *  Returns the total number of solids and solid "pieces" in a boxnode.
 */
HIDDEN int
rt_ct_piececount(const union cutter *cutp)
{
	int	i;
	int	count;

	BU_ASSERT( cutp->cut_type == CUT_BOXNODE );

	count = cutp->bn.bn_len;

	if( cutp->bn.bn_piecelen <= 0 )  return count;

	for( i = cutp->bn.bn_piecelen-1; i >= 0; i-- )  {
		count += cutp->bn.bn_piecelist[i].npieces;
	}
	return count;
}

/*
 *			R T _ C T _ O P T I M
 *
 *  Optimize a cut tree.  Work on nodes which are over the pre-set limits,
 *  subdividing until either the limit on tree depth runs out, or until
 *  subdivision no longer gives different results, which could easily be
 *  the case when several solids involved in a CSG operation overlap in
 *  space.
 */
HIDDEN void
rt_ct_optim(struct rt_i *rtip, register union cutter *cutp, int depth)
{
 	int oldlen;

	if( cutp->cut_type == CUT_CUTNODE )  {
		rt_ct_optim( rtip, cutp->cn.cn_l, depth+1 );
		rt_ct_optim( rtip, cutp->cn.cn_r, depth+1 );
		return;
	}
	if( cutp->cut_type != CUT_BOXNODE )  {
		bu_log("rt_ct_optim: bad node x%x\n", cutp->cut_type);
		return;
	}

	oldlen = rt_ct_piececount(cutp);	/* save before rt_ct_box() */
	if( RT_G_DEBUG&DEBUG_CUTDETAIL )  bu_log("rt_ct_optim( cutp=x%x, depth=%d ) piececount=%d\n", cutp, depth, oldlen);

	/*
	 * BOXNODE (leaf)
	 */
	if( oldlen <= 1 )
		return;		/* this box is already optimal */
	if( depth > rtip->rti_cutdepth )  return;		/* too deep */

	/* Attempt to subdivide finer than rtip->rti_cutlen near treetop */
	/**** XXX This test can be improved ****/
	if( depth >= 6 && oldlen <= rtip->rti_cutlen )
		return;				/* Fine enough */
#if NEW_WAY
 /* New way */
	/*
	 *  Attempt to make an optimal cut
	 */
	if( rt_ct_plan( rtip, cutp, depth ) < 0 )  {
		/* Unable to further subdivide this box node */
		return;
	}
#else
 /* Old (Release 3.7) way */
 {
	int did_a_cut;
	int i;
 	int axis;
 	double where, offcenter;
	/*
	 *  In general, keep subdividing until things don't get any better.
	 *  Really we might want to proceed for 2-3 levels.
	 *
	 *  First, make certain this is a worthwhile cut.
	 *  In absolute terms, each box must be at least 1mm wide after cut.
	 */
	axis = AXIS(depth);
#if 1
	did_a_cut = 0;
	for( i=0 ; i<3 ; i++ ) {
		axis += i;
		if( axis > Z ) {
			axis = X;
		}
		if( cutp->bn.bn_max[axis]-cutp->bn.bn_min[axis] < 2.0 ) {
			continue;
		}
		if( rt_ct_old_assess( cutp, axis, &where, &offcenter ) <= 0 ) {
			continue;
		}
		if( rt_ct_box( rtip, cutp, axis, where, 0 ) == 0 )  {
			continue;
		} else {
			did_a_cut = 1;
			break;
		}
	}

	if( !did_a_cut ) {
		return;
	}
#else
	if( cutp->bn.bn_max[axis]-cutp->bn.bn_min[axis] < 2.0 )
		return;
 	if( rt_ct_old_assess( cutp, axis, &where, &offcenter ) <= 0 )
 		return;			/* not practical */
	if( rt_ct_box( rtip, cutp, axis, where, 0 ) == 0 )  {
	 	if( rt_ct_old_assess( cutp, AXIS(depth+1), &where, &offcenter ) <= 0 )
	 		return;			/* not practical */
		if( rt_ct_box( rtip, cutp, AXIS(depth+1), where, 0 ) == 0 )
			return;	/* hopeless */
	}
#endif
	if( rt_ct_piececount(cutp->cn.cn_l) >= oldlen &&
	    rt_ct_piececount(cutp->cn.cn_r) >= oldlen )  {
 		if( RT_G_DEBUG&DEBUG_CUTDETAIL )
	    	bu_log("rt_ct_optim(cutp=x%x, depth=%d) oldlen=%d, lhs=%d, rhs=%d, hopeless\n",
	    		cutp, depth, oldlen,
			rt_ct_piececount(cutp->cn.cn_l),
			rt_ct_piececount(cutp->cn.cn_r) );
		return; /* hopeless */
	}
 }
#endif
	/* Box node is now a cut node, recurse */
	rt_ct_optim( rtip, cutp->cn.cn_l, depth+1 );
	rt_ct_optim( rtip, cutp->cn.cn_r, depth+1 );
}

/*
 *			R T _ C T _ O L D _ A S S E S S
 *
 *  From RCS revision 9.4
 *  NOTE:  Changing from rt_ct_assess() to this seems to result
 *  in a *massive* change in cut tree size.
 *	This version results in nbins=22, maxlen=3, avg=1.09,
 *  while new vewsion results in nbins=42, maxlen=3, avg=1.667 (on moss.g).
 */
HIDDEN int
rt_ct_old_assess(register union cutter *cutp, register int axis, double *where_p, double *offcenter_p)
{
	double		val;
	double		offcenter;		/* Closest distance from midpoint */
	double		where;		/* Point closest to midpoint */
	double		middle;		/* midpoint */
	double		d;
	fastf_t		max, min;
	register int	i;
	register double	left, right;

	if(RT_G_DEBUG&DEBUG_CUTDETAIL)bu_log("rt_ct_old_assess(x%x, %c)\n",cutp,"XYZ345"[axis]);

	/*  In absolute terms, each box must be at least 1mm wide after cut. */
	if( (right=cutp->bn.bn_max[axis])-(left=cutp->bn.bn_min[axis]) < 2.0 )
		return(0);

	/*
	 *  Split distance between min and max in half.
	 *  Find the closest edge of a solid's bounding RPP
	 *  to the mid-point, and split there.
	 *  This should ordinarily guarantee that at least one side of the
	 *  cut has one less item in it.
	 */
	min = MAX_FASTF;
	max = -min;
	where = left;
	middle = (left + right) * 0.5;
	offcenter = middle - where;	/* how far off 'middle', 'where' is */
	for( i=0; i < cutp->bn.bn_len; i++ )  {
		val = cutp->bn.bn_list[i]->st_min[axis];
		if( val < min ) min = val;
		if( val > max ) max = val;
		d = val - middle;
		if( d < 0 )  d = (-d);
		if( d < offcenter )  {
			offcenter = d;
			where = val-0.1;
		}
		val = cutp->bn.bn_list[i]->st_max[axis];
		if( val < min ) min = val;
		if( val > max ) max = val;
		d = val - middle;
		if( d < 0 )  d = (-d);
		if( d < offcenter )  {
			offcenter = d;
			where = val+0.1;
		}
	}

	/* Loop over all the solid pieces */
	for( i = cutp->bn.bn_piecelen-1; i >= 0; i-- )  {
		struct rt_piecelist *plp = &cutp->bn.bn_piecelist[i];
		struct soltab *stp = plp->stp;
		int	j;

		RT_CK_PIECELIST(plp);
		for( j = plp->npieces-1; j >= 0; j-- )  {
			int indx = plp->pieces[j];
			struct bound_rpp *rpp = &stp->st_piece_rpps[indx];

			val = rpp->min[axis];
			if( val < min ) min = val;
			if( val > max ) max = val;
			d = val - middle;
			if( d < 0 )  d = (-d);
			if( d < offcenter )  {
				offcenter = d;
				where = val-0.1;
			}
			val = rpp->max[axis];
			if( val < min ) min = val;
			if( val > max ) max = val;
			d = val - middle;
			if( d < 0 )  d = (-d);
			if( d < offcenter )  {
				offcenter = d;
				where = val+0.1;
			}
		}
	}

	if(RT_G_DEBUG&DEBUG_CUTDETAIL)bu_log("rt_ct_old_assess() left=%g, where=%g, right=%g, offcenter=%g\n",

	      left, where, right, offcenter);

	if( where < min || where > max ) {
		/* this will make an empty cell.
		 * try splitting the range instead
		 */
		where = (max + min) / 2.0;
		offcenter = where - middle;
		if( offcenter < 0 ) {
			offcenter = -offcenter;
		}
	}

	if( where <= left || where >= right )
		return(0);	/* not reasonable */

	if( where - left <= 1.0 || right - where <= 1.0 )
		return(0);	/* cut will be too small */

	/* We are going to cut */
	*where_p = where;
	*offcenter_p = offcenter;
	return(1);
}

/*
 *			R T _ C T _ G E T
 *
 *  This routine must run in parallel
 */
HIDDEN union cutter *
rt_ct_get(struct rt_i *rtip)
{
	register union cutter *cutp;

	RT_CK_RTI(rtip);
	bu_semaphore_acquire(RT_SEM_MODEL);
	if( !rtip->rti_busy_cutter_nodes.l.magic )
		bu_ptbl_init( &rtip->rti_busy_cutter_nodes, 128, "rti_busy_cutter_nodes" );

	if( rtip->rti_CutFree == CUTTER_NULL )  {
		register int bytes;

		bytes = bu_malloc_len_roundup(64*sizeof(union cutter));
		cutp = (union cutter *)bu_malloc(bytes," rt_ct_get");
		/* Remember this allocation for later */
		bu_ptbl_ins( &rtip->rti_busy_cutter_nodes, (long *)cutp );
		/* Now, dice it up */
		while( bytes >= sizeof(union cutter) )  {
			cutp->cut_forw = rtip->rti_CutFree;
			rtip->rti_CutFree = cutp++;
			bytes -= sizeof(union cutter);
		}
	}
	cutp = rtip->rti_CutFree;
	rtip->rti_CutFree = cutp->cut_forw;
	bu_semaphore_release(RT_SEM_MODEL);

	cutp->cut_forw = CUTTER_NULL;
	return(cutp);
}

/*
 *			R T _ C T _ R E L E A S E _ S T O R A G E
 *
 *  Release subordinate storage
 */
HIDDEN void
rt_ct_release_storage(register union cutter *cutp)
{
	int i;

	switch( cutp->cut_type )  {

	case CUT_CUTNODE:
		break;

	case CUT_BOXNODE:
		if( cutp->bn.bn_list )  {
			bu_free( (char *)cutp->bn.bn_list, "bn_list[]");
			cutp->bn.bn_list = (struct soltab **)NULL;
		}
		cutp->bn.bn_len = 0;
		cutp->bn.bn_maxlen = 0;

		if( cutp->bn.bn_piecelist )  {
			for( i=0 ; i<cutp->bn.bn_piecelen ; i++ ) {
				struct rt_piecelist *olp = &cutp->bn.bn_piecelist[i];
				if( olp->pieces ) {
					bu_free( (char *)olp->pieces, "olp->pieces" );
				}
			}
			bu_free( (char *)cutp->bn.bn_piecelist, "bn_piecelist[]" );
			cutp->bn.bn_piecelist = (struct rt_piecelist *)NULL;
		}
		cutp->bn.bn_piecelen = 0;
		cutp->bn.bn_maxpiecelen = 0;
		break;

	case CUT_NUGRIDNODE:
		bu_free( (genptr_t)cutp->nugn.nu_grid, "NUGrid children" );
		cutp->nugn.nu_grid = NULL; /* sanity */

		for( i=0; i<3; i++ ) {
			bu_free( (genptr_t)cutp->nugn.nu_axis[i],
				 "NUGrid axis" );
			cutp->nugn.nu_axis[i] = NULL; /* sanity */
		}
		break;

	default:
		bu_log("rt_ct_release_storage: Unknown type=x%x\n", cutp->cut_type );
		break;
	}
}

/*
 *			R T _ C T _ F R E E
 *
 *  This routine must run in parallel
 */
HIDDEN void
rt_ct_free(struct rt_i *rtip, register union cutter *cutp)
{
	RT_CK_RTI(rtip);

	rt_ct_release_storage( cutp );

	/* Put on global free list */
	bu_semaphore_acquire(RT_SEM_MODEL);
	cutp->cut_forw = rtip->rti_CutFree;
	rtip->rti_CutFree = cutp;
	bu_semaphore_release(RT_SEM_MODEL);
}

/*
 *			R T _ P R _ C U T
 *
 *  Print out a cut tree.
 */
void
rt_pr_cut(register const union cutter *cutp, int lvl)

        			/* recursion level */
{
	register int i,j;

	bu_log("%.8x ", cutp);
	for( i=lvl; i>0; i-- )
		bu_log("   ");

	if( cutp == CUTTER_NULL )  {
		bu_log("Null???\n");
		return;
	}

	switch( cutp->cut_type )  {

	case CUT_CUTNODE:
		bu_log("CUT L %c < %f\n",
			"XYZ?"[cutp->cn.cn_axis],
			cutp->cn.cn_point );
		rt_pr_cut( cutp->cn.cn_l, lvl+1 );

		bu_log("%.8x ", cutp);
		for( i=lvl; i>0; i-- )
			bu_log("   ");
		bu_log("CUT R %c >= %f\n",
			"XYZ?"[cutp->cn.cn_axis],
			cutp->cn.cn_point );
		rt_pr_cut( cutp->cn.cn_r, lvl+1 );
		return;

	case CUT_BOXNODE:
		bu_log("BOX Contains %d prims (%d alloc), %d prims with pieces:\n",
			cutp->bn.bn_len, cutp->bn.bn_maxlen,
			cutp->bn.bn_piecelen );
		bu_log("        ");
		for( i=lvl; i>0; i-- )
			bu_log("   ");
		VPRINT(" min", cutp->bn.bn_min);
		bu_log("        ");
		for( i=lvl; i>0; i-- )
			bu_log("   ");
		VPRINT(" max", cutp->bn.bn_max);

		/* Print names of regular solids */
		for( i=0; i < cutp->bn.bn_len; i++ )  {
			bu_log("        ");
			for( j=lvl; j>0; j-- )
				bu_log("   ");
			bu_log("    %s\n",
				cutp->bn.bn_list[i]->st_name);
		}

		/* Print names and piece lists of solids with pieces */
		for( i=0; i < cutp->bn.bn_piecelen; i++ )  {
			struct rt_piecelist *plp = &(cutp->bn.bn_piecelist[i]);
			struct soltab *stp;
			int j;

			RT_CK_PIECELIST(plp);
			stp = plp->stp;
			RT_CK_SOLTAB(stp);

			bu_log("        ");
			for( j=lvl; j>0; j-- )
				bu_log("   ");
			bu_log("    %s, %d pieces: ",
				stp->st_name, plp->npieces);

			/* Loop for every piece of this solid */
			for( j=0; j < plp->npieces; j++ )  {
				long indx = plp->pieces[j];
				bu_log("%ld, ", indx);
			}
			bu_log("\n");
		}
		return;

	case CUT_NUGRIDNODE:
		/* not implemented yet */
	default:
		bu_log("Unknown type=x%x\n", cutp->cut_type );
		break;
	}
	return;
}

/*
 *			R T _ F R _ C U T
 *
 *  Free a whole cut tree below the indicated node.
 *  The strategy we use here is to free everything BELOW the given
 *  node, so as not to clobber rti_CutHead !
 */
void
rt_fr_cut(struct rt_i *rtip, register union cutter *cutp)
{
	RT_CK_RTI(rtip);
	if( cutp == CUTTER_NULL )  {
		bu_log("rt_fr_cut NULL\n");
		return;
	}

	switch( cutp->cut_type )  {

	case CUT_CUTNODE:
		rt_fr_cut( rtip, cutp->cn.cn_l );
		rt_ct_free( rtip, cutp->cn.cn_l );
		cutp->cn.cn_l = CUTTER_NULL;

		rt_fr_cut( rtip, cutp->cn.cn_r );
		rt_ct_free( rtip, cutp->cn.cn_r );
		cutp->cn.cn_r = CUTTER_NULL;
		return;

	case CUT_BOXNODE:
		rt_ct_release_storage(cutp);
		return;

	case CUT_NUGRIDNODE: {
		register int i;
		int len = cutp->nugn.nu_cells_per_axis[X] *
			cutp->nugn.nu_cells_per_axis[Y] *
			cutp->nugn.nu_cells_per_axis[Z];
		register union cutter *bp = cutp->nugn.nu_grid;

		for( i = len-1; i >= 0; i-- )  {
			rt_fr_cut( rtip, bp );
			bp->cut_type = 7;	/* sanity */
			bp++;
		}

		rt_ct_release_storage(cutp);
		return; }

	default:
		bu_log("rt_fr_cut: Unknown type=x%x\n", cutp->cut_type );
		break;
	}
	return;
}

/*
 *  			R T _ P L O T _ C U T
 */
HIDDEN void
rt_plot_cut(FILE *fp, struct rt_i *rtip, register union cutter *cutp, int lvl)
{
	RT_CK_RTI(rtip);
	switch( cutp->cut_type )  {
	case CUT_NUGRIDNODE: {
		union cutter *bp;
		int i, x, y, z;
		int xmax = cutp->nugn.nu_cells_per_axis[X],
		    ymax = cutp->nugn.nu_cells_per_axis[Y],
		    zmax = cutp->nugn.nu_cells_per_axis[Z];
		if( !cutp->nugn.nu_grid ) return;
		/* Don't draw the boxnodes since that produces far too many
		   segments.  Optimize the output a little by drawing whole
		   line segments (below). */
		for( i=0, bp=cutp->nugn.nu_grid; i < xmax*ymax*zmax; i++, bp++ ) {
			if( bp->cut_type != CUT_BOXNODE )
				rt_plot_cut( fp, rtip, bp, lvl );
		}
		pl_color( fp, 180, 180, 220 );
		--xmax; --ymax; --zmax;

		/* XY plane */
		pd_3line( fp,
			  cutp->nugn.nu_axis[X][0].nu_spos,
			  cutp->nugn.nu_axis[Y][0].nu_spos,
			  cutp->nugn.nu_axis[Z][0].nu_spos,
			  cutp->nugn.nu_axis[X][0].nu_spos,
			  cutp->nugn.nu_axis[Y][0].nu_spos,
			  cutp->nugn.nu_axis[Z][zmax].nu_epos);
		for( y=0; y<=ymax; y++ ) {
			pd_3line( fp,
				  cutp->nugn.nu_axis[X][0].nu_spos,
				  cutp->nugn.nu_axis[Y][y].nu_epos,
				  cutp->nugn.nu_axis[Z][0].nu_spos,
				  cutp->nugn.nu_axis[X][0].nu_spos,
				  cutp->nugn.nu_axis[Y][y].nu_epos,
				  cutp->nugn.nu_axis[Z][zmax].nu_epos);
		}
		for( x=0; x<=xmax; x++ ) {
			pd_3line( fp,
				  cutp->nugn.nu_axis[X][x].nu_epos,
				  cutp->nugn.nu_axis[Y][0].nu_spos,
				  cutp->nugn.nu_axis[Z][0].nu_spos,
				  cutp->nugn.nu_axis[X][x].nu_epos,
				  cutp->nugn.nu_axis[Y][0].nu_spos,
				  cutp->nugn.nu_axis[Z][zmax].nu_epos);
			for( y=0; y<=ymax; y++ ) {
				pd_3line( fp,
					  cutp->nugn.nu_axis[X][x].nu_epos,
					  cutp->nugn.nu_axis[Y][y].nu_epos,
					  cutp->nugn.nu_axis[Z][0].nu_spos,
					  cutp->nugn.nu_axis[X][x].nu_epos,
					  cutp->nugn.nu_axis[Y][y].nu_epos,
					  cutp->nugn.nu_axis[Z][zmax].nu_epos);
			}
		}

		/* YZ plane */
		pd_3line( fp,
			  cutp->nugn.nu_axis[X][0].nu_spos,
			  cutp->nugn.nu_axis[Y][0].nu_spos,
			  cutp->nugn.nu_axis[Z][0].nu_spos,
			  cutp->nugn.nu_axis[X][xmax].nu_epos,
			  cutp->nugn.nu_axis[Y][0].nu_spos,
			  cutp->nugn.nu_axis[Z][0].nu_spos);
		for( z=0; z<=zmax; z++ ) {
			pd_3line( fp,
				  cutp->nugn.nu_axis[X][0].nu_spos,
				  cutp->nugn.nu_axis[Y][0].nu_spos,
				  cutp->nugn.nu_axis[Z][z].nu_epos,
				  cutp->nugn.nu_axis[X][zmax].nu_epos,
				  cutp->nugn.nu_axis[Y][0].nu_spos,
				  cutp->nugn.nu_axis[Z][z].nu_epos);
		}
		for( y=0; y<=ymax; y++ ) {
			pd_3line( fp,
				  cutp->nugn.nu_axis[X][0].nu_spos,
				  cutp->nugn.nu_axis[Y][y].nu_epos,
				  cutp->nugn.nu_axis[Z][0].nu_spos,
				  cutp->nugn.nu_axis[X][xmax].nu_epos,
				  cutp->nugn.nu_axis[Y][y].nu_epos,
				  cutp->nugn.nu_axis[Z][0].nu_spos);
			for( z=0; z<=zmax; z++ ) {
				pd_3line( fp,
					  cutp->nugn.nu_axis[X][0].nu_spos,
					  cutp->nugn.nu_axis[Y][y].nu_epos,
					  cutp->nugn.nu_axis[Z][z].nu_epos,
					  cutp->nugn.nu_axis[X][zmax].nu_epos,
					  cutp->nugn.nu_axis[Y][y].nu_epos,
					  cutp->nugn.nu_axis[Z][z].nu_epos);
			}
		}

		/* XZ plane */
		pd_3line( fp,
			  cutp->nugn.nu_axis[X][0].nu_spos,
			  cutp->nugn.nu_axis[Y][0].nu_spos,
			  cutp->nugn.nu_axis[Z][0].nu_spos,
			  cutp->nugn.nu_axis[X][0].nu_spos,
			  cutp->nugn.nu_axis[Y][ymax].nu_epos,
			  cutp->nugn.nu_axis[Z][0].nu_spos);
		for( z=0; z<=zmax; z++ ) {
			pd_3line( fp,
				  cutp->nugn.nu_axis[X][0].nu_spos,
				  cutp->nugn.nu_axis[Y][0].nu_spos,
				  cutp->nugn.nu_axis[Z][z].nu_epos,
				  cutp->nugn.nu_axis[X][0].nu_spos,
				  cutp->nugn.nu_axis[Y][ymax].nu_epos,
				  cutp->nugn.nu_axis[Z][z].nu_epos);
		}
		for( x=0; x<=xmax; x++ ) {
			pd_3line( fp,
				  cutp->nugn.nu_axis[X][x].nu_epos,
				  cutp->nugn.nu_axis[Y][0].nu_spos,
				  cutp->nugn.nu_axis[Z][0].nu_spos,
				  cutp->nugn.nu_axis[X][x].nu_epos,
				  cutp->nugn.nu_axis[Y][ymax].nu_epos,
				  cutp->nugn.nu_axis[Z][0].nu_spos);
			for( z=0; z<=zmax; z++ ) {
				pd_3line( fp,
					  cutp->nugn.nu_axis[X][x].nu_epos,
					  cutp->nugn.nu_axis[Y][0].nu_spos,
					  cutp->nugn.nu_axis[Z][z].nu_epos,
					  cutp->nugn.nu_axis[X][x].nu_epos,
					  cutp->nugn.nu_axis[Y][ymax].nu_epos,
					  cutp->nugn.nu_axis[Z][z].nu_epos);
			}
		}

		return; }
	case CUT_CUTNODE:
		rt_plot_cut( fp, rtip, cutp->cn.cn_l, lvl+1 );
		rt_plot_cut( fp, rtip, cutp->cn.cn_r, lvl+1 );
		return;
	case CUT_BOXNODE:
		/* Should choose color based on lvl, need a table */
		pl_color( fp,
			(AXIS(lvl)==0)?255:0,
			(AXIS(lvl)==1)?255:0,
			(AXIS(lvl)==2)?255:0 );
		pdv_3box( fp, cutp->bn.bn_min, cutp->bn.bn_max );
		return;
	}
	return;
}

/*
 *			R T _ C T _ M E A S U R E
 *
 *  Find the maximum number of solids in a leaf node,
 *  and other interesting statistics.
 */
HIDDEN void
rt_ct_measure(register struct rt_i *rtip, register union cutter *cutp, int depth)
{
	register int	len;

	RT_CK_RTI(rtip);
	switch( cutp->cut_type ) {
	case CUT_NUGRIDNODE: {
		register int i;
		register union cutter *bp;
		len = cutp->nugn.nu_cells_per_axis[X] *
			cutp->nugn.nu_cells_per_axis[Y] *
			cutp->nugn.nu_cells_per_axis[Z];
		rtip->rti_ncut_by_type[CUT_NUGRIDNODE]++;
		for( i=0, bp=cutp->nugn.nu_grid; i<len; i++, bp++ )
			rt_ct_measure( rtip, bp, depth+1 );
		return; }
	case CUT_CUTNODE:
		rtip->rti_ncut_by_type[CUT_CUTNODE]++;
		rt_ct_measure( rtip, cutp->cn.cn_l, len = (depth+1) );
		rt_ct_measure( rtip, cutp->cn.cn_r, len );
		return;
	case CUT_BOXNODE:
		rtip->rti_ncut_by_type[CUT_BOXNODE]++;
		rtip->rti_cut_totobj += (len = cutp->bn.bn_len);
		if( rtip->rti_cut_maxlen < len )
			rtip->rti_cut_maxlen = len;
		if( rtip->rti_cut_maxdepth < depth )
			rtip->rti_cut_maxdepth = depth;
		BU_HIST_TALLY( &rtip->rti_hist_cellsize, len );
		len = rt_ct_piececount( cutp ) - len;
		BU_HIST_TALLY( &rtip->rti_hist_cell_pieces, len );
		BU_HIST_TALLY( &rtip->rti_hist_cutdepth, depth );
		if( len == 0 ) {
			rtip->nempty_cells++;
		}
		return;
	default:
		bu_log("rt_ct_measure: bad node x%x\n", cutp->cut_type);
		return;
	}
}

/*
 *			R T _ C U T _ C L E A N
 *
 *  The rtip->rti_CutFree list can not be freed directly
 *  because  is bulk allocated.
 *  Fortunately, we have a list of all the bu_malloc()'ed blocks.
 *  This routine may be called before the first frame is done,
 *  so it must be prepared for uninitialized items.
 */
void
rt_cut_clean(struct rt_i *rtip)
{
	genptr_t	*p;

	RT_CK_RTI(rtip);

	if( rtip->rti_cuts_waiting.l.magic )
		bu_ptbl_free( &rtip->rti_cuts_waiting );

	/* Abandon the linked list of diced-up structures */
	rtip->rti_CutFree = CUTTER_NULL;

	if( BU_LIST_UNINITIALIZED(&rtip->rti_busy_cutter_nodes.l) )
		return;

	/* Release the blocks we got from bu_malloc() */
	for( BU_PTBL_FOR( p, (genptr_t *), &rtip->rti_busy_cutter_nodes ) )  {
		bu_free( *p, "rt_ct_get" );
	}
	bu_ptbl_free( &rtip->rti_busy_cutter_nodes );
}

/*
 *			R T _ P R _ C U T _ I N F O
 */
void
rt_pr_cut_info(const struct rt_i *rtip, const char *str)
{
	const struct nugridnode	*nugnp;
	int			i;

	RT_CK_RTI(rtip);

	bu_log("%s %s: %d nu, %d cut, %d box (%d empty)\n",
		str,
		rtip->rti_space_partition == RT_PART_NUGRID ?
			"NUGrid" : "NUBSP",
		rtip->rti_ncut_by_type[CUT_NUGRIDNODE],
		rtip->rti_ncut_by_type[CUT_CUTNODE],
		rtip->rti_ncut_by_type[CUT_BOXNODE],
		rtip->nempty_cells );
	bu_log( "Cut: maxdepth=%d, nbins=%d, maxlen=%d, avg=%g\n",
		rtip->rti_cut_maxdepth,
		rtip->rti_ncut_by_type[CUT_BOXNODE],
		rtip->rti_cut_maxlen,
		((double)rtip->rti_cut_totobj) /
		rtip->rti_ncut_by_type[CUT_BOXNODE] );
	bu_hist_pr( &rtip->rti_hist_cellsize,
		    "cut_tree: Number of prims per leaf cell");
	bu_hist_pr( &rtip->rti_hist_cell_pieces,
		    "cut_tree: Number of prim pieces per leaf cell");
	bu_hist_pr( &rtip->rti_hist_cutdepth,
		    "cut_tree: Depth (height)");

	switch( rtip->rti_space_partition )  {
	case RT_PART_NUGRID:
		nugnp = &rtip->rti_CutHead.nugn;
		if( nugnp->nu_type != CUT_NUGRIDNODE )
			bu_bomb( "rt_pr_cut_info: passed non-nugridnode" );

		for( i=0; i<3; i++ ) {
			register int j;
			bu_log( "NUgrid %c axis:  %d cells\n", "XYZ*"[i],
				nugnp->nu_cells_per_axis[i] );
			for( j=0; j<nugnp->nu_cells_per_axis[i]; j++ ) {
				bu_log( "  %g .. %g, w=%g\n",
					nugnp->nu_axis[i][j].nu_spos,
					nugnp->nu_axis[i][j].nu_epos,
					nugnp->nu_axis[i][j].nu_width );
			}
		}
		break;
	case RT_PART_NUBSPT:
		/* Anything good to print here? */
		break;
	default:
		bu_bomb("rt_pr_cut_info() bad rti_space_partition\n");
	}
}

void
remove_from_bsp( struct soltab *stp, union cutter *cutp, struct bn_tol *tol )
{
	int index;
	int i;

	switch( cutp->cut_type ) {
	case CUT_BOXNODE:
		if( stp->st_npieces ) {
			int remove_count, new_count;
			struct rt_piecelist *new_piece_list;

			index = 0;
			remove_count = 0;
			for( index=0 ; index<cutp->bn.bn_piecelen ; index++ ) {
				if( cutp->bn.bn_piecelist[index].stp == stp ) {
					remove_count++;
				}
			}

			if( remove_count ) {
				new_count = cutp->bn.bn_piecelen - remove_count;
				if( new_count > 0 ) {
					new_piece_list = (struct rt_piecelist *)bu_calloc(
								    new_count,
								    sizeof( struct rt_piecelist ),
								    "bn_piecelist" );

					i = 0;
					for( index=0 ; index<cutp->bn.bn_piecelen ; index++ ) {
						if( cutp->bn.bn_piecelist[index].stp != stp ) {
							new_piece_list[i] = cutp->bn.bn_piecelist[index];
							i++;
						}
					}
				} else {
					new_count = 0;
					new_piece_list = NULL;
				}

				for( index=0 ; index<cutp->bn.bn_piecelen ; index++ ) {
					if( cutp->bn.bn_piecelist[index].stp == stp ) {
						bu_free( cutp->bn.bn_piecelist[index].pieces, "pieces" );
					}
				}
				bu_free( cutp->bn.bn_piecelist, "piecelist" );
				cutp->bn.bn_piecelist = new_piece_list;
				cutp->bn.bn_piecelen = new_count;
				cutp->bn.bn_maxpiecelen = new_count;
			}
		} else {
			for( index=0 ; index < cutp->bn.bn_len ; index++ ) {
				if( cutp->bn.bn_list[index] == stp ) {
					/* found it, now remove it */
					cutp->bn.bn_len--;
					for( i=index ; i < cutp->bn.bn_len ; i++ ) {
						cutp->bn.bn_list[i] = cutp->bn.bn_list[i+1];
					}
					return;
				}
			}
		}
		break;
	case CUT_CUTNODE:
		if( stp->st_min[cutp->cn.cn_axis] > cutp->cn.cn_point + tol->dist ) {
			remove_from_bsp( stp, cutp->cn.cn_r, tol );
		} else if( stp->st_max[cutp->cn.cn_axis] < cutp->cn.cn_point - tol->dist ) {
			remove_from_bsp( stp, cutp->cn.cn_l, tol );
		} else {
			remove_from_bsp( stp, cutp->cn.cn_r, tol );
			remove_from_bsp( stp, cutp->cn.cn_l, tol );
		}
		break;
	default:
		bu_log( "remove_from_bsp(): unrecognized cut type (%d) in BSP!!!\n" );
		bu_bomb( "remove_from_bsp(): unrecognized cut type in BSP!!!\n" );
	}
}

#define PIECE_BLOCK 512

void
insert_in_bsp( struct soltab *stp, union cutter *cutp )
{
	int i,j;

	switch( cutp->cut_type ) {
	case CUT_BOXNODE:
		if( stp->st_npieces == 0 ) {
			/* add the solid in this box */
			if( cutp->bn.bn_len >= cutp->bn.bn_maxlen ) {
				/* need more space */
				if( cutp->bn.bn_maxlen <= 0 )  {
					/* Initial allocation */
					cutp->bn.bn_maxlen = 5;
					cutp->bn.bn_list = (struct soltab **)bu_malloc(
						   cutp->bn.bn_maxlen * sizeof(struct soltab *),
							"insert_in_bsp: initial list alloc" );
				} else {
					cutp->bn.bn_maxlen += 5;
					cutp->bn.bn_list = (struct soltab **) bu_realloc(
								(genptr_t)cutp->bn.bn_list,
								sizeof(struct soltab *) * cutp->bn.bn_maxlen,
								"insert_in_bsp: list extend" );
				}
			}
			cutp->bn.bn_list[cutp->bn.bn_len++] = stp;

		} else {
			/* this solid uses pieces, add the appropriate pieces to this box */
			long pieces[PIECE_BLOCK];
			long *more_pieces=NULL;
			long more_pieces_alloced=0;
			long more_pieces_count=0;
			long piece_count=0;
			struct rt_piecelist *plp;

			for( i=0 ; i<stp->st_npieces ; i++ ) {
				struct bound_rpp *piece_rpp=&stp->st_piece_rpps[i];
				if( V3RPP_OVERLAP( piece_rpp->min, piece_rpp->max, cutp->bn.bn_min, cutp->bn.bn_max ) ) {
					if( piece_count < PIECE_BLOCK ) {
						pieces[piece_count++] = i;
					} else if( more_pieces_alloced == 0 ) {
						more_pieces_alloced = stp->st_npieces - PIECE_BLOCK;
						more_pieces = (long *)bu_malloc( sizeof( long ) * more_pieces_alloced,
										 "more_pieces" );
						more_pieces[more_pieces_count++] = i;
					} else {
						more_pieces[more_pieces_count++] = i;
					}
				}
			}

			if( cutp->bn.bn_piecelen >= cutp->bn.bn_maxpiecelen ) {
				cutp->bn.bn_piecelist = (struct rt_piecelist *)bu_realloc( cutp->bn.bn_piecelist,
								      sizeof( struct rt_piecelist ) * (++cutp->bn.bn_maxpiecelen),
								      "cutp->bn.bn_piecelist" );
			}

			if( !piece_count ) {
				return;
			}

			plp = &cutp->bn.bn_piecelist[cutp->bn.bn_piecelen++];
			plp->magic = RT_PIECELIST_MAGIC;
			plp->stp = stp;
			plp->npieces = piece_count + more_pieces_count;
			plp->pieces = (long *)bu_malloc( plp->npieces * sizeof( long ), "plp->pieces" );
			for( i=0 ; i<piece_count ; i++ ) {
				plp->pieces[i] = pieces[i];
			}
			j = piece_count;
			for( i=0 ; i<more_pieces_count ; i++ ) {
				plp->pieces[j++] = more_pieces[i];
			}

			if( more_pieces ) {
				bu_free( (char *)more_pieces, "more_pieces" );
			}
		}
		break;
	case CUT_CUTNODE:
		if( stp->st_min[cutp->cn.cn_axis] >= cutp->cn.cn_point ) {
			insert_in_bsp( stp, cutp->cn.cn_r );
		} else if( stp->st_max[cutp->cn.cn_axis] < cutp->cn.cn_point ) {
			insert_in_bsp( stp, cutp->cn.cn_l );
		} else {
			insert_in_bsp( stp, cutp->cn.cn_r );
			insert_in_bsp( stp, cutp->cn.cn_l );
		}
		break;
	default:
		bu_log( "insert_in_bsp(): unrecognized cut type (%d) in BSP!!!\n" );
		bu_bomb( "insert_in_bsp(): unrecognized cut type in BSP!!!\n" );
	}

}

void
fill_out_bsp( struct rt_i *rtip, union cutter *cutp, struct resource *resp, fastf_t bb[6] )
{
	fastf_t bb2[6];
	int i, j;

	switch( cutp->cut_type ) {
	case CUT_BOXNODE:
		j = 3;
		for( i=0 ; i<3 ; i++ ) {
			if( bb[i] >= INFINITY ) {
				/* this node is at the edge of the model BB, make it fill the BB */
				cutp->bn.bn_min[i] = rtip->mdl_min[i];
			}
			if( bb[j] <= -INFINITY ) {
				/* this node is at the edge of the model BB, make it fill the BB */
				cutp->bn.bn_max[i] = rtip->mdl_max[i];
			}
			j++;
		}
		break;
	case CUT_CUTNODE:
		VMOVE( bb2, bb );
		VMOVE( &bb2[3], &bb[3] );
		bb[cutp->cn.cn_axis] = cutp->cn.cn_point;
		fill_out_bsp( rtip, cutp->cn.cn_r, resp, bb );
		bb2[cutp->cn.cn_axis + 3] = cutp->cn.cn_point;
		fill_out_bsp( rtip, cutp->cn.cn_l, resp, bb2 );
		break;
	default:
		bu_log( "fill_out_bsp(): unrecognized cut type (%d) in BSP!!!\n" );
		bu_bomb( "fill_out_bsp(): unrecognized cut type in BSP!!!\n" );
	}

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
