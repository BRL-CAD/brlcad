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

int rt_cutLen = 3;			/* normal limit on number objs per box node */
int rt_cutDepth = 32;		/* normal limit on depth of cut tree */

HIDDEN int rt_ck_overlap(), rt_ct_box();
HIDDEN void rt_ct_add(), rt_ct_optim(), rt_ct_free();
HIDDEN union cutter *rt_ct_get();

#define AXIS(depth)	((depth)%3)	/* cuts: X, Y, Z, repeat */

HIDDEN void rt_plot_cut();
static void space3(), plot_color(), move3(), cont3();
static FILE *plotfp;

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
rt_log("Cut: Tree Depth=%d, Leaf Len=%d\n", rt_cutDepth, rt_cutLen );
	rt_ct_optim( &rtip->rti_CutHead, 0 );
	if(rt_g.debug&DEBUG_CUT) rt_pr_cut( &rtip->rti_CutHead, 0 );
	if(rt_g.debug&DEBUG_PLOTBOX && (plotfp=fopen("rtbox.plot", "w"))!=NULL) {
		space3( 0,0,0, 4096, 4096, 4096);
		/* First, all the cutting boxes */
		rt_plot_cut( rtip, &rtip->rti_CutHead, 0 );
		/* Then, all the solid bounding boxes, in white */
		plot_color( 255, 255, 255 );
		for(stp=rtip->HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw)  {
			rt_draw_box( rtip, stp->st_min, stp->st_max );
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
		return(0);	/* not worthwhile */

	/* We are going to cut -- convert caller's node type */
	if(rt_g.debug&DEBUG_CUT)rt_log("rt_ct_box(x%x) %e..%e..%e\n",
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
rt_plot_cut( rtip, cutp, lvl )
struct rt_i		*rtip;
register union cutter	*cutp;
int			lvl;
{
	switch( cutp->cut_type )  {
	case CUT_CUTNODE:
		rt_plot_cut( rtip, cutp->cn.cn_l, lvl+1 );
		rt_plot_cut( rtip, cutp->cn.cn_r, lvl+1 );
		return;
	case CUT_BOXNODE:
		/* Should choose color based on lvl, need a table */
		plot_color( (AXIS(lvl)==0)?255:0,
			(AXIS(lvl)==1)?255:0,
			(AXIS(lvl)==2)?255:0 );
		rt_draw_box( rtip, cutp->bn.bn_min, cutp->bn.bn_max );
		return;
	}
	return;
}

/*
 *  			R T _ D R A W _ B O X
 *  
 *  Arrange to efficiently draw the edges of a box (RPP).
 *  Call on UNIX-Plot to do the actual drawing, which
 *  will fall out on plotfp.
 */
void
rt_draw_box( rtip, a, b )
register struct rt_i	*rtip;
register vect_t		a, b;
{
	int ax, ay, az;
	int bx, by, bz;
	double conv, f;

	conv = 4096.0 / (rtip->mdl_max[X] - rtip->mdl_min[X]);
	f = 4096.0 / (rtip->mdl_max[Y] - rtip->mdl_min[Y]);
	if( f < conv )  conv = f;
	f = 4096.0 / (rtip->mdl_max[Z] - rtip->mdl_min[Z]);
	if( f < conv )  conv = f;

	ax =	(a[X] - rtip->mdl_min[X])*conv;
	ay =	(a[Y] - rtip->mdl_min[Y])*conv;
	az =	(a[Z] - rtip->mdl_min[Z])*conv;
	bx =	(b[X] - rtip->mdl_min[X])*conv;
	by =	(b[Y] - rtip->mdl_min[Y])*conv;
	bz =	(b[Z] - rtip->mdl_min[Z])*conv;

	move3( ax, ay, az );
	cont3( bx, ay, az );
	cont3( bx, by, az );
	cont3( ax, by, az );

	move3( bx, ay, az );
	cont3( bx, ay, bz );
	cont3( bx, by, bz );
	cont3( bx, by, az );

	move3( bx, by, bz );
	cont3( ax, by, bz );
	cont3( ax, ay, bz );
	cont3( bx, ay, bz );

	move3( ax, by, bz );
	cont3( ax, by, az );
	cont3( ax, ay, az );
	cont3( ax, ay, bz );
}


/*
 *  Generate UNIX Plot in 3-D.
 *  cf. Doug Gwyn's Sys-V-standard (but-only-at-BRL) package,
 *  so we have to include the necessary sources here.
 */
#define putsi(a)	putc(a&0377,plotfp); putc((a>>8)&0377,plotfp)
static void
space3(x0,y0,z0,x1,y1,z1){
	putc('S',plotfp);
	putsi(x0);
	putsi(y0);
	putsi(z0);
	putsi(x1);
	putsi(y1);
	putsi(z1);
}
/*
 *	plot_color -- deposit color selection in UNIX plot output file
 *	04-Jan-1984	D A Gwyn
 */
static void
plot_color( r, g, b )
int	r, g, b;		/* color components, 0..255 */
{
	putc( 'C', plotfp );
	putc( r, plotfp );
	putc( g, plotfp );
	putc( b, plotfp );
}
static void
move3(xi,yi,zi){
	putc('M',plotfp);
	putsi(xi);
	putsi(yi);
	putsi(zi);
}
static void
cont3(xi,yi,zi){
	putc('N',plotfp);
	putsi(xi);
	putsi(yi);
	putsi(zi);
}
