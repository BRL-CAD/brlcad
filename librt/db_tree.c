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

/* XXX for raytrace.h */
#define OP_REGION	MKOP(9)		/* Leaf: tr_stp -> combined_tree_state */
#define OP_NOP		MKOP(10)	/* Leaf with no effect */

struct full_path {
	int		fp_len;
	int		fp_maxlen;
	struct directory **fp_names;	/* array of dir pointers */
};
#define DB_FULL_PATH_POP(_pp)	{(_pp)->fp_len--;}
#define DB_FULL_PATH_CUR_DIR(_pp)	((_pp)->fp_names[(_pp)->fp_len-1])

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
 *			D B _ A D D _ N O D E _ T O _ F U L L _ P A T H
 */
void
db_add_node_to_full_path( pp, dp )
struct full_path	*pp;
struct directory	*dp;
{
	if( pp->fp_maxlen <= 0 )  {
		pp->fp_maxlen = 32;
		pp->fp_names = (struct directory **)rt_malloc(
			pp->fp_maxlen * sizeof(struct directory *),
			"full_path array");
	} else if( pp->fp_len >= pp->fp_maxlen )  {
		pp->fp_maxlen *= 4;
		pp->fp_names = (struct directory **)rt_realloc(
			(char *)pp->fp_names,
			pp->fp_maxlen * sizeof(struct directory *),
			"enlarged full_path array");
	}
	pp->fp_names[pp->fp_len++] = dp;
}

/*
 *			D B _ D U P _ F U L L _ P A T H
 */
void
db_dup_full_path( newp, oldp )
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
 *			D B _ P A T H _ T O _ S T R I N G
 *
 *  Unlike rt_path_str(), this version can be used in parallel.
 *  Caller is responsible for freeing the returned buffer.
 */
char *
db_path_to_string( pp )
struct full_path	*pp;
{
	int	len;
	char	*buf;
	char	*cp;
	int	i;

	len = 3;	/* leading slash, trailing null, spare */
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
	*cp++ = '\0';
	return( buf );
}

/*
 *			D B _ F R E E _ F U L L _ P A T H
 */
void
db_free_full_path( pp )
struct full_path	*pp;
{
	if( pp->fp_maxlen > 0 )  {
		rt_free( (char *)pp->fp_names, "full_path array" );
		pp->fp_maxlen = pp->fp_len = 0;
		pp->fp_names = (struct directory **)0;
	}
}

/*
 *			D B _ F R E E _ C O M B I N E D _ T R E E _ S T A T E
 */
void
db_free_combined_tree_state( ctsp )
register struct combined_tree_state	*ctsp;
{
	db_free_full_path( &(ctsp->cts_p) );
	rt_free( (char *)ctsp, "combined_tree_state");
}

void
db_pr_tree_state( tsp )
register struct tree_state	*tsp;
{
	rt_log("db_pr_tree_state(x%x):\n", tsp);
	rt_log(" ts_dbip=x%x\n", tsp->ts_dbip);
	rt_printb(" ts_sofar", tsp->ts_sofar, "\020\3REGION\2INTER\1MINUS" );
	rt_log("\n");
	rt_log(" ts_regionid=%d\n", tsp->ts_regionid);
	rt_log(" ts_aircode=%d\n", tsp->ts_aircode);
	rt_log(" ts_gmater=%d\n", tsp->ts_gmater);
	rt_log(" ts_mater.ma_color=%g,%g,%g\n",
		tsp->ts_mater.ma_color[0],
		tsp->ts_mater.ma_color[1],
		tsp->ts_mater.ma_color[2] );
	rt_log(" ts_mater.ma_matname=%s\n", tsp->ts_mater.ma_matname );
	rt_log(" ts_mater.ma_matparam=%s\n", tsp->ts_mater.ma_matparm );
}

void
db_pr_combined_tree_state( ctsp )
register struct combined_tree_state	*ctsp;
{
	char	*str;

	rt_log("db_pr_combined_tree_state(x%x):\n", ctsp);
	db_pr_tree_state( &(ctsp->cts_s) );
	str = db_path_to_string( &(ctsp->cts_p) );
	rt_log(" path='%s'\n", str);
	rt_free( str, "path string" );
}

/*
 *			R T _ T R E E _ R E G I O N _ A S S I G N
 */
void
rt_tree_region_assign( tp, regionp )
register union tree	*tp;
register struct region	*regionp;
{
	switch( tp->tr_op )  {
	case OP_SOLID:
		tp->tr_a.tu_regionp = regionp;
		return;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		tp->tr_b.tb_regionp = regionp;
		rt_tree_region_assign( tp->tr_b.tb_left, regionp );
		return;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		tp->tr_b.tb_regionp = regionp;
		rt_tree_region_assign( tp->tr_b.tb_left, regionp );
		rt_tree_region_assign( tp->tr_b.tb_right, regionp );
		return;

	default:
		rt_bomb("rt_tree_region_assign: bad op\n");
	}
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
		char	*sofar = db_path_to_string(pathp);
		rt_log("db_apply_state_from_comb() defective record at '%s'\n",
			sofar );
		rt_free(sofar, "path string");
		return(-1);
	}

	if( rp->c.c_override == 1 )  {
		if( tsp->ts_sofar & TS_SOFAR_REGION )  {
			/* This combination is within a region */
			char	*sofar = db_path_to_string(pathp);

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
			char	*sofar = db_path_to_string(pathp);

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
				char	*sofar = db_path_to_string(pathp);
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
		char	*sofar = db_path_to_string(pathp);
		rt_log("db_follow_path_for_state:  defective member rec in '%s'\n", sofar);
		rt_free(sofar, "path string");
		return(-1);
	}

	/* Trim m_instname */
	strncpy( namebuf, mp->m_instname, NAMESIZE );
	namebuf[NAMESIZE] = '\0';
	if( (mdp = db_lookup( tsp->ts_dbip, namebuf, LOOKUP_NOISY )) == DIR_NULL )
		return(-1);

	db_add_node_to_full_path( pathp, mdp );

	mat_copy( old_xlate, tsp->ts_mat );

	/* convert matrix to fastf_t from disk format */
	rt_mat_dbmat( xmat, mp->m_mat );

	/* Check here for animation to apply */
	if ((mdp->d_animate != ANIM_NULL) && (rt_g.debug & DEBUG_ANIM)) {
		char	*sofar = db_path_to_string(pathp);
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
	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		rt_log("db_follow_path_for_state() pathp='%s', tsp=x%x, orig_str='%s', noisy=%d\n",
			sofar, tsp, orig_str, noisy );
		rt_free(sofar, "path string");
	}

	if( *orig_str == '\0' )  return(0);		/* Null string */

	cp = str = rt_strdup( orig_str );

	/*  Handle each path element */
	if( pathp->fp_len > 0 )
		comb_dp = DB_FULL_PATH_CUR_DIR(pathp);
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
			db_add_node_to_full_path( pathp, dp );

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
			db_add_node_to_full_path( pathp, dp );
			if( oldc == '\0' )  {
				/* No more path was given, all is well */
				goto out;
			}
			/* Additional path was given, this is wrong */
			if( noisy )  {
				char	*sofar = db_path_to_string(pathp);
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
	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
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
	struct region	region;
	struct region	*regionp = &region;

	region.reg_name = "DUMMY from rt_mkgift_tree()";

	/* Build tree representing boolean expression in Member records */
	if( rt_pure_boolean_expressions )  {
		curtree = rt_mkbool_tree( trees, subtreecount, regionp );
		if(rt_g.debug&DEBUG_TREEWALK)  {
			rt_log("rt_mkgift_tree returns pure tree:\n");
			rt_pr_tree(curtree, 0);
		}
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

		if(rt_g.debug&DEBUG_TREEWALK)  {
			rt_log("rt_mkgift_tree() intermediate term:\n");
			rt_pr_tree(tstart->tl_tree, 0);
		}

		/* tstart here at union */
		tstart = tnext;
	}

	curtree = rt_mkbool_tree( trees, subtreecount, regionp );
	if(rt_g.debug&DEBUG_TREEWALK)  {
		rt_log("rt_mkgift_tree() returns:\n");
		rt_pr_tree(curtree, 0);
	}
	return( curtree );
}

static vect_t xaxis = { 1.0, 0, 0 };
static vect_t yaxis = { 0, 1.0, 0 };
static vect_t zaxis = { 0, 0, 1.0 };

/*
 *			D B _ R E C U R S E
 *
 *  Recurse down the tree, finding all the leaves
 *  (or finding just all the regions).
 *
 *  ts_region_start_func() is called to permit regions to be skipped.
 *  It is not intended to be used for collecting state.
 */
union tree *
db_recurse( tsp, pathp, region_start_statepp )
struct tree_state	*tsp;
struct full_path	*pathp;
struct combined_tree_state	**region_start_statepp;
{
	struct directory	*dp;
	register union record	*rp = (union record *)0;
	register int		i;
	struct tree_list	*tlp;		/* cur elem of trees[] */
	struct tree_list	*trees = TREE_LIST_NULL;	/* array */
	union tree		*curtree = TREE_NULL;

	if( tsp->ts_dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_recurse:  bad dbip\n");
	if( pathp->fp_len <= 0 )  {
		rt_log("db_recurse() null path?\n");
		return(TREE_NULL);
	}
	dp = DB_FULL_PATH_CUR_DIR(pathp);
	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		rt_log("db_recurse() pathp='%s', tsp=x%x, *statepp=x%x\n",
			sofar, tsp,
			*region_start_statepp );
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
			struct combined_tree_state	*ctsp;
			/*
			 *  This is the start of a new region.
			 *  If handler rejects this region, skip on.
			 *  This might be used for ignoring air regions.
			 */
			if( tsp->ts_region_start_func && 
			    tsp->ts_region_start_func( &nts, pathp ) < 0 )
				goto fail;

			if( tsp->ts_stop_at_regions )  {
				goto region_end;
			}

			/* Take note of full state here at region start */
			if( *region_start_statepp != (struct combined_tree_state *)0 ) {
				rt_log("db_recurse() ERROR at start of a region, *region_start_statepp = x%x\n",
					*region_start_statepp );
				goto fail;
			}
			GETSTRUCT( ctsp, combined_tree_state );
			ctsp->cts_s = nts;	/* struct copy */
			db_dup_full_path( &(ctsp->cts_p), pathp );
			*region_start_statepp = ctsp;
			if(rt_g.debug&DEBUG_TREEWALK)  {
				rt_log("setting *region_start_statepp to x%x\n", ctsp );
				db_pr_combined_tree_state(ctsp);
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
			if( (tlp->tl_tree = db_recurse( &memb_state, pathp, region_start_statepp )) != TREE_NULL )  {
				tlp++;
			}

			DB_FULL_PATH_POP(pathp);
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
		curtree = TREE_NULL;
	}
out:
	if( rp )  rt_free( (char *)rp, dp->d_namep );
	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		rt_log("db_recurse() return curtree=x%x, pathp='%s', *statepp=x%x\n",
			curtree, sofar,
			*region_start_statepp );
		rt_free(sofar, "path string");
	}
	return(curtree);		/* SUCCESS */
fail:
	if( rp )  rt_free( (char *)rp, dp->d_namep );
	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		rt_log("db_recurse() return curtree=NULL, pathp='%s', *statepp=x%x\n",
			sofar,
			*region_start_statepp );
		rt_free(sofar, "path string");
	}
	return( (union tree *)0 );	/* FAIL */
}

/*
 *			D B _ D U P _ S U B T R E E
 */
union tree *
db_dup_subtree( tp )
union tree	*tp;
{
	union tree	*new;

	new = (union tree *)rt_malloc( sizeof(union tree), "db_dup_subtree");
	*new = *tp;		/* struct copy */

	switch( tp->tr_op )  {
	case OP_SOLID:
		/* If this is a leaf, done */
		return(new);
	case OP_REGION:
		/* If this is a REGION leaf, dup combined_tree_state & path */
		{
			struct combined_tree_state	*cts;
			struct combined_tree_state	*ots;
			ots = (struct combined_tree_state *)tp->tr_a.tu_stp;
			cts=(struct combined_tree_state *)rt_malloc(
				sizeof(struct combined_tree_state),
				"combined region state");
			cts->cts_s = ots->cts_s;	/* struct copy */
			db_dup_full_path( &(cts->cts_p), &(ots->cts_p) );
			new->tr_a.tu_stp = (struct soltab *)cts;
		}
		return(new);

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		new->tr_b.tb_left = db_dup_subtree( tp->tr_b.tb_left );
		return(new);

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		new->tr_b.tb_left = db_dup_subtree( tp->tr_b.tb_left );
		new->tr_b.tb_right = db_dup_subtree( tp->tr_b.tb_right );
		return(new);

	default:
		rt_bomb("db_dup_subtree: bad op\n");
	}
	return( TREE_NULL );
}

/*
 *			D B _ N O N _ U N I O N _ P U S H
 */
void
db_non_union_push( tp )
union tree	*tp;
{
	union tree	*lhs;

top:
	/* If this is a leaf, done */
	if( tp->tr_op == OP_REGION )  return;

	/* This node is known to be a binary op */
	if( tp->tr_op == OP_UNION )  {
		/* Recurse both left and right */
		db_non_union_push( tp->tr_b.tb_left );
		db_non_union_push( tp->tr_b.tb_right );
		return;
	}

	if( tp->tr_op == OP_INTERSECT || tp->tr_op == OP_SUBTRACT )  {
		union tree	*lhs = tp->tr_b.tb_left;
	    	union tree	*rhs;

		if( lhs->tr_op != OP_UNION )  {
			/* Recurse left only */
			db_non_union_push( lhs );
			if( (lhs=tp->tr_b.tb_left)->tr_op != OP_UNION )
				return;
			/* lhs rewrite turned up a union here, do rewrite */
		}

		/*  Rewrite intersect and subtraction nodes, such that
		 *  (A u B) - C  becomes (A - C) u (B - C)
		 *
		 * tp->	     -
		 *	   /   \
		 * lhs->  u     C
		 *	 / \
		 *	A   B
		 */
		rhs = (union tree *)rt_malloc( sizeof(union tree),
			"non_union_push new rhs" );

		/* duplicate top node into rhs */
		*rhs = *tp;		/* struct copy */
		tp->tr_b.tb_right = rhs;
		/* rhs->tr_b.tb_right remains unchanged:
		 *
		 * tp->	     -
		 *	   /   \
		 * lhs->  u     -   <-rhs
		 *	 / \   / \
		 *	A   B ?   C
		 */

		rhs->tr_b.tb_left = lhs->tr_b.tb_right;
		/*
		 * tp->	     -
		 *	   /   \
		 * lhs->  u     -   <-rhs
		 *	 / \   / \
		 *	A   B B   C
		 */

		/* exchange left and top operators */
		tp->tr_op = lhs->tr_op;
		lhs->tr_op = rhs->tr_op;
		/*
		 * tp->	     u
		 *	   /   \
		 * lhs->  -     -   <-rhs
		 *	 / \   / \
		 *	A   B B   C
		 */

		/* Make a duplicate of rhs->tr_b.tb_right */
		lhs->tr_b.tb_right = db_dup_subtree( rhs->tr_b.tb_right );
		/*
		 * tp->	     u
		 *	   /   \
		 * lhs->  -     -   <-rhs
		 *	 / \   / \
		 *	A  C' B   C
		 */

		/* Now reconsider whole tree again */
		goto top;
	}
    	rt_log("db_non_union_push() ERROR tree op=%d.?\n", tp->tr_op );
}

/*
 *			D B _ C O U N T _ S U B T R E E _ R E G I O N S
 */
int
db_count_subtree_regions( tp )
union tree	*tp;
{
	int	cnt;

	switch( tp->tr_op )  {
	case OP_SOLID:
	case OP_REGION:
		return(1);

	case OP_UNION:
		/* This node is known to be a binary op */
		cnt = db_count_subtree_regions( tp->tr_b.tb_left );
		cnt += db_count_subtree_regions( tp->tr_b.tb_right );
		return(cnt);

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		/* This is as far down as we go -- this is a region top */
		return(1);

	default:
		rt_bomb("db_count_subtree_regions: bad op\n");
	}
	return( 0 );
}

/*
 *			D B _ T A L L Y _ S U B T R E E _ R E G I O N S
 */
int
db_tally_subtree_regions( tp, reg_trees, cur )
union tree	*tp;
union tree	**reg_trees;
int		cur;
{

	switch( tp->tr_op )  {
	case OP_SOLID:
	case OP_REGION:
		reg_trees[cur++] = tp;
		return(cur);

	case OP_UNION:
		/* This node is known to be a binary op */
		cur = db_tally_subtree_regions( tp->tr_b.tb_left, reg_trees, cur );
		cur = db_tally_subtree_regions( tp->tr_b.tb_right, reg_trees, cur );
		return(cur);

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		/* This is as far down as we go -- this is a region top */
		reg_trees[cur++] = tp;
		return(cur);

	default:
		rt_bomb("db_tally_subtree_regions: bad op\n");
	}
	return( cur );
}

/* ============================== */

static struct rt_i	*db_rtip;
static union tree	**db_reg_trees;
static int		db_reg_count;
static int		db_reg_current;		/* semaphored when parallel */
static union tree *	(*db_reg_end_func)();
static union tree *	(*db_reg_leaf_func)();

HIDDEN union tree *db_gettree_region_end( tsp, pathp, curtree )
register struct tree_state	*tsp;
struct full_path	*pathp;
union tree		*curtree;
{
	register struct combined_tree_state	*cts;

	cts=(struct combined_tree_state *)rt_malloc(
		sizeof(struct combined_tree_state), "combined region state");
	cts->cts_s = *tsp;	/* struct copy */
	db_dup_full_path( &(cts->cts_p), pathp );

	curtree=(union tree *)rt_malloc(sizeof(union tree), "solid tree");
	curtree->tr_op = OP_REGION;
	curtree->tr_a.tu_stp = (struct soltab *)cts;
	curtree->tr_a.tu_name = (char *)0;
	curtree->tr_regionp = (struct region *)0;

	return(curtree);
}

HIDDEN union tree *db_gettree_leaf( tsp, pathp, rp, id )
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
	db_dup_full_path( &(cts->cts_p), pathp );

	curtree=(union tree *)rt_malloc(sizeof(union tree), "solid tree");
	curtree->tr_op = OP_REGION;
	curtree->tr_a.tu_stp = (struct soltab *)cts;
	curtree->tr_a.tu_name = (char *)0;
	curtree->tr_regionp = (struct region *)0;

	return(curtree);
}

void
db_walk_subtree( tp, region_start_statepp )
union tree	*tp;
struct combined_tree_state	**region_start_statepp;
{
	struct combined_tree_state	*ctsp;
	union tree	*curtree;

	switch( tp->tr_op )  {
	/*  case OP_SOLID:*/
	case OP_REGION:
		/* Flesh out remainder of subtree */
		ctsp = (struct combined_tree_state *)tp->tr_a.tu_stp;
		ctsp->cts_s.ts_dbip = db_rtip->rti_dbip;
		ctsp->cts_s.ts_stop_at_regions = 0;
		/* All regions will be accepted, in this 2nd pass */
		ctsp->cts_s.ts_region_start_func = 0;
		/* ts_region_end_func() will be called in db_walk_dispatcher() */
		ctsp->cts_s.ts_region_end_func = 0;
		/* Use user's leaf function. Import via static global */
		ctsp->cts_s.ts_leaf_func = db_reg_leaf_func;

		/* If region already seen, force flag */
		if( *region_start_statepp )
			ctsp->cts_s.ts_sofar |= TS_SOFAR_REGION;
		else
			ctsp->cts_s.ts_sofar &= ~TS_SOFAR_REGION;

		curtree = db_recurse( &ctsp->cts_s, &ctsp->cts_p, region_start_statepp );
		if( curtree == TREE_NULL )  {
			rt_log("db_walk_subtree()/db_recurse() FAIL\n");
			db_free_combined_tree_state( ctsp );
			tp->tr_op = OP_NOP;
			return;
		}
		/* replace *tp with new subtree */
		*tp = *curtree;		/* struct copy */
		db_free_combined_tree_state( ctsp );
		rt_free( (char *)curtree, "replaced tree node" );
		return;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		db_walk_subtree( tp->tr_b.tb_left, region_start_statepp );
		return;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		db_walk_subtree( tp->tr_b.tb_left, region_start_statepp );
		db_walk_subtree( tp->tr_b.tb_right, region_start_statepp );
		return;

	default:
		rt_bomb("db_walk_subtree: bad op\n");
	}
}

void
db_walk_dispatcher()
{
	struct combined_tree_state	*region_start_statep;
	int		mine;
	union tree	*curtree;

	while(1)  {
		RES_ACQUIRE( &rt_g.res_worker );
		mine = db_reg_current++;
		RES_RELEASE( &rt_g.res_worker );

		if( mine >= db_reg_count )
			break;

		if( rt_g.debug&DEBUG_TREEWALK )
			rt_log("\n\n***** db_walk_dispatcher() on item %d\n\n", mine );

		/* Walk the full subtree now */
		region_start_statep = (struct combined_tree_state *)0;
		if( (curtree = db_reg_trees[mine]) == TREE_NULL )
			continue;
		db_walk_subtree( curtree, &region_start_statep );
		if( curtree->tr_op == OP_NOP )  {
			rt_log("db_walk_dispatcher() Entire subtree vanished?\n");
			if( region_start_statep )
				db_free_combined_tree_state( region_start_statep );
			continue;
		}

		if( !region_start_statep )
			rt_bomb("db_walk_dispatcher() region started with no state?\n");

		/* This is a new region */
		if( rt_g.debug&DEBUG_TREEWALK )
			db_pr_combined_tree_state(region_start_statep);

		/* XXX use return code? */
		(*db_reg_end_func)( &(region_start_statep->cts_s),
			&(region_start_statep->cts_p),
			curtree );

		db_free_combined_tree_state( region_start_statep );
	}
}

db_walk_tree( rtip, argc, argv, ncpu, init_state, reg_start_func, reg_end_func, leaf_func )
struct rt_i	*rtip;
int		argc;
char		**argv;
int		ncpu;
struct tree_state *init_state;
int		(*reg_start_func)();
union tree *	(*reg_end_func)();
union tree *	(*leaf_func)();
{
	union tree		*whole_tree = TREE_NULL;
	int			new_reg_count;
	int			i;
	union tree		**reg_trees;	/* (*reg_trees)[] */

	RT_CHECK_RTI(rtip);

	db_rtip = rtip;			/* make global to this module */

	/* Walk each of the given path strings */
	for( i=0; i < argc; i++ )  {
		register union tree	*curtree;
		struct tree_state	ts;
		struct full_path	path;
		struct combined_tree_state	*region_start_statep;

		ts = *init_state;	/* struct copy */
		ts.ts_dbip = rtip->rti_dbip;
		path.fp_len = path.fp_maxlen = 0;

		/* First, establish context from given path */
		if( db_follow_path_for_state( &ts, &path, argv[i], LOOKUP_NOISY ) < 0 )
			continue;	/* ERROR */

		/*
		 *  Second, walk tree from root to start of all regions.
		 *  Build a boolean tree of all regions.
		 *  Use user function to accept/reject each region here.
		 *  Use internal functions to process regions & leaves.
		 */
		ts.ts_stop_at_regions = 1;
		ts.ts_region_start_func = reg_start_func;
		ts.ts_region_end_func = db_gettree_region_end;
		ts.ts_leaf_func = db_gettree_leaf;

		region_start_statep = (struct combined_tree_state *)0;
		curtree = db_recurse( &ts, &path, &region_start_statep );
		if( region_start_statep )
			db_free_combined_tree_state( region_start_statep );
		if( curtree == (union tree *)0 )
			continue;	/* ERROR */

		if( rt_g.debug&DEBUG_TREEWALK )  {
			rt_log("tree after db_recurse():\n");
			rt_pr_tree( curtree, 0 );
		}

		if( whole_tree == TREE_NULL )  {
			whole_tree = curtree;
		} else {
			union tree	*new;

			new = (union tree *)rt_malloc( sizeof(union tree), "new tree top");
			new->tr_op = OP_UNION;
			new->tr_b.tb_left = whole_tree;
			new->tr_b.tb_right = curtree;
			whole_tree = new;
		}
	}

	if( whole_tree == TREE_NULL )
		return(-1);	/* ERROR, nothing worked */

	/*
	 *  Third, push all non-union booleans down.
	 */
	db_non_union_push( whole_tree );
	if( rt_g.debug&DEBUG_TREEWALK )  {
		rt_log("tree after db_non_union_push():\n");
		rt_pr_tree( whole_tree, 0 );
	}

	/*
	 *  Build array of sub-tree pointers, one per region,
	 *  for parallel processing below.
	 */
	new_reg_count = db_count_subtree_regions( whole_tree );
	reg_trees = (union tree **)rt_malloc( sizeof(union tree *) * new_reg_count,
		"*reg_trees[]" );
	(void)db_tally_subtree_regions( whole_tree, reg_trees, 0 );

	if( rt_g.debug&DEBUG_TREEWALK )  {
		rt_log("new region count=%d\n", new_reg_count);
		for( i=0; i<new_reg_count; i++ )  {
			rt_log("tree %d =\n", i);
			rt_pr_tree( reg_trees[i], 0 );
		}
	}

	/*
	 *  Fourth, in parallel, for each region, walk the tree to the leaves.
	 */
	/* Export some state to read-only static variables */
	db_reg_trees = reg_trees;
	db_reg_count = new_reg_count;
	db_reg_current = 0;
	db_reg_end_func = reg_end_func;
	db_reg_leaf_func = leaf_func;

	if( ncpu <= 1 )  {
		db_walk_dispatcher();
	} else {
		/* XXX Need to ensure that rt_g.rtg_parallel is set! */
		/* XXX Should actually be done by rt_parallel. */
		rt_g.rtg_parallel = 1;
		/* XXX Need to check that RES_INIT()s have been done too! */
		rt_parallel( db_walk_dispatcher, ncpu );
	}
	return(0);	/* OK */
}

/* ============================== */

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

HIDDEN int rt_gettree_region_start( tsp, pathp )
struct tree_state	*tsp;
struct full_path	*pathp;
{

	/* Ignore "air" regions unless wanted */
	if( db_rtip->useair == 0 &&  tsp->ts_aircode != 0 )  {
		db_rtip->rti_air_discards++;
		return(-1);	/* drop this region */
	}
	return(0);
}

HIDDEN union tree *rt_gettree_region_end( tsp, pathp, curtree )
register struct tree_state	*tsp;
struct full_path	*pathp;
union tree		*curtree;
{
	register struct combined_tree_state	*cts;
	struct region		*rp;
	struct directory	*dp;

	GETSTRUCT( rp, region );
	rp->reg_forw = REGION_NULL;
	rp->reg_regionid = tsp->ts_regionid;
	rp->reg_aircode = tsp->ts_aircode;
	rp->reg_gmater = tsp->ts_gmater;
	rp->reg_mater = tsp->ts_mater;		/* struct copy */
	rp->reg_name = db_path_to_string( pathp );

	dp = DB_FULL_PATH_CUR_DIR(pathp);
	/* XXX This should be semaphore protected! */
	rp->reg_instnum = dp->d_uses++;

	if(rt_g.debug&DEBUG_TREEWALK)  {
		rt_log("rt_gettree_region_end() %s\n", rp->reg_name );
		rt_pr_tree( curtree, 0 );
	}

	/* Mark all solids & nodes as belonging to this region */
	rt_tree_region_assign( curtree, rp );

	rt_add_regtree( db_rtip, rp, curtree );
	return(curtree);
}

HIDDEN union tree *rt_gettree_leaf( tsp, pathp, rp, id )
struct tree_state	*tsp;
struct full_path	*pathp;
union record		*rp;
int			id;
{
	struct soltab	*stp;
	union tree	*curtree;
	struct directory	*dp;

	/* Note:  solid may not be contained by a region (yet) */

	dp = DB_FULL_PATH_CUR_DIR(pathp);
	if( (stp = rt_add_solid( db_rtip, rp, dp, tsp->ts_mat )) == SOLTAB_NULL )
		return(TREE_NULL);

	curtree=(union tree *)rt_malloc(sizeof(union tree), "solid tree");
	curtree->tr_op = OP_SOLID;
	curtree->tr_a.tu_stp = stp;
	curtree->tr_a.tu_name = db_path_to_string( pathp );
	/* regionp will be filled in later by rt_tree_region_assign() */
	curtree->tr_a.tu_regionp = (struct region *)0;

	if(rt_g.debug&DEBUG_TREEWALK)
		rt_log("rt_gettree_leaf() %s\n", curtree->tr_a.tu_name );

	return(curtree);
}

/*
 * XXX  NEW NEW NEW
 *  			R T _ G E T T R E E
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
	int			prev_sol_count;
	int			i;

	RT_CHECK_RTI(rtip);

	if(!rtip->needprep)
		rt_bomb("rt_gettree called again after rt_prep!\n");

	prev_sol_count = rtip->nsolids;

	i = db_walk_tree( rtip, 1, &node, 1,
		&rt_initial_tree_state,
		rt_gettree_region_start,
		rt_gettree_region_end,
		rt_gettree_leaf );

	if( i < 0 )  return(-1);

	if( rtip->nsolids <= prev_sol_count )
		rt_log("rt_gettree(%s) warning:  no solids found\n", node);
	return(0);	/* OK */
}

/*
 *  			R T _ G E T T R E E S
 *
 *  User-called function to add a set of tree hierarchies to the active set.
 *  
 *  Returns -
 *  	0	Ordinarily
 *	-1	On major error
 */
int
rt_gettrees( rtip, argc, argv )
struct rt_i	*rtip;
int		argc;
char		**argv;
{
	int			prev_sol_count;
	int			i;

	RT_CHECK_RTI(rtip);

	if(!rtip->needprep)
		rt_bomb("rt_gettree called again after rt_prep!\n");

	if( argc <= 0 )  return(-1);	/* FAIL */

	prev_sol_count = rtip->nsolids;

	i = db_walk_tree( rtip, argc, argv, 1,	/* # cpus */
		&rt_initial_tree_state,
		rt_gettree_region_start,
		rt_gettree_region_end,
		rt_gettree_leaf );

	if( i < 0 )  return(-1);

	if( rtip->nsolids <= prev_sol_count )
		rt_log("rt_gettrees(%s) warning:  no solids found\n", argv[0]);
	return(0);	/* OK */
}
