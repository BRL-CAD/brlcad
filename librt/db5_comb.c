/*
 *			D B 5 _ C O M B . C
 *
 *  Handle import/export of combinations (union tree) in v5 format.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db5.h"
#include "raytrace.h"

#include "./debug.h"


/*
 *			D B _ T R E E _ N L E A V E S
 *
 *  Count number of non-identity matricies,
 *  number of leaf nodes, number of operator nodes.
 */
void
db_tree_counter( tp, noperp, nleafp, nmatp )
CONST union tree	*tp;
long			*noperp;
long			*nleafp;
long			*nmatp;
{
	if( tp == TREE_NULL )  return;
	RT_CK_TREE(tp);

	switch( tp->tr_op )  {
	case OP_NOP:
		(*nleafp)++;
		return;
	case OP_DB_LEAF:
		(*nleafp)++;
		return;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		/* Unary ops */
		(*noperp)++;
		db_tree_counter( tp->tr_b.tb_left, noperp, nleafp, nmatp );
		return;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		(*noperp)++;
		db_tree_counter( tp->tr_b.tb_left, noperp, nleafp, nmatp );
		db_tree_counter( tp->tr_b.tb_right, noperp, nleafp, nmatp );
		return;

	default:
		bu_log("db_tree_counter: bad op %d\n", tp->tr_op);
		bu_bomb("db_tree_counter\n");
	}
}

/*
 *			R T _ C O M B _ E X P O R T 5
 */
int
rt_comb_export5( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_comb_internal	*comb;
	long	noper=0, nleaf=0, nmat=0;

	RT_CK_DB_INTERNAL( ip );
	RT_CK_DBI(dbip);

	if( ip->idb_type != ID_COMBINATION ) bu_bomb("rt_comb_v4_export() type not ID_COMBINATION");
	comb = (struct rt_comb_internal *)ip->idb_ptr;
	RT_CK_COMB(comb);

	/* First pass -- count number of non-identity matricies,
	 * number of leaf nodes, number of operator nodes.
	 */
	db_tree_counter( comb->tree, &noper, &nleaf, &nmat );

	return 0;
}
