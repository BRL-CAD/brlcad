/*
 *			A N I M
 *
 *  Ray Tracing program, routines to apply animation directives.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSanim[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"


/*
 *			R T _ A D D _ A N I M
 *
 *  Add a user-supplied animate structure to the end of the chain of such
 *  structures hanging from the directory structure of the last node of
 *  the path specifier.  A pathlen of 0 indicates that this change is
 *  to affect the root of the tree itself, rather than an arc, and is
 *  stored differently.
 *
 *  In the future, might want to check to make sure that callers directory
 *  references are in the right model (rtip).
 */
int
rt_add_anim( rtip, anp )
struct rt_i *rtip;
register struct animate *anp;
{
	register struct animate **headp;
	register int i;

	/* Could validate an_type here */

	for( i=0; i < anp->an_pathlen; i++ )
		if( anp->an_path[i] == DIR_NULL )
			return(-1);	/* BAD */

	anp->an_forw = ANIM_NULL;
	if( anp->an_pathlen < 1 )
		headp = &(rtip->rti_anroot);
	else
		headp = &(anp->an_path[anp->an_pathlen-1]->d_animate);

	/* Append to list */
	while( *headp != ANIM_NULL )
		headp = &((*headp)->an_forw);
	*headp = anp;
	return(0);			/* OK */
}

/*
 *			R T _ D O _ A N I M
 *
 *  Perform the one animation operation.
 *  Leave results in form that additional operations can be cascaded.
 */
int
rt_do_anim( anp, stack, arc, materp )
register struct animate *anp;
mat_t	stack;
mat_t	arc;
struct mater_info	*materp;
{
	mat_t	temp;

	switch( anp->an_type )  {
	case AN_MATRIX:
		switch( anp->an_u.anu_m.anm_op )  {
		case ANM_RSTACK:
			mat_copy( stack, anp->an_u.anu_m.anm_mat );
			break;
		case ANM_RARC:
			mat_copy( arc, anp->an_u.anu_m.anm_mat );
			break;
		case ANM_RBOTH:
			mat_copy( stack, anp->an_u.anu_m.anm_mat );
			mat_idn( arc );
			break;
		case ANM_LMUL:
			/* arc = DELTA * arc */
			mat_mul( temp, anp->an_u.anu_m.anm_mat, arc );
			mat_copy( arc, temp );
			break;
		case ANM_RMUL:
			/* arc = arc * DELTA */
			mat_mul( temp, arc, anp->an_u.anu_m.anm_mat );
			mat_copy( arc, temp );
			break;
		default:
			return(-1);		/* BAD */
		}
		break;
	case AN_PROPERTY:
		break;
	default:
		/* Print something here? */
		return(-1);			/* BAD */
	}
	return(0);				/* OK */
}
