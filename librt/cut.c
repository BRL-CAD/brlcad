/*
 *  			C U T . C
 *  
 *  Cut space into lots of small boxes (RPPs actually).
 *  
 *  Before this can be done, the model max and min must have
 *  been computed -- no incremental cutting.
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

HIDDEN int rt_ck_overlap(), rt_ct_box();
HIDDEN void rt_ct_add(), rt_ct_optim(), rt_ct_free();
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

	rtip->rti_CutHead.bn.bn_type = CUT_BOXNODE;
	VMOVE( rtip->rti_CutHead.bn.bn_min, rtip->mdl_min );
	VMOVE( rtip->rti_CutHead.bn.bn_max, rtip->mdl_max );
	rtip->rti_CutHead.bn.bn_len = 0;
	rtip->rti_CutHead.bn.bn_maxlen = rtip->nsolids+1;
	rtip->rti_CutHead.bn.bn_list = (struct soltab **)rt_malloc(
		rtip->rti_CutHead.bn.bn_maxlen * sizeof(struct soltab *),
		"rt_cut_it: root list" );
	for(stp=rtip->HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw)  {
		if( stp->st_aradius >= INFINITY )
			continue;
		rtip->rti_CutHead.bn.bn_list[rtip->rti_CutHead.bn.bn_len++] = stp;
		if( rtip->rti_CutHead.bn.bn_len > rtip->rti_CutHead.bn.bn_maxlen )  {
			rt_log("rt_cut_it:  rtip->nsolids wrong, dropping solids\n");
			break;
		}
	}
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

	if(rt_g.debug&DEBUG_CUT) rt_pr_cut( &rtip->rti_CutHead, 0 );

	/* For plotting, build slightly enlarged model RPP, to
	 * allow rays clipped to the model RPP to be depicted,
	 * and compute a scale factor for using 4096 units.
	 * Always do this, because application debugging uses it too.
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

		/* These calcuations no longer needed, kept for compat. */
#define PL_MIN	(-32767L)
#define PL_MAX	 (32767L)
#define PL_DIFF	(PL_MAX-PL_MIN)
		diff = PL_DIFF / (rtip->rti_pmax[X] - rtip->rti_pmin[X]);
		f = PL_DIFF / (rtip->rti_pmax[Y] - rtip->rti_pmin[Y]);
		if( f < diff )  diff = f;
		f = PL_DIFF / (rtip->rti_pmax[Z] - rtip->rti_pmin[Z]);
		if( f < diff )  diff = f;
		rtip->rti_pconv = diff;
	}

	if( !(rt_g.debug&DEBUG_PLOTBOX) )  return;

	/* Debugging code to plot cuts, solid RRPs */
	if( (plotfp=fopen("rtcut.plot", "w"))!=NULL) {
		pdv_3space( plotfp, rtip->rti_pmin, rtip->rti_pmax );
		/* First, all the cutting boxes */
		rt_plot_cut( plotfp, rtip, &rtip->rti_CutHead, 0 );
		(void)fclose(plotfp);
	}
	if( (plotfp=fopen("rtrpp.plot", "w"))!=NULL) {
		/* Then, all the solid bounding boxes, in white */
		pdv_3space( plotfp, rtip->rti_pmin, rtip->rti_pmax );
		pl_color( plotfp, 255, 255, 255 );
		for(stp=rtip->HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw)  {
			if( stp->st_aradius >= INFINITY )
				continue;
			pdv_3box( plotfp, stp->st_min, stp->st_max );
		}
		(void)fclose(plotfp);
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
	if(rt_g.debug&DEBUG_CUT)rt_log("rt_ct_add(x%x, %s, %d)\n",
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
				"rt_ct_add: initial list alloc" );
		} else {
			register char *newlist;

			newlist = rt_malloc(
				sizeof(struct soltab *) * cutp->bn.bn_maxlen * 2,
				"rt_ct_add: list extend" );
			bcopy( cutp->bn.bn_list, newlist,
				cutp->bn.bn_maxlen * sizeof(struct soltab *));
			cutp->bn.bn_maxlen *= 2;
			rt_free( (char *)cutp->bn.bn_list,
				"rt_ct_add: list extend (old list)");
			cutp->bn.bn_list = (struct soltab **)newlist;
		}
	}
	cutp->bn.bn_list[cutp->bn.bn_len++] = stp;
}

/*
 *			R T _ C T _ B O X
 *  
 *  Cut a box node with a plane, generating a cut node and two box nodes.
 *  NOTE that this routine guarantees that each new box node will
 *  be able to hold at least one more item in it's list.
 *  Returns 1 if box cut and cutp has become a CUT_CUTNODE;
 *  returns 0 if this cut didn't help.
 */
HIDDEN int
rt_ct_box( cutp, axis )
register union cutter *cutp;
register int axis;
{
	register struct boxnode *bp;
	auto union cutter oldbox;
	auto double d_close;		/* Closest distance from midpoint */
	auto double pt_close;		/* Point closest to midpoint */
	auto double middle;		/* midpoint */
	auto double d;
	register int i;

	if(rt_g.debug&DEBUG_CUT)rt_log("rt_ct_box(x%x, %c)\n",cutp,"XYZ345"[axis]);

	/*  In absolute terms, each box must be at least 1mm wide after cut. */
	if( cutp->bn.bn_max[axis]-cutp->bn.bn_min[axis] < 2.0 )
		return(0);
	oldbox = *cutp;		/* struct copy */

	/*
	 *  Split distance between min and max in half.
	 *  Find the closest edge of a solid's bounding RPP
	 *  to the mid-point, and split there.
	 *  This should ordinarily guarantee that at least one side of the
	 *  cut has one less item in it.
	 */
	pt_close = oldbox.bn.bn_min[axis];
	middle = (pt_close + oldbox.bn.bn_max[axis]) * 0.5;
	d_close = middle - pt_close;
	for( i=0; i < oldbox.bn.bn_len; i++ )  {
		d = oldbox.bn.bn_list[i]->st_min[axis] - middle;
		if( d < 0 )  d = (-d);
		if( d < d_close )  {
			d_close = d;
			pt_close = oldbox.bn.bn_list[i]->st_min[axis]-0.1;
		}
		d = oldbox.bn.bn_list[i]->st_max[axis] - middle;
		if( d < 0 )  d = (-d);
		if( d < d_close )  {
			d_close = d;
			pt_close = oldbox.bn.bn_list[i]->st_max[axis]+0.1;
		}
	}
	if( pt_close <= oldbox.bn.bn_min[axis] ||
	    pt_close >= oldbox.bn.bn_max[axis] )
		return(0);	/* not reasonable */

	if( pt_close - oldbox.bn.bn_min[axis] <= 1.0 ||
	    oldbox.bn.bn_max[axis] - pt_close <= 1.0 )
		return(0);	/* cut will be too small */

	/* We are going to cut -- convert caller's node type */
	if(rt_g.debug&DEBUG_CUT)rt_log("rt_ct_box(x%x) [%g,%g,%g]\n",
		cutp,
		oldbox.bn.bn_min[axis], pt_close, oldbox.bn.bn_max[axis]);
	cutp->cut_type = CUT_CUTNODE;
	cutp->cn.cn_axis = axis;
	cutp->cn.cn_point = pt_close;

	/* LEFT side */
	cutp->cn.cn_l = rt_ct_get();
	bp = &(cutp->cn.cn_l->bn);
	bp->bn_type = CUT_BOXNODE;
	VMOVE( bp->bn_min, oldbox.bn.bn_min );
	VMOVE( bp->bn_max, oldbox.bn.bn_max );
	bp->bn_max[axis] = cutp->cn.cn_point;
	bp->bn_len = 0;
	bp->bn_maxlen = oldbox.bn.bn_len + 1;
	bp->bn_list = (struct soltab **) rt_malloc(
		sizeof(struct soltab *) * bp->bn_maxlen,
		"rt_ct_box: left list" );
	for( i=0; i < oldbox.bn.bn_len; i++ )  {
		if( !rt_ck_overlap(bp->bn_min, bp->bn_max, oldbox.bn.bn_list[i]))
			continue;
		bp->bn_list[bp->bn_len++] = oldbox.bn.bn_list[i];
	}

	/* RIGHT side */
	cutp->cn.cn_r = rt_ct_get();
	bp = &(cutp->cn.cn_r->bn);
	bp->bn_type = CUT_BOXNODE;
	VMOVE( bp->bn_min, oldbox.bn.bn_min );
	VMOVE( bp->bn_max, oldbox.bn.bn_max );
	bp->bn_min[axis] = cutp->cn.cn_point;
	bp->bn_len = 0;
	bp->bn_maxlen = oldbox.bn.bn_len + 1;
	bp->bn_list = (struct soltab **) rt_malloc(
		sizeof(struct soltab *) * bp->bn_maxlen,
		"rt_ct_box: right list" );
	for( i=0; i < oldbox.bn.bn_len; i++ )  {
		if( !rt_ck_overlap(bp->bn_min, bp->bn_max, oldbox.bn.bn_list[i]))
			continue;
		bp->bn_list[bp->bn_len++] = oldbox.bn.bn_list[i];
	}
	rt_free( (char *)oldbox.bn.bn_list, "rt_ct_box:  old list" );
	oldbox.bn.bn_list = (struct soltab **)0;
	return(1);
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
int
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
int depth;
{
	register int oldlen;
	register int axis;

	if( cutp->cut_type == CUT_CUTNODE )  {
		rt_ct_optim( cutp->cn.cn_l, depth+1 );
		rt_ct_optim( cutp->cn.cn_r, depth+1 );
		return;
	}
	if( cutp->cut_type != CUT_BOXNODE )  {
		rt_log("rt_ct_optim: bad node\n");
		return;
	}
	/*
	 * BOXNODE
	 */
	if( cutp->bn.bn_len <= 1 )  return;	/* optimal */
	if( depth > rt_cutDepth )  return;		/* too deep */
	/* Attempt to subdivide finer than rt_cutLen near treetop */
/**** THIS STATEMENT MUST GO ****/
	if( depth >= 12 && cutp->bn.bn_len <= rt_cutLen )
		return;				/* Fine enough */
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
	if( rt_ct_box( cutp, axis ) == 0 )  {
		if( rt_ct_box( cutp, AXIS(depth+1) ) == 0 )
			return;	/* hopeless */
	}
	if( cutp->cn.cn_l->bn.bn_len < oldlen ||
	    cutp->cn.cn_r->bn.bn_len < oldlen )  {
		rt_ct_optim( cutp->cn.cn_l, depth+1 );
		rt_ct_optim( cutp->cn.cn_r, depth+1 );
	}
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
 *			R T _ V C L I P
 *
 *  Clip a ray against a rectangular parallelpiped (RPP)
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
static int
rt_vclip( a, b, min, max )
vect_t a, b;
register fastf_t *min, *max;
{
	static vect_t diff;
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
