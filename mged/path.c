/*
 *			P A T H . C
 *
 * Functions -
 *	drawHobj	Call drawsolid for all solids in an object
 *	pathHmat	Find matrix across a given path
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
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "./solid.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

int	regmemb;	/* # of members left to process in a region */
char	memb_oper;	/* operation for present member of processed region */
int	reg_pathpos;	/* pathpos of a processed region */

struct directory	*cur_path[MAX_PATH];	/* Record of current path */

/*
 *			D R A W H O B J
 *
 * This routine is used to get an object drawn.
 * The actual drawing of solids is performed by drawsolid(),
 * but all transformations down the path are done here.
 */
void
drawHobj( dp, flag, pathpos, old_xlate, regionid )
register struct directory *dp;
matp_t old_xlate;
{
	union record	*rp;
	auto mat_t	new_xlate;	/* Accumulated translation matrix */
	auto int	i;

	if( pathpos >= MAX_PATH )  {
		(void)printf("nesting exceeds %d levels\n", MAX_PATH );
		for(i=0; i<MAX_PATH; i++)
			(void)printf("/%s", cur_path[i]->d_namep );
		(void)putchar('\n');
		return;			/* ERROR */
	}

	/*
	 * Load the record into local record buffer
	 */
	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
		return;

	if( rp[0].u_id == ID_SOLID ||
	    rp[0].u_id == ID_ARS_A ||
	    rp[0].u_id == ID_BSOLID ||
	    rp[0].u_id == ID_P_HEAD )  {
		register struct solid *sp;
		/*
		 * Enter new solid (or processed region) into displaylist.
		 */
		cur_path[pathpos] = dp;

		GET_SOLID( sp );
		if( sp == SOLID_NULL )
			return;		/* ERROR */
		if( drawHsolid( sp, flag, pathpos, old_xlate, rp, regionid ) != 1 ) {
			FREE_SOLID( sp );
		}
		goto out;
	}

	if( rp[0].u_id != ID_COMB )  {
		(void)printf("drawobj:  defective input '%c'\n", rp[0].u_id );
		goto out;		/* ERROR */
	}
	if( dp->d_len <= 1 )  {
		(void)printf("Warning: combination with zero members \"%s\".\n",
			dp->d_namep );
		goto out;			/* non-fatal ERROR */
	}
	if( rp[0].c.c_flags == 'R' )  {
		if( regionid != 0 )
			(void)printf("regionid %d overriden by %d\n",
				regionid, rp[0].c.c_regionid );
		regionid = rp[0].c.c_regionid;
	}

	/*
	 *  This node is a combination (eg, a directory).
	 *  Process all the arcs (eg, directory members).
	 */
	if( drawreg && rp[0].c.c_flags == 'R' && dp->d_len > 1 ) {
		if( regmemb >= 0  ) {
			(void)printf(
			"ERROR: region (%s) is member of region (%s)\n",
				dp->d_namep,
				cur_path[reg_pathpos]->d_namep);
			goto out;	/* ERROR */
		}
		/* Well, we are processing regions and this is a region */
		/* if region has only 1 member, don't process as a region */
		if( dp->d_len > 2) {
			regmemb = dp->d_len-1;
			reg_pathpos = pathpos;
		}
	}

	/* Process all the member records */
	for( i=1; i < dp->d_len; i++ )  {
		register struct member	*mp;
		register struct directory *nextdp;
		static mat_t		xmat;	/* temporary fastf_t matrix */

		mp = &(rp[i].M);
		if( mp->m_id != ID_MEMB )  {
			fprintf(stderr,"drawHobj:  %s bad member rec\n",
				dp->d_namep);
			goto out;			/* ERROR */
		}
		cur_path[pathpos] = dp;
		if( regmemb > 0  ) { 
			regmemb--;
			memb_oper = mp->m_relation;
		}
		if( (nextdp = db_lookup( dbip,  mp->m_instname, LOOKUP_NOISY )) == DIR_NULL )
			continue;

		/* s' = M3 . M2 . M1 . s
		 * Here, we start at M3 and descend the tree.
		 * convert matrix to fastf_t from disk format.
		 */
		rt_mat_dbmat( xmat, mp->m_mat );
		mat_mul(new_xlate, old_xlate, xmat);

		/* Recursive call */
		drawHobj(
			nextdp,
			(mp->m_relation != SUBTRACT) ? ROOT : INNER,
			pathpos + 1,
			new_xlate,
			regionid
		);
	}
out:
	bu_free( (char *)rp, "drawHobj recs");
}

/*
 *  			P A T H h M A T
 *  
 *  Find the transformation matrix obtained when traversing
 *  the arc indicated in sp->s_path[] to the indicated depth.
 *  Be sure to omit s_path[sp->s_last] -- it's a solid.
 */
void
pathHmat( sp, matp, depth )
register struct solid *sp;
matp_t matp;
{
	register union record	*rp;
	register struct directory *parentp;
	register struct directory *kidp;
	register int		j;
	auto mat_t		tmat;
	register int		i;

	mat_idn( matp );
	for( i=0; i <= depth; i++ )  {
		parentp = sp->s_path[i];
		kidp = sp->s_path[i+1];
		if( !(parentp->d_flags & DIR_COMB) )  {
			printf("pathHmat:  %s is not a combination\n",
				parentp->d_namep);
			return;		/* ERROR */
		}
		if( (rp = db_getmrec( dbip, parentp )) == (union record *)0 )
			return;		/* ERROR */
		for( j=1; j < parentp->d_len; j++ )  {
			static mat_t xmat;	/* temporary fastf_t matrix */

			/* Examine Member records */
			if( strcmp( kidp->d_namep, rp[j].M.m_instname ) != 0 )
				continue;

			/* convert matrix to fastf_t from disk format */
			rt_mat_dbmat( xmat, rp[j].M.m_mat );
			mat_mul( tmat, matp, xmat );
			mat_copy( matp, tmat );
			goto next_level;
		}
		(void)printf("pathHmat: unable to follow %s/%s path\n",
			parentp->d_namep, kidp->d_namep );
		return;			/* ERROR */
next_level:
		bu_free( (char *)rp, "pathHmat recs");
	}
}
