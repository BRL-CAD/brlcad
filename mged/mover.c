/*
 *			M O V E R . C
 *
 * Functions -
 *	moveHobj	used to update position of an object in objects file
 *	moveinstance	Given a COMB and an object, modify all the regerences
 *	combadd		Add an instance of an object to a combination
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"

/* default region ident codes */
int	item_default = 1000;	/* GIFT region ID */
int	air_default = 0;
int	mat_default = 1;	/* GIFT material code */
int	los_default = 100;	/* Line-of-sight estimate */

/*
 *			M O V E H O B J
 *
 * This routine is used when the object to be moved is
 * the top level in its reference path.  The object itself
 * is relocated.
 *
 * This routine really should just be a bunch of calls to
 * solid-specific routines.
 */
void
moveHobj( dp, xlate )
register struct directory *dp;
matp_t xlate;
{
	vect_t	work;
	register int i;
	register dbfloat_t *p;		/* -> to vector to be worked on */
	static dbfloat_t *area_end;	/* End of area to be processed */
	union record record;

	db_get( dbip,  dp, &record, 0 , 1);

	switch( record.u_id )  {

	case ID_ARS_A:
		/* 1st B type record is special:  Vertex point */
		db_get( dbip,  dp, &record, 1 , 1);

		/* displace the base vector */
		MAT4X3PNT( work, xlate, &record.b.b_values[0] );
		VMOVE( &record.b.b_values[0], work );

		/* Transform remaining vectors */
		for( p = &record.b.b_values[1*3]; p < &record.b.b_values[8*3];
								p += 3) {
			MAT4X3VEC( work, xlate, p );
			VMOVE( p, work );
		}
		db_put( dbip,  dp, &record, 1, 1 );

		/* Process all the remaining B records */
		for( i = 2; i < dp->d_len; i++ )  {
			db_get( dbip,  dp, &record, i , 1);
			/* Transform remaining vectors */
			for( p = &record.b.b_values[0*3];
			     p < &record.b.b_values[8*3]; p += 3) {
				MAT4X3VEC( work, xlate, p );
				VMOVE( p, work );
			}
			db_put( dbip,  dp, &record, i, 1 );
		}
		break;

	case ID_BSOLID:
		move_spline( &record.B, dp, xlate );
		break;

	case ID_SOLID:
		/* Displace the vertex (V) */
		MAT4X3PNT( work, xlate, &record.s.s_values[0] );
		VMOVE( &record.s.s_values[0], work );

		switch( record.s.s_type )  {

		case GENARB8:
			if(record.s.s_cgtype < 0)
				record.s.s_cgtype = -record.s.s_cgtype;
			area_end = &record.s.s_values[8*3];
			goto common;

		case GENTGC:
			area_end = &record.s.s_values[6*3];
			goto common;

		case GENELL:
			area_end = &record.s.s_values[4*3];
			goto common;

		case TOR:
			area_end = &record.s.s_values[8*3];
			/* Fall into COMMON section */

		common:
			/* Transform all the vectors */
			for( p = &record.s.s_values[1*3]; p < area_end;
			     p += 3) {
				MAT4X3VEC( work, xlate, p );
				VMOVE( p, work );
			}
			break;

		default:
			(void)printf("moveobj:  can't move obj type %d\n",
				record.s.s_type );
			return;		/* ERROR */
		}
		db_put( dbip,  dp, &record, 0, 1 );
		break;

	default:
		(void)printf("MoveHobj -- bad disk record\n");
		return;			/* ERROR */

	case ID_COMB:
		/*
		 * Move all the references within a combination
		 */
		for( i=1; i < dp->d_len; i++ )  {
			static mat_t temp, xmat;

			db_get( dbip,  dp, &record, i , 1);
			rt_mat_dbmat( xmat, record.M.m_mat );

			mat_mul( temp, xlate, xmat );

			rt_dbmat_mat( record.M.m_mat, temp );
			db_put( dbip,  dp, &record, i, 1 );
		}
	}
	return;
}

/*
 *			M O V E H I N S T A N C E
 *
 * This routine is used when an instance of an object is to be
 * moved relative to a combination, as opposed to modifying the
 * co-ordinates of member solids.  Input is a pointer to a COMB,
 * a pointer to an object within the COMB, and modifications.
 */
void
moveHinstance( cdp, dp, xlate )
struct directory *cdp;
struct directory *dp;
matp_t xlate;
{
	register int i;
	union record record;
	mat_t temp, xmat;		/* Temporary for mat_mul */

	for( i=1; i < cdp->d_len; i++ )  {
		db_get( dbip,  cdp, &record, i , 1);

		/* Check for match */
		if( strcmp( dp->d_namep, record.M.m_instname ) != 0 )
			continue;

		rt_mat_dbmat( xmat, record.M.m_mat );
		mat_mul(temp, xlate, xmat);

		rt_dbmat_mat( record.M.m_mat, temp );
		db_put( dbip,  cdp, &record, i, 1 );
		return;
	}
	(void)printf( "moveinst:  couldn't find %s/%s\n",
		cdp->d_namep, dp->d_namep );
	return;				/* ERROR */
}

/*
 *			C O M B A D D
 *
 * Add an instance of object 'dp' to combination 'name'.
 * If the combination does not exist, it is created.
 * Flag is 'r' (region), or 'g' (group).
 */
struct directory *
combadd( objp, combname, region_flag, relation, ident, air )
register struct directory *objp;
char *combname;
int region_flag;			/* true if adding region */
int relation;				/* = UNION, SUBTRACT, INTERSECT */
int ident;				/* "Region ID" */
int air;				/* Air code */
{
	register struct directory *dp;
	union record record;
	mat_t	identity;

	/*
	 * Check to see if we have to create a new combination
	 */
	if( (dp = db_lookup( dbip,  combname, LOOKUP_QUIET )) == DIR_NULL )  {

		/* Update the in-core directory */
		dp = db_diradd( dbip, combname, -1, 2, DIR_COMB );
		if( dp == DIR_NULL )
			return DIR_NULL;
		db_alloc( dbip, dp, 2 );

		/* Generate the disk record */
		record.c.c_id = ID_COMB;
		record.c.c_flags = record.c.c_aircode = 0;
		record.c.c_regionid = -1;
		record.c.c_material = 0;
		record.c.c_los = 0;
		record.c.c_override = 0;
		record.c.c_matname[0] = '\0';
		record.c.c_matparm[0] = '\0';
		NAMEMOVE( combname, record.c.c_name );
		if( region_flag ) {       /* creating a region */
			record.c.c_flags = 'R';
			record.c.c_regionid = ident;
			record.c.c_aircode = air;
			record.c.c_los = los_default;
			record.c.c_material = mat_default;
			(void)printf("Creating region id=%d, air=%d, los=%d, GIFTmaterial=%d\n",
				ident, air, los_default, mat_default );
		}

		/* finished with combination record - write it out */
		db_put( dbip,  dp, &record, 0, 1 );

		/* create first member record */
		db_get( dbip,  dp, &record, 1, 1);
		(void)strcpy( record.M.m_instname, objp->d_namep );

		record.M.m_id = ID_MEMB;
		record.M.m_relation = relation;
		mat_idn( identity );
		rt_dbmat_mat( record.M.m_mat, identity );
		db_put( dbip,  dp, &record, 1, 1 );
		return( dp );
	}

	/*
	 * The named combination already exists.  Fetch the header record,
	 * and verify that this is a combination.
	 */
	db_get( dbip,  dp, &record, 0 , 1);
	if( record.u_id != ID_COMB )  {
		(void)printf("%s:  not a combination\n", combname );
		return DIR_NULL;
	}

	if( region_flag ) {
		if( record.c.c_flags != 'R' ) {
			(void)printf("%s: not a region\n",combname);
			return DIR_NULL;
		}
	}
	db_grow( dbip, dp, 1 );

	/* Fill in new Member record */
	db_get( dbip,  dp, &record, dp->d_len-1, 1);
	record.M.m_id = ID_MEMB;
	record.M.m_relation = relation;
	(void)strcpy( record.M.m_instname, objp->d_namep );

	mat_idn( identity );
	rt_dbmat_mat( record.M.m_mat, identity );
	db_put( dbip,  dp, &record, dp->d_len-1, 1 );
	return( dp );
}
