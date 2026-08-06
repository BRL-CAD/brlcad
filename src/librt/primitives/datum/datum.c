/*                         D A T U M . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/datum/datum.c
 *
 * Implement support for datum reference entities.  The basic
 * structural container supports fairly arbitrary collections of
 * points, lines, or planes which can be used to represent reference
 * features, axes, and coordinate systems.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bnetwork.h"

#include "bu/cv.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/db5.h"
#include "rt/geom.h"
#include "rt/primitives/datum.h"
#include "raytrace.h"

/* local interface header */
#include "./datum.h"

/* maximum number of values a datum may have (positioned plane case) */
#define MAX_VALS (ELEMENTS_PER_POINT + ELEMENTS_PER_VECT + 1) /* for w */
#define DATUM_EXT_MAGIC 0x44415432u /* "DAT2" */
#define DATUM_EXT_VERSION 1u


rt_datum_type
rt_datum_resolved_type(const struct rt_datum_internal *datum)
{
    if (!datum)
	return RT_DATUM_AUTO;
    RT_DATUM_CK_MAGIC(datum);

    if (datum->type >= RT_DATUM_POINT && datum->type <= RT_DATUM_TARGET_AREA)
	return (rt_datum_type)datum->type;
    if (!ZERO(datum->w))
	return RT_DATUM_PLANE;
    if (MAGNITUDE(datum->dir) > SMALL_FASTF)
	return RT_DATUM_LINE;
    return RT_DATUM_POINT;
}


static void
datum_validation_message(struct bu_vls *messages, size_t index, const char *message)
{
    if (!messages)
	return;
    if (bu_vls_strlen(messages))
	bu_vls_strcat(messages, "; ");
    bu_vls_printf(messages, "datum %zu: %s", index, message);
}


int
rt_datum_validate(const struct rt_datum_internal *datum, struct bu_vls *messages)
{
    const struct rt_datum_internal *dp = datum;
    size_t index = 0;
    int errors = 0;

    if (!dp) {
	datum_validation_message(messages, 0, "empty datum chain");
	return 1;
    }

    while (dp) {
	rt_datum_type type;
	double xmag = MAGNITUDE(dp->xdir);
	double ymag = MAGNITUDE(dp->ydir);

	RT_DATUM_CK_MAGIC(dp);
	type = rt_datum_resolved_type(dp);
	if (dp->type > RT_DATUM_TARGET_AREA) {
	    datum_validation_message(messages, index, "unknown explicit type");
	    errors++;
	}
	if (dp->role > RT_DATUM_ROLE_REFERENCE_FRAME) {
	    datum_validation_message(messages, index, "unknown semantic role");
	    errors++;
	}
	if (dp->flags & ~RT_DATUM_FLAG_BOUNDED) {
	    datum_validation_message(messages, index, "unknown flags");
	    errors++;
	}
	if (!isfinite(dp->pnt[X]) || !isfinite(dp->pnt[Y]) ||
		!isfinite(dp->pnt[Z]) || !isfinite(dp->dir[X]) ||
		!isfinite(dp->dir[Y]) || !isfinite(dp->dir[Z]) ||
		!isfinite(dp->xdir[X]) || !isfinite(dp->xdir[Y]) ||
		!isfinite(dp->xdir[Z]) || !isfinite(dp->ydir[X]) ||
		!isfinite(dp->ydir[Y]) || !isfinite(dp->ydir[Z]) ||
		!isfinite(dp->dimensions[0]) || !isfinite(dp->dimensions[1])) {
	    datum_validation_message(messages, index, "non-finite geometry");
	    errors++;
	}
	if ((type == RT_DATUM_LINE || type == RT_DATUM_PLANE ||
		type == RT_DATUM_FRAME || type == RT_DATUM_TARGET_LINE ||
		type == RT_DATUM_TARGET_AREA) &&
		MAGNITUDE(dp->dir) <= SMALL_FASTF) {
	    datum_validation_message(messages, index, "type requires a direction");
	    errors++;
	}
	if (type == RT_DATUM_FRAME) {
	    if (xmag <= SMALL_FASTF || ymag <= SMALL_FASTF) {
		datum_validation_message(messages, index,
		    "reference frame requires X, Y, and Z axes");
		errors++;
	    } else {
		vect_t xunit, yunit, zunit;
		VMOVE(xunit, dp->xdir);
		VMOVE(yunit, dp->ydir);
		VMOVE(zunit, dp->dir);
		VUNITIZE(xunit);
		VUNITIZE(yunit);
		VUNITIZE(zunit);
		if (!NEAR_ZERO(VDOT(xunit, yunit), RT_DOT_TOL) ||
			!NEAR_ZERO(VDOT(xunit, zunit), RT_DOT_TOL) ||
			!NEAR_ZERO(VDOT(yunit, zunit), RT_DOT_TOL)) {
		    datum_validation_message(messages, index,
			"reference-frame axes are not perpendicular");
		    errors++;
		}
	    }
	}
	if ((xmag > SMALL_FASTF) != (ymag > SMALL_FASTF) &&
		type != RT_DATUM_TARGET_LINE) {
	    datum_validation_message(messages, index,
		"oriented planes require both in-plane axes");
	    errors++;
	}
	if (dp->flags & RT_DATUM_FLAG_BOUNDED) {
	    if (type == RT_DATUM_TARGET_LINE) {
		if (dp->dimensions[0] <= SMALL_FASTF) {
		    datum_validation_message(messages, index,
			"bounded line target requires a positive length");
		    errors++;
		}
	    } else if (type == RT_DATUM_TARGET_AREA) {
		if (xmag <= SMALL_FASTF || ymag <= SMALL_FASTF ||
			dp->dimensions[0] <= SMALL_FASTF ||
			dp->dimensions[1] <= SMALL_FASTF) {
		    datum_validation_message(messages, index,
			"bounded area target requires two axes and positive dimensions");
		    errors++;
		}
	    } else if (type != RT_DATUM_TARGET_POINT) {
		datum_validation_message(messages, index,
		    "bounded flag is only valid for datum targets");
		errors++;
	    }
	}

	dp = dp->next;
	index++;
    }

    return errors;
}


static int
datum_has_extension(const struct rt_datum_internal *datum)
{
    while (datum) {
	if (datum->type || datum->role || datum->flags ||
		MAGNITUDE(datum->xdir) > SMALL_FASTF ||
		MAGNITUDE(datum->ydir) > SMALL_FASTF ||
		!ZERO(datum->dimensions[0]) || !ZERO(datum->dimensions[1]) ||
		datum->identifier || datum->description)
	    return 1;
	datum = datum->next;
    }
    return 0;
}


/**
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid DATUM, and if so, precompute
 * various terms (like how many datums there are).
 *
 * Returns -
 * 0 datum is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct datum_specific is created, and its address is stored in
 * stp->st_specific for use by datum_shot().
 */
C_DECL int
rt_datum_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_datum_internal *datum_ip;
    struct datum_specific *datum;

    RT_CK_SOLTAB(stp);
    RT_CK_DB_INTERNAL(ip);
    if (rtip) RT_CK_RTI(rtip);

    datum_ip = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum_ip);

    BU_GET(datum, struct datum_specific);
    datum->datum = datum_ip;
    while (datum_ip) {
	datum->count++;
	datum_ip = datum_ip->next;
    }

    if (rt_datum_validate(datum->datum, NULL)) {
	BU_PUT(datum, struct datum_specific);
	return 1;
    }

    stp->st_specific = (void *)datum;

    return 0;
}


C_DECL void
rt_datum_print(const struct soltab *stp)
{
    /* unnecessary callback */
    RT_CK_SOLTAB(stp);
    return;
}


C_DECL int
rt_datum_shot(struct soltab *UNUSED(stp), struct xray *UNUSED(rp), struct application *UNUSED(ap), struct seg *UNUSED(seghead))
{
    /* these are not solid geometry, so always a miss */
    return 0;
}


/**
 * Vectorized rt_datum_shot(): datums are not solid geometry, so every
 * ray in the batch misses.
 */
C_DECL void
rt_datum_vshot(struct soltab **stp, struct xray **UNUSED(rp), struct seg *segp, int n, struct application *ap)
{
    int i;
    if (ap) RT_CK_APPLICATION(ap);
    for (i = 0; i < n; i++) {
	if (stp[i] == 0) continue;		/* skip this ray */
	segp[i].seg_stp = (struct soltab *)0;	/* always MISS */
    }
}


C_DECL void
rt_datum_norm(struct hit *UNUSED(hitp), struct soltab *UNUSED(stp), struct xray *UNUSED(rp))
{
    return;
}


C_DECL void
rt_datum_curve(struct curvature *UNUSED(cvp), struct hit *UNUSED(hitp), struct soltab *UNUSED(stp))
{
    return;
}


C_DECL void
rt_datum_uv(struct application *UNUSED(ap), struct soltab *UNUSED(stp), struct hit *UNUSED(hitp), struct uvcoord *UNUSED(uvp))
{
    return;
}


C_DECL void
rt_datum_free(struct soltab *stp)
{
    struct datum_specific *datum;

    if (!stp) return;
    RT_CK_SOLTAB(stp);
    datum = (struct datum_specific *)stp->st_specific;
    if (!datum) return;

    BU_PUT(datum, struct datum_specific);
}


C_DECL int
rt_datum_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct rt_datum_internal *datum_ip;
    point_t point_size = VINIT_ZERO;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = &rt_vlfree;
    datum_ip = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum_ip);

    BU_CK_LIST_HEAD(vhead);

    /* make sure plotted points are an odd selectable number of pixels with a center pixel, 5x5 */
    point_size[X] = 5.0;
    BV_ADD_VLIST(vlfree, vhead, point_size, BV_VLIST_POINT_SIZE);

    while (datum_ip) {
	rt_datum_type type = rt_datum_resolved_type(datum_ip);
	if (type == RT_DATUM_FRAME) {
	    point_t tip;
	    const fastf_t *axes[3] = {datum_ip->xdir, datum_ip->ydir,
		datum_ip->dir};
	    size_t axis;
	    BV_ADD_VLIST(vlfree, vhead, datum_ip->pnt, BV_VLIST_POINT_DRAW);
	    for (axis = 0; axis < 3; ++axis) {
		if (MAGNITUDE(axes[axis]) <= SMALL_FASTF)
		    continue;
		VADD2(tip, datum_ip->pnt, axes[axis]);
		BV_ADD_VLIST(vlfree, vhead, datum_ip->pnt, BV_VLIST_LINE_MOVE);
		BV_ADD_VLIST(vlfree, vhead, tip, BV_VLIST_LINE_DRAW);
	    }
	} else if (type == RT_DATUM_PLANE || type == RT_DATUM_TARGET_AREA) {
	    vect_t left, right, nleft, nright;
	    point_t tip, ul, ll, ur, lr;

	    /* center and normal points */
	    VADD2(tip, datum_ip->pnt, datum_ip->dir);
	    BV_ADD_VLIST(vlfree, vhead, datum_ip->pnt, BV_VLIST_POINT_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, tip, BV_VLIST_POINT_DRAW);

	    if (MAGNITUDE(datum_ip->xdir) > SMALL_FASTF &&
		    MAGNITUDE(datum_ip->ydir) > SMALL_FASTF) {
		VMOVE(left, datum_ip->xdir);
		VMOVE(right, datum_ip->ydir);
		VUNITIZE(left);
		VUNITIZE(right);
	    } else {
		vect_t normal;
		VMOVE(normal, datum_ip->dir);
		VUNITIZE(normal);
		bn_vec_ortho(left, normal);
		VCROSS(right, left, normal);
	    }
	    if ((datum_ip->flags & RT_DATUM_FLAG_BOUNDED) &&
		    datum_ip->dimensions[0] > SMALL_FASTF &&
		    datum_ip->dimensions[1] > SMALL_FASTF) {
		VSCALE(left, left, 0.5 * datum_ip->dimensions[0]);
		VSCALE(right, right, 0.5 * datum_ip->dimensions[1]);
	    } else {
		double extent = MAGNITUDE(datum_ip->dir);
		if (!ZERO(datum_ip->w))
		    extent *= fabs(datum_ip->w);
		VSCALE(left, left, extent);
		VSCALE(right, right, extent);
	    }
	    VREVERSE(nright, right);
	    VREVERSE(nleft, left);

	    /* line to normal point */
	    BV_ADD_VLIST(vlfree, vhead, datum_ip->pnt, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, tip, BV_VLIST_LINE_DRAW);

	    /* draw the box */
	    VADD3(ul, datum_ip->pnt, left, right);
	    VADD3(ll, datum_ip->pnt, nleft, right);
	    VADD3(ur, datum_ip->pnt, left, nright);
	    VADD3(lr, datum_ip->pnt, nleft, nright);

	    BV_ADD_VLIST(vlfree, vhead, ul, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, ll, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, lr, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, ur, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, ul, BV_VLIST_LINE_DRAW);

	} else if (type == RT_DATUM_LINE || type == RT_DATUM_TARGET_LINE) {
	    vect_t left, right, nleft, nright, dir;
	    point_t tip, endpt, line_seg, ul, ll, ur, lr;
	    fastf_t arrowhead_percentage = 0.05;
	    fastf_t arrowhead_ratio = 0.3;
	    vect_t display_dir;

	    VMOVE(display_dir, datum_ip->dir);
	    if ((datum_ip->flags & RT_DATUM_FLAG_BOUNDED) &&
		    datum_ip->dimensions[0] > SMALL_FASTF) {
		VUNITIZE(display_dir);
		VSCALE(display_dir, display_dir, datum_ip->dimensions[0]);
	    }

	    /* Find the tip of the line */
	    VADD2(tip, datum_ip->pnt, display_dir);

	    /* draw main segment minus a smidgen for an arrowhead */
	    VSCALE(line_seg, display_dir, 1.0 - arrowhead_percentage);
	    VADD2(endpt, datum_ip->pnt, line_seg);
	    BV_ADD_VLIST(vlfree, vhead, datum_ip->pnt, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, endpt, BV_VLIST_LINE_DRAW);

	    /* calculate arrowhead points */
	    VMOVE(dir, display_dir);
	    VUNITIZE(dir);
	    bn_vec_ortho(left, dir);
	    VCROSS(right, left, dir);
	    VREVERSE(nright, right);
	    VREVERSE(nleft, left);
	    VSCALE(left, left, MAGNITUDE(display_dir) * arrowhead_percentage * arrowhead_ratio);
	    VSCALE(right, right, MAGNITUDE(display_dir) * arrowhead_percentage * arrowhead_ratio);
	    VREVERSE(nright, right);
	    VREVERSE(nleft, left);

	    VJOIN2(ul, endpt, 1, left,  1, right);
	    VJOIN2(ll, endpt, 1, nleft, 1, right);
	    VJOIN2(ur, endpt, 1, left,  1, nright);
	    VJOIN2(lr, endpt, 1, nleft, 1, nright);

	    /* draw arrowhead */
	    BV_ADD_VLIST(vlfree, vhead, ul, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, tip, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, lr, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, ll, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, tip, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, ur, BV_VLIST_LINE_DRAW);


	    BV_ADD_VLIST(vlfree, vhead, ul, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, ll, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, lr, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, ur, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, ul, BV_VLIST_LINE_DRAW);

	} else {
	    BV_ADD_VLIST(vlfree, vhead, datum_ip->pnt, BV_VLIST_POINT_DRAW);
	}
	datum_ip = datum_ip->next;
    }

    return 0;
}


/**
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
C_DECL int
rt_datum_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    struct rt_datum_internal *datum_ip;

    if (r) NMG_CK_REGION(*r);
    if (m) NMG_CK_MODEL(m);
    RT_CK_DB_INTERNAL(ip);
    datum_ip = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum_ip);

    return -1;
}


static unsigned char *
datum_pack_double(unsigned char *buf, unsigned char *data, size_t count)
{
    bu_cv_htond(buf, data, count);
    buf += count * SIZEOF_NETWORK_DOUBLE;
    return buf;
}


static unsigned char *
datum_unpack_double(unsigned char *buf, unsigned char *data, size_t count)
{
    bu_cv_ntohd(data, buf, count);
    buf += count * SIZEOF_NETWORK_DOUBLE;
    return buf;
}


/**
 * Export datums from internal form to external format.  Note that
 * this means converting all integers to Big-Endian format and
 * floating point data to IEEE double.
 *
 * Apply the transformation to mm units as well.
 */
C_DECL int
rt_datum_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_datum_internal *datum_ip;
    unsigned char *buf = NULL;
    unsigned long count = 0;
    size_t extension_size = 0;
    int enhanced = 0;

    /* must be double for import and export */
    double vec[MAX_VALS] = {0.0};

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_DATUM) return -1;
    datum_ip = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum_ip);
    if (dbip) RT_CK_DBI(dbip);
    if (rt_datum_validate(datum_ip, NULL))
	return -1;

    enhanced = datum_has_extension(datum_ip);

    /* tally */
    do {
	count++;
    } while ((datum_ip = datum_ip->next));
    /* rewind */
    datum_ip = (struct rt_datum_internal *)ip->idb_ptr;

    if (enhanced) {
	const struct rt_datum_internal *dp = datum_ip;
	extension_size = 3 * SIZEOF_NETWORK_LONG;
	while (dp) {
	    extension_size += 5 * SIZEOF_NETWORK_LONG +
		9 * SIZEOF_NETWORK_DOUBLE;
	    if (dp->identifier)
		extension_size += strlen(dp->identifier);
	    if (dp->description)
		extension_size += strlen(dp->description);
	    dp = dp->next;
	}
    }

    BU_CK_EXTERNAL(ep);

    /* we allocate potentially more than strictly necessary so we can
     * change datums in place. avoids growing the export unnecessarily.
     */
    ep->ext_nbytes = SIZEOF_NETWORK_LONG /* #datums */ +
	(count * MAX_VALS * SIZEOF_NETWORK_DOUBLE) +
	(count * SIZEOF_NETWORK_LONG /* #vals */) + extension_size;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "datum external");
    buf = (unsigned char *)ep->ext_buf;

    *(uint32_t *)buf = htonl((uint32_t)count);
    buf += SIZEOF_NETWORK_LONG;

    do {
	rt_datum_type type = rt_datum_resolved_type(datum_ip);
	if (type == RT_DATUM_PLANE || type == RT_DATUM_FRAME ||
		type == RT_DATUM_TARGET_AREA) {
	    /* plane */
	    *(uint32_t *)buf = htonl(MAX_VALS);
	    buf += SIZEOF_NETWORK_LONG;
	    VSCALE(vec, datum_ip->pnt, local2mm);
	    VSCALE(vec+3, datum_ip->dir, local2mm);
	    /* An old reader needs a nonzero w to recognize an explicitly typed
	     * plane.  The extension below restores the authored value. */
	    vec[6] = ZERO(datum_ip->w) ? 1.0 : datum_ip->w;
	    buf = datum_pack_double(buf, (unsigned char *)vec, MAX_VALS);

	} else if (type == RT_DATUM_LINE || type == RT_DATUM_TARGET_LINE) {
	    /* line */
	    *(uint32_t *)buf = htonl(ELEMENTS_PER_POINT + ELEMENTS_PER_VECT);
	    buf += SIZEOF_NETWORK_LONG;
	    VSCALE(vec, datum_ip->pnt, local2mm);
	    VSCALE(vec+3, datum_ip->dir, local2mm);
	    buf = datum_pack_double(buf, (unsigned char *)vec, ELEMENTS_PER_POINT + ELEMENTS_PER_VECT);

	} else {
	    /* point */
	    *(uint32_t *)buf = htonl(ELEMENTS_PER_POINT);
	    buf += SIZEOF_NETWORK_LONG;
	    VSCALE(vec, datum_ip->pnt, local2mm);
	    buf = datum_pack_double(buf, (unsigned char *)vec, ELEMENTS_PER_POINT);

	}

    } while ((datum_ip = datum_ip->next));

    if (enhanced) {
	const struct rt_datum_internal *dp =
	    (const struct rt_datum_internal *)ip->idb_ptr;
	*(uint32_t *)buf = htonl(DATUM_EXT_MAGIC);
	buf += SIZEOF_NETWORK_LONG;
	*(uint32_t *)buf = htonl(DATUM_EXT_VERSION);
	buf += SIZEOF_NETWORK_LONG;
	*(uint32_t *)buf = htonl((uint32_t)count);
	buf += SIZEOF_NETWORK_LONG;
	while (dp) {
	    double ext[9];
	    size_t identifier_len = dp->identifier ? strlen(dp->identifier) : 0;
	    size_t description_len = dp->description ? strlen(dp->description) : 0;

	    *(uint32_t *)buf = htonl(dp->type);
	    buf += SIZEOF_NETWORK_LONG;
	    *(uint32_t *)buf = htonl(dp->role);
	    buf += SIZEOF_NETWORK_LONG;
	    *(uint32_t *)buf = htonl(dp->flags);
	    buf += SIZEOF_NETWORK_LONG;
	    ext[0] = dp->w;
	    VSCALE(&ext[1], dp->xdir, local2mm);
	    VSCALE(&ext[4], dp->ydir, local2mm);
	    ext[7] = dp->dimensions[0] * local2mm;
	    ext[8] = dp->dimensions[1] * local2mm;
	    buf = datum_pack_double(buf, (unsigned char *)ext, 9);
	    *(uint32_t *)buf = htonl((uint32_t)identifier_len);
	    buf += SIZEOF_NETWORK_LONG;
	    *(uint32_t *)buf = htonl((uint32_t)description_len);
	    buf += SIZEOF_NETWORK_LONG;
	    if (identifier_len) {
		memcpy(buf, dp->identifier, identifier_len);
		buf += identifier_len;
	    }
	    if (description_len) {
		memcpy(buf, dp->description, description_len);
		buf += description_len;
	    }
	    dp = dp->next;
	}
    }

    return 0;
}


C_DECL int
rt_datum_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip)
{
    if (!rop || !mat)
	return BRLCAD_OK;

    // For the moment, we only support applying a mat to a datum in place - the
    // input and output must be the same.
    if (ip && rop != ip) {
	bu_log("rt_datum_mat:  alignment of data between multiple datums is unsupported - input datum must be the same as the output datum.\n");
	return BRLCAD_ERROR;
    }

    struct rt_datum_internal *datum_ip = (struct rt_datum_internal *)rop->idb_ptr;
    RT_DATUM_CK_MAGIC(datum_ip);

    vect_t v;
    while (datum_ip) {
	VMOVE(v, datum_ip->pnt);
	MAT4X3PNT(datum_ip->pnt, mat, v);
	if (MAGNITUDE(datum_ip->dir) > 0.0) {
	    VMOVE(v, datum_ip->dir);
	    MAT4X3VEC(datum_ip->dir, mat, v);
	}
	if (MAGNITUDE(datum_ip->xdir) > 0.0) {
	    VMOVE(v, datum_ip->xdir);
	    MAT4X3VEC(datum_ip->xdir, mat, v);
	}
	if (MAGNITUDE(datum_ip->ydir) > 0.0) {
	    VMOVE(v, datum_ip->ydir);
	    MAT4X3VEC(datum_ip->ydir, mat, v);
	}
	if (!ZERO(mat[15])) {
	    datum_ip->dimensions[0] /= mat[15];
	    datum_ip->dimensions[1] /= mat[15];
	}
	datum_ip = datum_ip->next;
    }

    return BRLCAD_OK;
}


/**
 * Import datums from the database format to the internal format.
 * Note that the data read will be in network order.  This means
 * Big-Endian integers and IEEE doubles for floating point.
 *
 * Apply modeling transformations as well.
 */
C_DECL int
rt_datum_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip)
{
    struct rt_datum_internal *first = NULL;
    struct rt_datum_internal *prev = NULL;
    unsigned char *buf = NULL;
    const unsigned char *end = NULL;
    size_t count = 0;
    size_t record_count = 0;
    size_t i;

    /* must be double for import and export */
    double vec[MAX_VALS];

    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);
    buf = (unsigned char *)ep->ext_buf;
    end = buf + ep->ext_nbytes;

#define DATUM_IMPORT_NEED(_n) do { \
	if ((size_t)(end - buf) < (size_t)(_n)) goto import_error; \
    } while (0)
#define DATUM_IMPORT_UINT32(_v) do { \
	uint32_t _net; \
	DATUM_IMPORT_NEED(SIZEOF_NETWORK_LONG); \
	memcpy(&_net, buf, SIZEOF_NETWORK_LONG); \
	(_v) = ntohl(_net); \
	buf += SIZEOF_NETWORK_LONG; \
    } while (0)

    /* unpack our datum set count */
    DATUM_IMPORT_UINT32(count);
    if (!count)
	goto import_error;
    record_count = count;

    while (count-- > 0) {
	struct rt_datum_internal *datum_ip;
	size_t vals;

	DATUM_IMPORT_UINT32(vals);
	if (vals != ELEMENTS_PER_POINT &&
		vals != ELEMENTS_PER_POINT + ELEMENTS_PER_VECT &&
		vals != MAX_VALS)
	    goto import_error;

	DATUM_IMPORT_NEED(vals * SIZEOF_NETWORK_DOUBLE);
	buf = datum_unpack_double(buf, (unsigned char *)vec, vals);

	BU_ALLOC(datum_ip, struct rt_datum_internal);
	if (!first)
	    first = datum_ip;

	if (vals >= ELEMENTS_PER_POINT)
	    VMOVE(datum_ip->pnt, vec);
	if (vals >= ELEMENTS_PER_POINT + ELEMENTS_PER_VECT)
	    VMOVE(datum_ip->dir, vec+3);
	if (vals == MAX_VALS)
	    datum_ip->w = vec[6];

	datum_ip->magic = RT_DATUM_INTERNAL_MAGIC;
	if (prev)
	    prev->next = datum_ip;
	prev = datum_ip;
    }

    /* Enhanced payloads are trailers, so legacy readers continue to see and
     * display the historical point/line/plane approximation. */
    if ((size_t)(end - buf) >= SIZEOF_NETWORK_LONG) {
	uint32_t magic;
	uint32_t version;
	uint32_t extension_count;
	uint32_t net;
	memcpy(&net, buf, SIZEOF_NETWORK_LONG);
	magic = ntohl(net);
	if (magic == DATUM_EXT_MAGIC) {
	    struct rt_datum_internal *dp = first;
	    DATUM_IMPORT_UINT32(magic);
	    DATUM_IMPORT_UINT32(version);
	    DATUM_IMPORT_UINT32(extension_count);
	    if (version != DATUM_EXT_VERSION || extension_count != record_count)
		goto import_error;

	    for (i = 0; i < record_count; ++i) {
		double ext[9];
		uint32_t identifier_len;
		uint32_t description_len;
		if (!dp)
		    goto import_error;
		DATUM_IMPORT_UINT32(dp->type);
		DATUM_IMPORT_UINT32(dp->role);
		DATUM_IMPORT_UINT32(dp->flags);
		DATUM_IMPORT_NEED(9 * SIZEOF_NETWORK_DOUBLE);
		buf = datum_unpack_double(buf, (unsigned char *)ext, 9);
		dp->w = ext[0];
		VMOVE(dp->xdir, &ext[1]);
		VMOVE(dp->ydir, &ext[4]);
		dp->dimensions[0] = ext[7];
		dp->dimensions[1] = ext[8];
		DATUM_IMPORT_UINT32(identifier_len);
		DATUM_IMPORT_UINT32(description_len);
		DATUM_IMPORT_NEED((size_t)identifier_len + description_len);
		if (identifier_len) {
		    dp->identifier = (char *)bu_calloc(identifier_len + 1, 1,
			"datum identifier");
		    memcpy(dp->identifier, buf, identifier_len);
		    buf += identifier_len;
		}
		if (description_len) {
		    dp->description = (char *)bu_calloc(description_len + 1, 1,
			"datum description");
		    memcpy(dp->description, buf, description_len);
		    buf += description_len;
		}
		dp = dp->next;
	    }
	}
    }

    if (rt_datum_validate(first, NULL))
	goto import_error;

    /* set up the internal structure */
    ip->idb_ptr = first;
    ip->idb_meth = &OBJ[ID_DATUM];
    ip->idb_type = ID_DATUM;
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;

    /* Apply transform */
    return rt_datum_mat(ip, mat, ip);

import_error:
    while (first) {
	struct rt_datum_internal *next = first->next;
	if (first->identifier)
	    bu_free(first->identifier, "datum identifier");
	if (first->description)
	    bu_free(first->description, "datum description");
	bu_free(first, "datum import");
	first = next;
    }
    return -1;

#undef DATUM_IMPORT_UINT32
#undef DATUM_IMPORT_NEED
}


C_DECL int
rt_datum_make(const struct rt_functab* ftp, struct rt_db_internal* intern, const char* UNUSED(variant), const point_t origin, double UNUSED(scale))
{
    struct rt_datum_internal *datum_ip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_DATUM;
    BU_ASSERT(&OBJ[intern->idb_type] == ftp);
    intern->idb_meth = ftp;

    /* Set a default color for datum objects */
    bu_avs_add(&intern->idb_avs, "color", "255/255/0");

    BU_ALLOC(intern->idb_ptr, struct rt_datum_internal);
    datum_ip = (struct rt_datum_internal *)intern->idb_ptr;
    datum_ip->magic = RT_DATUM_INTERNAL_MAGIC;

    /* center point */
    VSET(datum_ip->pnt, origin[X], origin[Y], origin[Z]);

    /* just a point */
    VSETALL(datum_ip->dir, 0.0);
    datum_ip->w = 0.0;
    datum_ip->next = NULL;

#if 0
    /* Historically 'make' would create a full demo coordinate system datum: 7 
     * datums chained (one center point, three axis vectors, and three planes)
     * This isn't really in the spirit of a "default" make, but is still a good
     * example
     */
    struct rt_datum_internal *next_ip;

    /* X-axis */
    BU_ALLOC(next_ip, struct rt_datum_internal);
    next_ip->magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(next_ip->pnt, origin[X], origin[Y], origin[Z]);
    VSET(next_ip->dir, 1.0, 0.0, 0.0);
    datum_ip->next = next_ip;

    /* Y-axis */
    BU_ALLOC(next_ip, struct rt_datum_internal);
    next_ip->magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(next_ip->pnt, origin[X], origin[Y], origin[Z]);
    VSET(next_ip->dir, 0.0, 1.0, 0.0);
    datum_ip->next->next = next_ip;

    /* Z-axis */
    BU_ALLOC(next_ip, struct rt_datum_internal);
    next_ip->magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(next_ip->pnt, origin[X], origin[Y], origin[Z]);
    VSET(next_ip->dir, 0.0, 0.0, 1.0);
    datum_ip->next->next->next = next_ip;

    /* X-plane */
    BU_ALLOC(next_ip, struct rt_datum_internal);
    next_ip->magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(next_ip->pnt, origin[X], origin[Y], origin[Z]);
    VSET(next_ip->dir, 1.0, 0.0, 0.0);
    next_ip->w = 1.0;
    datum_ip->next->next->next->next = next_ip;

    /* Y-plane */
    BU_ALLOC(next_ip, struct rt_datum_internal);
    next_ip->magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(next_ip->pnt, origin[X], origin[Y], origin[Z]);
    VSET(next_ip->dir, 0.0, 1.0, 0.0);
    next_ip->w = 1.0;
    datum_ip->next->next->next->next->next = next_ip;

    /* Z-plane */
    BU_ALLOC(next_ip, struct rt_datum_internal);
    next_ip->magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(next_ip->pnt, origin[X], origin[Y], origin[Z]);
    VSET(next_ip->dir, 0.0, 0.0, 1.0);
    next_ip->w = 1.0;
    datum_ip->next->next->next->next->next->next = next_ip;
#endif

    return BRLCAD_OK;
}

/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
C_DECL int
rt_datum_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_datum_internal *datum_ip = (struct rt_datum_internal *)ip->idb_ptr;

    RT_DATUM_CK_MAGIC(datum_ip);

    bu_vls_strcat(str, "Datum Set (DATUM)\n");

    /* NOTE: this value is not arbitrary, but is dependent on the init in libged/list/list.c */
    if (verbose <= 99) {
	bu_vls_strcat(str, "    use -v (verbose) for all data\n");
	return 0;
    }

    while (datum_ip) {
	rt_datum_type type = rt_datum_resolved_type(datum_ip);
	const char *type_name = "point";
	switch (type) {
	    case RT_DATUM_LINE: type_name = "line"; break;
	    case RT_DATUM_PLANE: type_name = "plane"; break;
	    case RT_DATUM_FRAME: type_name = "reference frame"; break;
	    case RT_DATUM_TARGET_POINT: type_name = "point target"; break;
	    case RT_DATUM_TARGET_LINE: type_name = "line target"; break;
	    case RT_DATUM_TARGET_AREA: type_name = "area target"; break;
	    default: break;
	}
	bu_vls_printf(str, "\t%s datum\n", type_name);
	if (datum_ip->identifier)
	    bu_vls_printf(str, "\t\tIdentifier: %s\n", datum_ip->identifier);
	if (datum_ip->description)
	    bu_vls_printf(str, "\t\tDescription: %s\n", datum_ip->description);

	bu_vls_printf(str, "\t\tV (%g, %g, %g)\n",
		      INTCLAMP(datum_ip->pnt[X] * mm2local),
		      INTCLAMP(datum_ip->pnt[Y] * mm2local),
		      INTCLAMP(datum_ip->pnt[Z] * mm2local));

	if (type == RT_DATUM_PLANE || type == RT_DATUM_FRAME ||
		type == RT_DATUM_TARGET_AREA) {
	    bu_vls_printf(str, "\t\tDIR (%g, %g, %g)\n\t\tW (%g)\n",
			  INTCLAMP(datum_ip->dir[X] * mm2local),
			  INTCLAMP(datum_ip->dir[Y] * mm2local),
			  INTCLAMP(datum_ip->dir[Z] * mm2local),
			  INTCLAMP(datum_ip->w));
	} else if (type == RT_DATUM_LINE || type == RT_DATUM_TARGET_LINE) {
	    bu_vls_printf(str, "\t\tDIR (%g, %g, %g)\n",
			  INTCLAMP(datum_ip->dir[X] * mm2local),
			  INTCLAMP(datum_ip->dir[Y] * mm2local),
			  INTCLAMP(datum_ip->dir[Z] * mm2local));
	}
	if (MAGNITUDE(datum_ip->xdir) > SMALL_FASTF)
	    bu_vls_printf(str, "\t\tXDIR (%g, %g, %g)\n",
		INTCLAMP(datum_ip->xdir[X] * mm2local),
		INTCLAMP(datum_ip->xdir[Y] * mm2local),
		INTCLAMP(datum_ip->xdir[Z] * mm2local));
	if (MAGNITUDE(datum_ip->ydir) > SMALL_FASTF)
	    bu_vls_printf(str, "\t\tYDIR (%g, %g, %g)\n",
		INTCLAMP(datum_ip->ydir[X] * mm2local),
		INTCLAMP(datum_ip->ydir[Y] * mm2local),
		INTCLAMP(datum_ip->ydir[Z] * mm2local));
	if (!ZERO(datum_ip->dimensions[0]) || !ZERO(datum_ip->dimensions[1]))
	    bu_vls_printf(str, "\t\tDimensions (%g, %g)\n",
		INTCLAMP(datum_ip->dimensions[0] * mm2local),
		INTCLAMP(datum_ip->dimensions[1] * mm2local));

	datum_ip = datum_ip->next;
    }

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
C_DECL void
rt_datum_ifree(struct rt_db_internal *ip)
{
    struct rt_datum_internal *datum_ip;

    RT_CK_DB_INTERNAL(ip);

    datum_ip = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum_ip);

    while (datum_ip) {
	struct rt_datum_internal *next;
	next = datum_ip->next;
	datum_ip->next = NULL;
	if (datum_ip->identifier)
	    bu_free(datum_ip->identifier, "datum identifier");
	if (datum_ip->description)
	    bu_free(datum_ip->description, "datum description");
	datum_ip->magic = 0; /* sanity */
	bu_free((char *)datum_ip, "datum ifree");
	datum_ip = next;
    }
    ip->idb_ptr = NULL;	/* sanity */
}


C_DECL const char *
rt_datum_keypoint(point_t *pt, const char *keystr, const mat_t mat, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    if (!pt || !ip)
	return NULL;

    point_t mpt = VINIT_ZERO;
    struct rt_datum_internal *datum = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum);

    static const char *default_keystr = "V";
    const char *k = (keystr) ? keystr : default_keystr;

    if (BU_STR_EQUAL(k, default_keystr)) {
	VMOVE(mpt, datum->pnt);
	goto datum_kpt_end;
    }

    // No keystr matches - failed
    return NULL;

datum_kpt_end:

    MAT4X3PNT(*pt, mat, mpt);

    return k;
}


C_DECL int
rt_datum_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct rt_datum_internal *datum;

    RT_CK_DB_INTERNAL(intern);
    datum = (struct rt_datum_internal *)intern->idb_ptr;
    RT_DATUM_CK_MAGIC(datum);

    if (attr) {
        bu_vls_printf(logstr, "datum has no attribute '%s'", attr);
        return BRLCAD_ERROR;
    }

    bu_vls_strcpy(logstr, "datum data {");
    while (datum) {
        if (!ZERO(datum->w)) {
            bu_vls_printf(logstr, " {plane %.25G %.25G %.25G %.25G %.25G %.25G %.25G}",
                          V3ARGS(datum->pnt), V3ARGS(datum->dir), datum->w);
        } else if (MAGNITUDE(datum->dir) > 0.0 && ZERO(datum->w)) {
            bu_vls_printf(logstr, " {line %.25G %.25G %.25G %.25G %.25G %.25G}",
                          V3ARGS(datum->pnt), V3ARGS(datum->dir));
        } else {
            bu_vls_printf(logstr, " {point %.25G %.25G %.25G}",
                          V3ARGS(datum->pnt));
        }
        datum = datum->next;
    }
    bu_vls_strcat(logstr, "}");
    return BRLCAD_OK;
}


C_DECL int
rt_datum_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    RT_CK_FUNCTAB(ftp);
    bu_vls_printf(logstr, "data { {point %%f %%f %%f} {line %%f %%f %%f %%f %%f %%f} {plane %%f %%f %%f %%f %%f %%f %%f} ...}");
    return BRLCAD_OK;
}


C_DECL int
rt_datum_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_datum_internal *datum;
    int i;

    RT_CK_DB_INTERNAL(intern);
    datum = (struct rt_datum_internal *)intern->idb_ptr;
    RT_DATUM_CK_MAGIC(datum);

    for (i = 0; i < argc; i += 2) {
        if (BU_STR_EQUAL(argv[i], "data")) {
            const char **list_argv;
            int list_argc;
            struct rt_datum_internal *head = NULL, *tail = NULL;
            int j;

            if (i + 1 >= argc) {
                bu_vls_printf(logstr, "missing value for 'data' attribute");
                return BRLCAD_ERROR;
            }

            if (bu_argv_from_tcl_list(argv[i+1], &list_argc, (const char ***)&list_argv) != 0) {
                bu_vls_printf(logstr, "invalid data list");
                return BRLCAD_ERROR;
            }

            for (j = 0; j < list_argc; j++) {
                const char **elem_argv;
                int elem_argc;
                if (bu_argv_from_tcl_list(list_argv[j], &elem_argc, (const char ***)&elem_argv) != 0) {
                    bu_vls_printf(logstr, "invalid datum element list");
                    bu_free((char *)list_argv, "list_argv");
                    return BRLCAD_ERROR;
                }

                if (elem_argc > 0) {
                    struct rt_datum_internal *new_datum;
                    BU_ALLOC(new_datum, struct rt_datum_internal);
                    new_datum->magic = RT_DATUM_INTERNAL_MAGIC;
                    new_datum->next = NULL;

                    if (BU_STR_EQUAL(elem_argv[0], "point") && elem_argc == 4) {
                        new_datum->pnt[X] = atof(elem_argv[1]);
                        new_datum->pnt[Y] = atof(elem_argv[2]);
                        new_datum->pnt[Z] = atof(elem_argv[3]);
                        VSETALL(new_datum->dir, 0.0);
                        new_datum->w = 0.0;
                    } else if (BU_STR_EQUAL(elem_argv[0], "line") && elem_argc == 7) {
                        new_datum->pnt[X] = atof(elem_argv[1]);
                        new_datum->pnt[Y] = atof(elem_argv[2]);
                        new_datum->pnt[Z] = atof(elem_argv[3]);
                        new_datum->dir[X] = atof(elem_argv[4]);
                        new_datum->dir[Y] = atof(elem_argv[5]);
                        new_datum->dir[Z] = atof(elem_argv[6]);
                        new_datum->w = 0.0;
                    } else if (BU_STR_EQUAL(elem_argv[0], "plane") && elem_argc == 8) {
                        new_datum->pnt[X] = atof(elem_argv[1]);
                        new_datum->pnt[Y] = atof(elem_argv[2]);
                        new_datum->pnt[Z] = atof(elem_argv[3]);
                        new_datum->dir[X] = atof(elem_argv[4]);
                        new_datum->dir[Y] = atof(elem_argv[5]);
                        new_datum->dir[Z] = atof(elem_argv[6]);
                        new_datum->w = atof(elem_argv[7]);
                    } else {
                        bu_vls_printf(logstr, "invalid datum element");
                        bu_free((char *)elem_argv, "elem_argv");
                        bu_free((char *)list_argv, "list_argv");
                        if (head) {
                            struct rt_db_internal dummy;
                            dummy.idb_ptr = head;
                            rt_datum_ifree(&dummy);
                        }
                        return BRLCAD_ERROR;
                    }
                    if (!head) {
                        head = new_datum;
                    } else {
                        tail->next = new_datum;
                    }
                    tail = new_datum;
                }
                bu_free((char *)elem_argv, "elem_argv");
            }
            bu_free((char *)list_argv, "list_argv");

            if (!head) {
                bu_vls_printf(logstr, "datum must have at least one element");
                return BRLCAD_ERROR;
            }

            rt_datum_ifree(intern);
            intern->idb_ptr = head;
        } else {
            bu_vls_printf(logstr, "unknown attribute '%s'", argv[i]);
            return BRLCAD_ERROR;
        }
    }

    return BRLCAD_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
