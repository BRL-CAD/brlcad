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
db_functree( dbip, dp, comb_func, leaf_func, client_data)
struct db_i	*dbip;
struct directory *dp;
void		(*comb_func)BU_ARGS((struct db_i *, struct directory *, genptr_t));
void		(*leaf_func)BU_ARGS((struct db_i *, struct directory *, genptr_t));
genptr_t	client_data;
{
	register union record	*rp;
	register int		i;
	register struct directory *mdp;

	RT_CK_DBI(dbip);
	if(rt_g.debug&DEBUG_DB) bu_log("db_functree(%s) x%x, x%x, comb=x%x, leaf=x%x, client_data=x%x\n",
		dp->d_namep, dbip, dp, comb_func, leaf_func, client_data );

	if( dp->d_flags & DIR_COMB )  {
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
			db_functree( dbip, mdp, comb_func, leaf_func, client_data );
		}

		/* Finally, the combination itself */
		if( comb_func )
			comb_func( dbip, dp, client_data );

		bu_free( (char *)rp, "db_functree record[]" );
	} else if( dp->d_flags & DIR_SOLID )  {
		if( leaf_func )
			leaf_func( dbip, dp, client_data );
	} else {
		bu_log("db_functree:  %s is neither COMB nor SOLID?\n",
			dp->d_namep );
	}
}
