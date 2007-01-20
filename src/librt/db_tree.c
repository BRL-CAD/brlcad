/*                       D B _ T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup dbio */

/*@{*/
/** @file db_tree.c
 *
 * Functions -
 *	db_walk_tree		Parallel tree walker
 *	db_path_to_mat		Given a path, return a matrix.
 *	db_region_mat		Given a name, return a matrix
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"

#include "./debug.h"


BU_EXTERN(void db_ck_tree, (const union tree *tp));

/**
 *			D B _ D U P _ D B _ T R E E _ S T A T E
 *
 *  Duplicate the contents of a db_tree_state structure,
 *  including a private copy of the ts_mater field(s) and the attribute/value set.
 */
void
db_dup_db_tree_state(struct db_tree_state *otsp, const struct db_tree_state *itsp)
{
	int		shader_len=0;
	int		i;

	RT_CK_DBTS(itsp);
	RT_CK_DBI(itsp->ts_dbip);

	*otsp = *itsp;			/* struct copy */

	if( itsp->ts_mater.ma_shader )
		shader_len = strlen( itsp->ts_mater.ma_shader );
	if( shader_len )
	{
		otsp->ts_mater.ma_shader = (char *)bu_malloc( shader_len+1, "db_new_combined_tree_state: ma_shader" );
		strcpy( otsp->ts_mater.ma_shader, itsp->ts_mater.ma_shader );
	}
	else
		otsp->ts_mater.ma_shader = (char *)NULL;

	if( itsp->ts_attrs.count > 0 ) {
		bu_avs_init( &otsp->ts_attrs, itsp->ts_attrs.count, "otsp->ts_attrs" );
		for( i=0 ; i<itsp->ts_attrs.count ; i++ )
			bu_avs_add( &otsp->ts_attrs, itsp->ts_attrs.avp[i].name,
				    itsp->ts_attrs.avp[i].value );
	} else {
		bu_avs_init_empty( &otsp->ts_attrs );
	}
}

/**
 *			D B _ F R E E _ D B _ T R E E _ S T A T E
 *
 *  Release dynamic fields inside the structure, but not the structure itself.
 */
void
db_free_db_tree_state( struct db_tree_state *tsp )
{
	RT_CK_DBTS(tsp);
	RT_CK_DBI(tsp->ts_dbip);
	if( tsp->ts_mater.ma_shader )  {
		bu_free( tsp->ts_mater.ma_shader, "db_free_combined_tree_state: ma_shader" );
		tsp->ts_mater.ma_shader = (char *)NULL;		/* sanity */
	}
	if( tsp->ts_attrs.max > 0 ) {
		bu_avs_free( &tsp->ts_attrs );
		tsp->ts_attrs.avp = (struct bu_attribute_value_pair *)NULL;
	}
	tsp->ts_dbip = (struct db_i *)NULL;			/* sanity */
}

/**
 *			D B _ I N I T _ D B _ T R E E _ S T A T E
 *
 *  In most cases, you don't want to use this routine, you want to
 *  struct copy mged_initial_tree_state or rt_initial_tree_state,
 *  and then set ts_dbip in your copy.
 */
void
db_init_db_tree_state( struct db_tree_state *tsp, struct db_i *dbip, struct resource *resp )
{
	RT_CK_DBI(dbip);
	RT_CK_RESOURCE(resp);

	bzero( (char *)tsp, sizeof(*tsp) );
	tsp->magic = RT_DBTS_MAGIC;
	tsp->ts_dbip = dbip;
	tsp->ts_resp = resp;
	bu_avs_init_empty( &tsp->ts_attrs );
	MAT_IDN( tsp->ts_mat );	/* XXX should use null pointer convention! */
}

/**
 *			D B _ N E W _ C O M B I N E D _ T R E E _ S T A T E
 */
struct combined_tree_state *
db_new_combined_tree_state(register const struct db_tree_state *tsp, register const struct db_full_path *pathp)
{
	struct combined_tree_state	*new;

	RT_CK_DBTS(tsp);
	RT_CK_FULL_PATH(pathp);
	RT_CK_DBI(tsp->ts_dbip);

	BU_GETSTRUCT( new, combined_tree_state );
	new->magic = RT_CTS_MAGIC;
	db_dup_db_tree_state( &(new->cts_s), tsp );
	db_full_path_init( &(new->cts_p) );
	db_dup_full_path( &(new->cts_p), pathp );
	return new;
}

/**
 *			D B _ D U P _ C O M B I N E D _ T R E E _ S T A T E
 */
struct combined_tree_state *
db_dup_combined_tree_state(const struct combined_tree_state *old)
{
	struct combined_tree_state	*new;

 	RT_CK_CTS(old);
	BU_GETSTRUCT( new, combined_tree_state );
	new->magic = RT_CTS_MAGIC;
	db_dup_db_tree_state( &(new->cts_s), &(old->cts_s) );
	db_full_path_init( &(new->cts_p) );
	db_dup_full_path( &(new->cts_p), &(old->cts_p) );
	return new;
}

/**
 *			D B _ F R E E _ C O M B I N E D _ T R E E _ S T A T E
 */
void
db_free_combined_tree_state(register struct combined_tree_state *ctsp)
{
 	RT_CK_CTS(ctsp);
	db_free_full_path( &(ctsp->cts_p) );
	db_free_db_tree_state( &(ctsp->cts_s) );
	bzero( (char *)ctsp, sizeof(*ctsp) );		/* sanity */
	bu_free( (char *)ctsp, "combined_tree_state");
}

/**
 *			D B _ P R _ T R E E _ S T A T E
 */
void
db_pr_tree_state(register const struct db_tree_state *tsp)
{
	int i;

	RT_CK_DBTS(tsp);

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
	bu_log(" ts_mater.ma_temperature=%g K\n", tsp->ts_mater.ma_temperature);
	bu_log(" ts_mater.ma_shader=%s\n", tsp->ts_mater.ma_shader ? tsp->ts_mater.ma_shader : "" );
	for( i=0 ; i<tsp->ts_attrs.count ; i++ ) {
		bu_log( "\t%s = %s\n", tsp->ts_attrs.avp[i].name, tsp->ts_attrs.avp[i].value );
	}
	bn_mat_print("ts_mat", tsp->ts_mat );
	bu_log(" ts_resp=x%x\n", tsp->ts_resp );
}

/**
 *			D B _ P R _ C O M B I N E D _ T R E E _ S T A T E
 */
void
db_pr_combined_tree_state(register const struct combined_tree_state *ctsp)
{
	char	*str;

 	RT_CK_CTS(ctsp);
	bu_log("db_pr_combined_tree_state(x%x):\n", ctsp);
	db_pr_tree_state( &(ctsp->cts_s) );
	str = db_path_to_string( &(ctsp->cts_p) );
	bu_log(" path='%s'\n", str);
	bu_free( str, "path string" );
}

/**
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
db_apply_state_from_comb(struct db_tree_state *tsp, const struct db_full_path *pathp, register const struct rt_comb_internal *comb)
{
	RT_CK_DBTS(tsp);
	RT_CK_COMB(comb);

	if( comb->rgb_valid == 1 )  {
		if( tsp->ts_sofar & TS_SOFAR_REGION )  {
			if( (tsp->ts_sofar&(TS_SOFAR_MINUS|TS_SOFAR_INTER)) == 0 )  {
				/* This combination is within a region */
				char	*sofar = db_path_to_string(pathp);

				bu_log("db_apply_state_from_comb(): WARNING: color override in combination within region '%s', ignored\n",
					sofar );
				bu_free(sofar, "path string");
			}
			/* Just quietly ignore it -- it's being subtracted off */
		} else if( tsp->ts_mater.ma_cinherit == 0 )  {
			/* DB_INH_LOWER was set -- lower nodes in tree override */
			tsp->ts_mater.ma_color_valid = 1;
			tsp->ts_mater.ma_color[0] =
				(((float)(comb->rgb[0]))*bn_inv255);
			tsp->ts_mater.ma_color[1] =
				(((float)(comb->rgb[1]))*bn_inv255);
			tsp->ts_mater.ma_color[2] =
				(((float)(comb->rgb[2]))*bn_inv255);
			/* Track further inheritance as specified by this combination */
			tsp->ts_mater.ma_cinherit = comb->inherit;
		}
	}
	if( comb->temperature > 0 )  {
		if( tsp->ts_sofar & TS_SOFAR_REGION )  {
			if( (tsp->ts_sofar&(TS_SOFAR_MINUS|TS_SOFAR_INTER)) == 0 )  {
				/* This combination is within a region */
				char	*sofar = db_path_to_string(pathp);

				bu_log("db_apply_state_from_comb(): WARNING: temperature in combination below region '%s', ignored\n",
					sofar );
				bu_free(sofar, "path string");
			}
			/* Just quietly ignore it -- it's being subtracted off */
		} else if( tsp->ts_mater.ma_minherit == 0 )  {
			/* DB_INH_LOWER -- lower nodes in tree override */
			tsp->ts_mater.ma_temperature = comb->temperature;
		}
	}
	if( bu_vls_strlen( &comb->shader ) > 0 )  {
		if( tsp->ts_sofar & TS_SOFAR_REGION )  {
			if( (tsp->ts_sofar&(TS_SOFAR_MINUS|TS_SOFAR_INTER)) == 0 )  {
				/* This combination is within a region */
				char	*sofar = db_path_to_string(pathp);

				bu_log("db_apply_state_from_comb(): WARNING: material property spec in combination below region '%s', ignored\n",
					sofar );
				bu_free(sofar, "path string");
			}
			/* Just quietly ignore it -- it's being subtracted off */
		} else if( tsp->ts_mater.ma_minherit == 0 )  {
			struct bu_vls tmp_vls;

			/* DB_INH_LOWER -- lower nodes in tree override */
			if( tsp->ts_mater.ma_shader )
				bu_free( (genptr_t)tsp->ts_mater.ma_shader, "ma_shader" );

			bu_vls_init( &tmp_vls );
			if( bu_shader_to_key_eq( bu_vls_addr( &comb->shader ), &tmp_vls ) )
			{
				char *sofar = db_path_to_string(pathp);

				bu_log( "db_apply_state_from_comb: Warning: bad shader in %s (ignored):\n", sofar );
				bu_vls_free( &tmp_vls );
				bu_free(sofar, "path string");
				tsp->ts_mater.ma_shader = (char *)NULL;
			}
			else
			{
				tsp->ts_mater.ma_shader = bu_vls_strdup( &tmp_vls );
				bu_vls_free( &tmp_vls );
			}
			tsp->ts_mater.ma_minherit = comb->inherit;
		}
	}

	/* Handle combinations which are the top of a "region" */
	if( comb->region_flag )  {
		if( tsp->ts_sofar & TS_SOFAR_REGION )  {
			if( (tsp->ts_sofar&(TS_SOFAR_MINUS|TS_SOFAR_INTER)) == 0 )  {
				char	*sofar = db_path_to_string(pathp);
				bu_log("Warning:  region unioned into region at '%s', lower region info ignored\n",
					sofar);
				bu_free(sofar, "path string");
			}
			/* Go on as if it was not a region */
		} else {
			/* This starts a new region */
			tsp->ts_sofar |= TS_SOFAR_REGION;
			tsp->ts_regionid = comb->region_id;
			tsp->ts_is_fastgen = comb->is_fastgen;
			tsp->ts_aircode = comb->aircode;
			tsp->ts_gmater = comb->GIFTmater;
			tsp->ts_los = comb->los;
			return(1);	/* Success, this starts new region */
		}
	}
	return(0);	/* Success */
}

/**
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
db_apply_state_from_memb(struct db_tree_state *tsp, struct db_full_path *pathp, const union tree *tp)
{
	register struct directory *mdp;
	mat_t			xmat;
	mat_t			old_xlate;

	RT_CK_DBTS(tsp);
	RT_CK_FULL_PATH(pathp);
	RT_CK_TREE(tp);

	if( (mdp = db_lookup( tsp->ts_dbip, tp->tr_l.tl_name, LOOKUP_QUIET )) == DIR_NULL )  {
		char	*sofar = db_path_to_string(pathp);
		bu_log("db_lookup(%s) failed in %s\n", tp->tr_l.tl_name, sofar);
		bu_free(sofar, "path string");
		return -1;
	}

	db_add_node_to_full_path( pathp, mdp );

	MAT_COPY( old_xlate, tsp->ts_mat );
	if( tp->tr_l.tl_mat ) {
		MAT_COPY( xmat, tp->tr_l.tl_mat );
	}
	else {
		MAT_IDN( xmat );
	}

	/*  If the owning region it above this node in the tree,
	 *  it is not possible to animation region-material properties
	 *  lower down in the arc.  So note by sending a NULL pointer.
	 */
	db_apply_anims( pathp, mdp, old_xlate, xmat,
		(tsp->ts_sofar & TS_SOFAR_REGION) ?
			(struct mater_info *)NULL :
			&tsp->ts_mater );

	bn_mat_mul(tsp->ts_mat, old_xlate, xmat);

	return(0);		/* Success */
}

/**
 *		D B _ A P P L Y _ S T A T E _ F R O M _ O N E _ M E M B E R
 *
 *  Returns -
 *	-1	found member, failed to apply state
 *	 0	unable to find member 'cp'
 *	 1	state applied OK
 */
int
db_apply_state_from_one_member(
	struct db_tree_state *tsp,
	struct db_full_path *pathp,
	const char *cp,
	int sofar,
	const union tree *tp )
{
	int	ret;

	RT_CK_DBTS(tsp);
	RT_CHECK_DBI( tsp->ts_dbip );
	RT_CK_FULL_PATH( pathp );
	RT_CK_TREE(tp);

	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		if( strcmp( cp, tp->tr_l.tl_name ) != 0 )
			return 0;		/* NO-OP */
		tsp->ts_sofar |= sofar;
		if( db_apply_state_from_memb( tsp, pathp, tp ) < 0 )
			return -1;		/* FAIL */
		return 1;			/* success */

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		ret = db_apply_state_from_one_member( tsp, pathp, cp, sofar,
			tp->tr_b.tb_left );
		if( ret != 0 )  return ret;
		if( tp->tr_op == OP_SUBTRACT )
			sofar |= TS_SOFAR_MINUS;
		else if( tp->tr_op == OP_INTERSECT )
			sofar |= TS_SOFAR_INTER;
		return db_apply_state_from_one_member( tsp, pathp, cp, sofar,
			tp->tr_b.tb_right );

	default:
		bu_log("db_apply_state_from_one_member: bad op %d\n", tp->tr_op);
		bu_bomb("db_apply_state_from_one_member\n");
	}
	return -1;
}

/**
 *			D B _ F I N D _ N A M E D _ L E A F
 *
 *  The search stops on the first match.
 *
 *  Returns -
 *	tp		if found
 *	TREE_NULL	if not found in this tree
 */
union tree *
db_find_named_leaf( union tree *tp, const char *cp )
{
	union tree	*ret;

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		if( strcmp( cp, tp->tr_l.tl_name )  )
			return TREE_NULL;
		return tp;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		ret = db_find_named_leaf( tp->tr_b.tb_left, cp );
		if( ret != TREE_NULL )  return ret;
		return db_find_named_leaf( tp->tr_b.tb_right, cp );

	default:
		bu_log("db_find_named_leaf: bad op %d\n", tp->tr_op);
		bu_bomb("db_find_named_leaf\n");
	}
	return TREE_NULL;
}

/**
 *			D B _ F I N D _ N A M E D _ L E A F S _ P A R E N T
 *
 *  The search stops on the first match.
 *
 *  Returns -
 *	TREE_NULL	if not found in this tree
 *	tp		if found
 *			*side == 1 if leaf is on lhs.
 *			*side == 2 if leaf is on rhs.
 *
 */
union tree *
db_find_named_leafs_parent( int *side, union tree *tp, const char *cp )
{
	union tree	*ret;

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		/* Always return NULL -- we are seeking parent, not leaf */
		return TREE_NULL;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		if( tp->tr_b.tb_left->tr_op == OP_DB_LEAF )  {
			if( strcmp( cp, tp->tr_b.tb_left->tr_l.tl_name ) == 0 )  {
				*side = 1;
				return tp;
			}
		}
		if( tp->tr_b.tb_right->tr_op == OP_DB_LEAF )  {
			if( strcmp( cp, tp->tr_b.tb_right->tr_l.tl_name ) == 0 )  {
				*side = 2;
				return tp;
			}
		}

		/* If target not on immediate children, descend down. */
		ret = db_find_named_leafs_parent( side, tp->tr_b.tb_left, cp );
		if( ret != TREE_NULL )  return ret;
		return db_find_named_leafs_parent( side, tp->tr_b.tb_right, cp );

	default:
		bu_log("db_find_named_leafs_parent: bad op %d\n", tp->tr_op);
		bu_bomb("db_find_named_leafs_parent\n");
	}
	return TREE_NULL;
}

/**
 *			D B _ T R E E _ D E L _ L H S
 */
void
db_tree_del_lhs( union tree *tp, struct resource *resp )
{
	union tree	*subtree;

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {

	default:
		bu_bomb("db_tree_del_lhs() called with leaf node as parameter\n");

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		switch( tp->tr_b.tb_left->tr_op )  {
		case OP_NOP:
		case OP_SOLID:
		case OP_REGION:
		case OP_NMG_TESS:
		case OP_DB_LEAF:
			/* lhs is indeed a leaf node */
			db_free_tree( tp->tr_b.tb_left, resp );
			tp->tr_b.tb_left = TREE_NULL;	/* sanity */
			subtree = tp->tr_b.tb_right;
			/*
			 *  Since we don't know what node has the downpointer
			 *  to 'tp', replicate 'subtree' data in 'tp' node,
			 *  then release memory of 'subtree' node
			 *  (but not the actual subtree).
			 */
			*tp = *subtree;			/* struct copy */
			RT_FREE_TREE( subtree, resp );
			return;
		default:
			bu_bomb("db_tree_del_lhs()  lhs is not a leaf node\n");
		}
	}
}

/**
 *			D B _ T R E E _ D E L _ R H S
 */
void
db_tree_del_rhs( union tree *tp, struct resource *resp )
{
	union tree	*subtree;

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {

	default:
		bu_bomb("db_tree_del_rhs() called with leaf node as parameter\n");

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		switch( tp->tr_b.tb_right->tr_op )  {
		case OP_NOP:
		case OP_SOLID:
		case OP_REGION:
		case OP_NMG_TESS:
		case OP_DB_LEAF:
			/* rhs is indeed a leaf node */
			db_free_tree( tp->tr_b.tb_right, resp );
			tp->tr_b.tb_right = TREE_NULL;	/* sanity */
			subtree = tp->tr_b.tb_left;
			/*
			 *  Since we don't know what node has the downpointer
			 *  to 'tp', replicate 'subtree' data in 'tp' node,
			 *  then release memory of 'subtree' node
			 *  (but not the actual subtree).
			 */
			*tp = *subtree;			/* struct copy */
			RT_FREE_TREE( subtree, resp );
			return;
		default:
			bu_bomb("db_tree_del_rhs()  rhs is not a leaf node\n");
		}
	}
}

/**
 *			D B _ T R E E _ D E L _ D B L E A F
 *
 *  Given a name presumably referenced in a OP_DB_LEAF node,
 *  delete that node, and the operation node that references it.
 *  Not that this may not produce an equivalant tree,
 *  for example when rewriting (A - subtree) as (subtree),
 *  but that will be up to the caller/user to adjust.
 *  This routine gets rid of exactly two nodes in the tree: leaf, and op.
 *  Use some other routine if you wish to kill the entire rhs
 *  below "-" and "intersect" nodes.
 *
 *  The two nodes deleted will have their memory freed.
 *
 *  If the tree is a single OP_DB_LEAF node, the leaf is freed and
 *  *tp is set to NULL.
 *
 *  Returns -
 *	-3	Internal error
 *	-2	Tree is empty
 *	-1	Unable to find OP_DB_LEAF node specified by 'cp'.
 *	 0	OK
 */
int
db_tree_del_dbleaf(union tree **tp, const char *cp, struct resource *resp)
{
	union tree	*parent;
	int		side = 0;

	if( *tp == TREE_NULL )  return -1;

	RT_CK_TREE(*tp);
	RT_CK_RESOURCE(resp);

	if( (parent = db_find_named_leafs_parent( &side, *tp, cp )) == TREE_NULL )  {
		/* Perhaps the root of the tree is the named leaf? */
		if( (*tp)->tr_op == OP_DB_LEAF &&
		    strcmp( cp, (*tp)->tr_l.tl_name ) == 0 )  {
		    	db_free_tree( *tp, resp );
		    	*tp = TREE_NULL;
		    	return 0;
		}
		return -2;
	}

	switch( side )  {
	case 1:
		db_tree_del_lhs( parent, resp );
		(void)db_tree_del_dbleaf( tp, cp, resp );	/* recurse for extras */
		return 0;
	case 2:
		db_tree_del_rhs( parent, resp );
		(void)db_tree_del_dbleaf( tp, cp, resp );	/* recurse for extras */
		return 0;
	}
	bu_log("db_tree_del_dbleaf() unknown side=%d?\n", side);
	return -3;
}

/**
 *			D B _ T R E E _ M U L _ D B L E A F
 *
 *  Multiply on the left every matrix found in a DB_LEAF node in a tree.
 */
void
db_tree_mul_dbleaf( union tree *tp, const mat_t mat )
{
	mat_t	temp;

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		if( tp->tr_l.tl_mat == NULL )  {
			tp->tr_l.tl_mat = bn_mat_dup(mat);
			return;
		}
		bn_mat_mul( temp, mat, tp->tr_l.tl_mat );
		MAT_COPY( tp->tr_l.tl_mat, temp );
		break;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		db_tree_mul_dbleaf( tp->tr_b.tb_left, mat );
		db_tree_mul_dbleaf( tp->tr_b.tb_right, mat );
		break;

	default:
		bu_log("db_tree_mul_dbleaf: bad op %d\n", tp->tr_op);
		bu_bomb("db_tree_mul_dbleaf\n");
	}
}

/**
 *			D B _ T R E E _ F U N C L E A F
 *
 *	This routine traverses a combination (union tree) in LNR order
 *	and calls the provided function for each OP_DB_LEAF node.
 *	Note that this routine does not go outside this one
 *	combination!!!!
 *	was comb_functree().
 */
void
db_tree_funcleaf(
	struct db_i		*dbip,
	struct rt_comb_internal	*comb,
	union tree		*comb_tree,
	void			(*leaf_func)(),
	genptr_t		user_ptr1,
	genptr_t		user_ptr2,
	genptr_t		user_ptr3 )
{
	RT_CK_DBI( dbip );

	if( !comb_tree )
		return;

	RT_CK_TREE( comb_tree );

	switch( comb_tree->tr_op )
	{
		case OP_DB_LEAF:
			(*leaf_func)( dbip, comb, comb_tree, user_ptr1, user_ptr2, user_ptr3 );
			break;
		case OP_UNION:
		case OP_INTERSECT:
		case OP_SUBTRACT:
		case OP_XOR:
			db_tree_funcleaf( dbip, comb, comb_tree->tr_b.tb_left, leaf_func, user_ptr1, user_ptr2, user_ptr3 );
			db_tree_funcleaf( dbip, comb, comb_tree->tr_b.tb_right, leaf_func, user_ptr1, user_ptr2, user_ptr3 );
			break;
		default:
			bu_log( "db_tree_funcleaf: bad op %d\n", comb_tree->tr_op );
			bu_bomb( "db_tree_funcleaf: bad op\n" );
			break;
	}
}

/**
 *			D B _ F O L L O W _ P A T H
 *
 *  Starting with possible prior partial path and corresponding accumulated state,
 *  follow the path given by "new_path", updating
 *  *tsp and *total_path with full state information along the way.
 *  In a better world, there would have been a "combined_tree_state" arg.
 *
 *  Parameter 'depth' controls how much of 'new_path' is used:
 *	0	use all of new_path
 *	>0	use only this many of the first elements of the path
 *	<0	use all but this many path elements.
 *
 *  A much more complete version of rt_plookup() and pathHmat().
 *  There is also a TCL interface.
 *
 *  Returns -
 *	 0	success (plus *tsp is updated)
 *	-1	error (*tsp values are not useful)
 */
int
db_follow_path(
	struct db_tree_state		*tsp,
	struct db_full_path		*total_path,
	const struct db_full_path	*new_path,
	int				noisy,
	int				depth )		/* # arcs in new_path to use */
{
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	struct directory	*comb_dp;	/* combination's dp */
	struct directory	*dp;		/* element's dp */
	int			j;

	RT_CK_DBTS(tsp);
	RT_CHECK_DBI( tsp->ts_dbip );
	RT_CK_FULL_PATH( total_path );
	RT_CK_FULL_PATH( new_path );
	RT_CK_RESOURCE( tsp->ts_resp );

	if(RT_G_DEBUG&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(total_path);
		char	*toofar = db_path_to_string(new_path);
		bu_log("db_follow_path() total_path='%s', tsp=x%x, new_path='%s', noisy=%d, depth=%d\n",
			sofar, tsp, toofar, noisy, depth );
		bu_free(sofar, "path string");
		bu_free(toofar, "path string");
	}

	if( depth < 0 )  {
		depth = new_path->fp_len-1 + depth;
		if( depth < 0 ) rt_bomb("db_follow_path() depth exceeded provided path\n");
	} else if( depth >= new_path->fp_len )  {
		depth = new_path->fp_len-1;
	} else if( depth == 0 )  {
		/* depth of zero means "do it all". */
		depth = new_path->fp_len-1;
	}

	j = 0;

	/* Get the first combination */
	if( total_path->fp_len > 0 )  {
		/* Some path has already been processed */
		comb_dp = DB_FULL_PATH_CUR_DIR(total_path);
	}  else  {
		/* No prior path. Process any animations located at the root */
		comb_dp = DIR_NULL;
		dp = new_path->fp_names[0];
		RT_CK_DIR(dp);

		if( tsp->ts_dbip->dbi_anroot )  {
			register struct animate *anp;
			mat_t	old_xlate, xmat;

			for( anp=tsp->ts_dbip->dbi_anroot; anp != ANIM_NULL; anp = anp->an_forw ) {
				RT_CK_ANIMATE(anp);
				if( dp != anp->an_path.fp_names[0] )
					continue;
				MAT_COPY( old_xlate, tsp->ts_mat );
				MAT_IDN( xmat );
				db_do_anim( anp, old_xlate, xmat, &(tsp->ts_mater) );
				bn_mat_mul( tsp->ts_mat, old_xlate, xmat );
			}
		}

		/* Put first element on output path, either way */
		db_add_node_to_full_path( total_path, dp );

		if( (dp->d_flags & DIR_COMB) == 0 )  goto is_leaf;

		/* Advance to next path element */
		j = 1;
		comb_dp = dp;
	}
	/*
	 *  Process two things at once:
	 *  the combination at [j], and it's member at [j+1].
	 */
	do  {
		/* j == depth is the last one, presumably a leaf */
		if( j > depth )  break;
		dp = new_path->fp_names[j];
		RT_CK_DIR(dp);

		if( (comb_dp->d_flags & DIR_COMB) == 0 )  {
			bu_log("db_follow_path() %s isn't combination\n", comb_dp->d_namep);
			goto fail;
		}

		/* At this point, comb_db is the comb, dp is the member */
		if(RT_G_DEBUG&DEBUG_TREEWALK)  {
			bu_log("db_follow_path() at %s/%s\n", comb_dp->d_namep, dp->d_namep );
		}

		/* Load the combination object into memory */
		if( rt_db_get_internal( &intern, comb_dp, tsp->ts_dbip, NULL, tsp->ts_resp ) < 0 )
			goto fail;
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB(comb);
		if( db_apply_state_from_comb( tsp, total_path, comb ) < 0 )
			goto fail;

		/* Crawl tree searching for specified leaf */
		if( db_apply_state_from_one_member( tsp, total_path, dp->d_namep, 0, comb->tree ) <= 0 )  {
			bu_log("db_follow_path() ERROR: unable to apply member %s state\n", dp->d_namep);
			goto fail;
		}
		/* Found it, state has been applied, sofar applied,
		 * member's directory entry pushed onto total_path
		 */
		rt_db_free_internal( &intern, tsp->ts_resp );

		/* If member is a leaf, handle leaf processing too. */
		if( (dp->d_flags & DIR_COMB) == 0 )  {
is_leaf:
			/* Object is a leaf */
			if( j == new_path->fp_len-1 )  {
				/* No more path was given, all is well */
				goto out;
			}
			/* Additional path was given, this is wrong */
			if( noisy )  {
				char	*sofar = db_path_to_string(total_path);
				char	*toofar = db_path_to_string(new_path);
				bu_log("db_follow_path() ERROR: path ended in leaf at '%s', additional path specified '%s'\n",
					sofar, toofar );
				bu_free(sofar, "path string");
				bu_free(toofar, "path string");
			}
			goto fail;
		}

		/* Advance to next path element */
		j++;
		comb_dp = dp;
	} while( j <= depth );

out:
	if(RT_G_DEBUG&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(total_path);
		bu_log("db_follow_path() returns total_path='%s'\n",
			sofar);
		bu_free(sofar, "path string");
	}
	return 0;		/* SUCCESS */
fail:
	return -1;		/* FAIL */
}

/**
 *			D B _ F O L L O W _ P A T H _ F O R _ S T A T E
 *
 *  Follow the slash-separated path given by "cp", and update
 *  *tsp and *total_path with full state information along the way.
 *
 *  A much more complete version of rt_plookup().
 *
 *  Returns -
 *	 0	success (plus *tsp is updated)
 *	-1	error (*tsp values are not useful)
 */
int
db_follow_path_for_state(struct db_tree_state *tsp, struct db_full_path *total_path, const char *orig_str, int noisy)
{
	struct db_full_path	new_path;
	int			ret;

	RT_CK_DBTS(tsp);

	if( *orig_str == '\0' )  return 0;		/* Null string */

	if( db_string_to_path( &new_path, tsp->ts_dbip, orig_str ) < 0 )
		return -1;

	if( new_path.fp_len <= 0 )  return 0;		/* Null string */

	ret = db_follow_path( tsp, total_path, &new_path, noisy, 0 );
	db_free_full_path( &new_path );

	return ret;
}

/**
 *   D B _ D E T E C T _ C Y C L E
 *
 *  Helper routine to detect cyclic references
 */
HIDDEN int
db_detect_cycle( struct db_full_path *pathp, union tree *tp )
{
    /* skip the last one added since it is currently being tested. */
    long int depth = pathp->fp_len - 1;

    /* check the path to see if it is groundhog day */
    while (--depth >= 0) {
	if (strcmp(tp->tr_l.tl_name, pathp->fp_names[depth]->d_namep) == 0) {
	    return 1;
	}
    }

    /* not found */
    return 0;
}

/**
 *			D B _ R E C U R S E _ S U B T R E E
 *
 *  Helper routine for db_recurse()
 */
HIDDEN void
db_recurse_subtree(union tree *tp, struct db_tree_state *msp, struct db_full_path *pathp, struct combined_tree_state **region_start_statepp, genptr_t client_data)
{
	struct db_tree_state	memb_state;
	union tree		*subtree;

	RT_CK_TREE(tp);
	RT_CK_DBTS(msp);
	RT_CK_RESOURCE(msp->ts_resp);
	db_dup_db_tree_state( &memb_state, msp );

	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		if( db_apply_state_from_memb( &memb_state, pathp, tp ) < 0 )  {
			/* Lookup of this leaf failed, NOP it out. */
			if( tp->tr_l.tl_mat )  {
				bu_free( (char *)tp->tr_l.tl_mat, "tl_mat" );
				tp->tr_l.tl_mat = NULL;
			}
			bu_free( tp->tr_l.tl_name, "tl_name" );
			tp->tr_l.tl_name = NULL;
			tp->tr_op = OP_NOP;
			goto out;
		}

		/* protect against cyclic geometry */
		if ( db_detect_cycle( pathp, tp ) ) {
		    int depth = pathp->fp_len;

		    bu_log("Detected cyclic reference of %s\nPath stack is:\n", tp->tr_l.tl_name);
		    while (--depth >=0) {
			bu_log("\tPath depth %d is %s\n", depth, pathp->fp_names[depth]->d_namep);
		    }
		    bu_log("WARNING: skipping the cyclic reference lookup\n");

		    /* Lookup of this leaf resulted in a cyclic reference, NOP it out */
		    if( tp->tr_l.tl_mat )  {
			bu_free( (char *)tp->tr_l.tl_mat, "tl_mat" );
			tp->tr_l.tl_mat = NULL;
		    }
		    bu_free( tp->tr_l.tl_name, "tl_name" );
		    tp->tr_l.tl_name = NULL;
		    tp->tr_op = OP_NOP;
		    goto out;
		}

		/* Recursive call */
		if( (subtree = db_recurse( &memb_state, pathp, region_start_statepp, client_data )) != TREE_NULL )  {
			union tree	*tmp;

			/* graft subtree on in place of 'tp' leaf node */
			/* exchange what subtree and tp point at */
			RT_GET_TREE( tmp, msp->ts_resp );
			RT_CK_TREE(subtree);
			*tmp = *tp;	/* struct copy */
			*tp = *subtree;	/* struct copy */
			RT_FREE_TREE( subtree, msp->ts_resp );

			db_free_tree( tmp, msp->ts_resp );
			RT_CK_TREE(tp);
		} else {
			/* Processing of this leaf failed, NOP it out. */
			if( tp->tr_l.tl_mat )  {
				bu_free( (char *)tp->tr_l.tl_mat, "tl_mat" );
				tp->tr_l.tl_mat = NULL;
			}
			bu_free( tp->tr_l.tl_name, "tl_name" );
			tp->tr_l.tl_name = NULL;
			tp->tr_op = OP_NOP;
		}
		DB_FULL_PATH_POP(pathp);
		break;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		db_recurse_subtree( tp->tr_b.tb_left, &memb_state, pathp, region_start_statepp, client_data );
		if( tp->tr_op == OP_SUBTRACT )
			memb_state.ts_sofar |= TS_SOFAR_MINUS;
		else if( tp->tr_op == OP_INTERSECT )
			memb_state.ts_sofar |= TS_SOFAR_INTER;
		db_recurse_subtree( tp->tr_b.tb_right, &memb_state, pathp, region_start_statepp, client_data );
		break;

	default:
		bu_log("db_recurse_subtree: bad op %d\n", tp->tr_op);
		rt_bomb("db_recurse_subtree\n");
	}
out:
	db_free_db_tree_state( &memb_state );
	RT_CK_TREE(tp);
	return;
}

/**
 *			D B _ R E C U R S E
 *
 *  Recurse down the tree, finding all the leaves
 *  (or finding just all the regions).
 *
 *  ts_region_start_func() is called to permit regions to be skipped.
 *  It is not intended to be used for collecting state.
 */
union tree *
db_recurse(struct db_tree_state *tsp, struct db_full_path *pathp, struct combined_tree_state **region_start_statepp, genptr_t client_data)
{
	struct directory	*dp;
	struct rt_db_internal	intern;
	union tree		*curtree = TREE_NULL;
	static long int		depth = 0;

	RT_CK_DBTS(tsp);
	RT_CHECK_DBI( tsp->ts_dbip );
	RT_CK_RESOURCE(tsp->ts_resp);
	RT_CK_FULL_PATH(pathp);
	RT_INIT_DB_INTERNAL(&intern);

	if( pathp->fp_len <= 0 )  {
		bu_log("db_recurse() null path?\n");
		return(TREE_NULL);
	}
	dp = DB_FULL_PATH_CUR_DIR(pathp);

	if(RT_G_DEBUG&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		bu_log("db_recurse() pathp='%s', tsp=x%x, *statepp=x%x, tsp->ts_sofar=%d\n",
			sofar, tsp,
			*region_start_statepp, tsp->ts_sofar );
		bu_free(sofar, "path string");
		bn_mat_ck("db_recurse() tsp->ts_mat at start", tsp->ts_mat);
	}

	/*
	 * Load the entire object into contiguous memory.
	 * Note that this code depends on the d_flags being set properly.
	 */
	if( dp->d_addr == RT_DIR_PHONY_ADDR )  return TREE_NULL;

	if( dp->d_flags & DIR_COMB )  {
		struct rt_comb_internal	*comb;
		struct db_tree_state	nts;
		int			is_region;

		if( rt_db_get_internal( &intern, dp, tsp->ts_dbip, NULL, tsp->ts_resp ) < 0 )  {
			bu_log("db_recurse() rt_db_get_internal(%s) FAIL\n", dp->d_namep);
			curtree = TREE_NULL;		/* FAIL */
			goto out;
		}

		/*  Handle inheritance of material property. */
		db_dup_db_tree_state( &nts, tsp );

		comb = (struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB(comb);
		if( (is_region = db_apply_state_from_comb( &nts, pathp, comb )) < 0 )  {
			db_free_db_tree_state( &nts );
			curtree = TREE_NULL;		/* FAIL */
			goto out;
		}

		if( is_region > 0 )  {
			struct combined_tree_state	*ctsp;

			/* get attribute/value structure */
			bu_avs_merge( &nts.ts_attrs, &intern.idb_avs );

			/*
			 *  This is the start of a new region.
			 *  If handler rejects this region, skip on.
			 *  This might be used for ignoring air regions.
			 */
			if( tsp->ts_region_start_func &&
			    tsp->ts_region_start_func( &nts, pathp, comb, client_data ) < 0 )  {
				if(RT_G_DEBUG&DEBUG_TREEWALK)  {
					char	*sofar = db_path_to_string(pathp);
					bu_log("db_recurse() ts_region_start_func deletes %s\n",
						sofar);
					bu_free(sofar, "path string");
				}
				db_free_db_tree_state( &nts );
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
				db_free_db_tree_state( &nts );
				curtree = TREE_NULL;		/* FAIL */
				goto out;
			}
			ctsp =  db_new_combined_tree_state( &nts, pathp );
			*region_start_statepp = ctsp;
			if(RT_G_DEBUG&DEBUG_TREEWALK)  {
				bu_log("setting *region_start_statepp to x%x\n", ctsp );
				db_pr_combined_tree_state(ctsp);
			}
		}

		if( comb->tree )  {
			/* Steal tree from combination, so it won't be freed */
			curtree = comb->tree;
			comb->tree = TREE_NULL;
			if(curtree) RT_CK_TREE(curtree);

			/* Release most of internal form before recursing */
			rt_db_free_internal( &intern, tsp->ts_resp );
			comb = NULL;

			db_recurse_subtree( curtree, &nts, pathp, region_start_statepp, client_data );
			if(curtree) RT_CK_TREE(curtree);
		} else {
			/* No subtrees in this combination, invent a NOP */
			RT_GET_TREE( curtree, tsp->ts_resp );
			curtree->magic = RT_TREE_MAGIC;
			curtree->tr_op = OP_NOP;
			if(curtree) RT_CK_TREE(curtree);
		}

region_end:
		if( is_region > 0 )  {
			/*
			 *  This is the end of processing for a region.
			 */
			if( tsp->ts_region_end_func )  {
				curtree = tsp->ts_region_end_func(
					&nts, pathp, curtree, client_data );
				if(curtree) RT_CK_TREE(curtree);
			}
		}
		db_free_db_tree_state( &nts );
		if(curtree) RT_CK_TREE(curtree);
	} else if( dp->d_flags & DIR_SOLID )  {

		if( bn_mat_ck( dp->d_namep, tsp->ts_mat ) < 0 )  {
			bu_log("db_recurse(%s):  matrix does not preserve axis perpendicularity.\n",
				dp->d_namep );
			bn_mat_print("bad matrix", tsp->ts_mat);
			curtree = TREE_NULL;		/* FAIL */
			goto out;
		}

		if(RT_G_DEBUG&DEBUG_TREEWALK)
			bu_log("db_recurse() rt_db_get_internal(%s) solid\n", dp->d_namep);

	    	RT_INIT_DB_INTERNAL(&intern);
		if( rt_db_get_internal( &intern, dp, tsp->ts_dbip, tsp->ts_mat, tsp->ts_resp ) < 0 )  {
			bu_log("db_recurse() rt_db_get_internal(%s) FAIL\n", dp->d_namep);
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
			if( RT_G_DEBUG & DEBUG_REGIONS )  {
			    	bu_log("NOTICE: db_recurse(): solid '%s' not contained in a region, creating a region for it of the same name.\n",
			    		sofar );
			}

		    	ctsp = db_new_combined_tree_state( tsp, pathp );
		    	ctsp->cts_s.ts_sofar |= TS_SOFAR_REGION;
			*region_start_statepp = ctsp;
			if(RT_G_DEBUG&DEBUG_TREEWALK)  {
				bu_log("db_recurse(%s): setting *region_start_statepp to x%x (bare solid)\n",
					sofar, ctsp );
				db_pr_combined_tree_state(ctsp);
			}
		    	bu_free( sofar, "path string" );
		}

		/* Hand the solid off for leaf processing */
		if( !tsp->ts_leaf_func )  {
			curtree = TREE_NULL;		/* FAIL */
			goto out;
		}
		curtree = tsp->ts_leaf_func( tsp, pathp, &intern, client_data );
		if(curtree) RT_CK_TREE(curtree);
	} else {
		bu_log("%s is not a drawable database object\n",
			dp->d_namep );
		curtree = TREE_NULL;
		return(curtree);
	}
out:
	/* rt_db_get_internal() may not have been called yet, so do not try to free intern unless
	 * we know there is something to free
	 */
	if( intern.idb_ptr != NULL ) {
	    rt_db_free_internal( &intern, tsp->ts_resp );
	}
	if(RT_G_DEBUG&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		bu_log("db_recurse() return curtree=x%x, pathp='%s', *statepp=x%x\n",
			curtree, sofar,
			*region_start_statepp );
		bu_free(sofar, "path string");
	}
	if(curtree) RT_CK_TREE(curtree);
	return(curtree);
}

/**
 *			D B _ D U P _ S U B T R E E
 */
union tree *
db_dup_subtree( const union tree *tp, struct resource *resp )
{
	union tree	*new;

	RT_CK_TREE(tp);
	RT_CK_RESOURCE(resp);

	RT_GET_TREE( new, resp );
	*new = *tp;		/* struct copy */

	switch( tp->tr_op )  {
	case OP_NOP:
	case OP_SOLID:
		/* If this is a simple leaf, done */
		return(new);

	case OP_DB_LEAF:
		if( tp->tr_l.tl_mat )
			new->tr_l.tl_mat = bn_mat_dup( tp->tr_l.tl_mat );
		new->tr_l.tl_name = bu_strdup( tp->tr_l.tl_name );
		return new;

	case OP_REGION:
		/* If this is a REGION leaf, dup combined_tree_state & path */
		new->tr_c.tc_ctsp = db_dup_combined_tree_state(
			tp->tr_c.tc_ctsp );
		return(new);

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		new->tr_b.tb_left = db_dup_subtree( tp->tr_b.tb_left, resp );
		return(new);

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		new->tr_b.tb_left = db_dup_subtree( tp->tr_b.tb_left, resp );
		new->tr_b.tb_right = db_dup_subtree( tp->tr_b.tb_right, resp );
		return(new);

	default:
		bu_log("db_dup_subtree: bad op %d\n", tp->tr_op);
		rt_bomb("db_dup_subtree\n");
	}
	return( TREE_NULL );
}

/**
 *			D B _ C K _ T R E E
 */
void
db_ck_tree( const union tree *tp )
{

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {
	case OP_NOP:
		break;
	case OP_DB_LEAF:
		BU_ASSERT_PTR( tp->tr_l.tl_name, !=, NULL );
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

/**
 *			D B _ F R E E _ T R E E
 *
 *  Release all storage associated with node 'tp', including
 *  children nodes.
 */
void
db_free_tree( register union tree *tp, struct resource *resp )
{
	RT_CK_TREE(tp);
	RT_CK_RESOURCE(resp);

	/*
	 *  Before recursion, smash the magic number, so that if
	 *  another thread tries to free this same tree, they will fail.
	 */
	tp->magic = -3;		/* special bad flag */

	switch( tp->tr_op )  {
	case OP_FREE :
	    tp->tr_op = 0;		/* sanity */
	    return;
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
				bu_free( (char *)tp->tr_d.td_name, "region name" );
				tp->tr_d.td_name = (const char *)NULL;
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
			bu_free( (char *)tp->tr_l.tl_mat, "tl_mat" );
			tp->tr_l.tl_mat = NULL;
		}
		bu_free( tp->tr_l.tl_name, "tl_name" );
		tp->tr_l.tl_name = NULL;
		break;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		if( tp->tr_b.tb_left->magic == RT_TREE_MAGIC )
			db_free_tree( tp->tr_b.tb_left, resp );
		tp->tr_b.tb_left = TREE_NULL;
		break;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		{
			register union tree *fp;

			/* This node is known to be a binary op */
			fp = tp->tr_b.tb_left;
			tp->tr_b.tb_left = TREE_NULL;
			RT_CK_TREE(fp);
			db_free_tree( fp, resp );

			fp = tp->tr_b.tb_right;
			tp->tr_b.tb_right = TREE_NULL;
			RT_CK_TREE(fp);
			db_free_tree( fp, resp );
		}
		break;

	default:
		bu_log("db_free_tree: bad op %d\n", tp->tr_op);
		rt_bomb("db_free_tree\n");
	}
	tp->tr_op = 0;		/* sanity */
	RT_FREE_TREE( tp, resp );
}

/**			D B _ L E F T _ H V Y _ N O D E
 *
 *	Re-balance this node to make it left heavy.
 *	Unions operators will be moved to left side.
 *	when finished "tp" MUST still point to top node
 *	od this subtree.
 */
void
db_left_hvy_node( union tree *tp )
{
	union tree *lhs, *rhs;

	RT_CK_TREE(tp);

	if( tp->tr_op != OP_UNION )
		return;

	while( tp->tr_b.tb_right->tr_op == OP_UNION )
	{
		lhs = tp->tr_b.tb_left;
		rhs = tp->tr_b.tb_right;

		tp->tr_b.tb_left = rhs;
		tp->tr_b.tb_right = rhs->tr_b.tb_right;
		rhs->tr_b.tb_right = rhs->tr_b.tb_left;
		rhs->tr_b.tb_left = lhs;
	}
}

/**
 *			D B _ N O N _ U N I O N _ P U S H
 *
 *  If there are non-union operations in the tree,
 *  above the region nodes, then rewrite the tree so that
 *  the entire tree top is nothing but union operations,
 *  and any non-union operations are clustered down near the region nodes.
 */
void
db_non_union_push( register union tree *tp, struct resource *resp )
{
	union tree *A, *B, *C;
	union tree *tmp;
	int repush_child=0;

	RT_CK_TREE(tp);
	RT_CK_RESOURCE(resp);

	switch( tp->tr_op )  {
	case OP_REGION:
	case OP_SOLID:
	case OP_DB_LEAF:
		/* If this is a leaf, done */
		return;

	case OP_NOP:
		/* This tree has nothing in it, done */
		return;

	default:
		db_non_union_push( tp->tr_b.tb_left, resp );
		db_non_union_push( tp->tr_b.tb_right, resp );
		break;
	}
	if( (tp->tr_op == OP_INTERSECT || tp->tr_op == OP_SUBTRACT) &&
	    tp->tr_b.tb_left->tr_op == OP_UNION ) {
		union tree	*lhs = tp->tr_b.tb_left;
	    	union tree	*rhs;

		A = lhs->tr_b.tb_left;
		B = lhs->tr_b.tb_right;

		if( A->tr_op == OP_NOP && B->tr_op == OP_NOP ) {
		  /* nothing here, eliminate entire subtree */
		  db_free_tree( tp->tr_b.tb_left, resp );
		  db_free_tree( tp->tr_b.tb_right, resp );
		  tp->tr_op = OP_NOP;
		  tp->tr_b.tb_left = NULL;
		  tp->tr_b.tb_right = NULL;

		} else if( A->tr_op == OP_NOP ) {
		  db_tree_del_lhs( lhs, resp );

		  /* recurse */
		  db_non_union_push( tp, resp );
		} else if( B->tr_op == OP_NOP ) {
		  db_tree_del_rhs( lhs, resp );

		  /* recurse */
		  db_non_union_push( tp, resp );
		} else {

		  repush_child = 1;

		  /*  Rewrite intersect and subtraction nodes, such that
		   *  (A u B) - C  becomes (A - C) u (B - C)
		   *
		   * tp->    -
		   *	   /   \
		   * lhs->  u     C
		   *	 / \
		   *	A   B
		   */
		  RT_GET_TREE( rhs, resp );

		  /* duplicate top node into rhs */
		  *rhs = *tp;		/* struct copy */
		  tp->tr_b.tb_right = rhs;
		  /* rhs->tr_b.tb_right remains unchanged:
		   *
		   * tp->    -
		   *	   /   \
		   * lhs->  u     -   <-rhs
		   *	 / \   / \
		   *	A   B ?   C
		   */

		  rhs->tr_b.tb_left = lhs->tr_b.tb_right;
		  /*
		   * tp->    -
		   *	   /   \
		   * lhs->  u     -   <-rhs
		   *	 / \   / \
		   *	A   B B   C
		   */

		  /* exchange left and top operators */
		  tp->tr_op = lhs->tr_op;
		  lhs->tr_op = rhs->tr_op;
		  /*
		   * tp->    u
		   *	   /   \
		   * lhs->  -     -   <-rhs
		   *	 / \   / \
		   *	A   B B   C
		   */

		  /* Make a duplicate of rhs->tr_b.tb_right */
		  lhs->tr_b.tb_right = db_dup_subtree( rhs->tr_b.tb_right, resp );
		  /*
		   * tp->    u
		   *	   /   \
		   * lhs->  -     -   <-rhs
		   *	 / \   / \
		   *	A  C' B   C
		   */
		}

	}

	else if( tp->tr_op == OP_INTERSECT &&
		tp->tr_b.tb_right->tr_op == OP_UNION )
	{
		/* C + (A u B) -> (C + A) u (C + B) */
		union tree	*rhs = tp->tr_b.tb_right;

		C = tp->tr_b.tb_left;
		A = tp->tr_b.tb_right->tr_b.tb_left;
		B = tp->tr_b.tb_right->tr_b.tb_right;

		if( A->tr_op == OP_NOP && B->tr_op == OP_NOP ) {
		  /* nothing here, eliminate entire subtree */
		  tp->tr_op = OP_NOP;
		  db_free_tree( tp->tr_b.tb_left, resp );
		  db_free_tree( tp->tr_b.tb_right, resp );
		  tp->tr_b.tb_left = NULL;
		  tp->tr_b.tb_right = NULL;
		} else if( A->tr_op == OP_NOP ) {
		  db_tree_del_lhs( rhs, resp );

		  /* recurse */
		  db_non_union_push( tp, resp );
		} else if( B->tr_op == OP_NOP ) {
		  db_tree_del_rhs( rhs, resp );

		  /* recurse */
		  db_non_union_push( tp, resp );
		} else {
		  repush_child = 1;

		  tp->tr_op = OP_UNION;
		  RT_GET_TREE( tmp, resp );
		  tmp->tr_regionp = tp->tr_regionp;
		  tmp->magic = RT_TREE_MAGIC;
		  tmp->tr_op = OP_INTERSECT;
		  tmp->tr_b.tb_left = C;
		  tmp->tr_b.tb_right = A;
		  tp->tr_b.tb_left = tmp;
		  tp->tr_b.tb_right->tr_op = OP_INTERSECT;
		  tp->tr_b.tb_right->tr_b.tb_left = db_dup_subtree( C, resp );
		}
	}
	else if( tp->tr_op == OP_SUBTRACT &&
		tp->tr_b.tb_right->tr_op == OP_UNION )
	{
		/* C - (A u B) -> C - A - B */
		union tree	*rhs = tp->tr_b.tb_right;


		C = tp->tr_b.tb_left;
		A = tp->tr_b.tb_right->tr_b.tb_left;
		B = tp->tr_b.tb_right->tr_b.tb_right;

		if( C->tr_op == OP_NOP ) {
		  /* nothing here, eliminate entire subtree */
		  tp->tr_op = OP_NOP;
		  db_free_tree( tp->tr_b.tb_left, resp );
		  db_free_tree( tp->tr_b.tb_right, resp );
		  tp->tr_b.tb_left = NULL;
		  tp->tr_b.tb_right = NULL;
		} else if( A->tr_op == OP_NOP && B->tr_op == OP_NOP ) {
		  db_tree_del_rhs( tp, resp );

		  /* recurse */
		  db_non_union_push( tp, resp );
		} else if( A->tr_op == OP_NOP ) {
		  db_tree_del_lhs( rhs, resp );

		  /* recurse */
		  db_non_union_push( tp, resp );
		} else if( B->tr_op == OP_NOP ) {
		  db_tree_del_rhs( rhs, resp );

		  /* recurse */
		  db_non_union_push( tp, resp );
		} else {
		  repush_child = 1;

		  tp->tr_b.tb_left = tp->tr_b.tb_right;
		  tp->tr_b.tb_left->tr_op = OP_SUBTRACT;
		  tp->tr_b.tb_right = B;
		  tmp = tp->tr_b.tb_left;
		  tmp->tr_b.tb_left = C;
		  tmp->tr_b.tb_right = A;
		}
	}

	/* if this operation has moved a UNION operator towards the leaves
	 * then the children must be processed again
	 */
	if( repush_child )
	{
		db_non_union_push( tp->tr_b.tb_left, resp );
		db_non_union_push( tp->tr_b.tb_right, resp );
	}

	/* rebalance this node (moves UNIONs to left side) */
	db_left_hvy_node( tp );
}

/**
 *			D B _ C O U N T _ T R E E _ N O D E S
 *
 *  Return a count of the number of "union tree" nodes below "tp",
 *  including tp.
 */
int
db_count_tree_nodes( const union tree *tp, int count )
{
	RT_CK_TREE(tp);
	switch( tp->tr_op )  {
	case OP_NOP:
	case OP_SOLID:
	case OP_REGION:
	case OP_DB_LEAF:
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
		bu_log("db_count_tree_nodes: bad op %d\n", tp->tr_op);
		rt_bomb("db_count_tree_nodes\n");
	}
	return( 0 );
}

/**
 *			D B _ I S _ T R E E _ A L L _ U N I O N S
 *
 *  Returns -
 *	1	if this tree contains nothing but union operations.
 *	0	if at least one subtraction or intersection op exists.
 */
int
db_is_tree_all_unions( const union tree *tp )
{
	RT_CK_TREE(tp);
	switch( tp->tr_op )  {
	case OP_NOP:
	case OP_SOLID:
	case OP_REGION:
	case OP_DB_LEAF:
		/* A leaf node */
		return 1;		/* yep */

	case OP_UNION:
		if( db_is_tree_all_unions( tp->tr_b.tb_left ) == 0 )
			return 0;
		return db_is_tree_all_unions( tp->tr_b.tb_right );

	case OP_INTERSECT:
	case OP_SUBTRACT:
		return 0;		/* nope */

	case OP_XOR:
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		return 0;		/* nope */

	default:
		bu_log("db_is_tree_all_unions: bad op %d\n", tp->tr_op);
		rt_bomb("db_is_tree_all_unions\n");
	}
	return 0;
}

/**
 *			D B _ C O U N T _ S U B T R E E _ R E G I O N S
 */
int
db_count_subtree_regions( const union tree *tp )
{
	int	cnt;

	RT_CK_TREE(tp);
	switch( tp->tr_op )  {
	case OP_SOLID:
	case OP_REGION:
	case OP_DB_LEAF:
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

/**
 *			D B _ T A L L Y _ S U B T R E E _ R E G I O N S
 */
int
db_tally_subtree_regions(
	union tree	*tp,
	union tree	**reg_trees,
	int		cur,
	int		lim,
	struct resource *resp)
{
	union tree	*new;

	RT_CK_TREE(tp);
	RT_CK_RESOURCE(resp);
	if( cur >= lim )  rt_bomb("db_tally_subtree_regions: array overflow\n");

	switch( tp->tr_op )  {
	case OP_NOP:
		return(cur);

	case OP_SOLID:
	case OP_REGION:
	case OP_DB_LEAF:
		RT_GET_TREE( new, resp );
		*new = *tp;		/* struct copy */
		tp->tr_op = OP_NOP;	/* Zap original */
		reg_trees[cur++] = new;
		return(cur);

	case OP_UNION:
		/* This node is known to be a binary op */
		cur = db_tally_subtree_regions( tp->tr_b.tb_left, reg_trees, cur, lim, resp );
		cur = db_tally_subtree_regions( tp->tr_b.tb_right, reg_trees, cur, lim, resp );
		return(cur);

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		/* This is as far down as we go -- this is a region top */
		RT_GET_TREE( new, resp );
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

HIDDEN union tree *db_gettree_region_end(register struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{

	RT_CK_DBTS(tsp);
	RT_CK_DBI(tsp->ts_dbip);
	RT_CK_FULL_PATH(pathp);
	RT_CK_RESOURCE(tsp->ts_resp);

	RT_GET_TREE( curtree, tsp->ts_resp );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_REGION;
	curtree->tr_c.tc_ctsp = db_new_combined_tree_state( tsp, pathp );

	return(curtree);
}

HIDDEN union tree *db_gettree_leaf(struct db_tree_state *tsp, struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t client_data)
{
	register union tree	*curtree;

	RT_CK_DBTS(tsp);
	RT_CK_DBI(tsp->ts_dbip);
	RT_CK_FULL_PATH(pathp);
	RT_CK_DB_INTERNAL(ip);
	RT_CK_RESOURCE(tsp->ts_resp);

	RT_GET_TREE( curtree, tsp->ts_resp );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_REGION;
	curtree->tr_c.tc_ctsp = db_new_combined_tree_state( tsp, pathp );

	return(curtree);
}

struct db_walk_parallel_state {
	long		magic;
	union tree	**reg_trees;
	int		reg_count;
	int		reg_current;		/* semaphored when parallel */
	union tree *	(*reg_end_func)();
	union tree *	(*reg_leaf_func)();
	struct rt_i	*rtip;
	genptr_t	client_data;
};
#define DB_WALK_PARALLEL_STATE_MAGIC	0x64777073	/* dwps */
#define DB_CK_WPS(_p)	BU_CKMAG(_p, DB_WALK_PARALLEL_STATE_MAGIC, "db_walk_parallel_state")

/**
 *			D B _ W A L K _ S U B T R E E
 */
HIDDEN void
db_walk_subtree(
	register union tree	*tp,
	struct combined_tree_state	**region_start_statepp,
	union tree	 *(*leaf_func) BU_ARGS((struct db_tree_state *, struct db_full_path *, struct rt_db_internal *, void *)),
	genptr_t	client_data,
	struct resource	*resp )
{
	struct combined_tree_state	*ctsp;
	union tree	*curtree;

	RT_CK_TREE(tp);
	RT_CK_RESOURCE(resp);

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
		RT_CK_DBI(ctsp->cts_s.ts_dbip);
		ctsp->cts_s.ts_stop_at_regions = 0;
		/* All regions will be accepted, in this 2nd pass */
		ctsp->cts_s.ts_region_start_func = 0;
		/* ts_region_end_func() will be called in db_walk_dispatcher() */
		ctsp->cts_s.ts_region_end_func = 0;
		/* Use user's leaf function */
		ctsp->cts_s.ts_leaf_func = leaf_func;
		ctsp->cts_s.ts_resp = resp;

		/* If region already seen, force flag */
		if( *region_start_statepp )
			ctsp->cts_s.ts_sofar |= TS_SOFAR_REGION;
		else
			ctsp->cts_s.ts_sofar &= ~TS_SOFAR_REGION;

		curtree = db_recurse( &ctsp->cts_s, &ctsp->cts_p, region_start_statepp, client_data );
		if( curtree == TREE_NULL )  {
			char	*str;
			str = db_path_to_string( &(ctsp->cts_p) );
			bu_log("db_walk_subtree() FAIL on '%s'\n", str);
			bu_free( str, "path string" );

			db_free_combined_tree_state( ctsp );
			/* Result is an empty tree */
			tp->tr_op = OP_NOP;
			tp->tr_a.tu_stp = 0;
			return;
		}
		/* replace *tp with new subtree */
		*tp = *curtree;		/* struct copy */
		db_free_combined_tree_state( ctsp );
		RT_FREE_TREE( curtree, resp );
		return;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		db_walk_subtree( tp->tr_b.tb_left, region_start_statepp,
			leaf_func, client_data, resp );
		return;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		db_walk_subtree( tp->tr_b.tb_left, region_start_statepp,
			leaf_func, client_data, resp );
		db_walk_subtree( tp->tr_b.tb_right, region_start_statepp,
			leaf_func, client_data, resp );
		return;

	case OP_DB_LEAF:
		rt_pr_tree( tp, 1 );
		bu_bomb("db_walk_subtree() unexpected DB_LEAF\n");

	default:
		bu_log("db_walk_subtree: bad op %d\n", tp->tr_op);
		bu_bomb("db_walk_subtree() bad op\n");
	}
}

/**
 *			D B _ W A L K _ D I S P A T C H E R
 *
 *  This routine handles the PARALLEL portion of db_walk_tree().
 *  There will be at least one, and possibly more, instances of
 *  this routine running simultaneously.
 *
 *  Uses the self-dispatcher pattern:
 *  Pick off the next region's tree, and walk it.
 */
HIDDEN void
db_walk_dispatcher(int cpu, genptr_t arg)
{
	struct combined_tree_state	*region_start_statep;
	int		mine;
	union tree	*curtree;
	struct db_walk_parallel_state	*wps = (struct db_walk_parallel_state *)arg;
	struct resource	*resp;

	DB_CK_WPS(wps);

	if( wps->rtip == NULL && cpu == 0 )  {
		resp = &rt_uniresource;
	} else {
		RT_CK_RTI(wps->rtip);

		resp = (struct resource *)BU_PTBL_GET( &wps->rtip->rti_resources, cpu );
		if( resp == NULL && cpu == 0 )  resp = &rt_uniresource;
	}
	RT_CK_RESOURCE(resp);

	while(1)  {
		bu_semaphore_acquire( RT_SEM_WORKER );
		mine = wps->reg_current++;
		bu_semaphore_release( RT_SEM_WORKER );

		if( mine >= wps->reg_count )
			break;

		if( RT_G_DEBUG&DEBUG_TREEWALK )
			bu_log("\n\n***** db_walk_dispatcher() on item %d\n\n", mine );

		if( (curtree = wps->reg_trees[mine]) == TREE_NULL )
			continue;
		RT_CK_TREE(curtree);

		/* Walk the full subtree now */
		region_start_statep = (struct combined_tree_state *)0;
		db_walk_subtree( curtree, &region_start_statep,
			wps->reg_leaf_func, wps->client_data, resp );

		/*  curtree->tr_op may be OP_NOP here.
		 *  It is up to db_reg_end_func() to deal with this,
		 *  either by discarding it, or making a null region.
		 */
		RT_CK_TREE(curtree);
		if( !region_start_statep )  {
			bu_log("ERROR: db_walk_dispatcher() region %d started with no state\n", mine);
			if( RT_G_DEBUG&DEBUG_TREEWALK )
				rt_pr_tree( curtree, 0 );
			continue;
		}
		RT_CK_CTS( region_start_statep );

		/* This is a new region */
		if( RT_G_DEBUG&DEBUG_TREEWALK )
			db_pr_combined_tree_state(region_start_statep);

		/*
		 *  reg_end_func() returns a pointer to any unused
		 *  subtree for freeing.
		 */
		if( wps->reg_end_func )  {
			wps->reg_trees[mine] = (*(wps->reg_end_func))(
				&(region_start_statep->cts_s),
				&(region_start_statep->cts_p),
				curtree, wps->client_data );
		}

		db_free_combined_tree_state( region_start_statep );
	}
}

/**
 *			D B _ W A L K _ T R E E
 *
 *  This is the top interface to the "tree walker."
 *
 * Parameters:
 *	rtip		rt_i structure to database (open with rt_dirbuild())
 *	argc		# of tree-tops named
 *	argv		names of tree-tops to process
 *	init_state	Input parameter: initial state of the tree.
 *			For example:  rt_initial_tree_state,
 *			and mged_initial_tree_state.
 *
 * These parameters are pointers to callback routines.
 * If NULL, they won't be called.
 *
 *	reg_start_func	Called at beginning of each region, before visiting
 *			any nodes within the region.
 *			Return 0 if region should be skipped without recursing,
 *			 otherwise non-zero.  DO NOT USE FOR OTHER PURPOSES!
 *			For example, can be used to quickly skip air regions.
 *
 *	reg_end_func	Called after all nodes within a region have been
 *			recursively processed by leaf_func.
 *			If it wants to retain 'curtree' then it may steal
 *			that pointer and return TREE_NULL.
 *			If it wants us to clean up some or all of that
 *			tree, then it returns a non-null (union tree *)
 *			pointer, and that tree is safely freed
 *			in a non-parallel section before we return.
 *
 *	leaf_func	Function to process a leaf node.
 *			It is actually invoked from db_recurse() from db_walk_subtree().
 *			Returns (union tree *) representing the leaf, or
 *			TREE_NULL if leaf does not exist or has an error.
 *
 *
 *  This routine will employ multiple CPUs if asked,
 *  but is not multiply-parallel-recursive.
 *  Call this routine with ncpu > 1 from serial code only.
 *  When called from within an existing thread, ncpu must be 1.
 *
 *  If ncpu > 1, the caller is responsible for making sure that
 *	rt_g.rtg_parallel is non-zero, and that the
 *	bu_semaphore_init() functions has been performed, first.
 *
 *  Plucks per-cpu resources out of rtip->rti_resources[].
 *  They need to have been initialized first.
 *
 *  Returns -
 *	-1	Failure to prepare even a single sub-tree
 *	 0	OK
 */
int
db_walk_tree(struct db_i *dbip,
	     int argc,
	     const char **argv,
	     int ncpu,
	     const struct db_tree_state *init_state,
	     int (*reg_start_func) (struct db_tree_state *, struct db_full_path *, const struct rt_comb_internal *, genptr_t),
	     union tree *(*reg_end_func) (struct db_tree_state *, struct db_full_path *, union tree *, genptr_t),
	     union tree *(*leaf_func) (struct db_tree_state *, struct db_full_path *, struct rt_db_internal *, genptr_t),
	     genptr_t client_data)
{
	union tree		*whole_tree = TREE_NULL;
	int			new_reg_count;
	int			i;
	int                     something_not_found = 0;
	union tree		**reg_trees;	/* (*reg_trees)[] */
	struct db_walk_parallel_state	wps;
	struct resource		*resp;

	RT_CK_DBTS(init_state);
	RT_CHECK_DBI(dbip);

	if( init_state->ts_rtip == NULL && ncpu == 1 )  {
		resp = &rt_uniresource;
	} else {
		RT_CK_RTI(init_state->ts_rtip);
		resp = (struct resource *)BU_PTBL_GET(&init_state->ts_rtip->rti_resources, 0);
		if( resp == NULL && ncpu == 1 )  {
			resp = &rt_uniresource;
		}
	}
	RT_CK_RESOURCE(resp);

	/* Walk each of the given path strings */
	for( i=0; i < argc; i++ )  {
		register union tree	*curtree;
		struct db_tree_state	ts;
		struct db_full_path	path;
		struct combined_tree_state	*region_start_statep;

		ts = *init_state;	/* struct copy */
		ts.ts_dbip = dbip;
		ts.ts_resp = resp;
		db_full_path_init( &path );

		/* First, establish context from given path */
		if( db_follow_path_for_state( &ts, &path, argv[i],
					      LOOKUP_NOISY ) < 0 )
		  {
		    bu_log ("db_walk_tree: warning - %s not found.\n",
			    argv[i]);
		    ++ something_not_found;
		    continue;	/* ERROR */
		  }
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
		curtree = db_recurse( &ts, &path, &region_start_statep, client_data );
		if( region_start_statep )
			db_free_combined_tree_state( region_start_statep );
		db_free_full_path( &path );
		if( curtree == TREE_NULL )
			continue;	/* ERROR */

		RT_CK_TREE(curtree);
		if( RT_G_DEBUG&DEBUG_TREEWALK )  {
			bu_log("tree after db_recurse():\n");
			rt_pr_tree( curtree, 0 );
		}

		if( whole_tree == TREE_NULL )  {
			whole_tree = curtree;
		} else {
			union tree	*new;

			RT_GET_TREE( new, ts.ts_resp );
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
	db_non_union_push( whole_tree, resp );
	if( RT_G_DEBUG&DEBUG_TREEWALK )  {
		char *str;

		bu_log("tree after db_non_union_push():\n");
		rt_pr_tree( whole_tree, 0 );
		bu_log( "Same tree in another form:\n" );
		str = (char *)rt_pr_tree_str( whole_tree );
		bu_log( "%s\n", str );
		bu_free( str, "rturn from rt_pr_tree_str" );
	}

	/*
	 *  Build array of sub-tree pointers, one per region,
	 *  for parallel processing below.
	 */
	new_reg_count = db_count_subtree_regions( whole_tree );
	reg_trees = (union tree **)bu_calloc( sizeof(union tree *),
		(new_reg_count+1), "*reg_trees[]" );
	new_reg_count = db_tally_subtree_regions( whole_tree, reg_trees, 0,
		new_reg_count, resp );

	/*  Release storage for tree from whole_tree to leaves.
	 *  db_tally_subtree_regions() duplicated and OP_NOP'ed the original
	 *  top of any sub-trees that it wanted to keep, so whole_tree
	 *  is just the left-over part now.
	 */
	db_free_tree( whole_tree, resp );

	/* As a debugging aid, print out the waiting region names */
	if( RT_G_DEBUG&DEBUG_TREEWALK )  {
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
				bu_log("%d: op=%d\n", i, treep->tr_op);
				rt_pr_tree( treep, 2 );
				continue;
			}
			ctsp = treep->tr_c.tc_ctsp;
		 	RT_CK_CTS(ctsp);
			str = db_path_to_string( &(ctsp->cts_p) );
			bu_log("%d '%s'\n", i, str);
			bu_free( str, "path string" );
		}
		bu_log("end of waiting regions\n");
	}

	/*
	 *  Fourth, in parallel, for each region, walk the tree to the leaves.
	 */
	if( bu_is_parallel() && ncpu != 1 )  {
		bu_log("db_walk_tree() recursively invoked while inside parallel section with additional parallelism of ncpu=%d requested.  Running only in one thread.\n",
			ncpu );
		ncpu = 1;
	}

	/* Make state available to the threads */
	wps.magic = DB_WALK_PARALLEL_STATE_MAGIC;
	wps.reg_trees = reg_trees;
	wps.reg_count = new_reg_count;
	wps.reg_current = 0;			/* Semaphored */
	wps.reg_end_func = reg_end_func;
	wps.reg_leaf_func = leaf_func;
	wps.client_data = client_data;
	wps.rtip = init_state->ts_rtip;

	if( ncpu <= 1 )  {
		db_walk_dispatcher( 0, (genptr_t)&wps );
	} else {
		bu_parallel( db_walk_dispatcher, ncpu, (genptr_t)&wps );
	}

	/* Clean up any remaining sub-trees still in reg_trees[] */
	for( i=0; i < new_reg_count; i++ )  {
		if( reg_trees[i] != TREE_NULL )  {
			db_free_tree( reg_trees[i], resp );
		}
	}
	bu_free( (char *)reg_trees, "*reg_trees[]" );

	if (something_not_found)
	  {
	    bu_log ("db_walk_tree: %d %s not found.\n",
		    something_not_found,
		    (something_not_found > 1) ? "items" : "item" ) ;
	    return -2;
	  }
	else
	  {
	    return 0;	/* OK */
	  }
}

/**
 *			D B _ P A T H _ T O _ M A T
 *
 *  Returns -
 *	1	OK, path matrix written into 'mat'.
 *	0	FAIL
 *
 *  Called in librt/db_tree.c, mged/dodraw.c, and mged/animedit.c
 */
int
db_path_to_mat(
	struct db_i		*dbip,
	struct db_full_path	*pathp,
	mat_t			mat,		/* result */
	int			depth,		/* number of arcs */
	struct resource		*resp)
{
	struct db_tree_state	ts;
	struct db_full_path	null_path;
	int			ret;

	RT_CHECK_DBI(dbip);
	RT_CK_FULL_PATH(pathp);
	if( !mat ) rt_bomb("db_path_to_mat() NULL matrix pointer\n");

	db_full_path_init( &null_path );
	db_init_db_tree_state( &ts, dbip, resp );

	ret = db_follow_path( &ts, &null_path, pathp, LOOKUP_NOISY, depth );
	db_free_full_path( &null_path );
	MAT_COPY( mat, ts.ts_mat );	/* implicit return */
	db_free_db_tree_state( &ts );

	if( ret < 0 )  {
		return 0;	/* FAIL */
	}
	return 1;		/* OK */
}

/**
 *			D B _ A P P L Y _ A N I M S
 *
 *  'arc' may be a null pointer, signifying an identity matrix.
 *  'materp' may be a null pointer, signifying that
 *  the region has already been finalized above this point in the tree.
 */
void
db_apply_anims(struct db_full_path *pathp, struct directory *dp, mat_t stack, mat_t arc, struct mater_info *materp)
{
	register struct animate *anp;
	register int i,j;

	/* Check here for animation to apply */

	if ((dp->d_animate != ANIM_NULL) && (RT_G_DEBUG & DEBUG_ANIM)) {
		char	*sofar = db_path_to_string(pathp);
		bu_log("Animate %s with...\n", sofar);
		bu_free(sofar, "path string");
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

		if (RT_G_DEBUG & DEBUG_ANIM) {
			char	*str;

			str = db_path_to_string( &(anp->an_path) );
			bu_log( "\t%s\t", str );
			bu_free( str, "path string" );
			bu_log("an_path.fp_len-1:%d  pathp->fp_len-1:%d\n",
				i, j);
		}

		for( ; i>=0 && j>=0; i--, j-- )  {
			if( anp->an_path.fp_names[i] != pathp->fp_names[j] ) {
				if (RT_G_DEBUG & DEBUG_ANIM) {
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

/**
 *			D B _ R E G I O N _ M A T
 *
 *  Given the name of a region, return the matrix which maps model coordinates
 *  into "region" coordinates.
 *
 *  Returns:
 *	0	OK
 *	<0	Failure
 */
int
db_region_mat(
	mat_t		m,		/* result */
	struct db_i	*dbip,
	const char	*name,
	struct resource *resp)
{
	struct db_full_path		full_path;
	mat_t	region_to_model;

	/* get transformation between world and "region" coordinates */
	if (db_string_to_path( &full_path, dbip, name) ) {
		/* bad thing */
		bu_log("db_region_mat: db_string_to_path(%s) error\n", name);
		return -1;
	}
	if(! db_path_to_mat(dbip, &full_path, region_to_model, 0, resp)) {
		/* bad thing */
		bu_log("db_region_mat: db_path_to_mat(%s) error", name);
		return -2;
	}

	/* get matrix to map points from model (world) space
	 * to "region" space
	 */
	bn_mat_inv(m, region_to_model);
	db_free_full_path( &full_path );
	return 0;
}



/**		D B _ S H A D E R _ M A T
 * XXX given that this routine depends on rtip, it should be called
 * XXX rt_shader_mat().
 *
 *  Given a region, return a matrix which maps model coordinates into
 *  region "shader space".  This is a space where points in the model
 *  within the bounding box of the region are mapped into "region"
 *  space (the coordinate system in which the region is defined).
 *  The area occupied by the region's bounding box (in region coordinates)
 *  are then mapped into the unit cube.  This unit cube defines
 *  "shader space".
 *
 *  Returns:
 *	0	OK
 *	<0	Failure
 */
int
db_shader_mat(
	mat_t			model_to_shader,	/* result */
	const struct rt_i	*rtip,
	const struct region	*rp,
	point_t			p_min,	/* input/output: shader/region min point */
	point_t			p_max,	/* input/output: shader/region max point */
	struct resource		*resp)
{
	mat_t	model_to_region;
	mat_t	m_xlate;
	mat_t	m_scale;
	mat_t	m_tmp;
	vect_t	v_tmp;
	struct	rt_i *my_rtip;
	const char	*reg_name;

	RT_CK_RTI(rtip);
	RT_CK_RESOURCE(resp);

	reg_name = rt_basename(rp->reg_name);
#ifdef DEBUG_SHADER_MAT
	bu_log("db_shader_mat(%s)\n", rp->reg_name);
#endif
	/* get model-to-region space mapping */
	if( db_region_mat(model_to_region, rtip->rti_dbip, rp->reg_name, resp) < 0 )
		return -1;

#ifdef DEBUG_SHADER_MAT
	bn_mat_print("model_to_region", model_to_region);
#endif
	if (VEQUAL(p_min, p_max)) {
		/* User/shader did not specify bounding box,
		 * obtain bounding box for un-transformed region
		 */

		/* XXX This should really be handled by a special set of
		 * tree walker routines which just build up the RPP of the
		 * region.  For now we just re-use rt_rpp_region() with
		 * a scratch rtip.
		 */
		my_rtip = rt_new_rti(rtip->rti_dbip);
		my_rtip->useair = rtip->useair;

		/* XXX Should have our own semaphore here */
		bu_semaphore_acquire( RT_SEM_MODEL );
		if (rt_gettree(my_rtip, reg_name)) bu_bomb(rp->reg_name);
		bu_semaphore_release( RT_SEM_MODEL );
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
	MAT_IDN(m_xlate);
	VSCALE(v_tmp, p_min, -1);
	MAT_DELTAS_VEC(m_xlate, v_tmp);
	bn_mat_mul(m_tmp, m_xlate, model_to_region);


	/*
	 * Scale the bounding box to unit cube
	 */
	VSUB2(v_tmp, p_max, p_min);
	VINVDIR(v_tmp, v_tmp);
	MAT_IDN(m_scale);
	MAT_SCALE_VEC(m_scale, v_tmp);
	bn_mat_mul(model_to_shader, m_scale, m_tmp);
	return 0;
}

/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
