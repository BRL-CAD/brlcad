/*
 *			M O V E R . C
 *
 * Functions -
 *	moveobj		used to update position of an object in objects file
 *	moveinstance	Given a COMB and an object, modify all the regerences
 *	combadd		Add an instance of an object to a combination
 *
 * The U. S. Army Ballistic Research Laboratory
 */

#include	<string.h>
#include "ged_types.h"
#include "3d.h"
#include "ged.h"
#include "dir.h"
#include "solid.h"
#include "vmath.h"

extern int	printf();

/*
 *			M O V E H O B J
 *
 * This routine is used when the object to be moved is
 * the top level in its reference path.  The object itself
 * is relocated.
 */
void
moveHobj( dp, xlate )
register struct directory *dp;
matp_t xlate;
{
	static int Nb_strsl;
	static int n;
	vect_t	work;			/* Working vector */
	register int i;
	register float *p;		/* -> to vector to be worked on */
	static float *area_end;		/* End of area to be processed */
	union record record;

	db_getrec( dp, &record, 0 );

	if( record.u_id == ID_ARS_A )  {
		Nb_strsl = record.a.a_totlen;

		/* 1st b type record  */
		db_getrec( dp, &record, 1 );

		/* displace the base vector */
		MAT4X3PNT( work, xlate, &record.b.b_values[0] );
		VMOVE( &record.b.b_values[0], work );

		/* Transform remaining vectors */
		for( p = &record.b.b_values[1*3]; p < &record.b.b_values[8*3];
								p += 3) {
			MAT4X3VEC( work, xlate, p );
			VMOVE( p, work );
		}
		db_putrec( dp, &record, 1 );

		/* process next (Nb_strcx * NX)-1  records  */
		for(n=2; n<=Nb_strsl; n++)  {
			db_getrec( dp, &record, n );
			/* Transform remaining vectors */
			for( p = &record.b.b_values[0*3];
					p < &record.b.b_values[8*3]; p += 3) {
				MAT4X3VEC( work, xlate, p );
				VMOVE( p, work );
			}
			db_putrec( dp, &record, n );
		}
		return;
	}

	if( record.u_id == ID_SOLID )  {

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
			(void)printf("moveobj:  cant move obj type %d\n",
					record.s.s_type );
			return;		/* ERROR */
		}
		db_putrec( dp, &record, 0 );
		return;
	}

	if( record.u_id != ID_COMB )  {
		(void)printf("MoveHobj -- bad disk record\n");
		return;			/* ERROR */
	}

	/*
	 * Move all the references within a combination
	 */
	for( i=1; i < dp->d_len; i++ )  {
		static mat_t temp;

		db_getrec( dp, &record, i );
		mat_mul( temp, xlate, record.M.m_mat );
		mat_copy( record.M.m_mat, temp );
		db_putrec( dp, &record, i );
	}
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
	mat_t temp;			/* Temporary for mat_mul */

	for( i=1; i < cdp->d_len; i++ )  {
		db_getrec( cdp, &record, i );

		/* Check for match, including alias (branch name) */
		if( strcmp( dp->d_namep, record.M.m_instname ) == 0 ||
		    strcmp( dp->d_namep, record.M.m_brname ) == 0 )  {
			/* Apply the Homogeneous Transformation Matrix */
			mat_mul(temp, xlate, record.M.m_mat);
			mat_copy( record.M.m_mat, temp );

			db_putrec( cdp, &record, i );
			return;
		}
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
 */
struct directory *
combadd( objp, combname, elementname, flag, relation, ident, air )
register struct directory *objp;
char *combname, *elementname;
int flag;				/* flag character */
int relation;				/* = UNION, SUBTRACT, INTERSECT */
int ident;				/* "Region ID" */
int air;				/* Air code */
{
	register struct directory *dp;
	union record record;
	int instf,regf,groupf;          /* instance, region, group flags */

	if ( elementname == NULL )
		elementname = "";	/* safety first */

	instf = 1;
	regf = groupf = 0;

	if( flag == 'r' )
		regf = 1;
	if( flag == 'g' )
		groupf = 1;
	if( regf == 1 || groupf == 1 )
		instf = 0;

	/*
	 * Check to see if we have to create a new combination
	 */
	if( (dp = lookup( combname, LOOKUP_QUIET )) == DIR_NULL )  {

		/* Update the in-core directory */
		dp = dir_add( combname, -1, DIR_COMB, 2 );
		if( dp == DIR_NULL )
			return DIR_NULL;
		db_alloc( dp, 2 );

		/* Generate the disk record */
		record.c.c_id = ID_COMB;
		record.c.c_length = 1;
		record.c.c_flags = record.c.c_aircode = 0;
		record.c.c_regionid = -1;
		record.c.c_material = record.c.c_los = 0;
		NAMEMOVE( combname, record.c.c_name );
		if( regf ) {       /* creating a region */
			record.c.c_flags = 'R';
			record.c.c_regionid = ident;
			record.c.c_aircode = air;
		}

		/* finished with combination record - write it out */
		db_putrec( dp, &record, 0 );

		/* create first member record */
		(void)strcpy( record.M.m_instname, objp->d_namep );

		if( instf )		/* creating an instance */
			/* Insert name of this branch */
			(void)strcpy( record.M.m_brname, elementname );
		else
			record.M.m_brname[0] = '\0';

		record.M.m_id = ID_MEMB;
		record.M.m_relation = relation;
		mat_idn( record.M.m_mat );

		db_putrec( dp, &record, 1 );
		return( dp );
	}

	/*
	 * The named combination already exists.  Fetch the header record,
	 * and verify that this is a combination.
	 */
	db_getrec( dp, &record, 0 );
	if( record.u_id != ID_COMB )  {
		(void)printf("%s:  not a combination\n", combname );
		return DIR_NULL;
	}

	if( regf ) {
		if( record.c.c_flags != 'R' ) {
			(void)printf("%s: not a region\n",combname);
			return DIR_NULL;
		}
	}
	record.c.c_length++;
	db_putrec( dp, &record, 0 );
	db_grow( dp, 1 );

	/* Fill in new Member record */
	record.M.m_id = ID_MEMB;
	record.M.m_relation = relation;
	mat_idn( record.M.m_mat );
	(void)strcpy( record.M.m_instname, objp->d_namep );

	if( instf )
		/* Record the name of this branch */
		(void)strcpy( record.M.m_brname, elementname );
	else
		record.M.m_brname[0] = '\0';

	db_putrec( dp, &record, dp->d_len-1 );
	return( dp );
}
