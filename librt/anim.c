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
 *  the path specifier.  When 'root' is non-zero, this matrix is
 *  located at the root of the tree itself, rather than an arc, and is
 *  stored differently.
 *
 *  In the future, might want to check to make sure that callers directory
 *  references are in the right model (rtip).
 */
int
rt_add_anim( rtip, anp, root )
struct rt_i *rtip;
register struct animate *anp;
int	root;
{
	register struct animate **headp;
	register int i;

	/* Could validate an_type here */

	for( i=0; i < anp->an_pathlen; i++ )
		if( anp->an_path[i] == DIR_NULL )
			return(-1);	/* BAD */

	anp->an_forw = ANIM_NULL;
	if( root )  {
		if( rt_g.debug&DEBUG_ANIM )
			rt_log("rt_add_anim(x%x) root\n", anp);
		headp = &(rtip->rti_anroot);
	} else {
		if( rt_g.debug&DEBUG_ANIM )
			rt_log("rt_add_anim(x%x) arc %s\n", anp,
				anp->an_path[anp->an_pathlen-1]->d_namep);
		headp = &(anp->an_path[anp->an_pathlen-1]->d_animate);
	}

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

	if( rt_g.debug&DEBUG_ANIM )
		rt_log("rt_do_anim(x%x) ", anp);
	switch( anp->an_type )  {
	case AN_MATRIX:
		if( rt_g.debug&DEBUG_ANIM )  {
			rt_log("matrix, op=%d\n", anp->an_u.anu_m.anm_op);
#if 0
			mat_print("on original arc", arc);
			mat_print("on original stack", stack);
#endif
		}
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
#if 0
		if( rt_g.debug&DEBUG_ANIM )  {
			mat_print("arc result", arc);
			mat_print("stack result", stack);
		}
#endif
		break;
	case AN_PROPERTY:
		if( rt_g.debug&DEBUG_ANIM )
			rt_log("property\n");
		break;
	default:
		if( rt_g.debug&DEBUG_ANIM )
			rt_log("unknown op\n");
		/* Print something here? */
		return(-1);			/* BAD */
	}
	return(0);				/* OK */
}

/*
 *			R T _ F R _ A N I M
 *
 *  Release chain of animation structures
 */
void
rt_fr_anim( rtip )
register struct rt_i *rtip;
{
	register struct animate *anp;
	register struct directory *dp;
	register int		i;

	/* Rooted animations */
	for( anp = rtip->rti_anroot; anp != ANIM_NULL; )  {
		register struct animate *nextanp = anp->an_forw;

		rt_free( (char *)anp->an_path, "animation path[]");
		rt_free( (char *)anp, "struct animate");
		anp = nextanp;
	}
	rtip->rti_anroot = ANIM_NULL;

	/* Node animations */
	for( i=0; i < RT_DBNHASH; i++ )  {
		dp = rtip->rti_dbip->dbi_Head[i];
		for( ; dp != DIR_NULL; dp = dp->d_forw )  {
			for( anp = dp->d_animate; anp != ANIM_NULL; )  {
				register struct animate *nextanp = anp->an_forw;

				rt_free( (char *)anp->an_path, "animation path[]");
				rt_free( (char *)anp, "struct animate");
				anp = nextanp;
			}
			dp->d_animate = ANIM_NULL;
		}
	}
}
