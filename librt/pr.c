/*
 *			P R . C
 * 
 *  Routines to print LIBRT data structures using rt_log()
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *			R T _ L O G I N D E N T _ V L S
 *
 *  For multi-line vls generators, honor rtg_logindent like rt_log() does.
 *  Should be called at the front of each new line.
 */
void
rt_logindent_vls( v )
struct rt_vls	*v;
{
	register int	i, todo;
	CONST static char	spaces[65] = "                                                                ";

	RT_VLS_CHECK( v );

	i = rt_g.rtg_logindent;
	while( i > 0 )  {
		todo = i;
		if( todo > 64 )  todo = 64;
		rt_vls_strncat( v, spaces, todo );
		i -= todo;
	}
}

/*
 *			R T _ P R _ S O L T A B
 */
void
rt_pr_soltab( stp )
register CONST struct soltab	*stp;
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
register CONST struct region *rp;
{
	struct rt_vls	v;

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

	rt_vls_init(&v);
	rt_pr_tree_vls(&v, rp->reg_treetop);
	rt_log("%s %d %s\n", rp->reg_name,
		rp->reg_instnum, rt_vls_addr(&v) );
	rt_vls_free(&v);
}

/*
 *			R T _ P R _ P A R T I T I O N S
 *
 */
void
rt_pr_partitions( rtip, phead, title )
CONST struct rt_i		*rtip;
register CONST struct partition	*phead;
CONST char			*title;
{
	register CONST struct partition *pp;
	struct rt_vls		v;

	RT_CHECK_RTI(rtip);

	rt_vls_init( &v );
	rt_logindent_vls( &v );
	rt_vls_strcat( &v, "------" );
	rt_vls_strcat( &v, title );
	rt_vls_strcat( &v, "\n" );

	for( pp = phead->pt_forw; pp != phead; pp = pp->pt_forw ) {
		RT_CHECK_PT(pp);
		rt_pr_pt_vls( &v, rtip, pp );
	}
	rt_logindent_vls( &v );
	rt_vls_strcat( &v, "------\n");

	rt_log("%s", rt_vls_addr( &v ) );
	rt_vls_free( &v );
}

/*
 *			R T _ P R _ P T _ V L S
 */
void
rt_pr_pt_vls( v, rtip, pp )
struct rt_vls			*v;
CONST struct rt_i		*rtip;
register CONST struct partition *pp;
{
	register CONST struct soltab	*stp;
	char		buf[128];

	RT_CHECK_RTI(rtip);
	RT_CHECK_PT(pp);
	RT_VLS_CHECK(v);

	rt_logindent_vls( v );
	sprintf(buf, "%.8x: PT ", pp );
	rt_vls_strcat( v, buf );

	stp = pp->pt_inseg->seg_stp;
	sprintf(buf, "%s (%s#%d) ",
		stp->st_dp->d_namep,
		rt_functab[stp->st_id].ft_name+3,
		stp->st_bit );
	rt_vls_strcat( v, buf );

	stp = pp->pt_outseg->seg_stp;
	sprintf(buf, "%s (%s#%d) ",
		stp->st_dp->d_namep,
		rt_functab[stp->st_id].ft_name+3,
		stp->st_bit );
	rt_vls_strcat( v, buf );

	sprintf(buf, "(%g,%g)",
		pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist );
	rt_vls_strcat( v, buf );
	if( pp->pt_inflip )  rt_vls_strcat( v, " Iflip" );
	if( pp->pt_outflip )  rt_vls_strcat( v, " Oflip" );
	rt_vls_strcat( v, "\n");

	rt_pr_hit_vls( v, "  In", pp->pt_inhit );
	rt_pr_hit_vls( v, " Out", pp->pt_outhit );
	rt_logindent_vls( v );
	rt_vls_strcat( v, "  Solids: " );
	rt_pr_bitv_vls( v, pp->pt_solhit, rtip->nsolids );
	rt_vls_strcat( v, "\n" );
	}
}

/*
 *			R T _ P R _ P T
 */
void
rt_pr_pt( rtip, pp )
CONST struct rt_i		*rtip;
register CONST struct partition *pp;
	struct rt_vls	v;
	struct bu_vls	v;

	RT_CHECK_RTI(rtip);
	rt_vls_init( &v );
	bu_vls_init( &v );
	rt_log("%s", rt_vls_addr( &v ) );
	rt_vls_free( &v );
	bu_vls_free( &v );
}

 *			R T _ P R _ B I T V _ V L S
 *
 *  Print the bits set in a bit vector.
 */
void
rt_pr_bitv_vls( v, bv, len )
struct rt_vls		*v;
register CONST bitv_t	*bv;
register int		len;
{
	register int	i;
	char		buf[128];
	int		seen = 0;

	RT_VLS_CHECK( v );

	rt_vls_strcat( v, "(" );
	for( i=0; i<len; i++ )  {
		if( BITTEST(bv,i) )  {
			if( seen )  rt_vls_strcat( v, ", " );
			sprintf( buf, "%d", i );
			rt_vls_strcat( v, buf );
			seen = 1;
		}
	}
	rt_vls_strcat( v, ") " );
}

/*
 *			R T _ P R _ B I T V
 *
 *  Print the bits set in a bit vector.
 *  Use rt_vls stuff, to make only a single call to rt_log().
 */
void
rt_pr_bitv( str, bv, len )
CONST char		*str;
register CONST bitv_t	*bv;
register int		len;
{
	struct rt_vls	v;

	rt_vls_init( &v );
	rt_vls_strcat( &v, str );
	rt_vls_strcat( &v, ": " );
	rt_pr_bitv_vls( &v, bv, len );
	rt_log("%s", rt_vls_addr( &v ) );
	rt_vls_free( &v );
}

/*
/*
 *			R T _ P R _ S E G
 */
void
rt_pr_seg(segp)
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
rt_pr_hit( str, hitp )
CONST char			*str;
register CONST struct hit	*hitp;
	struct rt_vls		v;
	struct bu_vls		v;
	rt_vls_init( &v );
	bu_vls_init( &v );
	rt_log("%s", rt_vls_addr( &v ) );
	rt_vls_free( &v );
	bu_vls_free( &v );
}

/*
 *			R T _ P R _ H I T _ V L S
 */
void
struct rt_vls			*v;
struct bu_vls			*v;
CONST char			*str;
register CONST struct hit	*hitp;
	char		buf[128];
	BU_CK_VLS( v );
	RT_VLS_CHECK( v );
	bu_vls_strcat( v, str );
	rt_logindent_vls( v );
	rt_vls_strcat( v, str );

	sprintf(buf, "HIT dist=%g (surf %d)\n",
	bu_vls_printf(v, "HIT dist=%g (surf %d)\n",
	rt_vls_strcat( v, buf );
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
rt_pr_tree( tp, lvl )
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
		db_pr_combined_tree_state( tp->tr_c.tc_ctsp );
		return;

		rt_log("DB_LEAF %s%s\n",
		bu_log("DB_LEAF %s%s\n",
			tp->tr_l.tl_name,
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
		rt_pr_tree( tp->tr_b.tb_left, lvl+1 );
		break;
	}
}

/*
 *			R T _ P R _ T R E E _ V L S
 *
 *  Produce a compact representation of this tree.
 *  The destination vls must be initialized by the caller.
 *
 *  Operations are responsible for generating white space.
 */
void
struct rt_vls		*vls;
struct bu_vls		*vls;
register CONST union tree *tp;
{
	char		*str;

		rt_vls_strcat( vls, "??NULL_tree??" );
		bu_vls_strcat( vls, "??NULL_tree??" );
		return;
	}

	switch( tp->tr_op )  {

		rt_vls_strcat( vls, "NOP");
		bu_vls_strcat( vls, "NOP");
		return;

		rt_vls_strcat( vls, tp->tr_a.tu_stp->st_name );
		bu_vls_strcat( vls, tp->tr_a.tu_stp->st_dp->d_namep );
		return;

	case OP_REGION:
		rt_vls_strcat( vls, str );
		bu_vls_strcat( vls, str );
		rt_free( str, "path string" );
		return;

		rt_log("rt_pr_tree_vls() Unknown op=x%x\n", tp->tr_op );
		bu_log("rt_pr_tree_vls() Unknown op=x%x\n", tp->tr_op );
		return;

	case OP_UNION:
		rt_vls_strcat( vls, " (" );
		bu_vls_strcat( vls, " (" );
		rt_vls_strcat( vls, ") u (" );
		bu_vls_strcat( vls, ") u (" );
		rt_vls_strcat( vls, ") " );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_INTERSECT:
		rt_vls_strcat( vls, " (" );
		bu_vls_strcat( vls, " (" );
		rt_vls_strcat( vls, ") + (" );
		bu_vls_strcat( vls, ") + (" );
		rt_vls_strcat( vls, ") " );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_SUBTRACT:
		rt_vls_strcat( vls, " (" );
		bu_vls_strcat( vls, " (" );
		rt_vls_strcat( vls, ") - (" );
		bu_vls_strcat( vls, ") - (" );
		rt_vls_strcat( vls, ") " );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_XOR:
		rt_vls_strcat( vls, " (" );
		bu_vls_strcat( vls, " (" );
		rt_vls_strcat( vls, ") ^ (" );
		bu_vls_strcat( vls, ") ^ (" );
		rt_vls_strcat( vls, ") " );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_NOT:
		rt_vls_strcat( vls, " !(" );
		bu_vls_strcat( vls, " !(" );
		rt_vls_strcat( vls, ") " );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_GUARD:
		rt_vls_strcat( vls, " guard(" );
		bu_vls_strcat( vls, " guard(" );
		rt_vls_strcat( vls, ") " );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_XNOP:
		rt_vls_strcat( vls, " xnop(" );
		bu_vls_strcat( vls, " xnop(" );
		rt_vls_strcat( vls, ") " );
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
rt_pr_tree_val( tp, partp, pr_name, lvl )
register CONST union tree *tp;		/* Tree to print */
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
CONST char		*s;
register unsigned long	v;
register CONST char	*bits;
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
struct bu_vls	*str;
CONST char	*prefix;
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
double		angles[5];
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
	if( vec[Z] <= -1.0 )  {
		asinZ = rt_halfpi * 3;
		asinZ = bn_halfpi * 3;
	} else if( vec[Z] >= 1.0 )  {
		asinZ = rt_halfpi;
		asinZ = bn_halfpi;
	} else {
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
	}
}

/*
 *			R T _ P R _ T O L
 *
 *  Print a tolerance structure.
 */
void
CONST struct rt_tol	*tol;
CONST struct bn_tol	*tol;
	RT_CK_TOL(tol);
	BN_CK_TOL(tol);
	rt_log("%8.8x TOL %e (sq=%e) perp=%e, para=%e\n",
	bu_log("%8.8x TOL %e (sq=%e) perp=%e, para=%e\n",
		tol, tol->dist, tol->dist_sq,
		tol->perp, tol->para );
}
