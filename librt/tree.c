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
#include "vmath.h"
#include "raytrace.h"
#include "../h/db.h"
#include "rtdir.h"
#include "debug.h"

struct region *HeadRegion;

/* A bounding RPP around the whole model */
vect_t model_min = {  INFINITY,  INFINITY,  INFINITY };
vect_t model_max = { -INFINITY, -INFINITY, -INFINITY };
#define MINMAX(a,b,c)	{ if( (c) < (a) )  a = (c);\
			if( (c) > (b) )  b = (c); }

HIDDEN union tree *drawHobj();

extern int nul_prep(),	nul_print();
extern int tor_prep(),	tor_print();
extern int tgc_prep(),	tgc_print();
extern int ell_prep(),	ell_print();
extern int arb_prep(),	arb_print();
extern int haf_prep(),	haf_print();
extern int ars_prep(),  ars_print();
extern int rec_print();

extern struct seg *nul_shot();
extern struct seg *tor_shot();
extern struct seg *tgc_shot();
extern struct seg *ell_shot();
extern struct seg *arb_shot();
extern struct seg *ars_shot();
extern struct seg *haf_shot();
extern struct seg *rec_shot();

struct functab functab[] = {
	nul_prep,	nul_shot,	nul_print,	"ID_NULL",
	tor_prep,	tor_shot,	tor_print,	"ID_TOR",
	tgc_prep,	tgc_shot,	tgc_print,	"ID_TGC",
	ell_prep,	ell_shot,	ell_print,	"ID_ELL",
	arb_prep,	arb_shot,	arb_print,	"ID_ARB8",
	ars_prep,	ars_shot,	ars_print,	"ID_ARS",
	haf_prep,	haf_shot,	haf_print,	"ID_HALF",
	nul_prep,	nul_shot,	nul_print,	">ID_NULL"
};

/*
 *  Hooks for unimplemented routines
 */
#define DEF(func)	func() { fprintf(stderr,"func unimplemented\n"); }

DEF(haf_prep); struct seg * DEF(haf_shot); DEF(haf_print);
DEF(nul_prep); struct seg * DEF(nul_shot); DEF(nul_print);

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
		register struct soltab *stp;

		GETSTRUCT( regionp, region );
		regionp->reg_active = REGION_NULL;
		fprintf(stderr,"Warning:  Top level solid, region %s created\n",
			path_str(0) );
		if( curtree->tr_op != OP_SOLID )
			rtbomb("root subtree not Solid");
		stp = curtree->tr_stp;
		stp->st_regionp = regionp;
		regionp->reg_treetop = curtree;
		regionp->reg_name = strdup(path_str(0));
		regionp->reg_forw = HeadRegion;
		HeadRegion = regionp;
		nregions++;
	}
	color_soltab();			/* associate soltab w/material */

	if( debug & DEBUG_REGIONS )  {
		register struct region *rp = HeadRegion;	/* XXX */

		fprintf(stderr,"printing regions\n");
		while( rp != REGION_NULL )  {
			pr_region( rp );
			rp = rp->reg_forw;
		}
	}
	return(0);	/* OK */
}

static vect_t xaxis = { 1.0, 0, 0, 0 };
static vect_t yaxis = { 0, 1.0, 0, 0 };
static vect_t zaxis = { 0, 0, 1.0, 0 };

HIDDEN
struct soltab *
add_solid( rec, name, mat, regp )
union record *rec;
char	*name;
matp_t	mat;
struct region *regp;
{
	register struct soltab *stp;
	static vect_t v[8];
	static vect_t A, B, C;
	static fastf_t fx, fy, fz;
	FAST fastf_t f;

	/* Validate that matrix preserves perpendicularity of axis */
	/* by checking that A.B == 0, B.C == 0, A.C == 0 */
	MAT4X3VEC( A, mat, xaxis );
	MAT4X3VEC( B, mat, yaxis );
	MAT4X3VEC( C, mat, zaxis );
	fx = VDOT( A, B );
	fy = VDOT( B, C );
	fz = VDOT( A, C );
	if( ! NEAR_ZERO(fx) || ! NEAR_ZERO(fy) || ! NEAR_ZERO(fz) )  {
		fprintf(stderr,"add_solid(%s):  matrix does not preserve axis perpendicularity.\n  X.Y=%f, Y.Z=%f, X.Z=%f\n",
			name, fx, fy, fz );
		mat_print("bad matrix", mat);
		return( SOLTAB_NULL );		/* BAD */
	}

	/* Convert from database (float) to fastf_t */
	fastf_float( v, rec->s.s_values, 8 );

	GETSTRUCT(stp, soltab);
	stp->st_id = idmap[rec->s.s_type];	/* PUN for a.a_type, too */
	stp->st_name = name;
	stp->st_specific = (int *)0;
	stp->st_regionp = regp;

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

	/* Update the model maxima and minima */
#define MMM(v)		MINMAX( model_min[X], model_max[X], v[X] ); \
			MINMAX( model_min[Y], model_max[Y], v[Y] ); \
			MINMAX( model_min[Z], model_max[Z], v[Z] )
	MMM( stp->st_min );
	MMM( stp->st_max );

	if(debug&DEBUG_SOLIDS)  {
		fprintf(stderr,"-------------- %s -------------\n", stp->st_name);
		VPRINT("Bound Sph CENTER", stp->st_center);
		fprintf(stderr,"Bound Sph Rad**2 = %f\n", stp->st_radsq);
		VPRINT("Bound RPP min", stp->st_min);
		VPRINT("Bound RPP max", stp->st_max);
		if( regp != REGION_NULL )
			fprintf(stderr,"Member of region %s\n", regp->reg_name );
		functab[stp->st_id].ft_print( stp );
	}
	nsolids++;
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
		fprintf(stderr,"%s: nesting exceeds %d levels\n",
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
		stp = add_solid( &rec, strdup(path_str(pathpos)), old_xlate, argregion );
		(void)lseek( ged_fd, savepos, 0);	/* restore pos */
		if( stp == SOLTAB_NULL )
			return( TREE_NULL );

		/**GETSTRUCT( curtree, union tree ); **/
		curtree = (union tree *)malloc(sizeof(union tree));
		if( curtree == TREE_NULL )
			rtbomb("drawHobj: curtree malloc failed\n");
		bzero( (char *)curtree, sizeof(union tree) );
		curtree->tr_op = OP_SOLID;
		curtree->tr_stp = stp;
		VMOVE( curtree->tr_min, stp->st_min );
		VMOVE( curtree->tr_max, stp->st_max );
		return( curtree );
	}

	if( rec.u_id != ID_COMB )  {
		fprintf(stderr,"drawobj:  defective input '%c'\n", rec.u_id );
		return(TREE_NULL);			/* ERROR */
	}

	/* recurse down through a combination (directory) node */
	regionp = argregion;
	if( rec.c.c_flags == 'R' )  {
		if( argregion != REGION_NULL )  {
			fprintf(stderr,"Warning:  region %s within region %s (ID %d overrides)\n",
				path_str(pathpos), argregion->reg_name,
				argregion->reg_regionid );
/***			argregion = REGION_NULL;	/* override! */
		} else {
			/* Start a new region here */
			GETSTRUCT( regionp, region );
			regionp->reg_forw = regionp->reg_active = REGION_NULL;
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
		if( rec.M.m_brname[0] != '\0' )  {
			register struct directory *tdp;	/* XXX */
			/*
			 * Create an alias.  First step towards full
			 * branch naming.  User is responsible for his
			 * branch names being unique.
			 */
			tdp = dir_lookup(rec.M.m_brname, LOOKUP_QUIET);
			if( tdp != DIR_NULL )
				nextdp = tdp; /* use existing alias */
			else
				nextdp=dir_add(rec.M.m_brname,nextdp->d_addr);
		}
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
			register struct soltab *stp;
			/*
			 * Found subtree that is not contained in a region;
			 * invent a region to hold JUST THIS SOLID,
			 * and add it to the region chain.  Don't force
			 * this whole combination into this region, because
			 * other subtrees might themselves contain regions.
			 * This matches GIFT's current behavior.
			 */
			fprintf(stderr,"Warning:  Forced to create region %s\n",
				path_str(pathpos+1) );
			if(subtree->tr_op != OP_SOLID )
				rtbomb("subtree not Solid");
			stp = subtree->tr_stp;
			GETSTRUCT( xrp, region );
			xrp->reg_treetop = subtree;
			stp->st_regionp = xrp;
			xrp->reg_name = stp->st_name;
			xrp->reg_forw = HeadRegion;
			HeadRegion = xrp;
			xrp->reg_active = REGION_NULL;
			nregions++;
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
			xtp->tr_left = curtree;
			xtp->tr_right = subtree;
			switch( rec.M.m_relation )  {
			default:
				fprintf(stderr,"(%s) bad m_relation '%c'\n",
					path_str(pathpos), rec.M.m_relation );
				/* FALL THROUGH */
			case UNION:
				xtp->tr_op = OP_UNION; break;
			case SUBTRACT:
				xtp->tr_op = OP_SUBTRACT; break;
			case INTERSECT:
				xtp->tr_op = OP_INTERSECT; break;
			}

			/* Compute bounding RPP of full subtree */
#define MM(v)		MINMAX( xtp->tr_min[X], xtp->tr_max[X], v[X] ); \
			MINMAX( xtp->tr_min[Y], xtp->tr_max[Y], v[Y] ); \
			MINMAX( xtp->tr_min[Z], xtp->tr_max[Z], v[Z] )

			VMOVE( xtp->tr_min, curtree->tr_min );
			VMOVE( xtp->tr_max, curtree->tr_max );
			MM( subtree->tr_min );
			MM( subtree->tr_max );
			curtree = xtp;
		}
	}
	if( curtree != TREE_NULL )  {
		if( regionp == REGION_NULL )  {
			fprintf(stderr,"drawHobj: (%s) null regionp, non-null curtree\n", path_str(pathpos) );
		} else if( argregion == REGION_NULL )  {
			/* Region began at this level */
			regionp->reg_treetop = curtree;
			regionp->reg_name = strdup(path_str(pathpos));
			regionp->reg_forw = HeadRegion;
			HeadRegion = regionp;
			nregions++;
			if( debug & DEBUG_REGIONS )
				fprintf(stderr,"Add Region %s\n", regionp->reg_name);
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
	fprintf(stderr,"REGION %s:\n", rp->reg_name );
	fprintf(stderr,"id=%d, air=%d, material=%d, los=%d\n",
		rp->reg_regionid, rp->reg_aircode,
		rp->reg_material, rp->reg_los );
	pr_tree( rp->reg_treetop, 0 );
	fprintf(stderr,"\n");
}

void
pr_tree( tp, lvl )
register union tree *tp;
int lvl;			/* recursion level */
{
	register int i;

	fprintf(stderr,"%.8x ", tp);
	for( i=3*lvl; i>0; i-- )
		putc(' ',stderr);	/* indent */

	if( tp == TREE_NULL )  {
		fprintf(stderr,"Null???\n");
		return;
	}

	switch( tp->tr_op )  {

	case OP_SOLID:
		fprintf(stderr,"SOLID %s (bin %d)",
			tp->tr_stp->st_name,
			tp->tr_stp->st_bin );
		break;

	default:
		fprintf(stderr,"Unknown op=x%x\n", tp->tr_op );
		return;

	case OP_UNION:
		fprintf(stderr,"UNION");
		break;
	case OP_INTERSECT:
		fprintf(stderr,"INTERSECT");
		break;
	case OP_SUBTRACT:
		fprintf(stderr,"MINUS");
		break;
	}
	fprintf(stderr," (%.0f,%.0f,%.0f) (%.0f,%.0f,%.0f)\n",
		tp->tr_min[0], tp->tr_min[1], tp->tr_min[2],
		tp->tr_max[0], tp->tr_max[1], tp->tr_max[2] );
	if( BINOP(tp) )  {
		/* BINARY TYPE */
		pr_tree( tp->tr_left, lvl+1 );
		pr_tree( tp->tr_right, lvl+1 );
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
 *  		R T B O M B
 *  
 *  Abort the RT library
 */
void
rtbomb(str)
char *str;
{
	fprintf(stderr,"\nrt FATAL ERROR %s.\n", str);
	fflush(stderr);
	exit(12);
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
}
