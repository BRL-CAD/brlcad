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
#include "db.h"
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

/* so we don't have to include mat.o */
static fastf_t ident_mat[16] = {
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0
};

/* -------------------- Begin old code, compat only -------------------- */

/*
 *			M K _ C O M B
 *
 *  Make a combination with material properties info.
 *  Must be followed by 'len' mk_memb() calls before any other mk_* routines.
 *
 *  XXX WARNING:  This routine should not be called by new code.
 *  XXX use mk_lcomb() instead.
 */
int
mk_comb( fp, name, len, region, matname, matparm, rgb, inherit )
FILE			*fp;
CONST char		*name;
int			len;
int			region;
CONST char		*matname;
CONST char		*matparm;
CONST unsigned char	*rgb;
int			inherit;
{
	union record rec;

	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.c.c_id = ID_COMB;
	/* XXX What values to pass for FASTGEN plate and volume regions? */
	if( region )
		rec.c.c_flags = DBV4_REGION;
	else
		rec.c.c_flags = DBV4_NON_REGION;
	NAMEMOVE( name, rec.c.c_name );
	rec.c.c_pad1 = len;		/* backwards compat, was c_length */
	if( matname ) {
		strncpy( rec.c.c_matname, matname, sizeof(rec.c.c_matname) );
		if( matparm )
			strncpy( rec.c.c_matparm, matparm,
				sizeof(rec.c.c_matparm) );
	}
	if( rgb )  {
		rec.c.c_override = 1;
		rec.c.c_rgb[0] = rgb[0];
		rec.c.c_rgb[1] = rgb[1];
		rec.c.c_rgb[2] = rgb[2];
	}
	rec.c.c_inherit = inherit;
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/*
 *			M K _ R C O M B
 *
 *  Make a combination with material properties info.
 *  Must be followed by 'len' mk_memb() calls before any other mk_* routines.
 *  Like mk_comb except for additional region parameters.
 *
 *  XXX WARNING:  This routine should not be called by new code.
 *  XXX use mk_addmember() instead.
 */
int
mk_rcomb( fp, name, len, region, matname, matparm, rgb, id, air, material, los, inherit )
FILE		*fp;
CONST char	*name;
int		len;
int		region;
CONST char	*matname;
CONST char	*matparm;
CONST unsigned char	*rgb;
int		id;
int		air;
int		material;
int		los;
int		inherit;
{
	union record rec;

	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.c.c_id = ID_COMB;
	if( region ){
		switch( region )  {
		case DBV4_NON_REGION:	/* sanity, fixes a non-bool arg */
		case DBV4_REGION:
		case DBV4_REGION_FASTGEN_PLATE:
		case DBV4_REGION_FASTGEN_VOLUME:
			rec.c.c_flags = region;
			break;
		default:
			rec.c.c_flags = DBV4_REGION;
		}
		rec.c.c_inherit = inherit;
		rec.c.c_regionid = id;
		rec.c.c_aircode = air;
		rec.c.c_material = material;
		rec.c.c_los = los;
	}
	else
		rec.c.c_flags = DBV4_NON_REGION;
	NAMEMOVE( name, rec.c.c_name );
	rec.c.c_pad1 = len;		/* backwards compat, was c_length */
	if( matname ) {
		strncpy( rec.c.c_matname, matname, sizeof(rec.c.c_matname) );
		if( matparm )
			strncpy( rec.c.c_matparm, matparm,
				sizeof(rec.c.c_matparm) );
	}
	if( rgb )  {
		rec.c.c_override = 1;
		rec.c.c_rgb[0] = rgb[0];
		rec.c.c_rgb[1] = rgb[1];
		rec.c.c_rgb[2] = rgb[2];
	}

	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}


/*
 *			M K _ F C O M B
 *
 *  Make a simple combination header ("fast" version).
 *  Must be followed by 'len' mk_memb() calls before any other mk_* routines.
 *
 *  XXX WARNING:  This routine should not be called by new code.
 */
int
mk_fcomb( fp, name, len, region )
FILE		*fp;
CONST char	*name;
int		len;
int		region;
{
	union record rec;

	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.c.c_id = ID_COMB;
	if( region )
		rec.c.c_flags = DBV4_REGION;
	else
		rec.c.c_flags = DBV4_NON_REGION;
	NAMEMOVE( name, rec.c.c_name );
	rec.c.c_pad1 = len;		/* backwards compat, was c_length */
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/*
 *			M K _ M E M B
 *
 *  Must be part of combination/member clump of records.
 *
 *  XXX WARNING:  This routine should not be called by new code.
 */
int
mk_memb( fp, name, mat, bool_op )
FILE		*fp;
CONST char	*name;
CONST mat_t	mat;
int		bool_op;
{
	union record rec;
	register int i;

	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.M.m_id = ID_MEMB;
	NAMEMOVE( name, rec.M.m_instname );
	if( bool_op )
		rec.M.m_relation = bool_op;
	else
		rec.M.m_relation = UNION;
	if( mat ) {
		for( i=0; i<16; i++ )
			rec.M.m_mat[i] = mat[i];  /* double -> float */
	} else {
		for( i=0; i<16; i++ )
			rec.M.m_mat[i] = ident_mat[i];
	}
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/* -------------------- End old code, compat only -------------------- */

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
		wp->wm_op = UNION;
		break;
	case WMOP_INTERSECT:
		wp->wm_op = INTERSECT;
		break;
	case WMOP_SUBTRACT:
		wp->wm_op = SUBTRACT;
		break;
	default:
		fprintf(stderr, "mk_addmember() op=x%x is bad\n", op);
		return(WMEMBER_NULL);
	}
	bcopy( ident_mat, wp->wm_mat, sizeof(mat_t) );
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

	return mk_export_fwrite( fp, name, comb, ID_COMB );
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

	return mk_export_fwrite( fp, name, comb, ID_COMB );
}

/*
 *			M K _ C O M B 1
 *
 *  Convenience interface to make a combination with a single member.
 */
int
mk_comb1( fp, combname, membname, regflag )
FILE	*fp;
CONST char	*combname;
CONST char	*membname;
int	regflag;
{
	struct wmember	head;

	BU_LIST_INIT( &head.l );
	if( mk_addmember( membname, &head, WMOP_UNION ) == WMEMBER_NULL )
		return -2;
	return mk_lcomb( fp, combname, &head, regflag,
		(char *)NULL, (char *)NULL, (unsigned char *)NULL, 1 );
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

	return mk_export_fwrite( fp, name, comb, ID_COMB );
}

