/*
 *			D B _ T R E E . C
 *
 * Functions -
 *	db_functree	No-frills tree-walk
 *
 *
 *  Authors -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
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
#include "raytrace.h"
#include "db.h"

#include "./debug.h"

struct full_path {
	int		fp_len;
	int		fp_maxlen;
	struct directory **fp_names;	/* array of dir pointers */
};

struct tree_list {
	union tree *tl_tree;
	int	tl_op;
};
#define TREE_LIST_NULL	((struct tree_list *)0)

struct tree_state {
	struct db_i	*ts_dbip;
	int		ts_sofar;		/* Flag bits */

	int		ts_regionid;	/* GIFT compat region ID code*/
	int		ts_aircode;	/* GIFT compat air code */
	int		ts_gmater;	/* GIFT compat material code */
	struct mater_info ts_mater;	/* material properties */

	mat_t		ts_mat;		/* transform matrix */

	int		ts_stop_at_regions;	/* else stop at solids */
	int		(*ts_region_start_func)();
	union tree *	(*ts_region_end_func)();
	union tree *	(*ts_leaf_func)();

/**	vect_t		ts_min;		/* RPP minimum */
/**	vect_t		ts_max;		/* RPP maximum */
};
#define TS_SOFAR_MINUS	1		/* Subtraction encountered above */
#define TS_SOFAR_INTER	2		/* Intersection encountered above */
#define TS_SOFAR_REGION	4		/* Region encountered above */

struct combined_tree_state {
	struct tree_state	cts_s;
	struct full_path	cts_p;
};

/*
 *  rt_ or db_ ?
 */
void
rt_add_node_to_full_path( pp, dp )
struct full_path	*pp;
struct directory	*dp;
{
	if( pp->fp_maxlen <= 0 )  {
		pp->fp_maxlen = 32;
		pp->fp_names = (struct directory **)rt_malloc(
			pp->fp_maxlen * sizeof(struct directory *),
			"initial full path array");
	} else if( pp->fp_len >= pp->fp_maxlen )  {
		pp->fp_maxlen *= 4;
		pp->fp_names = (struct directory **)rt_realloc(
			(char *)pp->fp_names,
			pp->fp_maxlen * sizeof(struct directory *),
			"enlarged full path array");
	}
	pp->fp_names[pp->fp_len++] = dp;
}

void
rt_dup_full_path( newp, oldp )
register struct full_path	*newp;
register struct full_path	*oldp;
{
	newp->fp_maxlen = oldp->fp_maxlen;
	if( (newp->fp_len = oldp->fp_len) <= 0 )  {
		newp->fp_names = (struct directory **)0;
		return;
	}
	newp->fp_names = (struct directory **)rt_malloc(
		newp->fp_maxlen * sizeof(struct directory *),
		"duplicate full path array" );
	bcopy( (char *)oldp->fp_names, (char *)newp->fp_names,
		newp->fp_len * sizeof(struct directory *) );
}

/*
 *  Unlike rt_path_str(), this version can be used in parallel.
 *  Caller is responsible for freeing the returned buffer.
 */
char *
rt_path_to_string( pp )
struct full_path	*pp;
{
	int	len;
	char	*buf;
	char	*cp;
	int	i;

	len = 2;	/* leading slash, trailing null */
	for( i=pp->fp_len-1; i >= 0; i-- )  {
		if( pp->fp_names[i] )
			len += strlen( pp->fp_names[i]->d_namep ) + 1;
		else
			len += 16;
	}

	buf = rt_malloc( len, "pathname string" );
	cp = buf;

	for( i=0; i < pp->fp_len; i++ )  {
		*cp++ = '/';
		if( pp->fp_names[i] )
			strcpy( cp, pp->fp_names[i]->d_namep );
		else
			strcpy( cp, "**NULL**" );
		cp += strlen( cp );
	}
	return( buf );
}

/*
 *			D B _ A P P L Y _ S T A T E _ F R O M _ C O M B
 *
 *  Handle inheritance of material property found in combination record.
 *  Color and the material property have separate inheritance interlocks.
 *
 *  Returns -
 *	-1	failure
 *	 0	success
 *	 1	success, this is the top of a new region.
 */
int
db_apply_state_from_comb( tsp, pathp, rp )
struct tree_state	*tsp;
struct full_path	*pathp;
union record		*rp;
{
	if( rp->u_id != ID_COMB )  {
		char	*sofar = rt_path_to_string(pathp);
		rt_log("db_apply_state_from_comb() defective record at '%s'\n",
			sofar );
		rt_free(sofar, "path string");
		return(-1);
	}

	if( rp->c.c_override == 1 )  {
		if( tsp->ts_sofar & TS_SOFAR_REGION )  {
			/* This combination is within a region */
			char	*sofar = rt_path_to_string(pathp);

			rt_log("db_apply_state_from_comb(): WARNING: color override in combination within region '%s', ignored\n",
				sofar );
			rt_free(sofar, "path string");
		} else if( tsp->ts_mater.ma_cinherit == DB_INH_LOWER )  {
			tsp->ts_mater.ma_override = 1;
			tsp->ts_mater.ma_color[0] = (rp->c.c_rgb[0])*rt_inv255;
			tsp->ts_mater.ma_color[1] = (rp->c.c_rgb[1])*rt_inv255;
			tsp->ts_mater.ma_color[2] = (rp->c.c_rgb[2])*rt_inv255;
			tsp->ts_mater.ma_cinherit = rp->c.c_inherit;
		}
	}
	if( rp->c.c_matname[0] != '\0' )  {
		if( tsp->ts_sofar & TS_SOFAR_REGION )  {
			/* This combination is within a region */
			char	*sofar = rt_path_to_string(pathp);

			rt_log("db_apply_state_from_comb(): WARNING: material property spec in combination within region '%s', ignored\n",
				sofar );
			rt_free(sofar, "path string");
		} else if( tsp->ts_mater.ma_minherit == DB_INH_LOWER )  {
			strncpy( tsp->ts_mater.ma_matname, rp->c.c_matname, sizeof(rp->c.c_matname) );
			strncpy( tsp->ts_mater.ma_matparm, rp->c.c_matparm, sizeof(rp->c.c_matparm) );
			tsp->ts_mater.ma_minherit = rp->c.c_inherit;
		}
	}

	/* Handle combinations which are the top of a "region" */
	if( rp->c.c_flags == 'R' )  {
		if( tsp->ts_sofar & TS_SOFAR_REGION )  {
			if( (tsp->ts_sofar&(TS_SOFAR_MINUS|TS_SOFAR_INTER)) == 0 )  {
				char	*sofar = rt_path_to_string(pathp);
				rt_log("Warning:  region unioned into region at '%s', lower region info ignored\n",
					sofar);
				rt_free(sofar, "path string");
			}
			/* Go on as if it was not a region */
		} else {
			/* This starts a new region */
			tsp->ts_sofar |= TS_SOFAR_REGION;
			tsp->ts_regionid = rp->c.c_regionid;
			tsp->ts_aircode = rp->c.c_aircode;
			tsp->ts_gmater = rp->c.c_material;
			/* XXX region name, instnum too? */
			return(1);	/* Success, this starts new region */
		}
	}
	return(0);	/* Success */
}

/*
 *			D B _ A P P L Y _ S T A T E _ F R O M _ M E M B
 *
 *  Updates state via *tsp, pushes member's directory entry on *pathp.
 *  (Caller is responsible for popping it).
 *
 *  Returns -
 *	-1	failure
 *	 0	success, member pushed on path
 */
int
db_apply_state_from_memb( tsp, pathp, mp )
struct tree_state	*tsp;
struct full_path	*pathp;
struct member		*mp;
{
	register struct directory *mdp;
	mat_t			xmat;
	mat_t			old_xlate;
	register struct animate *anp;
	char			namebuf[NAMESIZE+2];

	if( mp->m_id != ID_MEMB )  {
		char	*sofar = rt_path_to_string(pathp);
		rt_log("db_follow_path_for_state:  defective member rec in '%s'\n", sofar);
		rt_free(sofar, "path string");
		return(-1);
	}

	/* Trim m_instname */
	strncpy( namebuf, mp->m_instname, NAMESIZE );
	namebuf[NAMESIZE] = '\0';
	if( (mdp = db_lookup( tsp->ts_dbip, namebuf, LOOKUP_NOISY )) == DIR_NULL )
		return(-1);

	rt_add_node_to_full_path( pathp, mdp );

	mat_copy( old_xlate, tsp->ts_mat );

	/* convert matrix to fastf_t from disk format */
	rt_mat_dbmat( xmat, mp->m_mat );

	/* Check here for animation to apply */
	if ((mdp->d_animate != ANIM_NULL) && (rt_g.debug & DEBUG_ANIM)) {
		char	*sofar = rt_path_to_string(pathp);
		rt_log("Animate %s/%s with...\n", sofar, mp->m_instname);
		rt_free(sofar, "path string");
	}
	for( anp = mdp->d_animate; anp != ANIM_NULL; anp = anp->an_forw ) {
		register int i = anp->an_pathlen-2;
		/*
		 * pathlen - 1 would point to the leaf (a
		 * solid), but the solid is implicit in "path"
		 * so we need to backup "2" such that we point
		 * at the combination just above this solid.
		 */
		register int j = pathp->fp_len;

		if (rt_g.debug & DEBUG_ANIM) {
			rt_log("\t%s\t",rt_path_str(
			    anp->an_path, anp->an_pathlen-1));
		}
		for( ; i>=0 && j>=0; i--, j-- )  {
			if( anp->an_path[i] != pathp->fp_names[j] )  {
				if (rt_g.debug & DEBUG_ANIM) {
					rt_log("%s != %s\n",
					     anp->an_path[i]->d_namep,
					     pathp->fp_names[j]->d_namep);
				}
				goto next_one;
			}
		}
		/* Perhaps tsp->ts_mater should be just tsp someday? */
		rt_do_anim( anp, old_xlate, xmat, &(tsp->ts_mater) );
next_one:	;
	}

	mat_mul(tsp->ts_mat, old_xlate, xmat);
	return(0);		/* Success */
}

/*
 *			D B _ F O L L O W _ P A T H _ F O R _ S T A T E
 *
 *  Follow the slash-separated path given by "cp", and update
 *  *tsp and *pathp with full state information along the way.
 *
 *  A much more complete version of rt_plookup().
 *
 *  Returns -
 *	 0	success (plus *tsp is updated)
 *	-1	error (*tsp values are not useful)
 */
int
db_follow_path_for_state( tsp, pathp, orig_str, noisy )
struct tree_state	*tsp;
struct full_path	*pathp;
char			*orig_str;
int			noisy;
{
	register union record	*rp = (union record *)0;
	register int		i;
	register char		*cp;
	register char		*ep;
	char			*str;		/* ptr to duplicate string */
	char			oldc;
	register struct member *mp;
	struct directory	*comb_dp;	/* combination's dp */
	struct directory	*dp;		/* element's dp */

	if( tsp->ts_dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_follow_path_for_state:  bad dbip\n");
	if(rt_g.debug&DEBUG_DB)  {
		char	*sofar = rt_path_to_string(pathp);
		rt_log("db_follow_path_for_state() pathp='%s', tsp=x%x, orig_str='%s', noisy=%d\n",
			sofar, tsp, orig_str, noisy );
		rt_free(sofar, "path string");
	}

	if( *orig_str == '\0' )  return(0);		/* Null string */

	cp = str = rt_strdup( orig_str );

	/*  Handle each path element */
	if( pathp->fp_len > 0 )
		comb_dp = pathp->fp_names[pathp->fp_len-1];
	else
		comb_dp = DIR_NULL;
	do  {
		/* Skip any leading slashes */
		while( *cp && *cp == '/' )  cp++;

		/* Find end of this path element and null terminate */
		ep = cp;
		while( *ep != '\0' && *ep != '/' )  ep++;
		oldc = *ep;
		*ep = '\0';

		if( (dp = db_lookup( tsp->ts_dbip, cp, noisy )) == DIR_NULL )
			goto fail;

		/* If first element, push it, and go on */
		if( pathp->fp_len <= 0 )  {
			rt_add_node_to_full_path( pathp, dp );

#if 0
			/* XXX should these be dbip->db_anroot ?? */
			/* XXX rt_do_anim should perhaps be db_do_anim? */
			/* Process animations located at the root */
			if( rtip->rti_anroot )  {
				register struct animate *anp;
				mat_t	old_xlate, xmat;

				for( anp=rtip->rti_anroot; anp != ANIM_NULL; anp = anp->an_forw ) {
					if( dp != anp->an_path[0] )
						continue;
					mat_copy( old_xlate, tsp->ts_mat );
					mat_idn( xmat );
					rt_do_anim( anp, old_xlate, xmat, &(tsp->ts_mater) );
					mat_mul( tsp->ts_mat, old_xlate, xmat );
				}
			}
#endif

			/* Advance to next path element */
			cp = ep+1;
			comb_dp = dp;
			continue;
		}

		if( (dp->d_flags & DIR_COMB) == 0 )  {
			/* Object is a leaf */
			rt_add_node_to_full_path( pathp, dp );
			if( oldc == '\0' )  {
				/* No more path was given, all is well */
				goto out;
			}
			/* Additional path was given, this is wrong */
			if( noisy )  {
				char	*sofar = rt_path_to_string(pathp);
				rt_log("db_follow_path_for_state(%s) ERROR: found leaf early at '%s'\n",
					cp, sofar );
				rt_free(sofar, "path string");
			}
			goto fail;
		}

		/* Object is a combination */
		if( dp->d_len <= 1 )  {
			/* Combination has no members */
			if( noisy )  {
				rt_log("db_follow_path_for_state(%s) ERROR: combination '%s' has no members\n",
					cp, dp->d_namep );
			}
			goto fail;
		}

		/* Load the entire combination into contiguous memory */
		if( (rp = db_getmrec( tsp->ts_dbip, comb_dp )) == (union record *)0 )
			goto fail;

		/* Apply state changes from new combination */
		if( db_apply_state_from_comb( tsp, pathp, rp ) < 0 )
			goto fail;

		for( i=1; i < dp->d_len; i++ )  {
			mp = &(rp[i].M);

			/* If this is not the desired element, skip it */
			if( strncmp( mp->m_instname, cp, sizeof(mp->m_instname)) == 0 )
				goto found_it;
		}
		if(noisy) rt_log("db_follow_path_for_state() ERROR: unable to find element '%s'\n", cp );
		goto fail;
found_it:
		if( db_apply_state_from_memb( tsp, pathp, mp ) < 0 )
			goto fail;
		/* directory entry was pushed */

		/* Take note of operation */
		switch( mp->m_relation )  {
		default:
			break;		/* handle as union */
		case UNION:
			break;
		case SUBTRACT:
			tsp->ts_sofar |= TS_SOFAR_MINUS;
			break;
		case INTERSECT:
			tsp->ts_sofar |= TS_SOFAR_INTER;
			break;
		}

		/* Free record */
		rt_free( (char *)rp, comb_dp->d_namep );
		rp = (union record *)0;

		/* Advance to next path element */
		cp = ep+1;
		comb_dp = dp;
	} while( oldc != '\0' );

out:
	if( rp )  rt_free( (char *)rp, dp->d_namep );
	rt_free( str, "dupped path" );
	if(rt_g.debug&DEBUG_DB)  {
		char	*sofar = rt_path_to_string(pathp);
		rt_log("db_follow_path_for_state() returns pathp='%s'\n",
			sofar);
		rt_free(sofar, "path string");
	}
	return(0);		/* SUCCESS */
fail:
	if( rp )  rt_free( (char *)rp, dp->d_namep );
	rt_free( str, "dupped path" );
	return(-1);		/* FAIL */
}

extern union tree *rt_mkbool_tree();

/*
 *			R T _ M K G I F T _ T R E E
 */
union tree *
rt_mkgift_tree( trees, subtreecount, tsp )
struct tree_list	*trees;
int			subtreecount;
struct tree_state	*tsp;
{
	extern int	rt_pure_boolean_expressions;
	register struct tree_list *tstart;
	register struct tree_list *tnext;
	union tree		*curtree;
	int	i;
	int	j;
	struct region	*regionp = 0;	/* XXX */

	/* Build tree representing boolean expression in Member records */
	if( rt_pure_boolean_expressions )  {
		curtree = rt_mkbool_tree( trees, subtreecount, regionp );
		if(rt_g.debug&DEBUG_REGIONS) rt_pr_tree(curtree, 0);
		return( curtree );
	}

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
	tnext = trees+1;
	for( i=subtreecount-1; i>=0; i--, tnext++ )  {
		/* If we went off end, or hit a union, do it */
		if( i>0 && tnext->tl_op != OP_UNION )
			continue;
		if( (j = tnext-tstart) <= 0 )
			continue;
		curtree = rt_mkbool_tree( tstart, j, regionp );
		/* rt_mkbool_tree() has side effect of zapping tree array,
		 * so build new first node in array.
		 */
		tstart->tl_op = OP_UNION;
		tstart->tl_tree = curtree;

		if(rt_g.debug&DEBUG_REGIONS) rt_pr_tree(tstart->tl_tree, 0);

		/* tstart here at union */
		tstart = tnext;
	}

	curtree = rt_mkbool_tree( trees, subtreecount, regionp );
	if(rt_g.debug&DEBUG_REGIONS) rt_pr_tree(curtree, 0);
	return( curtree );
}

static vect_t xaxis = { 1.0, 0, 0 };
static vect_t yaxis = { 0, 1.0, 0 };
static vect_t zaxis = { 0, 0, 1.0 };

/*
 *			D B _ R E C U R S E
 */
union tree *
db_recurse( tsp, pathp )
struct tree_state	*tsp;
struct full_path	*pathp;
{
	struct directory	*dp;
	register union record	*rp = (union record *)0;
	register int		i;
	struct tree_list	*tlp;		/* cur elem of trees[] */
	struct tree_list	*trees = TREE_LIST_NULL;	/* array */
	union tree		*curtree;

	if( tsp->ts_dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_recurse:  bad dbip\n");
	if( pathp->fp_len <= 0 )  {
		rt_log("db_recurse() null path?\n");
		return(TREE_NULL);
	}
	dp = pathp->fp_names[pathp->fp_len-1];
	if(rt_g.debug&DEBUG_DB)  {
		char	*sofar = rt_path_to_string(pathp);
		rt_log("db_recurse() pathp='%s', tsp=x%x\n",
			sofar, tsp );
		rt_free(sofar, "path string");
	}

	/*
	 * Load the entire object into contiguous memory.
	 */
	if( (rp = db_getmrec( tsp->ts_dbip, dp )) == (union record *)0 )
		return(TREE_NULL);		/* FAIL */

	if( dp->d_flags & DIR_COMB )  {
		struct tree_state	nts;
		int			is_region;

		if( dp->d_len <= 1 )  {
			rt_log("Warning: combination with zero members \"%s\".\n",
				dp->d_namep );
			goto fail;
		}

		/*  Handle inheritance of material property. */
		nts = *tsp;	/* struct copy */

		if( (is_region = db_apply_state_from_comb( &nts, pathp, rp )) < 0 )
			goto fail;

		if( is_region > 0 )  {
			/*
			 *  This is the start of a new region.
			 *  If handler rejects this region, skip on.
			 *  This might be used for ignoring air regions.
			 */
			if( tsp->ts_region_start_func && 
			    tsp->ts_region_start_func( &nts, pathp ) < 0 )
				goto fail;

			if( tsp->ts_stop_at_regions )  {
				curtree = (union tree *)0;
				goto region_end;
			}
		}

		tlp = trees = (struct tree_list *)rt_malloc(
			sizeof(struct tree_list) * (dp->d_len-1),
			"tree_list array" );

		for( i=1; i < dp->d_len; i++ )  {
			register struct member *mp;
			struct tree_state	memb_state;

			memb_state = nts;	/* struct copy */

			mp = &(rp[i].M);

			if( db_apply_state_from_memb( &memb_state, pathp, mp ) < 0 )
				continue;
			/* Member was pushed on pathp stack */

			/* Note & store operation on subtree */
			switch( mp->m_relation )  {
			default:
				rt_log("%s: bad m_relation '%c'\n",
					dp->d_namep, mp->m_relation );
				tlp->tl_op = OP_UNION;
				break;
			case UNION:
				tlp->tl_op = OP_UNION;
				break;
			case SUBTRACT:
				tlp->tl_op = OP_SUBTRACT;
				memb_state.ts_sofar |= TS_SOFAR_MINUS;
				break;
			case INTERSECT:
				tlp->tl_op = OP_INTERSECT;
				memb_state.ts_sofar |= TS_SOFAR_INTER;
				break;
			}

			/* Recursive call */
			if( (tlp->tl_tree = db_recurse( &memb_state, pathp )) != TREE_NULL )  {
				tlp++;
			}

			pathp->fp_len--;	/* pop member */
		}
		if( tlp <= trees )  {
			/* No subtrees */
			goto fail;
		}

		curtree = rt_mkgift_tree( trees, tlp-trees, tsp );

region_end:
		if( is_region > 0 )  {
			/*
			 *  This is the end of processing for a region.
			 */
			if( tsp->ts_region_end_func )
				curtree = tsp->ts_region_end_func(
					&nts, pathp, curtree );
		}
	} else if( dp->d_flags & DIR_SOLID )  {
		int	id;
		vect_t	A, B, C;
		fastf_t	fx, fy, fz;

		/* Get solid ID */
		if( (id = rt_id_solid( rp )) == ID_NULL )  {
			rt_log("db_functree(%s): defective database record, type '%c' (0%o), addr=x%x\n",
				dp->d_namep,
				rp->u_id, rp->u_id, dp->d_addr );
			goto fail;
		}

		/*
		 * Validate that matrix preserves perpendicularity of axis
		 * by checking that A.B == 0, B.C == 0, A.C == 0
		 * XXX these vectors should just be grabbed out of the matrix
		 */
		MAT4X3VEC( A, tsp->ts_mat, xaxis );
		MAT4X3VEC( B, tsp->ts_mat, yaxis );
		MAT4X3VEC( C, tsp->ts_mat, zaxis );
		fx = VDOT( A, B );
		fy = VDOT( B, C );
		fz = VDOT( A, C );
		if( ! NEAR_ZERO(fx, 0.0001) ||
		    ! NEAR_ZERO(fy, 0.0001) ||
		    ! NEAR_ZERO(fz, 0.0001) )  {
			rt_log("db_functree(%s):  matrix does not preserve axis perpendicularity.\n  X.Y=%g, Y.Z=%g, X.Z=%g\n",
				dp->d_namep, fx, fy, fz );
			mat_print("bad matrix", tsp->ts_mat);
			goto fail;
		}

		/* Note:  solid may not be contained by a region */

		if( !tsp->ts_leaf_func )  goto fail;
		curtree = tsp->ts_leaf_func( tsp, pathp, rp, id );
		/* eg, rt_add_solid() */
	} else {
		rt_log("db_functree:  %s is neither COMB nor SOLID?\n",
			dp->d_namep );
		curtree = (union tree *)0;
	}
out:
	if( rp )  rt_free( (char *)rp, dp->d_namep );
	if(rt_g.debug&DEBUG_DB)  {
		char	*sofar = rt_path_to_string(pathp);
		rt_log("db_recurse() return curtree=x%x, pathp='%s'\n",
			curtree, sofar);
		rt_free(sofar, "path string");
	}
	return(curtree);		/* SUCCESS */
fail:
	if( rp )  rt_free( (char *)rp, dp->d_namep );
	if(rt_g.debug&DEBUG_DB)  {
		char	*sofar = rt_path_to_string(pathp);
		rt_log("db_recurse() return curtree=NULL, pathp='%s'\n",
			sofar);
		rt_free(sofar, "path string");
	}
	return( (union tree *)0 );	/* FAIL */
}

static struct tree_state	rt_initial_tree_state = {
	0,			/* ts_dbip */
	0,			/* ts_sofar */
	0, 0, 0,		/* region, air, gmater */
	1.0, 1.0, 1.0,		/* color, RGB */
	0,			/* override */
	DB_INH_LOWER,		/* color inherit */
	DB_INH_LOWER,		/* mater inherit */
	"",			/* material name */
	"",			/* material params */
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0,
};

HIDDEN int rt_gettree_p1_region_start( tsp, pathp )
struct tree_state	*tsp;
struct full_path	*pathp;
{

#if 0
	/* Ignore "air" regions unless wanted */
	if( rtip->useair == 0 &&  tsp->ts_aircode != 0 )  {
		rtip->rti_air_discards++;
		return(-1);	/* drop this region */
	}
#endif
	return(0);
}

HIDDEN union tree *rt_gettree_p1_region_end( tsp, pathp, curtree )
register struct tree_state	*tsp;
struct full_path	*pathp;
union tree		*curtree;
{
	register struct combined_tree_state	*cts;

	cts=(struct combined_tree_state *)rt_malloc(
		sizeof(struct combined_tree_state), "combined region state");
	cts->cts_s = *tsp;	/* struct copy */
	rt_dup_full_path( &(cts->cts_p), pathp );

	curtree=(union tree *)rt_malloc(sizeof(union tree), "solid tree");
	bzero( (char *)curtree, sizeof(union tree) );
	curtree->tr_op = OP_SOLID;		/* XXX OP_REGION? */
	curtree->tr_a.tu_stp = (struct soltab *)cts;
	curtree->tr_a.tu_name = (char *)0;
	curtree->tr_regionp = (struct region *)0;

	return(curtree);
}

HIDDEN union tree *rt_gettree_p1_leaf( tsp, pathp, rp, id )
struct tree_state	*tsp;
struct full_path	*pathp;
union record		*rp;
int			id;
{
	register struct combined_tree_state	*cts;
	union tree	*curtree;

	cts=(struct combined_tree_state *)rt_malloc(
		sizeof(struct combined_tree_state), "combined region state");
	cts->cts_s = *tsp;	/* struct copy */
	rt_dup_full_path( &(cts->cts_p), pathp );

	curtree=(union tree *)rt_malloc(sizeof(union tree), "solid tree");
	bzero( (char *)curtree, sizeof(union tree) );
	curtree->tr_op = OP_SOLID;		/* XXX OP_REGION? */
	curtree->tr_a.tu_stp = (struct soltab *)cts;
	curtree->tr_a.tu_name = (char *)0;
	curtree->tr_regionp = (struct region *)0;

	return(curtree);
}

/*
 * XXX  NEW NEW NEW
 *  			R T _ G E T _ T R E E
 *
 *  User-called function to add a tree hierarchy to the displayed set.
 *  
 *  Returns -
 *  	0	Ordinarily
 *	-1	On major error
 */
int
NEW_rt_gettree( rtip, node)
struct rt_i	*rtip;
char		*node;
{
	register union tree	*curtree;
	struct tree_state	ts;
	struct full_path	path;
	int			prev_sol_count;

	RT_CHECK_RTI(rtip);

	if(!rtip->needprep)
		rt_bomb("rt_gettree called again after rt_prep!");

	ts = rt_initial_tree_state;	/* struct copy */
	ts.ts_dbip = rtip->rti_dbip;

	path.fp_len = path.fp_maxlen = 0;

	prev_sol_count = rtip->nsolids;

	/* First, establish context from given path */
	if( db_follow_path_for_state( &ts, &path, node, LOOKUP_NOISY ) < 0 )
		return(-1);		/* ERROR */

	/*
	 *  Second, walk tree from root to start of all regions.
	 *  Build a boolean tree of all regions.
	 */
	ts.ts_stop_at_regions = 1;
	ts.ts_region_start_func = rt_gettree_p1_region_start;
	ts.ts_region_end_func = rt_gettree_p1_region_end;
	ts.ts_leaf_func = rt_gettree_p1_leaf;
	curtree = db_recurse( &ts, &path );
	if( curtree == (union tree *)0 )  return(-1);

	rt_pr_tree( curtree, 0 );

	/*
	 *  Third, push all non-union booleans down
	 */

	/*
	 *  Fourth, in parallel, for each region, walk the tree to the leaves.
	 */

	if( rtip->nsolids <= prev_sol_count )
		rt_log("rt_gettree(%s) warning:  no solids found\n", node);
	return(0);	/* OK */

	/* XXX need to free path storage */
}
