/*
 *			M O V E R . C
 *
 * Functions -
 *	moveHobj	used to update position of an object in objects file
 *	moveinstance	Given a COMB and an object, modify all the regerences
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
 *	This software is Copyright (C) 1985-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



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
#include "./ged.h"
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
moveHobj(register struct directory *dp, matp_t xlate)
{
	struct rt_db_internal	intern;

	if(dbip == DBI_NULL)
	  return;

    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_db_get_internal( &intern, dp, dbip, xlate, &rt_uniresource ) < 0 )
	{
		Tcl_AppendResult(interp, "rt_db_get_internal() failed for ", dp->d_namep,
			(char *)NULL );
		rt_db_free_internal( &intern, &rt_uniresource );
		READ_ERR_return;
	}

	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )
	{
		Tcl_AppendResult(interp, "moveHobj(", dp->d_namep,
			   "):  solid export failure\n", (char *)NULL);
		rt_db_free_internal( &intern, &rt_uniresource );
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
moveHinstance(struct directory *cdp, struct directory *dp, matp_t xlate)
{
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;

	if(dbip == DBI_NULL)
	  return;

	if( rt_db_get_internal( &intern, cdp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
		READ_ERR_return;

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	if( comb->tree )
	{
		union tree *tp;

		tp = (union tree *)db_find_named_leaf( comb->tree, dp->d_namep );
		if( tp != TREE_NULL )
		{
			if( tp->tr_l.tl_mat )
				bn_mat_mul2( xlate, tp->tr_l.tl_mat );
			else
			{
				tp->tr_l.tl_mat = (matp_t)bu_malloc( 16 * sizeof( fastf_t ), "tl_mat" );
				MAT_COPY( tp->tr_l.tl_mat, xlate );
			}
			if( rt_db_put_internal( cdp, dbip, &intern, &rt_uniresource ) < 0 )
			{
				Tcl_AppendResult(interp, "rt_db_put_internal failed for ",
					cdp->d_namep, "\n", (char *)NULL );
				rt_db_free_internal( &intern, &rt_uniresource );
			}
		}
		else
		{
			Tcl_AppendResult(interp, "moveHinst:  couldn't find ", cdp->d_namep,
				"/", dp->d_namep, "\n", (char *)NULL);
			rt_db_free_internal( &intern, &rt_uniresource );
		}
	}
}
