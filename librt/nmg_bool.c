/*
 *			N M G _ B O O L . C
 *
 *	Support for boolean operations on NMG objects.  Most of the routines
 *	in here are static/local to this file.  The interfaces here are the
 *	functions "nmg_do_bool" and "nmg_mesh_faces".  The former does boolean
 *	operations on a pair of shells.  The latter is a function to make
 *	edges shared between two faces whenever possible.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./debug.h"

extern int nmg_class_nothing_broken;

/* XXX Move to nmg_manif.c or nmg_ck.c */
struct dangling_faceuse_state {
	char		*visited;
	CONST char	*manifolds;
	int		count;
};

static void
nmg_dangling_handler( longp, state, first )
long		*longp;
genptr_t	state;
int		first;
{
	register struct faceuse	*fu = (struct faceuse *)longp;
	register struct dangling_faceuse_state *sp =
		(struct dangling_faceuse_state *)state;

	NMG_CK_FACEUSE(fu);
	if( fu->orientation != OT_SAME )  return;
	/* If this faceuse has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, fu) )  return;

	if( nmg_dangling_face(fu, sp->manifolds ) )  {
		sp->count++;
	}
}

/*
 *			N M G _ H A S _ D A N G L I N G _ F A C E S
 *
 *  Argument is expected to be model, region, shell, or faceuse pointer.
 *
 *  Returns -
 *	0	No dangling faces
 *	!0	Has dangling faces
 */
int
nmg_has_dangling_faces( magic_p, manifolds )
long		*magic_p;
CONST char	*manifolds;
{
	struct model			*m;
	struct nmg_visit_handlers	handlers;
	struct dangling_faceuse_state	st;

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL(m);
	st.visited = (char *)rt_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.manifolds = manifolds;
	st.count = 0;

	handlers = nmg_visit_handlers_null;		/* struct copy */
	handlers.bef_faceuse = nmg_dangling_handler;
	nmg_visit( magic_p, &handlers, (genptr_t)&st );

	rt_free( (char *)st.visited, "visited[]");
	return st.count;
}

/*
 *			N M G _ S H O W _ E A C H _ L O O P
 *
 *  Within a shell, show each loop as a separate display.
 *  Pause after displaying each one.
 *
 *  Note that in "non-fancy" mode, show_broken_eu() draws just the edge.
 */
void
nmg_show_each_loop(s, classlist, new, fancy, str)
struct shell	*s;
long		*classlist[4];
int		new;		/* non-zero means flush previous vlist */
CONST char	*str;		/* non-zero means pause after the display */
{
	struct faceuse	*fu;
	struct loopuse	*lu;
	char		buf[128];
	long		save;

	NMG_CK_SHELL(s);
	save = rt_g.NMG_debug;
	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		if( fu->orientation == OT_OPPOSITE )  continue;
		for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu);
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )
				continue;
			/* Display only OT_SAME, and OT_UNSPEC et.al.  */
			if( lu->orientation == OT_OPPOSITE )  continue;

			sprintf(buf, "%s=x%x", str, lu);
			nmg_show_broken_classifier_stuff(&lu->l.magic, classlist, new, fancy, buf);
		}
	}
	for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		sprintf(buf, "%s=x%x (wire)", str, lu);
		nmg_show_broken_classifier_stuff(&lu->l.magic, classlist, new, fancy, buf);
	}
	rt_g.NMG_debug = save;		/* restore it */
}

/*
 *			N M G _ B O O L
 *
 *	Perform boolean operations on a pair of shells.
 *
 *  The return is an updated version of shell A.
 *  Shell B is destroyed.
 *
 *  XXX this probably should operate on regions, not shells.
 */
static struct shell * nmg_bool(sA, sB, oper, tol)
struct shell *sA, *sB;
CONST int		oper;
CONST struct rt_tol	*tol;
{
	int	i;
	int	nelem;
	long	*classlist[8];
	FILE	*fd, *fp;
	struct model		*m;
	struct nmgregion	*rA;
	struct nmgregion	*rB;

	NMG_CK_SHELL(sA);
	NMG_CK_SHELL(sB);
	rA = sA->r_p;
	rB = sB->r_p;
	NMG_CK_REGION(rA);
	NMG_CK_REGION(rB);
	m = rA->m_p;
	NMG_CK_MODEL(m);

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
#if 0
		nmg_vshell( &rA->s_hd, rA );
		nmg_vshell( &rB->s_hd, rB );
#else
		/* Sometimes the tessllations of non-participating regions
		 * are damaged during a boolean operation.  Check everything.
		 */
		nmg_vmodel(m);
#endif
#if VERBOSE_VERIFY
		rt_log("\n==================== Shell A:\n");
		nmg_pr_s_briefly(sA, 0);
		rt_log("\n==================== Shell B:\n");
		nmg_pr_s_briefly(sB, 0);
		rt_log("\n====================\n");
#endif
	}
nmg_s_radial_check( sA, tol );
nmg_s_radial_check( sB, tol );

	nmg_shell_coplanar_face_merge(sA, tol, 1);
	nmg_shell_coplanar_face_merge(sB, tol, 1);

	if (nmg_ck_closed_surf(sA, tol)) {
		if (rt_g.NMG_debug & DEBUG_BOOL &&
		    rt_g.NMG_debug & DEBUG_PLOTEM) {
			if ((fp=fopen("Unclosed.pl", "w")) != (FILE *)NULL) {
				rt_log("Plotting unclosed NMG shell\n");
				nmg_pl_s(fp, sA);
				fclose(fp);
			}
		}
		if (rt_g.NMG_debug & DEBUG_BOOL)
			nmg_pr_s(sA, "");

		rt_log("nmg_bool: sA is unclosed, barging ahead\n");
	}

	if (nmg_ck_closed_surf(sB, tol)) {
		if (rt_g.NMG_debug & DEBUG_BOOL &&
		    rt_g.NMG_debug & DEBUG_PLOTEM) {
			if ((fp=fopen("Unclosed.pl", "w")) != (FILE *)NULL) {
				rt_log("Plotting unclosed NMG shell\n");
				nmg_pl_s(fp, sB);
				fclose(fp);
			}
		}
		if (rt_g.NMG_debug & DEBUG_BOOL)
			nmg_pr_s(sB, "");
		rt_log("nmg_bool: sB is unclosed, barging ahead\n");
	}


	if (rt_g.NMG_debug & DEBUG_BOOL && rt_g.NMG_debug & DEBUG_PLOTEM) {
		if ((fp=fopen("shellA.pl", "w")) == (FILE*)NULL) {
			(void)perror("shellA.pl");
			exit(-1);
		}
		rt_log("plotting shellA.pl\n");
		nmg_pl_s(fp, sA);
		fclose(fp);

		if ((fp=fopen("shellB.pl", "w")) == (FILE*)NULL) {
			(void)perror("shellB.pl");
			exit(-1);
		}
		rt_log("plotting shellB.pl\n");
		nmg_pl_s(fp, sB);
		fclose(fp);
	}

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
#if 0
		nmg_vshell( &rA->s_hd, rA );
		nmg_vshell( &rB->s_hd, rB );
#else
		/* Sometimes the tessllations of non-participating regions
		 * are damaged during a boolean operation.  Check everything.
		 */
		nmg_vmodel(m);
#endif
#if VERBOSE_VERIFY
		rt_log("\n==================== Shell A: ====== before crackshells\n");
		nmg_pr_s_briefly(sA, 0);
		rt_log("\n==================== Shell B:\n");
		nmg_pr_s_briefly(sB, 0);
		rt_log("\n====================\n");
#endif
	}

#if 1
	if (rt_g.NMG_debug & DEBUG_BOOL)
		nmg_stash_model_to_file( "before.g", m, "Before crackshells" );
#endif

	/* Perform shell/shell intersections */
	nmg_crackshells(sA, sB, tol);

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
#if 0
		nmg_vshell( &rA->s_hd, rA );
		nmg_vshell( &rB->s_hd, rB );
#else
		/* Sometimes the tessllations of non-participating regions
		 * are damaged during a boolean operation.  Check everything.
		 */
		nmg_vmodel(m);
#endif
#if VERBOSE_VERIFY
		rt_log("\n==================== Shell A: ====== before mesh_shell_shell\n");
		nmg_pr_s_briefly(sA, 0);
		rt_log("\n==================== Shell B:\n");
		nmg_pr_s_briefly(sB, 0);
		rt_log("\n====================\n");
#endif
		if( (i = nmg_model_fuse( m, tol )) > 0 )  {
			rt_log("NOTICE: nmg_bool: fused %d entities while cracking shells\n", i);
			rt_bomb("nmg_bool() entities unfused after nmg_crackshells()\n");
		}
	}
#if 1
	/* Temporary search */
	if( nmg_has_dangling_faces( (long *)rA, (char *)NULL ) )
		rt_log("Dangling faces detected in rA before classification\n");
	if( nmg_has_dangling_faces( (long *)rB, (char *)NULL ) )
		rt_log("Dangling faces detected in rB before classification\n");
	if( nmg_has_dangling_faces( (long *)m, (char *)NULL ) )
		rt_log("Dangling faces detected in model before classification\n");
#endif

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
#if 0
		nmg_vshell( &rA->s_hd, rA );
		nmg_vshell( &rB->s_hd, rB );
#else
		/* Sometimes the tessllations of non-participating regions
		 * are damaged during a boolean operation.  Check everything.
		 */
		nmg_vmodel(m);
#endif
#if VERBOSE_VERIFY
		rt_log("\n==================== Shell A: ====== after mesh_shell_shell\n");
		nmg_pr_s_briefly(sA, 0);
		rt_log("\n==================== Shell B:\n");
		nmg_pr_s_briefly(sB, 0);
		rt_log("\n====================\n");
#endif
	}
nmg_s_radial_check( sA, tol );
nmg_s_radial_check( sB, tol );

	/*
	 *  Before splitting, join up small loop fragments into large
	 *  ones, so that maximal splits will be possible.
	 *  This is essential for cutting holes in faces, e.g. Test3.r
	 */
	nmg_s_join_touchingloops(sA, tol);
	nmg_s_join_touchingloops(sB, tol);
	if (rt_g.NMG_debug & DEBUG_BOOL)
		nmg_stash_model_to_file( "joined.g", m, "After s_join_touchingloops" );

	/* Separate any touching loops, so classifier does not have any
	 * really complex loops to do.
	 * In particular, it is necessary for (r410) to make
	 * interior (touching) loop segments into true interior loops
	 * that are separate from the exterior loop,
	 * so the classifier can assess each separately.
	 */
	nmg_s_split_touchingloops(sA, tol);
	nmg_s_split_touchingloops(sB, tol);

	/* Re-build bounding boxes, edge geometry, as needed. */
	nmg_shell_a(sA, tol);
	nmg_shell_a(sB, tol);

#if 1
	if (rt_g.NMG_debug & DEBUG_BOOL)
		nmg_stash_model_to_file( "after.g", m, "After crackshells" );
#endif

	if (rt_g.NMG_debug & DEBUG_BOOL) {
		if (rt_g.NMG_debug & DEBUG_PLOTEM) {
			if ((fd = fopen("Cracked_Shells.pl", "w")) == (FILE *)NULL) {
				(void)perror("Cracked_Shells");
				exit(-1);
			}
			rt_log("plotting Cracked_Shells.pl\n");

			nmg_pl_s(fd, sA);
			nmg_pl_s(fd, sB);
			(void)fclose(fd);

			nmg_pl_isect("isectA.pl", sA, tol);
			nmg_pl_isect("isectB.pl", sB, tol);
		}

		rt_log("check 2\n");
	}

	if (nmg_ck_closed_surf(sA, tol))
		rt_log("nmg_bool() WARNING: sA unclosed before classification.  Boldly pressing on.\n");
	if (nmg_ck_closed_surf(sB, tol))
		rt_log("nmg_bool() WARNING: sB unclosed before classification.  Boldly pressing on.\n");

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
#if 0
		nmg_vshell( &rA->s_hd, rA );
		nmg_vshell( &rB->s_hd, rB );
#else
		/* Sometimes the tessllations of non-participating regions
		 * are damaged during a boolean operation.  Check everything.
		 */
		nmg_vmodel(m);
#endif
	}

	/* Reindex structures before classification or evaluation. */
	nmg_m_reindex( m, 0 );

	/*
	 *  Allocate storage for classlist[].
	 *  Get all 8 lists at once, and just build pointers for the rest.
	 *  XXX In some cases, number of items may grow 
	 *  XXX (e.g. added vu's, loops) as things are demoted, etc.
	 *  XXX Try to accomodate here by reserving some extra table space.
	 *
	 *  XXX The classlist really only needs to be an unsigned char,
	 *  XXX not a long*.
	 */
	nelem = (m->maxindex)*4+1;		/* includes extra space */
	classlist[0] = (long *)rt_calloc( 8 * nelem + 1,
		sizeof(long), "nmg_bool classlist[8]" );
	for( i = 1; i < 8; i++ )  {
		classlist[i] = classlist[0] + i * nelem;
	}

	nmg_class_nothing_broken = 1;
	if (rt_g.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) {
		nmg_show_broken_classifier_stuff(sA, &classlist[0],
			nmg_class_nothing_broken, 1, "unclassed sA");
		nmg_show_broken_classifier_stuff(sB, &classlist[4], 1, 1, "unclassed sB");
	}

	/*
	 *  Classify A -vs- B, then B -vs- A.
	 *  Carry onAonBshared and onAonBanti classifications forward
	 *  from first step to second step.
	 *  A -vs- B live in classlist[0..3], B -vs- A live in classlist[4..7].
	 */
	nmg_class_shells(sA, sB, &classlist[0], tol);
	memcpy( (char *)classlist[4+NMG_CLASS_AonBshared],
		(char *)classlist[0+NMG_CLASS_AonBshared],
		nelem*sizeof(long) );
	memcpy( (char *)classlist[4+NMG_CLASS_AonBanti],
		(char *)classlist[0+NMG_CLASS_AonBanti],
		nelem*sizeof(long) );
	nmg_class_shells(sB, sA, &classlist[4], tol);

	if (rt_g.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) {
		nmg_class_nothing_broken = 1;

		/* Show each loop, one at a time, non-fancy */
		/* XXX Should have it's own bit, or combination -- not always wanted */
		nmg_show_each_loop(sA, &classlist[0], 1, 0, "sA lu");
		nmg_show_each_loop(sB, &classlist[4], 1, 0, "sB lu");

		/* Show each shell as a whole */
		nmg_show_broken_classifier_stuff(sA, &classlist[0], 1, 0, "sA classed");
		nmg_show_broken_classifier_stuff(sB, &classlist[4], 1, 0, "sB classed");
	}
#if 1
	if( rt_g.NMG_debug & DEBUG_BOOL )
	{
		rt_log( "Just before nmg_evaluate_boolean:\nShell A:\n" );
		nmg_pr_s_briefly( sA , 0 );
		rt_log( "Shell B:\n" );
		nmg_pr_s_briefly( sB , 0 );
	}
#endif
nmg_s_radial_check( sA, tol );
nmg_s_radial_check( sB, tol );
	nmg_evaluate_boolean( sA, sB, oper, classlist, tol);

#if 1
	if( rt_g.NMG_debug & DEBUG_BOOL )
	{
		rt_log( "Just after nmg_evaluate_boolean:\nShell A:\n" );
		nmg_pr_s_briefly( sA , 0 );
		rt_log( "Shell B:\n" );
		nmg_pr_s_briefly( sB , 0 );
	}
#endif

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vmodel(m);
		if( (i = nmg_model_fuse( m, tol )) > 0 )  {
			rt_log("ERROR: nmg_bool: fused %d entities after BOOLEAN.  Isect bug.\n", i);
			rt_bomb("nmg_bool() entities unfused after nmg_evaluate_boolean()\n");
		}
	}

	/*
	 *  nmg_evaluate_boolean() may return an invalid shell,
	 *  i.e., one that has absolutely nothing in it.
	 *  This is an indication that the shell should be deleted
	 *  from the region, an operation which can not be accomplished
	 *  this far down in the subroutine tree.
	 */
	if( !nmg_shell_is_empty(sA) )  {
nmg_s_radial_check( sA, tol );
		/* Temporary search */
		if( nmg_has_dangling_faces( (long *)rA, (char *)NULL ) )
		rt_log("Dangling faces detected in rA after boolean\n");
		if( nmg_has_dangling_faces( (long *)rB, (char *)NULL ) )
			rt_log("Dangling faces detected in rB after boolean\n");
		if( nmg_has_dangling_faces( (long *)m, (char *)NULL ) )  {
			rt_log("Dangling faces detected in model after boolean\n");
#if 0
			nmg_stash_model_to_file( "dangle.g", m, "After Boolean" );
			rt_bomb("Dangling faces detected after boolean\n");
#endif
		}

		/* Do this before table size changes */
		if (rt_g.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) {
			nmg_class_nothing_broken = 1;

			/* Show final result of the boolean */
			nmg_show_broken_classifier_stuff(sA, &classlist[0], 1, 0, "sA result");
		}

		/*  Go back and combine loops
		 *  of faces together wherever possible
		 *  to reduce the loop/edge count.
		 */
		nmg_simplify_shell(sA);
		if( rt_g.NMG_debug & DEBUG_VERIFY )
			nmg_vshell( &rA->s_hd, rA );

		/* Bounding boxes may have changed */
		nmg_shell_a(sA, tol);

		if( nmg_ck_closed_surf(sA, tol) )  {
			if( rt_g.NMG_debug )
				rt_log("nmg_bool() WARNING: sA unclosed at return, barging on.\n");
			else
				rt_bomb("nmg_bool() sA unclosed at return, aborting.\n");
		}
nmg_s_radial_check( sA, tol );
	}

	rt_free( (char *)classlist[0], "nmg_bool classlist[8]" );

	if (rt_g.NMG_debug & DEBUG_BOOL) {
		rt_log("Returning from NMG_BOOL\n");
	}
	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
#if 0
		nmg_vshell( &rA->s_hd, rA );
#else
		/* Sometimes the tessllations of non-participating regions
		 * are damaged during a boolean operation.  Check everything.
		 */
		nmg_vmodel(m);
#endif
	}

	return(sA);
}

/*
 *			N M G _ D O _ B O O L
 *
 *	BUG: we assume only one shell per region
 */
struct nmgregion *
nmg_do_bool(rA, rB, oper, tol)
struct nmgregion *rA, *rB;
CONST int		oper;
CONST struct rt_tol	*tol;
{
	struct shell		*s;
	struct nmgregion	*r;

	NMG_CK_REGION(rA);
	NMG_CK_REGION(rB);

#if 1
nmg_region_v_unique( rA, tol );
nmg_region_v_unique( rB, tol );
#endif

	s = nmg_bool(RT_LIST_FIRST(shell, &rA->s_hd),
		RT_LIST_FIRST(shell, &rB->s_hd),
		oper, tol);
	r = s->r_p;

	/* shell B was destroyed, need to eliminate region B */
	nmg_kr( rB );

	NMG_CK_SHELL(s);
	NMG_CK_REGION(r);

	/* If shell A became empty, eliminate it from the returned region */
	if( nmg_shell_is_empty(s) )
		nmg_ks(s);

	return(r);
}

/* XXX move to ??? Everything from here on down needs to go into another module */


/*
 *			N M G _ B O O L T R E E _ L E A F _ T E S S
 *
 *  Called from db_walk_tree() each time a tree leaf is encountered.
 *  The primitive solid, in external format, is provided in 'ep',
 *  and the type of that solid (e.g. ID_ELL) is in 'id'.
 *  The full tree state including the accumulated transformation matrix
 *  and the current tolerancing is in 'tsp',
 *  and the full path from root to leaf is in 'pathp'.
 *
 *  Import the solid, tessellate it into an NMG, stash a pointer to
 *  the tessellation in a new tree structure (union), and return a
 *  pointer to that.
 *
 *  Usually given as an argument to, and called from db_walk_tree().
 *
 *  This routine must be prepared to run in parallel.
 */
union tree *
nmg_booltree_leaf_tess(tsp, pathp, ep, id)
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_external	*ep;
int			id;
{
	struct rt_db_internal	intern;
	struct nmgregion	*r1;
	union tree		*curtree;
	struct directory	*dp;

	NMG_CK_MODEL(*tsp->ts_m);
	RT_CK_TOL(tsp->ts_tol);
	RT_CK_TESS_TOL(tsp->ts_ttol);

	RT_CK_FULL_PATH(pathp);
	dp = DB_FULL_PATH_CUR_DIR(pathp);
	RT_CK_DIR(dp);

	RT_INIT_DB_INTERNAL(&intern);
	if (rt_functab[id].ft_import(&intern, ep, tsp->ts_mat) < 0) {
		rt_log("nmg_booltree_leaf_tess(%s):  solid import failure\n", dp->d_namep);
	    	if (intern.idb_ptr)  rt_functab[id].ft_ifree(&intern);
	    	return(TREE_NULL);		/* ERROR */
	}
	RT_CK_DB_INTERNAL(&intern);

	if (rt_functab[id].ft_tessellate(
	    &r1, *tsp->ts_m, &intern, tsp->ts_ttol, tsp->ts_tol) < 0) {
		rt_log("nmg_booltree_leaf_tess(%s): tessellation failure\n", dp->d_namep);
		rt_functab[id].ft_ifree(&intern);
	    	return(TREE_NULL);
	}
	rt_functab[id].ft_ifree(&intern);

	NMG_CK_REGION(r1);
	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vshell( &r1->s_hd, r1 );
	}

	GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NMG_TESS;
	curtree->tr_d.td_name = rt_strdup(dp->d_namep);
	curtree->tr_d.td_r = r1;

	if (rt_g.debug&DEBUG_TREEWALK)
		rt_log("nmg_booltree_leaf_tess(%s) OK\n", dp->d_namep);

	return(curtree);
}

/*
 *			N M G _ B O O L T R E E _ E V A L U A T E
 *
 *  Given a tree of leaf nodes tesselated earlier by nmg_booltree_leaf_tess(),
 *  use recursion to do a depth-first traversal of the tree,
 *  evaluating each pair of boolean operations
 *  and reducing that result to a single nmgregion.
 *
 *  Usually called from a do_region_end() handler from db_walk_tree().
 *  For an example of several, see mged/dodraw.c.
 *
 *  Returns an OP_NMG_TESS union tree node, which will contain the
 *  resulting region and it's name, as a dynamic string.
 *  The caller is responsible for releasing the string, and the node,
 *  by calling db_free_tree() on the node.
 *
 *  It is *essential* that the caller call nmg_model_fuse() before
 *  calling this subroutine.
 *
 *  Returns NULL if there is no geometry to return.
 *
 *  Typical calls will be of this form:
 *	(void)nmg_model_fuse( m, tol );
 *	curtree = nmg_booltree_evaluate( curtree, tol );
 */
union tree *
nmg_booltree_evaluate(tp, tol)
register union tree		*tp;
CONST struct rt_tol		*tol;
{
	union tree		*tl;
	union tree		*tr;
	struct nmgregion	*reg;
	int			op;
	CONST char		*op_str;
	char			*name;

	RT_CK_TREE(tp);
	RT_CK_TOL(tol);

	switch(tp->tr_op) {
	case OP_NOP:
		return(0);
	case OP_NMG_TESS:
		/* Hit a tree leaf */
		if( rt_g.NMG_debug & DEBUG_VERIFY )  {
			nmg_vshell( &tp->tr_d.td_r->s_hd, tp->tr_d.td_r );
		}
		return tp;
	case OP_UNION:
		op = NMG_BOOL_ADD;
		op_str = " u ";
		break;
	case OP_INTERSECT:
		op = NMG_BOOL_ISECT;
		op_str = " + ";
		break;
	case OP_SUBTRACT:
		op = NMG_BOOL_SUB;
		op_str = " - ";
		break;
	default:
		rt_log("nmg_booltree_evaluate: bad op %d\n", tp->tr_op);
		return(0);
	}
	/* Handle a boolean operation node.  First get it's leaves. */
	tl = nmg_booltree_evaluate(tp->tr_b.tb_left, tol);
	tr = nmg_booltree_evaluate(tp->tr_b.tb_right, tol);
	if (tl == 0) {
		if (tr == 0)
			return 0;
		if( op == NMG_BOOL_ADD )
			return tr;
		/* For sub and intersect, if lhs is 0, result is null */
		db_free_tree(tr);
		return 0;
	}
	if (tr == 0) {
		if (tl == 0)
			return 0;
		if( op == NMG_BOOL_ISECT )  {
			db_free_tree(tl);
			return 0;
		}
		/* For sub and add, if rhs is 0, result is lhs */
		return tl;
	}
	if( tl->tr_op != OP_NMG_TESS )  rt_bomb("nmg_booltree_evaluate() bad left tree\n");
	if( tr->tr_op != OP_NMG_TESS )  rt_bomb("nmg_booltree_evaluate() bad right tree\n");

rt_log(" {%s}%s{%s}\n", tl->tr_d.td_name, op_str, tr->tr_d.td_name );

	NMG_CK_REGION(tr->tr_d.td_r);
	NMG_CK_REGION(tl->tr_d.td_r);
	if (nmg_ck_closed_region(tr->tr_d.td_r, tol) != 0)
	    	rt_log("nmg_booltree_evaluate:  WARNING, non-closed shell (r), barging ahead\n");
	if (nmg_ck_closed_region(tl->tr_d.td_r, tol) != 0)
	    	rt_log("nmg_booltree_evaluate:  WARNING, non-closed shell (l), barging ahead\n");
nmg_r_radial_check( tr->tr_d.td_r, tol );
nmg_r_radial_check( tl->tr_d.td_r, tol );

	/* input r1 and r2 are destroyed, output is new region */
	reg = nmg_do_bool(tl->tr_d.td_r, tr->tr_d.td_r, op, tol);
	tl->tr_d.td_r = NULL;
	tr->tr_d.td_r = NULL;
	NMG_CK_REGION(reg);
nmg_r_radial_check( reg, tol );

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vshell( &reg->s_hd, reg );
	}

	/* Build string of result name */
	name = (char *)rt_malloc( strlen(tl->tr_d.td_name)+3+strlen(tr->tr_d.td_name)+2+1,
		"nmg_booltree_evaluate name");
	name[0] = '(';
	strcpy( name+1, tl->tr_d.td_name );
	strcat( name+1, op_str );
	strcat( name+1, tr->tr_d.td_name );
	strcat( name+1, ")" );

	/* Clean up child tree nodes (and their names) /
	db_free_tree(tl);
	db_free_tree(tr);

	/* Convert argument binary node into a result node */
	tp->tr_op = OP_NMG_TESS;
	tp->tr_d.td_r = reg;
	tp->tr_d.td_name = name;
	return tp;
}

/*
 *			N M G _ B O O L E A N
 *
 *  This is the main application interface to the NMG Boolean Evaluator.
 *
 *  This routine has the opportunity to do "once only" operations
 *  before and after the boolean tree is walked.
 *
 *  Returns -
 *	0	Boolean went OK.  Result region is in tp->tr_d.td_r
 *	!0	Boolean produced null result.
 *
 *  The caller is responsible for freeing 'tp' in either case,
 *  typically with db_free_tree(tp);
 */
int
nmg_boolean( tp, m, tol )
register union tree		*tp;
struct model			*m;
CONST struct rt_tol		*tol;
{
	union tree	*result;
	int		ret;

	RT_CK_TREE(tp);
	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	if (rt_g.NMG_debug & (DEBUG_BOOL|DEBUG_BASIC) )  {
		rt_log("\n\nnmg_boolean( tp=x%x, m=x%x ) START\n",
			tp, m );
	}

	/*
	 *  Find all entities within geometric tolerance of each other
	 *  and "fuse" them, establishing topological sharing.
	 *  Also breaks all edges on all vertices that are within tolerance.
	 *  Operate on the entire model at once.
	 */
	(void)nmg_model_fuse( m, tol );

	/*
	 *  Evaluate the nodes of the boolean tree one at a time,
	 *  until only a single region remains.
	 */
	result = nmg_booltree_evaluate( tp, tol );
	RT_CK_TREE( result );
	if( result != tp )  rt_bomb("nmg_boolean() result of nmg_booltree_evaluate() isn't tp\n");
	if( tp->tr_op != OP_NMG_TESS )  {
		rt_log("nmg_boolean() result of nmg_booltree_evaluate() op != OP_NMG_TESS\n");
		rt_pr_tree( tp, 0 );
		ret = 1;
		goto out;
	}
	if( tp->tr_d.td_r == (struct nmgregion *)NULL )  {
		/* Pointers are all OK, but boolean produced null set */
		ret = 1;
		goto out;
	}
	ret = 0;

out:
	if (rt_g.NMG_debug & (DEBUG_BOOL|DEBUG_BASIC) )  {
		rt_log("nmg_boolean( tp=x%x, m=x%x ) END, ret=%d\n\n",
			tp, m, ret );
	}
	return ret;
}
