/*                       D B _ W A L K . C
 * BRL-CAD
 *
 * Copyright (C) 1988-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup db */

/*@{*/
/** @file db_walk.c
 *
 * Functions -
 *	db_functree	No-frills tree-walk
 *	comb_functree	No-frills combination-walk
 *
 *
 *  Authors -
 *	Michael John Muuss
 *	John R. Anderson
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"

#include "./debug.h"

/*	D B _ F U N C T R E E _ S U B T R E E
 *
 *	The only reason for this to be broken out is that
 *	2 separate locations in db_functree() call it.
 */
void
db_functree_subtree(struct db_i *dbip,
		    union tree *tp,
		    void (*comb_func) (struct db_i *, struct directory *, genptr_t),
		    void (*leaf_func) (struct db_i *, struct directory *, genptr_t),
		    struct resource *resp,
		    genptr_t client_data)
{
	struct directory *dp;

	if( !tp )
		return;

	RT_CHECK_DBI( dbip );
	RT_CK_TREE( tp );
	RT_CK_RESOURCE( resp );

	switch( tp->tr_op )  {

		case OP_DB_LEAF:
			if( (dp=db_lookup( dbip, tp->tr_l.tl_name, LOOKUP_NOISY )) == DIR_NULL )
				return;
			db_functree( dbip, dp, comb_func, leaf_func, resp, client_data);
			break;

		case OP_UNION:
		case OP_INTERSECT:
		case OP_SUBTRACT:
		case OP_XOR:
			db_functree_subtree( dbip, tp->tr_b.tb_left, comb_func, leaf_func, resp, client_data );
			db_functree_subtree( dbip, tp->tr_b.tb_right, comb_func, leaf_func, resp, client_data );
			break;
		default:
			bu_log( "db_functree_subtree: unrecognized operator %d\n", tp->tr_op );
			bu_bomb( "db_functree_subtree: unrecognized operator\n" );
	}
}

/**
 *			D B _ F U N C T R E E
 *
 *  This subroutine is called for a no-frills tree-walk,
 *  with the provided subroutines being called at every combination
 *  and leaf (solid) node, respectively.
 *
 *  This routine is recursive, so no variables may be declared static.
 *
 */
void
db_functree(struct db_i *dbip,
	    struct directory *dp,
	    void (*comb_func) (struct db_i *, struct directory *, genptr_t),
	    void (*leaf_func) (struct db_i *, struct directory *, genptr_t),
	    struct resource *resp,
	    genptr_t client_data)
{
	register int		i;

	RT_CK_DBI(dbip);
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db_functree(%s) x%x, x%x, comb=x%x, leaf=x%x, client_data=x%x\n",
		dp->d_namep, dbip, dp, comb_func, leaf_func, client_data );

	if( dp->d_flags & DIR_COMB )  {
		if( dbip->dbi_version < 5 ) {
			register union record	*rp;
			register struct directory *mdp;
			/*
			 * Load the combination into local record buffer
			 * This is in external v4 format.
			 */
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
				return;

			/* recurse */
			for( i=1; i < dp->d_len; i++ )  {
				if( (mdp = db_lookup( dbip, rp[i].M.m_instname,
				    LOOKUP_NOISY )) == DIR_NULL )
					continue;
				db_functree( dbip, mdp, comb_func, leaf_func, resp, client_data );
			}
			bu_free( (char *)rp, "db_functree record[]" );
		} else {
			struct rt_db_internal in;
			struct rt_comb_internal *comb;

			if( rt_db_get_internal5( &in, dp, dbip, NULL, resp ) < 0 )
				return;

			comb = (struct rt_comb_internal *)in.idb_ptr;
			db_functree_subtree( dbip, comb->tree, comb_func, leaf_func, resp, client_data );
			rt_db_free_internal( &in, resp );
		}

		/* Finally, the combination itself */
		if( comb_func )
			comb_func( dbip, dp, client_data );

	} else if( dp->d_flags & DIR_SOLID || dp->d_major_type & DB5_MAJORTYPE_BINARY_MASK )  {
		if( leaf_func )
			leaf_func( dbip, dp, client_data );
	} else {
		bu_log("db_functree:  %s is neither COMB nor SOLID?\n",
			dp->d_namep );
	}
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
