/*
 *			R E G
 *
 *  Library for writing MGED databases from arbitrary procedures.
 *
 *  This module contains routines to create combinations, and regions.
 *
 *  It is expected that this library will grow as experience is gained.
 *
 *  Authors -
 *	Michael John Muuss
 *	Paul R. Stay
 *	Bill Mermagen Jr.
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			M K _ C O M B _ I N T E R N A L
 *
 *  A helper routine to create an rt_comb_internal version of the
 *  combination given a linked list of wmember structures.
 *
 *  Note that this doesn't provide GIFT-like semantics to the op list.
 *  Should it?
 */
struct rt_comb_internal *
mk_comb_internal( struct wmember *headp )
{
	struct rt_comb_internal	*comb;
	register struct wmember *wp;
	union tree	*leafp, *nodep;

	BU_GETSTRUCT( comb, rt_comb_internal );
	comb->magic = RT_COMB_MAGIC;
	bu_vls_init( &comb->shader );
	bu_vls_init( &comb->material );

	for( BU_LIST_FOR( wp, wmember, &headp->l ) )  {
		if( wp->l.magic != WMEMBER_MAGIC )  {
			bu_bomb("mk_comb_internal:  corrupted linked list\n");
		}
		BU_GETUNION( leafp, tree );
		leafp->tr_l.magic = RT_TREE_MAGIC;
		leafp->tr_l.tl_op = OP_DB_LEAF;
		leafp->tr_l.tl_name = bu_strdup( wp->wm_name );
		if( !bn_mat_is_identity( wp->wm_mat ) )  {
			leafp->tr_l.tl_mat = bn_mat_dup( wp->wm_mat );
		}

		if( !comb->tree )  {
			comb->tree = leafp;
			continue;
		}
		/* Build a left-heavy tree */
		BU_GETUNION( nodep, tree );
		nodep->tr_b.magic = RT_TREE_MAGIC;
		switch( wp->wm_op )  {
		case WMOP_UNION:
			nodep->tr_b.tb_op = OP_UNION;
			break;
		case WMOP_INTERSECT:
			nodep->tr_b.tb_op = OP_INTERSECT;
			break;
		case WMOP_SUBTRACT:
			nodep->tr_b.tb_op = OP_SUBTRACT;
			break;
		default:
			bu_bomb("mk_comb_internal() bad wm_op");
		}
		nodep->tr_b.tb_left = comb->tree;
		nodep->tr_b.tb_right = leafp;
		comb->tree = nodep;
	}
	return comb;
}

/*
 *			M K _ A D D M E M B E R
 *
 *  Obtain dynamic storage for a new wmember structure, fill in the
 *  name, default the operation and matrix, and add to doubly linked
 *  list.  In typical use, a one-line call is sufficient.  To change
 *  the defaults, catch the pointer that is returned, and adjust the
 *  structure to taste.
 *
 *  The caller is responsible for initializing the header structures
 *  forward and backward links.
 */
struct wmember *
mk_addmember( name, headp, op )
CONST char	*name;
register struct wmember *headp;
int	op;
{
	register struct wmember *wp;

	BU_GETSTRUCT( wp, wmember );
	wp->l.magic = WMEMBER_MAGIC;
	strncpy( wp->wm_name, name, sizeof(wp->wm_name) );
	switch( op )  {
	case WMOP_UNION:
	case WMOP_INTERSECT:
	case WMOP_SUBTRACT:
		wp->wm_op = op;
		break;
	default:
		fprintf(stderr, "mk_addmember() op=x%x is bad\n", op);
		return(WMEMBER_NULL);
	}
	bn_mat_idn( wp->wm_mat );

	/* Append to end of doubly linked list */
	BU_LIST_INSERT( &headp->l, &wp->l );
	return(wp);
}

/*
 *			M K _ L C O M B
 *
 *  Make a combination, much like mk_comb(), but where the
 *  members are described by a linked list of wmember structs.
 *  This routine produces the combination and member records
 *  all at once, making it easier and less risky to use than
 *  direct use of the pair of mk_comb() and mk_memb().
 *  The linked list is freed when it has been output.
 *
 *  A shorthand version is given in wdb.h as a macro.
 */
int
mk_lcomb( fp, name, headp, region, matname, matparm, rgb, inherit )
FILE		*fp;
CONST char	*name;
register struct wmember *headp;
int		region;
CONST char	*matname;
CONST char	*matparm;
CONST unsigned char	*rgb;
int		inherit;
{
	struct rt_comb_internal *comb;

	comb = mk_comb_internal( headp );
	if( region )  comb->region_flag = 1;
	if( matname )  bu_vls_strcat( &comb->shader, matname );
	if( matparm )  {
		bu_vls_strcat( &comb->shader, " " );
		bu_vls_strcat( &comb->shader, matparm );
	}
	/* XXX Convert to TCL form? */
	if( rgb )  {
		comb->rgb_valid = 1;
		comb->rgb[0] = rgb[0];
		comb->rgb[1] = rgb[1];
		comb->rgb[2] = rgb[2];
	}
	comb->inherit = inherit;

	/* Release the member structure dynamic storage */
	mk_freemembers( headp );

	return mk_export_fwrite( fp, name, comb, ID_COMBINATION );
}

/*
 *			M K _ F R E E M E M B E R S
 *
 *  Returns -
 *	 0	All OK
 *	<0	List was corrupted
 */
int
mk_freemembers( headp )
register struct wmember *headp;
{
	register struct wmember *wp;
	register int	ret = 0;

	while( BU_LIST_WHILE( wp, wmember, &headp->l ) )  {
		if( wp->l.magic != WMEMBER_MAGIC )
			ret--;
		BU_LIST_DEQUEUE( &wp->l );
		wp->l.magic = -1;	/* Sanity */
		free( (char *)wp );
	}
	return(ret);
}

/*
 *			M K _ L R C O M B
 *
 *  Make a combination, much like mk_comb(), but where the
 *  members are described by a linked list of wmember structs.
 *  This routine produces the combination and member records
 *  all at once, making it easier and less risky to use than
 *  direct use of the pair of mk_comb() and mk_memb().
 *  The linked list is freed when it has been output.
 *  Like mk_lcomb except for additional region parameters.
 *
 */
int
mk_lrcomb( fp, name, headp, region, matname, matparm, rgb, id, air, material, los, inherit )
FILE		*fp;
CONST char	*name;
register struct wmember *headp;
int		region;
CONST char	*matname;
CONST char	*matparm;
CONST unsigned char	*rgb;
int	id;
int	air;
int	material;
int	los;
int	inherit;
{
	struct rt_comb_internal *comb;

	comb = mk_comb_internal( headp );
	if( region )  comb->region_flag = 1;
	if( matname )  bu_vls_strcat( &comb->shader, matname );
	if( matparm )  {
		bu_vls_strcat( &comb->shader, " " );
		bu_vls_strcat( &comb->shader, matparm );
	}
	/* XXX Convert to TCL form? */
	if( rgb )  {
		comb->rgb_valid = 1;
		comb->rgb[0] = rgb[0];
		comb->rgb[1] = rgb[1];
		comb->rgb[2] = rgb[2];
	}

	comb->region_id = id;
	comb->aircode = air;
	comb->GIFTmater = material;
	comb->los = los;

	comb->inherit = inherit;

	/* Release the member structure dynamic storage */
	mk_freemembers( headp );

	return mk_export_fwrite( fp, name, comb, ID_COMBINATION );
}

/*
 *			M K _ C O M B 1
 *
 *  Convenience interface to make a combination with a single member.
 */
int
mk_comb1( FILE *fp,
	CONST char *combname,
	CONST char *membname,
	int regflag )
{
	struct wmember	head;

	BU_LIST_INIT( &head.l );
	if( mk_addmember( membname, &head, WMOP_UNION ) == WMEMBER_NULL )
		return -2;
	return mk_lcomb( fp, combname, &head, regflag,
		(char *)NULL, (char *)NULL, (unsigned char *)NULL, 0 );
}

/*
 *			M K _ R E G I O N 1
 *
 *  Convenience routine to make a region with shader and rgb possibly set.
 */
int
mk_region1(
	FILE *fp,
	const char *combname,
	const char *membname,
	const char *matname,
	const char *matparm,
	const unsigned char *rgb )
{
	struct wmember	head;

	BU_LIST_INIT( &head.l );
	if( mk_addmember( membname, &head, WMOP_UNION ) == WMEMBER_NULL )
		return -2;
	return mk_lcomb( fp, combname, &head, 1, matname, matparm, rgb, 0 );
}

/*
 *		M K _ F A S T G E N _ R E G I O N
 *
 *	Code to create a region with the FASTGEN Plate-mode or FASTGEN Volume-mode
 *	flags set
 */
int
mk_fastgen_region( fp, name, headp, mode, matname, matparm, rgb, id, air, material, los, inherit )
FILE		*fp;
CONST char	*name;
register struct wmember *headp;
char		mode;
CONST char	*matname;
CONST char	*matparm;
CONST unsigned char	*rgb;
int	id;
int	air;
int	material;
int	los;
int	inherit;
{
	struct rt_comb_internal *comb;

	comb = mk_comb_internal( headp );
	comb->region_flag = 1;
	switch( mode )  {
	case 'P':
		comb->is_fastgen = REGION_FASTGEN_PLATE;
		break;
	case 'V':
		comb->is_fastgen = REGION_FASTGEN_VOLUME;
		break;
	default:
		fprintf( stderr, "ERROR: mk_fastgen_region: Unrecognized mode flag (%c)\n", mode );
		abort();
	}
	if( matname )  bu_vls_strcat( &comb->shader, matname );
	if( matparm )  {
		bu_vls_strcat( &comb->shader, " " );
		bu_vls_strcat( &comb->shader, matparm );
	}
	/* XXX Convert to TCL form? */
	if( rgb )  {
		comb->rgb_valid = 1;
		comb->rgb[0] = rgb[0];
		comb->rgb[1] = rgb[1];
		comb->rgb[2] = rgb[2];
	}

	comb->region_id = id;
	comb->aircode = air;
	comb->GIFTmater = material;
	comb->los = los;

	comb->inherit = inherit;

	/* Release the member structure dynamic storage */
	mk_freemembers( headp );

	return mk_export_fwrite( fp, name, comb, ID_COMBINATION );
}

