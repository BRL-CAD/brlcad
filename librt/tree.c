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
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

int rt_pure_boolean_expressions = 0;

HIDDEN union tree *rt_drawobj();
HIDDEN void rt_add_regtree();
HIDDEN union tree *rt_mkbool_tree();
HIDDEN int rt_rpp_tree();
extern char	*rt_basename();
HIDDEN struct region *rt_getregion();
extern int	rt_id_solid();
extern void	rt_pr_soltab();

HIDDEN char *rt_path_str();

static struct mater_info rt_no_mater = {
	1.0, 1.0, 1.0,		/* color, RGB */
	0,			/* override */
	DB_INH_LOWER,		/* color inherit */
	DB_INH_LOWER		/* mater inherit */
};

/*
 * Note that while GED has a more limited MAXLEVELS, GED can
 * work on sub-trees, while RT must be able to process the full tree.
 * Thus the difference, and the large value here.
 */
#define	MAXLEVELS	64
static struct directory	*path[MAXLEVELS];	/* Record of current path */

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

	if( rtip->rti_magic != RTI_MAGIC )  {
		rt_log("rtip=x%x, rti_magic=x%x s/b x%x\n", rtip,
			rtip->rti_magic, RTI_MAGIC );
		rt_bomb("rt_gettree:  bad rtip\n");
	}

	if(!rtip->needprep)
		rt_bomb("rt_gettree called again after rt_prep!");

	root_mater = rt_no_mater;	/* struct copy */

	if( (dp = db_lookup( rtip->rti_dbip, node, LOOKUP_NOISY )) == DIR_NULL )
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
			rt_path_str(path,0) );
		if( curtree->tr_op != OP_SOLID )
			rt_bomb("root subtree not Solid");
		regionp->reg_name = rt_strdup(rt_path_str(path,0));
		rt_add_regtree( rtip, regionp, curtree );
	}
	return(0);	/* OK */
}

static vect_t xaxis = { 1.0, 0, 0 };
static vect_t yaxis = { 0, 1.0, 0 };
static vect_t zaxis = { 0, 0, 1.0 };

/*
 *			R T _ A D D _ S O L I D
 *
 *  The record pointer "rec" points to all relevant records,
 *  in a contiguous in-core array.
 */
HIDDEN
struct soltab *
rt_add_solid( rtip, rec, dp, mat )
struct rt_i	*rtip;
union record	*rec;
struct directory *dp;
matp_t		mat;
{
	register struct soltab *stp;
	static vect_t	A, B, C;
	static fastf_t	fx, fy, fz;
	FAST fastf_t	f;
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
			dp->d_namep, fx, fy, fz );
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
			dp->d_namep[0] != nsp->st_name[0]  ||	/* speed */
			dp->d_namep[1] != nsp->st_name[1]  ||	/* speed */
			strcmp( dp->d_namep, nsp->st_name ) != 0
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
				dp->d_namep );
		return(nsp);
next_one: ;
	}

	if( (id = rt_id_solid( rec )) == ID_NULL )  
		return( SOLTAB_NULL );		/* BAD */

	if( id < 0 || id >= rt_nfunctab )
		rt_bomb("rt_add_solid:  bad st_id");

	GETSTRUCT(stp, soltab);
	stp->st_id = id;
	stp->st_dp = dp;
	stp->st_name = dp->d_namep;	/* st_name could be eliminated */
	mat_copy( stp->st_pathmat, mat );
	stp->st_specific = (int *)0;

	/* init solid's maxima and minima */
	VSETALL( stp->st_max, -INFINITY );
	VSETALL( stp->st_min,  INFINITY );

	/*
	 * "rec" points to array of all relevant records, in
	 *  database format.  xxx_prep() routine is responsible for
	 *  import/export issues.
	 */
	if( rt_functab[id].ft_prep( stp, rec, rtip ) )  {
		/* Error, solid no good */
		rt_free( (char *)stp, "struct soltab");
		return( SOLTAB_NULL );		/* BAD */
	}
	id = stp->st_id;	/* type may have changed in prep */

	/* For now, just link them all onto the same list */
	stp->st_forw = rtip->HeadSolid;
	rtip->HeadSolid = stp;

	/*
	 * Update the model maxima and minima
	 *
	 *  Don't update min & max for halfspaces;  instead, add them
	 *  to the list of infinite solids, for special handling.
	 *
	 *  XXX If this solid is subtracted, don't update model RPP either.
	 */
	if( stp->st_aradius >= INFINITY )  {
		rt_cut_extend( &rtip->rti_inf_box, stp );
	}  else  {
		VMINMAX( rtip->mdl_min, rtip->mdl_max, stp->st_min );
		VMINMAX( rtip->mdl_min, rtip->mdl_max, stp->st_max );
	}

	stp->st_bit = rtip->nsolids++;
	if(rt_g.debug&DEBUG_SOLIDS)  rt_pr_soltab( stp );
	return( stp );
}

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
int		pathpos;
matp_t		old_xlate;
struct mater_info *materp;
{
	union record	*rp;
	register int	i;
	auto int	j;
	union tree	*curtree = TREE_NULL;/* cur tree top, ret. code */
	struct region	*regionp;
	int		subtreecount;	/* number of non-null subtrees */
	struct tree_list *trees = (struct tree_list *)0;/* ptr to ary of structs */
	struct tree_list *tlp;		/* cur tree_list */
	struct mater_info curmater;

	if( pathpos >= MAXLEVELS )  {
		rt_log("%s: nesting exceeds %d levels\n",
			rt_path_str(path,MAXLEVELS), MAXLEVELS );
		return(TREE_NULL);
	}
	path[pathpos] = dp;

	if( (rp = db_getmrec( rtip->rti_dbip, dp )) == (union record *)0 )
		return(TREE_NULL);
	/* Any "return" below here must release "rp" dynamic memory */

	/*
	 *  Draw a solid
	 */
	if( rp->u_id != ID_COMB )  {
		register struct soltab *stp;

		if( rt_id_solid( rp ) == ID_NULL )  {
			rt_log("rt_drawobj(%s): defective database record, type '%c' (0%o), addr=x%x\n",
				dp->d_namep,
				rp->u_id, rp->u_id, dp->d_addr );
			goto null;
		}

		if( (stp = rt_add_solid( rtip, rp, dp, old_xlate )) ==
		    SOLTAB_NULL )
			goto out;

		/**GETSTRUCT( curtree, union tree ); **/
		if( (curtree=(union tree *)rt_malloc(sizeof(union tree), "solid tree"))
		    == TREE_NULL )
			rt_bomb("rt_drawobj: solid tree malloc failed\n");
		bzero( (char *)curtree, sizeof(union tree) );
		curtree->tr_op = OP_SOLID;
		curtree->tr_a.tu_stp = stp;
		curtree->tr_a.tu_name = rt_strdup(rt_path_str(path,pathpos));
		curtree->tr_regionp = argregion;
		goto out;
	}

	/*
	 *  At this point, u_id == ID_COMB.
	 *  Process a Combination (directory) node
	 */
	if( dp->d_len <= 1 )  {
		rt_log(  "Warning: combination with zero members \"%s\".\n",
			dp->d_namep );
		goto null;
	}
	regionp = argregion;

	/*
	 *  Handle inheritance of material property.
	 *  Color and the material property have separate
	 *  inheritance interlocks.
	 */
	curmater = *materp;	/* struct copy */
	if( rp->c.c_override == 1 )  {
		if( argregion != REGION_NULL )  {
			rt_log("rt_drawobj: ERROR: color override in combination within region %s\n",
				argregion->reg_name );
		} else {
			if( curmater.ma_cinherit == DB_INH_LOWER )  {
				curmater.ma_override = 1;
				curmater.ma_color[0] = (rp->c.c_rgb[0])*rt_inv255;
				curmater.ma_color[1] = (rp->c.c_rgb[1])*rt_inv255;
				curmater.ma_color[2] = (rp->c.c_rgb[2])*rt_inv255;
				curmater.ma_cinherit = rp->c.c_inherit;
			}
		}
	}
	if( rp->c.c_matname[0] != '\0' )  {
		if( argregion != REGION_NULL )  {
			rt_log("rt_drawobj: ERROR: material property spec in combination within region %s\n",
				argregion->reg_name );
		} else {
			if( curmater.ma_minherit == DB_INH_LOWER )  {
				strncpy( curmater.ma_matname, rp->c.c_matname, sizeof(rp->c.c_matname) );
				strncpy( curmater.ma_matparm, rp->c.c_matparm, sizeof(rp->c.c_matparm) );
				curmater.ma_minherit = rp->c.c_inherit;
			}
		}
	}

	/* Handle combinations which are the top of a "region" */
	if( rp->c.c_flags == 'R' )  {
		if( argregion != REGION_NULL )  {
			rt_log("Warning:  region %s below region %s, ignored\n",
				rt_path_str(path,pathpos),
				argregion->reg_name );
		} else {
			register struct region *nrp;

			/* Ignore "air" regions unless wanted */
			if( rtip->useair == 0 &&  rp->c.c_aircode != 0 )
				goto null;

			/* Start a new region here */
			GETSTRUCT( nrp, region );
			nrp->reg_forw = REGION_NULL;
			nrp->reg_regionid = rp->c.c_regionid;
			nrp->reg_aircode = rp->c.c_aircode;
			nrp->reg_gmater = rp->c.c_material;
			nrp->reg_name = rt_strdup(rt_path_str(path,pathpos));
			nrp->reg_instnum = dp->d_uses++; /* after rt_path_str() */
			nrp->reg_mater = curmater;	/* struct copy */
			/* Material property processing in rt_add_regtree() */
			regionp = nrp;
		}
	}

	/* Process all the member records */
	j = sizeof(struct tree_list) * (dp->d_len-1);
	if( (trees = (struct tree_list *)rt_malloc( j, "tree_list array" )) ==
	    (struct tree_list *)0 )
		rt_bomb("rt_drawobj:  malloc failure\n");

	/* Process and store all the sub-trees */
	subtreecount = 0;
	tlp = trees;
	for( i = 1; i < dp->d_len; i++ )  {
		register struct member *mp;
		auto struct directory *nextdp;
		auto mat_t new_xlate;		/* Accum translation mat */
		auto struct mater_info newmater;
		mat_t xmat;		/* temp fastf_t matrix for conv */

		mp = &(rp[i].M);
		if( mp->m_id != ID_MEMB )  {
			rt_log("rt_drawobj:  defective member of %s\n", dp->d_namep);
			continue;
		}
		/* m_instname needs trimmed here! */
		if( (nextdp = db_lookup( rtip->rti_dbip, mp->m_instname,
		    LOOKUP_NOISY )) == DIR_NULL )
			continue;

		newmater = curmater;	/* struct copy -- modified below */

		/* convert matrix to fastf_t from disk format */
		rt_mat_dbmat( xmat, mp->m_mat );

		/* Check here for animation to apply */
		{
			register struct animate *anp;

			if ((nextdp->d_animate != ANIM_NULL) &&
			    (rt_g.debug & DEBUG_ANIM)) {
				rt_log("Animate %s/%s with...\n",
				    rt_path_str(path,pathpos),mp->m_instname);
			}
			for( anp = nextdp->d_animate; anp != ANIM_NULL; anp = anp->an_forw ) {
				register int i = anp->an_pathlen-2;
				/*
				 * pathlen - 1 would point to the leaf (a
				 * solid), but the solid is implicit in "path"
				 * so we need to backup "2" such that we point
				 * at the combination just above this solid.
				 */
				register int j = pathpos;

				if (rt_g.debug & DEBUG_ANIM) {
					rt_log("\t%s\t",rt_path_str(
					    anp->an_path, anp->an_pathlen-1));
				}
				for( ; i>=0 && j>=0; i--, j-- )  {
					if( anp->an_path[i] != path[j] )  {
						if (rt_g.debug & DEBUG_ANIM) {
							rt_log("%s != %s\n",
							     anp->an_path[i]->d_namep,
							     path[j]->d_namep);
						}
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
				rt_path_str(path, pathpos+1) );
			if((tlp->tl_tree)->tr_op != OP_SOLID )
				rt_bomb("subtree not Solid");
			GETSTRUCT( xrp, region );
			xrp->reg_name = rt_strdup(rt_path_str(path, pathpos+1));
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
	if( trees )  rt_free( (char *)trees, "tree_list array");
	if( rp )  rt_free( (char *)rp, "rt_drawobj records");
	return( curtree );
null:
	if( trees )  rt_free( (char *)trees, "tree_list array");
	if( rp )  rt_free( (char *)rp, "rt_drawobj records");
	return( TREE_NULL );				/* ERROR */
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
int		howfar;
struct region	*regionp;
{
	register struct tree_list *tlp;
	register int		i;
	register struct tree_list *first_tlp = (struct tree_list *)0;
	register union tree	*xtp;
	register union tree	*curtree;
	register int		inuse;

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
rt_path_str( whichpath, pos )
struct directory *whichpath[];
int pos;
{
	static char line[MAXLEVELS*(NAMESIZE+2)+10];
	register char *cp = &line[0];
	register int i;
	register struct directory	*dp;

	if( pos >= MAXLEVELS )  pos = MAXLEVELS-1;
	line[0] = '/';
	line[1] = '\0';
	for( i=0; i<=pos; i++ )  {
		(void)sprintf( cp, "/%s", (dp=whichpath[i])->d_namep );
		cp += strlen(cp);
#if 1
		/* This should be superceeded by reg_instnum soon */
		if( (dp->d_flags & DIR_REGION) || dp->d_uses > 0 )  {
			(void)sprintf( cp, "{{%d}}", dp->d_uses );
			cp += strlen(cp);
		}
#endif
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
		if( (dp = db_lookup( rtip->rti_dbip, cp, noisy )) == DIR_NULL )
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
	rt_log("instnum=%d, id=%d, air=%d, gift_material=%d, los=%d\n",
		rp->reg_instnum,
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
char *
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
register struct rt_i	*rtip;
register struct region	*regp;
register union tree	*tp;
{

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
	if( rt_g.debug & DEBUG_REGIONS )  {
		rt_log("Add Region %s instnum %d\n",
			regp->reg_name, regp->reg_instnum);
	}
}

/*
 *			R T _ O P T I M _ T R E E
 */
HIDDEN void
rt_optim_tree( tp, resp )
register union tree	*tp;
struct resource		*resp;
{
	register union tree	**sp;
	register union tree	*low;
	register union tree	**stackend;

	while( (sp = resp->re_boolstack) == (union tree **)0 )
		rt_grow_boolstack( resp );
	stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
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
			if( sp >= stackend )  {
				register int off = sp - resp->re_boolstack;
				rt_grow_boolstack( resp );
				sp = &(resp->re_boolstack[off]);
				stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
			}
			break;
		case OP_UNION:
		case OP_INTERSECT:
		case OP_XOR:
			/* Need to look at 3-level optimizations here, both sides */
			/* push both nodes - search left first */
			*sp++ = tp->tr_b.tb_right;
			*sp++ = tp->tr_b.tb_left;
			if( sp >= stackend )  {
				register int off = sp - resp->re_boolstack;
				rt_grow_boolstack( resp );
				sp = &(resp->re_boolstack[off]);
				stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
			}
			break;
		default:
			rt_log("rt_optim_tree: bad op x%x\n", tp->tr_op);
			break;
		}
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
rt_solid_bitfinder( treep, regbit, resp )
register union tree	*treep;
register int		regbit;
struct resource		*resp;
{
	register union tree	**sp;
	register struct soltab	*stp;
	register union tree	**stackend;

	while( (sp = resp->re_boolstack) == (union tree **)0 )
		rt_grow_boolstack( resp );
	stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
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
			if( sp >= stackend )  {
				register int off = sp - resp->re_boolstack;
				rt_grow_boolstack( resp );
				sp = &(resp->re_boolstack[off]);
				stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
			}
			break;
		default:
			rt_log("rt_solid_bitfinder:  op=x%x\n", treep->tr_op);
			break;
		}
	}
}

/*
 *			R T _ P R _ S O L T A B
 */
void
rt_pr_soltab( stp )
register struct soltab	*stp;
{
	register int	id = stp->st_id;

	if( id <= 0 || id > ID_MAXIMUM )  {
		rt_log("stp=x%x, id=%d.\n", stp, id);
		rt_bomb("rt_pr_soltab:  bad st_id");
	}
	rt_log("------------ %s (bit %d) %s ------------\n",
		stp->st_name, stp->st_bit,
		rt_functab[id].ft_name );
	VPRINT("Bound Sph CENTER", stp->st_center);
	rt_log("Approx Sph Radius = %g\n", stp->st_aradius);
	rt_log("Bounding Sph Radius = %g\n", stp->st_bradius);
	VPRINT("Bound RPP min", stp->st_min);
	VPRINT("Bound RPP max", stp->st_max);
	rt_pr_bitv( "Referenced by Regions",
		stp->st_regions, stp->st_maxreg );
	rt_functab[id].ft_print( stp );
}
