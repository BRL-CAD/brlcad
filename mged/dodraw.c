/*
 *			D O D R A W . C
 *
 * Functions -
 *	drawHsolid	Manage the drawing of a COMGEOM solid
 *	freevgcore	De-allocate display processor memory
 *  
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "ged_types.h"
#include "3d.h"
#include "ged.h"
#include "solid.h"
#include "dir.h"
#include "vmath.h"
#include "dm.h"
#ifdef BSD42
extern void bcopy();
#define	memcpy(to,from,cnt)	bcopy(from,to,cnt)
#else
extern char *memcpy();
#endif

extern void	perror();
extern int	printf(), write();

#define NVL	1000
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
drawHsolid( sp, flag, pathpos, xform, recordp )
register struct solid *sp;		/* solid structure */
int flag;
int pathpos;
matp_t xform;
union record *recordp;
{
	register struct veclist *vp;
	register int i;
	int dashflag;		/* draw with dashed lines */
	int count;
	static float xmax, ymax, zmax;
	static float xmin, ymin, zmin;

	if( recordp->u_id != ID_SOLID )  {
		printf("dodraw: non-solid, id=%c\n", recordp->u_id );
		return(-1);	/* ERROR */
	}
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
				draw_tgc( recordp->s.s_values, xform );
				break;

			case GENELL:
				draw_ell( recordp->s.s_values, xform );
				break;

			case TOR:
				draw_tor( recordp->s.s_values, xform );
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
	if( (sp->s_vlist = (struct veclist *)malloc(count)) == VLIST_NULL )  {
		no_memory = 1;
		printf("draw: malloc error\n");
		return(-1);		/* ERROR */
	}
	(void)memcpy( (char *)sp->s_vlist, (char *)veclist, count );

	/* Cvt to displaylist, determine displaylist memory requirement. */
	sp->s_bytes = dmp->dmr_cvtvecs( &veclist[0],
		sp->s_center, sp->s_size, dashflag );

	/* Allocate displaylist storage for object */
	sp->s_addr = memalloc( sp->s_bytes );
	if( sp->s_addr == 0 )  {
		no_memory = 1;
		(void)printf("draw: out of Displaylist\n");
		return(-1);		/* ERROR */
	}
	sp->s_bytes = dmp->dmr_load( sp->s_addr, sp->s_bytes );

	/* set solid/dashed line flag */
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

		/* Add to linked list of solid structs */
		APPEND_SOLID( sp, HeadSolid.s_back );
	} else {
		/* replacing illuminated solid -- struct already linked in */
		sp->s_iflag = UP;
	}

	/* Compute maximum */
	MAX( maxview, sp->s_center[X] + sp->s_size );
	MAX( maxview, sp->s_center[Y] + sp->s_size );
	MAX( maxview, sp->s_center[Z] + sp->s_size );

	return(1);		/* OK */
}

/*
 *			F R E E V G C O R E
 *
 * This routine is used to recycle displaylist memory.
 */
void
freevgcore( addr, bytes )
unsigned	addr;
unsigned	bytes;
{
	memfree( bytes, addr );

	/* reset memory used up flag */
	no_memory = 0;
	return;		/* OK */
}



#ifdef never
/*
 *			M R E A D
 *
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This KLUDGE is necessary because pipes
 * may not return any more than 512 characters on a single read.
 */
static int
mread(fd, bufp, n)
int fd;
register char	*bufp;
unsigned	n;
{
	register unsigned	count = 0;
	register int		nread;

	do {
		nread = read(fd, bufp, n-count);
		if(nread == -1)
			return(nread);
		if(nread == 0)
			return((int)count);
		count += (unsigned)nread;
		bufp += nread;
	 } while(count < n);

	return((int)count);
}
#endif
