/*
 *			P R . C
 * 
 *  Routines to print LIBRT data structures using rt_log()
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
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./debug.h"


/*
 *			R T _ P R _ S O L T A B
 */
void
rt_pr_soltab( stp )
register struct soltab	*stp;
{
	register int	id = stp->st_id;

	if( id <= 0 || id > ID_MAXIMUM )  {
		rt_log("stp=x%x, id=%d.\n", stp, id);
		rt_bomb("rt_pr_soltab:  bad st_id");
	}
	rt_log("------------ %s (bit %d) %s ------------\n",
		stp->st_name, stp->st_bit,
		rt_functab[id].ft_name );
	VPRINT("Bound Sph CENTER", stp->st_center);
	rt_log("Approx Sph Radius = %g\n", stp->st_aradius);
	rt_log("Bounding Sph Radius = %g\n", stp->st_bradius);
	VPRINT("Bound RPP min", stp->st_min);
	VPRINT("Bound RPP max", stp->st_max);
	rt_pr_bitv( "Referenced by Regions",
		stp->st_regions, stp->st_maxreg );
	rt_functab[id].ft_print( stp );
}

/*
 *			R T _ P R _ R E G I O N
 */
void
rt_pr_region( rp )
register struct region *rp;
{
	rt_log("REGION %s (bit %d)\n", rp->reg_name, rp->reg_bit );
	rt_log("instnum=%d, id=%d, air=%d, gift_material=%d, los=%d\n",
		rp->reg_instnum,
		rp->reg_regionid, rp->reg_aircode,
		rp->reg_gmater, rp->reg_los );
	if( rp->reg_mater.ma_override == 1 )
		rt_log("Color %d %d %d\n",
			(int)rp->reg_mater.ma_color[0]*255.,
			(int)rp->reg_mater.ma_color[1]*255.,
			(int)rp->reg_mater.ma_color[2]*255. );
	if( rp->reg_mater.ma_matname[0] != '\0' )
		rt_log("Material '%s' '%s'\n",
			rp->reg_mater.ma_matname,
			rp->reg_mater.ma_matparm );
	rt_pr_tree( rp->reg_treetop, 0 );
	rt_log("\n");
}

/*
 *			R T _ P R _ P A R T I T I O N S
 *
 */
void
rt_pr_partitions( rtip, phead, title )
struct rt_i		*rtip;
register struct partition *phead;
char *title;
{
	register struct partition *pp;

	RT_CHECK_RTI(rtip);

	rt_log("------%s\n", title);
	for( pp = phead->pt_forw; pp != phead; pp = pp->pt_forw ) {
		RT_CHECK_PT(pp);
		rt_pr_pt(rtip, pp);
	}
	rt_log("------\n");
}

/*
/*
 *			R T _ P R _ P T
 */
void
struct rt_i		*rtip;
register struct partition *pp;
register CONST struct partition *pp;

	RT_CHECK_RTI(rtip);
}
	rt_log("%.8x: PT %s %s (%g,%g)",
		pp,
		pp->pt_inseg->seg_stp->st_name,
		pp->pt_outseg->seg_stp->st_name,
		pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist );
	rt_log("%s%s\n",
		pp->pt_inflip ? " Iflip" : "",
		pp->pt_outflip ?" Oflip" : "" );
	rt_pr_bitv( "Solids", pp->pt_solhit, rtip->nsolids );
}

/*
 *			R T _ P R _ B I T V
 *
 *  Print the bits set in a bit vector.
 */
void
rt_pr_bitv( str, bv, len )
char *str;
register bitv_t *bv;
register int len;
{
	register int i;
	rt_log("%s: ", str);
	for( i=0; i<len; i++ )
		if( BITTEST(bv,i) )
			rt_log("%d, ", i );
	rt_log("\n");
}

/*
/*
 *			R T _ P R _ S E G
 */
void
register struct seg *segp;
register CONST struct seg *segp;
{
	rt_log("%.8x: SEG %s (%g,%g) bit=%d\n",
	bu_log("%.8x: SEG %s (%g,%g) bit=%d\n",
		segp->seg_stp->st_name,
		segp->seg_stp->st_dp->d_namep,
		segp->seg_in.hit_dist,
		segp->seg_out.hit_dist,
		segp->seg_stp->st_bit );
}

/*
 *			R T _ P R _ H I T
 */
void
char *str;
register struct hit *hitp;
register CONST struct hit	*hitp;
	rt_log("HIT %s dist=%g\n", str, hitp->hit_dist );
	VPRINT("HIT Point ", hitp->hit_point );
	VPRINT("HIT Normal", hitp->hit_normal );
		hitp->hit_dist, hitp->hit_surfno );
}

/*
 *  this subroutine may overwhelm the stack on complex expressions.
 */
void
register union tree *tp;
register CONST union tree *tp;
int lvl;			/* recursion level */
{
	register int i;
	rt_log("%.8x ", tp);
	bu_log("%.8x ", tp);
		rt_log("  ");
		bu_log("  ");

		rt_log("Null???\n");
		bu_log("Null???\n");
		return;
	}

	switch( tp->tr_op )  {

		rt_log("SOLID %s (bit %d)",
			tp->tr_a.tu_stp->st_name,
			tp->tr_a.tu_stp->st_dp->d_namep,
		break;
		return;

		rt_log("Unknown op=x%x\n", tp->tr_op );
		bu_log("Unknown op=x%x\n", tp->tr_op );
		return;

		rt_log("UNION");
		bu_log("UNION\n");
		break;
		rt_log("INTERSECT");
		bu_log("INTERSECT\n");
		break;
		rt_log("MINUS");
		bu_log("MINUS\n");
		break;
		rt_log("XOR");
		bu_log("XOR\n");
		break;
		rt_log("NOT");
		bu_log("NOT\n");
		break;
	rt_log("\n");
	}

	case OP_SOLID:
		break;
	switch( tp->tr_op )  {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* BINARY type */
		rt_pr_tree( tp->tr_b.tb_left, lvl+1 );
		rt_pr_tree( tp->tr_b.tb_right, lvl+1 );
		break;
	case OP_NOT:
	case OP_XNOP:
		/* UNARY tree */
		bu_vls_strcat( vls, ") " );
		break;
	if( lvl == 0 )  bu_log("\n");
}

 *			R T _ P R I N T B
 *
 *  Print a value a la the %b format of the kernel's printf
 *
 *    s		title string
 *    v		the integer with the bits in it
 *    bits	a string which starts with the desired base, then followed by
 *		words preceeded with embedded low-value bytes indicating
 *		bit number plus one,
 *		in little-endian order, eg:
 *		"\010\2Bit_one\1BIT_zero"
 */
void
rt_printb(s, v, bits)
char *s;
register unsigned long v;
register char *bits;
{
	register int i, any = 0;
	register char c;

	if (*bits++ == 8)
		rt_log("%s=0%o <", s, v);
	else
		rt_log("%s=x%x <", s, v);
	while (i = *bits++) {
		if (v & (1L << (i-1))) {
			if (any)
				rt_log(",");
			any = 1;
			for (; (c = *bits) > 32; bits++)
				rt_log("%c", c);
		} else
			for (; *bits > 32; bits++)
				;
	}
	rt_log(">");
		tol->perp, tol->para );
}
