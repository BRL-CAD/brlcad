/*
 *			T R E E
 *
 * Ray Tracing program, GED tree tracer.
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
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/db.h"
#include "raytrace.h"
#include "rtdir.h"
#include "debug.h"

union tree *RootTree;		/* Root of tree containing all regions */
struct region *HeadRegion;
struct region **Regions;	/* Ptr to array indexed by reg_bit */

/* A bounding RPP around the whole model */
vect_t mdl_min = {  INFINITY,  INFINITY,  INFINITY };
vect_t mdl_max = { -INFINITY, -INFINITY, -INFINITY };
#define MINMAX(a,b,c)	{ if( (c) < (a) )  a = (c);\
			if( (c) > (b) )  b = (c); }

HIDDEN union tree *drawHobj();
HIDDEN void add_regtree();

extern int nul_prep(),	nul_print(), nul_norm(), nul_uv();
extern int tor_prep(),	tor_print(), tor_norm(), tor_uv();
extern int tgc_prep(),	tgc_print(), tgc_norm(), tgc_uv();
extern int ell_prep(),	ell_print(), ell_norm(), ell_uv();
extern int arb_prep(),	arb_print(), arb_norm(), arb_uv();
extern int haf_prep(),	haf_print(), haf_norm(), haf_uv();
extern int ars_prep(),  ars_print(), ars_norm(), ars_uv();
extern int rec_prep(),	rec_print(), rec_norm(), rec_uv();

extern struct seg *nul_shot();
extern struct seg *tor_shot();
extern struct seg *tgc_shot();
extern struct seg *ell_shot();
extern struct seg *arb_shot();
extern struct seg *ars_shot();
extern struct seg *haf_shot();
extern struct seg *rec_shot();

struct functab functab[] = {
	nul_prep, nul_shot, nul_print, nul_norm, nul_uv, "ID_NULL",
	tor_prep, tor_shot, tor_print, tor_norm, tor_uv, "ID_TOR",
	tgc_prep, tgc_shot, tgc_print, tgc_norm, tgc_uv, "ID_TGC",
	ell_prep, ell_shot, ell_print, ell_norm, ell_uv, "ID_ELL",
	arb_prep, arb_shot, arb_print, arb_norm, arb_uv, "ID_ARB8",
	ars_prep, ars_shot, ars_print, ars_norm, ars_uv, "ID_ARS",
	haf_prep, haf_shot, haf_print, haf_norm, haf_uv, "ID_HALF",
	rec_prep, rec_shot, rec_print, rec_norm, rec_uv, "ID_REC",
	nul_prep, nul_shot, nul_print, nul_norm, nul_uv, ">ID_NULL"
};

/*
 *  Hooks for unimplemented routines
 */
#define DEF(func)	func() { rtlog("func unimplemented\n"); }

DEF(nul_prep); struct seg * DEF(nul_shot); DEF(nul_print); DEF(nul_norm); DEF(nul_uv);

/* To be replaced with code someday */
DEF(haf_prep); struct seg * DEF(haf_shot); DEF(haf_print); DEF(haf_norm); DEF(haf_uv);

static char idmap[] = {
	ID_NULL,	/* undefined, 0 */
	ID_NULL,	/*  RPP	1 axis-aligned rectangular parallelopiped */
	ID_NULL,	/* BOX	2 arbitrary rectangular parallelopiped */
	ID_NULL,	/* RAW	3 right-angle wedge */
	ID_NULL,	/* ARB4	4 tetrahedron */
	ID_NULL,	/* ARB5	5 pyramid */
	ID_NULL,	/* ARB6	6 extruded triangle */
	ID_NULL,	/* ARB7	7 weird 7-vertex shape */
	ID_NULL,	/* ARB8	8 hexahedron */
	ID_NULL,	/* ELL	9 ellipsoid */
	ID_NULL,	/* ELL1	10 another ellipsoid ? */
	ID_NULL,	/* SPH	11 sphere */
	ID_NULL,	/* RCC	12 right circular cylinder */
	ID_NULL,	/* REC	13 right elliptic sylinder */
	ID_NULL,	/* TRC	14 truncated regular cone */
	ID_NULL,	/* TEC	15 truncated elliptic cone */
	ID_TOR,		/* TOR	16 toroid */
	ID_NULL,	/* TGC	17 truncated general cone */
	ID_TGC,		/* GENTGC 18 supergeneralized TGC; internal form */
	ID_ELL,		/* GENELL 19: V,A,B,C */
	ID_ARB8,	/* GENARB8 20:  V, and 7 other vectors */
	ID_ARS,		/* ARS 21: arbitrary triangular-surfaced polyhedron */
	ID_NULL,	/* ARSCONT 22: extension record type for ARS solid */
	ID_NULL		/* n+1 */
};

HIDDEN char *path_str();

extern void color_map();
extern int rt_needprep;


/*
 *  			G E T _ T R E E
 *
 *  User-called function to add a tree hierarchy to the displayed set.
 *  
 *  Returns -
 *  	0	Ordinarily
 *	-1	On major error
 */
int
get_tree(node)
char *node;
{
	register union tree *curtree;
	register struct directory *dp;
	mat_t	mat;

	if(!rt_needprep)
		rtbomb("get_tree called again after rt_prep!");

	mat_idn( mat );

	dp = dir_lookup( node, LOOKUP_NOISY );
	if( dp == DIR_NULL )
		return(-1);		/* ERROR */

	curtree = drawHobj( dp, REGION_NULL, 0, mat );
	if( curtree != TREE_NULL )  {
		/*  Subtree has not been contained by a region.
		 *  This should only happen when a top-level solid
		 *  is encountered.  Build a special region for it.
		 */
		register struct region *regionp;	/* XXX */

		GETSTRUCT( regionp, region );
		rtlog("Warning:  Top level solid, region %s created\n",
			path_str(0) );
		if( curtree->tr_op != OP_SOLID )
			rtbomb("root subtree not Solid");
		regionp->reg_name = strdup(path_str(0));
		add_regtree( regionp, curtree );
	}
	return(0);	/* OK */
}

static vect_t xaxis = { 1.0, 0, 0, 0 };
static vect_t yaxis = { 0, 1.0, 0, 0 };
static vect_t zaxis = { 0, 0, 1.0, 0 };

HIDDEN
struct soltab *
add_solid( rec, name, mat )
union record *rec;
char	*name;
matp_t	mat;
{
	register struct soltab *stp;
	static vect_t v[8];
	static vect_t A, B, C;
	static fastf_t fx, fy, fz;
	FAST fastf_t f;
	register struct soltab *nsp;

	/* Validate that matrix preserves perpendicularity of axis */
	/* by checking that A.B == 0, B.C == 0, A.C == 0 */
	MAT4X3VEC( A, mat, xaxis );
	MAT4X3VEC( B, mat, yaxis );
	MAT4X3VEC( C, mat, zaxis );
	fx = VDOT( A, B );
	fy = VDOT( B, C );
	fz = VDOT( A, C );
	if( ! NEAR_ZERO(fx) || ! NEAR_ZERO(fy) || ! NEAR_ZERO(fz) )  {
		rtlog("add_solid(%s):  matrix does not preserve axis perpendicularity.\n  X.Y=%f, Y.Z=%f, X.Z=%f\n",
			name, fx, fy, fz );
		mat_print("bad matrix", mat);
		return( SOLTAB_NULL );		/* BAD */
	}

	/*
	 *  Check to see if this exact solid has already been processed.
	 *  Match on leaf name and matrix.
	 */
	for( nsp = HeadSolid; nsp != SOLTAB_NULL; nsp = nsp->st_forw )  {
		register int i;

		if(
			name[0] != nsp->st_name[0]  ||	/* speed */
			name[1] != nsp->st_name[1]  ||	/* speed */
			strcmp( name, nsp->st_name ) != 0
		)
			continue;
		for( i=0; i<16; i++ )  {
			f = mat[i] - nsp->st_pathmat[i];
			if( !NEAR_ZERO(f) )
				goto next_one;
		}
		/* Success, we have a match! */
		if( debug & DEBUG_SOLIDS )
			rtlog("add_solid:  %s re-referenced\n",
				name );
		return(nsp);
next_one: ;
	}

	/* Convert from database (float) to fastf_t */
	fastf_float( v, rec->s.s_values, 8 );

	GETSTRUCT(stp, soltab);
	stp->st_id = idmap[rec->s.s_type];	/* PUN for a.a_type, too */
	stp->st_name = name;
	stp->st_specific = (int *)0;

	/* init solid's maxima and minima */
	stp->st_max[X] = stp->st_max[Y] = stp->st_max[Z] = -INFINITY;
	stp->st_min[X] = stp->st_min[Y] = stp->st_min[Z] =  INFINITY;

	if( functab[stp->st_id].ft_prep( v, stp, mat, &(rec->s) ) )  {
		/* Error, solid no good */
		free(stp);
		return( SOLTAB_NULL );		/* BAD */
	}

	/* For now, just link them all onto the same list */
	stp->st_forw = HeadSolid;
	HeadSolid = stp;

	mat_copy( stp->st_pathmat, mat );

	/* Update the model maxima and minima */
#define MMM(v)		MINMAX( mdl_min[X], mdl_max[X], v[X] ); \
			MINMAX( mdl_min[Y], mdl_max[Y], v[Y] ); \
			MINMAX( mdl_min[Z], mdl_max[Z], v[Z] )
	MMM( stp->st_min );
	MMM( stp->st_max );

	stp->st_bit = nsolids++;
	if(debug&DEBUG_SOLIDS)  {
		rtlog("-------------- %s (bit %d) -------------\n",
			stp->st_name, stp->st_bit );
		VPRINT("Bound Sph CENTER", stp->st_center);
		rtlog("Approx Sph Radius = %f\n", stp->st_aradius);
		rtlog("Bounding Sph Radius = %f\n", stp->st_bradius);
		VPRINT("Bound RPP min", stp->st_min);
		VPRINT("Bound RPP max", stp->st_max);
		functab[stp->st_id].ft_print( stp );
	}
	return( stp );
}

#define	MAXLEVELS	8
static struct directory	*path[MAXLEVELS];	/* Record of current path */

static mat_t xmat;				/* temporary fastf_t matrix */

/*
 *			D R A W H O B J
 *
 * This routine is used to get an object drawn.
 * The actual processing of solids is performed by add_solid(),
 * but all transformations and region building is done here.
 */
HIDDEN
union tree *
drawHobj( dp, argregion, pathpos, old_xlate )
struct directory *dp;
struct region *argregion;
matp_t old_xlate;
{
	auto struct directory *nextdp;	/* temporary */
	auto mat_t new_xlate;		/* Accumulated xlation matrix */
	auto union record rec;		/* local copy of this record */
	auto long savepos;
	auto int nparts;		/* Number of sub-parts to this comb */
	auto int i;
	register union tree *curtree;	/* ptr to current tree top */
	auto union tree *subtree;	/* ptr to subtree passed up */
	register struct region *regionp;

	if( pathpos >= MAXLEVELS )  {
		rtlog("%s: nesting exceeds %d levels\n",
			path_str(MAXLEVELS), MAXLEVELS );
		return(TREE_NULL);
	}
	path[pathpos] = dp;

	/* Routine is recursive:  save file pointer for restoration. */
	savepos = lseek( ged_fd, 0L, 1 );

	/*
	 * Load the record into local record buffer
	 */
	(void)lseek( ged_fd, dp->d_addr, 0 );
	(void)read( ged_fd, (char *)&rec, sizeof rec );

	if( rec.u_id == ID_SOLID || rec.u_id == ID_ARS_A )  {
		register struct soltab *stp;		/* XXX */

		/* Draw a solid */
		stp = add_solid( &rec, dp->d_namep, old_xlate );
		(void)lseek( ged_fd, savepos, 0);	/* restore pos */
		if( stp == SOLTAB_NULL )
			return( TREE_NULL );

		/**GETSTRUCT( curtree, union tree ); **/
		curtree = (union tree *)malloc(sizeof(union tree));
		if( curtree == TREE_NULL )
			rtbomb("drawHobj: curtree malloc failed\n");
		bzero( (char *)curtree, sizeof(union tree) );
		curtree->tr_op = OP_SOLID;
		curtree->tr_a.tu_stp = stp;
		curtree->tr_a.tu_name = strdup(path_str(pathpos));
		curtree->tr_regionp = argregion;
		return( curtree );
	}

	if( rec.u_id != ID_COMB )  {
		rtlog("drawobj:  defective input '%c'\n", rec.u_id );
		return(TREE_NULL);			/* ERROR */
	}

	/* recurse down through a combination (directory) node */
	regionp = argregion;
	if( rec.c.c_flags == 'R' )  {
		if( argregion != REGION_NULL )  {
			rtlog("Warning:  region %s within region %s (ID %d overrides)\n",
				path_str(pathpos), argregion->reg_name,
				argregion->reg_regionid );
/***			argregion = REGION_NULL;	/* override! */
		} else {
			/* HACK:  ignore "air" solids */
			if( rec.c.c_aircode != 0 )  {
				(void)lseek(ged_fd, savepos, 0);
				return(TREE_NULL);
			}

			/* Start a new region here */
			GETSTRUCT( regionp, region );
			regionp->reg_forw = REGION_NULL;
			regionp->reg_regionid = rec.c.c_regionid;
			regionp->reg_aircode = rec.c.c_aircode;
			regionp->reg_material = rec.c.c_material;
			regionp->reg_los = rec.c.c_los;
			regionp->reg_name = "being created";
		}
	}
	curtree = TREE_NULL;
	nparts = rec.c.c_length;		/* save in an auto var */
	for( i=0; i<nparts; i++ )  {
		(void)read(ged_fd, (char *)&rec, sizeof rec );
		nextdp = dir_lookup( rec.M.m_instname, LOOKUP_NOISY );
		if( nextdp == DIR_NULL )
			continue;
		{
			register int j;
			for( j=0; j<4*4; j++ )
				xmat[j] = rec.M.m_mat[j];/* cvt to fastf_t */
		}
		mat_mul(new_xlate, old_xlate, xmat);

		/* Recursive call */
		subtree = drawHobj( nextdp, regionp, pathpos+1, new_xlate );
		if( subtree == TREE_NULL )
			continue;	/* no valid subtree, keep on going */
		if( regionp == REGION_NULL )  {
			register struct region *xrp;
			/*
			 * Found subtree that is not contained in a region;
			 * invent a region to hold JUST THIS SOLID,
			 * and add it to the region chain.  Don't force
			 * this whole combination into this region, because
			 * other subtrees might themselves contain regions.
			 * This matches GIFT's current behavior.
			 */
			rtlog("Warning:  Forced to create region %s\n",
				path_str(pathpos+1) );
			if(subtree->tr_op != OP_SOLID )
				rtbomb("subtree not Solid");
			GETSTRUCT( xrp, region );
			xrp->reg_name = strdup(path_str(pathpos+1));
			add_regtree( xrp, subtree );
			continue;	/* no remaining subtree, go on */
		}

		if( curtree == TREE_NULL )  {
			curtree = subtree;
		} else {
			register union tree *xtp;	/* XXX */
			/** GETSTRUCT( xtp, union tree ); **/
			xtp=(union tree *)malloc(sizeof(union tree));
			if( xtp == TREE_NULL )
				rtbomb("drawHobj: xtp malloc failed\n");
			bzero( (char *)xtp, sizeof(union tree) );
			xtp->tr_b.tb_left = curtree;
			xtp->tr_b.tb_right = subtree;
			xtp->tr_b.tb_regionp = regionp;
			switch( rec.M.m_relation )  {
			default:
				rtlog("(%s) bad m_relation '%c'\n",
					path_str(pathpos), rec.M.m_relation );
				/* FALL THROUGH */
			case UNION:
				xtp->tr_op = OP_UNION; break;
			case SUBTRACT:
				xtp->tr_op = OP_SUBTRACT; break;
			case INTERSECT:
				xtp->tr_op = OP_INTERSECT; break;
			}
			curtree = xtp;
		}
	}
	if( curtree != TREE_NULL )  {
		if( regionp == REGION_NULL )  {
			rtlog("drawHobj: (%s) null regionp, non-null curtree\n", path_str(pathpos) );
		} else if( argregion == REGION_NULL )  {
			/* Region began at this level */
			regionp->reg_name = strdup(path_str(pathpos));
			add_regtree( regionp, curtree );

			/* Return to caller indicates no remaining subtree */
			curtree = TREE_NULL;
		}
	} else {
		/* Null result tree, release region struct */
		if( argregion == REGION_NULL && regionp != REGION_NULL )
			free( regionp );
	}

	/* Clean up and return */
	(void)lseek( ged_fd, savepos, 0 );
	return( curtree );
}

HIDDEN char *
path_str( pos )
int pos;
{
	static char line[256];
	register char *cp = &line[0];
	register int i;

	if( pos >= MAXLEVELS )  pos = MAXLEVELS-1;
	line[0] = '/';
	line[1] = '\0';
	for( i=0; i<=pos; i++ )  {
		(void)sprintf( cp, "/%s%c", path[i]->d_namep, '\0' );
		cp += strlen(cp);
	}
	return( &line[0] );
}

void
pr_region( rp )
register struct region *rp;
{
	rtlog("REGION %s (bit %d)\n", rp->reg_name, rp->reg_bit );
	rtlog("id=%d, air=%d, material=%d, los=%d\n",
		rp->reg_regionid, rp->reg_aircode,
		rp->reg_material, rp->reg_los );
	pr_tree( rp->reg_treetop, 0 );
	rtlog("\n");
}

void
pr_tree( tp, lvl )
register union tree *tp;
int lvl;			/* recursion level */
{
	register int i;

	rtlog("%.8x ", tp);
	for( i=lvl; i>0; i-- )
		rtlog("  ");

	if( tp == TREE_NULL )  {
		rtlog("Null???\n");
		return;
	}

	switch( tp->tr_op )  {

	case OP_SOLID:
		rtlog("SOLID %s (bit %d)",
			tp->tr_a.tu_stp->st_name,
			tp->tr_a.tu_stp->st_bit );
		break;

	default:
		rtlog("Unknown op=x%x\n", tp->tr_op );
		return;

	case OP_UNION:
		rtlog("UNION");
		break;
	case OP_INTERSECT:
		rtlog("INTERSECT");
		break;
	case OP_SUBTRACT:
		rtlog("MINUS");
		break;
	case OP_XOR:
		rtlog("XOR");
		break;
	case OP_NOT:
		rtlog("NOT");
		break;
	}
	rtlog("\n");

	switch( tp->tr_op )  {
	case OP_SOLID:
		break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* BINARY type */
		pr_tree( tp->tr_b.tb_left, lvl+1 );
		pr_tree( tp->tr_b.tb_right, lvl+1 );
		break;
	case OP_NOT:
	case OP_GUARD:
		/* UNARY tree */
		pr_tree( tp->tr_b.tb_left, lvl+1 );
		break;
	}
}

/* Convert TO 4xfastf_t FROM 3xfloats (for database)  */
void
fastf_float( ff, fp, n )
register fastf_t *ff;
register float *fp;
register int n;
{
	while( n-- )  {
		*ff++ = *fp++;
		*ff++ = *fp++;
		*ff++ = *fp++;
		ff += ELEMENTS_PER_VECT-3;
	}
}

/*
 *  			S O L I D _ P O S
 *  
 *  Let the user enquire about the position of the Vertex (V vector)
 *  of a solid.  Useful mostly to find the light sources.
 *
 *  Returns:
 *	0	If solid found and ``pos'' vector filled in.
 *	-1	If error.  ``pos'' vector left untouched.
 */
int
solid_pos( name, pos )
char *name;
vect_t *pos;
{
	register struct directory *dp;
	auto union record rec;		/* local copy of this record */

	if( (dp = dir_lookup( name, LOOKUP_QUIET )) == DIR_NULL )
		return(-1);	/* BAD */
	
	(void)lseek( ged_fd, dp->d_addr, 0 );
	(void)read( ged_fd, (char *)&rec, sizeof rec );

	if( rec.u_id != ID_SOLID )
		return(-1);	/* BAD:  too hard */
	fastf_float( pos, rec.s.s_values, 1 );
	return(0);		/* OK */
}

/*
 *  			F I N D _ S O L I D
 *  
 *  Given the (leaf) name of a solid, find the first occurance of it
 *  in the solid list.  Used mostly to find the light source.
 */
struct soltab *
find_solid( name )
register char *name;
{
	register struct soltab *stp;
	register char *cp;
	for( stp = HeadSolid; stp != SOLTAB_NULL; stp = stp->st_forw )  {
		if( *(cp = stp->st_name) == *name  &&
		    strcmp( cp, name ) == 0 )  {
			return(stp);
		}
	}
	return(SOLTAB_NULL);
}


/*
 *  			A D D _ R E G T R E E
 *  
 *  Add a region and it's boolean tree to all the appropriate places.
 *  The region and treetop are cross-linked, and the region is added
 *  to the linked list of regions.
 *  Positions in the region bit vector are established at this time.
 *  Also, this tree is also included in the overall RootTree.
 */
HIDDEN void
add_regtree( regp, tp )
register struct region *regp;
register union tree *tp;
{
	register union tree *xor;

	/* Cross-reference */
	regp->reg_treetop = tp;
	tp->tr_regionp = regp;
	/* Add to linked list */
	regp->reg_forw = HeadRegion;
	HeadRegion = regp;
	/* Determine material properties */
	color_map(regp);
	/* Add to bit vectors */
	regp->reg_bit = nregions;
	/* Will be added to Regions[] in final prep stage */
	nregions++;
	if( debug & DEBUG_REGIONS )
		rtlog("Add Region %s\n", regp->reg_name);

	if( RootTree == TREE_NULL )  {
		RootTree = tp;
		return;
	}
	/**GETSTRUCT( xor, union tree ); **/
	xor = (union tree *)vmalloc(sizeof(union tree), "add_regtree tree");
	bzero( (char *)xor, sizeof(union tree) );
	xor->tr_b.tb_op = OP_XOR;
	xor->tr_b.tb_left = RootTree;
	xor->tr_b.tb_right = tp;
	xor->tr_b.tb_regionp = REGION_NULL;
	RootTree = xor;
}

optim_tree( tp )
register union tree *tp;
{
	register union tree *low;

	switch( tp->tr_op )  {
	case OP_SOLID:
		return;
	case OP_SUBTRACT:
		while( (low=tp->tr_b.tb_left)->tr_op == OP_SUBTRACT )  {
			/* Rewrite X - A - B as X - ( A union B ) */
			tp->tr_b.tb_left = low->tr_b.tb_left;
			low->tr_op = OP_UNION;
			low->tr_b.tb_left = low->tr_b.tb_right;
			low->tr_b.tb_right = tp->tr_b.tb_right;
			tp->tr_b.tb_right = low;
		}
		goto binary;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_XOR:
		/* Need to look at 3-level optimizations here, both sides */
		goto binary;
	default:
		rtlog("optim_tree: bad op x%x\n", tp->tr_op);
		return;
	}
binary:
	optim_tree( tp->tr_b.tb_left );
	optim_tree( tp->tr_b.tb_right );
}

/*
 *  			S O L I D _ B I T F I N D E R
 *  
 *  Used to walk the boolean tree, setting bits for all the solids in the tree
 *  to the provided bit vector.  Should be called AFTER the region bits
 *  have been assigned.
 */
HIDDEN void
solid_bitfinder( treep, regbit )
register union tree *treep;
register int regbit;
{
	register struct soltab *stp;

	switch( treep->tr_op )  {
	case OP_SOLID:
		stp = treep->tr_a.tu_stp;
		BITSET( stp->st_regions, regbit );
		if( regbit+1 > stp->st_maxreg )  stp->st_maxreg = regbit+1;
		if( debug&DEBUG_REGIONS )  {
			pr_bitv( stp->st_name, stp->st_regions,
				stp->st_maxreg );
		}
		return;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
		/* BINARY type */
		solid_bitfinder( treep->tr_b.tb_left, regbit );
		solid_bitfinder( treep->tr_b.tb_right, regbit );
		return;
	default:
		rtlog("solid_bitfinder:  op=x%x\n", treep->tr_op);
		return;
	}
	/* NOTREACHED */
}

/*
 *  			R T _ P R E P
 *  
 *  This routine should be called just before the first call to shootray().
 *  Right now, it should only be called ONCE per execution.
 */
void
rt_prep()
{
	register struct region *regp;
	register struct soltab *stp;

	if(!rt_needprep)
		rtbomb("second invocation of rt_prep");
	rt_needprep = 0;

	/*
	 *  Allocate space for a per-solid bit of nregions length.
	 */
	for( stp=HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw )  {
		stp->st_regions = (bitv_t *)vmalloc(
			(nregions/sizeof(bitv_t))+sizeof(bitv_t),
			"st_regions bitv" );
		BITZERO( stp->st_regions, nregions );
		stp->st_maxreg = 0;
	}

	/*  Build array of region pointers indexed by reg_bit.
	 *  Optimize each region's expression tree.
	 *  Set this region's bit in the bit vector of every solid
	 *  contained in the subtree.
	 */
	Regions = (struct region **)vmalloc(
		nregions * sizeof(struct region *),
		"Regions[]" );
	for( regp=HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		Regions[regp->reg_bit] = regp;
		optim_tree( regp->reg_treetop );
		solid_bitfinder( regp->reg_treetop, regp->reg_bit );
		if(debug&DEBUG_REGIONS)  {
			pr_region( regp );
		}
	}
	if(debug&DEBUG_REGIONS)  {
		for( stp=HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw )  {
			rtlog("solid %s ", stp->st_name);
			pr_bitv( "regions ref", stp->st_regions,
				stp->st_maxreg);
		}
	}

	/* Partition space */
	cut_it();
}

/*
 *			V I E W B O U N D S
 *
 *  Given a model2view transformation matrix, compute the RPP which
 *  encloses all the solids in the model, in view space.
 */
void
viewbounds( min, max, m2v )
matp_t m2v;
register vect_t min, max;
{
	register struct soltab *stp;
	static vect_t xlated;

	max[X] = max[Y] = max[Z] = -INFINITY;
	min[X] = min[Y] = min[Z] =  INFINITY;

	for( stp=HeadSolid; stp != 0; stp=stp->st_forw ) {
		MAT4X3PNT( xlated, m2v, stp->st_center );
#define VBMIN(v,t) {FAST fastf_t rt; rt=(t); if(rt<v) v = rt;}
#define VBMAX(v,t) {FAST fastf_t rt; rt=(t); if(rt>v) v = rt;}
		VBMIN( min[X], xlated[0]-stp->st_bradius );		
		VBMAX( max[X], xlated[0]+stp->st_bradius );
		VBMIN( min[Y], xlated[1]-stp->st_bradius );
		VBMAX( max[Y], xlated[1]+stp->st_bradius );
		VBMIN( min[Z], xlated[2]-stp->st_bradius );
		VBMAX( max[Z], xlated[2]+stp->st_bradius );
	}

	/* Provide a slight border */
	min[X] -= min[X] * 0.03;
	min[Y] -= min[Y] * 0.03;
	min[Z] -= min[Z] * 0.03;
	max[X] *= 1.03;
	max[Y] *= 1.03;
	max[Z] *= 1.03;
}
