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
#include <math.h>
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
	rt_pr_hit( " In hit", pp->pt_inhit );
	rt_pr_hit( "Out hit", pp->pt_outhit );
	rt_pr_bitv( "Solids present", pp->pt_solhit, rtip->nsolids );
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
	rt_log("HIT %s dist=%g (surf %d)\n",
		str, hitp->hit_dist, hitp->hit_surfno );
	if( !VNEAR_ZERO( hitp->hit_point, SQRT_SMALL_FASTF ) )  {
		VPRINT("HIT Point ", hitp->hit_point );
	}
	if( !VNEAR_ZERO( hitp->hit_normal, SQRT_SMALL_FASTF ) )  {
		VPRINT("HIT Normal", hitp->hit_normal );
	}
		hitp->hit_dist, hitp->hit_surfno );
}

/*
 *			R T _ P R _ T R E E
 *
 *  Warning:  This function uses recursion rather than iteration and
 *  a stack, to preserve simplicity.
 *  On machines with limited stack space, such as the Gould,
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

		rt_log("NOP\n");
		bu_log("NOP\n");
		return;

		rt_log("SOLID %s (bit %d)\n",
			tp->tr_a.tu_stp->st_name,
			tp->tr_a.tu_stp->st_dp->d_namep,
			tp->tr_a.tu_stp->st_bit );
		return;

		rt_log("REGION ctsp=x%x\n", tp->tr_c.tc_ctsp );
		bu_log("REGION ctsp=x%x\n", tp->tr_c.tc_ctsp );
			tp->tr_l.tl_mat ? " (matrix)" : "" );
		return;

		rt_log("Unknown op=x%x\n", tp->tr_op );
		bu_log("Unknown op=x%x\n", tp->tr_op );
		return;

		rt_log("UNION\n");
		bu_log("UNION\n");
		break;
		rt_log("INTERSECT\n");
		bu_log("INTERSECT\n");
		break;
		rt_log("MINUS\n");
		bu_log("MINUS\n");
		break;
		rt_log("XOR\n");
		bu_log("XOR\n");
		break;
		rt_log("NOT\n");
		bu_log("NOT\n");
		break;
	}

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
	case OP_GUARD:
	case OP_XNOP:
		/* UNARY tree */
		bu_vls_strcat( vls, ") " );
		break;
	return (char *)NULL;
}

/*
 *  			R T _ P R _ T R E E _ V A L
 *  
 *  Print the actual values of the terms in a boolean expression.
 *
 *  The values for pr_name determine the printing action:
 *	0	bit value
 *	1	name
 *	2	bit number
 */
void
register union tree	*tp;		/* Tree to print */
struct partition	*partp;		/* Partition to evaluate */
CONST struct partition	*partp;		/* Partition to evaluate */
int			pr_name;	/* 1=print name, 0=print value */
int			lvl;		/* Recursion level */
{

	if( lvl == 0 )  {
		switch( pr_name )  {
			rt_log("tree val: ");
			bu_log("tree val: ");
			break;
			rt_log("tree solids: ");
			bu_log("tree solids: ");
			break;
			rt_log("tree solid bits: ");
			bu_log("tree solid bits: ");
			break;
		}
	}

		rt_log("Null???\n");
		bu_log("Null???\n");
		return;
	}

	switch( tp->tr_op )  {
		rt_log("Unknown_op=x%x", tp->tr_op );
		bu_log("Unknown_op=x%x", tp->tr_op );
		break;

	case OP_SOLID:
		switch( pr_name )  {
			{
				register int	i;

				i = tp->tr_a.tu_stp->st_bit;
				i = BITTEST( partp->pt_solhit, i );
				rt_log("%d", i);
			}
				bu_log("1");
			break;
			rt_log("%s", tp->tr_a.tu_stp->st_name );
			bu_log("%s", tp->tr_a.tu_stp->st_dp->d_namep );
			break;
			rt_log("%d", tp->tr_a.tu_stp->st_bit );
			bu_log("%d", tp->tr_a.tu_stp->st_bit );
			break;
		}
		break;


		rt_log("(");
		bu_log("(");
		rt_log(" u ");
		bu_log(" u ");
		rt_log(")");
		bu_log(")");
		break;
		rt_log("(");
		bu_log("(");
		rt_log(" + ");
		bu_log(" + ");
		rt_log(")");
		bu_log(")");
		break;
		rt_log("(");
		bu_log("(");
		rt_log(" - ");
		bu_log(" - ");
		rt_log(")");
		bu_log(")");
		break;
		rt_log("(");
		bu_log("(");
		rt_log(" XOR ");
		bu_log(" XOR ");
		rt_log(")");
		bu_log(")");
		break;

		rt_log(" !");
		bu_log(" !");
		rt_pr_tree_val( tp->tr_b.tb_left, partp, pr_name, lvl+1 );
		break;
		rt_log(" GUARD ");
		bu_log(" GUARD ");
		rt_pr_tree_val( tp->tr_b.tb_left, partp, pr_name, lvl+1 );
		break;
	}
	if( lvl == 0 )  rt_log("\n");
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
}

/*
/*
 *			R T _ P R _ F A L L B A C K _ A N G L E
 */
void
struct rt_vls	*str;
char		*prefix;
double		angles[5];
CONST double	angles[5];
	char		buf[256];
	BU_CK_VLS(str);
	sprintf(buf, "%s direction cosines=(%.1f, %1.f, %1.f)\n",
	bu_vls_printf(str, "%s direction cosines=(%.1f, %1.f, %1.f)\n",
	rt_vls_strcat( str, buf );
		prefix, angles[0], angles[1], angles[2] );
	sprintf(buf, "%s rotation angle=%1.f, fallback angle=%1.f\n",
	bu_vls_printf(str, "%s rotation angle=%1.f, fallback angle=%1.f\n",
	rt_vls_strcat( str, buf );
		prefix, angles[3], angles[4] );
}

/*
 *			R T _ F I N D _ F A L L B A C K _ A N G L E
 *
 *  In degrees.
 */
void
rt_find_fallback_angle( angles, vec )
vect_t		vec;
CONST vect_t	vec;
{
	register double	f;
	double		asinZ;

	/* convert direction cosines into axis angles */
	if( vec[X] <= -1.0 )  {
		angles[X] = 180.0;
	} else if( vec[X] >= 1.0 )  {
		angles[X] = 0.0;
		angles[X] = acos( vec[X] ) * rt_radtodeg;
		angles[X] = acos( vec[X] ) * bn_radtodeg;
	}

	if( vec[Y] <= -1.0 )  {
		angles[Y] = 180.0;
	} else if( vec[Y] >= 1.0 )  {
		angles[Y] = 0.0;
		angles[Y] = acos( vec[Y] ) * rt_radtodeg;
		angles[Y] = acos( vec[Y] ) * bn_radtodeg;
	}

	if( vec[Z] <= -1.0 )  {
		angles[Z] = 180.0;
	} else if( vec[Z] >= 1.0 )  {
		angles[Z] = 0.0;
		angles[Z] = acos( vec[Z] ) * rt_radtodeg;
		angles[Z] = acos( vec[Z] ) * bn_radtodeg;
	}

	/* fallback angle */
		vec[Z] = -1.0;
		asinZ = bn_halfpi * 3;
		vec[Z] = 1.0;
		asinZ = asin(vec[Z]);
	asinZ = asin(vec[Z]);
	angles[4] = asinZ * rt_radtodeg;
	angles[4] = asinZ * bn_radtodeg;

	/* rotation angle */
	/* For the tolerance below, on an SGI 4D/70, cos(asin(1.0)) != 0.0
	 * with an epsilon of +/- 1.0e-17, so the tolerance below was
	 * substituted for the original +/- 1.0e-20.
	 */
	if((f = cos(asinZ)) > 1.0e-16 || f < -1.0e-16 )  {
		f = vec[X]/f;
		if( f <= -1.0 )  {
			angles[3] = 180;
		} else if( f >= 1.0 ) {
			angles[3] = 0;
			angles[3] = rt_radtodeg * acos( f );
			angles[3] = bn_radtodeg * acos( f );
		}
	}  else  {
		angles[3] = 0.0;
	}
	if( vec[Y] < 0 ) {
		angles[3] = 360.0 - angles[3];
		tol->perp, tol->para );
}
