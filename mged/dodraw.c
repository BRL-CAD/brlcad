/*
 *			D O D R A W . C
 *
 * Functions -
 *	drawHsolid	Manage the drawing of a COMGEOM solid
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
#include "raytrace.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"

struct vlist	*rtg_vlFree;	/* should be rt_g.rtg_vlFree !! XXX dm.h */

int	reg_error;	/* error encountered in region processing */
int	no_memory;	/* flag indicating memory for drawing is used up */
long	nvectors;	/* number of vectors drawn so far */

extern struct directory	*cur_path[MAX_PATH];	/* from path.c */

/*
 *			D R A W H S O L I D
 *
 * Returns -
 *	-1	on error
 *	 0	if NO OP
 *	 1	if solid was drawn
 */
int
drawHsolid( sp, flag, pathpos, xform, recordp, regionid )
register struct solid *sp;		/* solid structure */
int flag;
int pathpos;
matp_t xform;
union record *recordp;
int regionid;
{
	register struct vlist *vp;
	register int i;
	int dashflag;		/* draw with dashed lines */
	int count;
	struct vlhead	vhead;
	vect_t		max, min;

	vhead.vh_first = vhead.vh_last = VL_NULL;
	if( regmemb >= 0 ) {
		/* processing a member of a processed region */
		/* regmemb  =>  number of members left */
		/* regmemb == 0  =>  last member */
		/* reg_error > 0  =>  error condition  no more processing */
		if(reg_error) { 
			if(regmemb == 0) {
				reg_error = 0;
				regmemb = -1;
			}
			return(-1);		/* ERROR */
		}
		if(memb_oper == UNION)
			flag = 999;

		/* The hard part */
		i = proc_reg( recordp, xform, flag, regmemb, &vhead );

		if( i < 0 )  {
			/* error somwhere */
			(void)printf("will skip region: %s\n",
					cur_path[reg_pathpos]->d_namep);
			reg_error = 1;
			if(regmemb == 0) {
				regmemb = -1;
				reg_error = 0;
			}
			return(-1);		/* ERROR */
		}
		reg_error = 0;		/* reset error flag */

		/* if more member solids to be processed, no drawing was done
		 */
		if( i > 0 )
			return(0);		/* NOP */
		dashflag = 0;
	}  else  {
		/* Doing a normal solid */
		int id;

		dashflag = (flag != ROOT);

		id = rt_id_solid( recordp );
		if( id < 0 || id >= rt_nfunctab )  {
			printf("drawHsolid(%s):  unknown database object\n",
				cur_path[pathpos]->d_namep);
			return(-1);			/* ERROR */
		}

		rt_functab[id].ft_plot( recordp, xform, &vhead,
			cur_path[pathpos] );
	}

	/*
	 * Compute the min, max, and center points.
	 */
	VSETALL( max, -INFINITY );
	VSETALL( min,  INFINITY );
	sp->s_vlist = vhead.vh_first;
	sp->s_vlen = 0;
	for( vp = vhead.vh_first; vp != VL_NULL; vp = vp->vl_forw )  {
		VMINMAX( min, max, vp->vl_pnt );
		sp->s_vlen++;
	}
	nvectors += sp->s_vlen;

	VADD2SCALE( sp->s_center, min, max, 0.5 );

	sp->s_size = max[X] - min[X];
	MAX( sp->s_size, max[Y] - min[Y] );
	MAX( sp->s_size, max[Z] - min[Z] );

	/*
	 * If this solid is not illuminated, fill in it's information.
	 * A solid might be illuminated yet vectorized again by redraw().
	 */
	if( sp != illump )  {
		sp->s_iflag = DOWN;
		sp->s_soldash = dashflag;

		if(regmemb == 0) {
			/* done processing a region */
			regmemb = -1;
			sp->s_last = reg_pathpos;
			sp->s_Eflag = 1;	/* This is processed region */
		}  else  {
			sp->s_Eflag = 0;	/* This is a solid */
			sp->s_last = pathpos;
		}
		/* Copy path information */
		for( i=0; i<=sp->s_last; i++ )
			sp->s_path[i] = cur_path[i];
	}
	sp->s_materp = (char *)0;
	sp->s_regionid = regionid;
	sp->s_addr = 0;
	sp->s_bytes = 0;

	/* Cvt to displaylist, determine displaylist memory requirement. */
	if( !no_memory && (sp->s_bytes = dmp->dmr_cvtvecs( sp )) != 0 )  {
		/* Allocate displaylist storage for object */
		sp->s_addr = memalloc( &(dmp->dmr_map), sp->s_bytes );
		if( sp->s_addr == 0 )  {
			no_memory = 1;
			(void)printf("draw: out of Displaylist\n");
			sp->s_bytes = 0;	/* not drawn */
		} else {
			sp->s_bytes = dmp->dmr_load(sp->s_addr, sp->s_bytes );
		}
	}

	/* Solid is successfully drawn */
	if( sp != illump )  {
		/* Add to linked list of solid structs */
		APPEND_SOLID( sp, HeadSolid.s_back );
		dmp->dmr_viewchange( DM_CHGV_ADD, sp );
	} else {
		/* replacing illuminated solid -- struct already linked in */
		sp->s_iflag = UP;
		dmp->dmr_viewchange( DM_CHGV_REPL, sp );
	}

	return(1);		/* OK */
}
