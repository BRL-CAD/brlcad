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

#include "./machine.h"	/* special copy */
#include "../h/vmath.h"
#include "../h/db.h"
#include "ged.h"
#include "solid.h"
#include "objdir.h"
#include "dm.h"

#ifdef BSD42
extern void bcopy();
#define	memcpy(to,from,cnt)	bcopy(from,to,cnt)
#else
extern char *memcpy();
#endif

extern void	perror();
extern char	*malloc();
extern int	printf(), write();

#define NVL	2000
static struct veclist veclist[NVL];

struct veclist *vlp;		/* pointer to first free veclist element */
struct veclist *vlend = &veclist[NVL]; /* pntr to 1st inval veclist element */

int	reg_error;	/* error encountered in region processing */
int	no_memory;	/* flag indicating memory for drawing is used up */

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
	register struct veclist *vp;
	register int i;
	int dashflag;		/* draw with dashed lines */
	int count;
	static float xmax, ymax, zmax;
	static float xmin, ymin, zmin;

	vlp = &veclist[0];
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
		i = proc_reg( recordp, xform, flag, regmemb );

		if( i < 0 )  {
			/* error somwhere */
			(void)printf("will skip region: %s\n",
					path[reg_pathpos]->d_namep);
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
		dashflag = (flag != ROOT);
	
		switch( recordp->u_id )  {

		case ID_SOLID:
			switch( recordp->s.s_type )  {

			case GENARB8:
				draw_arb8( &recordp->s, xform );
				break;

			case GENTGC:
				draw_tgc( &recordp->s, xform );
				break;

			case GENELL:
				draw_ell( &recordp->s, xform );
				break;

			case TOR:
				draw_tor( &recordp->s, xform );
				break;

			default:
				(void)printf("draw:  bad SOLID type %d.\n",
					recordp->s.s_type );
				return(-1);		/* ERROR */
			}
			break;

		case ID_ARS_A:
			draw_ars( &recordp->a, path[pathpos], xform );
			break;

		case ID_B_SPL_HEAD:
			draw_spline( &recordp->a, path[pathpos], xform );
			break;

		case ID_P_HEAD:
			draw_poly( path[pathpos], xform );
			break;

		default:
			(void)printf("draw:  bad database OBJECT type %d\n",
				recordp->u_id );
			return(-1);			/* ERROR */
		}
	}

	/*
	 * The vector list is now safely stored in veclist[].
	 * Compute the min, max, and center points.
	 */
#define INFINITY	100000000.0
	xmax = ymax = zmax = -INFINITY;
	xmin = ymin = zmin =  INFINITY;
	for( vp = &veclist[0]; vp < vlp; vp++ )  {
		MIN( xmin, vp->vl_pnt[X] );
		MAX( xmax, vp->vl_pnt[X] );
		MIN( ymin, vp->vl_pnt[Y] );
		MAX( ymax, vp->vl_pnt[Y] );
		MIN( zmin, vp->vl_pnt[Z] );
		MAX( zmax, vp->vl_pnt[Z] );
	}
	VSET( sp->s_center,
		(xmax + xmin)/2, (ymax + ymin)/2, (zmax + zmin)/2 );

	sp->s_size = xmax - xmin;
	MAX( sp->s_size, ymax - ymin );
	MAX( sp->s_size, zmax - zmin );

	/* Make a private copy of the vector list */
	sp->s_vlen = vlp - &veclist[0];		/* # of structs */
	count = sp->s_vlen * sizeof(struct veclist);
	if( (sp->s_vlist = (struct veclist *)malloc((unsigned)count)) == VLIST_NULL )  {
		no_memory = 1;
		(void)printf("draw: malloc error\n");
		return(-1);		/* ERROR */
	}
	(void)memcpy( (char *)sp->s_vlist, (char *)veclist, count );

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
			sp->s_path[i] = path[i];
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
			return(-1);		/* ERROR */
		}
		sp->s_bytes = dmp->dmr_load( sp->s_addr, sp->s_bytes );
	}

	/* Solid is successfully drawn.  Compute maximum. */
	MAX( maxview, sp->s_center[X] + sp->s_size );
	MAX( maxview, sp->s_center[Y] + sp->s_size );
	MAX( maxview, sp->s_center[Z] + sp->s_size );

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
