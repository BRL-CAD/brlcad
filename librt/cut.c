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

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./debug.h"

int rt_cutLen;			/* normal limit on number objs per box node */
int rt_cutDepth;		/* normal limit on depth of cut tree */

HIDDEN int	rt_ck_overlap(), rt_ct_box();
HIDDEN void	rt_ct_add(), rt_ct_optim(), rt_ct_free();
HIDDEN void	rt_ct_measure();
HIDDEN union cutter *rt_ct_get();

#define AXIS(depth)	((depth)%3)	/* cuts: X, Y, Z, repeat */

HIDDEN void rt_plot_cut();

/*
 *  			R T _ C U T _ I T
 *  
 *  Go through all the solids in the model, given the model mins and maxes,
 *  and generate a cutting tree.  A strategy better than incrementally
 *  cutting each solid is to build a box node which contains everything
 *  in the model, and optimize it.
 */
void
rt_cut_it(rtip)
register struct rt_i *rtip;
{
	register struct soltab *stp;
	FILE *plotfp;
	struct histogram xhist;
	struct histogram yhist;
	struct histogram zhist;

	rtip->rti_CutHead.bn.bn_type = CUT_BOXNODE;
	VMOVE( rtip->rti_CutHead.bn.bn_min, rtip->mdl_min );
	VMOVE( rtip->rti_CutHead.bn.bn_max, rtip->mdl_max );
	rtip->rti_CutHead.bn.bn_len = 0;
	rtip->rti_CutHead.bn.bn_maxlen = rtip->nsolids+1;
	rtip->rti_CutHead.bn.bn_list = (struct soltab **)rt_malloc(
		rtip->rti_CutHead.bn.bn_maxlen * sizeof(struct soltab *),
		"rt_cut_it: root list" );
	for( RT_LIST( stp, soltab, &(rtip->rti_headsolid) ) )  {
		if( stp->st_aradius >= INFINITY )  {
			/* Add to infinite solids list for special handling */
			rt_cut_extend( &rtip->rti_inf_box, stp );
		} else {
			rt_cut_extend( &rtip->rti_CutHead, stp );
		}
	}

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

	/*
	 *  Build histograms of solid RPP extent distribution
	 *  when projected onto X, Y, and Z axes.
	 *  First stage in implementing the Gigante NUgrid algorithm.
	 *  (Proc. Ausgraph 1990, pg 157)
	 *  XXX For small or huge models, needs a floating-point histogram.
	 */
#define	RT_NUGRID_NBINS	120
	rt_hist_init( &xhist, (int)rtip->rti_pmin[X], (int)rtip->rti_pmax[X], RT_NUGRID_NBINS );
	rt_hist_init( &yhist, (int)rtip->rti_pmin[Y], (int)rtip->rti_pmax[Y], RT_NUGRID_NBINS );
	rt_hist_init( &zhist, (int)rtip->rti_pmin[Z], (int)rtip->rti_pmax[Z], RT_NUGRID_NBINS );
	for( RT_LIST( stp, soltab, &(rtip->rti_headsolid) ) )  {
		if( stp->st_aradius >= INFINITY )  continue;
		rt_hist_range( &xhist, (int)stp->st_min[X], (int)stp->st_max[X] );
		rt_hist_range( &yhist, (int)stp->st_min[Y], (int)stp->st_max[Y] );
		rt_hist_range( &zhist, (int)stp->st_min[Z], (int)stp->st_max[Z] );
	}
	if(rt_g.debug&DEBUG_CUT)  {
		rt_hist_pr( &xhist, "cut_tree:  solid RPP extent distribution in X" );
		rt_hist_pr( &yhist, "cut_tree:  solid RPP extent distribution in Y" );
		rt_hist_pr( &zhist, "cut_tree:  solid RPP extent distribution in Z" );
	}
	rt_hist_free( &xhist );
	rt_hist_free( &yhist );
	rt_hist_free( &zhist );

	/*  Dynamic decisions on tree limits.
	 *  Note that there will be (2**rt_cutDepth)*rt_cutLen leaf slots,
	 *  but solids will typically span several leaves.
	 */
	rt_cutLen = (int)log((double)rtip->nsolids);	/* ln ~= log2 */
	rt_cutDepth = 2 * rt_cutLen;
	if( rt_cutLen < 3 )  rt_cutLen = 3;
	if( rt_cutDepth < 9 )  rt_cutDepth = 9;
	if( rt_cutDepth > 24 )  rt_cutDepth = 24;		/* !! */
	if(rt_g.debug&DEBUG_CUT) rt_log("Cut: Tree Depth=%d, Leaf Len=%d\n", rt_cutDepth, rt_cutLen );
	rt_ct_optim( &rtip->rti_CutHead, 0 );

	/* Measure the depth of tree, find max # of RPPs in a cut node */
	rt_hist_init( &rtip->rti_hist_cellsize, 0, 99, 100 );
	rt_hist_init( &rtip->rti_hist_cutdepth, 0, rt_cutDepth+1, 100 );
	rt_ct_measure( rtip, &rtip->rti_CutHead, 0 );
	rt_ct_measure( rtip, &rtip->rti_inf_box, 0 );
	if(rt_g.debug&DEBUG_CUT)  {
		rt_log("Cut: maxdepth=%d, nbins=%d, maxlen=%d, avg=%g\n",
		rtip->rti_cut_maxdepth,
		rtip->rti_cut_nbins,
		rtip->rti_cut_maxlen,
		((double)rtip->rti_cut_totobj)/rtip->rti_cut_nbins );
		rt_hist_pr( &rtip->rti_hist_cellsize, "cut_tree: Number of solids per leaf cell");
		rt_hist_pr( &rtip->rti_hist_cutdepth, "cut_tree: Depth (height)"); 
	}

	if( rt_g.debug&DEBUG_PLOTBOX )  {
		/* Debugging code to plot cuts */
		if( (plotfp=fopen("rtcut.plot", "w"))!=NULL) {
			pdv_3space( plotfp, rtip->rti_pmin, rtip->rti_pmax );
			/* Plot all the cutting boxes */
			rt_plot_cut( plotfp, rtip, &rtip->rti_CutHead, 0 );
			(void)fclose(plotfp);
		}
	}
	if(rt_g.debug&DEBUG_CUTDETAIL)  {
		/* Produce a voluminous listing of the cut tree */
		rt_pr_cut( &rtip->rti_CutHead, 0 );
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
rt_ct_add( cutp, stp, min, max, depth )
register union cutter *cutp;
struct soltab *stp;	/* should be object handle & routine addr */
vect_t min, max;
int depth;
{
	if(rt_g.debug&DEBUG_CUTDETAIL)rt_log("rt_ct_add(x%x, %s, %d)\n",
		cutp, stp->st_name, depth);
	if( cutp->cut_type == CUT_CUTNODE )  {
		vect_t temp;

		/* Cut to the left of point */
		VMOVE( temp, max );
		temp[cutp->cn.cn_axis] = cutp->cn.cn_point;
		if( rt_ck_overlap( min, temp, stp ) )
			rt_ct_add( cutp->cn.cn_l, stp, min, temp, depth+1 );

		/* Cut to the right of point */
		VMOVE( temp, min );
		temp[cutp->cn.cn_axis] = cutp->cn.cn_point;
		if( rt_ck_overlap( temp, max, stp ) )
			rt_ct_add( cutp->cn.cn_r, stp, temp, max, depth+1 );
		return;
	}
	if( cutp->cut_type != CUT_BOXNODE )  {
		rt_log("rt_ct_add:  node type =x%x\n",cutp->cut_type);
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
			cutp->bn.bn_list = (struct soltab **)rt_malloc(
				cutp->bn.bn_maxlen * sizeof(struct soltab *),
				"rt_cut_extend: initial list alloc" );
		} else {
			register char *newlist;

			newlist = rt_malloc(
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
rt_ct_plan( cutp, depth )
union cutter	*cutp;
int		depth;
{
	int	axis;
	int	status[3];
	double	where[3];
	double	offcenter[3];
	int	best;
	double	bestoff;
	int	i;

	for( axis = X; axis <= Z; axis++ )  {
		status[axis] = rt_ct_assess(
			cutp, axis, &where[axis], &offcenter[axis] );
	}

	for(;;)  {
		best = -1;
		bestoff = INFINITY;
		for( i = X; i <= Z; i++ )  {
			axis = (depth + i) % 3;
			if( status[axis] <= 0 )  continue;
			if( offcenter[axis] >= bestoff )  continue;
			/* This one is better than previous ones */
			best = axis;
			bestoff = offcenter[axis];
		}

		if( best < 0 )  return(-1);	/* No cut is possible */

		if( rt_ct_box( cutp, best, where[best] ) > 0 )
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
 *	1 if box cut and cutp has become a CUT_CUTNODE;
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
rt_ct_box( cutp, axis, where )
register union cutter	*cutp;
register int		axis;
double			where;
{
	register union cutter	*rhs, *lhs;
	register int	i;

	if(rt_g.debug&DEBUG_CUTDETAIL)  {
		rt_log("rt_ct_box(x%x, %c) %g .. %g .. %g\n",
			cutp, "XYZ345"[axis],
			cutp->bn.bn_min[axis],
			where,
			cutp->bn.bn_max[axis]);
	}

	/* LEFT side */
	lhs = rt_ct_get();
	lhs->bn.bn_type = CUT_BOXNODE;
	VMOVE( lhs->bn.bn_min, cutp->bn.bn_min );
	VMOVE( lhs->bn.bn_max, cutp->bn.bn_max );
	lhs->bn.bn_max[axis] = where;
	lhs->bn.bn_len = 0;
	lhs->bn.bn_maxlen = cutp->bn.bn_len;
	lhs->bn.bn_list = (struct soltab **) rt_malloc(
		sizeof(struct soltab *) * lhs->bn.bn_maxlen,
		"rt_ct_box: left list" );
	for( i = cutp->bn.bn_len-1; i >= 0; i-- )  {
		if( !rt_ck_overlap(lhs->bn.bn_min, lhs->bn.bn_max,
		    cutp->bn.bn_list[i]))
			continue;
		lhs->bn.bn_list[lhs->bn.bn_len++] = cutp->bn.bn_list[i];
	}

	/* RIGHT side */
	rhs = rt_ct_get();
	rhs->bn.bn_type = CUT_BOXNODE;
	VMOVE( rhs->bn.bn_min, cutp->bn.bn_min );
	VMOVE( rhs->bn.bn_max, cutp->bn.bn_max );
	rhs->bn.bn_min[axis] = where;
	rhs->bn.bn_len = 0;
	rhs->bn.bn_maxlen = cutp->bn.bn_len;
	rhs->bn.bn_list = (struct soltab **) rt_malloc(
		sizeof(struct soltab *) * rhs->bn.bn_maxlen,
		"rt_ct_box: right list" );
	for( i = cutp->bn.bn_len-1; i >= 0; i-- )  {
		if( !rt_ck_overlap(rhs->bn.bn_min, rhs->bn.bn_max,
		    cutp->bn.bn_list[i]))
			continue;
		rhs->bn.bn_list[rhs->bn.bn_len++] = cutp->bn.bn_list[i];
	}

	if( rhs->bn.bn_len == cutp->bn.bn_len && lhs->bn.bn_len == cutp->bn.bn_len )  {
		/*
		 *  This cut operation did no good, release storage,
		 *  and let caller attempt something else.
		 */
		if(rt_g.debug&DEBUG_CUTDETAIL)  {
			rt_log("rt_ct_box:  no luck, len=%d\n",
				cutp->bn.bn_len );
		}
		rt_free( (char *)rhs->bn.bn_list, "rt_ct_box, rhs list");
		rt_free( (char *)lhs->bn.bn_list, "rt_ct_box, lhs list");
		rt_ct_free( rhs );
		rt_ct_free( lhs );
		return(0);		/* fail */
	}

	/* Success, convert callers box node into a cut node */
	rt_free( (char *)cutp->bn.bn_list, "rt_ct_box:  old list" );
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
rt_ck_overlap( min, max, stp )
register vect_t min, max;
register struct soltab *stp;
{
	if( rt_g.debug&DEBUG_BOXING )  {
		rt_log("rt_ck_overlap(%s)\n",stp->st_name);
		VPRINT("box min", min);
		VPRINT("sol min", stp->st_min);
		VPRINT("box max", max);
		VPRINT("sol max", stp->st_max);
	}
	if( stp->st_min[X] > max[X]  || stp->st_max[X] < min[X] )  goto fail;
	if( stp->st_min[Y] > max[Y]  || stp->st_max[Y] < min[Y] )  goto fail;
	if( stp->st_min[Z] > max[Z]  || stp->st_max[Z] < min[Z] )  goto fail;
	/* Need more sophistication here, per-object type */
	if( rt_g.debug&DEBUG_BOXING )  rt_log("rt_ck_overlap:  TRUE\n");
	return(1);
fail:
	if( rt_g.debug&DEBUG_BOXING )  rt_log("rt_ck_overlap:  FALSE\n");
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
rt_ct_optim( cutp, depth )
register union cutter *cutp;
int	depth;
{

	if( cutp->cut_type == CUT_CUTNODE )  {
		rt_ct_optim( cutp->cn.cn_l, depth+1 );
		rt_ct_optim( cutp->cn.cn_r, depth+1 );
		return;
	}
	if( cutp->cut_type != CUT_BOXNODE )  {
		rt_log("rt_ct_optim: bad node x%x\n", cutp->cut_type);
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
	/*
	 *  Attempt to make an optimal cut
	 */
	if( rt_ct_plan( cutp, depth ) < 0 )  {
		/* Unable to further subdivide this box node */
		return;
	}

	/* Box node is now a cut node, recurse */
	rt_ct_optim( cutp->cn.cn_l, depth+1 );
	rt_ct_optim( cutp->cn.cn_r, depth+1 );
}

/*
 *			R T _ C T _ G E T
 */
HIDDEN union cutter *
rt_ct_get()
{
	register union cutter *cutp;

	if( rt_g.rtg_CutFree == CUTTER_NULL )  {
		register int bytes;

		bytes = rt_byte_roundup(64*sizeof(union cutter));
		cutp = (union cutter *)rt_malloc(bytes," rt_ct_get");
		while( bytes >= sizeof(union cutter) )  {
			cutp->cut_forw = rt_g.rtg_CutFree;
			rt_g.rtg_CutFree = cutp++;
			bytes -= sizeof(union cutter);
		}
	}
	cutp = rt_g.rtg_CutFree;
	rt_g.rtg_CutFree = cutp->cut_forw;
	cutp->cut_forw = CUTTER_NULL;
	return(cutp);
}

/*
 *			R T _ C T _ F R E E
 */
HIDDEN void
rt_ct_free( cutp )
register union cutter *cutp;
{
	cutp->cut_forw = rt_g.rtg_CutFree;
	rt_g.rtg_CutFree = cutp;
}

/*
 *			R T _ P R _ C U T
 *  
 *  Print out a cut tree.
 */
void
rt_pr_cut( cutp, lvl )
register union cutter *cutp;
int lvl;			/* recursion level */
{
	register int i,j;

	rt_log("%.8x ", cutp);
	for( i=lvl; i>0; i-- )
		rt_log("   ");

	if( cutp == CUTTER_NULL )  {
		rt_log("Null???\n");
		return;
	}

	switch( cutp->cut_type )  {

	case CUT_CUTNODE:
		rt_log("CUT L %c < %f\n",
			"XYZ?"[cutp->cn.cn_axis],
			cutp->cn.cn_point );
		rt_pr_cut( cutp->cn.cn_l, lvl+1 );

		rt_log("%.8x ", cutp);
		for( i=lvl; i>0; i-- )
			rt_log("   ");
		rt_log("CUT R %c >= %f\n",
			"XYZ?"[cutp->cn.cn_axis],
			cutp->cn.cn_point );
		rt_pr_cut( cutp->cn.cn_r, lvl+1 );
		return;

	case CUT_BOXNODE:
		rt_log("BOX Contains %d solids (%d alloc):\n",
			cutp->bn.bn_len, cutp->bn.bn_maxlen );
		rt_log("        ");
		for( i=lvl; i>0; i-- )
			rt_log("   ");
		VPRINT(" min", cutp->bn.bn_min);
		rt_log("        ");
		for( i=lvl; i>0; i-- )
			rt_log("   ");
		VPRINT(" max", cutp->bn.bn_max);

		for( i=0; i < cutp->bn.bn_len; i++ )  {
			rt_log("        ");
			for( j=lvl; j>0; j-- )
				rt_log("   ");
			rt_log("    %s\n",
				cutp->bn.bn_list[i]->st_name);
		}
		return;

	default:
		rt_log("Unknown type=x%x\n", cutp->cut_type );
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
rt_fr_cut( cutp )
register union cutter *cutp;
{

	if( cutp == CUTTER_NULL )  {
		rt_log("rt_fr_cut NULL\n");
		return;
	}

	switch( cutp->cut_type )  {

	case CUT_CUTNODE:
		rt_fr_cut( cutp->cn.cn_l );
		rt_ct_free( cutp->cn.cn_l );
		cutp->cn.cn_l = CUTTER_NULL;

		rt_fr_cut( cutp->cn.cn_r );
		rt_ct_free( cutp->cn.cn_r );
		cutp->cn.cn_r = CUTTER_NULL;
		return;

	case CUT_BOXNODE:
		if( cutp->bn.bn_maxlen > 0 )
			rt_free( (char *)cutp->bn.bn_list, "cut_box list");
		return;

	default:
		rt_log("rt_fr_cut: Unknown type=x%x\n", cutp->cut_type );
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
	switch( cutp->cut_type )  {
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

	if( cutp->cut_type == CUT_CUTNODE )  {
		rt_ct_measure( rtip, cutp->cn.cn_l, len = (depth+1) );
		rt_ct_measure( rtip, cutp->cn.cn_r, len );
		return;
	}
	if( cutp->cut_type != CUT_BOXNODE )  {
		rt_log("rt_ct_measure: bad node x%x\n", cutp->cut_type);
		return;
	}
	/*
	 * BOXNODE (leaf)
	 */
	rtip->rti_cut_nbins++;
	rtip->rti_cut_totobj += (len = cutp->bn.bn_len);
	if( rtip->rti_cut_maxlen < len )
		rtip->rti_cut_maxlen = len;
	if( rtip->rti_cut_maxdepth < depth )
		rtip->rti_cut_maxdepth = depth;

	RT_HISTOGRAM_TALLY( &rtip->rti_hist_cellsize, len );
	RT_HISTOGRAM_TALLY( &rtip->rti_hist_cutdepth, depth ); 

}
