/*
 *			D B _ W A L K . C
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"

#include "./debug.h"

/*
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
db_functree( dbip, dp, comb_func, leaf_func)
struct db_i	*dbip;
struct directory *dp;
void		(*comb_func)();
void		(*leaf_func)();
{
	register union record	*rp;
	register int		i;
	register struct directory *mdp;

	if( dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_gettree:  bad dbip\n");
	if(rt_g.debug&DEBUG_DB) bu_log("db_functree(%s) x%x, x%x, comb=x%x, leaf=x%x\n",
		dp->d_namep, dbip, dp, comb_func, leaf_func );

	/*
	 * Load the first record of the object into local record buffer
	 */
	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
		return;

	if( dp->d_flags & DIR_COMB )  {
		/* recurse */
		for( i=1; i < dp->d_len; i++ )  {
			if( (mdp = db_lookup( dbip, rp[i].M.m_instname,
			    LOOKUP_NOISY )) == DIR_NULL )
				continue;
			db_functree( dbip, mdp, comb_func, leaf_func );
		}

		/* Finally, the combination itself */
		if( comb_func )
			comb_func( dbip, dp );
	} else if( dp->d_flags & DIR_SOLID )  {
		if( leaf_func )
			leaf_func( dbip, dp );
	} else {
		bu_log("db_functree:  %s is neither COMB nor SOLID?\n",
			dp->d_namep );
	}
	rt_free( (char *)rp, "db_functree record[]" );
}

/*		C O M B _ F U N C T R E E
 *
 *	This routine traverses a combination (union tree) in LNR order
 *	and calls the provided function for each leaf node.
 *	Note that this routine does not go outside this one
 *	combination!!!!
 *
 */
void
comb_functree( dbip, comb_tree, leaf_func, user_ptr )
struct db_i		*dbip;
union tree		*comb_tree;
void			(*leaf_func)();
genptr_t		user_ptr;
{
	RT_CK_DBI( dbip );

	if( !comb_tree )
		return;

	RT_CK_TREE( comb_tree );

	switch( comb_tree->tr_op )
	{
		case OP_DB_LEAF:
			(*leaf_func)( dbip, comb_tree, user_ptr );
			break;
		case OP_UNION:
		case OP_INTERSECT:
		case OP_SUBTRACT:
		case OP_XOR:
			comb_functree( dbip, comb_tree->tr_b.tb_left, leaf_func, user_ptr );
			comb_functree( dbip, comb_tree->tr_b.tb_right, leaf_func, user_ptr );
			break;
		default:
			bu_log( "comb_functree: bad op %d\n", comb_tree->tr_op );
			bu_bomb( "comb_functree: bad op\n" );
			break;
	}
}
