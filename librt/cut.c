/*
 *  			C U T . C
 *  
 *  Cut space into lots of small boxes (RPPs actually).
 *  
 *  Before this can be done, the model max and min must have
 *  been computed -- no incremental cutting.
 */
#include <stdio.h>
#include <math.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "raytrace.h"
#include "debug.h"
#include "cut.h"

int CutLen = 3;			/* normal limit on number objs per box node */
int CutDepth = 32;		/* normal limit on depth of cut tree */

union cutter CutHead;		/* Global head of cutting tree */
union cutter *CutFree;		/* Head of freelist */

extern int ck_overlap();
extern void cut_it(), pr_cut();
HIDDEN void cut_add(), cut_box(), cut_optim();
HIDDEN union cutter *cut_get();

#define AXIS(depth)	((depth>>1)%3)	/* cuts: X, X, Y, Y, Z, Z, repeat */

static FILE *plotfp;

/*
 *  			C U T _ I T
 *  
 *  Go through all the solids in the model, given the model mins and maxes,
 *  and generate a cutting tree.  A strategy better than incrementally
 *  cutting each solid is to build a box node which contains everything
 *  in the model, and optimize it.
 */
void
cut_it()  {
	register struct soltab *stp;

	CutHead.bn.bn_type = CUT_BOXNODE;
	VMOVE( CutHead.bn.bn_min, mdl_min );
	VMOVE( CutHead.bn.bn_max, mdl_max );
	CutHead.bn.bn_len = 0;
	CutHead.bn.bn_maxlen = nsolids+1;
	CutHead.bn.bn_list = (struct soltab **)vmalloc(
		CutHead.bn.bn_maxlen * sizeof(struct soltab *),
		"cut_it: root list" );
	for(stp=HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw)  {
		CutHead.bn.bn_list[CutHead.bn.bn_len++] = stp;
		if( CutHead.bn.bn_len > CutHead.bn.bn_maxlen )  {
			rtlog("cut_it:  nsolids wrong, dropping solids\n");
			break;
		}
	}
	/*  Dynamic decisions on tree limits.
	 *  Note that there will be (2**CutDepth)*CutLen leaf slots,
	 *  but solids will typically span several leaves.
	 */
	CutLen = (int)log((double)nsolids);
	if( CutLen < 3 )  CutLen = 3;
	CutDepth = (int)(2 * log((double)nsolids));	/* ln ~= log2 */
	if( CutDepth < 9 )  CutDepth = 9;
	if( CutDepth > 24 )  CutDepth = 24;		/* !! */
rtlog("Cut: Tree Depth=%d, Leaf Len=%d\n", CutDepth, CutLen );
	cut_optim( &CutHead, 0 );
	if(debug&DEBUG_CUT) pr_cut( &CutHead, 0 );
	if(debug&DEBUG_PLOTBOX && (plotfp=fopen("rtbox.plot", "w"))!=NULL) {
		space3( 0,0,0, 4096, 4096, 4096);
		/* First, all the cutting boxes */
		plot_cut( &CutHead, 0 );
		/* Then, all the solid bounding boxes, in white */
		plot_color( 255, 255, 255 );
		for(stp=HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw)  {
			draw_box( stp->st_min, stp->st_max );
		}
		(void)fclose(plotfp);
	}
}

/*
 *  			C U T _ A D D
 *  
 *  Add a solid to the cut tree, extending the tree as necessary,
 *  but without being paranoid about obeying the CutLen limits,
 *  so as to retain O(depth) performance.
 */
HIDDEN void
cut_add( cutp, stp, min, max, depth )
register union cutter *cutp;
struct soltab *stp;	/* should be object handle & routine addr */
vect_t min, max;
int depth;
{
	if(debug&DEBUG_CUT)rtlog("cut_add(x%x, %s, %d)\n",
		cutp, stp->st_name, depth);
	if( cutp->cut_type == CUT_CUTNODE )  {
		vect_t temp;

		/* Cut to the left of point */
		VMOVE( temp, max );
		temp[cutp->cn.cn_axis] = cutp->cn.cn_point;
		if( ck_overlap( min, temp, stp ) )
			cut_add( cutp->cn.cn_l, stp, min, temp, depth+1 );

		/* Cut to the right of point */
		VMOVE( temp, min );
		temp[cutp->cn.cn_axis] = cutp->cn.cn_point;
		if( ck_overlap( temp, max, stp ) )
			cut_add( cutp->cn.cn_r, stp, temp, max, depth+1 );
		return;
	}
	if( cutp->cut_type != CUT_BOXNODE )  {
		rtlog("cut_add:  node type =x%x\n",cutp->cut_type);
		return;
	}
	/* BOX NODE */
	if( cutp->bn.bn_len >= CutLen && depth <= CutDepth )  {
		register struct boxnode *bp;

		/*
		 *  Too many objects in box, cut in half.
		 *  Cut box node, converting into a CUTNODE and
		 *  two BOXNODES.  Then, just tack on new object,
		 *  without trying to limit size or doing recursion.
		 */
		cut_box( cutp, AXIS(depth) );
		/* cutp now points to a CUTNODE */

		/* add extra object to left, without recursing */
		bp = &(cutp->cn.cn_l->bn);
		if( ck_overlap( bp->bn_min, bp->bn_max, stp ) )
			bp->bn_list[bp->bn_len++] = stp;

		bp = &(cutp->cn.cn_r->bn);
		/* add extra object to right, without recursing */
		if( ck_overlap( bp->bn_min, bp->bn_max, stp ) )
			bp->bn_list[bp->bn_len++] = stp;
		return;
	}

	/* Just add to list at this box node */
	if( cutp->bn.bn_len >= cutp->bn.bn_maxlen )  {
		/* Need to get more space in list.  */
		if( cutp->bn.bn_maxlen <= 0 )  {
			/* Initial allocation */
			if( CutLen > 6 )
				cutp->bn.bn_maxlen = CutLen;
			else
				cutp->bn.bn_maxlen = 6;
			cutp->bn.bn_list = (struct soltab **)vmalloc(
				cutp->bn.bn_maxlen * sizeof(struct soltab *),
				"cut_add: initial list alloc" );
		} else {
			char *newlist;
			newlist = vmalloc(
				sizeof(struct soltab *) * cutp->bn.bn_maxlen * 2,
				"cut_add: list extend" );
			bcopy( cutp->bn.bn_list, newlist,
				cutp->bn.bn_maxlen * sizeof(struct soltab *));
			cutp->bn.bn_maxlen *= 2;
			vfree( (char *)cutp->bn.bn_list,
				"cut_add: list extend (old list)");
			cutp->bn.bn_list = (struct soltab **)newlist;
		}
	}
	cutp->bn.bn_list[cutp->bn.bn_len++] = stp;
}

/*
 *  			C U T _ B O X
 *  
 *  Cut a box node with a plane, generating a cut node and two box nodes.
 *  NOTE that this routine guarantees that each new box node will
 *  be able to hold at least one more item in it's list.
 */
HIDDEN void
cut_box( cutp, axis )
register union cutter *cutp;
register int axis;
{
	register struct boxnode *bp;
	auto union cutter oldbox;
	auto double d_close;		/* Closest distance from midpoint */
	auto double pt_close;		/* Point closest to midpoint */
	auto double d;
	register int i;

	if(debug&DEBUG_CUT)rtlog("cut_box(x%x, %c)\n",cutp,"XYZ345"[axis]);
	oldbox = *cutp;		/* struct copy */
	cutp->cut_type = CUT_CUTNODE;
	cutp->cn.cn_axis = axis;
	/*
	 *  Split distance between min and max in half.
	 *  Find the closest edge of a solid's bounding RPP
	 *  to the mid-point, and split there.
	 *  This should ordinarily guarantee that each side of the
	 *  cut has one less item in it.
	 */
	cutp->cn.cn_point =
		(oldbox.bn.bn_max[axis]+oldbox.bn.bn_min[axis]) * 0.5;
	pt_close = oldbox.bn.bn_min[axis];
	d_close = cutp->cn.cn_point - pt_close;
	for( i=0; i < oldbox.bn.bn_len; i++ )  {
		d = oldbox.bn.bn_list[i]->st_min[axis] - cutp->cn.cn_point;
		if( d < 0 )  d = (-d);
		if( d < d_close )  {
			d_close = d;
			pt_close = oldbox.bn.bn_list[i]->st_min[axis]+EPSILON;
		}
		d = oldbox.bn.bn_list[i]->st_max[axis] - cutp->cn.cn_point;
		if( d < 0 )  d = (-d);
		if( d < d_close )  {
			d_close = d;
			pt_close = oldbox.bn.bn_list[i]->st_max[axis]+EPSILON;
		}
	}
	if( pt_close != oldbox.bn.bn_min[axis] )
		cutp->cn.cn_point = pt_close;

	/* LEFT side */
	cutp->cn.cn_l = cut_get();
	bp = &(cutp->cn.cn_l->bn);
	bp->bn_type = CUT_BOXNODE;
	VMOVE( bp->bn_min, oldbox.bn.bn_min );
	VMOVE( bp->bn_max, oldbox.bn.bn_max );
	bp->bn_max[axis] = cutp->cn.cn_point;
	bp->bn_len = 0;
	bp->bn_maxlen = oldbox.bn.bn_len + 1;
	bp->bn_list = (struct soltab **) vmalloc(
		sizeof(struct soltab *) * bp->bn_maxlen,
		"cut_box: left list" );
	for( i=0; i < oldbox.bn.bn_len; i++ )  {
		if( !ck_overlap(bp->bn_min, bp->bn_max, oldbox.bn.bn_list[i]))
			continue;
		bp->bn_list[bp->bn_len++] = oldbox.bn.bn_list[i];
	}

	/* RIGHT side */
	cutp->cn.cn_r = cut_get();
	bp = &(cutp->cn.cn_r->bn);
	bp->bn_type = CUT_BOXNODE;
	VMOVE( bp->bn_min, oldbox.bn.bn_min );
	VMOVE( bp->bn_max, oldbox.bn.bn_max );
	bp->bn_min[axis] = cutp->cn.cn_point;
	bp->bn_len = 0;
	bp->bn_maxlen = oldbox.bn.bn_len + 1;
	bp->bn_list = (struct soltab **) vmalloc(
		sizeof(struct soltab *) * bp->bn_maxlen,
		"cut_box: right list" );
	for( i=0; i < oldbox.bn.bn_len; i++ )  {
		if( !ck_overlap(bp->bn_min, bp->bn_max, oldbox.bn.bn_list[i]))
			continue;
		bp->bn_list[bp->bn_len++] = oldbox.bn.bn_list[i];
	}
	vfree( (char *)oldbox.bn.bn_list, "cut_box:  old list" );
}

/*
 *			C K _ O V E R L A P
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
ck_overlap( min, max, stp )
register vect_t min, max;
register struct soltab *stp;
{
	if( debug&DEBUG_BOXING )  {
		rtlog("ck_overlap(%s)\n",stp->st_name);
		VPRINT("box min", min);
		VPRINT("sol min", stp->st_min);
		VPRINT("box max", max);
		VPRINT("sol max", stp->st_max);
	}
	if( stp->st_min[X] > max[X]  || stp->st_max[X] < min[X] )  goto fail;
	if( stp->st_min[Y] > max[Y]  || stp->st_max[Y] < min[Y] )  goto fail;
	if( stp->st_min[Z] > max[Z]  || stp->st_max[Z] < min[Z] )  goto fail;
	/* Need more sophistication here, per-object type */
	if( debug&DEBUG_BOXING )  rtlog("ck_overlap:  TRUE\n");
	return(1);
fail:
	if( debug&DEBUG_BOXING )  rtlog("ck_overlap:  FALSE\n");
	return(0);
}

/*
 *  			C U T _ O P T I M
 *  
 *  Optimize a cut tree.  Work on nodes which are over the pre-set limits,
 *  subdividing until either the limit on tree depth runs out, or until
 *  subdivision no longer gives different results, which could easily be
 *  the case when several solids involved in a CSG operation overlap in
 *  space.
 */
HIDDEN void
cut_optim( cutp, depth )
register union cutter *cutp;
int depth;
{
	register int oldlen;

	if( cutp->cut_type == CUT_CUTNODE )  {
		cut_optim( cutp->cn.cn_l, depth+1 );
		cut_optim( cutp->cn.cn_r, depth+1 );
		return;
	}
	if( cutp->cut_type != CUT_BOXNODE )  {
		rtlog("cut_optim: bad node\n");
		return;
	}
	/*
	 * BOXNODE
	 */
	if( cutp->bn.bn_len <= 1 )  return;	/* optimal */
	if( depth > CutDepth )  return;		/* too deep */
	/* Attempt to subdivide finer than CutLen near treetop */
	if( depth >= 12 && cutp->bn.bn_len <= CutLen )
		return;				/* Fine enough */
	/* Keep subdividing until things don't get any better. */
	/* Really we might want to proceed for 2-3 levels, but... */
	oldlen = cutp->bn.bn_len;
	cut_box( cutp, AXIS(depth) );
	if( cutp->cn.cn_l->bn.bn_len < oldlen ||
	    cutp->cn.cn_r->bn.bn_len < oldlen )  {
		cut_optim( cutp->cn.cn_l, depth+1 );
		cut_optim( cutp->cn.cn_r, depth+1 );
	}
}

/*
 *  			C U T _ G E T
 */
HIDDEN union cutter *
cut_get()
{
	register union cutter *cutp;

	if( CutFree == CUTTER_NULL )  {
		register int bytes;

		bytes = byte_roundup(64*sizeof(union cutter));
		cutp = (union cutter *)vmalloc(bytes," cut_get");
		while( bytes >= sizeof(union cutter) )  {
			cutp->cut_forw = CutFree;
			CutFree = cutp++;
			bytes -= sizeof(union cutter);
		}
	}
	cutp = CutFree;
	CutFree = cutp->cut_forw;
	return(cutp);
}

/*
 *  			P R _ C U T
 *  
 *  Print out a cut tree.
 */
void
pr_cut( cutp, lvl )
register union cutter *cutp;
int lvl;			/* recursion level */
{
	register int i,j;

	rtlog("%.8x ", cutp);
	for( i=lvl; i>0; i-- )
		rtlog("   ");

	if( cutp == CUTTER_NULL )  {
		rtlog("Null???\n");
		return;
	}

	switch( cutp->cut_type )  {

	case CUT_CUTNODE:
		rtlog("CUT L %c < %f\n",
			"XYZ?"[cutp->cn.cn_axis],
			cutp->cn.cn_point );
		pr_cut( cutp->cn.cn_l, lvl+1 );

		rtlog("%.8x ", cutp);
		for( i=lvl; i>0; i-- )
			rtlog("   ");
		rtlog("CUT R %c >= %f\n",
			"XYZ?"[cutp->cn.cn_axis],
			cutp->cn.cn_point );
		pr_cut( cutp->cn.cn_r, lvl+1 );
		return;

	case CUT_BOXNODE:
		rtlog("BOX Contains %d solids (%d alloc):\n",
			cutp->bn.bn_len, cutp->bn.bn_maxlen );
		rtlog("        ");
		for( i=lvl; i>0; i-- )
			rtlog("   ");
		VPRINT(" min", cutp->bn.bn_min);
		rtlog("        ");
		for( i=lvl; i>0; i-- )
			rtlog("   ");
		VPRINT(" max", cutp->bn.bn_max);

		for( i=0; i < cutp->bn.bn_len; i++ )  {
			rtlog("        ");
			for( j=lvl; j>0; j-- )
				rtlog("   ");
			rtlog("    %s\n",
				cutp->bn.bn_list[i]->st_name);
		}
		return;

	default:
		rtlog("Unknown type=x%x\n", cutp->cut_type );
		break;
	}
	return;
}

/*
 *  			P L O T _ C U T
 */
plot_cut( cutp, lvl )
register union cutter *cutp;
int lvl;
{
	switch( cutp->cut_type )  {
	case CUT_CUTNODE:
		plot_cut( cutp->cn.cn_l, lvl+1 );
		plot_cut( cutp->cn.cn_r, lvl+1 );
		return;
	case CUT_BOXNODE:
		/* Should choose color based on lvl, need a table */
		plot_color( (AXIS(lvl)==0)?255:0,
			(AXIS(lvl)==1)?255:0,
			(AXIS(lvl)==2)?255:0 );
		draw_box( cutp->bn.bn_min, cutp->bn.bn_max );
		return;
	}
	return;
}

/*
 *  			D R A W _ B O X
 *  
 *  Arrange to efficiently draw the edges of a box (RPP).
 *  Call on UNIX-Plot to do the actual drawing, which
 *  will fall out on plotfp.
 */
draw_box( a, b )
register vect_t a, b;
{
	int ax, ay, az;
	int bx, by, bz;
	double conv, f;
	conv = 4096.0 / (mdl_max[X]-mdl_min[X]);
	f = 4096.0 / (mdl_max[Y]-mdl_min[Y]);
	if( f < conv )  conv = f;
	f = 4096.0 / (mdl_max[Z]-mdl_min[Z]);
	if( f < conv )  conv = f;

	ax =	(a[X]-mdl_min[X])*conv;
	ay =	(a[Y]-mdl_min[Y])*conv;
	az =	(a[Z]-mdl_min[Z])*conv;
	bx =	(b[X]-mdl_min[X])*conv;
	by =	(b[Y]-mdl_min[Y])*conv;
	bz =	(b[Z]-mdl_min[Z])*conv;

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
plot_color( r, g, b )
int	r, g, b;		/* color components, 0..255 */
{
	putc( 'C', plotfp );
	putc( r, plotfp );
	putc( g, plotfp );
	putc( b, plotfp );
}
move3(xi,yi,zi){
	putc('M',plotfp);
	putsi(xi);
	putsi(yi);
	putsi(zi);
}
cont3(xi,yi,zi){
	putc('N',plotfp);
	putsi(xi);
	putsi(yi);
	putsi(zi);
}
