#define USE_NEW_IMPORT	1

/*
 *			D B _ T R E E . C
 *
 * Functions -
 *	db_walk_tree		Parallel tree walker
 *	db_path_to_mat		Given a path, return a matrix.
 *	db_region_mat		Given a name, return a matrix
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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"

#include "./debug.h"

struct tree_list {
	union tree *tl_tree;
	int	tl_op;
};
#define TREE_LIST_NULL	((struct tree_list *)0)

/* -------------------------------------------------- */


RT_EXTERN( union tree *db_mkbool_tree , (struct tree_list *tree_list , int howfar ) );
RT_EXTERN( union tree *db_mkgift_tree , (struct tree_list *tree_list , int howfar , struct db_tree_state *tsp ) );

/*
 *			R T _ C O M B _ V 4 _ I M P O R T
 *
 *  Import a combination record from a V4 database into internal form.
 */
int
rt_comb_v4_import( ip, ep, matrix )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
CONST matp_t			matrix;		/* NULL if identity */
{
	union record		*rp;
	struct tree_list	*tree_list;
	union tree		*tree;
	struct rt_comb_internal	*comb;
	int			i,j;
	int			node_count;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;

	if( rp[0].u_id != ID_COMB )
	{
		bu_log( "rt_comb_v4_import: Attempt to import a non-combination\n" );
		return( -1 );
	}

	/* Compute how many granules of MEMBER records follow */
	node_count = ep->ext_nbytes/sizeof( union record ) - 1;

	if( node_count )
		tree_list = (struct tree_list *)rt_calloc( node_count , sizeof( struct tree_list ) , "tree_list" );
	else
		tree_list = (struct tree_list *)NULL;

	for( j=0 ; j<node_count ; j++ )
	{
		if( rp[j+1].u_id != ID_MEMB )
		{
			rt_free( (char *)tree_list , "rt_comb_v4_import: tree_list" );
			bu_log( "rt_comb_v4_import(): granule in external buffer is not ID_MEMB, id=%d\n", rp[j+1].u_id );
			return( -1 );
		}

		switch( rp[j+1].M.m_relation )
		{
			case '+':
				tree_list[j].tl_op = OP_INTERSECT;
				break;
			case '-':
				tree_list[j].tl_op = OP_SUBTRACT;
				break;
			default:
				bu_log("rt_comb_v4_import() unknown op=x%x, assuming UNION\n", rp[j+1].M.m_relation );
				/* Fall through */
			case 'u':
				tree_list[j].tl_op = OP_UNION;
				break;
		}
		/* Build leaf node for in-memory tree */
		{
			union tree		*tp;
			struct tree_db_leaf	*mi;
			mat_t			diskmat;
			char			namebuf[NAMESIZE+2];

			BU_GETUNION( tp, tree );
			tree_list[j].tl_tree = tp;
			tp->tr_l.magic = RT_TREE_MAGIC;
			tp->tr_l.tl_op = OP_DB_LEAF;
			strncpy( namebuf, rp[j+1].M.m_instname, NAMESIZE );
			namebuf[NAMESIZE] = '\0';	/* ensure null term */
			tp->tr_l.tl_name = bu_strdup( namebuf );

			/* See if disk record is identity matrix */
			rt_mat_dbmat( diskmat, rp[j+1].M.m_mat );
			if( mat_is_identity( diskmat ) )  {
				if( matrix == NULL )  {
					tp->tr_l.tl_mat = NULL;	/* identity */
				} else {
					tp->tr_l.tl_mat = mat_dup( matrix );
				}
			} else {
				if( matrix == NULL )  {
					tp->tr_l.tl_mat = mat_dup( diskmat );
				} else {
					mat_t	prod;
					mat_mul( prod, matrix, diskmat );
					tp->tr_l.tl_mat = mat_dup( prod );
				}
			}
/* bu_log("M_name=%s, matp=x%x\n", tp->tr_l.tl_name, tp->tr_l.tl_mat ); */
		}
	}
	if( node_count )
		tree = db_mkgift_tree( tree_list, node_count, (struct db_tree_state *)NULL );
	else
		tree = (union tree *)NULL;

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_COMBINATION;
	comb = (struct rt_comb_internal *)rt_malloc( sizeof( struct rt_comb_internal ) , "rt_comb_v4_import: rt_comb_internal" );
	ip->idb_ptr = (genptr_t)comb;
	comb->magic = RT_COMB_MAGIC;
	bu_vls_init( &comb->shader_name );
	bu_vls_init( &comb->shader_param );
	bu_vls_init( &comb->material );
	comb->tree = tree;
	comb->region_flag = (rp[0].c.c_flags == 'R');

	comb->region_id = rp[0].c.c_regionid;
	comb->aircode = rp[0].c.c_aircode;
	comb->GIFTmater = rp[0].c.c_material;
	comb->los = rp[0].c.c_los;

	comb->rgb_valid = rp[0].c.c_override;
	comb->rgb[0] = rp[0].c.c_rgb[0];
	comb->rgb[1] = rp[0].c.c_rgb[1];
	comb->rgb[2] = rp[0].c.c_rgb[2];
	bu_vls_strncpy( &comb->shader_name , rp[0].c.c_matname, 32 );
	bu_vls_strncpy( &comb->shader_param , rp[0].c.c_matparm, 60 );
	comb->inherit = rp[0].c.c_inherit;
	/* Automatic material table lookup here? */
	bu_vls_printf( &comb->material, "gift%d", comb->GIFTmater );

	return( 0 );
}

/*
 *			R T _ C O M B _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this combination.
 */
void
rt_comb_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_comb_internal	*comb;

	RT_CK_DB_INTERNAL(ip);
	comb = (struct rt_comb_internal *)ip->idb_ptr;
	RT_CK_COMB(comb);

	/* If tree hasn't been stolen, release it */
	if(comb->tree) db_free_tree( comb->tree );
	comb->tree = NULL;

	bu_vls_free( &comb->shader_name );
	bu_vls_free( &comb->shader_param );
	bu_vls_free( &comb->material );

	comb->magic = 0;			/* sanity */
	rt_free( (char *)comb, "comb ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

/* -------------------- */
/* Some export support routines */

/*
 *			D B _ C K _ L E F T _ H E A V Y _ T R E E
 *
 *  Support routine for db_ck_v4gift_tree().
 *  Ensure that the tree below 'tp' is left-heavy, i.e. that there are
 *  nothing but solids on the right side of any binary operations.
 *
 *  Returns -
 *	-1	ERROR
 *	 0	OK
 */
int
db_ck_left_heavy_tree( tp, no_unions )
CONST union tree	*tp;
int			no_unions;
{
	RT_CK_TREE(tp);
	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		break;

	case OP_UNION:
		if( no_unions )  return -1;
		/* else fall through */
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		if( tp->tr_b.tb_right->tr_op != OP_DB_LEAF )
			return -1;
		return db_ck_left_heavy_tree( tp->tr_b.tb_left, no_unions );

	default:
		bu_log("db_ck_left_heavy_tree: bad op %d\n", tp->tr_op);
		rt_bomb("db_ck_left_heavy_tree\n");
	}
	return 0;
}


/*
 *			D B _ C K _ V 4 G I F T _ T R E E
 *
 *  Look a gift-tree in the mouth.
 *  Ensure that this boolean tree conforms to the GIFT convention that
 *  union operations must bind the loosest.
 *  There are two stages to this check:
 *  1)  Ensure that if unions are present they are all at the root of tree,
 *  2)  Ensure non-union children of union nodes are all left-heavy
 *      (nothing but solid nodes permitted on rhs of binary operators).
 *
 *  Returns -
 *	-1	ERROR
 *	 0	OK
 */
int
db_ck_v4gift_tree( tp )
CONST union tree	*tp;
{
	RT_CK_TREE(tp);
	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		break;

	case OP_UNION:
		if( db_ck_v4gift_tree( tp->tr_b.tb_left ) < 0 )
			return -1;
		return db_ck_v4gift_tree( tp->tr_b.tb_right );

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		return db_ck_left_heavy_tree( tp, 1 );

	default:
		bu_log("db_ck_v4gift_tree: bad op %d\n", tp->tr_op);
		rt_bomb("db_ck_v4gift_tree\n");
	}
	return 0;
}



/* -------------------------------------------------- */

/*
 *			D B _ N E W _ C O M B I N E D _ T R E E _ S T A T E
 */
struct combined_tree_state *
db_new_combined_tree_state( tsp, pathp )
register CONST struct db_tree_state	*tsp;
register CONST struct db_full_path	*pathp;
{
	struct combined_tree_state	*new;

	RT_CK_FULL_PATH(pathp);
	RT_CK_DBI(tsp->ts_dbip);

	BU_GETSTRUCT( new, combined_tree_state );
	new->magic = RT_CTS_MAGIC;
	new->cts_s = *tsp;		/* struct copy */
	db_full_path_init( &(new->cts_p) );
	db_dup_full_path( &(new->cts_p), pathp );
	return new;
}

/*
 *			D B _ D U P _ C O M B I N E D _ T R E E _ S T A T E
 */
struct combined_tree_state *
db_dup_combined_tree_state( old )
CONST struct combined_tree_state	*old;
{
	struct combined_tree_state	*new;

 	RT_CK_CTS(old);
	BU_GETSTRUCT( new, combined_tree_state );
	new->magic = RT_CTS_MAGIC;
	new->cts_s = old->cts_s;	/* struct copy */
	db_full_path_init( &(new->cts_p) );
	db_dup_full_path( &(new->cts_p), &(old->cts_p) );
	return new;
}

/*
 *			D B _ F R E E _ C O M B I N E D _ T R E E _ S T A T E
 */
void
db_free_combined_tree_state( ctsp )
register struct combined_tree_state	*ctsp;
{
 	RT_CK_CTS(ctsp);
	db_free_full_path( &(ctsp->cts_p) );
	bzero( (char *)ctsp, sizeof(*ctsp) );		/* sanity */
	rt_free( (char *)ctsp, "combined_tree_state");
}

/*
 *			D B _ P R _ T R E E _ S T A T E
 */
void
db_pr_tree_state( tsp )
register CONST struct db_tree_state	*tsp;
{
	bu_log("db_pr_tree_state(x%x):\n", tsp);
	bu_log(" ts_dbip=x%x\n", tsp->ts_dbip);
	bu_printb(" ts_sofar", tsp->ts_sofar, "\020\3REGION\2INTER\1MINUS" );
	bu_log("\n");
	bu_log(" ts_regionid=%d\n", tsp->ts_regionid);
	bu_log(" ts_aircode=%d\n", tsp->ts_aircode);
	bu_log(" ts_gmater=%d\n", tsp->ts_gmater);
	bu_log(" ts_los=%d\n", tsp->ts_los);
	bu_log(" ts_mater.ma_color=%g,%g,%g\n",
		tsp->ts_mater.ma_color[0],
		tsp->ts_mater.ma_color[1],
		tsp->ts_mater.ma_color[2] );
	bu_log(" ts_mater.ma_matname=%s\n", tsp->ts_mater.ma_matname );
	bu_log(" ts_mater.ma_matparam=%s\n", tsp->ts_mater.ma_matparm );
	mat_print("ts_mat", tsp->ts_mat );
}

/*
 *			D B _ P R _ C O M B I N E D _ T R E E _ S T A T E
 */
void
db_pr_combined_tree_state( ctsp )
register CONST struct combined_tree_state	*ctsp;
{
	char	*str;

 	RT_CK_CTS(ctsp);
	bu_log("db_pr_combined_tree_state(x%x):\n", ctsp);
	db_pr_tree_state( &(ctsp->cts_s) );
	str = db_path_to_string( &(ctsp->cts_p) );
	bu_log(" path='%s'\n", str);
	rt_free( str, "path string" );
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
db_apply_state_from_comb( tsp, pathp, ep )
struct db_tree_state		*tsp;
CONST struct db_full_path	*pathp;
CONST struct bu_external	*ep;
{
	register CONST union record	*rp;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != ID_COMB )  {
		char	*sofar = db_path_to_string(pathp);
		bu_log("db_apply_state_from_comb() defective record at '%s'\n",
			sofar );
		rt_free(sofar, "path string");
		return(-1);
	}

	if( rp->c.c_override == 1 )  {
		if( tsp->ts_sofar & TS_SOFAR_REGION )  {
			if( (tsp->ts_sofar&(TS_SOFAR_MINUS|TS_SOFAR_INTER)) == 0 )  {
				/* This combination is within a region */
				char	*sofar = db_path_to_string(pathp);

				bu_log("db_apply_state_from_comb(): WARNING: color override in combination within region '%s', ignored\n",
					sofar );
				rt_free(sofar, "path string");
			}
			/* Just quietly ignore it -- it's being subtracted off */
		} else if( tsp->ts_mater.ma_cinherit == DB_INH_LOWER )  {
			tsp->ts_mater.ma_override = 1;
			tsp->ts_mater.ma_color[0] =
				(((double)(rp->c.c_rgb[0]))*bn_inv255);
			tsp->ts_mater.ma_color[1] =
				(((double)(rp->c.c_rgb[1]))*bn_inv255);
			tsp->ts_mater.ma_color[2] =
				(((double)(rp->c.c_rgb[2]))*bn_inv255);
			tsp->ts_mater.ma_cinherit = rp->c.c_inherit;
		}
	}
	if( rp->c.c_matname[0] != '\0' )  {
		if( tsp->ts_sofar & TS_SOFAR_REGION )  {
			if( (tsp->ts_sofar&(TS_SOFAR_MINUS|TS_SOFAR_INTER)) == 0 )  {
				/* This combination is within a region */
				char	*sofar = db_path_to_string(pathp);

				bu_log("db_apply_state_from_comb(): WARNING: material property spec in combination within region '%s', ignored\n",
					sofar );
				rt_free(sofar, "path string");
			}
			/* Just quietly ignore it -- it's being subtracted off */
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
				bu_log("Warning:  region unioned into region at '%s', lower region info ignored\n",
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
			tsp->ts_los = rp->c.c_los;
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
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
CONST struct member	*mp;
{
	register struct directory *mdp;
	mat_t			xmat;
	mat_t			old_xlate;
	register struct animate *anp;
	char			namebuf[NAMESIZE+2];

	if( mp->m_id != ID_MEMB )  {
		char	*sofar = db_path_to_string(pathp);
		bu_log("db_apply_state_from_memb:  defective member rec in '%s'\n", sofar);
		rt_free(sofar, "path string");
		return(-1);
	}

	/* Trim m_instname, and add to the path */
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
		bu_log("Animate %s with...\n", sofar);
		rt_free(sofar, "path string");
	}
	/*
	 *  For each of the animations attached to the mentioned object,
	 *  see if the current accumulated path matches the path
	 *  specified in the animation.
	 *  Comparison is performed right-to-left (from leafward to rootward).
	 */
	for( anp = mdp->d_animate; anp != ANIM_NULL; anp = anp->an_forw ) {
		register int i;
		register int j = pathp->fp_len-1;
		register int anim_flag;
		
		RT_CK_ANIMATE(anp);
		i = anp->an_path.fp_len-1;
		anim_flag = 1;

		if (rt_g.debug & DEBUG_ANIM) {
			char	*str;

			str = db_path_to_string( &(anp->an_path) );
			bu_log( "\t%s\t", str );
			rt_free( str, "path string" );
			bu_log("an_path.fp_len-1:%d  pathp->fp_len-1:%d\n",
				i, j);

		}
		for( ; i>=0 && j>=0; i--, j-- )  {
			if( anp->an_path.fp_names[i] != pathp->fp_names[j] ) {
				if (rt_g.debug & DEBUG_ANIM) {
					bu_log("%s != %s\n",
					     anp->an_path.fp_names[i]->d_namep,
					     pathp->fp_names[j]->d_namep);
				}
				anim_flag = 0;
				break;
			}
		}
		/* Perhaps tsp->ts_mater should be just tsp someday? */
		if (anim_flag)
			db_do_anim( anp, old_xlate, xmat, &(tsp->ts_mater) );
	}

	mat_mul(tsp->ts_mat, old_xlate, xmat);
	return(0);		/* Success */
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
db_apply_state_from_memb_new( tsp, pathp, tp )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
CONST union tree	*tp;
{
	register struct directory *mdp;
	mat_t			xmat;
	mat_t			old_xlate;
	register struct animate *anp;

	RT_CK_FULL_PATH(pathp);
	RT_CK_TREE(tp);
	if( (mdp = db_lookup( tsp->ts_dbip, tp->tr_l.tl_name, LOOKUP_NOISY )) == DIR_NULL )
		return(-1);

	db_add_node_to_full_path( pathp, mdp );

	mat_copy( old_xlate, tsp->ts_mat );
	if( tp->tr_l.tl_mat )
		mat_copy( xmat, tp->tr_l.tl_mat );
	else
		mat_idn( xmat );

	/* Check here for animation to apply */
	if ((mdp->d_animate != ANIM_NULL) && (rt_g.debug & DEBUG_ANIM)) {
		char	*sofar = db_path_to_string(pathp);
		bu_log("Animate %s with...\n", sofar);
		rt_free(sofar, "path string");
	}
	/*
	 *  For each of the animations attached to the mentioned object,
	 *  see if the current accumulated path matches the path
	 *  specified in the animation.
	 *  Comparison is performed right-to-left (from leafward to rootward).
	 */
	for( anp = mdp->d_animate; anp != ANIM_NULL; anp = anp->an_forw ) {
		register int i;
		register int j = pathp->fp_len-1;
		register int anim_flag;
		
		RT_CK_ANIMATE(anp);
		i = anp->an_path.fp_len-1;
		anim_flag = 1;

		if (rt_g.debug & DEBUG_ANIM) {
			char	*str;

			str = db_path_to_string( &(anp->an_path) );
			bu_log( "\t%s\t", str );
			rt_free( str, "path string" );
			bu_log("an_path.fp_len-1:%d  pathp->fp_len-1:%d\n",
				i, j);

		}
		for( ; i>=0 && j>=0; i--, j-- )  {
			if( anp->an_path.fp_names[i] != pathp->fp_names[j] ) {
				if (rt_g.debug & DEBUG_ANIM) {
					bu_log("%s != %s\n",
					     anp->an_path.fp_names[i]->d_namep,
					     pathp->fp_names[j]->d_namep);
				}
				anim_flag = 0;
				break;
			}
		}
		/* Perhaps tsp->ts_mater should be just tsp someday? */
		if (anim_flag)  {
			db_do_anim( anp, old_xlate, xmat, &(tsp->ts_mater) );
		}
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
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
CONST char		*orig_str;
int			noisy;
{
	struct bu_external	ext;
	register int		i;
	register char		*cp;
	register char		*ep;
	char			*str;		/* ptr to duplicate string */
	char			oldc;
	register struct member *mp;
	struct directory	*comb_dp;	/* combination's dp */
	struct directory	*dp;		/* element's dp */

	RT_CHECK_DBI( tsp->ts_dbip );
	RT_CK_FULL_PATH( pathp );

	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		bu_log("db_follow_path_for_state() pathp='%s', tsp=x%x, orig_str='%s', noisy=%d\n",
			sofar, tsp, orig_str, noisy );
		rt_free(sofar, "path string");
	}

	if( *orig_str == '\0' )  return(0);		/* Null string */

	BU_INIT_EXTERNAL( &ext );
	cp = str = bu_strdup( orig_str );

	/* Prime the pumps, and get the starting combination */
	if( pathp->fp_len > 0 )  {
		comb_dp = DB_FULL_PATH_CUR_DIR(pathp);
		oldc = 'X';	/* Anything non-null */
	}  else  {
		/* Peel out first path element & look it up. */

		/* Skip any leading slashes */
		while( *cp && *cp == '/' )  cp++;

		/* Find end of this path element and null terminate */
		ep = cp;
		while( *ep != '\0' && *ep != '/' )  ep++;
		oldc = *ep;
		*ep = '\0';

		if( (dp = db_lookup( tsp->ts_dbip, cp, noisy )) == DIR_NULL )
			goto fail;

		/* Process animations located at the root */
		if( tsp->ts_dbip->dbi_anroot )  {
			register struct animate *anp;
			mat_t	old_xlate, xmat;

			for( anp=tsp->ts_dbip->dbi_anroot; anp != ANIM_NULL; anp = anp->an_forw ) {
				RT_CK_ANIMATE(anp);
				if( dp != anp->an_path.fp_names[0] )
					continue;
				mat_copy( old_xlate, tsp->ts_mat );
				mat_idn( xmat );
				db_do_anim( anp, old_xlate, xmat, &(tsp->ts_mater) );
				mat_mul( tsp->ts_mat, old_xlate, xmat );
			}
		}

		db_add_node_to_full_path( pathp, dp );

		if( (dp->d_flags & DIR_COMB) == 0 )  goto is_leaf;

		/* Advance to next path element */
		cp = ep+1;
		comb_dp = dp;
	}
	/*
	 *  Process two things at once: the combination, and it's member.
	 */
	do  {
		if( oldc == '\0' )  break;

		/* Skip any leading slashes */
		while( *cp && *cp == '/' )  cp++;
		if( *cp == '\0' )  break;

		/* Find end of this path element and null terminate */
		ep = cp;
		while( *ep != '\0' && *ep != '/' )  ep++;
		oldc = *ep;
		*ep = '\0';

		if( (dp = db_lookup( tsp->ts_dbip, cp, noisy )) == DIR_NULL )
			goto fail;

		/* At this point, comb_db is the comb, dp is the member */
		if(rt_g.debug&DEBUG_TREEWALK)  {
			bu_log("db_follow_path_for_state() at %s/%s\n", comb_dp->d_namep, dp->d_namep );
		}

		/* Load the entire combination into contiguous memory */
		if( db_get_external( &ext, comb_dp, tsp->ts_dbip ) < 0 )
			goto fail;

		/* Apply state changes from new combination */
		if( db_apply_state_from_comb( tsp, pathp, &ext ) < 0 )
			goto fail;

		for( i=1; i < comb_dp->d_len; i++ )  {
			register union record *rp =
				(union record *)ext.ext_buf;
			mp = &(rp[i].M);

			/* If this is not the desired element, skip it */
			if( strncmp( mp->m_instname, cp, sizeof(mp->m_instname)) == 0 )
				goto found_it;
		}
		if(noisy) bu_log("db_follow_path_for_state() ERROR: unable to find '%s/%s'\n", comb_dp->d_namep, cp );
		goto fail;
found_it:
		if( db_apply_state_from_memb( tsp, pathp, mp ) < 0 )  {
			bu_log("db_follow_path_for_state() ERROR: unable to apply member %s state\n", dp->d_namep);
			goto fail;
		}
		/* directory entry was pushed */

		/* If not first element of comb, take note of operation */
		if( i > 1 )  {
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
		} else {
			/* Handle as a union */
		}
		db_free_external( &ext );

		/* If member is a leaf, handle leaf processing too. */
		if( (dp->d_flags & DIR_COMB) == 0 )  {
is_leaf:
			/* Object is a leaf */
			/*db_add_node_to_full_path( pathp, dp );*/
			if( oldc == '\0' )  {
				/* No more path was given, all is well */
				goto out;
			}
			/* Additional path was given, this is wrong */
			if( noisy )  {
				char	*sofar = db_path_to_string(pathp);
				bu_log("db_follow_path_for_state(%s) ERROR: found leaf early at '%s'\n",
					cp, sofar );
				rt_free(sofar, "path string");
			}
			goto fail;
		}

		/* Member is itself a combination */
		if( dp->d_len <= 1 )  {
			/* Combination has no members */
			if( noisy )  {
				bu_log("db_follow_path_for_state(%s) ERROR: combination '%s' has no members\n",
					cp, dp->d_namep );
			}
			goto fail;
		}

		/* Advance to next path element */
		cp = ep+1;
		comb_dp = dp;
	} while( oldc != '\0' );

out:
	db_free_external( &ext );
	rt_free( str, "bu_strdup (dup'ed path)" );
	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		bu_log("db_follow_path_for_state() returns pathp='%s'\n",
			sofar);
		rt_free(sofar, "path string");
	}
	return(0);		/* SUCCESS */
fail:
	db_free_external( &ext );
	rt_free( str, "bu_strdup (dup'ed path)" );
	return(-1);		/* FAIL */
}


/*
 *			D B _ M K B O O L _ T R E E
 *
 *  Given a tree_list array, build a tree of "union tree" nodes
 *  appropriately connected together.  Every element of the
 *  tree_list array used is replaced with a TREE_NULL.
 *  Elements which are already TREE_NULL are ignored.
 *  Returns a pointer to the top of the tree.
 */
HIDDEN union tree *
db_mkbool_tree( tree_list, howfar )
struct tree_list *tree_list;
int		howfar;
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

	/* Handle trivial cases */
	if( inuse <= 0 )
		return(TREE_NULL);
	if( inuse == 1 )  {
		curtree = first_tlp->tl_tree;
		first_tlp->tl_tree = TREE_NULL;
		return( curtree );
	}

	if( first_tlp->tl_op != OP_UNION )  {
		first_tlp->tl_op = OP_UNION;	/* Fix it */
		if( rt_g.debug & DEBUG_TREEWALK )  {
			bu_log("db_mkbool_tree() WARNING: non-union (%c) first operation ignored\n",
				first_tlp->tl_op );
		}
	}

	curtree = first_tlp->tl_tree;
	first_tlp->tl_tree = TREE_NULL;
	tlp=first_tlp+1;
	for( i=howfar-(tlp-tree_list); i>0; i--, tlp++ )  {
		if( tlp->tl_tree == TREE_NULL )
			continue;

		BU_GETUNION( xtp, tree );
		xtp->magic = RT_TREE_MAGIC;
		xtp->tr_b.tb_left = curtree;
		xtp->tr_b.tb_right = tlp->tl_tree;
		xtp->tr_b.tb_regionp = (struct region *)0;
		xtp->tr_op = tlp->tl_op;
		curtree = xtp;
		tlp->tl_tree = TREE_NULL;	/* empty the input slot */
	}
	return(curtree);
}

/*
 *			D B _ M K G I F T _ T R E E
 */
HIDDEN union tree *
db_mkgift_tree( trees, subtreecount, tsp )
struct tree_list	*trees;
int			subtreecount;
struct db_tree_state	*tsp;
{
	register struct tree_list *tstart;
	register struct tree_list *tnext;
	union tree		*curtree;
	int	i;
	int	j;

	/*
	 * This is how GIFT interpreted equations, so it is duplicated here.
	 * Any expressions between UNIONs are evaluated first.  For example:
	 *		A - B - C u D - E - F
	 * becomes	(A - B - C) u (D - E - F)
	 * so first do the parenthesised parts, and then go
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
		curtree = db_mkbool_tree( tstart, j );
		/* db_mkbool_tree() has side effect of zapping tree array,
		 * so build new first node in array.
		 */
		tstart->tl_op = OP_UNION;
		tstart->tl_tree = curtree;

		if(rt_g.debug&DEBUG_TREEWALK)  {
			bu_log("db_mkgift_tree() intermediate term:\n");
			rt_pr_tree(tstart->tl_tree, 0);
		}

		/* tstart here at union */
		tstart = tnext;
	}

final:
	curtree = db_mkbool_tree( trees, subtreecount );
	if(rt_g.debug&DEBUG_TREEWALK)  {
		bu_log("db_mkgift_tree() returns:\n");
		rt_pr_tree(curtree, 0);
	}
	return( curtree );
}

void
recurseit( tp, msp, pathp, region_start_statepp )
union tree		*tp;
struct db_tree_state	*msp;
struct db_full_path	*pathp;
struct combined_tree_state	**region_start_statepp;
{
	struct db_tree_state	memb_state;
	union tree		*subtree;

	memb_state = *msp;		/* struct copy */

	RT_CK_TREE(tp);
	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		if( db_apply_state_from_memb_new( &memb_state, pathp, tp ) < 0 )
			return;		/* error? */

		/* Recursive call */
		if( (subtree = db_recurse( &memb_state, pathp, region_start_statepp )) != TREE_NULL )  {
			union tree	tmp;

			/* graft subtree on in place of 'tp' leaf node */
			/* exchange what subtree and tp point at */
			RT_CK_TREE(subtree);
			tmp = *tp;	/* struct copy */
			*tp = *subtree;	/* struct copy */
			db_free_tree( &tmp );
			RT_CK_TREE(tp);
		}
		DB_FULL_PATH_POP(pathp);
		break;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		if( tp->tr_op == OP_SUBTRACT )
			memb_state.ts_sofar |= TS_SOFAR_MINUS;
		else if( tp->tr_op == OP_INTERSECT )
			memb_state.ts_sofar |= TS_SOFAR_INTER;
		recurseit( tp->tr_b.tb_right, &memb_state, pathp, region_start_statepp );
		recurseit( tp->tr_b.tb_left, &memb_state, pathp, region_start_statepp );
		break;

	default:
		bu_log("recurseit: bad op %d\n", tp->tr_op);
		rt_bomb("recurseit\n");
	}
	RT_CK_TREE(tp);
	return;
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
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct combined_tree_state	**region_start_statepp;
{
	struct directory	*dp;
	struct bu_external	ext;
	struct rt_db_internal	intern;
	int			i;
	struct tree_list	*tlp;		/* cur elem of trees[] */
	struct tree_list	*trees = TREE_LIST_NULL;	/* array */
	union tree		*curtree = TREE_NULL;

	RT_CHECK_DBI( tsp->ts_dbip );
	RT_CK_FULL_PATH(pathp);
	RT_INIT_DB_INTERNAL(&intern);

	if( pathp->fp_len <= 0 )  {
		bu_log("db_recurse() null path?\n");
		return(TREE_NULL);
	}
	dp = DB_FULL_PATH_CUR_DIR(pathp);

	if(rt_g.debug&DEBUG_TREEWALK)  {

		char	*sofar = db_path_to_string(pathp);
		bu_log("db_recurse() pathp='%s', tsp=x%x, *statepp=x%x\n",
			sofar, tsp,
			*region_start_statepp );
		rt_free(sofar, "path string");
	}

	/*
	 * Load the entire object into contiguous memory.
	 */
	if( dp->d_addr == RT_DIR_PHONY_ADDR )  return TREE_NULL;
	if( db_get_external( &ext, dp, tsp->ts_dbip ) < 0 )  {
		bu_log("db_recurse() db_get_external() FAIL\n");
		return(TREE_NULL);		/* FAIL */
	}

	if( dp->d_flags & DIR_COMB )  {
		register CONST union record	*rp = (union record *)ext.ext_buf;
		struct rt_comb_internal	*comb;
		struct db_tree_state	nts;
		int			is_region;

		/*  Handle inheritance of material property. */
		nts = *tsp;	/* struct copy */

#if USE_NEW_IMPORT
		if( rt_comb_v4_import( &intern , &ext , NULL ) < 0 )  {
			bu_log("db_recurse() import of %s failed\n", dp->d_namep);
			curtree = TREE_NULL;		/* FAIL */
			goto out;
		}
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB(comb);
#endif

/* XXX USE_NEW_IMPORT */
		/* XXX Can't convert this part yet */
		if( (is_region = db_apply_state_from_comb( &nts, pathp, &ext )) < 0 )  {
			curtree = TREE_NULL;		/* FAIL */
			goto out;
		}

		if( is_region > 0 )  {
			struct combined_tree_state	*ctsp;
			/*
			 *  This is the start of a new region.
			 *  If handler rejects this region, skip on.
			 *  This might be used for ignoring air regions.
			 */
			if( tsp->ts_region_start_func && 
			    tsp->ts_region_start_func( &nts, pathp ) < 0 )  {
				curtree = TREE_NULL;		/* FAIL */
				goto out;
			}

			if( tsp->ts_stop_at_regions )  {
				goto region_end;
			}

			/* Take note of full state here at region start */
			if( *region_start_statepp != (struct combined_tree_state *)0 ) {
				bu_log("db_recurse() ERROR at start of a region, *region_start_statepp = x%x\n",
					*region_start_statepp );
				curtree = TREE_NULL;		/* FAIL */
				goto out;
			}
			ctsp =  db_new_combined_tree_state( &nts, pathp );
			*region_start_statepp = ctsp;
			if(rt_g.debug&DEBUG_TREEWALK)  {
				bu_log("setting *region_start_statepp to x%x\n", ctsp );
				db_pr_combined_tree_state(ctsp);
			}
		}

#if !USE_NEW_IMPORT
		tlp = trees = (struct tree_list *)rt_malloc(
			sizeof(struct tree_list) * (dp->d_len),
			"tree_list array" );

		for( i=1; i < dp->d_len; i++ )  {
			register CONST struct member *mp;
			struct db_tree_state	memb_state;

			memb_state = nts;	/* struct copy */

			mp = &(rp[i].M);

			if( db_apply_state_from_memb( &memb_state, pathp, mp ) < 0 )
				continue;
			/* Member was pushed on pathp stack */

			/* Note & store operation on subtree */
			if( i > 1 )  {
				switch( mp->m_relation )  {
				default:
				bu_log("%s: bad m_relation '%c'\n",
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
			} else {
				/* Handle first one as union */
				tlp->tl_op = OP_UNION;
			}

			/* Recursive call */
			if( (tlp->tl_tree = db_recurse( &memb_state, pathp, region_start_statepp )) != TREE_NULL )  {
				tlp++;
			}

			DB_FULL_PATH_POP(pathp);
		}
		if( tlp <= trees )  {
			/* No subtrees in this region, invent a NOP */
			BU_GETUNION( curtree, tree );
			curtree->magic = RT_TREE_MAGIC;
			curtree->tr_op = OP_NOP;
		} else {
			curtree = db_mkgift_tree( trees, tlp-trees, tsp );
		}
#else
		if( comb->tree )  {
			/* Steal tree from combination, so it won't be freed */
			curtree = comb->tree;
			comb->tree = TREE_NULL;
			if(curtree) RT_CK_TREE(curtree);
			recurseit( curtree, &nts, pathp, region_start_statepp );
			if(curtree) RT_CK_TREE(curtree);
		} else {
			/* No subtrees in this combination, invent a NOP */
			BU_GETUNION( curtree, tree );
			curtree->magic = RT_TREE_MAGIC;
			curtree->tr_op = OP_NOP;
			if(curtree) RT_CK_TREE(curtree);
		}
#endif

region_end:
		if( is_region > 0 )  {
			/*
			 *  This is the end of processing for a region.
			 */
			if( tsp->ts_region_end_func )  {
				curtree = tsp->ts_region_end_func(
					&nts, pathp, curtree );
				if(curtree) RT_CK_TREE(curtree);
			}
		}
		if(curtree) RT_CK_TREE(curtree);
	} else if( dp->d_flags & DIR_SOLID )  {
		int	id;
		vect_t	A, B, C;
		fastf_t	fx, fy, fz;

		/* Get solid ID */
		if( (id = rt_id_solid( &ext )) == ID_NULL )  {
			bu_log("db_recurse(%s): defective database record, addr=x%x\n",
				dp->d_namep,
				dp->d_addr );
			curtree = TREE_NULL;		/* FAIL */
			goto out;
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
			bu_log("db_recurse(%s):  matrix does not preserve axis perpendicularity.\n  X.Y=%g, Y.Z=%g, X.Z=%g\n",
				dp->d_namep, fx, fy, fz );
			mat_print("bad matrix", tsp->ts_mat);
			curtree = TREE_NULL;		/* FAIL */
			goto out;
		}

		if( (tsp->ts_sofar & TS_SOFAR_REGION) == 0 &&
		    tsp->ts_stop_at_regions == 0 )  {
			struct combined_tree_state	*ctsp;
			char	*sofar = db_path_to_string(pathp);
			/*
			 *  Solid is not contained in a region.
		    	 *  "Invent" region info.
		    	 *  Take note of full state here at "region start".
			 */
			if( *region_start_statepp != (struct combined_tree_state *)0 ) {
				bu_log("db_recurse(%s) ERROR at start of a region (bare solid), *region_start_statepp = x%x\n",
					sofar, *region_start_statepp );
				curtree = TREE_NULL;		/* FAIL */
				goto out;
			}
			if( rt_g.debug & DEBUG_REGIONS )  {
			    	bu_log("WARNING: db_recurse(): solid '%s' not contained in a region\n",
			    		sofar );
			}

		    	ctsp = db_new_combined_tree_state( tsp, pathp );
		    	ctsp->cts_s.ts_sofar |= TS_SOFAR_REGION;
			*region_start_statepp = ctsp;
			if(rt_g.debug&DEBUG_TREEWALK)  {
				bu_log("db_recurse(%s): setting *region_start_statepp to x%x (bare solid)\n",
					sofar, ctsp );
				db_pr_combined_tree_state(ctsp);
			}
		    	rt_free( sofar, "path string" );
		}

		/* Hand the solid off for leaf processing */
		if( !tsp->ts_leaf_func )  {
			curtree = TREE_NULL;		/* FAIL */
			goto out;
		}
		curtree = tsp->ts_leaf_func( tsp, pathp, &ext, id );
		if(curtree) RT_CK_TREE(curtree);
	} else {
		bu_log("db_recurse:  %s is neither COMB nor SOLID?\n",
			dp->d_namep );
		curtree = TREE_NULL;
	}
out:
	if( intern.idb_ptr )  rt_comb_ifree( &intern );
	db_free_external( &ext );
	if( trees )  rt_free( (char *)trees, "tree_list array" );
	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		bu_log("db_recurse() return curtree=x%x, pathp='%s', *statepp=x%x\n",
			curtree, sofar,
			*region_start_statepp );
		rt_free(sofar, "path string");
	}
	if(curtree) RT_CK_TREE(curtree);
	return(curtree);
}

/*
 *			D B _ D U P _ S U B T R E E
 */
union tree *
db_dup_subtree( tp )
CONST union tree	*tp;
{
	union tree	*new;

	RT_CK_TREE(tp);
	BU_GETUNION( new, tree );
	*new = *tp;		/* struct copy */

	switch( tp->tr_op )  {
	case OP_NOP:
	case OP_SOLID:
		/* If this is a leaf, done */
		return(new);
	case OP_REGION:
		/* If this is a REGION leaf, dup combined_tree_state & path */
		new->tr_c.tc_ctsp = db_dup_combined_tree_state(
			tp->tr_c.tc_ctsp );
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
		bu_log("db_dup_subtree: bad op %d\n", tp->tr_op);
		rt_bomb("db_dup_subtree\n");
	}
	return( TREE_NULL );
}

/*
 *			D B _ C K _ T R E E
 */
void
db_ck_tree( tp )
CONST union tree	*tp;
{

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {
	case OP_NOP:
		break;
	case OP_SOLID:
		if( tp->tr_a.tu_stp )
			RT_CK_SOLTAB( tp->tr_a.tu_stp );
		break;
	case OP_REGION:
		RT_CK_CTS( tp->tr_c.tc_ctsp );
		break;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		db_ck_tree( tp->tr_b.tb_left );
		break;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		db_ck_tree( tp->tr_b.tb_left );
		db_ck_tree( tp->tr_b.tb_right );
		break;

	default:
		bu_log("db_ck_tree: bad op %d\n", tp->tr_op);
		rt_bomb("db_ck_tree\n");
	}
}

/*
 *			D B _ F R E E _ T R E E
 *
 *  Release all storage associated with node 'tp', including
 *  children nodes.
 */
void
db_free_tree( tp )
union tree	*tp;
{
	RT_CK_TREE(tp);
	switch( tp->tr_op )  {
	case OP_NOP:
		break;

	case OP_SOLID:
		if( tp->tr_a.tu_stp )  {
			register struct soltab	*stp = tp->tr_a.tu_stp;
			RT_CK_SOLTAB(stp);
			tp->tr_a.tu_stp = RT_SOLTAB_NULL;
			rt_free_soltab(stp);
		}
		break;
	case OP_REGION:
		/* REGION leaf, free combined_tree_state & path */
		db_free_combined_tree_state( tp->tr_c.tc_ctsp );
		tp->tr_c.tc_ctsp = (struct combined_tree_state *)0;
		break;

	case OP_NMG_TESS:
		{
			struct nmgregion *r = tp->tr_d.td_r;
			if( tp->tr_d.td_name )  {
				rt_free( (char *)tp->tr_d.td_name, "region name" );
				tp->tr_d.td_name = (CONST char *)NULL;
			}
			if( r == (struct nmgregion *)NULL )  {
				break;
			}
			/* Disposing of the nmg model structue is
			 * left to someone else.
			 * It would be rude to zap all the other regions here.
			 */
#if 0
			if( r->l.magic == (-1L) )  {
				bu_log("db_free_tree: OP_NMG_TESS, r = -1, skipping\n");
			} else if( r->l.magic != NMG_REGION_MAGIC )  {
				/* It may have been freed, and the memory re-used */
				bu_log("db_free_tree: OP_NMG_TESS, bad magic x%x (s/b x%x), skipping\n",
					r->l.magic, NMG_REGION_MAGIC );
			} else {
#endif
			if( r->l.magic == NMG_REGION_MAGIC )
			{
				NMG_CK_REGION(r);
				nmg_kr(r);
			}
			tp->tr_d.td_r = (struct nmgregion *)NULL;
		}
		break;

	case OP_DB_LEAF:
		if( tp->tr_l.tl_mat )  {
			rt_free( (char *)tp->tr_l.tl_mat, "tl_mat" );
			tp->tr_l.tl_mat = NULL;
		}
		rt_free( tp->tr_l.tl_name, "tl_name" );
		tp->tr_l.tl_name = NULL;
		break;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		if( tp->tr_b.tb_left->magic == RT_TREE_MAGIC )
			db_free_tree( tp->tr_b.tb_left );
		tp->tr_b.tb_left = TREE_NULL;
		break;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		if( tp->tr_b.tb_left->magic == RT_TREE_MAGIC )
			db_free_tree( tp->tr_b.tb_left );
		tp->tr_b.tb_left = TREE_NULL;
		if( tp->tr_b.tb_right->magic == RT_TREE_MAGIC )
			db_free_tree( tp->tr_b.tb_right );
		tp->tr_b.tb_right = TREE_NULL;
		break;

	default:
		bu_log("db_free_tree: bad op %d\n", tp->tr_op);
		rt_bomb("db_free_tree\n");
	}
	tp->tr_op = 0;		/* sanity */
	rt_free( (char *)tp, "union tree" );
}

/*
 *			D B _ N O N _ U N I O N _ P U S H
 *
 *  If there are non-union operations in the tree,
 *  above the region nodes, then rewrite the tree so that
 *  the entire tree top is nothing but union operations,
 *  and any non-union operations are clustered down near the region nodes.
 */
void
db_non_union_push( tp )
register union tree	*tp;
{
	RT_CK_TREE(tp);
top:
	switch( tp->tr_op )  {
	case OP_REGION:
	case OP_SOLID:
		/* If this is a leaf, done */
		return;

	case OP_NOP:
		/* This tree has nothing in it, done */
		return;

	case OP_UNION:
		/* This node is known to be a binary op */
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
		BU_GETUNION( rhs, tree );

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
    	bu_log("db_non_union_push() ERROR tree op=%d.?\n", tp->tr_op );
}

/*
 *			D B _ C O U N T _ T R E E _ N O D E S
 *
 *  Return a count of the number of "union tree" nodes below "tp",
 *  including tp.
 */
int
db_count_tree_nodes( tp, count )
register CONST union tree	*tp;
register int			count;
{
	RT_CK_TREE(tp);
	switch( tp->tr_op )  {
	case OP_NOP:
	case OP_SOLID:
	case OP_REGION:
		/* A leaf node */
		return(count+1);

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
		/* This node is known to be a binary op */
		count = db_count_tree_nodes( tp->tr_b.tb_left, count );
		count = db_count_tree_nodes( tp->tr_b.tb_right, count );
		return(count);

	case OP_XOR:
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		/* This node is known to be a unary op */
		count = db_count_tree_nodes( tp->tr_b.tb_left, count );
		return(count);

	default:
		bu_log("db_count_subtree_regions: bad op %d\n", tp->tr_op);
		rt_bomb("db_count_subtree_regions\n");
	}
	return( 0 );
}

/*
 *			D B _ C O U N T _ S U B T R E E _ R E G I O N S
 */
int
db_count_subtree_regions( tp )
CONST union tree	*tp;
{
	int	cnt;

	RT_CK_TREE(tp);
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
	case OP_NOP:
		/* This is as far down as we go -- this is a region top */
		return(1);

	default:
		bu_log("db_count_subtree_regions: bad op %d\n", tp->tr_op);
		rt_bomb("db_count_subtree_regions\n");
	}
	return( 0 );
}

/*
 *			D B _ T A L L Y _ S U B T R E E _ R E G I O N S
 */
int
db_tally_subtree_regions( tp, reg_trees, cur, lim )
union tree	*tp;
union tree	**reg_trees;
int		cur;
int		lim;
{
	union tree	*new;

	RT_CK_TREE(tp);
	if( cur >= lim )  rt_bomb("db_tally_subtree_regions: array overflow\n");

	switch( tp->tr_op )  {
	case OP_NOP:
		return(cur);

	case OP_SOLID:
	case OP_REGION:
		BU_GETUNION( new, tree );
		*new = *tp;		/* struct copy */
		tp->tr_op = OP_NOP;	/* Zap original */
		reg_trees[cur++] = new;
		return(cur);

	case OP_UNION:
		/* This node is known to be a binary op */
		cur = db_tally_subtree_regions( tp->tr_b.tb_left, reg_trees, cur, lim );
		cur = db_tally_subtree_regions( tp->tr_b.tb_right, reg_trees, cur, lim );
		return(cur);

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		/* This is as far down as we go -- this is a region top */
		BU_GETUNION( new, tree );
		*new = *tp;		/* struct copy */
		tp->tr_op = OP_NOP;	/* Zap original */
		reg_trees[cur++] = new;
		return(cur);

	default:
		bu_log("db_tally_subtree_regions: bad op %d\n", tp->tr_op);
		rt_bomb("db_tally_subtree_regions\n");
	}
	return( cur );
}

/* ============================== */

HIDDEN union tree *db_gettree_region_end( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{

	RT_CK_DBI(tsp->ts_dbip);
	RT_CK_FULL_PATH(pathp);

	BU_GETUNION( curtree, tree );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_REGION;
	curtree->tr_c.tc_ctsp = db_new_combined_tree_state( tsp, pathp );

	return(curtree);
}

HIDDEN union tree *db_gettree_leaf( tsp, pathp, ext, id )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct bu_external	*ext;
int			id;
{
	register union tree	*curtree;

	RT_CK_DBI(tsp->ts_dbip);
	RT_CK_FULL_PATH(pathp);

	BU_GETUNION( curtree, tree );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_REGION;
	curtree->tr_c.tc_ctsp = db_new_combined_tree_state( tsp, pathp );

	return(curtree);
}

static struct db_i	*db_dbip;
static union tree	**db_reg_trees;
static int		db_reg_count;
static int		db_reg_current;		/* semaphored when parallel */
static union tree *	(*db_reg_end_func)();
static union tree *	(*db_reg_leaf_func)();

/*
 *			D B _ W A L K _ S U B T R E E
 */
HIDDEN void
db_walk_subtree( tp, region_start_statepp, leaf_func )
register union tree	*tp;
struct combined_tree_state	**region_start_statepp;
union tree		 *(*leaf_func)();
{
	struct combined_tree_state	*ctsp;
	union tree	*curtree;

	RT_CK_TREE(tp);
	switch( tp->tr_op )  {
	case OP_NOP:
		return;

	/*  case OP_SOLID:*/
	case OP_REGION:
		/* Flesh out remainder of subtree */
		ctsp = tp->tr_c.tc_ctsp;
	 	RT_CK_CTS(ctsp);
		if( ctsp->cts_p.fp_len <= 0 )  {
			bu_log("db_walk_subtree() REGION with null path?\n");
			db_free_combined_tree_state( ctsp );
			/* Result is an empty tree */
			tp->tr_op = OP_NOP;
			tp->tr_a.tu_stp = 0;
			return;
		}
		ctsp->cts_s.ts_dbip = db_dbip;
		ctsp->cts_s.ts_stop_at_regions = 0;
		/* All regions will be accepted, in this 2nd pass */
		ctsp->cts_s.ts_region_start_func = 0;
		/* ts_region_end_func() will be called in db_walk_dispatcher() */
		ctsp->cts_s.ts_region_end_func = 0;
		/* Use user's leaf function */
		ctsp->cts_s.ts_leaf_func = leaf_func;

		/* If region already seen, force flag */
		if( *region_start_statepp )
			ctsp->cts_s.ts_sofar |= TS_SOFAR_REGION;
		else
			ctsp->cts_s.ts_sofar &= ~TS_SOFAR_REGION;

		curtree = db_recurse( &ctsp->cts_s, &ctsp->cts_p, region_start_statepp );
		if( curtree == TREE_NULL )  {
			char	*str;
			str = db_path_to_string( &(ctsp->cts_p) );
			bu_log("db_walk_subtree() FAIL on '%s'\n", str);
			rt_free( str, "path string" );

			db_free_combined_tree_state( ctsp );
			/* Result is an empty tree */
			tp->tr_op = OP_NOP;
			tp->tr_a.tu_stp = 0;
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
		db_walk_subtree( tp->tr_b.tb_left, region_start_statepp, leaf_func );
		return;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		db_walk_subtree( tp->tr_b.tb_left, region_start_statepp, leaf_func );
		db_walk_subtree( tp->tr_b.tb_right, region_start_statepp, leaf_func );
		return;

	default:
		bu_log("db_walk_subtree: bad op\n", tp->tr_op);
		rt_bomb("db_walk_subtree\n");
	}
}

/*
 *			D B _ W A L K _ D I S P A T C H E R
 *
 *  This routine handles parallel operation.
 *  There will be at least one, and possibly more, instances of
 *  this routine running simultaneously.
 *
 *  Pick off the next region's tree, and walk it.
 */
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
			bu_log("\n\n***** db_walk_dispatcher() on item %d\n\n", mine );

		if( (curtree = db_reg_trees[mine]) == TREE_NULL )
			continue;
		RT_CK_TREE(curtree);

		/* Walk the full subtree now */
		region_start_statep = (struct combined_tree_state *)0;
		db_walk_subtree( curtree, &region_start_statep, db_reg_leaf_func );

		/*  curtree->tr_op may be OP_NOP here.
		 *  It is up to db_reg_end_func() to deal with this,
		 *  either by discarding it, or making a null region.
		 */
		RT_CK_TREE(curtree);
		if( !region_start_statep )  {
			bu_log("ERROR: db_walk_dispatcher() region %d started with no state\n", mine);
			if( rt_g.debug&DEBUG_TREEWALK )			
				rt_pr_tree( curtree, 0 );
			continue;
		}
		RT_CK_CTS( region_start_statep );

		/* This is a new region */
		if( rt_g.debug&DEBUG_TREEWALK )
			db_pr_combined_tree_state(region_start_statep);

		/*
		 *  reg_end_func() returns a pointer to any unused
		 *  subtree for freeing.
		 */
		if( db_reg_end_func )  {
			db_reg_trees[mine] = (*db_reg_end_func)(
				&(region_start_statep->cts_s),
				&(region_start_statep->cts_p),
				curtree );
		}

		db_free_combined_tree_state( region_start_statep );
	}
}

/*
 *			D B _ W A L K _ T R E E
 *
 *  This is the top interface to the tree walker.
 *
 *  If ncpu > 1, the caller is responsible for making sure that
 *	rt_g.rtg_parallel is non-zero, and that the various
 *	bu_semaphore_init(5)functions have been performed, first.
 *
 *  Returns -
 *	-1	Failure to prepare even a single sub-tree
 *	 0	OK
 */
int
db_walk_tree( dbip, argc, argv, ncpu, init_state, reg_start_func, reg_end_func, leaf_func )
struct db_i	*dbip;
int		argc;
CONST char	**argv;
int		ncpu;
CONST struct db_tree_state *init_state;
int		(*reg_start_func)();
union tree *	(*reg_end_func)();
union tree *	(*leaf_func)();
{
	union tree		*whole_tree = TREE_NULL;
	int			new_reg_count;
	int			i;
	union tree		**reg_trees;	/* (*reg_trees)[] */

	RT_CHECK_DBI(dbip);

	db_dbip = dbip;			/* make global to this module */

	/* Walk each of the given path strings */
	for( i=0; i < argc; i++ )  {
		register union tree	*curtree;
		struct db_tree_state	ts;
		struct db_full_path	path;
		struct combined_tree_state	*region_start_statep;

		ts = *init_state;	/* struct copy */
		ts.ts_dbip = dbip;
		db_full_path_init( &path );

		/* First, establish context from given path */
		if( db_follow_path_for_state( &ts, &path, argv[i], LOOKUP_NOISY ) < 0 )
			continue;	/* ERROR */

		if( path.fp_len <= 0 )  {
			continue;	/* e.g., null combination */
		}

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
		db_free_full_path( &path );
		if( curtree == TREE_NULL )
			continue;	/* ERROR */

		RT_CK_TREE(curtree);
		if( rt_g.debug&DEBUG_TREEWALK )  {
			bu_log("tree after db_recurse():\n");
			rt_pr_tree( curtree, 0 );
		}

		if( whole_tree == TREE_NULL )  {
			whole_tree = curtree;
		} else {
			union tree	*new;

			BU_GETUNION( new, tree );
			new->magic = RT_TREE_MAGIC;
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
		bu_log("tree after db_non_union_push():\n");
		rt_pr_tree( whole_tree, 0 );
	}

	/*
	 *  Build array of sub-tree pointers, one per region,
	 *  for parallel processing below.
	 */
	new_reg_count = db_count_subtree_regions( whole_tree );
	reg_trees = (union tree **)rt_calloc( sizeof(union tree *),
		(new_reg_count+1), "*reg_trees[]" );
	new_reg_count = db_tally_subtree_regions( whole_tree, reg_trees, 0,
		new_reg_count );

	/*  Release storage for tree from whole_tree to leaves.
	 *  db_tally_subtree_regions() duplicated and OP_NOP'ed the original
	 *  top of any sub-trees that it wanted to keep, so whole_tree
	 *  is just the left-over part now.
	 */
	db_free_tree( whole_tree );

	/* As a debugging aid, print out the waiting region names */
	if( rt_g.debug&DEBUG_TREEWALK )  {
		bu_log("%d waiting regions:\n", new_reg_count);
		for( i=0; i < new_reg_count; i++ )  {
			union tree	*treep;
			struct combined_tree_state	*ctsp;
			char	*str;

			if( (treep = reg_trees[i]) == TREE_NULL )  {
				bu_log("%d: NULL\n", i);
				continue;
			}
			RT_CK_TREE(treep);
			if( treep->tr_op != OP_REGION )  {
				bu_log("%d: op=%\n", i, treep->tr_op);
				continue;
			}
			ctsp = treep->tr_c.tc_ctsp;
		 	RT_CK_CTS(ctsp);
			str = db_path_to_string( &(ctsp->cts_p) );
			bu_log("%d '%s'\n", i, str);
			rt_free( str, "path string" );
		}
		bu_log("end of waiting regions\n");
	}

	/*
	 *  Fourth, in parallel, for each region, walk the tree to the leaves.
	 */
	/* Export some state to read-only static variables */
	db_reg_trees = reg_trees;
	db_reg_count = new_reg_count;
	db_reg_current = 0;			/* Semaphored */
	db_reg_end_func = reg_end_func;
	db_reg_leaf_func = leaf_func;

	if( ncpu <= 1 )  {
		db_walk_dispatcher();
	} else {
		/* Ensure that rt_g.rtg_parallel is set */
		/* XXX Should actually be done by bu_parallel(). */
		if( rt_g.rtg_parallel == 0 )  {
			bu_log("db_walk_tree() ncpu=%d, rtg_parallel not set!\n", ncpu);
			rt_g.rtg_parallel = 1;
		}
		bu_parallel( db_walk_dispatcher, ncpu );
	}

	/* Clean up any remaining sub-trees still in reg_trees[] */
	for( i=0; i < new_reg_count; i++ )  {
		if( reg_trees[i] != TREE_NULL )
			db_free_tree( reg_trees[i] );
	}
	rt_free( (char *)reg_trees, "*reg_trees[]" );

	return(0);	/* OK */
}

/*
 *			D B _ P A T H _ T O _ M A T
 */
int
db_path_to_mat( dbip, pathp, mat, depth)
struct db_i	*dbip;
struct db_full_path *pathp;
mat_t mat;
int depth;			/* number of arcs */
{
	register union record	*rp;
	struct directory	*kidp;
	struct directory	*parentp;
	int			i,j;
	mat_t			tmat;

	RT_CHECK_DBI(dbip);
	RT_CK_FULL_PATH(pathp);


	/* XXX case where depth == 0 and pathp->fp_len=2 */


	/*
	 * if depth <= 0 then use the full path.
	 */
	if (depth <= 0) depth = pathp->fp_len-1;
	/*
	 * set depth to the max of depth or path length.
	 */
	if (depth > pathp->fp_len-1) depth = pathp->fp_len-1;

	mat_idn(mat);
	/*
	 * if there is no arc, return ident matrix now
	 */
	if (depth == 0) return 1;

	for (i=0; i < depth; i++) {
		parentp = pathp->fp_names[i];
		kidp = pathp->fp_names[i+1];
		if (!(parentp->d_flags & DIR_COMB)) {
			char *sofar = db_path_to_string(pathp);
			bu_log("db_path_to_mat: '%s' of '%s' not a combination.\n",
			    parentp->d_namep, sofar);
			rt_free(sofar, "path string");
			return 0;
		}

		if (!(rp= db_getmrec(dbip, parentp))) return 0;
		for (j=1; j< parentp->d_len; j++ ) {
			mat_t xmat;	/* temporary matrix */
			int holdlength;

			/* search for the right member */
			if (strcmp(kidp->d_namep, rp[j].M.m_instname) != 0) {
				continue;
			}
			/* convert matrix to fastf_t from disk format */
			rt_mat_dbmat( xmat, rp[j].M.m_mat);
			/*
			 * xmat is the matrix from the disk.
			 * mat is the collection of all operations so far.
			 * (Stack)
			 */
			holdlength = pathp->fp_len;
			pathp->fp_len = i+2;
			db_apply_anims(pathp, kidp, mat, xmat, 0);
			pathp->fp_len = holdlength;
			mat_mul(tmat, mat, xmat);
			mat_copy(mat, tmat);
			break;
		}
		if (j >= parentp->d_len) {
			bu_log("db_path_to_mat: unable to follow %s/%s path\n",
			    parentp->d_namep, kidp->d_namep);
			return 0;
		}
		rt_free((char *) rp, "db_path_to_mat recs");
	}
	return 1;
}

/*
 *			D B _ A P P L Y _ A N I M S
 */
void
db_apply_anims(pathp, dp, stack, arc, materp)
struct db_full_path *pathp;
struct directory *dp;
mat_t	stack;
mat_t	arc;
struct mater_info *materp;
{
	register struct animate *anp;
	register int i,j;

	/* Check here for animation to apply */

	if ((dp->d_animate != ANIM_NULL) && (rt_g.debug & DEBUG_ANIM)) {
		char	*sofar = db_path_to_string(pathp);
		bu_log("Animate %s with...\n", sofar);
		rt_free(sofar, "path string");
	}

	/*
	 * For each of the animations attached to the
	 * mentioned object,  see if the current accumulated
	 * path matches the path  specified in the animation.
	 * Comparison is performed right-to-left (from
	 * leafward to rootward).
	 */
	for( anp = dp->d_animate; anp != ANIM_NULL; anp = anp->an_forw ) {
		register int anim_flag;

		j = pathp->fp_len-1;
		
		RT_CK_ANIMATE(anp);
		i = anp->an_path.fp_len-1;
		anim_flag = 1;

		if (rt_g.debug & DEBUG_ANIM) {
			char	*str;

			str = db_path_to_string( &(anp->an_path) );
			bu_log( "\t%s\t", str );
			rt_free( str, "path string" );
		}

		for( ; i>=0 && j>=0; i--, j-- )  {
			if( anp->an_path.fp_names[i] != pathp->fp_names[j] ) {
				if (rt_g.debug & DEBUG_ANIM) {
					bu_log("%s != %s\n",
					     anp->an_path.fp_names[i]->d_namep,
					     pathp->fp_names[j]->d_namep);
				}
				anim_flag = 0;
				break;
			}
		}

				/* anim, stack, arc, mater */
		if (anim_flag)
			db_do_anim( anp, stack, arc, materp);
	}
	return;
}

/*
 *			D B _ R E G I O N _ M A T
 *
 *  Given the name of a region, return the matrix which maps model coordinates
 *  into "region" coordinates.
 */
void
db_region_mat(m, dbip, name)
mat_t m;
CONST struct db_i *dbip;
CONST char *name;
{
	struct db_full_path		full_path;
	mat_t	region_to_model;

	/* get transformation between world and "region" coordinates */
	if (db_string_to_path( &full_path, dbip, name) ) {
		/* bad thing */
		bu_log("db_string_to_path(%s) error\n", name);
		rt_bomb("error getting path\n");
	}
	if(! db_path_to_mat((struct db_i *)dbip, &full_path, region_to_model, 0)) {
		/* bad thing */
		bu_log("db_path_to_mat(%s) error", name);
		rt_bomb("error getting region coordinate matrix\n");
	}

	/* get matrix to map points from model (world) space
	 * to "region" space
	 */
	mat_inv(m, region_to_model);
}



/*		D B _ S H A D E R _ M A T
 *
 *  Given a region, return a matrix which maps model coordinates into
 *  region "shader space".  This is a space where points in the model
 *  within the bounding box of the region are mapped into "region"
 *  space (the coordinate system in which the region is defined).
 *  The area occupied by the region's bounding box (in region coordinates)
 *  are then mapped into the unit cube.  This unit cube defines
 *  "shader space".
 */
void
db_shader_mat(model_to_shader, rtip, rp, p_min, p_max)
mat_t model_to_shader;
CONST struct rt_i *rtip;
CONST struct region *rp;
point_t p_min;	/* shader/region min point */
point_t p_max;	/* shader/region max point */
{
	mat_t	model_to_region;
	mat_t	m_xlate;
	mat_t	m_scale;
	mat_t	m_tmp;
	vect_t	v_tmp;
	struct	rt_i *my_rtip;
	CONST char	*reg_name;

	reg_name = rt_basename(rp->reg_name);
#ifdef DEBUG_SHADER_MAT
	bu_log("db_shader_mat(%s)\n", rp->reg_name);
#endif
	/* get model-to-region space mapping */
	db_region_mat(model_to_region, rtip->rti_dbip, rp->reg_name);

#ifdef DEBUG_SHADER_MAT
	mat_print("model_to_region", model_to_region);
#endif
	if (VEQUAL(p_min, p_max)) {

		bu_log("db_shader_mat() min/max equal, getting bounding box of \"%s\"\n", reg_name);
		/* User/shader did not specify bounding box,
		 * obtain bounding box for un-transformed region
		 */
		my_rtip = rt_new_rti(rtip->rti_dbip);
		my_rtip->useair = rtip->useair;
		
		/* XXX Should have our own semaphore here */
		bu_semaphore_acquire( RT_SEM_STATS );
		if (rt_gettree(my_rtip, reg_name)) bu_bomb(rp->reg_name);
		bu_semaphore_release( RT_SEM_STATS );
		rt_rpp_region(my_rtip, reg_name, p_min, p_max);
		rt_clean(my_rtip);
	}
#ifdef DEBUG_SHADER_MAT
	bu_log("db_shader_mat(%s) min(%g %g %g) max(%g %g %g)\n", reg_name,
			V3ARGS(p_min), V3ARGS(p_max));
#endif
	/*
	 * Translate bounding box to origin
	 */
	mat_idn(m_xlate);
	VSCALE(v_tmp, p_min, -1);
	MAT_DELTAS_VEC(m_xlate, v_tmp);
	mat_mul(m_tmp, m_xlate, model_to_region);


	/* 
	 * Scale the bounding box to unit cube
	 */
	VSUB2(v_tmp, p_max, p_min);
	VINVDIR(v_tmp, v_tmp);
	mat_idn(m_scale);
	MAT_SCALE_VEC(m_scale, v_tmp);
	mat_mul(model_to_shader, m_scale, m_tmp);
}
