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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./mged_solid.h"

/* default region ident codes */
int	item_default = 1000;	/* GIFT region ID */
int	air_default = 0;
int	mat_default = 1;	/* GIFT material code */
int	los_default = 100;	/* Line-of-sight estimate */

/*
 *			M O V E H O B J
 *
 * This routine is used when the object to be moved is
 * the top level in its reference path.
 * The object itself (solid or "leaf" combination) is relocated.
 */
void
moveHobj( dp, xlate )
register struct directory *dp;
matp_t xlate;
{
	struct bu_external	ext;
	struct rt_db_internal	intern;
	register int		i;
	union record		*rec;
	int			id;

	if( db_get_external( &ext, dp, dbip ) < 0 )
		READ_ERR_return;

	rec = (union record *)ext.ext_buf;
	if( rec->u_id == ID_COMB )  {
		/*
		 * Move all the references within a combination
		 * XXX should use combination import/export routines here too!
		 */
		for( i=1; i < dp->d_len; i++ )  {
			static mat_t temp, xmat;

			rt_mat_dbmat( xmat, rec[i].M.m_mat );
			bn_mat_mul( temp, xlate, xmat );
			rt_dbmat_mat( rec[i].M.m_mat, temp );
		}
		if( db_put_external( &ext, dp, dbip ) < 0 )  {
		  db_free_external( &ext );
		  TCL_WRITE_ERR;
		  return;
		}
		db_free_external( &ext );
		return;				/* OK */
	}

	/*
	 *  Import the solid, applying the transform on the way in.
	 *  Then, export it, and re-write the database record.
	 *  Will work on all solids.
	 */
	if( (id = rt_id_solid( &ext )) == ID_NULL )  {
	  Tcl_AppendResult(interp, "moveHobj(", dp->d_namep,
			   ") unable to identify type\n", (char *)NULL);
	  return;				/* FAIL */
	}

    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, &ext, xlate ) < 0 )  {
	  Tcl_AppendResult(interp, "moveHobj(", dp->d_namep,
			   "):  solid import failure\n", (char *)NULL);
	  if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
	  db_free_external( &ext );
	  return;				/* FAIL */
	}
	RT_CK_DB_INTERNAL( &intern );
	db_free_external( &ext );

	if( rt_db_put_internal( dp, dbip, &intern ) < 0 )  {
	  Tcl_AppendResult(interp, "moveHobj(", dp->d_namep,
			   "):  solid export failure\n", (char *)NULL);
	  if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
	  TCL_WRITE_ERR;
	  return;
	}
	return;					/* OK */
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
	union record	*rec;
	mat_t temp, xmat;		/* Temporary for mat_mul */

	if( (rec = db_getmrec( dbip, cdp )) == (union record *)0 )
		READ_ERR_return;
	for( i=1; i < cdp->d_len; i++ )  {
		/* Check for match */
		if( strcmp( dp->d_namep, rec[i].M.m_instname ) != 0 )
			continue;

		rt_mat_dbmat( xmat, rec[i].M.m_mat );
		bn_mat_mul(temp, xlate, xmat);
		rt_dbmat_mat( rec[i].M.m_mat, temp );

		if( db_put( dbip,  cdp, rec, 0, cdp->d_len ) < 0 )
			WRITE_ERR_return;
		bu_free( (genptr_t)rec, "union record");
		return;
	}
	bu_free( (genptr_t)rec, "union record");
	Tcl_AppendResult(interp, "moveinst:  couldn't find ", cdp->d_namep,
			 "/", dp->d_namep, "\n", (char *)NULL);
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
#if 0
	mat_t	identity;
#endif
	/*
	 * Check to see if we have to create a new combination
	 */
	if( (dp = db_lookup( dbip,  combname, LOOKUP_QUIET )) == DIR_NULL )  {

		/* Update the in-core directory */
		if( (dp = db_diradd( dbip, combname, -1, 2, DIR_COMB )) == DIR_NULL ||
		    db_alloc( dbip, dp, 2 ) < 0 )  {
		  Tcl_AppendResult(interp, "An error has occured while adding '",
				   combname, "' to the database.\n", (char *)NULL);
		  TCL_ERROR_RECOVERY_SUGGESTION;
		  return DIR_NULL;
		}

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
		  struct bu_vls tmp_vls;

		  
		  record.c.c_flags = 'R';
		  record.c.c_regionid = ident;
		  record.c.c_aircode = air;
		  record.c.c_los = los_default;
		  record.c.c_material = mat_default;
		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls,
				"Creating region id=%d, air=%d, los=%d, GIFTmaterial=%d\n",
				ident, air, los_default, mat_default );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		}

		/* finished with combination record - write it out */
		if( db_put( dbip,  dp, &record, 0, 1 ) < 0 )  {
		  Tcl_AppendResult(interp, "write error, aborting\n", (char *)NULL);
		  TCL_ERROR_RECOVERY_SUGGESTION;
		  return DIR_NULL;
		}

		/* create first member record */
		if( db_get( dbip,  dp, &record, 1, 1) < 0 )  {
		  Tcl_AppendResult(interp, "read error, aborting\n", (char *)NULL);
		  TCL_ERROR_RECOVERY_SUGGESTION;
		  return DIR_NULL;
		}
		(void)strcpy( record.M.m_instname, objp->d_namep );

		record.M.m_id = ID_MEMB;
		record.M.m_relation = relation;
#if 0
		bn_mat_idn( identity );
#endif
		rt_dbmat_mat( record.M.m_mat, identity );
		if( db_put( dbip,  dp, &record, 1, 1 ) < 0 )  {
		  Tcl_AppendResult(interp, "write error, aborting\n", (char *)NULL);
		  TCL_ERROR_RECOVERY_SUGGESTION;
		  return DIR_NULL;
		}
		return( dp );
	}

	/*
	 * The named combination already exists.  Fetch the header record,
	 * and verify that this is a combination.
	 */
	if( db_get( dbip,  dp, &record, 0 , 1) < 0 )  {
	  Tcl_AppendResult(interp, "read error, aborting\n", (char *)NULL);
	  TCL_ERROR_RECOVERY_SUGGESTION;
	  return DIR_NULL;
	}
	if( record.u_id != ID_COMB )  {
	  Tcl_AppendResult(interp, combname, ":  not a combination\n", (char *)NULL);
	  return DIR_NULL;
	}

	if( region_flag ) {
	  if( record.c.c_flags != 'R' ) {
	    Tcl_AppendResult(interp, combname, ": not a region\n", (char *)NULL);
	    return DIR_NULL;
	  }
	}
	if( db_grow( dbip, dp, 1 ) < 0 )  {
	  Tcl_AppendResult(interp, "db_grow error, aborting\n", (char *)NULL);
	  TCL_ERROR_RECOVERY_SUGGESTION;
	  return DIR_NULL;
	}

	/* Fill in new Member record */
	if( db_get( dbip,  dp, &record, dp->d_len-1, 1) < 0 )  {
	  Tcl_AppendResult(interp, "read error, aborting\n", (char *)NULL);
	  TCL_ERROR_RECOVERY_SUGGESTION;
	  return DIR_NULL;
	}
	record.M.m_id = ID_MEMB;
	record.M.m_relation = relation;
	(void)strcpy( record.M.m_instname, objp->d_namep );
#if 0
	bn_mat_idn( identity );
#endif
	rt_dbmat_mat( record.M.m_mat, identity );
	if( db_put( dbip,  dp, &record, dp->d_len-1, 1 ) < 0 )  {
	  Tcl_AppendResult(interp, "write error, aborting\n", (char *)NULL);
	  TCL_ERROR_RECOVERY_SUGGESTION;
	  return DIR_NULL;
	}
	return( dp );
}
