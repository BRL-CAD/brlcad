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
static char RCStree[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

struct rt_g rt_g;

int rt_pure_boolean_expressions = 0;

HIDDEN union tree *rt_drawobj();
HIDDEN void rt_add_regtree();
HIDDEN union tree *rt_mkbool_tree();
HIDDEN int rt_rpp_tree();
HIDDEN char *rt_basename();
HIDDEN struct region *rt_getregion();
HIDDEN void rt_fr_tree();

extern int	nul_prep(), nul_class();
extern int	tor_prep(), tor_class();
extern int	tgc_prep(), tgc_class();
extern int	ell_prep(), ell_class();
extern int	arb_prep(), arb_class();
extern int	hlf_prep(), hlf_class();
extern int	ars_prep(), ars_class();
extern int	rec_prep(), rec_class();
extern int	pg_prep(), pg_class();
extern int	spl_prep(), spl_class();
extern int	sph_prep(), sph_class();

extern void	nul_print(), nul_norm(), nul_uv();
extern void	tor_print(), tor_norm(), tor_uv();
extern void	tgc_print(), tgc_norm(), tgc_uv();
extern void	ell_print(), ell_norm(), ell_uv();
extern void	arb_print(), arb_norm(), arb_uv();
extern void	hlf_print(), hlf_norm(), hlf_uv();
extern void	ars_print(), ars_norm(), ars_uv();
extern void	rec_print(), rec_norm(), rec_uv();
extern void	pg_print(),  pg_norm(),  pg_uv();
extern void	spl_print(), spl_norm(), spl_uv();
extern void	sph_print(), sph_norm(), sph_uv();

extern void	nul_curve(), nul_free(), nul_plot();
extern void	tor_curve(), tor_free(), tor_plot();
extern void	tgc_curve(), tgc_free(), tgc_plot();
extern void	ell_curve(), ell_free(), ell_plot();
extern void	arb_curve(), arb_free(), arb_plot();
extern void	hlf_curve(), hlf_free(), hlf_plot();
extern void	ars_curve(), ars_free(), ars_plot();
extern void	rec_curve(), rec_free(), rec_plot();
extern void	pg_curve(),  pg_free(),  pg_plot();
extern void	spl_curve(), spl_free(), spl_plot();
extern void	sph_curve(), sph_free(), sph_plot();

extern struct seg *nul_shot();
extern struct seg *tor_shot();
extern struct seg *tgc_shot();
extern struct seg *ell_shot();
extern struct seg *arb_shot();
extern struct seg *ars_shot();
extern struct seg *hlf_shot();
extern struct seg *rec_shot();
extern struct seg *pg_shot();
extern struct seg *spl_shot();
extern struct seg *sph_shot();

struct rt_functab rt_functab[] = {
	"ID_NULL",	0,
		nul_prep,	nul_shot,	nul_print, 	nul_norm,
	 	nul_uv,		nul_curve,	nul_class,	nul_free,
		nul_plot,
		
	"ID_TOR",	1,
		tor_prep,	tor_shot,	tor_print,	tor_norm,
		tor_uv,		tor_curve,	tor_class,	tor_free,
		tor_plot,

	"ID_TGC",	1,
		tgc_prep,	tgc_shot,	tgc_print,	tgc_norm,
		tgc_uv,		tgc_curve,	tgc_class,	tgc_free,
		tgc_plot,

	"ID_ELL",	1,
		ell_prep,	ell_shot,	ell_print,	ell_norm,
		ell_uv,		ell_curve,	ell_class,	ell_free,
		ell_plot,

	"ID_ARB8",	0,
		arb_prep,	arb_shot,	arb_print,	arb_norm,
		arb_uv,		arb_curve,	arb_class,	arb_free,
		arb_plot,

	"ID_ARS",	1,
		ars_prep,	ars_shot,	ars_print,	ars_norm,
		ars_uv,		ars_curve,	ars_class,	ars_free,
		ars_plot,

	"ID_HALF",	0,
		hlf_prep,	hlf_shot,	hlf_print,	hlf_norm,
		hlf_uv,		hlf_curve,	hlf_class,	hlf_free,
		hlf_plot,

	"ID_REC",	1,
		rec_prep,	rec_shot,	rec_print,	rec_norm,
		rec_uv,		rec_curve,	rec_class,	rec_free,
		rec_plot,

	"ID_POLY",	1,
		pg_prep,	pg_shot,	pg_print,	pg_norm,
		pg_uv,		pg_curve,	pg_class,	pg_free,
		pg_plot,

	"ID_BSPLINE",	1,
		spl_prep,	spl_shot,	spl_print,	spl_norm,
		spl_uv,		spl_curve,	spl_class,	spl_free,
		spl_plot,

	"ID_SPH",	1,
		sph_prep,	sph_shot,	sph_print,	sph_norm,
		sph_uv,		sph_curve,	sph_class,	sph_free,
		sph_plot,

	">ID_NULL",	0,
		nul_prep,	nul_shot,	nul_print,	nul_norm,
		nul_uv,		nul_curve,	nul_class,	nul_free,
		nul_plot
};
int rt_nfunctab = sizeof(rt_functab)/sizeof(struct rt_functab);

/*
 *  Hooks for unimplemented routines
 */
#define DEF(func)	func() { rt_log("func unimplemented\n"); return; }
#define IDEF(func)	func() { rt_log("func unimplemented\n"); return(0); }

int IDEF(nul_prep)
struct seg * IDEF(nul_shot)
void DEF(nul_print)
void DEF(nul_norm)
void DEF(nul_uv)
void DEF(nul_curve)
int IDEF(nul_class)
void DEF(nul_free)
void DEF(nul_plot)

/* Map for database solidrec objects to internal objects */
static char idmap[] = {
	ID_NULL,	/* undefined, 0 */
	ID_NULL,	/* RPP	1 axis-aligned rectangular parallelopiped */
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
	ID_NULL,	/* REC	13 right elliptic cylinder */
	ID_NULL,	/* TRC	14 truncated regular cone */
	ID_NULL,	/* TEC	15 truncated elliptic cone */
	ID_TOR,		/* TOR	16 toroid */
	ID_NULL,	/* TGC	17 truncated general cone */
	ID_TGC,		/* GENTGC 18 supergeneralized TGC; internal form */
	ID_ELL,		/* GENELL 19: V,A,B,C */
	ID_ARB8,	/* GENARB8 20:  V, and 7 other vectors */
	ID_NULL,	/* ARS 21: arbitrary triangular-surfaced polyhedron */
	ID_NULL,	/* ARSCONT 22: extension record type for ARS solid */
	ID_NULL,	/* ELLG 23:  gift-only */
	ID_HALF,	/* HALFSPACE 24:  halfspace */
	ID_NULL		/* n+1 */
};

HIDDEN char *rt_path_str();

static struct mater_info rt_no_mater = {
	1.0, 1.0, 1.0,		/* color, RGB */
	0,			/* override */
	DB_INH_LOWER,		/* color inherit */
	DB_INH_LOWER		/* mater inherit */
};

double		rt_inv255 = 1.0/255.0;

/*
 *  			R T _ G E T _ T R E E
 *
 *  User-called function to add a tree hierarchy to the displayed set.
 *  
 *  Returns -
 *  	0	Ordinarily
 *	-1	On major error
 */
int
rt_gettree( rtip, node)
struct rt_i *rtip;
char *node;
{
	register union tree *curtree;
	register struct directory *dp;
	mat_t	root;
	struct mater_info root_mater;

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_gettree:  bad rtip\n");

	if(!rtip->needprep)
		rt_bomb("rt_gettree called again after rt_prep!");

	root_mater = rt_no_mater;	/* struct copy */

	if( (dp = rt_dir_lookup( rtip, node, LOOKUP_NOISY )) == DIR_NULL )
		return(-1);		/* ERROR */

	/* Process animations located at the root */
	mat_idn( root );
	if( rtip->rti_anroot )  {
		register struct animate *anp;
		mat_t	temp_root, arc;

		for( anp=rtip->rti_anroot; anp != ANIM_NULL; anp = anp->an_forw ) {
			if( dp != anp->an_path[0] )
				continue;
			mat_copy( temp_root, root );
			mat_idn( arc );
			rt_do_anim( anp, temp_root, arc, &root_mater );
			mat_mul( root, temp_root, arc );
		}
	}

	curtree = rt_drawobj( rtip, dp, REGION_NULL, 0, root, &root_mater );
	if( curtree != TREE_NULL )  {
		/*  Subtree has not been contained by a region.
		 *  This should only happen when a top-level solid
		 *  is encountered.  Build a special region for it.
		 */
		register struct region *regionp;	/* XXX */

		GETSTRUCT( regionp, region );
		rt_log("Warning:  Top level solid, region %s created\n",
			rt_path_str(0) );
		if( curtree->tr_op != OP_SOLID )
			rt_bomb("root subtree not Solid");
		regionp->reg_name = rt_strdup(rt_path_str(0));
		rt_add_regtree( rtip, regionp, curtree );
	}
	return(0);	/* OK */
}

static vect_t xaxis = { 1.0, 0, 0 };
static vect_t yaxis = { 0, 1.0, 0 };
static vect_t zaxis = { 0, 0, 1.0 };

/*
 *			R T _ A D D _ S O L I D
 */
HIDDEN
struct soltab *
rt_add_solid( rtip, rec, name, mat )
struct rt_i	*rtip;
union record	*rec;
char		*name;
matp_t		mat;
{
	register struct soltab *stp;
	static vect_t v[8];
	static vect_t A, B, C;
	static fastf_t fx, fy, fz;
	FAST fastf_t f;
	register struct soltab *nsp;
	int id;

	/* Validate that matrix preserves perpendicularity of axis */
	/* by checking that A.B == 0, B.C == 0, A.C == 0 */
	MAT4X3VEC( A, mat, xaxis );
	MAT4X3VEC( B, mat, yaxis );
	MAT4X3VEC( C, mat, zaxis );
	fx = VDOT( A, B );
	fy = VDOT( B, C );
	fz = VDOT( A, C );
	if( ! NEAR_ZERO(fx, 0.0001) ||
	    ! NEAR_ZERO(fy, 0.0001) ||
	    ! NEAR_ZERO(fz, 0.0001) )  {
		rt_log("rt_add_solid(%s):  matrix does not preserve axis perpendicularity.\n  X.Y=%g, Y.Z=%g, X.Z=%g\n",
			name, fx, fy, fz );
		mat_print("bad matrix", mat);
		return( SOLTAB_NULL );		/* BAD */
	}

	/*
	 *  Check to see if this exact solid has already been processed.
	 *  Match on leaf name and matrix.
	 */
	for( nsp = rtip->HeadSolid; nsp != SOLTAB_NULL; nsp = nsp->st_forw )  {
		register int i;

		if(
			name[0] != nsp->st_name[0]  ||	/* speed */
			name[1] != nsp->st_name[1]  ||	/* speed */
			strcmp( name, nsp->st_name ) != 0
		)
			continue;
		for( i=0; i<16; i++ )  {
			f = mat[i] - nsp->st_pathmat[i];
			if( !NEAR_ZERO(f, 0.0001) )
				goto next_one;
		}
		/* Success, we have a match! */
		if( rt_g.debug & DEBUG_SOLIDS )
			rt_log("rt_add_solid:  %s re-referenced\n",
				name );
		return(nsp);
next_one: ;
	}

	if( (id = rt_id_solid( rec )) == ID_NULL )  
		return( SOLTAB_NULL );		/* BAD */

	if( id < 0 || id >= rt_nfunctab )
		rt_bomb("rt_add_solid:  bad st_id");

	GETSTRUCT(stp, soltab);
	stp->st_id = id;
	if( rec->u_id == ID_SOLID )  {
		/* Convert from database (float) to fastf_t */
		rt_fastf_float( (fastf_t *)v, rec->s.s_values, 8 );
	}
	stp->st_name = name;
	stp->st_specific = (int *)0;

	/* init solid's maxima and minima */
	VSETALL( stp->st_max, -INFINITY );
	VSETALL( stp->st_min,  INFINITY );

	if( rt_functab[id].ft_prep( v, stp, mat, &(rec->s), rtip ) )  {
		/* Error, solid no good */
		rt_free( (char *)stp, "struct soltab");
		return( SOLTAB_NULL );		/* BAD */
	}
	id = stp->st_id;	/* type may have changed in prep */

	/* For now, just link them all onto the same list */
	stp->st_forw = rtip->HeadSolid;
	rtip->HeadSolid = stp;

	mat_copy( stp->st_pathmat, mat );

	/*
	 * Update the model maxima and minima
	 *
	 *  Don't update min & max for halfspaces;  instead, add them
	 *  to the list of infinite solids, for special handling.
	 */
	if( stp->st_aradius >= INFINITY )  {
		rt_cut_extend( &rtip->rti_inf_box, stp );
	}  else  {
		VMINMAX( rtip->mdl_min, rtip->mdl_max, stp->st_min );
		VMINMAX( rtip->mdl_min, rtip->mdl_max, stp->st_max );
	}

	stp->st_bit = rtip->nsolids++;
	if(rt_g.debug&DEBUG_SOLIDS)  {
		rt_log("------------ %s (bit %d) %s ------------\n",
			stp->st_name, stp->st_bit, rt_functab[id].ft_name );
		VPRINT("Bound Sph CENTER", stp->st_center);
		rt_log("Approx Sph Radius = %g\n", stp->st_aradius);
		rt_log("Bounding Sph Radius = %g\n", stp->st_bradius);
		VPRINT("Bound RPP min", stp->st_min);
		VPRINT("Bound RPP max", stp->st_max);
		rt_functab[id].ft_print( stp );
	}
	return( stp );
}

/*
 *			R T _ I D _ S O L I D
 *
 *  Given a database record, determine the proper rt_functab subscript.
 *  Used by MGED as well as internally to librt.
 */
int
rt_id_solid( rec )
register union record *rec;
{
	register int id;

	switch( rec->u_id )  {
	case ID_SOLID:
		id = idmap[rec->s.s_type];
		break;
	case ID_ARS_A:
		id = ID_ARS;
		break;
	case ID_P_HEAD:
		id = ID_POLY;
		break;
	case ID_BSOLID:
		id = ID_BSPLINE;
		break;
	default:
		rt_log("rt_id_solid:  u_id=x%x unknown\n", rec->u_id);
		id = ID_NULL;		/* BAD */
		break;
	}
	return(id);
}

/*
 * Note that while GED has a more limited MAXLEVELS, GED can
 * work on sub-trees, while RT must be able to process the full tree.
 * Thus the difference, and the large value here.
 */
#define	MAXLEVELS	64
static struct directory	*path[MAXLEVELS];	/* Record of current path */

struct tree_list {
	union tree *tl_tree;
	int	tl_op;
};

/*
 *			R T _ D R A W _ O B J
 *
 * This routine is used to get an object drawn.
 * The actual processing of solids is performed by rt_add_solid(),
 * but all transformations and region building is done here.
 *
 * NOTE that this routine is used recursively, so no variables may
 * be declared static.
 */
HIDDEN
union tree *
rt_drawobj( rtip, dp, argregion, pathpos, old_xlate, materp )
struct rt_i	*rtip;
struct directory *dp;
struct region	*argregion;
matp_t		old_xlate;
struct mater_info *materp;
{
	auto union record rec;		/* local copy of this record */
	register int i;
	auto int j;
	union tree *curtree;		/* ptr to current tree top */
	struct region *regionp;
	union record *members;		/* ptr to array of member recs */
	int subtreecount;		/* number of non-null subtrees */
	struct tree_list *trees;	/* ptr to array of structs */
	struct tree_list *tlp;		/* cur tree_list */
	struct mater_info curmater;

	if( pathpos >= MAXLEVELS )  {
		rt_log("%s: nesting exceeds %d levels\n",
			rt_path_str(MAXLEVELS), MAXLEVELS );
		return(TREE_NULL);
	}
	path[pathpos] = dp;

	/*
	 * Load the first record of the object into local record buffer
	 */
#ifdef DB_MEM
	rec = rtip->rti_db[dp->d_addr];	/* struct copy */
#else DB_MEM
#ifdef CRAY_COS
	/* CRAY COS can't fseek() properly, hence this hideous I/O hack */
	rewind(rtip->fp);
	while( !feof(rtip->fp) && ftell(rtip->fp) <= dp->d_addr  &&
	    fread( (char *)&rec, sizeof rec, 1, rtip->fp ) == 1 )
		/* NULL */ ;
#else CRAY_COS
	if( fseek( rtip->fp, dp->d_addr, 0 ) < 0 ||
	    fread( (char *)&rec, sizeof rec, 1, rtip->fp ) != 1 )  {
		rt_log("rt_drawobj: %s record read error\n",
			rt_path_str(pathpos) );
		return(TREE_NULL);
	}
#endif CRAY_COS
#endif DB_MEM

	/*
	 *  Draw a solid
	 */
	if( rec.u_id == ID_SOLID || rec.u_id == ID_ARS_A ||
	    rec.u_id == ID_P_HEAD || rec.u_id == ID_BSOLID )  {
		register struct soltab *stp;
		register union tree *xtp;

		if( (stp = rt_add_solid( rtip, &rec, dp->d_namep, old_xlate )) ==
		    SOLTAB_NULL )
			return( TREE_NULL );

		/**GETSTRUCT( xtp, union tree ); **/
		if( (xtp=(union tree *)rt_malloc(sizeof(union tree), "solid tree"))
		    == TREE_NULL )
			rt_bomb("rt_drawobj: solid tree malloc failed\n");
		bzero( (char *)xtp, sizeof(union tree) );
		xtp->tr_op = OP_SOLID;
		xtp->tr_a.tu_stp = stp;
		xtp->tr_a.tu_name = rt_strdup(rt_path_str(pathpos));
		xtp->tr_regionp = argregion;
		return( xtp );
	}

	if( rec.u_id != ID_COMB )  {
		rt_log("rt_drawobj:  %s defective database record, type '%c' (0%o), addr=x%x\n",
			dp->d_namep,
			rec.u_id, rec.u_id, dp->d_addr );
		return(TREE_NULL);			/* ERROR */
	}

	/*
	 *  Process a Combination (directory) node
	 */
	if( rec.c.c_length <= 0 )  {
		rt_log(  "Warning: combination with zero members \"%.16s\".\n",
			rec.c.c_name );
		return(TREE_NULL);
	}
	regionp = argregion;

	/*
	 *  Handle inheritance of material property.
	 *  Color and the material property have separate
	 *  inheritance interlocks.
	 */
	curmater = *materp;	/* struct copy */
	if( rec.c.c_override == 1 )  {
		if( argregion != REGION_NULL )  {
			rt_log("rt_drawobj: ERROR: color override in combination within region %s\n", argregion->reg_name );
		} else {
			if( curmater.ma_cinherit == DB_INH_LOWER )  {
				curmater.ma_override = 1;
				curmater.ma_color[0] = (rec.c.c_rgb[0]+0.5)*rt_inv255;
				curmater.ma_color[1] = (rec.c.c_rgb[1]+0.5)*rt_inv255;
				curmater.ma_color[2] = (rec.c.c_rgb[2]+0.5)*rt_inv255;
				curmater.ma_cinherit = rec.c.c_inherit;
			}
		}
	}
	if( rec.c.c_matname[0] != '\0' )  {
		if( argregion != REGION_NULL )  {
			rt_log("rt_drawobj: ERROR: material property spec in combination within region %s\n", argregion->reg_name );
		} else {
			if( curmater.ma_minherit == DB_INH_LOWER )  {
				strncpy( curmater.ma_matname, rec.c.c_matname, sizeof(rec.c.c_matname) );
				strncpy( curmater.ma_matparm, rec.c.c_matparm, sizeof(rec.c.c_matparm) );
				curmater.ma_minherit = rec.c.c_inherit;
			}
		}
	}

	/* Handle combinations which are the top of a "region" */
	if( rec.c.c_flags == 'R' )  {
		if( argregion != REGION_NULL )  {
			if( rt_g.debug & DEBUG_REGIONS )
				rt_log("Warning:  region %s within region %s\n",
					rt_path_str(pathpos),
					argregion->reg_name );
/***			argregion = REGION_NULL;	/* override! */
		} else {
			register struct region *nrp;

			/* Ignore "air" regions unless wanted */
			if( rtip->useair == 0 &&  rec.c.c_aircode != 0 )
				return(TREE_NULL);

			/* Start a new region here */
			GETSTRUCT( nrp, region );
			nrp->reg_forw = REGION_NULL;
			nrp->reg_regionid = rec.c.c_regionid;
			nrp->reg_aircode = rec.c.c_aircode;
			nrp->reg_gmater = rec.c.c_material;
			nrp->reg_name = rt_strdup(rt_path_str(pathpos));
			nrp->reg_mater = curmater;	/* struct copy */
			/* Material property processing in rt_add_regtree() */
			regionp = nrp;
		}
	}

	/* Read all the member records */
	i = sizeof(union record) * rec.c.c_length;
	j = sizeof(struct tree_list) * rec.c.c_length;
	if( (members = (union record *)rt_malloc(i, "member records")) ==
	    (union record *)0  ||
	    (trees = (struct tree_list *)rt_malloc( j, "tree_list array" )) ==
	    (struct tree_list *)0 )
		rt_bomb("rt_drawobj:  malloc failure\n");

#ifdef DB_MEM
	/* For now, the "members" struct array is simply ignored */
#else DB_MEM
	/* Read in all members of this combination before recursing */
	if( fread( (char *)members, sizeof(union record), rec.c.c_length,
	    rtip->fp ) != rec.c.c_length )  {
		rt_log("rt_drawobj:  %s member read error\n",
			rt_path_str(pathpos) );
		return(TREE_NULL);
	}
#endif DB_MEM

	/* Process and store all the sub-trees */
	subtreecount = 0;
	tlp = trees;
	for( i=0; i<rec.c.c_length; i++ )  {
		register struct member *mp;
		auto struct directory *nextdp;
		auto mat_t new_xlate;		/* Accum translation mat */
		auto struct mater_info newmater;
		mat_t xmat;		/* temp fastf_t matrix for conv */

#ifdef DB_MEM
		mp = &(rtip->rti_db[dp->d_addr+i+1].M);
#else DB_MEM
		mp = &(members[i].M);
#endif DB_MEM
		if( mp->m_id != ID_MEMB )  {
			rt_log("rt_drawobj:  defective member of %s\n", dp->d_namep);
			continue;
		}
		if( (nextdp = rt_dir_lookup( rtip, mp->m_instname, LOOKUP_NOISY )) ==
		    DIR_NULL )
			continue;

		newmater = curmater;	/* struct copy -- modified below */

		/* convert matrix to fastf_t from disk format */
		rt_mat_dbmat( xmat, mp->m_mat );

		/* Check here for animation to apply */
		{
			register struct animate *anp;

			for( anp = nextdp->d_animate; anp != ANIM_NULL; anp = anp->an_forw ) {
				register int i = anp->an_pathlen-2;
				register int j = pathpos-1;

				for( ; i>=0 && j>=0; i--, j-- )  {
					if( anp->an_path[i] != path[j] )  {
rt_log("%s != %s\n", anp->an_path[i]->d_namep, path[j]->d_namep);
						goto next_one;
					}
				}
				rt_do_anim( anp,
					old_xlate, xmat, &newmater );
next_one:			;
			}
			mat_mul(new_xlate, old_xlate, xmat);
		}

		/* Recursive call */
		if( (tlp->tl_tree = rt_drawobj( rtip,
		    nextdp, regionp, pathpos+1, new_xlate, &newmater )
		    ) == TREE_NULL )
			continue;

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
			rt_log("Warning:  Forced to create region %s\n",
				rt_path_str(pathpos+1) );
			if((tlp->tl_tree)->tr_op != OP_SOLID )
				rt_bomb("subtree not Solid");
			GETSTRUCT( xrp, region );
			xrp->reg_name = rt_strdup(rt_path_str(pathpos+1));
			rt_add_regtree( rtip, xrp, (tlp->tl_tree) );
			tlp->tl_tree = TREE_NULL;
			continue;	/* no remaining subtree, go on */
		}

		/* Store operation on subtree */
		switch( mp->m_relation )  {
		default:
			rt_log("%s: bad m_relation '%c'\n",
				regionp->reg_name, mp->m_relation );
			/* FALL THROUGH */
		case UNION:
			tlp->tl_op = OP_UNION;
			break;
		case SUBTRACT:
			tlp->tl_op = OP_SUBTRACT;
			break;
		case INTERSECT:
			tlp->tl_op = OP_INTERSECT;
			break;
		}
		subtreecount++;
		tlp++;
	}

	if( subtreecount <= 0 )  {
		/* Null subtree in region, release region struct */
		if( argregion == REGION_NULL && regionp != REGION_NULL )  {
			rt_free( regionp->reg_name, "unused region name" );
			rt_free( (char *)regionp, "unused region struct" );
		}
		curtree = TREE_NULL;
		goto out;
	}

	/* Build tree representing boolean expression in Member records */
	if( rt_pure_boolean_expressions )  {
		curtree = rt_mkbool_tree( trees, subtreecount, regionp );
	} else {
		register struct tree_list *tstart;

		/*
		 * This is the way GIFT interpreted equations, so we
		 * duplicate it here.  Any expressions between UNIONs
		 * is evaluated first, eg:
		 *	A - B - C u D - E - F
		 * is	(A - B - C) u (D - E - F)
		 * so first we do the parenthesised parts, and then go
		 * back and glue the unions together.
		 * As always, unions are the downfall of free enterprise!
		 */
		tstart = trees;
		tlp = trees+1;
		for( i=subtreecount-1; i>=0; i--, tlp++ )  {
			/* If we went off end, or hit a union, do it */
			if( i>0 && tlp->tl_op != OP_UNION )
				continue;
			if( (j = tlp-tstart) <= 0 )
				continue;
			tstart->tl_tree = rt_mkbool_tree( tstart, j, regionp );
			if(rt_g.debug&DEBUG_REGIONS) rt_pr_tree(tstart->tl_tree, 0);
			/* has side effect of zapping all trees,
			 * so build new first node */
			tstart->tl_op = OP_UNION;
			/* tstart here at union */
			tstart = tlp;
		}
		curtree = rt_mkbool_tree( trees, subtreecount, regionp );
		if(rt_g.debug&DEBUG_REGIONS) rt_pr_tree(curtree, 0);
	}

	if( argregion == REGION_NULL )  {
		/* Region began at this level */
		rt_add_regtree( rtip, regionp, curtree );
		curtree = TREE_NULL;		/* no remaining subtree */
	}

	/* Release dynamic memory and return */
out:
	rt_free( (char *)trees, "tree_list array");
	rt_free( (char *)members, "member records" );
	return( curtree );
}

/*
 *			R T _ M K B O O L _ T R E E
 *
 *  Given a tree_list array, build a tree of "union tree" nodes
 *  appropriately connected together.  Every element of the
 *  tree_list array used is replaced with a TREE_NULL.
 *  Elements which are already TREE_NULL are ignored.
 *  Returns a pointer to the top of the tree.
 */
HIDDEN union tree *
rt_mkbool_tree( tree_list, howfar, regionp )
struct tree_list *tree_list;
int howfar;
struct region *regionp;
{
	register struct tree_list *tlp;
	register int i;
	register struct tree_list *first_tlp;
	register union tree *xtp;
	register union tree *curtree;
	register int inuse;

	if( howfar <= 0 )
		return(TREE_NULL);

	/* Count number of non-null sub-trees to do */
	for( i=howfar, inuse=0, tlp=tree_list; i>0; i--, tlp++ )  {
		if( tlp->tl_tree == TREE_NULL )
			continue;
		if( inuse++ == 0 )
			first_tlp = tlp;
	}
	if( rt_g.debug & DEBUG_REGIONS && first_tlp->tl_op != OP_UNION )
		rt_log("Warning: %s: non-union first operation ignored\n",
			regionp->reg_name);

	/* Handle trivial cases */
	if( inuse <= 0 )
		return(TREE_NULL);
	if( inuse == 1 )  {
		curtree = first_tlp->tl_tree;
		first_tlp->tl_tree = TREE_NULL;
		return( curtree );
	}

	/* Allocate all the tree structs we will need */
	i = sizeof(union tree)*(inuse-1);
	if( (xtp=(union tree *)rt_malloc( i, "tree array")) == TREE_NULL )
		rt_bomb("rt_mkbool_tree: malloc failed\n");
	bzero( (char *)xtp, i );

	curtree = first_tlp->tl_tree;
	first_tlp->tl_tree = TREE_NULL;
	tlp=first_tlp+1;
	for( i=howfar-(tlp-tree_list); i>0; i--, tlp++ )  {
		if( tlp->tl_tree == TREE_NULL )
			continue;

		xtp->tr_b.tb_left = curtree;
		xtp->tr_b.tb_right = tlp->tl_tree;
		xtp->tr_b.tb_regionp = regionp;
		xtp->tr_op = tlp->tl_op;
		curtree = xtp++;
		tlp->tl_tree = TREE_NULL;	/* empty the input slot */
	}
	return(curtree);
}

/*
 *			R T _ P A T H _ S T R
 */
HIDDEN char *
rt_path_str( pos )
int pos;
{
	static char line[MAXLEVELS*(16+2)];
	register char *cp = &line[0];
	register int i;

	if( pos >= MAXLEVELS )  pos = MAXLEVELS-1;
	line[0] = '/';
	line[1] = '\0';
	for( i=0; i<=pos; i++ )  {
		(void)sprintf( cp, "/%s", path[i]->d_namep );
		cp += strlen(cp);
	}
	return( &line[0] );
}

/*
 *			R T _ P L O O K U P
 * 
 *  Look up a path where the elements are separates by slashes,
 *  filling in the path[] array along the way.  Leading slashes
 *  have no significance.  If the whole path is valid, malloc space
 *  and duplicate the used portion of the path[] array, and set the
 *  callers pointer to point there.  The return is the number of
 *  elements in the path, or 0 or -1 on error.
 */
int
rt_plookup( rtip, dirp, cp, noisy )
struct rt_i	*rtip;
struct directory ***dirp;
register char	*cp;
int		noisy;
{
	int	depth;
	char	oldc;
	register char *ep;
	struct directory *dp;
	struct directory **newpath;

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_plookup:  bad rtip\n");

	depth = 0;
	if( *cp == '\0' )  return(-1);

	/* First, look up the names of the individual path elements */
	do {
		while( *cp && *cp == '/' )
			cp++;		/* skip leading slashes */
		ep = cp;
		while( *ep != '\0' && *ep != '/' )
			ep++;		/* walk over element name */
		oldc = *ep;
		*ep = '\0';
		if( (dp = rt_dir_lookup( rtip, cp, noisy )) == DIR_NULL )
			return(-1);
		path[depth++] = dp;
		cp = ep+1;
	} while( oldc != '\0' );

	/* Here, it might make sense to see if path[n+1] is
	 * actually mentioned in path[n], but save that for later...
	 */

	/* Successful conversion of path, duplicate and return */
	newpath = (struct directory **) rt_malloc(
		 depth*sizeof(struct directory *), "rt_plookup newpath");
	bcopy( (char *)path, (char *)newpath,
		depth*sizeof(struct directory *) );
	*dirp = newpath;
	return( depth );
}

/*
 *			R T _ P R _ R E G I O N
 */
void
rt_pr_region( rp )
register struct region *rp;
{
	rt_log("REGION %s (bit %d)\n", rp->reg_name, rp->reg_bit );
	rt_log("id=%d, air=%d, gift_material=%d, los=%d\n",
		rp->reg_regionid, rp->reg_aircode,
		rp->reg_gmater, rp->reg_los );
	if( rp->reg_mater.ma_override == 1 )
		rt_log("Color %d %d %d\n",
			(int)rp->reg_mater.ma_color[0]*255.,
			(int)rp->reg_mater.ma_color[1]*255.,
			(int)rp->reg_mater.ma_color[2]*255. );
	if( rp->reg_mater.ma_matname[0] != '\0' )
		rt_log("Material '%s' '%s'\n",
			rp->reg_mater.ma_matname,
			rp->reg_mater.ma_matparm );
	rt_pr_tree( rp->reg_treetop, 0 );
	rt_log("\n");
}

/*
 *			R T _ P R _ T R E E
 */
void
rt_pr_tree( tp, lvl )
register union tree *tp;
int lvl;			/* recursion level */
{
	register int i;

	rt_log("%.8x ", tp);
	for( i=lvl; i>0; i-- )
		rt_log("  ");

	if( tp == TREE_NULL )  {
		rt_log("Null???\n");
		return;
	}

	switch( tp->tr_op )  {

	case OP_SOLID:
		rt_log("SOLID %s (bit %d)",
			tp->tr_a.tu_stp->st_name,
			tp->tr_a.tu_stp->st_bit );
		break;

	default:
		rt_log("Unknown op=x%x\n", tp->tr_op );
		return;

	case OP_UNION:
		rt_log("UNION");
		break;
	case OP_INTERSECT:
		rt_log("INTERSECT");
		break;
	case OP_SUBTRACT:
		rt_log("MINUS");
		break;
	case OP_XOR:
		rt_log("XOR");
		break;
	case OP_NOT:
		rt_log("NOT");
		break;
	}
	rt_log("\n");

	switch( tp->tr_op )  {
	case OP_SOLID:
		break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* BINARY type */
		rt_pr_tree( tp->tr_b.tb_left, lvl+1 );
		rt_pr_tree( tp->tr_b.tb_right, lvl+1 );
		break;
	case OP_NOT:
	case OP_GUARD:
		/* UNARY tree */
		rt_pr_tree( tp->tr_b.tb_left, lvl+1 );
		break;
	}
}

/*
 *			R T _ R P P _ T R E E
 *
 *	Calculate the bounding RPP of the region whose boolean tree is 'tp'.
 *	Depends on caller having initialized min_rpp and max_rpp.
 *	Returns 0 for failure (and prints a diagnostic), or 1 for success.
 */
HIDDEN int
rt_rpp_tree( tp, min_rpp, max_rpp )
register union tree *tp;
register fastf_t *min_rpp, *max_rpp;
{	
	register int i;

	if( tp == TREE_NULL )  {
		rt_log( "librt/rt_rpp_tree: NULL tree pointer.\n" );
		return(0);
	}

	switch( tp->tr_op )  {

	case OP_SOLID:
		VMINMAX( min_rpp, max_rpp, tp->tr_a.tu_stp->st_min );
		VMINMAX( min_rpp, max_rpp, tp->tr_a.tu_stp->st_max );
		return(1);

	default:
		rt_log( "librt/rt_rpp_tree: unknown op=x%x\n", tp->tr_op );
		return(0);

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* BINARY type */
		if( !rt_rpp_tree( tp->tr_b.tb_left, min_rpp, max_rpp )  ||
		    !rt_rpp_tree( tp->tr_b.tb_right, min_rpp, max_rpp )  )
			return	0;
		break;
	case OP_NOT:
	case OP_GUARD:
		/* UNARY tree */
		if( ! rt_rpp_tree( tp->tr_b.tb_left, min_rpp, max_rpp ) )
			return	0;
		break;
	}
	return	1;
}

/*
 *			R T _ B A S E N A M E
 *
 *  Given a string containing slashes such as a pathname, return a
 *  pointer to the first character after the last slash.
 */
HIDDEN char *
rt_basename( str )
register char	*str;
{	
	register char	*p = str;
	while( *p != '\0' )
		if( *p++ == '/' )
			str = p;
	return	str;
}

/*
 *			R T _ G E T R E G I O N
 *
 *  Return a pointer to the corresponding region structure of the given
 *  region's name (reg_name), or REGION_NULL if it does not exist.
 *
 *  If the full path of a region is specified, then that one is
 *  returned.  However, if only the database node name of the
 *  region is specified and that region has been referenced multiple
 *  time in the tree, then this routine will simply return the first one.
 */
HIDDEN struct region *
rt_getregion( rtip, reg_name )
struct rt_i	*rtip;
register char	*reg_name;
{	
	register struct region	*regp = rtip->HeadRegion;
	register char *reg_base = rt_basename(reg_name);

	for( ; regp != REGION_NULL; regp = regp->reg_forw )  {	
		register char	*cp;
		/* First, check for a match of the full path */
		if( *reg_base == regp->reg_name[0] &&
		    strcmp( reg_base, regp->reg_name ) == 0 )
			return(regp);
		/* Second, check for a match of the database node name */
		cp = rt_basename( regp->reg_name );
		if( *cp == *reg_name && strcmp( cp, reg_name ) == 0 )
			return(regp);
	}
	return(REGION_NULL);
}

/*
 *			R T _ R P P _ R E G I O N
 *
 *  Calculate the bounding RPP for a region given the name of
 *  the region node in the database.  See remarks in rt_getregion()
 *  above for name conventions.
 *  Returns 0 for failure (and prints a diagnostic), or 1 for success.
 */
int
rt_rpp_region( rtip, reg_name, min_rpp, max_rpp )
struct rt_i		*rtip;
char			*reg_name;
register fastf_t	*min_rpp, *max_rpp;
{	
	register struct region	*regp = rt_getregion( rtip, reg_name );

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_rpp_region:  bad rtip\n");

	if( regp == REGION_NULL )  return(0);
	VMOVE( min_rpp, rtip->mdl_max );
	VMOVE( max_rpp, rtip->mdl_min );
	return( rt_rpp_tree( regp->reg_treetop, min_rpp, max_rpp ) );
}

/*
 *			R T _ F A S T F _ F L O A T
 *
 *  Convert TO fastf_t FROM 3xfloats (for database) 
 */
void
rt_fastf_float( ff, fp, n )
register fastf_t *ff;
register dbfloat_t *fp;
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
 *			R T _ M A T _ D B M A T
 *
 *  Convert TO fastf_t matrix FROM dbfloats (for database) 
 */
void
rt_mat_dbmat( ff, dbp )
register fastf_t *ff;
register dbfloat_t *dbp;
{

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
}

/*
 *			R T _ D B M A T _ M A T
 *
 *  Convert FROM fastf_t matrix TO dbfloats (for updating database) 
 */
void
rt_dbmat_mat( dbp, ff )
register dbfloat_t *dbp;
register fastf_t *ff;
{

	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;

	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;

	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;

	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;
}

/*
 *  			R T _ F I N D _ S O L I D
 *  
 *  Given the (leaf) name of a solid, find the first occurance of it
 *  in the solid list.  Used mostly to find the light source.
 *  Returns soltab pointer, or SOLTAB_NULL.
 */
struct soltab *
rt_find_solid( rtip, name )
struct rt_i *rtip;
register char *name;
{
	register struct soltab *stp;
	register char *cp;

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_find_solid:  bad rtip\n");

	for( stp=rtip->HeadSolid; stp != SOLTAB_NULL; stp = stp->st_forw )  {
		if( *(cp = stp->st_name) == *name  &&
		    strcmp( cp, name ) == 0 )  {
			return(stp);
		}
	}
	return(SOLTAB_NULL);
}


/*
 *  			R T _ A D D _ R E G T R E E
 *  
 *  Add a region and it's boolean tree to all the appropriate places.
 *  The region and treetop are cross-linked, and the region is added
 *  to the linked list of regions.
 *  Positions in the region bit vector are established at this time.
 */
HIDDEN void
rt_add_regtree( rtip, regp, tp )
struct rt_i *rtip;
register struct region *regp;
register union tree *tp;
{
	register union tree *xor;

	/* Cross-reference */
	regp->reg_treetop = tp;
	tp->tr_regionp = regp;
	/* Add to linked list */
	regp->reg_forw = rtip->HeadRegion;
	rtip->HeadRegion = regp;

	/* Determine material properties */
	regp->reg_mfuncs = (char *)0;
	regp->reg_udata = (char *)0;
	if( regp->reg_mater.ma_override == 0 )
		rt_region_color_map(regp);

	regp->reg_bit = rtip->nregions;	/* Add to bit vectors */
	/* Will be added to rtip->Regions[] in final prep stage */
	rtip->nregions++;
	if( rt_g.debug & DEBUG_REGIONS )
		rt_log("Add Region %s\n", regp->reg_name);
}

/*
 *			R T _ D E L _ R E G T R E E
 *
 *  Remove a region from the linked list.  Used to remove a particular
 *  region from the active database, presumably after some useful
 *  information has been extracted (eg, a light being converted to
 *  implicit type), or for special effects.
 *
 *  Returns -
 *	-1	if unable to find indicated region
 *	 0	success
 */
int
rt_del_regtree( rtip, delregp )
struct rt_i *rtip;
register struct region *delregp;
{
	register struct region *regp;
	register struct region *nextregp;

	if( rt_g.debug & DEBUG_REGIONS )
		rt_log("Del Region %s\n", regp->reg_name);

	if( (regp = rtip->HeadRegion ) == delregp )  {
		rtip->HeadRegion = regp->reg_forw;
		goto zot;
	}

	for( ; regp != REGION_NULL; regp=nextregp )  {
		nextregp=regp->reg_forw;
		if( nextregp == delregp )  {
			regp->reg_forw = nextregp->reg_forw;	/* unlink */
			goto zot;
		}
	}
	rt_log("rt_del_region:  unable to find %s\n", delregp->reg_name);
	return(-1);
zot:
	rt_fr_tree( delregp->reg_treetop );
	rt_free( delregp->reg_name, "region name str");
	rt_free( (char *)delregp, "struct region");
	return(0);
}

/*
 *			R T _ O P T I M _ T R E E
 */
HIDDEN void
rt_optim_tree( tp )
register union tree *tp;
{
#define STACKDEPTH	10000
	LOCAL union tree *stackpile[STACKDEPTH];
	register union tree **sp;
	register union tree *low;

	sp = stackpile;
	*sp++ = TREE_NULL;
	*sp++ = tp;
	while( (tp = *--sp) != TREE_NULL ) {
		switch( tp->tr_op )  {
		case OP_SOLID:
			break;
		case OP_SUBTRACT:
			while( (low=tp->tr_b.tb_left)->tr_op == OP_SUBTRACT )  {
				/* Rewrite X - A - B as X - ( A union B ) */
				tp->tr_b.tb_left = low->tr_b.tb_left;
				low->tr_op = OP_UNION;
				low->tr_b.tb_left = low->tr_b.tb_right;
				low->tr_b.tb_right = tp->tr_b.tb_right;
				tp->tr_b.tb_right = low;
			}
			/* push both nodes - search left first */
			*sp++ = tp->tr_b.tb_right;
			*sp++ = tp->tr_b.tb_left;
			if( sp >= &stackpile[STACKDEPTH-1] )  {
				rt_log("rt_optim_tree: stack overflow!\n");
				return;
			}
			break;
		case OP_UNION:
		case OP_INTERSECT:
		case OP_XOR:
			/* Need to look at 3-level optimizations here, both sides */
			/* push both nodes - search left first */
			*sp++ = tp->tr_b.tb_right;
			*sp++ = tp->tr_b.tb_left;
			if( sp >= &stackpile[STACKDEPTH-1] )  {
				rt_log("rt_optim_tree: stack overflow!\n");
				return;
			}
			break;
		default:
			rt_log("rt_optim_tree: bad op x%x\n", tp->tr_op);
			break;
		}
	}
}

/*
 *			R T _ F R  _ T R E E
 */
HIDDEN void
rt_fr_tree( tp )
register union tree *tp;
{
	register union tree *low;

	switch( tp->tr_op )  {
	case OP_SOLID:
		rt_free( (char *)tp, "leaf tree union");
		return;
	case OP_SUBTRACT:
	case OP_UNION:
	case OP_INTERSECT:
	case OP_XOR:
		rt_fr_tree( tp->tr_b.tb_left );
		rt_fr_tree( tp->tr_b.tb_right );
		/*rt_free( (char *)tp, "binary tree union"); XXX*/
		return;
	default:
		rt_log("rt_fr_tree: bad op x%x\n", tp->tr_op);
		return;
	}
}

/*
 *  			S O L I D _ B I T F I N D E R
 *  
 *  Used to walk the boolean tree, setting bits for all the solids in the tree
 *  to the provided bit vector.  Should be called AFTER the region bits
 *  have been assigned.
 */
HIDDEN void
rt_solid_bitfinder( treep, regbit )
register union tree *treep;
register int regbit;
{
#define STACKDEPTH	10000
	LOCAL union tree *stackpile[STACKDEPTH];
	register union tree **sp;
	register struct soltab *stp;

	sp = stackpile;
	*sp++ = TREE_NULL;
	*sp++ = treep;
	while( (treep = *--sp) != TREE_NULL ) {
		switch( treep->tr_op )  {
		case OP_SOLID:
			stp = treep->tr_a.tu_stp;
			BITSET( stp->st_regions, regbit );
			if( !BITTEST( stp->st_regions, regbit ) )
				rt_bomb("BITSET failure\n");	/* sanity check */
			if( regbit+1 > stp->st_maxreg )  stp->st_maxreg = regbit+1;
			if( rt_g.debug&DEBUG_REGIONS )  {
				rt_pr_bitv( stp->st_name, stp->st_regions,
					stp->st_maxreg );
			}
			break;
		case OP_UNION:
		case OP_INTERSECT:
		case OP_SUBTRACT:
			/* BINARY type */
			/* push both nodes - search left first */
			*sp++ = treep->tr_b.tb_right;
			*sp++ = treep->tr_b.tb_left;
			if( sp >= &stackpile[STACKDEPTH-1] )  {
				rt_log("rt_optim_tree: stack overflow!\n");
				return;
			}
			break;
		default:
			rt_log("rt_solid_bitfinder:  op=x%x\n", treep->tr_op);
			break;
		}
	}
}

/*
 *  			R T _ P R E P
 *  
 *  This routine should be called just before the first call to rt_shootray().
 *  It should only be called ONCE per execution, unless rt_clean() is
 *  called inbetween.
 */
void
rt_prep(rtip)
register struct rt_i *rtip;
{
	register struct region *regp;
	register struct soltab *stp;

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_prep:  bad rtip\n");

	if(!rtip->needprep)
		rt_bomb("rt_prep: re-invocation");
	rtip->needprep = 0;
	if( rtip->nsolids <= 0 )
		rt_bomb("rt_prep:  no solids to prep");

	/* Compute size of model-specific variable-length data structures */
	/* -sizeof(bitv_t) == sizeof(struct partition.pt_solhit) */
	rtip->rti_pt_bytes = sizeof(struct partition) + 
		BITS2BYTES(rtip->nsolids) - sizeof(bitv_t) + 1;
	rtip->rti_bv_bytes = BITS2BYTES(rtip->nsolids) +
		BITS2BYTES(rtip->nregions) + 4*sizeof(bitv_t);

	if( rtip->rti_db )  {
		rt_free( (char *)rtip->rti_db, "in-core database");
		rtip->rti_db = (union record *)0;
	}

	/*
	 *  Allocate space for a per-solid bit of rtip->nregions length.
	 */
	for( stp=rtip->HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw )  {
		stp->st_regions = (bitv_t *)rt_malloc(
			BITS2BYTES(rtip->nregions)+sizeof(bitv_t),
			"st_regions bitv" );
		BITZERO( stp->st_regions, rtip->nregions );
		stp->st_maxreg = 0;
	}

	/* In case everything is a halfspace, set a minimum space */
	if( rtip->mdl_min[X] >= INFINITY )  {
		VSETALL( rtip->mdl_min, -1 );
	}
	if( rtip->mdl_max[X] <= -INFINITY )  {
		VSETALL( rtip->mdl_max, 1 );
	}

	/*
	 *  Enlarge the model RPP just slightly, to avoid nasty
	 *  effects with a solid's face being exactly on the edge
	 */
	rtip->mdl_min[X] = floor( rtip->mdl_min[X] );
	rtip->mdl_min[Y] = floor( rtip->mdl_min[Y] );
	rtip->mdl_min[Z] = floor( rtip->mdl_min[Z] );
	rtip->mdl_max[X] = ceil( rtip->mdl_max[X] );
	rtip->mdl_max[Y] = ceil( rtip->mdl_max[Y] );
	rtip->mdl_max[Z] = ceil( rtip->mdl_max[Z] );

	/*  Build array of region pointers indexed by reg_bit.
	 *  Optimize each region's expression tree.
	 *  Set this region's bit in the bit vector of every solid
	 *  contained in the subtree.
	 */
	rtip->Regions = (struct region **)rt_malloc(
		rtip->nregions * sizeof(struct region *),
		"rtip->Regions[]" );
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		rtip->Regions[regp->reg_bit] = regp;
		rt_optim_tree( regp->reg_treetop );
		rt_solid_bitfinder( regp->reg_treetop, regp->reg_bit );
		if(rt_g.debug&DEBUG_REGIONS)  {
			rt_pr_region( regp );
		}
	}
	if(rt_g.debug&DEBUG_REGIONS)  {
		for( stp=rtip->HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw )  {
			rt_log("solid %s ", stp->st_name);
			rt_pr_bitv( "regions ref", stp->st_regions,
				stp->st_maxreg);
		}
	}

	/* Partition space */
	rt_cut_it(rtip);
}

/*
 *			R T _ C L E A N
 *
 *  Release all the dynamic storage associated with a particular rt_i
 *  structure, except for the database directory.
 */
void
rt_clean( rtip )
register struct rt_i *rtip;
{
	register struct region *regp;
	register struct soltab *stp;

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_clean:  bad rtip\n");

	if( rtip->rti_db )  {
		rt_free( (char *)rtip->rti_db, "in-core database");
		rtip->rti_db = (union record *)0;
	}

	/*
	 *  Clear out the solid table
	 */
	for( stp=rtip->HeadSolid; stp != SOLTAB_NULL; )  {
		register struct soltab *nextstp = stp->st_forw;

		rt_free( (char *)stp->st_regions, "st_regions bitv" );
		if( stp->st_id < 0 || stp->st_id >= rt_nfunctab )
			rt_bomb("rt_clean:  bad st_id");
		rt_functab[stp->st_id].ft_free( stp );
		stp->st_name = (char *)0;	/* was ptr to directory */
		rt_free( (char *)stp, "struct soltab");
		stp = nextstp;			/* advance to next one */
	}
	rtip->HeadSolid = SOLTAB_NULL;
	rtip->nsolids = 0;

	/*  
	 *  Clear out the region table
	 */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; )  {
		register struct region *nextregp = regp->reg_forw;

		rt_fr_tree( regp->reg_treetop );
		rt_free( regp->reg_name, "region name str");
		rt_free( (char *)regp, "struct region");
		regp = nextregp;
	}
	rtip->HeadRegion = REGION_NULL;
	rtip->nregions = 0;

	/**** The best thing to do would be to hunt down the
	 *  bitv and partition structs and release them, because
	 *  they depend on the number of solids & regions!  XXX
	 */

	/* Clean out the array of pointers to regions, if any */
	if( rtip->Regions )  {
		rt_free( (char *)rtip->Regions, "rtip->Regions[]" );
		rtip->Regions = (struct region **)0;

		/* Free space partitions */
		rt_fr_cut( &(rtip->rti_CutHead) );
		bzero( (char *)&(rtip->rti_CutHead), sizeof(union cutter) );
		rt_fr_cut( &(rtip->rti_inf_box) );
		bzero( (char *)&(rtip->rti_inf_box), sizeof(union cutter) );
	}

	/* Free animation structures */
	rt_fr_anim(rtip);

	/*
	 *  Re-initialize everything important.
	 *  Some things, like file & fp remain untouched.
	 */

	rtip->rti_inf_box.bn.bn_type = CUT_BOXNODE;
	VMOVE( rtip->rti_inf_box.bn.bn_min, rtip->mdl_min );
	VMOVE( rtip->rti_inf_box.bn.bn_max, rtip->mdl_max );
	VSETALL( rtip->mdl_min, -0.1 );
	VSETALL( rtip->mdl_max,  0.1 );

	rtip->rti_magic = RTI_MAGIC;
	rtip->needprep = 1;
}
