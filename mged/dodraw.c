/*
 *			D O D R A W . C
 *
 * Functions -
 *	drawtree	Call drawHobj to draw a tree
 *	drawHobj	Call drawsolid for all solids in an object
 *	drawHsolid	Manage the drawing of a COMGEOM solid
 *	pathHmat	Find matrix across a given path
 *	redraw		redraw a single solid, given matrix and record.
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
#include "nmg.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

struct vlist	*rtg_vlFree;	/* should be rt_g.rtg_vlFree !! XXX dm.h */

int	reg_error;	/* error encountered in region processing */
int	no_memory;	/* flag indicating memory for drawing is used up */
long	nvectors;	/* number of vectors drawn so far */

int	regmemb;	/* # of members left to process in a region */
char	memb_oper;	/* operation for present member of processed region */
int	reg_pathpos;	/* pathpos of a processed region */

struct directory	*cur_path[MAX_PATH];	/* Record of current path */

static struct mater_info mged_no_mater = {
	/* RT default is white.  This is red, to stay clear of illuminate mode */
	1.0, 0, 0,		/* */
	0,			/* override */
	DB_INH_LOWER,		/* color inherit */
	DB_INH_LOWER		/* mater inherit */
};

/*
 *			D R A W T R E E
 *
 *  This routine is the analog of rt_gettree().
 */
void
drawtree( dp )
struct directory	*dp;
{
	mat_t		root;
	struct mater_info	root_mater;

	root_mater = mged_no_mater;	/* struct copy */

	mat_idn( root );
	/* Could apply root animations here ? */

	drawHobj( dp, ROOT, 0, root, 0, &root_mater );
}

/*
 *			D R A W H O B J
 *
 * This routine is used to get an object drawn.
 * The actual drawing of solids is performed by drawsolid(),
 * but all transformations down the path are done here.
 */
void
drawHobj( dp, flag, pathpos, old_xlate, regionid, materp )
register struct directory *dp;
int		flag;
int		pathpos;
matp_t		old_xlate;
int		regionid;
struct mater_info *materp;
{
	union record	*rp;
	auto mat_t	new_xlate;	/* Accumulated translation matrix */
	auto int	i;
	struct mater_info curmater;

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

	if( rp[0].u_id != ID_COMB )  {
		register struct solid *sp;

		if( rt_id_solid( rp ) == ID_NULL )  {
			(void)printf("drawobj(%s):  defective database record, type='%c' (0%o) addr=x%x\n",
				dp->d_namep,
				rp[0].u_id, rp[0].u_id, dp->d_addr );
			goto out;		/* ERROR */
		}
		/*
		 * Enter new solid (or processed region) into displaylist.
		 */
		cur_path[pathpos] = dp;

		GET_SOLID( sp );
		if( sp == SOLID_NULL )
			return;		/* ERROR */
		if( drawHsolid( sp, flag, pathpos, old_xlate, rp, regionid, materp ) != 1 ) {
			FREE_SOLID( sp );
		}
		goto out;
	}

	/*
	 *  At this point, u_id == ID_COMB.
	 *  Process a Combination (directory) node
	 */
	if( dp->d_len <= 1 )  {
		(void)printf("Warning: combination with zero members \"%s\".\n",
			dp->d_namep );
		goto out;			/* non-fatal ERROR */
	}

	/*
	 *  Handle inheritance of material property.
	 *  Color and the material property have separate
	 *  inheritance interlocks.
	 */
	curmater = *materp;	/* struct copy */
	if( rp[0].c.c_override == 1 )  {
		if( regionid != 0 )  {
			rt_log("rt_drawobj: ERROR: color override in combination within region %s\n",
				dp->d_namep );
		} else {
			if( curmater.ma_cinherit == DB_INH_LOWER )  {
				curmater.ma_override = 1;
				curmater.ma_color[0] = (rp[0].c.c_rgb[0])*rt_inv255;
				curmater.ma_color[1] = (rp[0].c.c_rgb[1])*rt_inv255;
				curmater.ma_color[2] = (rp[0].c.c_rgb[2])*rt_inv255;
				curmater.ma_cinherit = rp[0].c.c_inherit;
			}
		}
	}
	if( rp[0].c.c_matname[0] != '\0' )  {
		if( regionid != 0 )  {
			rt_log("rt_drawobj: ERROR: material property spec in combination within region %s\n",
				dp->d_namep );
		} else {
			if( curmater.ma_minherit == DB_INH_LOWER )  {
				strncpy( curmater.ma_matname, rp[0].c.c_matname, sizeof(rp[0].c.c_matname) );
				strncpy( curmater.ma_matparm, rp[0].c.c_matparm, sizeof(rp[0].c.c_matparm) );
				curmater.ma_minherit = rp[0].c.c_inherit;
			}
		}
	}

	/* Handle combinations which are the top of a "region" */
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
		/* Check here for animation to apply */
		mat_mul(new_xlate, old_xlate, xmat);

		/* Recursive call */
		drawHobj(
			nextdp,
			(mp->m_relation != SUBTRACT) ? ROOT : INNER,
			pathpos + 1,
			new_xlate,
			regionid,
			&curmater
		);
	}
out:
	rt_free( (char *)rp, "drawHobj recs");
}

/*
 *			D R A W H S O L I D
 *
 * Returns -
 *	-1	on error
 *	 0	if NO OP
 *	 1	if solid was drawn
 */
int
drawHsolid( sp, flag, pathpos, xform, recordp, regionid, materp )
register struct solid *sp;
int		flag;
int		pathpos;
matp_t		xform;
union record	*recordp;
int		regionid;
struct mater_info *materp;
{
	register struct vlist *vp;
	register int i;
	int dashflag;		/* draw with dashed lines */
	int count;
	struct vlhead	vhead;
	vect_t		maxvalue, minvalue;

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
		extern int use_nmg_flag;	/* XXX from chgview.c */

		dashflag = (flag != ROOT);

		id = rt_id_solid( recordp );
		if( id < 0 || id >= rt_nfunctab )  {
			printf("drawHsolid(%s):  unknown database object\n",
				cur_path[pathpos]->d_namep);
			return(-1);			/* ERROR */
		}

		if( use_nmg_flag )  {
			struct model		*m;
			struct nmgregion	*r;

			/* Tessellate Solid to NMG region */
			m = nmg_mm();		/* Make model */
			if( rt_functab[id].ft_tessellate( &r, m,
			    recordp, xform, cur_path[pathpos] ) < 0 )  {
				printf("%s: tessellate failure\n", cur_path[pathpos]->d_namep );
			} else {
				/* Convert NMG to vlist */
				NMG_CK_REGION(r);
				/* 0 = vectors, 1 = w/polygon markers */
				nmg_s_to_vlist( &vhead, r->s_p, 1 );
			}

			/* Destroy NMG */
			nmg_km( m );
		} else {
			if( rt_functab[id].ft_plot( recordp, xform, &vhead,
			    cur_path[pathpos] ) < 0 )  {
				printf("%s: vector conversion failure\n",
					cur_path[pathpos]->d_namep);
			}
		}
	}

	/* Take note of the base color */
	if( materp )  {
		sp->s_basecolor[0] = materp->ma_color[0] * 255.;
		sp->s_basecolor[1] = materp->ma_color[1] * 255.;
		sp->s_basecolor[2] = materp->ma_color[2] * 255.;
	}

	/*
	 * Compute the min, max, and center points.
	 */
	VSETALL( maxvalue, -INFINITY );
	VSETALL( minvalue,  INFINITY );
	sp->s_vlist = vhead.vh_first;
	sp->s_vlen = 0;
	for( vp = vhead.vh_first; vp != VL_NULL; vp = vp->vl_forw )  {
		VMINMAX( minvalue, maxvalue, vp->vl_pnt );
		sp->s_vlen++;
	}
	nvectors += sp->s_vlen;

	VADD2SCALE( sp->s_center, minvalue, maxvalue, 0.5 );

	sp->s_size = maxvalue[X] - minvalue[X];
	MAX( sp->s_size, maxvalue[Y] - minvalue[Y] );
	MAX( sp->s_size, maxvalue[Z] - minvalue[Z] );

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
		rt_free( (char *)rp, "pathHmat recs");
	}
}

/*
 *  			R E D R A W
 *  
 *  Probably misnamed.
 */
struct solid *
redraw( sp, recp, mat )
struct solid *sp;
union record *recp;
mat_t	mat;
{
	int addr, bytes;

	if( sp == SOLID_NULL )
		return( sp );

	/* Remember displaylist location of previous solid */
	addr = sp->s_addr;
	bytes = sp->s_bytes;

	if( drawHsolid(
		sp,
		sp->s_soldash,
		sp->s_last,
		mat,
		recp,
		sp->s_regionid,
		(struct mater_info *)0
	) != 1 )  {
		(void)printf("redraw():  error in drawHsolid()\n");
		return(sp);
	}

	/* Release previous chunk of displaylist, and rewrite control list */
	memfree( &(dmp->dmr_map), (unsigned)bytes, (unsigned long)addr );
	dmaflag = 1;
	return( sp );
}
