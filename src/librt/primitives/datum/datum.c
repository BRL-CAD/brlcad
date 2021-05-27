/*                         D A T U M . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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

#include "bnetwork.h"

#include "bu/cv.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/db5.h"
#include "rt/geom.h"
#include "raytrace.h"

/* local interface header */
#include "./datum.h"

/* maximum number of values a datum may have (positioned plane case) */
#define MAX_VALS (ELEMENTS_PER_POINT + ELEMENTS_PER_VECT + 1 ) /* for w */


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
int
rt_datum_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_datum_internal *datum_ip;
    struct datum_specific *datum;

    RT_CK_SOLTAB(stp);
    RT_CK_DB_INTERNAL(ip);
    if (rtip) RT_CK_RTI(rtip);

    datum_ip = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum_ip);

    stp->st_specific = (void *)datum_ip;

    BU_GET(datum, struct datum_specific);
    datum->datum = datum_ip;
    while ((datum_ip = datum_ip->next)) {
	datum->count++;
    }

    /* !!! validate */
    bu_log("Datum has %zd references\n", datum->count);

    return 0;
}


void
rt_datum_print(const struct soltab *stp)
{
    /* unnecessary callback */
    RT_CK_SOLTAB(stp);
    return;
}


int
rt_datum_shot(struct soltab *UNUSED(stp), struct xray *UNUSED(rp), struct application *UNUSED(ap), struct seg *UNUSED(seghead))
{
    /* these are not solid geometry, so always a miss */
    return 0;
}


void
rt_datum_norm(struct hit *UNUSED(hitp), struct soltab *UNUSED(stp), struct xray *UNUSED(rp))
{
    return;
}


void
rt_datum_curve(struct curvature *UNUSED(cvp), struct hit *UNUSED(hitp), struct soltab *UNUSED(stp))
{
    return;
}


void
rt_datum_uv(struct application *UNUSED(ap), struct soltab *UNUSED(stp), struct hit *UNUSED(hitp), struct uvcoord *UNUSED(uvp))
{
    return;
}


void
rt_datum_free(struct soltab *stp)
{
    struct datum_specific *datum;

    if (!stp) return;
    RT_CK_SOLTAB(stp);
    datum = (struct datum_specific *)stp->st_specific;
    if (!datum) return;

    BU_PUT(datum, struct datum_specific);
}


int
rt_datum_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct rt_datum_internal *datum_ip;
    point_t point_size = VINIT_ZERO;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = ip->idb_vlfree;
    datum_ip = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum_ip);

    BU_CK_LIST_HEAD(vhead);

    /* make sure plotted points are an odd selectable number of pixels with a center pixel, 5x5 */
    point_size[X] = 5.0;
    BV_ADD_VLIST(vlfree, vhead, point_size, BV_VLIST_POINT_SIZE);

    while (datum_ip) {
	if (!ZERO(datum_ip->w)) {
	    vect_t up, left, right, nleft, nright;
	    point_t tip, ul, ll, ur, lr;

	    /* center and normal points */
	    VMOVE(up, datum_ip->dir);
	    VUNITIZE(up);
	    VADD2(tip, datum_ip->pnt, datum_ip->dir);
	    BV_ADD_VLIST(vlfree, vhead, datum_ip->pnt, BV_VLIST_POINT_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, tip, BV_VLIST_POINT_DRAW);

	    VSUB2(up, tip, datum_ip->pnt);
	    VUNITIZE(up);
	    bn_vec_ortho(left, up);
	    VCROSS(right, left, up);
	    VREVERSE(nright, right);
	    VREVERSE(nleft, left);
	    VSCALE(left, left, MAGNITUDE(datum_ip->dir));
	    VSCALE(nleft, nleft, MAGNITUDE(datum_ip->dir));
	    VSCALE(right, right, MAGNITUDE(datum_ip->dir));
	    VSCALE(nright, nright, MAGNITUDE(datum_ip->dir));

	    /* line to normal point */
	    BV_ADD_VLIST(vlfree, vhead, datum_ip->pnt, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, tip, BV_VLIST_LINE_DRAW);

	    /* draw the box */
	    VJOIN2(ul, datum_ip->pnt, datum_ip->w, left, datum_ip->w, right);
	    VJOIN2(ll, datum_ip->pnt, datum_ip->w, nleft, datum_ip->w, right);
	    VJOIN2(ur, datum_ip->pnt, datum_ip->w, left, datum_ip->w, nright);
	    VJOIN2(lr, datum_ip->pnt, datum_ip->w, nleft, datum_ip->w, nright);

	    BV_ADD_VLIST(vlfree, vhead, ul, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, ll, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, lr, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, ur, BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, ul, BV_VLIST_LINE_DRAW);

	} else if (MAGNITUDE(datum_ip->dir) > 0.0 && ZERO(datum_ip->w)) {
	    vect_t left, right, nleft, nright, dir;
	    point_t tip, endpt, line_seg, ul, ll, ur, lr;
	    fastf_t arrowhead_percentage = 0.05;
	    fastf_t arrowhead_ratio = 0.3;

	    /* Find the tip of the line */
	    VADD2(tip, datum_ip->pnt, datum_ip->dir);

	    /* draw main segment minus a smidgen for an arrowhead */
	    VSCALE(line_seg, datum_ip->dir, 1.0 - arrowhead_percentage);
	    VADD2(endpt, datum_ip->pnt, line_seg);
	    BV_ADD_VLIST(vlfree, vhead, datum_ip->pnt, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, endpt, BV_VLIST_LINE_DRAW);

	    /* calculate arrowhead points */
	    VMOVE(dir, datum_ip->dir);
	    VUNITIZE(dir);
	    bn_vec_ortho(left, dir);
	    VCROSS(right, left, dir);
	    VREVERSE(nright, right);
	    VREVERSE(nleft, left);
	    VSCALE(left, left, MAGNITUDE(datum_ip->dir) * arrowhead_percentage * arrowhead_ratio);
	    VSCALE(right, right, MAGNITUDE(datum_ip->dir) * arrowhead_percentage * arrowhead_ratio);
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
int
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


HIDDEN unsigned char *
datum_pack_double(unsigned char *buf, unsigned char *data, size_t count)
{
    bu_cv_htond(buf, data, count);
    buf += count * SIZEOF_NETWORK_DOUBLE;
    return buf;
}

HIDDEN unsigned char *
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
int
rt_datum_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_datum_internal *datum_ip;
    unsigned char *buf = NULL;
    unsigned long count = 0;

    /* must be double for import and export */
    double vec[MAX_VALS] = {0.0};

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_DATUM) return -1;
    datum_ip = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum_ip);
    if (dbip) RT_CK_DBI(dbip);

    /* tally */
    do {
	count++;
    } while ((datum_ip = datum_ip->next));
    /* rewind */
    datum_ip = (struct rt_datum_internal *)ip->idb_ptr;

    BU_CK_EXTERNAL(ep);

    /* we allocate potentially more than strictly necessary so we can
     * change datums in place. avoids growing the export unnecessarily.
     */
    ep->ext_nbytes = SIZEOF_NETWORK_LONG /* #datums */ + (count * MAX_VALS * SIZEOF_NETWORK_DOUBLE) + (count * SIZEOF_NETWORK_LONG /* #vals */);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "datum external");
    buf = (unsigned char *)ep->ext_buf;

    *(uint32_t *)buf = htonl((uint32_t)count);
    buf += SIZEOF_NETWORK_LONG;

    do {
	if (!ZERO(datum_ip->w)) {
	    /* plane */
	    *(uint32_t *)buf = htonl(MAX_VALS);
	    buf += SIZEOF_NETWORK_LONG;
	    VSCALE(vec, datum_ip->pnt, local2mm);
	    VSCALE(vec+3, datum_ip->dir, local2mm);
	    vec[6] = datum_ip->w;
	    buf = datum_pack_double(buf, (unsigned char *)vec, MAX_VALS);

	} else if (MAGNITUDE(datum_ip->dir) > 0.0 && ZERO(datum_ip->w)) {
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

    return 0;
}


/**
 * Import datums from the database format to the internal format.
 * Note that the data read will be in network order.  This means
 * Big-Endian integers and IEEE doubles for floating point.
 *
 * Apply modeling transformations as well.
 */
int
rt_datum_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip)
{
    struct rt_datum_internal *first = NULL;
    struct rt_datum_internal *prev = NULL;
    unsigned char *buf = NULL;
    size_t count = 0;

    /* must be double for import and export */
    double vec[MAX_VALS];

    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);
    buf = (unsigned char *)ep->ext_buf;

    /* unpack our datum set count */
    count = ntohl(*(uint32_t *)buf);
    buf += SIZEOF_NETWORK_LONG;

    while (count-- > 0) {
	struct rt_datum_internal *datum_ip;

	size_t vals = ntohl(*(uint32_t *)buf);
	buf += SIZEOF_NETWORK_LONG;

	if (vals > MAX_VALS)
	    return -1;

	buf = datum_unpack_double(buf, (unsigned char *)vec, vals);

	BU_ALLOC(datum_ip, struct rt_datum_internal);
	if (!first)
	    first = datum_ip;

	if (vals >= ELEMENTS_PER_POINT)
	    MAT4X3PNT(datum_ip->pnt, mat, vec);
	if (vals >= ELEMENTS_PER_POINT + ELEMENTS_PER_VECT)
	    MAT4X3VEC(datum_ip->dir, mat, vec+3);
	if (vals == MAX_VALS)
	    datum_ip->w = vec[6];

	datum_ip->magic = RT_DATUM_INTERNAL_MAGIC;
	if (prev)
	    prev->next = datum_ip;
	prev = datum_ip;
    }

    /* set up the internal structure */
    ip->idb_ptr = first;
    ip->idb_meth = &OBJ[ID_DATUM];
    ip->idb_type = ID_DATUM;
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;

    return 0;			/* OK */
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_datum_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_datum_internal *datum_ip = (struct rt_datum_internal *)ip->idb_ptr;

    RT_DATUM_CK_MAGIC(datum_ip);

    bu_vls_strcat(str, "Datum Set (DATUM)\n");
    if (!verbose)
	return 0;

    while (datum_ip) {
	if (!ZERO(datum_ip->w)) {
	    bu_vls_strcat(str, "\tPlane datum\n");
	} else if (MAGNITUDE(datum_ip->dir) > 0.0 && ZERO(datum_ip->w)) {
	    bu_vls_strcat(str, "\tLine datum\n");
	} else {
	    bu_vls_strcat(str, "\tPoint datum\n");
	}

	bu_vls_printf(str, "\t\tV (%g, %g, %g)\n",
		      INTCLAMP(datum_ip->pnt[X] * mm2local),
		      INTCLAMP(datum_ip->pnt[Y] * mm2local),
		      INTCLAMP(datum_ip->pnt[Z] * mm2local));

	if (!ZERO(datum_ip->w)) {
	    bu_vls_printf(str, "\t\tDIR (%g, %g, %g)\n\t\tW (%g)\n",
			  INTCLAMP(datum_ip->dir[X] * mm2local),
			  INTCLAMP(datum_ip->dir[Y] * mm2local),
			  INTCLAMP(datum_ip->dir[Z] * mm2local),
			  INTCLAMP(datum_ip->w));
	} else if (MAGNITUDE(datum_ip->dir) > 0.0 && ZERO(datum_ip->w)) {
	    bu_vls_printf(str, "\t\tDIR (%g, %g, %g)\n",
			  INTCLAMP(datum_ip->dir[X] * mm2local),
			  INTCLAMP(datum_ip->dir[Y] * mm2local),
			  INTCLAMP(datum_ip->dir[Z] * mm2local));
	}

	datum_ip = datum_ip->next;
    }

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
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
	datum_ip->magic = 0; /* sanity */
	bu_free((char *)datum_ip, "datum ifree");
	datum_ip = next;
    }
    ip->idb_ptr = NULL;	/* sanity */
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
