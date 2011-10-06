/*                 R E G I O N _ E N D _ M C . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @file libgcv/region_end.c
 *
 * Routines to process regions during a db_walk_tree using the marching cubes
 * algorithm.
 *
 */

#include "common.h"

#include "gcv.h"

/* FIXME: this be a dumb hack to avoid void* conversion */
struct gcv_data {
    void (*func)(struct nmgregion *, const struct db_full_path *, int, int, float [3]);
};


/* in region_end.c */
union tree * _gcv_cleanup(int state, union tree *tp);

union tree *
gcv_region_end_mc(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
    union tree *tp = NULL;
    struct model *m = NULL;
    struct nmgregion *r = NULL;
    struct shell *s = NULL;
    struct bu_list vhead;

    int empty_region = 0;
    int empty_model = 0;
    int NMG_debug_state = 0;
    int count = 0;

    void (*write_region)(struct nmgregion *, const struct db_full_path *, int, int, float [3]);

    if (!tsp || !pathp || !client_data) {
	bu_log("INTERNAL ERROR: gcv_region_end_mc missing parameters\n");
	return TREE_NULL;
    }

    write_region = ((struct gcv_data *)client_data)->func;
    if (!write_region) {
	bu_log("INTERNAL ERROR: gcv_region_end missing conversion callback function\n");
	return TREE_NULL;
    }

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    /*
      if (curtree->tr_op == OP_NOP)
      return 0;
    */


    /* get a copy to play with as the parameters might get clobbered
     * by a longjmp.  FIXME: db_dup_subtree() doesn't create real copies
     */
    tp = db_dup_subtree(curtree, &rt_uniresource);

    /* FIXME: we can't free curtree until we get a "real" copy form
     * db_dup_subtree().  right now we get a fake copy just so we can
     * keep the compiler quiet about clobbering curtree during longjmp
     */
    /* db_free_tree(curtree, &rt_uniresource); */

    /* Sometimes the NMG library adds debugging bits when it detects
     * an internal error, before bombing.  Stash.
     */
    NMG_debug_state = rt_g.NMG_debug;

    m = nmg_mmr();
    r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &r->s_hd);

    if(tsp->ts_rtip == NULL)
	tsp->ts_rtip = rt_new_rti(tsp->ts_dbip);

    count += nmg_mc_evaluate (s, tsp->ts_rtip, pathp, tsp->ts_ttol, tsp->ts_tol);

    /* empty region? */
    if(count == 0) {
	bu_log("Region %s appears to be empty.\n", db_path_to_string(pathp));
	return TREE_NULL;
    }

    /*
      bu_log("Target is shot, %d triangles seen.\n", count);

      bu_log("Fusing\n"); fflush(stdout);
      nmg_model_fuse(m, tsp->ts_tol);
      bu_log("Done\n"); fflush(stdout);
    */

    /* Kill cracks */
    while (BU_LIST_NOT_HEAD(&s->l, &r->s_hd)) {
	struct shell *next_s;

	next_s = BU_LIST_PNEXT(shell, &s->l);
	if (nmg_kill_cracks(s)) {
	    if (nmg_ks(s)) {
		empty_region = 1;
		break;
	    }
	}
	/*
	  nmg_shell_coplanar_face_merge(s, tsp->ts_tol, 42);
	*/
	s = next_s;
    }
    if (empty_region)
	return _gcv_cleanup(NMG_debug_state, tp);

    /* kill zero length edgeuses */
    empty_model = nmg_kill_zero_length_edgeuses(*tsp->ts_m);
    if (empty_model)
	return _gcv_cleanup(NMG_debug_state, tp);

    if (BU_SETJUMP) {
	/* Error, bail out */
	char *sofar;

	/* Relinquish bomb protection */
	BU_UNSETJUMP;

	sofar = db_path_to_string(pathp);
	bu_log("FAILED in triangulator: %s\n", sofar);
	bu_free((char *)sofar, "sofar");

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Get rid of (m)any other intermediate structures */
	if ((*tsp->ts_m)->magic == NMG_MODEL_MAGIC)
	    nmg_km(*tsp->ts_m);
	else
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");

	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();
	nmg_kr(r);

	return _gcv_cleanup(NMG_debug_state, tp);
    } else {
	/* Write the region out */
	write_region(r, pathp, tsp->ts_regionid, tsp->ts_gmater, tsp->ts_mater.ma_color);

    } BU_UNSETJUMP; /* Relinquish bomb protection */

    nmg_kr(r);

    return _gcv_cleanup(NMG_debug_state, tp);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
