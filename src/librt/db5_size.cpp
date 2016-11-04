/*                     D B 5 _ S I Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
/** @addtogroup db5 */
/** @{ */
/** @file librt/db5_size.c
 *
 * Calculate sizes of v5 database objects.
 *
 */

#include "common.h"

#include <set>
#include <queue>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bnetwork.h"

#include "vmath.h"
#include "bu/units.h"
#include "rt/db5.h"
#include "raytrace.h"
#include "librt_private.h"

/* flags for size calculations */
#define RT_DIR_SIZE_FINALIZED   0x1
#define RT_DIR_SIZE_ATTR_DONE   0x2
#define RT_DIR_SIZE_COMB_DONE   0x4
#define RT_DIR_SIZE_ACTIVE      0x8

/* sizes in directory pointer arrays */
#define RT_DIR_SIZE_OBJ 0
#define RT_DIR_SIZE_KEEP 1
#define RT_DIR_SIZE_XPUSH 2

#define HSIZE(buf, var) \
    (void)bu_humanize_number(buf, 5, (int64_t)var, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);


HIDDEN long
_db5_get_attributes_size(const struct db_i *dbip, const struct directory *dp)
{
    long attr_size = 0;
    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    struct db5_raw_internal raw;
    if (dbip->dbi_version < 5) return 0; /* db4 has no attributes */
    if (db_get_external(&ext, dp, dbip) < 0) return 0;
    if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
	bu_free_external(&ext);
	return 0;
    }
    attr_size = raw.attributes.ext_nbytes;
    bu_free_external(&ext);
    return attr_size;
}

long
db5_size(struct db_i *dbip, struct directory *in_dp, int flags)
{
    int local_flags = flags;
    int size_flag_cnt = 0;
    int i, j, finalized, finalized_prev, active_prev;
    int active = 0;
    int dcnt = 0;
    int *s_lflags;
    struct directory *dp;
    struct directory **dps;
    if (!dbip) return 0;
    if (!in_dp) return 0;
    if (flags & DB_SIZE_OBJ) size_flag_cnt++;
    if (flags & DB_SIZE_KEEP) size_flag_cnt++;
    if (flags & DB_SIZE_XPUSH) size_flag_cnt++;

    if (!size_flag_cnt) {
	local_flags |= DB_SIZE_OBJ;
    }

    /* If we're being asked for a) the isolated size of the object or b) any
     * size for a self-contained solid, take a short cut. DB_SIZE_OBJ is always
     * the d_len, and if the object is not a comb or one of the primitives that
     * references other primitives it's KEEP and XPUSH sizes are also just the
     * object size. In that situation, we don't need the more elaborate iterative
     * logic and can just return the local size. */
    if ((local_flags & DB_SIZE_OBJ) || (!(in_dp->d_flags & RT_DIR_COMB) &&
	    !(in_dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) &&
	    !(in_dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE) &&
	    !(in_dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP))) {
	long fsize = 0;
	if (local_flags & DB_SIZE_ATTR) {
	    fsize += _db5_get_attributes_size(dbip, in_dp);
	}
	fsize += in_dp->d_len;	
	return fsize;
    }

    /* It's not one of the simple cases - we've been asked for a definition that
     * requires awareness of some portion of the hierarchy. */

    /* Get a count of all objects we might care about. */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_HIDDEN)) dcnt++;
	}
    }
    s_lflags = (int *)bu_calloc(dcnt+1, sizeof(int), "local size flags");
    dps = (struct directory **)bu_calloc(dcnt+1, sizeof(struct directory *), "sortable array");

    /* Associate the local flag with the directory pointer */
    j = 0;
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		dp->u_data = (void *)(&(s_lflags[j]));
		dps[j] = dp;
		j++;
	    }
	}
    }

    /* If we are asked to force a recalculation, redo everything - otherwise,
     * just deactivate everything. */
    for (i = 0; i < dcnt; i++) {
	dp = dps[i];
	if (flags & DB_SIZE_FORCE_RECALC) {
	    dp->s_flags = 0;
	    if (dp->children) bu_free(dp->children, "free child dp ptr array");
	    dp->children = NULL;
	    for (int dpcind = 0; dpcind != 3; dpcind++) dp->sizes[dpcind] = 0;
	    for (int dpcind = 0; dpcind != 3; dpcind++) dp->sizes_wattr[dpcind] = 0;
	} else {
	    dp->s_flags = dp->s_flags & ~(RT_DIR_SIZE_ACTIVE);
	}
    }

    /* Activate our directory object of interest. */
    in_dp->s_flags |= RT_DIR_SIZE_ACTIVE;
    active++;

    /* Now that we have our initial setup, start calculating sizes.  Each pass should
     * resolve at least one size - if no sizes are resolved, we either are done or
     * there is a cyclic loop in the database.  We can't do all combs at once after
     * processing the solids because of the possibility of cyclic loops - those would
     * trigger an infinite loop when walking the child trees. "finalization" is the
     * indication that a comb is safe to process. */
    finalized = 0;
    finalized_prev = -1;
    active_prev = -1;
    while (finalized != finalized_prev || active != active_prev) {
	finalized_prev = finalized;
	active_prev = active;
	for (i = 0; i < dcnt; i++) {
	    dp = dps[i];
	    if ((dp->s_flags & RT_DIR_SIZE_ACTIVE) && !(dp->s_flags & RT_DIR_SIZE_FINALIZED)) {
		if (!(dp->s_flags & RT_DIR_SIZE_ATTR_DONE)) {
		    dp->sizes_wattr[RT_DIR_SIZE_OBJ] = _db5_get_attributes_size(dbip, dp);
		    dp->s_flags |= RT_DIR_SIZE_ATTR_DONE;
		}

		if (dp->d_flags & RT_DIR_COMB) {
		    struct directory *cdp;
		    int children_finalized = 1;
		    if (!(dp->s_flags & RT_DIR_SIZE_COMB_DONE)) {
			struct rt_db_internal in;
			struct rt_comb_internal *comb;
			if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0) continue;
			comb = (struct rt_comb_internal *)in.idb_ptr;
			if (dp->children) bu_free(dp->children, "free old dp child list");
			dp->children = db_comb_children(dbip, comb, NULL, NULL);
			dp->s_flags |= RT_DIR_SIZE_COMB_DONE;
			rt_db_free_internal(&in);
		    }

		    if (dp->children) {
			int cind = 0;
			cdp = dp->children[0];
			while (cdp != RT_DIR_NULL) {
			    if (!(cdp->s_flags & RT_DIR_SIZE_FINALIZED)) children_finalized = 0;
			    if (!(cdp->s_flags & RT_DIR_SIZE_ACTIVE)) active++;
			    cdp->s_flags |= RT_DIR_SIZE_ACTIVE;
			    cind++;
			    cdp = dp->children[cind];
			}
			if (children_finalized) {
			    /* Handle the xpushed size, which is just the sum of all of the
			     * xpushed sizes of the children plus this object. */
			    cind = 0;
			    cdp = dp->children[0];
			    while (cdp) {
				//bu_log("XPUSH %s: adding %s (%ld)\n", dp->d_namep, cdp->d_namep, cdp->sizes[RT_DIR_SIZE_XPUSH]);
				dp->sizes[RT_DIR_SIZE_XPUSH] += cdp->sizes[RT_DIR_SIZE_XPUSH];
				dp->sizes_wattr[RT_DIR_SIZE_XPUSH] += cdp->sizes_wattr[RT_DIR_SIZE_XPUSH];
				cind++;
				cdp = dp->children[cind];
			    }
			    dp->sizes[RT_DIR_SIZE_XPUSH] += dp->d_len;
			    dp->sizes_wattr[RT_DIR_SIZE_XPUSH] += (dp->d_len + dp->sizes_wattr[RT_DIR_SIZE_OBJ]);

			    /* Now that we've handled the hierarchy, finalize the obj numbers */
			    dp->sizes[RT_DIR_SIZE_OBJ] = dp->d_len;
			    dp->sizes_wattr[RT_DIR_SIZE_OBJ] += dp->d_len;

			    /* Mark the comb as done */
			    dp->s_flags |= RT_DIR_SIZE_FINALIZED;
			    finalized++;
			}
		    } else {
			/* No children - it's just the comb */
			dp->sizes[RT_DIR_SIZE_OBJ] = dp->d_len;
			dp->sizes[RT_DIR_SIZE_KEEP] = dp->d_len;
			dp->sizes[RT_DIR_SIZE_XPUSH] = dp->d_len;
			dp->sizes_wattr[RT_DIR_SIZE_OBJ] += dp->d_len;
			dp->sizes_wattr[RT_DIR_SIZE_KEEP] += dp->sizes_wattr[RT_DIR_SIZE_OBJ];
			dp->sizes_wattr[RT_DIR_SIZE_XPUSH] += dp->sizes_wattr[RT_DIR_SIZE_OBJ];
			dp->s_flags |= RT_DIR_SIZE_FINALIZED;
			finalized++;
		    }
		} else {
		    /* This is not a comb - handle the solids that have other data
		     * associated with them and otherwise use dp->d_len */
		    struct directory *edp = NULL;
		    struct rt_db_internal in;

		    if (!(dp->s_flags & RT_DIR_SIZE_ATTR_DONE)) {
			dp->sizes_wattr[RT_DIR_SIZE_OBJ] = _db5_get_attributes_size(dbip, dp);
			dp->s_flags |= RT_DIR_SIZE_ATTR_DONE;
			//bu_log("%s attr size: %d\n", dp->d_namep, dp->sizes_wattr[RT_DIR_SIZE_OBJ]);
		    }
		    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
			struct rt_extrude_internal *extr;
			if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0) continue;
			extr = (struct rt_extrude_internal *)in.idb_ptr;
			if (extr->sketch_name) {
			    edp = db_lookup(dbip, extr->sketch_name, LOOKUP_QUIET);
			}
			rt_db_free_internal(&in);
		    } else if (dp->d_minor_type ==  DB5_MINORTYPE_BRLCAD_REVOLVE) {
			struct rt_revolve_internal *revolve;
			if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0) continue;
			revolve = (struct rt_revolve_internal *)in.idb_ptr;
			if (bu_vls_strlen(&revolve->sketch_name) > 0) {
			    edp = db_lookup(dbip, bu_vls_addr(&revolve->sketch_name), LOOKUP_QUIET);
			}
			rt_db_free_internal(&in);
		    } else if (dp->d_minor_type ==  DB5_MINORTYPE_BRLCAD_DSP) {
			struct rt_dsp_internal *dsp;
			if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0) continue;
			dsp = (struct rt_dsp_internal *)in.idb_ptr;
			/* TODO - handle other dsp_datasrc cases */
			if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ && bu_vls_strlen(&dsp->dsp_name) > 0) {
			    edp = db_lookup(dbip, bu_vls_addr(&dsp->dsp_name), LOOKUP_QUIET);
			}
			rt_db_free_internal(&in);
		    }
		    if (edp) {
			if (!(edp->s_flags & RT_DIR_SIZE_FINALIZED)) {
			    if (!(edp->s_flags & RT_DIR_SIZE_ACTIVE)) active++;
			    edp->s_flags |= RT_DIR_SIZE_ACTIVE;
			    continue;
			} else {
			    dp->sizes[RT_DIR_SIZE_OBJ] += edp->sizes[RT_DIR_SIZE_OBJ];
			    dp->sizes[RT_DIR_SIZE_KEEP] += edp->sizes[RT_DIR_SIZE_OBJ];
			    dp->sizes[RT_DIR_SIZE_XPUSH] += edp->sizes[RT_DIR_SIZE_OBJ];
			    dp->sizes_wattr[RT_DIR_SIZE_OBJ] += edp->sizes_wattr[RT_DIR_SIZE_OBJ];
			    dp->sizes_wattr[RT_DIR_SIZE_KEEP] += edp->sizes_wattr[RT_DIR_SIZE_OBJ];
			    dp->sizes_wattr[RT_DIR_SIZE_XPUSH] += edp->sizes_wattr[RT_DIR_SIZE_XPUSH];
			}
		    }
		    dp->sizes[RT_DIR_SIZE_OBJ] += dp->d_len;
		    dp->sizes[RT_DIR_SIZE_KEEP] += dp->d_len;
		    dp->sizes[RT_DIR_SIZE_XPUSH] += dp->d_len;
		    dp->sizes_wattr[RT_DIR_SIZE_OBJ] += dp->d_len;
		    dp->sizes_wattr[RT_DIR_SIZE_KEEP] += dp->sizes_wattr[RT_DIR_SIZE_OBJ];
		    dp->sizes_wattr[RT_DIR_SIZE_XPUSH] += dp->sizes_wattr[RT_DIR_SIZE_OBJ];
		    dp->s_flags |= RT_DIR_SIZE_FINALIZED;
		    finalized++;
		}
	    }
	}
    }

    /* Now that we have completed our size calculations, see if there are any active but
     * unfinalized directory objects.  These will be an indication of a cyclic loop and
     * require returning a -1 error code (since the size of geometry definitions containing
     * a cyclic loop is meaningless - arguably the evaluated size is infinite */
    int cycl_finalized = 1;
    for (i = 0; i < dcnt; i++) {
	dp = dps[i];
	if (!(dp->d_flags & RT_DIR_HIDDEN)) {
	    if (dp->d_flags & RT_DIR_SIZE_ACTIVE && !(dp->d_flags & RT_DIR_SIZE_FINALIZED)) {
		bu_log("unfinalized object size: %s\n", dp->d_namep);
		cycl_finalized = 0;
	    }
	}
    }

    if (!cycl_finalized) return 0;

    /* IFF we need it, calculate the keep size for dp.  This is harder than the
     * other calculations, because the size is (possibly) smaller than the sum
     * of the children if the child combs share objects. We must find all
     * unique directory objects and sum over that unique set.  Because of the
     * finalize guard, we do not need to be concerned in this logic with
     * possible cyclic paths. Since the keep size for an object doesn't help in
     * calculating other objects' sizes, we evaluate it only for the specific
     * object of interest rather than mass evaluating it in the loop. */
    if (local_flags & DB_SIZE_KEEP && !in_dp->sizes[RT_DIR_SIZE_KEEP]) {
	std::set<struct directory *> uniq;
	std::queue<struct directory *> q;
	struct directory *cdp;
	int cind = 0;
	q.push(in_dp);
	while (!q.empty()) {
	    struct directory *qdp = q.front();
	    q.pop();
	    if (qdp->u_data) {
		*((int *)(qdp->u_data)) = 1;
		uniq.insert(qdp);
	    }
	    cind = 0;
	    cdp = (qdp->children) ? qdp->children[0] : NULL;
	    while (cdp) {
		if (qdp->u_data && !(*((int *)(cdp->u_data)))) q.push(cdp);
		cind++;
		cdp = qdp->children[cind];
	    }
	}
	for (std::set<struct directory *>::iterator di = uniq.begin(); di != uniq.end(); di++) {
	    in_dp->sizes[RT_DIR_SIZE_KEEP] += (*di)->sizes[RT_DIR_SIZE_OBJ];
	    in_dp->sizes_wattr[RT_DIR_SIZE_KEEP] += (*di)->sizes_wattr[RT_DIR_SIZE_OBJ];
	}
    }

    /* We now have what we need to return the requested size */
    long fsize = 0;
    if (local_flags & DB_SIZE_ATTR) {
	if (local_flags & DB_SIZE_KEEP) {
	    fsize = in_dp->sizes_wattr[RT_DIR_SIZE_KEEP];
	} else if (local_flags & DB_SIZE_XPUSH) {
	    fsize = in_dp->sizes_wattr[RT_DIR_SIZE_XPUSH];
	}
    } else {
	if (local_flags & DB_SIZE_KEEP) {
	    fsize = in_dp->sizes[RT_DIR_SIZE_KEEP];
	} else if (local_flags & DB_SIZE_XPUSH) {
	    fsize = in_dp->sizes[RT_DIR_SIZE_XPUSH];
	}
    }

#if 0
    char hlen[6] = { '\0' };
    char hlen2[6] = { '\0' };
    HSIZE(hlen, dp->sizes[RT_DIR_SIZE_OBJ]);
    HSIZE(hlen2, dp->sizes_wattr[RT_DIR_SIZE_OBJ]);
    bu_log("%s/%s: %s\n", hlen, hlen2, dp->d_namep);
#endif
    return fsize;
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
