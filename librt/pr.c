/*
 *			P R . C
 * 
 *  Routines to print LIBRT data structures using bu_log()
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
#include "bu.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *			R T _ P R _ S O L T A B
 */
void
rt_pr_soltab( stp )
register CONST struct soltab	*stp;
{
	register int	id = stp->st_id;

	if( id <= 0 || id > ID_MAXIMUM )  {
		bu_log("stp=x%x, id=%d.\n", stp, id);
		rt_bomb("rt_pr_soltab:  bad st_id");
	}
	bu_log("------------ %s (bit %d) %s ------------\n",
		stp->st_dp->d_namep, stp->st_bit,
		rt_functab[id].ft_name );
	VPRINT("Bound Sph CENTER", stp->st_center);
	bu_log("Approx Sph Radius = %g\n", stp->st_aradius);
	bu_log("Bounding Sph Radius = %g\n", stp->st_bradius);
	VPRINT("Bound RPP min", stp->st_min);
	VPRINT("Bound RPP max", stp->st_max);
	bu_pr_ptbl( "st_regions", &stp->st_regions, 1 );
	rt_functab[id].ft_print( stp );
}

/*
 *			R T _ P R _ R E G I O N
 */
void
rt_pr_region( rp )
register CONST struct region *rp;
{
	struct bu_vls	v;

	bu_log("REGION %s (bit %d)\n", rp->reg_name, rp->reg_bit );
	bu_log("instnum=%d, id=%d, air=%d, gift_material=%d, los=%d\n",
		rp->reg_instnum,
		rp->reg_regionid, rp->reg_aircode,
		rp->reg_gmater, rp->reg_los );
	if( rp->reg_mater.ma_override == 1 )
		bu_log("Color %d %d %d\n",
			(int)rp->reg_mater.ma_color[0]*255.,
			(int)rp->reg_mater.ma_color[1]*255.,
			(int)rp->reg_mater.ma_color[2]*255. );
	if( rp->reg_mater.ma_matname[0] != '\0' )
		bu_log("Material '%s' '%s'\n",
			rp->reg_mater.ma_matname,
			rp->reg_mater.ma_matparm );

	bu_vls_init(&v);
	rt_pr_tree_vls(&v, rp->reg_treetop);
	bu_log("%s %d %s\n", rp->reg_name,
		rp->reg_instnum, bu_vls_addr(&v) );
	bu_vls_free(&v);
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
	struct bu_vls		v;

	RT_CHECK_RTI(rtip);

	bu_vls_init( &v );
	bu_log_indent_vls( &v );
	bu_vls_strcat( &v, "------" );
	bu_vls_strcat( &v, title );
	bu_vls_strcat( &v, "\n" );

	for( pp = phead->pt_forw; pp != phead; pp = pp->pt_forw ) {
		RT_CHECK_PT(pp);
		rt_pr_pt_vls( &v, rtip, pp );
	}
	bu_log_indent_vls( &v );
	bu_vls_strcat( &v, "------\n");

	bu_log("%s", bu_vls_addr( &v ) );
	bu_vls_free( &v );
}

/*
 *			R T _ P R _ P T _ V L S
 */
void
rt_pr_pt_vls( v, rtip, pp )
struct bu_vls			*v;
CONST struct rt_i		*rtip;
register CONST struct partition *pp;
{
	register CONST struct soltab	*stp;
	register struct soltab		**spp;

	RT_CHECK_RTI(rtip);
	RT_CHECK_PT(pp);
	BU_CK_VLS(v);

	bu_log_indent_vls( v );
	bu_vls_printf( v, "%.8x: PT ", pp );

	stp = pp->pt_inseg->seg_stp;
	bu_vls_printf(v, "%s (%s#%d) ",
		stp->st_dp->d_namep,
		rt_functab[stp->st_id].ft_name+3,
		stp->st_bit );

	stp = pp->pt_outseg->seg_stp;
	bu_vls_printf(v, "%s (%s#%d) ",
		stp->st_dp->d_namep,
		rt_functab[stp->st_id].ft_name+3,
		stp->st_bit );

	bu_vls_printf(v, "(%g,%g)",
		pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist );
	if( pp->pt_inflip )  bu_vls_strcat( v, " Iflip" );
	if( pp->pt_outflip )  bu_vls_strcat( v, " Oflip" );
	bu_vls_strcat( v, "\n");

	rt_pr_hit_vls( v, "  In", pp->pt_inhit );
	rt_pr_hit_vls( v, " Out", pp->pt_outhit );
	bu_log_indent_vls( v );
	bu_vls_strcat( v, "  Solids: " );
	for( BU_PTBL_FOR( spp, (struct soltab **), &pp->pt_solids_hit ) )  {
		stp = *spp;
		RT_CK_SOLTAB(stp);
		bu_vls_strcat( v, stp->st_dp->d_namep );
		bu_vls_strcat( v, ", " );
	}
	bu_vls_strcat( v, "\n" );
	}
}

/*
 *			R T _ P R _ P T
 */
void
rt_pr_pt( rtip, pp )
CONST struct rt_i		*rtip;
register CONST struct partition *pp;
{
	struct bu_vls	v;

	RT_CHECK_RTI(rtip);
	RT_CHECK_PT(pp);
	bu_vls_init( &v );
	rt_pr_pt_vls( &v, rtip, pp );
	bu_log("%s", bu_vls_addr( &v ) );
	bu_vls_free( &v );
}

/*
 *			R T _ P R _ S E G
 */
void
rt_pr_seg(segp)
register CONST struct seg *segp;
{
	RT_CHECK_SEG(segp);
	bu_log("%.8x: SEG %s (%g,%g) bit=%d\n",
		segp,
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
{
	struct bu_vls		v;

	bu_vls_init( &v );
	rt_pr_hit_vls( &v, str, hitp );
	bu_log("%s", bu_vls_addr( &v ) );
	bu_vls_free( &v );
}

/*
 *			R T _ P R _ H I T _ V L S
 */
void
rt_pr_hit_vls( v, str, hitp )
struct bu_vls			*v;
CONST char			*str;
register CONST struct hit	*hitp;
{
	BU_CK_VLS( v );

	bu_log_indent_vls( v );
	bu_vls_strcat( v, str );

	bu_vls_printf(v, "HIT dist=%g (surf %d)\n",
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

	bu_log("%.8x ", tp);
	for( i=lvl; i>0; i-- )
		bu_log("  ");

	if( tp == TREE_NULL )  {
		bu_log("Null???\n");
		return;
	}

	switch( tp->tr_op )  {

	case OP_NOP:
		bu_log("NOP\n");
		return;

	case OP_SOLID:
		bu_log("SOLID %s (bit %d)\n",
			tp->tr_a.tu_stp->st_dp->d_namep,
			tp->tr_a.tu_stp->st_bit );
		return;

	case OP_REGION:
		bu_log("REGION ctsp=x%x\n", tp->tr_c.tc_ctsp );
		db_pr_combined_tree_state( tp->tr_c.tc_ctsp );
		return;

	case OP_DB_LEAF:
		bu_log("DB_LEAF %s%s\n",
			tp->tr_l.tl_name,
			tp->tr_l.tl_mat ? " (matrix)" : "" );
		return;

	default:
		bu_log("Unknown op=x%x\n", tp->tr_op );
		return;

	case OP_UNION:
		bu_log("UNION\n");
		break;
	case OP_INTERSECT:
		bu_log("INTERSECT\n");
		break;
	case OP_SUBTRACT:
		bu_log("MINUS\n");
		break;
	case OP_XOR:
		bu_log("XOR\n");
		break;
	case OP_NOT:
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
rt_pr_tree_vls( vls, tp )
struct bu_vls		*vls;
register CONST union tree *tp;
{
	char		*str;

	if( tp == TREE_NULL )  {
		bu_vls_strcat( vls, "??NULL_tree??" );
		return;
	}

	switch( tp->tr_op )  {

	case OP_NOP:
		bu_vls_strcat( vls, "NOP");
		return;

	case OP_SOLID:
		bu_vls_strcat( vls, tp->tr_a.tu_stp->st_dp->d_namep );
		return;

	case OP_REGION:
		str = db_path_to_string( &(tp->tr_c.tc_ctsp->cts_p) );
		bu_vls_strcat( vls, str );
		rt_free( str, "path string" );
		return;

	case OP_DB_LEAF:
		bu_vls_strcat( vls, tp->tr_l.tl_name );
		return;

	default:
		bu_log("rt_pr_tree_vls() Unknown op=x%x\n", tp->tr_op );
		return;

	case OP_UNION:
		/* BINARY type */
		bu_vls_strcat( vls, " (" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_left );
		bu_vls_strcat( vls, ") u (" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_right );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_INTERSECT:
		/* BINARY type */
		bu_vls_strcat( vls, " (" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_left );
		bu_vls_strcat( vls, ") + (" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_right );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_SUBTRACT:
		/* BINARY type */
		bu_vls_strcat( vls, " (" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_left );
		bu_vls_strcat( vls, ") - (" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_right );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_XOR:
		/* BINARY type */
		bu_vls_strcat( vls, " (" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_left );
		bu_vls_strcat( vls, ") ^ (" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_right );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_NOT:
		/* UNARY tree */
		bu_vls_strcat( vls, " !(" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_left );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_GUARD:
		/* UNARY tree */
		bu_vls_strcat( vls, " guard(" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_left );
		bu_vls_strcat( vls, ") " );
		break;
	case OP_XNOP:
		/* UNARY tree */
		bu_vls_strcat( vls, " xnop(" );
		rt_pr_tree_vls( vls, tp->tr_b.tb_left );
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
		default:
			bu_log("tree val: ");
			break;
		case 1:
			bu_log("tree solids: ");
			break;
		case 2:
			bu_log("tree solid bits: ");
			break;
		}
	}

	if( tp == TREE_NULL )  {
		bu_log("Null???\n");
		return;
	}

	switch( tp->tr_op )  {
	default:
		bu_log("Unknown_op=x%x", tp->tr_op );
		break;

	case OP_SOLID:
		switch( pr_name )  {
		case 0:
			if( bu_ptbl_locate( &partp->pt_solids_hit, (long *)(tp->tr_a.tu_stp) ) ==  -1 )
				bu_log("0");
			else
				bu_log("1");
			break;
		case 1:
			bu_log("%s", tp->tr_a.tu_stp->st_dp->d_namep );
			break;
		case 2:
			bu_log("%d", tp->tr_a.tu_stp->st_bit );
			break;
		}
		break;


	case OP_UNION:
		bu_log("(");
		rt_pr_tree_val( tp->tr_b.tb_left,  partp, pr_name, lvl+1 );
		bu_log(" u ");
		rt_pr_tree_val( tp->tr_b.tb_right, partp, pr_name, lvl+1 );
		bu_log(")");
		break;
	case OP_INTERSECT:
		bu_log("(");
		rt_pr_tree_val( tp->tr_b.tb_left,  partp, pr_name, lvl+1 );
		bu_log(" + ");
		rt_pr_tree_val( tp->tr_b.tb_right, partp, pr_name, lvl+1 );
		bu_log(")");
		break;
	case OP_SUBTRACT:
		bu_log("(");
		rt_pr_tree_val( tp->tr_b.tb_left,  partp, pr_name, lvl+1 );
		bu_log(" - ");
		rt_pr_tree_val( tp->tr_b.tb_right, partp, pr_name, lvl+1 );
		bu_log(")");
		break;
	case OP_XOR:
		bu_log("(");
		rt_pr_tree_val( tp->tr_b.tb_left,  partp, pr_name, lvl+1 );
		bu_log(" XOR ");
		rt_pr_tree_val( tp->tr_b.tb_right, partp, pr_name, lvl+1 );
		bu_log(")");
		break;

	case OP_NOT:
		bu_log(" !");
		rt_pr_tree_val( tp->tr_b.tb_left, partp, pr_name, lvl+1 );
		break;
	case OP_GUARD:
		bu_log(" GUARD ");
		rt_pr_tree_val( tp->tr_b.tb_left, partp, pr_name, lvl+1 );
		break;
	}

	if( lvl == 0 )  bu_log("\n");
}

/*
 *			R T _ P R _ F A L L B A C K _ A N G L E
 */
void
rt_pr_fallback_angle( str, prefix, angles )
struct bu_vls	*str;
CONST char	*prefix;
CONST double	angles[5];
{
	BU_CK_VLS(str);

	bu_vls_printf(str, "%s direction cosines=(%.1f, %1.f, %1.f)\n",
		prefix, angles[0], angles[1], angles[2] );

	bu_vls_printf(str, "%s rotation angle=%1.f, fallback angle=%1.f\n",
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
	} else {
		angles[X] = acos( vec[X] ) * bn_radtodeg;
	}

	if( vec[Y] <= -1.0 )  {
		angles[Y] = 180.0;
	} else if( vec[Y] >= 1.0 )  {
		angles[Y] = 0.0;
	} else {
		angles[Y] = acos( vec[Y] ) * bn_radtodeg;
	}

	if( vec[Z] <= -1.0 )  {
		angles[Z] = 180.0;
	} else if( vec[Z] >= 1.0 )  {
		angles[Z] = 0.0;
	} else {
		angles[Z] = acos( vec[Z] ) * bn_radtodeg;
	}

	/* fallback angle */
	if( vec[Z] <= -1.0 )  {
		/* 270 degrees:  3/2 pi */
		asinZ = bn_halfpi * 3;
	} else if( vec[Z] >= 1.0 )  {
		/* +90 degrees: 1/2 pi */
		asinZ = bn_halfpi;
	} else {
		asinZ = asin(vec[Z]);
	}
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
		} else {
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
rt_pr_tol(tol)
CONST struct bn_tol	*tol;
{
	BN_CK_TOL(tol);

	bu_log("%8.8x TOL %e (sq=%e) perp=%e, para=%e\n",
		tol, tol->dist, tol->dist_sq,
		tol->perp, tol->para );
}
