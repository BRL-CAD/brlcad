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
	int			id;

	if(dbip == DBI_NULL)
	  return;

    	RT_INIT_DB_INTERNAL(&intern);
	if( (id=rt_db_get_internal( &intern, dp, dbip, xlate )) < 0 )
	{
		Tcl_AppendResult(interp, "rt_db_get_internal() failed for ", dp->d_namep,
			(char *)NULL );
		if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		READ_ERR_return;
	}

	if( rt_db_put_internal( dp, dbip, &intern ) < 0 )
	{
		Tcl_AppendResult(interp, "moveHobj(", dp->d_namep,
			   "):  solid export failure\n", (char *)NULL);
		if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		TCL_WRITE_ERR;
		return;
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
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	int found=0;
	mat_t temp, xmat;		/* Temporary for mat_mul */

	if(dbip == DBI_NULL)
	  return;

	if( rt_db_get_internal( &intern, cdp, dbip, (mat_t *)NULL ) < 0 )
		READ_ERR_return;

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	if( comb->tree )
	{
		union tree *tp;

		tp = (union tree *)db_find_named_leaf( comb->tree, dp->d_namep );
		if( tp != TREE_NULL )
		{
			found = 1;
			bn_mat_mul2( xlate, tp->tr_l.tl_mat );
			if( rt_db_put_internal( cdp, dbip, &intern ) < 0 )
			{
				Tcl_AppendResult(interp, "rt_db_put_internal failed for ",
					cdp->d_namep, "\n", (char *)NULL );
				rt_comb_ifree( &intern );
			}
		}
		else
		{
			Tcl_AppendResult(interp, "moveinst:  couldn't find ", cdp->d_namep,
				"/", dp->d_namep, "\n", (char *)NULL);
			rt_comb_ifree( &intern );
		}
			
	}
}

/*
 *			C O M B A D D
 *
 * Add an instance of object 'dp' to combination 'name'.
 * If the combination does not exist, it is created.
 * region_flag is 1 (region), or 0 (group).
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
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	union tree *tp;
	struct rt_tree_array *tree_list;
	int node_count;
	int actual_count;

	if(dbip == DBI_NULL)
	  return DIR_NULL;

	/*
	 * Check to see if we have to create a new combination
	 */
	if( (dp = db_lookup( dbip,  combname, LOOKUP_QUIET )) == DIR_NULL )  {
		int flags;

		if( region_flag )
			flags = DIR_REGION | DIR_COMB;
		else
			flags = DIR_COMB;

		/* Update the in-core directory */
		if( (dp = db_diradd( dbip, combname, -1, 2, flags )) == DIR_NULL ||
		    db_alloc( dbip, dp, 2 ) < 0 )  {
		  Tcl_AppendResult(interp, "An error has occured while adding '",
				   combname, "' to the database.\n", (char *)NULL);
		  TCL_ERROR_RECOVERY_SUGGESTION;
		  return DIR_NULL;
		}

		BU_GETSTRUCT( comb, rt_comb_internal );
		comb->magic = RT_COMB_MAGIC;
		bu_vls_init( &comb->shader );
		bu_vls_init( &comb->material );
		comb->region_id = -1;
		comb->tree = TREE_NULL;

		RT_INIT_DB_INTERNAL( &intern );
		intern.idb_type = ID_COMBINATION;
		intern.idb_ptr = (genptr_t)comb;

		if( region_flag )
		{
			struct bu_vls tmp_vls;

			comb->region_flag = 1;
			comb->region_id = ident;
			comb->aircode = air;
			comb->los = los_default;
			comb->GIFTmater = mat_default;
			bu_vls_init(&tmp_vls);
			bu_vls_printf(&tmp_vls,
				"Creating region id=%d, air=%d, los=%d, GIFTmaterial=%d\n",
				ident, air, los_default, mat_default );
			Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			bu_vls_free(&tmp_vls);
		}

		BU_GETUNION( tp, tree );
		tp->magic = RT_TREE_MAGIC;
		tp->tr_l.tl_op = OP_DB_LEAF;
		tp->tr_l.tl_name = bu_strdup( objp->d_namep );
		tp->tr_l.tl_mat = (matp_t)NULL;
		comb->tree = tp;

		if( rt_db_put_internal( dp, dbip, &intern ) < 0 )
		{
			Tcl_AppendResult(interp, "Failed to write ", dp->d_namep, (char *)NULL );
			return( DIR_NULL );
		}
		return( dp );
	}

	/* combination exists, add a new member */
	if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
	{
		Tcl_AppendResult(interp, "read error, aborting\n", (char *)NULL);
		TCL_ERROR_RECOVERY_SUGGESTION;
		return DIR_NULL;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB( comb );

	if( region_flag && !comb->region_flag )
	{
		Tcl_AppendResult(interp, combname, ": not a region\n", (char *)NULL);
		return DIR_NULL;
	}

	if( comb->tree && db_ck_v4gift_tree( comb->tree ) < 0 )
	{
		db_non_union_push( comb->tree );
		if( db_ck_v4gift_tree( comb->tree ) < 0 )
		{
			Tcl_AppendResult(interp, "Cannot flatten tree for editing\n", (char *)NULL );
			rt_comb_ifree( comb );
			return DIR_NULL;
		}
	}

	/* make space for an extra leaf */
	node_count = db_tree_nleaves( comb->tree ) + 1;
	tree_list = (struct rt_tree_array *)bu_calloc( node_count,
		sizeof( struct rt_tree_array ), "tree list" );

	/* flatten tree */
	actual_count = 1 + (struct rt_tree_array *)db_flatten_tree( tree_list, comb->tree, OP_UNION ) - tree_list;
	if( actual_count > node_count )  bu_bomb("combadd() array overflow!");
	if( actual_count < node_count )  bu_log("WARNING combadd() array underflow! %d", actual_count, node_count);

	/* insert new member at end */
	switch( relation )
	{
		case '+':
			tree_list[node_count - 1].tl_op = OP_INTERSECT;
			break;
		case '-':
			tree_list[node_count - 1].tl_op = OP_SUBTRACT;
			break;
		default:
			Tcl_AppendResult(interp, "unrecognized relation (assume UNION)\n",
				(char *)NULL );
		case 'u':
			tree_list[node_count - 1].tl_op = OP_UNION;
			break;
	}

	/* make new leaf node, and insert at end of list */
	BU_GETUNION( tp, tree );
	tree_list[node_count-1].tl_tree = tp;
	tp->tr_l.magic = RT_TREE_MAGIC;
	tp->tr_l.tl_op = OP_DB_LEAF;
	tp->tr_l.tl_name = bu_strdup( objp->d_namep );
	tp->tr_l.tl_mat = (matp_t)NULL;

	/* rebuild the tree */
	comb->tree = (union tree *)db_mkgift_tree( tree_list, node_count, (struct db_tree_state *)NULL );

	/* increase the length of this record */
	if( db_grow( dbip, dp, 1 ) < 0 )
	{
		Tcl_AppendResult(interp, "db_grow error, aborting\n", (char *)NULL);
		TCL_ERROR_RECOVERY_SUGGESTION;
		return DIR_NULL;
	}

	/* and finally, write it out */
	if( rt_db_put_internal( dp, dbip, &intern ) < 0 )
	{
		Tcl_AppendResult(interp, "Failed to write ", dp->d_namep, (char *)NULL );
		return( DIR_NULL );
	}

	bu_free( (char *)tree_list, "combadd: tree_list" );

	return( dp );
}
