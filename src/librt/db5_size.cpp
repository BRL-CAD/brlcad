/*                     D B 5 _ S I Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#include <vector>
#include <queue>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bnetwork.h"

#include "vmath.h"
#include "bu/sort.h"
#include "bu/time.h"
#include "bu/units.h"
#include "rt/db5.h"
#include "raytrace.h"
#include "librt_private.h"

/* flags for size calculations */
#define RT_DIR_SIZE_FINALIZED   0x1
#define RT_DIR_SIZE_ATTR_DONE   0x2
#define RT_DIR_SIZE_CHILDREN_DONE   0x4
#define RT_DIR_SIZE_ACTIVE      0x8

/* sizes in directory pointer arrays */
#define RT_DIR_SIZE_OBJ 0
#define RT_DIR_SIZE_TREE_INSTANCED 1
#define RT_DIR_SIZE_TREE_DEINSTANCED 2

#define HSIZE(buf, var) \
    (void)bu_humanize_number(buf, 5, (int64_t)var, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);

struct db5_sizecalc {
    int s_flags;		/**< @brief flags having to do with the state of size calculations */
    long sizes[3];              /**< @brief calculated sizes of directory object */
    long sizes_wattr[3];        /**< @brief calculated sizes of directory object including attributes */
    struct directory **children;/**< @brief RT_DIR_NULL terminated array of child objects of this object */
    int queue_flag;
    void *data; 		/**< @brief void pointer hook for misc user data. */
};
#define DB5SIZE(_dp) ((struct db5_sizecalc *)_dp->u_data)

HIDDEN int
db_get_external_reuse(struct bu_external *ep, const struct directory *dp, const struct db_i *dbip)
{
    ep->ext_nbytes = dp->d_len;

    if (dp->d_flags & RT_DIR_INMEM) {
	memcpy((char *)ep->ext_buf, dp->d_un.ptr, ep->ext_nbytes);
	return 0;
    }

    if (db_read(dbip, (char *)ep->ext_buf, ep->ext_nbytes, dp->d_addr) < 0) {
	memset(ep->ext_buf, 0, dp->d_len);
	ep->ext_nbytes = 0;
	return -1;      /* VERY BAD */
    }
    return 0;
}

// TODO - figure out what to do about submodel...
#define DB5COMB_TOKEN_LEAF 1
HIDDEN int
_db5_children(
	const struct directory *dp,
	const struct db_i *dbip,
	int have_extern,
       	struct bu_external *ext,
       	struct directory ***children)
{
    struct db5_raw_internal raw;
    struct directory **c = NULL;
    int dpcnt = 0;

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	int wid;
	size_t ius;
	size_t nmat, nleaf, rpn_len, max_stack_depth;
	size_t leafbytes;
	unsigned char *leafp;
	unsigned char *leafp_end;
	unsigned char *exprp;
	unsigned char *cp;

	if (!have_extern) {
	    if (db_get_external_reuse(ext, dp, dbip) < 0) return 0;
	}
	if (db5_get_raw_internal_ptr(&raw, ext->ext_buf) == NULL) return 0;
	if (!raw.body.ext_buf) return 0;

	//bu_log("children of: %s\n", dp->d_namep);

	cp = raw.body.ext_buf;
	wid = *cp++;
	cp += db5_decode_length(&nmat, cp, wid);
	cp += db5_decode_length(&nleaf, cp, wid);
	cp += db5_decode_length(&leafbytes, cp, wid);
	cp += db5_decode_length(&rpn_len, cp, wid);
	cp += db5_decode_length(&max_stack_depth, cp, wid);
	leafp = cp + nmat * (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE);
	exprp = leafp + leafbytes;
	leafp_end = exprp;


	if (rpn_len == 0) {
	    ssize_t is;
	    c = (struct directory **)bu_calloc(nleaf + 1, sizeof(struct directory *), "children");
	    for (is = nleaf-1; is >= 0; is--) {
		size_t mi;
		struct directory *ndp = db_lookup(dbip, (const char *)leafp, LOOKUP_QUIET);
		leafp += strlen((const char *)leafp) + 1;
		mi = 4095;                  /* sanity */
		leafp += db5_decode_signed(&mi, leafp, wid);
		if (ndp) {
		    c[dpcnt] = ndp;
		    dpcnt++;
		}
	    }
	} else {
	    c = (struct directory **)bu_calloc(rpn_len + 1, sizeof(struct directory *), "children");
	    for (ius = 0; ius < rpn_len; ius++, exprp++) {
		size_t mi;
		struct directory *ndp = RT_DIR_NULL;
		switch (*exprp) {
		    case DB5COMB_TOKEN_LEAF:
			ndp = db_lookup(dbip, (const char *)leafp, LOOKUP_QUIET);
			leafp += strlen((const char *)leafp) + 1;
			/* Get matrix index */
			mi = 4095;                      /* sanity */
			leafp += db5_decode_signed(&mi, leafp, wid);
			if (ndp) {
			    c[dpcnt] = ndp;
			    dpcnt++;
			}
			break;
		    default:
			break;
		}
	    }
	    BU_ASSERT(leafp == leafp_end);
	}

	if (c) {
	    c[dpcnt] = RT_DIR_NULL;
	    (*children) = c;
	}
	return dpcnt;

    } else  if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE || dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE) {
	char *sketch_name = NULL;
	unsigned char *ptr;
	struct directory *ndp = RT_DIR_NULL;

	if (!have_extern) {
	    if (db_get_external_reuse(ext, dp, dbip) < 0) return 0;
	}
	if (db5_get_raw_internal_ptr(&raw, ext->ext_buf) == NULL) return 0;
	if (!raw.body.ext_buf) return 0;

	ptr = (unsigned char *)raw.body.ext_buf;
	if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
	    sketch_name = (char *)ptr + ELEMENTS_PER_VECT*4*SIZEOF_NETWORK_DOUBLE + SIZEOF_NETWORK_LONG;
	}
	if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE) {
	    sketch_name = (char *)ptr + (ELEMENTS_PER_VECT*3 + 1)*SIZEOF_NETWORK_DOUBLE;
	}
	if (!sketch_name) return 0;

	if (db_lookup(dbip, sketch_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	    c = (struct directory **)bu_calloc(dpcnt + 1, sizeof(struct directory *), "children");
	    c[0] = ndp;
	    c[1] = RT_DIR_NULL;
	    dpcnt++;
	}
	return dpcnt;
    } else if (dp->d_minor_type ==  DB5_MINORTYPE_BRLCAD_DSP) {
	struct directory *ndp = RT_DIR_NULL;
	unsigned char *cp;
	char dsp_datasrc;

	if (!have_extern) {
	    if (db_get_external_reuse(ext, dp, dbip) < 0) return 0;
	}
	if (db5_get_raw_internal_ptr(&raw, ext->ext_buf) == NULL) return 0;
	if (!raw.body.ext_buf) return 0;

	cp = (unsigned char *)raw.body.ext_buf;
	cp += 2*SIZEOF_NETWORK_LONG + SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_MAT + SIZEOF_NETWORK_SHORT;
	dsp_datasrc = *cp;
	cp++; cp++;
	switch (dsp_datasrc) {
	    case RT_DSP_SRC_V4_FILE:
		/* TODO - get file size and add it to obj size */
		break;
	    case RT_DSP_SRC_FILE:
		/* TODO - get file size and add it to obj size */
		break;
	    case RT_DSP_SRC_OBJ:
		ndp = db_lookup(dbip, (char *)cp, LOOKUP_QUIET);
		break;
	    default:
		bu_log("%s: invalid DSP datasrc: '%c'\n", (char *)cp, dsp_datasrc);
		break;
	}

	if (ndp != RT_DIR_NULL) {
	    c = (struct directory **)bu_calloc(dpcnt + 1, sizeof(struct directory *), "children");
	    c[0] = ndp;
	    c[1] = RT_DIR_NULL;
	    dpcnt++;
	}
	return dpcnt;
    }

    /* If it's none of the above, it has no children. */
    return 0;
}


HIDDEN long
_db5_get_attributes_size(struct bu_external *ext, const struct db_i *dbip, const struct directory *dp)
{
    struct db5_raw_internal raw;
    if (db_get_external_reuse(ext, dp, dbip) < 0) return 0;
    if (db5_get_raw_internal_ptr(&raw, ext->ext_buf) == NULL) return 0;
    return (long)(raw.attributes.ext_nbytes);
}

long
db5_size(struct db_i *dbip, struct directory *in_dp, int flags)
{
    int local_flags = flags;
    int size_flag_cnt = 0;
    int i, j, finalized, finalized_prev, active_prev;
    int active = 0;
    int dcnt = 0;
    int wcnt = 0;
    struct directory *dp;
    struct directory **dps;
    struct db5_sizecalc *dsr;
    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    unsigned int max_bufsize = 0;
    long fsize = 0;

    if (!dbip) return 0;
    if (!in_dp) return 0;
    if (flags & DB_SIZE_OBJ) size_flag_cnt++;
    if (flags & DB_SIZE_TREE_INSTANCED) size_flag_cnt++;
    if (flags & DB_SIZE_TREE_DEINSTANCED) size_flag_cnt++;

    if (!size_flag_cnt) {
	local_flags |= DB_SIZE_OBJ;
    }
    BU_EXTERNAL_INIT(&ext);

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
	fsize = 0;
	if (local_flags & DB_SIZE_ATTR) {
	    ext.ext_buf = (uint8_t *)bu_realloc(ext.ext_buf, in_dp->d_len, "resize ext_buf");
	    fsize += _db5_get_attributes_size(&ext, dbip, in_dp);
	}
	fsize += in_dp->d_len;
	if (ext.ext_buf) bu_free(ext.ext_buf, "free ext_buf");
	return fsize;
    }

    /* It's not one of the simple cases - we've been asked for a definition that
     * requires awareness of some portion of the hierarchy. */


    /* Get a count of all objects we might care about, and find out what the
     * size of our biggest object is. */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		dcnt++;
		if (dp->d_len > max_bufsize) max_bufsize = dp->d_len;
	    }
	}
    }

    /* Make he array to hold usable struct directory pointers */
    /* Also, make the containers that will hold size specific data */
    dps = (struct directory **)bu_calloc(dcnt+1, sizeof(struct directory *), "sortable directory pointer array *");
    dsr = (struct db5_sizecalc *)bu_calloc(dcnt+1, sizeof(db5_sizecalc), "per-dp size information");

    /* TODO - make reusable directory pointer array container to test on-the-fly child lookup as oppose to lookup-and-store */

   /* Make sure the bu_external buffer has as much memory as it will ever need to handle any one object */
    ext.ext_buf = (uint8_t *)bu_realloc(ext.ext_buf, max_bufsize, "resize ext_buf");

    /* Associate the local struct with the directory pointer and put ptr in array */
    j = 0;
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		dsr[j].data = dp->u_data;
		dp->u_data = (void *)(&(dsr[j]));
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
	    DB5SIZE(dp)->s_flags = 0;
	    if (DB5SIZE(dp)->children) bu_free(DB5SIZE(dp)->children, "free child dp ptr array");
	    DB5SIZE(dp)->children = NULL;
	    for (int dpcind = 0; dpcind != 3; dpcind++) DB5SIZE(dp)->sizes[dpcind] = 0;
	    for (int dpcind = 0; dpcind != 3; dpcind++) DB5SIZE(dp)->sizes_wattr[dpcind] = 0;
	} else {
	    DB5SIZE(dp)->s_flags = DB5SIZE(dp)->s_flags & ~(RT_DIR_SIZE_ACTIVE);
	}
    }

    /* Activate our directory object of interest. */
    DB5SIZE(in_dp)->s_flags |= RT_DIR_SIZE_ACTIVE;
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
    wcnt = dcnt;

    int64_t s1, s2;
    s1 = bu_gettime();
    while (finalized != finalized_prev || active != active_prev) {
	finalized_prev = finalized;
	active_prev = active;
	for (i = 0; i < wcnt; i++) {
	    dp = dps[i];
	    if ((DB5SIZE(dp)->s_flags & RT_DIR_SIZE_ACTIVE) && !(DB5SIZE(dp)->s_flags & RT_DIR_SIZE_FINALIZED)) {
		int have_extern = 0;
		if (!(DB5SIZE(dp)->s_flags & RT_DIR_SIZE_ATTR_DONE)) {
		    DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_OBJ] = _db5_get_attributes_size(&ext, dbip, dp);
		    DB5SIZE(dp)->s_flags |= RT_DIR_SIZE_ATTR_DONE;
		    have_extern = 1;
		}

		struct directory *cdp;
		int children_finalized = 1;
		if (!(DB5SIZE(dp)->s_flags & RT_DIR_SIZE_CHILDREN_DONE)) {
		    if (DB5SIZE(dp)->children) bu_free(DB5SIZE(dp)->children, "free old dp child list");
		    DB5SIZE(dp)->children = NULL;
		    (void)_db5_children(dp, dbip, have_extern, &ext, &(DB5SIZE(dp)->children));
		    DB5SIZE(dp)->s_flags |= RT_DIR_SIZE_CHILDREN_DONE;
		}

		if (DB5SIZE(dp)->children) {
		    int cind = 0;
		    cdp = DB5SIZE(dp)->children[0];
		    while (cdp != RT_DIR_NULL) {
			if (!(DB5SIZE(cdp)->s_flags & RT_DIR_SIZE_FINALIZED)) children_finalized = 0;
			if (!(DB5SIZE(cdp)->s_flags & RT_DIR_SIZE_ACTIVE)) active++;
			DB5SIZE(cdp)->s_flags |= RT_DIR_SIZE_ACTIVE;
			cind++;
			cdp = DB5SIZE(dp)->children[cind];
		    }
		    if (children_finalized) {
			/* Handle the xpushed size, which is just the sum of all of the
			 * xpushed sizes of the children plus this object. */
			cind = 0;
			cdp = DB5SIZE(dp)->children[0];
			while (cdp) {
			    //bu_log("XPUSH %s: adding %s (%ld)\n", dp->d_namep, cdp->d_namep, DB5SIZE(cdp)->sizes[RT_DIR_SIZE_TREE_DEINSTANCED]);
			    DB5SIZE(dp)->sizes[RT_DIR_SIZE_TREE_DEINSTANCED] += DB5SIZE(cdp)->sizes[RT_DIR_SIZE_TREE_DEINSTANCED];
			    DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_TREE_DEINSTANCED] += DB5SIZE(cdp)->sizes_wattr[RT_DIR_SIZE_TREE_DEINSTANCED];
			    cind++;
			    cdp = DB5SIZE(dp)->children[cind];
			}
			DB5SIZE(dp)->sizes[RT_DIR_SIZE_TREE_DEINSTANCED] += dp->d_len;
			DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_TREE_DEINSTANCED] += (dp->d_len + DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_OBJ]);

			/* Now that we've handled the hierarchy, finalize the obj numbers */
			DB5SIZE(dp)->sizes[RT_DIR_SIZE_OBJ] = dp->d_len;
			DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_OBJ] += dp->d_len;

			/* Mark the comb as done */
			DB5SIZE(dp)->s_flags |= RT_DIR_SIZE_FINALIZED;
			finalized++;
		    }
		} else {
		    /* No children - it's just the object */
		    DB5SIZE(dp)->sizes[RT_DIR_SIZE_OBJ] = dp->d_len;
		    DB5SIZE(dp)->sizes[RT_DIR_SIZE_TREE_INSTANCED] = dp->d_len;
		    DB5SIZE(dp)->sizes[RT_DIR_SIZE_TREE_DEINSTANCED] = dp->d_len;
		    DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_OBJ] += dp->d_len;
		    DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_TREE_INSTANCED] += DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_OBJ];
		    DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_TREE_DEINSTANCED] += DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_OBJ];
		    DB5SIZE(dp)->s_flags |= RT_DIR_SIZE_FINALIZED;
		    finalized++;
		}
	    }
	}
    }

    s2 = bu_gettime() - s1;
    bu_log("processing time: %f\n", s2 / 1000000.0);

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

    if (!cycl_finalized) {
	fsize = 0;
	goto db5size_freemem;
    }

    /* IFF we need it, calculate the keep size for dp.  This is harder than the
     * other calculations, because the size is (possibly) smaller than the sum
     * of the children if the child combs share objects. We must find all
     * unique directory objects and sum over that unique set.  Because of the
     * finalize guard, we do not need to be concerned in this logic with
     * possible cyclic paths. Since the keep size for an object doesn't help in
     * calculating other objects' sizes, we evaluate it only for the specific
     * object of interest rather than mass evaluating it in the loop. */
    if (local_flags & DB_SIZE_TREE_INSTANCED && !DB5SIZE(in_dp)->sizes[RT_DIR_SIZE_TREE_INSTANCED]) {
	std::vector<struct directory *> uniq;
	std::queue<struct directory *> q;
	struct directory *cdp;
	int cind = 0;
	q.push(in_dp);
	while (!q.empty()) {
	    struct directory *qdp = q.front();
	    q.pop();
	    if (qdp->u_data && !(DB5SIZE(qdp)->queue_flag)) {
		DB5SIZE(qdp)->queue_flag = 1;
		uniq.push_back(qdp);
	    }
	    cind = 0;
	    cdp = (DB5SIZE(qdp)->children) ? DB5SIZE(qdp)->children[0] : NULL;
	    while (cdp) {
		if (cdp->u_data && !(DB5SIZE(cdp)->queue_flag)) q.push(cdp);
		cind++;
		cdp = DB5SIZE(qdp)->children[cind];
	    }
	}
	for (std::vector<struct directory *>::iterator di = uniq.begin(); di != uniq.end(); di++) {
	    DB5SIZE(in_dp)->sizes[RT_DIR_SIZE_TREE_INSTANCED] += DB5SIZE((*di))->sizes[RT_DIR_SIZE_OBJ];
	    DB5SIZE(in_dp)->sizes_wattr[RT_DIR_SIZE_TREE_INSTANCED] += DB5SIZE((*di))->sizes_wattr[RT_DIR_SIZE_OBJ];
	}
    }

    /* We now have what we need to return the requested size */
    if (local_flags & DB_SIZE_ATTR) {
	if (local_flags & DB_SIZE_TREE_INSTANCED) {
	    fsize = DB5SIZE(in_dp)->sizes_wattr[RT_DIR_SIZE_TREE_INSTANCED];
	} else if (local_flags & DB_SIZE_TREE_DEINSTANCED) {
	    fsize = DB5SIZE(in_dp)->sizes_wattr[RT_DIR_SIZE_TREE_DEINSTANCED];
	}
    } else {
	if (local_flags & DB_SIZE_TREE_INSTANCED) {
	    fsize = DB5SIZE(in_dp)->sizes[RT_DIR_SIZE_TREE_INSTANCED];
	} else if (local_flags & DB_SIZE_TREE_DEINSTANCED) {
	    fsize = DB5SIZE(in_dp)->sizes[RT_DIR_SIZE_TREE_DEINSTANCED];
	}
    }

#if 0
    char hlen[6] = { '\0' };
    char hlen2[6] = { '\0' };
    HSIZE(hlen, DB5SIZE(dp)->sizes[RT_DIR_SIZE_OBJ]);
    HSIZE(hlen2, DB5SIZE(dp)->sizes_wattr[RT_DIR_SIZE_OBJ]);
    bu_log("%s/%s: %s\n", hlen, hlen2, dp->d_namep);
#endif

db5size_freemem:
    /* Free memory */
    for(i = 0; i < dcnt; i++) {
	if (dsr[i].children) bu_free(dsr[i].children, "child array");
    }
    /* Put the original u_data pointers back */
    for (i = 0; i < dcnt; i++) {
	dps[i]->u_data = DB5SIZE(dps[i])->data;
    }
    bu_free(dsr, "data size containers");
    bu_free(dps, "old u_data pointers");
    if (ext.ext_buf) bu_free(ext.ext_buf, "final free of ext_buf");
    return fsize;
}

/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

