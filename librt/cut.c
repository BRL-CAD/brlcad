/*
 *  			C U T . C
 *  
 *  Cut space into lots of small boxes (RPPs actually).
 *  
 *  Before this can be done, the model max and min must have
 *  been computed -- no incremental cutting.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCScut[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nmg.h"
#include "./debug.h"

int rt_cutLen;			/* normal limit on number objs per box node */
int rt_cutDepth;		/* normal limit on depth of cut tree */

HIDDEN int		rt_ck_overlap RT_ARGS((CONST vect_t min,
					       CONST vect_t max,
					       CONST struct soltab *stp,
					       CONST struct rt_i *rtip));
HIDDEN int		rt_ct_box RT_ARGS((struct rt_i *rtip,
					   union cutter *cutp,
					   int axis, double where));
HIDDEN void		rt_ct_add RT_ARGS((union cutter *cutp,
					   struct soltab *stp,
					   vect_t min, vect_t max, int depth,
					   CONST struct rt_i *rtip));
HIDDEN void		rt_ct_optim RT_ARGS((struct rt_i *rtip,
					     union cutter *cutp, int depth));
HIDDEN void		rt_ct_free RT_ARGS((struct rt_i *rtip,
					    union cutter *cutp));
HIDDEN void		rt_ct_measure RT_ARGS((struct rt_i *rtip,
					       union cutter *cutp, int depth));
HIDDEN union cutter	*rt_ct_get RT_ARGS((struct rt_i *rtip));
HIDDEN void		rt_plot_cut RT_ARGS((FILE *fp, struct rt_i *rtip,
					     union cutter *cutp, int lvl));

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
rt_cut_one_axis( boxes, rtip, axis, min, max, nuginfop )
struct bu_ptbl	*boxes;
struct rt_i	*rtip;
int		axis;
int		min;
int		max;
struct nugridnode *nuginfop;
{
	register struct soltab *stp;
	union cutter	*box;
	int	cur;
	int	ret;
	int	slice;

	BU_CK_PTBL(boxes);
	RT_CK_RTI(rtip);

	if( min == max )  {
		/* Down to one cell, generate a boxnode */
		slice = min;
		box = (union cutter *)rt_calloc( 1, sizeof(union cutter),
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
	box = (union cutter *)rt_calloc( 1, sizeof(union cutter),
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
static struct rt_i	*rt_cut_rtip;	/* Shared across threads */

void
rt_cut_optimize_parallel()
{
	struct rt_i	*rtip = rt_cut_rtip;
	union cutter	*cp;
	int		i;

	RT_CK_RTI(rtip);
	for(;;)  {

		RES_ACQUIRE( &rt_g.res_worker );
		i = rtip->rti_cuts_waiting.end--;	/* get first free index */
		RES_RELEASE( &rt_g.res_worker );
		i -= 1;				/* change to last used index */

		if( i < 0 )  break;

		cp = (union cutter *)BU_PTBL_GET( &rtip->rti_cuts_waiting, i );

		rt_ct_optim( rtip, cp, Z );
	}
}

#define CMP(p1,p2,memb,ind) \
	(*(CONST struct soltab **)(p1))->memb[ind] < \
	(*(CONST struct soltab **)(p2))->memb[ind] ? -1 : \
	(*(CONST struct soltab **)(p1))->memb[ind] > \
	(*(CONST struct soltab **)(p2))->memb[ind] ? 1 : 0
	
HIDDEN int
rt_projXmin_comp( p1, p2 )
CONST void *p1, *p2;
{
	return CMP(p1,p2,st_min,X);
}

HIDDEN int
rt_projXmax_comp( p1, p2 )
CONST void *p1, *p2;
{
	return CMP(p1,p2,st_max,X);
}

HIDDEN int
rt_projYmin_comp( p1, p2 )
CONST void *p1, *p2;
{
	return CMP(p1,p2,st_min,Y);
}

HIDDEN int
rt_projYmax_comp( p1, p2 )
CONST void *p1, *p2;
{
	return CMP(p1,p2,st_max,Y);
}

HIDDEN int
rt_projZmin_comp( p1, p2 )
CONST void *p1, *p2;
{
	return CMP(p1,p2,st_min,Z);
}

HIDDEN int
rt_projZmax_comp( p1, p2 )
CONST void *p1, *p2;
{
	return CMP(p1,p2,st_max,Z);
}

/*
 *			R T _ N U G R I D _ C U T
 *
 *   Makes a NUGrid node (CUT_NUGRIDNODE) and fills in the specified
 *   list of solids.
 *
 *   This may possibly recurse if a particular box is
 *   assigned more than RT_MAX_NUGRIDCELL_ENTRIES.
 */

void
rt_nugrid_cut( nugnp, fromp, rtip, just_collect_info, depth )
register struct nugridnode	*nugnp;
register struct boxnode		*fromp;
struct rt_i			*rtip;
int				 just_collect_info, depth;
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
	int	fake_depth;
	register int	i, j, xp, yp, zp;
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
	fake_depth = 0;/*depth+(int)log((double)(nu_ncells*nu_ncells*nu_ncells));*/

	if( rt_g.debug&DEBUG_CUT )
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
		struct cmp_pair {
			int (*cmp_min)();
			int (*cmp_max)();
		} pairs[] = {
			{ rt_projXmin_comp, rt_projXmax_comp },
			{ rt_projYmin_comp, rt_projYmax_comp },
			{ rt_projZmin_comp, rt_projZmax_comp }
		};
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
			qsort( (void *)list_min, len,
			       sizeof(struct soltab *), pairs[i].cmp_min );
			qsort( (void *)list_max, len,
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
#if 0
				if( pos <= nugnp->nu_axis[i][axi].nu_spos +
				           rtip->rti_tol.dist )
					continue;
#else
				if( pos <= nugnp->nu_axis[i][axi].nu_spos +
				           2.0 )
					continue;
#endif				
				/* don't make any more cuts if we've gone
				   past the end. */
				if( pos >= fromp->bn_max[i] )
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
	if( rt_g.debug&DEBUG_CUT ) for( i=0; i<3; i++ ) {
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
					rt_ct_optim( rtip, cutp, fake_depth );
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

/*
 *  			R T _ C U T _ I T
 *  
 *  Go through all the solids in the model, given the model mins and maxes,
 *  and generate a cutting tree.  A strategy better than incrementally
 *  cutting each solid is to build a box node which contains everything
 *  in the model, and optimize it.
 */
void
rt_cut_it(rtip, ncpu)
register struct rt_i	*rtip;
int			ncpu;
{
	register struct soltab *stp;
	union cutter *finp;	/* holds the finite solids */
	FILE *plotfp;
	
	/* For plotting, compute a slight enlargement of the model RPP,
	 * to allow room for rays clipped to the model RPP to be depicted.
	 * Always do this, because application debugging may use it too.
	 */
	{
		register fastf_t f, diff;

		diff = (rtip->mdl_max[X] - rtip->mdl_min[X]);
		f = (rtip->mdl_max[Y] - rtip->mdl_min[Y]);
		if( f > diff )  diff = f;
		f = (rtip->mdl_max[Z] - rtip->mdl_min[Z]);
		if( f > diff )  diff = f;
		diff *= 0.1;	/* 10% expansion of box */
		rtip->rti_pmin[0] = rtip->mdl_min[0] - diff;
		rtip->rti_pmin[1] = rtip->mdl_min[1] - diff;
		rtip->rti_pmin[2] = rtip->mdl_min[2] - diff;
		rtip->rti_pmax[0] = rtip->mdl_max[0] + diff;
		rtip->rti_pmax[1] = rtip->mdl_max[1] + diff;
		rtip->rti_pmax[2] = rtip->mdl_max[2] + diff;
	}

	/* Copy solids into a list for rt_nugrid_cut, and throw infinite
	   solids into a special box. */

	BU_GETUNION( finp, cutter );
	finp->cut_type = CUT_BOXNODE;
	VMOVE( finp->bn.bn_min, rtip->rti_pmin );
	VMOVE( finp->bn.bn_max, rtip->rti_pmax );
	finp->bn.bn_maxlen = finp->bn.bn_len = 0;
	RT_VISIT_ALL_SOLTABS_START( stp, rtip ) {
		/* Ignore "dead" solids in the list.  (They failed prep) */
		if( stp->st_aradius <= 0 )  continue;
		if( stp->st_aradius >= INFINITY )  {
			/* Add to infinite solids list for special handling */
			rt_cut_extend( &rtip->rti_inf_box, stp );
		} else {
			rt_cut_extend( finp, stp );
		}
	} RT_VISIT_ALL_SOLTABS_END

	/*  Dynamic decisions on tree limits.  Note that there * will be
	    (2**rt_cutDepth)*rt_cutLen leaf slots, but * solids will typically
	    span several leaves.  */
		
	rt_cutLen = (int)log((double)rtip->nsolids);  /* ln ~= log2 */
	rt_cutDepth = 2 * rt_cutLen;
	if( rt_cutLen < 3 )  rt_cutLen = 3;
	if( rt_cutDepth < 9 )  rt_cutDepth = 9;
	if( rt_cutDepth > 24 )  rt_cutDepth = 24;     /* !! */
	if( rt_g.debug&DEBUG_CUT )
		bu_log( "Cut: Tree Depth=%d, Leaf Len=%d\n",
			rt_cutDepth, rt_cutLen );

	bu_ptbl_init( &rtip->rti_cuts_waiting, rtip->nsolids,
		      "rti_cuts_waiting ptbl" );

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
			rt_free( (char *)head, "union cutter" );
			
			if(rt_g.debug&DEBUG_CUTDETAIL)  {
				for( i=0; i<3; i++ )  {
					bu_log("\nNUgrid %c axis:  %d cells\n",
				     "XYZ*"[i], nuginfo.nu_cells_per_axis[i] );
				}
				rt_pr_cut( &rtip->rti_CutHead, 0 );
			}
			
			rt_cut_rtip = rtip;
			if( ncpu <= 1 )  {
				rt_cut_optimize_parallel(rtip);
			} else {
				bu_parallel( rt_cut_optimize_parallel, ncpu );
			}
		}
#endif
		break; }
	default:
		rt_bomb( "rt_cut_it: unknown space partitioning method\n" );
	}

	bu_free( (genptr_t)finp, "finite solid box" );

	/* Measure the depth of tree, find max # of RPPs in a cut node */

	bu_hist_init( &rtip->rti_hist_cellsize, 0.0, 400.0, 400 );
	bu_hist_init( &rtip->rti_hist_cutdepth, 0.0,
		      (fastf_t)rt_cutDepth+1, rt_cutDepth+1 );
	bzero( rtip->rti_ncut_by_type, sizeof(rtip->rti_ncut_by_type) );
	rt_ct_measure( rtip, &rtip->rti_CutHead, 0 );
	rt_ct_measure( rtip, &rtip->rti_inf_box, 0 );
	if( rt_g.debug&DEBUG_CUT ) {
		bu_log( "Cut: maxdepth=%d, nbins=%d, maxlen=%d, avg=%g\n",
			rtip->rti_cut_maxdepth,
			rtip->rti_ncut_by_type[CUT_BOXNODE],
			rtip->rti_cut_maxlen,
			((double)rtip->rti_cut_totobj) /
			rtip->rti_ncut_by_type[CUT_BOXNODE] );
		bu_hist_pr( &rtip->rti_hist_cellsize,
			    "cut_tree: Number of solids per leaf cell");
		bu_hist_pr( &rtip->rti_hist_cutdepth,
			    "cut_tree: Depth (height)");
		bu_log( "Counts: %d nugrid nodes, %d cutnodes, %d boxnodes\n",
			rtip->rti_ncut_by_type[CUT_NUGRIDNODE],
			rtip->rti_ncut_by_type[CUT_CUTNODE],
			rtip->rti_ncut_by_type[CUT_BOXNODE] );
	}

	if( rt_g.debug&DEBUG_CUTDETAIL ) {
		/* Produce a voluminous listing of the cut tree */
		rt_pr_cut( &rtip->rti_CutHead, 0 );
	}

	if( rt_g.debug&DEBUG_PLOTBOX ) {
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
 *			R T _ C T _ A D D
 *  
 *  Add a solid to the cut tree, extending the tree as necessary,
 *  but without being paranoid about obeying the rt_cutLen limits,
 *  so as to retain O(depth) performance.
 */
HIDDEN void
rt_ct_add( cutp, stp, min, max, depth, rtip )
register union cutter *cutp;
struct soltab *stp;	/* should be object handle & routine addr */
vect_t min, max;
int depth;
CONST struct rt_i *rtip;
{
	if(rt_g.debug&DEBUG_CUTDETAIL)bu_log("rt_ct_add(x%x, %s, %d)\n",
		cutp, stp->st_name, depth);
	if( cutp->cut_type == CUT_CUTNODE )  {
		vect_t temp;

		/* Cut to the left of point */
		VMOVE( temp, max );
		temp[cutp->cn.cn_axis] = cutp->cn.cn_point;
		if( rt_ck_overlap( min, temp, stp, rtip ) )
			rt_ct_add( cutp->cn.cn_l, stp, min, temp, depth+1,
				   rtip );

		/* Cut to the right of point */
		VMOVE( temp, min );
		temp[cutp->cn.cn_axis] = cutp->cn.cn_point;
		if( rt_ck_overlap( temp, max, stp, rtip ) )
			rt_ct_add( cutp->cn.cn_r, stp, temp, max, depth+1,
				   rtip );
		return;
	}
	if( cutp->cut_type != CUT_BOXNODE )  {
		bu_log("rt_ct_add:  node type =x%x\n",cutp->cut_type);
		return;
	}
	/* BOX NODE */

	/* Just add to list at this box node */
	rt_cut_extend( cutp, stp );
}

/*
 *			R T _ C U T _ E X T E N D
 */
void
rt_cut_extend( cutp, stp )
register union cutter *cutp;
struct soltab *stp;
{
	if( cutp->bn.bn_len >= cutp->bn.bn_maxlen )  {
		/* Need to get more space in list.  */
		if( cutp->bn.bn_maxlen <= 0 )  {
			/* Initial allocation */
			if( rt_cutLen > 6 )
				cutp->bn.bn_maxlen = rt_cutLen;
			else
				cutp->bn.bn_maxlen = 6;
			cutp->bn.bn_list = (struct soltab **)bu_malloc(
				cutp->bn.bn_maxlen * sizeof(struct soltab *),
				"rt_cut_extend: initial list alloc" );
		} else {
			register char *newlist;

			newlist = bu_malloc(
				sizeof(struct soltab *) * cutp->bn.bn_maxlen * 2,
				"rt_cut_extend: list extend" );
			bcopy( cutp->bn.bn_list, newlist,
				cutp->bn.bn_maxlen * sizeof(struct soltab *));
			cutp->bn.bn_maxlen *= 2;
			rt_free( (char *)cutp->bn.bn_list,
				"rt_cut_extend: list extend (old list)");
			cutp->bn.bn_list = (struct soltab **)newlist;
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
rt_ct_plan( rtip, cutp, depth )
struct rt_i	*rtip;
union cutter	*cutp;
int		depth;
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

		if( rt_ct_box( rtip, cutp, best, where[best] ) > 0 )
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
rt_ct_assess( cutp, axis, where_p, offcenter_p )
register union cutter *cutp;
register int	axis;
double		*where_p;
double		*offcenter_p;
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
 *  XXX should really check & store all the overlaps, & calculate
 *  minimum necessary sizes for each side.
 *
 *  Returns -
 *	0	failure
 *	1	success
 */
HIDDEN int
rt_ct_box( rtip, cutp, axis, where )
struct rt_i		*rtip;
register union cutter	*cutp;
register int		axis;
double			where;
{
	register union cutter	*rhs, *lhs;
	register int	i;

	RT_CK_RTI(rtip);
	if(rt_g.debug&DEBUG_CUTDETAIL)  {
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
	lhs->bn.bn_len = 0;
	lhs->bn.bn_maxlen = cutp->bn.bn_len;
	lhs->bn.bn_list = (struct soltab **) rt_malloc(
		sizeof(struct soltab *) * lhs->bn.bn_maxlen,
		"rt_ct_box (left list)" );
	for( i = cutp->bn.bn_len-1; i >= 0; i-- )  {
		if( !rt_ck_overlap(lhs->bn.bn_min, lhs->bn.bn_max,
		    cutp->bn.bn_list[i], rtip))
			continue;
		lhs->bn.bn_list[lhs->bn.bn_len++] = cutp->bn.bn_list[i];
	}

	/* RIGHT side */
	rhs = rt_ct_get(rtip);
	rhs->bn.bn_type = CUT_BOXNODE;
	VMOVE( rhs->bn.bn_min, cutp->bn.bn_min );
	VMOVE( rhs->bn.bn_max, cutp->bn.bn_max );
	rhs->bn.bn_min[axis] = where;
	rhs->bn.bn_len = 0;
	rhs->bn.bn_maxlen = cutp->bn.bn_len;
	rhs->bn.bn_list = (struct soltab **) rt_malloc(
		sizeof(struct soltab *) * rhs->bn.bn_maxlen,
		"rt_ct_box (right list)" );
	for( i = cutp->bn.bn_len-1; i >= 0; i-- )  {
		if( !rt_ck_overlap(rhs->bn.bn_min, rhs->bn.bn_max,
		    cutp->bn.bn_list[i], rtip))
			continue;
		rhs->bn.bn_list[rhs->bn.bn_len++] = cutp->bn.bn_list[i];
	}

	if( rhs->bn.bn_len == cutp->bn.bn_len && lhs->bn.bn_len == cutp->bn.bn_len )  {
		/*
		 *  This cut operation did no good, release storage,
		 *  and let caller attempt something else.
		 */
		if(rt_g.debug&DEBUG_CUTDETAIL)  {
			bu_log("rt_ct_box:  no luck, len=%d\n",
				cutp->bn.bn_len );
		}
		rt_free( (char *)rhs->bn.bn_list, "rt_ct_box, rhs list");
		rt_free( (char *)lhs->bn.bn_list, "rt_ct_box, lhs list");
		rt_ct_free( rtip, rhs );
		rt_ct_free( rtip, lhs );
		return(0);		/* fail */
	}

	/* Success, convert callers box node into a cut node */
	rt_free( (char *)cutp->bn.bn_list, "rt_ct_box (old list)" );
	cutp->bn.bn_list = (struct soltab **)0;

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
 *  See if any part of the object is contained within the box (RPP).
 *  There should be a routine per solid type to further refine
 *  this if the bounding boxes overlap.  Also need hooks for polygons.
 *
 *  Returns -
 *	!0	if object overlaps box.
 *	0	if no overlap.
 */
HIDDEN int
rt_ck_overlap( min, max, stp, rtip )
register CONST vect_t	min;
register CONST vect_t	max;
register CONST struct soltab *stp;
register CONST struct rt_i *rtip;
{
	RT_CHECK_SOLTAB(stp);
	if( rt_g.debug&DEBUG_BOXING )  {
		bu_log("rt_ck_overlap(%s)\n",stp->st_name);
		VPRINT("box min", min);
		VPRINT("sol min", stp->st_min);
		VPRINT("box max", max);
		VPRINT("sol max", stp->st_max);
	}
	/* Ignore "dead" solids in the list.  (They failed prep) */
	if( stp->st_aradius <= 0 )  return(0);
	if( stp->st_aradius >= INFINITY )  {
		/* Need object classification test here */
		if( rt_g.debug&DEBUG_BOXING )  bu_log("rt_ck_overlap:  TRUE (inf)\n");
		return(1);
	}
	if( stp->st_min[X] >= max[X]  || stp->st_max[X] <= min[X] )  goto fail;
	if( stp->st_min[Y] >= max[Y]  || stp->st_max[Y] <= min[Y] )  goto fail;
	if( stp->st_min[Z] >= max[Z]  || stp->st_max[Z] <= min[Z] )  goto fail;

	if( rt_functab[stp->st_id].ft_classify( stp, min, max,
			  &rtip->rti_tol ) == RT_CLASSIFY_OUTSIDE )  goto fail;

	if( rt_g.debug&DEBUG_BOXING )  bu_log("rt_ck_overlap:  TRUE\n");
	return(1);
fail:
	if( rt_g.debug&DEBUG_BOXING )  bu_log("rt_ck_overlap:  FALSE\n");
	return(0);
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
rt_ct_optim( rtip, cutp, depth )
struct rt_i		*rtip;
register union cutter *cutp;
int	depth;
{

	if( cutp->cut_type == CUT_CUTNODE )  {
		rt_ct_optim( rtip, cutp->cn.cn_l, depth+1 );
		rt_ct_optim( rtip, cutp->cn.cn_r, depth+1 );
		return;
	}
	if( cutp->cut_type != CUT_BOXNODE )  {
		bu_log("rt_ct_optim: bad node x%x\n", cutp->cut_type);
		return;
	}

	/*
	 * BOXNODE (leaf)
	 */
	if( cutp->bn.bn_len <= 1 )  return;		/* optimal */
	if( depth > rt_cutDepth )  return;		/* too deep */

	/* Attempt to subdivide finer than rt_cutLen near treetop */
	/**** XXX This test can be improved ****/
	if( depth >= 6 && cutp->bn.bn_len <= rt_cutLen )
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
 	int axis;
 	int oldlen;
 	double where, offcenter;
	/*
	 *  In general, keep subdividing until things don't get any better.
	 *  Really we might want to proceed for 2-3 levels.
	 *
	 *  First, make certain this is a worthwhile cut.
	 *  In absolute terms, each box must be at least 1mm wide after cut.
	 */
	axis = AXIS(depth);
	if( cutp->bn.bn_max[axis]-cutp->bn.bn_min[axis] < 2.0 )
		return;
	oldlen = cutp->bn.bn_len;	/* save before rt_ct_box() */
 	if( rt_ct_old_assess( cutp, axis, &where, &offcenter ) <= 0 )
 		return;			/* not practical */
	if( rt_ct_box( rtip, cutp, axis, where ) == 0 )  {
	 	if( rt_ct_old_assess( cutp, AXIS(depth+1), &where, &offcenter ) <= 0 )
	 		return;			/* not practical */
		if( rt_ct_box( rtip, cutp, AXIS(depth+1), where ) == 0 )
			return;	/* hopeless */
	}
	if( cutp->cn.cn_l->bn.bn_len >= oldlen &&
	    cutp->cn.cn_r->bn.bn_len >= oldlen )  return;	/* hopeless */
 }
#endif
	/* Box node is now a cut node, recurse */
	rt_ct_optim( rtip, cutp->cn.cn_l, depth+1 );
	rt_ct_optim( rtip, cutp->cn.cn_r, depth+1 );
}

/*
 *  From RCS revision 9.4
 *  NOTE:  Changing from rt_ct_assess() to this seems to result
 *  in a *massive* change in cut tree size.
 *	This version results in nbins=22, maxlen=3, avg=1.09,
 *  while new vewsion results in nbins=42, maxlen=3, avg=1.667 (on moss.g).
 */
HIDDEN int
rt_ct_old_assess( cutp, axis, where_p, offcenter_p )
register union cutter *cutp;
register int axis;
double	*where_p;
double	*offcenter_p;
{
	double		val;
	double		offcenter;		/* Closest distance from midpoint */
	double		where;		/* Point closest to midpoint */
	double		middle;		/* midpoint */
	double		d;
	register int	i;
	register double	left, right;

	if(rt_g.debug&DEBUG_CUTDETAIL)bu_log("rt_ct_old_assess(x%x, %c)\n",cutp,"XYZ345"[axis]);

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
	where = left;
	middle = (left + right) * 0.5;
	offcenter = middle - where;
	for( i=0; i < cutp->bn.bn_len; i++ )  {
		val = cutp->bn.bn_list[i]->st_min[axis];
		d = val - middle;
		if( d < 0 )  d = (-d);
		if( d < offcenter )  {
			offcenter = d;
			where = val-0.1;
		}
		val = cutp->bn.bn_list[i]->st_max[axis];
		d = val - middle;
		if( d < 0 )  d = (-d);
		if( d < offcenter )  {
			offcenter = d;
			where = val+0.1;
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
rt_ct_get(rtip)
struct rt_i	*rtip;
{
	register union cutter *cutp;

	RT_CK_RTI(rtip);
	RES_ACQUIRE(&rt_g.res_model);
	if( !rtip->rti_busy_cutter_nodes.l.magic )
		bu_ptbl_init( &rtip->rti_busy_cutter_nodes, 128, "rti_busy_cutter_nodes" );

	if( rtip->rti_CutFree == CUTTER_NULL )  {
		register int bytes;

		bytes = bu_malloc_len_roundup(64*sizeof(union cutter));
		cutp = (union cutter *)rt_malloc(bytes," rt_ct_get");
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
	RES_RELEASE(&rt_g.res_model);

	cutp->cut_forw = CUTTER_NULL;
	return(cutp);
}

/*
 *			R T _ C T _ F R E E
 *
 *  This routine must run in parallel
 */
HIDDEN void
rt_ct_free( rtip, cutp )
struct rt_i		*rtip;
register union cutter	*cutp;
{
	RT_CK_RTI(rtip);
	RES_ACQUIRE(&rt_g.res_model);
	cutp->cut_forw = rtip->rti_CutFree;
	rtip->rti_CutFree = cutp;
	RES_RELEASE(&rt_g.res_model);
}

/*
 *			R T _ P R _ C U T
 *  
 *  Print out a cut tree.
 */
void
rt_pr_cut( cutp, lvl )
register CONST union cutter *cutp;
int lvl;			/* recursion level */
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
		bu_log("BOX Contains %d solids (%d alloc):\n",
			cutp->bn.bn_len, cutp->bn.bn_maxlen );
		bu_log("        ");
		for( i=lvl; i>0; i-- )
			bu_log("   ");
		VPRINT(" min", cutp->bn.bn_min);
		bu_log("        ");
		for( i=lvl; i>0; i-- )
			bu_log("   ");
		VPRINT(" max", cutp->bn.bn_max);

		for( i=0; i < cutp->bn.bn_len; i++ )  {
			bu_log("        ");
			for( j=lvl; j>0; j-- )
				bu_log("   ");
			bu_log("    %s\n",
				cutp->bn.bn_list[i]->st_name);
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
 *  Free a whole cut tree.
 *  The strategy we use here is to free everything BELOW the given
 *  node, so as not to clobber rti_CutHead !
 */
void
rt_fr_cut( rtip, cutp )
struct rt_i		*rtip;
register union cutter *cutp;
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
		if( cutp->bn.bn_maxlen > 0 )
			bu_free( (genptr_t)cutp->bn.bn_list, "cut_box list");
		return;

	case CUT_NUGRIDNODE: {
		register int i, len = cutp->nugn.nu_cells_per_axis[X] *
			cutp->nugn.nu_cells_per_axis[Y] *
			cutp->nugn.nu_cells_per_axis[Z];
		register union cutter *bp;
		for( i=0, bp=cutp->nugn.nu_grid; i<len; i++, bp++ )
			rt_fr_cut( rtip, bp );
		bu_free( (genptr_t)cutp->nugn.nu_grid, "NUGrid children" );
		cutp->nugn.nu_grid = 0; /* sanity */
		for( i=0; i<3; i++ ) {
			bu_free( (genptr_t)cutp->nugn.nu_axis[i],
				 "NUGrid axis" );
			cutp->nugn.nu_axis[i] = 0; /* sanity */
		}
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
rt_plot_cut( fp, rtip, cutp, lvl )
FILE			*fp;
struct rt_i		*rtip;
register union cutter	*cutp;
int			lvl;
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
 *			R T _ I S E C T _ L S E G _ R P P
 *
 *  Intersect a line segment with a rectangular parallelpiped (RPP)
 *  that has faces parallel to the coordinate planes (a clipping RPP).
 *  The RPP is defined by a minimum point and a maximum point.
 *  This is a very close relative to rt_in_rpp() from librt/shoot.c
 *
 *  Returns -
 *	 0  if ray does not hit RPP,
 *	!0  if ray hits RPP.
 *
 *  Implicit Return -
 *	if !0 was returned, "a" and "b" have been clipped to the RPP.
 */
int
rt_isect_lseg_rpp( a, b, min, max )
point_t		a;
point_t		b;
register fastf_t *min, *max;
{
	auto vect_t	diff;
	register fastf_t *pt = &a[0];
	register fastf_t *dir = &diff[0];
	register int i;
	register double sv;
	register double st;
	register double mindist, maxdist;

	mindist = -INFINITY;
	maxdist = INFINITY;
	VSUB2( diff, b, a );

	for( i=0; i < 3; i++, pt++, dir++, max++, min++ )  {
		if( *dir < -SQRT_SMALL_FASTF )  {
			if( (sv = (*min - *pt) / *dir) < 0.0 )
				return(0);	/* MISS */
			if(maxdist > sv)
				maxdist = sv;
			if( mindist < (st = (*max - *pt) / *dir) )
				mindist = st;
		}  else if( *dir > SQRT_SMALL_FASTF )  {
			if( (st = (*max - *pt) / *dir) < 0.0 )
				return(0);	/* MISS */
			if(maxdist > st)
				maxdist = st;
			if( mindist < ((sv = (*min - *pt) / *dir)) )
				mindist = sv;
		}  else  {
			/*
			 *  If direction component along this axis is NEAR 0,
			 *  (ie, this ray is aligned with this axis),
			 *  merely check against the boundaries.
			 */
			if( (*min > *pt) || (*max < *pt) )
				return(0);	/* MISS */;
		}
	}
	if( mindist >= maxdist )
		return(0);	/* MISS */

	if( mindist > 1 || maxdist < 0 )
		return(0);	/* MISS */

	if( mindist >= 0 && maxdist <= 1 )
		return(1);	/* HIT within box, no clipping needed */

	/* Don't grow one end of a contained segment */
	if( mindist < 0 )
		mindist = 0;
	if( maxdist > 1 )
		maxdist = 1;

	/* Compute actual intercept points */
	VJOIN1( b, a, maxdist, diff );		/* b must go first */
	VJOIN1( a, a, mindist, diff );
	return(1);		/* HIT */
}

/*
 *			R T _ C T _ M E A S U R E
 *
 *  Find the maximum number of solids in a leaf node,
 *  and other interesting statistics.
 */
HIDDEN void
rt_ct_measure( rtip, cutp, depth )
register struct rt_i	*rtip;
register union cutter	*cutp;
int			depth;
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
		BU_HIST_TALLY( &rtip->rti_hist_cutdepth, depth );
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
rt_cut_clean(rtip)
struct rt_i	*rtip;
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
