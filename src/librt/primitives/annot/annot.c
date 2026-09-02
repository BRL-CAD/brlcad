/*                        A N N O T . C
 * BRL-CAD
 *
 * Copyright (c) 2017-2026 United States Government as represented by
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
/** @file primitives/annot/annot.c
 *
 * Provide support for 2D annotations.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include "bnetwork.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/debug.h"
#include "bu/cv.h"
#include "bu/file.h"
#include "bu/mapped_file.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "bg/polygon.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "rt/primitives/annot.h"
#include "rt/primitives/bot.h"
#include "raytrace.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wbad-function-cast"
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#include "../../librt_private.h"

#define ANNOT_EXT_MAGIC 0x414e5432u /* "ANT2" */
#define ANNOT_EXT_VERSION 1u
#define ANNOT_PRES_MAGIC 0x414e5032u /* "ANP2" */
#define ANNOT_PRES_VERSION 1u

/* Stored line widths are display-scale multipliers, not model distances.
 * Primitive prep has no view pixel scale, so infer a stable model-space base
 * width from the annotation content before applying that multiplier. */
#define ANNOT_TEXT_STROKE_RATIO (1.0 / 14.0)
#define ANNOT_GEOMETRY_STROKE_RATIO 0.01
#define ANNOT_MIN_WIDTH_TOL_FACTOR 4.0

struct annot_mesh {
    point_t *vertices;
    size_t vertex_count;
    size_t vertex_capacity;
    int *faces;
    fastf_t *thickness;
    size_t face_count;
    size_t face_capacity;
};

C_DECL void rt_annot_ifree(struct rt_db_internal *ip);

/* Annotation prep uses BOT as a transient raytrace representation, as ARS
 * does.  These callbacks are internal to librt rather than public BOT APIs. */
C_DECL int rt_bot_prep(struct soltab *stp, struct rt_db_internal *ip,
	struct rt_i *rtip);
C_DECL int rt_bot_shot(struct soltab *stp, struct xray *rp,
	struct application *ap, struct seg *seghead);
C_DECL void rt_bot_print(const struct soltab *stp);
C_DECL void rt_bot_norm(struct hit *hitp, struct soltab *stp,
	struct xray *rp);
C_DECL void rt_bot_curve(struct curvature *cvp, struct hit *hitp,
	struct soltab *stp);
C_DECL void rt_bot_uv(struct application *ap, struct soltab *stp,
	struct hit *hitp, struct uvcoord *uvp);
C_DECL void rt_bot_free(struct soltab *stp);
C_DECL void rt_bot_ifree(struct rt_db_internal *ip);

static int annot_to_bot(struct rt_bot_internal **result,
	const struct rt_annot_internal *annot_ip,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	unsigned char bot_mode);

static uint32_t
annot_get_uint32(const unsigned char *cp)
{
    uint32_t value;

    memcpy(&value, cp, sizeof(value));
    return ntohl(value);
}


static void
annot_put_uint32(unsigned char *cp, uint32_t value)
{
    value = htonl(value);
    memcpy(cp, &value, sizeof(value));
}


static int
annot_has_extension(const struct rt_annot_internal *annot_ip)
{
    size_t i;
    if (annot_ip->flags || annot_ip->styles ||
	    MAGNITUDE(annot_ip->u_vec) > SMALL_FASTF ||
	    MAGNITUDE(annot_ip->v_vec) > SMALL_FASTF)
	return 1;
    for (i = 0; i < annot_ip->ant.count; ++i)
	if (annot_ip->ant.segments[i] &&
		*(uint32_t *)annot_ip->ant.segments[i] == ANN_FSEG_MAGIC)
	    return 1;
    return 0;
}


int
rt_txt_pos_flag(int *pos_flag, int p_hor, int p_ver)
{
    if (!pos_flag)
	return 1;

    /* sanity bounding */
    if (p_hor < 1)
	p_hor = 1;
    if (p_hor > 3)
	p_hor = 3;
    if (p_ver < 1)
	p_ver = 1;
    if (p_ver > 3)
	p_ver = 3;

    switch (p_ver) {
	case 1:
	    switch(p_hor) {
		case 1:
		    *pos_flag = RT_TXT_POS_BL;
		    break;
		case 2:
		    *pos_flag = RT_TXT_POS_BC;
		    break;
		case 3:
		    *pos_flag = RT_TXT_POS_BR;
	    }
	    break;
	case 2:
	    switch(p_hor) {
		case 1:
		    *pos_flag = RT_TXT_POS_ML;
		    break;
		case 2:
		    *pos_flag = RT_TXT_POS_MC;
		    break;
		case 3:
		    *pos_flag = RT_TXT_POS_MR;
	    }
	    break;
	case 3:
	    switch(p_hor) {
		case 1:
		    *pos_flag = RT_TXT_POS_TL;
		    break;
		case 2:
		    *pos_flag = RT_TXT_POS_TC;
		    break;
		case 3:
		    *pos_flag = RT_TXT_POS_TR;
	    }
    }
    return 0;
}


static int
ant_check_pos(const struct txt_seg *tsg, const char **rel_pos)
{
    switch (tsg->rel_pos) {
	case RT_TXT_POS_BL:
	    *rel_pos = "bottom left";
	    break;
	case RT_TXT_POS_BC:
	    *rel_pos = "bottom center";
	    break;
	case RT_TXT_POS_BR:
	    *rel_pos = "bottom right";
	    break;
	case RT_TXT_POS_ML:
	    *rel_pos = "middle left";
	    break;
	case RT_TXT_POS_MC:
	    *rel_pos = "middle center";
	    break;
	case RT_TXT_POS_MR:
	    *rel_pos = "middle right";
	    break;
	case RT_TXT_POS_TL:
	    *rel_pos = "top left";
	    break;
	case RT_TXT_POS_TC:
	    *rel_pos = "top center";
	    break;
	case RT_TXT_POS_TR:
	    *rel_pos = "top right";
	    break;
    }

    return 0;
}


static void
ant_label_dimensions(const struct txt_seg *tsg, hpoint_t ref_pt,
	fastf_t *length, fastf_t *height, struct bu_list *vlfree)
{
    point_t bmin, bmax;
    struct bu_list vhead;

    BU_LIST_INIT(&vhead);
    VSET(bmin, INFINITY, INFINITY, INFINITY);
    VSET(bmax, -INFINITY, -INFINITY, -INFINITY);

    bv_vlist_2string(&vhead, vlfree, tsg->label.vls_str, ref_pt[0], ref_pt[1], tsg->txt_size, tsg->txt_rot_angle);
    bv_vlist_bbox(&vhead, &bmin, &bmax, NULL, NULL);

    *length = bmax[0] - ref_pt[0];
    *height = bmax[1] - ref_pt[1];
    BV_FREE_VLIST(vlfree, &vhead);
}


static int
ant_pos_adjs(point2d_t adjusted, const struct txt_seg *tsg,
	const struct rt_annot_internal *annot_ip, struct bu_list *vlfree)
{
    fastf_t length = 0;
    fastf_t height = 0;

    V2MOVE(adjusted, annot_ip->verts[tsg->ref_pt]);
    ant_label_dimensions(tsg, annot_ip->verts[tsg->ref_pt], &length, &height, vlfree);

    if (tsg->rel_pos == RT_TXT_POS_BL) {
	adjusted[0] += 1;
    }else if (tsg->rel_pos == RT_TXT_POS_BC) {
	adjusted[0] -= length / 2;
    }else if (tsg->rel_pos == RT_TXT_POS_BR) {
	adjusted[0] -= length;
    }else if (tsg->rel_pos == RT_TXT_POS_ML) {
	adjusted[0] += 1;
	adjusted[1] -= height / 2;
    }else if (tsg->rel_pos == RT_TXT_POS_MC) {
	adjusted[0] -= length / 2;
	adjusted[1] -= height / 2;
    }else if (tsg->rel_pos == RT_TXT_POS_MR) {
	adjusted[1] -= height / 2;
	adjusted[0] -= length;
    }else if (tsg->rel_pos == RT_TXT_POS_TL) {
	adjusted[1] -= height;
    }else if (tsg->rel_pos == RT_TXT_POS_TC) {
	adjusted[0] -= length / 2;
	adjusted[1] -= height;
    } else {
	//this is the case of TR
	adjusted[0] -= length;
	adjusted[1] -= height;
    }
    return 0;
}


static void
annot_validation_message(struct bu_vls *messages, size_t index,
	const char *message)
{
    if (!messages)
	return;
    if (bu_vls_strlen(messages))
	bu_vls_strcat(messages, "; ");
    bu_vls_printf(messages, "annotation segment %zu: %s", index, message);
}


int
rt_annot_validate(const struct rt_annot_internal *annot_ip,
	struct bu_vls *messages)
{
    const struct rt_ant *ant;
    size_t i, j;
    int ret=0;

    if (!annot_ip) {
	annot_validation_message(messages, 0, "missing annotation");
	return 1;
    }
    RT_ANNOT_CK_MAGIC(annot_ip);
    ant = &annot_ip->ant;

    if (annot_ip->flags & ~RT_ANNOT_MODEL_SPACE) {
	annot_validation_message(messages, 0, "unknown placement flags");
	ret++;
    }
    if (!isfinite(annot_ip->V[X]) || !isfinite(annot_ip->V[Y]) ||
	    !isfinite(annot_ip->V[Z])) {
	annot_validation_message(messages, 0, "non-finite anchor");
	ret++;
    }
    for (i = 0; i < annot_ip->vert_count; ++i) {
	if (!isfinite(annot_ip->verts[i][X]) ||
		!isfinite(annot_ip->verts[i][Y])) {
	    annot_validation_message(messages, i, "non-finite control vertex");
	    ret++;
	}
    }
    if (annot_ip->flags & RT_ANNOT_MODEL_SPACE) {
	vect_t normal;
	if (MAGNITUDE(annot_ip->u_vec) <= SMALL_FASTF ||
		MAGNITUDE(annot_ip->v_vec) <= SMALL_FASTF) {
	    annot_validation_message(messages, 0,
		"model-space placement requires two nonzero basis vectors");
	    ret++;
	} else {
	    VCROSS(normal, annot_ip->u_vec, annot_ip->v_vec);
	    if (MAGNITUDE(normal) <= SMALL_FASTF) {
		annot_validation_message(messages, 0,
		    "model-space basis vectors are parallel");
		ret++;
	    }
	}
    }

    /* empty annotations are invalid */
    if (ant->count == 0) {
	annot_validation_message(messages, 0, "annotation is empty");
	return 1;
    }

    if (!ant->segments || !ant->reverse) {
	annot_validation_message(messages, 0, "missing segment arrays");
	return ret + 1;
    }

    for (i=0; i<ant->count; i++) {
	const struct line_seg *lsg;
	const struct carc_seg *csg;
	const struct nurb_seg *nsg;
	const struct bezier_seg *bsg;
	const struct txt_seg *tsg;
	const struct fill_seg *fsg;
	const uint32_t *lng;

	if (!ant->segments[i]) {
	    annot_validation_message(messages, i, "missing segment");
	    ret++;
	    continue;
	}
	lng = (uint32_t *)ant->segments[i];

	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		if ((size_t)lsg->start >= annot_ip->vert_count ||
		    (size_t)lsg->end >= annot_ip->vert_count)
		    ret++;
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)lng;
		if ((size_t)csg->start >= annot_ip->vert_count ||
		    (size_t)csg->end >= annot_ip->vert_count)
		    ret++;
		break;
	    case CURVE_NURB_MAGIC:
		nsg = (struct nurb_seg *)lng;
		for (j=0; j<(size_t)nsg->c_size; j++) {
		    if ((size_t)nsg->ctl_points[j] >= annot_ip->vert_count) {
			ret++;
			break;
		    }
		}
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
		for (j=0; j<=(size_t)bsg->degree; j++) {
		    if ((size_t)bsg->ctl_points[j] >= annot_ip->vert_count) {
			ret++;
			break;
		    }
		}
		break;
	    case ANN_TSEG_MAGIC:
		tsg = (struct txt_seg *)lng;
		if((size_t)tsg->ref_pt >= annot_ip->vert_count)
		    ret++;
		if((size_t)tsg->rel_pos > 9 || (size_t)tsg->rel_pos < 1)
		    ret++;
		if (!isfinite(tsg->txt_size) || tsg->txt_size <= 0.0) {
		    annot_validation_message(messages, i, "invalid text size");
		    ret++;
		}
		if (!isfinite(tsg->txt_rot_angle)) {
		    annot_validation_message(messages, i, "invalid text rotation");
		    ret++;
		}
		break;
	    case ANN_FSEG_MAGIC:
		fsg = (const struct fill_seg *)lng;
		if (fsg->loop_count < 1 || fsg->point_count < 3 ||
			!fsg->loop_ends || !fsg->points) {
		    annot_validation_message(messages, i, "invalid fill loops");
		    ret++;
		    break;
		}
		for (j = 0; j < (size_t)fsg->loop_count; ++j) {
		    int begin = j ? fsg->loop_ends[j - 1] : 0;
		    int end = fsg->loop_ends[j];
		    if (begin < 0 || end <= begin || end > fsg->point_count ||
			    end - begin < 3) {
			annot_validation_message(messages, i, "fill loop has fewer than three points");
			ret++;
			break;
		    }
		}
		if (fsg->loop_ends[fsg->loop_count - 1] != fsg->point_count) {
		    annot_validation_message(messages, i, "fill loop ends do not cover its points");
		    ret++;
		}
		for (j = 0; j < (size_t)fsg->point_count; ++j)
		    if (fsg->points[j] < 0 ||
			    (size_t)fsg->points[j] >= annot_ip->vert_count) {
			annot_validation_message(messages, i, "fill point index is out of range");
			ret++;
			break;
		    }
		if (fsg->legacy_start < 0 || fsg->legacy_count < 1 ||
			(size_t)fsg->legacy_start >= ant->count ||
			(size_t)fsg->legacy_count > ant->count -
			(size_t)fsg->legacy_start) {
		    annot_validation_message(messages, i, "fill has no compatible outline segments");
		    ret++;
		} else {
		    for (j = (size_t)fsg->legacy_start;
			    j < (size_t)(fsg->legacy_start + fsg->legacy_count); ++j)
			if (!ant->segments[j] ||
				*(uint32_t *)ant->segments[j] != CURVE_LSEG_MAGIC) {
			    annot_validation_message(messages, i,
				"fill compatibility range is not made of line segments");
			    ret++;
			    break;
			}
		}
		break;
	    default:
		ret++;
		annot_validation_message(messages, i, "unrecognized segment type");
		break;
	}
	if (annot_ip->styles) {
	    const struct rt_annot_seg_style *style = &annot_ip->styles[i];
	    if (style->role > RT_ANNOT_ROLE_TEXT_DECORATION) {
		annot_validation_message(messages, i, "unknown semantic role");
		ret++;
	    }
	    if (style->flags & ~(RT_ANNOT_STYLE_WIDTH | RT_ANNOT_STYLE_COLOR |
		    RT_ANNOT_STYLE_SCALE | RT_ANNOT_STYLE_FILLED |
		    RT_ANNOT_STYLE_UNDERLINE | RT_ANNOT_STYLE_OVERLINE |
		    RT_ANNOT_STYLE_STRIKETHROUGH | RT_ANNOT_STYLE_BOLD |
		    RT_ANNOT_STYLE_ITALIC)) {
		annot_validation_message(messages, i, "unknown style flags");
		ret++;
	    }
	    if (style->line_pattern > RT_ANNOT_LINE_PHANTOM) {
		annot_validation_message(messages, i, "unknown line pattern");
		ret++;
	    }
	    if ((style->flags & RT_ANNOT_STYLE_WIDTH) &&
		    (!isfinite(style->line_width) || style->line_width <= 0.0)) {
		annot_validation_message(messages, i, "invalid line width");
		ret++;
	    }
	    if ((style->flags & RT_ANNOT_STYLE_SCALE) &&
		    (!isfinite(style->x_scale) ||
		    !isfinite(style->xy_scale) || !isfinite(style->yx_scale) ||
		    !isfinite(style->y_scale) ||
		    fabs(style->x_scale * style->y_scale -
			style->xy_scale * style->yx_scale) <= SMALL_FASTF)) {
		annot_validation_message(messages, i, "invalid text or symbol scale");
		ret++;
	    }
	}
    }

    return ret;
}


/**
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid ANNOTATION, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 ANNOTATION is OK
 * !0 Error in description
 *
 */
C_DECL int
rt_annot_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct bg_tess_tol default_ttol = BG_TESS_TOL_INIT_TOL;
    struct bn_tol default_tol = BN_TOL_INIT_TOL;
    const struct rt_annot_internal *annot_ip;
    struct rt_bot_internal *bot = NULL;
    struct rt_db_internal bot_intern;
    const struct bg_tess_tol *ttol = &default_ttol;
    const struct bn_tol *tol = &default_tol;
    int ret;

    if (!stp)
	return -1;
    RT_CK_SOLTAB(stp);
    if (!ip)
	return -1;
    RT_CK_DB_INTERNAL(ip);
    if (rtip) {
	RT_CK_RTI(rtip);
	ttol = &rtip->rti_ttol;
	tol = &rtip->rti_tol;
    }

    annot_ip = (const struct rt_annot_internal *)ip->idb_ptr;
    if (rt_annot_validate(annot_ip, NULL))
	return -1;

    /* View-space annotations need camera state, which primitive prep does not
     * have.  Leave them non-intersecting until a view-aware prep API exists. */
    stp->st_specific = (void *)NULL;
    if (!(annot_ip->flags & RT_ANNOT_MODEL_SPACE))
	return 0;

    if (annot_to_bot(&bot, annot_ip, ttol, tol, RT_BOT_PLATE) || !bot)
	return -1;

    RT_DB_INTERNAL_INIT(&bot_intern);
    bot_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    bot_intern.idb_type = ID_BOT;
    bot_intern.idb_meth = &OBJ[ID_BOT];
    bot_intern.idb_ptr = bot;
    ret = rt_bot_prep(stp, &bot_intern, rtip);
    rt_bot_ifree(&bot_intern);
    return ret;
}


C_DECL void
rt_annot_print(const struct soltab *stp)
{
    if (!stp)
	return;
    RT_CK_SOLTAB(stp);
    if (stp->st_specific)
	rt_bot_print(stp);
}


/**
 * Intersect a ray with an annotation.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
C_DECL int
rt_annot_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    if (!stp || !rp || !ap || !seghead)
	return 0;

    RT_CK_SOLTAB(stp);
    RT_CK_RAY(rp);
    RT_CK_APPLICATION(ap);

    return stp->st_specific ? rt_bot_shot(stp, rp, ap, seghead) : 0;
}


/**
 * Vectorized annotation shooting uses the scalar wrapper so view-space
 * annotations without prepared geometry continue to miss safely.
 */
C_DECL void
rt_annot_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
{
    rt_vshot_via_shot(rt_annot_shot, stp, rp, segp, n, ap);
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
C_DECL void
rt_annot_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    if (!hitp || !rp)
	return;

    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);
    RT_CK_RAY(rp);

    if (stp && stp->st_specific) {
	rt_bot_norm(hitp, stp, rp);
	return;
    }
    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
}


/**
 * Return the curvature of the annotation.
 */
C_DECL void
rt_annot_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp || !hitp)
	return;

    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);

    if (stp && stp->st_specific) {
	rt_bot_curve(cvp, hitp, stp);
	return;
    }
    cvp->crv_c1 = cvp->crv_c2 = 0;

    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


C_DECL void
rt_annot_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);
    if (!uvp)
	return;
    if (stp && stp->st_specific)
	rt_bot_uv(ap, stp, hitp, uvp);
}


C_DECL void
rt_annot_free(struct soltab *stp)
{
    if (!stp)
	return;
    RT_CK_SOLTAB(stp);
    if (stp->st_specific)
	rt_bot_free(stp);
}


static int
annot_map_2d(point_t out, const point_t base, const point2d_t uv,
	const struct rt_annot_internal *annot_ip)
{
    if (annot_ip->flags & RT_ANNOT_MODEL_SPACE)
	VJOIN2(out, base, uv[X], annot_ip->u_vec, uv[Y], annot_ip->v_vec);
    else
	VSET(out, base[X] + uv[X], base[Y] + uv[Y], base[Z]);
    return 0;
}


#define ANNOT_DEFAULT_FONT "osifont-lgpl3fe.ttf"
#define ANNOT_FONT_APPL "librt annotation font"

static struct bu_mapped_file *
annot_try_font(const char *path, stt_fontinfo *font)
{
    struct bu_mapped_file *mapped;
    int offset;

    if (!path || !path[0] || !font)
	return NULL;
    mapped = bu_open_mapped_file(path, ANNOT_FONT_APPL);
    if (!mapped)
	return NULL;
    if (!mapped->buf || mapped->buflen == 0 || mapped->buflen > INT_MAX) {
	bu_close_mapped_file(mapped);
	return NULL;
    }
    offset = stt_GetFontOffsetForIndex((const unsigned char *)mapped->buf,
	(int)mapped->buflen, 0);
    if (offset < 0 || !stt_InitFont(font,
	    (const unsigned char *)mapped->buf, (int)mapped->buflen, offset)) {
	bu_close_mapped_file(mapped);
	return NULL;
    }
    return mapped;
}


static struct bu_mapped_file *
annot_open_font(const char *requested, stt_fontinfo *font)
{
    struct bu_mapped_file *mapped = NULL;
    char path[MAXPATHLEN];
    char filename[MAXPATHLEN];
    const char *name = requested;
    int filename_len;

    if (name && name[0]) {
	if (BU_STR_EQUIV(name, "osifont"))
	    name = ANNOT_DEFAULT_FONT;
	if (strchr(name, '/') || strchr(name, '\\')) {
	    mapped = annot_try_font(name, font);
	} else {
	    if (bu_dir(path, sizeof(path), BU_DIR_DATA, "fonts", name,
		    (const char *)NULL))
		mapped = annot_try_font(path, font);
	    if (!mapped && !strchr(name, '.')) {
		filename_len = snprintf(filename, sizeof(filename), "%s.ttf",
		    name);
		if (filename_len > 0 &&
			(size_t)filename_len < sizeof(filename) &&
			bu_dir(path, sizeof(path), BU_DIR_DATA, "fonts",
			    filename, (const char *)NULL))
		    mapped = annot_try_font(path, font);
	    }
	}
	if (mapped)
	    return mapped;
    }

    if (!name || !BU_STR_EQUAL(name, ANNOT_DEFAULT_FONT)) {
	if (bu_dir(path, sizeof(path), BU_DIR_DATA, "fonts",
		ANNOT_DEFAULT_FONT, (const char *)NULL))
	    mapped = annot_try_font(path, font);
    }
    return mapped;
}


static uint32_t
annot_utf8_next(const unsigned char **cursor, const unsigned char *end)
{
    const unsigned char *s;
    size_t remaining;
    uint32_t cp;

    if (!cursor || !*cursor || !end || *cursor >= end)
	return 0;
    s = *cursor;
    remaining = (size_t)(end - s);
    if (s[0] < 0x80) {
	*cursor = s + 1;
	return s[0];
    }
    if (s[0] >= 0xc2 && s[0] <= 0xdf && remaining >= 2 &&
	(s[1] & 0xc0) == 0x80) {
	*cursor = s + 2;
	return ((uint32_t)(s[0] & 0x1f) << 6) | (uint32_t)(s[1] & 0x3f);
    }
    if (s[0] >= 0xe0 && s[0] <= 0xef && remaining >= 3 &&
	(s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80 &&
	!(s[0] == 0xe0 && s[1] < 0xa0) &&
	!(s[0] == 0xed && s[1] >= 0xa0)) {
	cp = ((uint32_t)(s[0] & 0x0f) << 12) |
	    ((uint32_t)(s[1] & 0x3f) << 6) | (uint32_t)(s[2] & 0x3f);
	*cursor = s + 3;
	return cp;
    }
    if (s[0] >= 0xf0 && s[0] <= 0xf4 && remaining >= 4 &&
	(s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80 &&
	(s[3] & 0xc0) == 0x80 &&
	!(s[0] == 0xf0 && s[1] < 0x90) &&
	!(s[0] == 0xf4 && s[1] >= 0x90)) {
	cp = ((uint32_t)(s[0] & 0x07) << 18) |
	    ((uint32_t)(s[1] & 0x3f) << 12) |
	    ((uint32_t)(s[2] & 0x3f) << 6) | (uint32_t)(s[3] & 0x3f);
	*cursor = s + 4;
	return cp;
    }
    *cursor = s + 1;
    return 0xfffd;
}


static void
annot_local_point(struct bu_list *vlfree, struct bu_list *vhead,
	fastf_t x, fastf_t y, int command, point2d_t bmin, point2d_t bmax)
{
    point_t point;

    VSET(point, x, y, 0.0);
    BV_ADD_VLIST(vlfree, vhead, point, command);
    if (x < bmin[X]) bmin[X] = x;
    if (y < bmin[Y]) bmin[Y] = y;
    if (x > bmax[X]) bmax[X] = x;
    if (y > bmax[Y]) bmax[Y] = y;
}


static void
annot_quadratic(struct bu_list *vlfree, struct bu_list *vhead,
	const point2d_t start, const point2d_t control, const point2d_t end,
	point2d_t bmin, point2d_t bmax)
{
    int i;
    const int segments = 8;

    for (i = 1; i <= segments; ++i) {
	fastf_t t = (fastf_t)i / (fastf_t)segments;
	fastf_t mt = 1.0 - t;
	fastf_t x = mt*mt*start[X] + 2.0*mt*t*control[X] +
	    t*t*end[X];
	fastf_t y = mt*mt*start[Y] + 2.0*mt*t*control[Y] +
	    t*t*end[Y];
	annot_local_point(vlfree, vhead, x, y, BV_VLIST_LINE_DRAW,
	    bmin, bmax);
    }
}


static void
annot_cubic(struct bu_list *vlfree, struct bu_list *vhead,
	const point2d_t start, const point2d_t control1,
	const point2d_t control2, const point2d_t end,
	point2d_t bmin, point2d_t bmax)
{
    int i;
    const int segments = 12;

    for (i = 1; i <= segments; ++i) {
	fastf_t t = (fastf_t)i / (fastf_t)segments;
	fastf_t mt = 1.0 - t;
	fastf_t x = mt*mt*mt*start[X] + 3.0*mt*mt*t*control1[X] +
	    3.0*mt*t*t*control2[X] + t*t*t*end[X];
	fastf_t y = mt*mt*mt*start[Y] + 3.0*mt*mt*t*control1[Y] +
	    3.0*mt*t*t*control2[Y] + t*t*t*end[Y];
	annot_local_point(vlfree, vhead, x, y, BV_VLIST_LINE_DRAW,
	    bmin, bmax);
    }
}


static int
annot_font_text(struct bu_list *vlfree, struct bu_list *vhead,
	const point_t base, const struct rt_annot_internal *annot_ip,
	const struct txt_seg *tsg, const struct rt_annot_seg_style *style)
{
    struct bu_mapped_file *mapped;
    struct bu_list local;
    struct bv_vlist *vp;
    stt_fontinfo font;
    const unsigned char *cursor;
    const unsigned char *end;
    point2d_t bmin, bmax;
    fastf_t pen_x = 0.0;
    fastf_t pen_y = 0.0;
    fastf_t scale;
    fastf_t line_advance;
    fastf_t align_x = 0.0;
    fastf_t align_y = 0.0;
    fastf_t italic_shear = 0.0;
    fastf_t x_scale = 1.0;
    fastf_t xy_scale = 0.0;
    fastf_t yx_scale = 0.0;
    fastf_t y_scale = 1.0;
    fastf_t cosine, sine;
    int ascent, descent, line_gap;
    int previous_glyph = -1;
    const char *font_name = style ? style->font : NULL;

    if (style && (style->flags & RT_ANNOT_STYLE_SCALE)) {
	x_scale = style->x_scale;
	xy_scale = style->xy_scale;
	yx_scale = style->yx_scale;
	y_scale = style->y_scale;
    }
    if (style && (style->flags & RT_ANNOT_STYLE_ITALIC))
	italic_shear = 0.20;

    mapped = annot_open_font(font_name, &font);
    if (!mapped)
	return 0;
    scale = stt_ScaleForPixelHeight(&font, (float)tsg->txt_size);
    if (!(scale > 0.0) || !isfinite(scale)) {
	bu_close_mapped_file(mapped);
	return 0;
    }
    stt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
    line_advance = (fastf_t)(ascent - descent + line_gap) * scale;
    if (!(line_advance > 0.0) || !isfinite(line_advance))
	line_advance = tsg->txt_size;

    BU_LIST_INIT(&local);
    V2SET(bmin, 0.0, (fastf_t)descent * scale);
    V2SET(bmax, 0.0, (fastf_t)ascent * scale);
    cursor = (const unsigned char *)bu_vls_cstr(&tsg->label);
    end = cursor + bu_vls_strlen(&tsg->label);
    while (cursor < end) {
	uint32_t codepoint = annot_utf8_next(&cursor, end);
	int glyph, advance, bearing;
	stt_vertex *vertices = NULL;
	int vertex_count, i;

	if (codepoint == '\r')
	    continue;
	if (codepoint == '\n') {
	    if (pen_x > bmax[X]) bmax[X] = pen_x;
	    pen_x = 0.0;
	    pen_y -= line_advance;
	    if (pen_y + (fastf_t)descent*scale < bmin[Y])
		bmin[Y] = pen_y + (fastf_t)descent*scale;
	    previous_glyph = -1;
	    continue;
	}
	if (codepoint == '\t') {
	    glyph = stt_FindGlyphIndex(&font, ' ');
	    stt_GetGlyphHMetrics(&font, glyph, &advance, &bearing);
	    pen_x += 4.0 * (fastf_t)advance * scale;
	    if (pen_x > bmax[X]) bmax[X] = pen_x;
	    previous_glyph = -1;
	    continue;
	}

	glyph = stt_FindGlyphIndex(&font, (int)codepoint);
	if (previous_glyph >= 0) {
	    int kern = stt_GetGlyphKernAdvance(&font, previous_glyph, glyph);
	    pen_x += (fastf_t)kern * scale;
	}
	vertex_count = stt_GetGlyphShape(&font, glyph, &vertices);
	if (vertex_count > 0 && vertices) {
	    point2d_t current = V2INIT_ZERO;
	    for (i = 0; i < vertex_count; ++i) {
		point2d_t target, control1, control2;
		V2SET(target, pen_x + (fastf_t)vertices[i].x*scale,
		    pen_y + (fastf_t)vertices[i].y*scale);
		switch (vertices[i].type) {
		    case STT_vmove:
			V2MOVE(current, target);
			annot_local_point(vlfree, &local, current[X], current[Y],
			    BV_VLIST_LINE_MOVE, bmin, bmax);
			break;
		    case STT_vline:
			annot_local_point(vlfree, &local, target[X], target[Y],
			    BV_VLIST_LINE_DRAW, bmin, bmax);
			V2MOVE(current, target);
			break;
		    case STT_vcurve:
			V2SET(control1,
			    pen_x + (fastf_t)vertices[i].cx*scale,
			    pen_y + (fastf_t)vertices[i].cy*scale);
			annot_quadratic(vlfree, &local, current, control1,
			    target, bmin, bmax);
			V2MOVE(current, target);
			break;
		    case STT_vcubic:
			V2SET(control1,
			    pen_x + (fastf_t)vertices[i].cx*scale,
			    pen_y + (fastf_t)vertices[i].cy*scale);
			V2SET(control2,
			    pen_x + (fastf_t)vertices[i].cx1*scale,
			    pen_y + (fastf_t)vertices[i].cy1*scale);
			annot_cubic(vlfree, &local, current, control1,
			    control2, target, bmin, bmax);
			V2MOVE(current, target);
			break;
		}
	    }
	}
	if (vertices)
	    stt_FreeShape(&font, vertices);
	stt_GetGlyphHMetrics(&font, glyph, &advance, &bearing);
	pen_x += (fastf_t)advance * scale;
	if (pen_x > bmax[X]) bmax[X] = pen_x;
	if (pen_x < bmin[X]) bmin[X] = pen_x;
	previous_glyph = glyph;
    }

    if (style && (style->flags & (RT_ANNOT_STYLE_UNDERLINE |
	    RT_ANNOT_STYLE_OVERLINE | RT_ANNOT_STYLE_STRIKETHROUGH)) &&
	    bmax[X] > bmin[X]) {
	point_t decoration;
	fastf_t levels[3];
	size_t level_count = 0;
	if (style->flags & RT_ANNOT_STYLE_UNDERLINE)
	    levels[level_count++] = bmin[Y] - 0.08 * tsg->txt_size;
	if (style->flags & RT_ANNOT_STYLE_OVERLINE)
	    levels[level_count++] = bmax[Y] + 0.05 * tsg->txt_size;
	if (style->flags & RT_ANNOT_STYLE_STRIKETHROUGH)
	    levels[level_count++] = bmin[Y] + 0.45 * (bmax[Y] - bmin[Y]);
	for (size_t i = 0; i < level_count; ++i) {
	    VSET(decoration, bmin[X], levels[i], 0.0);
	    BV_ADD_VLIST(vlfree, &local, decoration, BV_VLIST_LINE_MOVE);
	    VSET(decoration, bmax[X], levels[i], 0.0);
	    BV_ADD_VLIST(vlfree, &local, decoration, BV_VLIST_LINE_DRAW);
	}
    }

    {
	fastf_t sheared_min_x = bmin[X] +
	    ((italic_shear * bmin[Y] < italic_shear * bmax[Y]) ?
	    italic_shear * bmin[Y] : italic_shear * bmax[Y]);
	fastf_t sheared_max_x = bmax[X] +
	    ((italic_shear * bmin[Y] > italic_shear * bmax[Y]) ?
	    italic_shear * bmin[Y] : italic_shear * bmax[Y]);
    switch (tsg->rel_pos) {
	case RT_TXT_POS_BL: case RT_TXT_POS_ML: case RT_TXT_POS_TL:
	    align_x = -sheared_min_x;
	    break;
	case RT_TXT_POS_BC: case RT_TXT_POS_MC: case RT_TXT_POS_TC:
	    align_x = -(sheared_min_x + sheared_max_x) * 0.5;
	    break;
	default:
	    align_x = -sheared_max_x;
	    break;
    }
    }
    switch (tsg->rel_pos) {
	case RT_TXT_POS_BL: case RT_TXT_POS_BC: case RT_TXT_POS_BR:
	    align_y = -bmin[Y];
	    break;
	case RT_TXT_POS_ML: case RT_TXT_POS_MC: case RT_TXT_POS_MR:
	    align_y = -(bmin[Y] + bmax[Y]) * 0.5;
	    break;
	default:
	    align_y = -bmax[Y];
	    break;
    }

    cosine = cos(tsg->txt_rot_angle * DEG2RAD);
    sine = sin(tsg->txt_rot_angle * DEG2RAD);
    for (BU_LIST_FOR(vp, bv_vlist, &local)) {
	size_t i;
	for (i = 0; i < vp->nused; ++i) {
	    point2d_t uv;
	    point_t point;
	    fastf_t x = vp->pt[i][X] + italic_shear * vp->pt[i][Y] +
		align_x;
	    fastf_t y = vp->pt[i][Y] + align_y;
	    fastf_t rx = cosine*x - sine*y;
	    fastf_t ry = sine*x + cosine*y;
	    fastf_t mx = x_scale*rx + xy_scale*ry;
	    fastf_t my = yx_scale*rx + y_scale*ry;
	    V2SET(uv, annot_ip->verts[tsg->ref_pt][X] + mx,
		annot_ip->verts[tsg->ref_pt][Y] + my);
	    annot_map_2d(point, base, uv, annot_ip);
	    BV_ADD_VLIST(vlfree, vhead, point, vp->cmd[i]);
	}
    }

    BV_FREE_VLIST(vlfree, &local);
    bu_close_mapped_file(mapped);
    return 1;
}


static int
annot_fill_vlist(struct bu_list *vlfree, struct bu_list *vhead,
	const point_t base, const struct rt_annot_internal *annot_ip,
	const struct fill_seg *fsg)
{
    const int **holes = NULL;
    size_t *hole_sizes = NULL;
    int *faces = NULL;
    int face_count = 0;
    int first_end;
    int ret;
    int i;
    vect_t normal;

    if (!fsg || fsg->loop_count < 1 || !fsg->loop_ends || !fsg->points)
	return 1;
    first_end = fsg->loop_ends[0];
    if (fsg->loop_count > 1) {
	holes = (const int **)bu_calloc((size_t)fsg->loop_count - 1,
	    sizeof(int *), "annotation fill hole pointers");
	hole_sizes = (size_t *)bu_calloc((size_t)fsg->loop_count - 1,
	    sizeof(size_t), "annotation fill hole sizes");
	for (i = 1; i < fsg->loop_count; ++i) {
	    holes[i - 1] = &fsg->points[fsg->loop_ends[i - 1]];
	    hole_sizes[i - 1] = (size_t)(fsg->loop_ends[i] -
		fsg->loop_ends[i - 1]);
	}
    }
    ret = bg_nested_poly_triangulate(&faces, &face_count, NULL, NULL,
	fsg->points, (size_t)first_end, holes, hole_sizes,
	(size_t)fsg->loop_count - 1, NULL, 0,
	(const point2d_t *)annot_ip->verts,
	annot_ip->vert_count, TRI_EAR_CLIPPING);
    if (holes) bu_free((void *)holes, "annotation fill hole pointers");
    if (hole_sizes) bu_free(hole_sizes, "annotation fill hole sizes");
    if (ret || !faces || face_count < 1) {
	if (faces) bu_free(faces, "annotation fill faces");
	return 1;
    }

    if (annot_ip->flags & RT_ANNOT_MODEL_SPACE) {
	VCROSS(normal, annot_ip->u_vec, annot_ip->v_vec);
	VUNITIZE(normal);
    } else {
	VSET(normal, 0.0, 0.0, 1.0);
    }
    for (i = 0; i < face_count; ++i) {
	point_t p0, p1, p2;
	annot_map_2d(p0, base, annot_ip->verts[faces[3*i]], annot_ip);
	annot_map_2d(p1, base, annot_ip->verts[faces[3*i + 1]], annot_ip);
	annot_map_2d(p2, base, annot_ip->verts[faces[3*i + 2]], annot_ip);
	BV_ADD_VLIST(vlfree, vhead, normal, BV_VLIST_POLY_START);
	BV_ADD_VLIST(vlfree, vhead, p2, BV_VLIST_POLY_MOVE);
	BV_ADD_VLIST(vlfree, vhead, p0, BV_VLIST_POLY_DRAW);
	BV_ADD_VLIST(vlfree, vhead, p1, BV_VLIST_POLY_DRAW);
	BV_ADD_VLIST(vlfree, vhead, p2, BV_VLIST_POLY_END);
    }
    bu_free(faces, "annotation fill faces");
    return 0;
}


static int
seg_to_vlist(struct bu_list *vlfree, struct bu_list *vhead, const struct bg_tess_tol *ttol, fastf_t *V, struct rt_annot_internal *annot_ip, void *seg, const struct rt_annot_seg_style *style)
{
    int ret=0;
    int i;
    uint32_t *lng;
    struct line_seg *lsg;
    struct txt_seg *tsg;
    struct fill_seg *fsg;
    struct carc_seg *csg;
    struct nurb_seg *nsg;
    struct bezier_seg *bsg;
    fastf_t delta;
    point_t center = VINIT_ZERO;
    point_t start_pt = VINIT_ZERO;
    hpoint_t pt = HINIT_ZERO;
    vect_t semi_a, semi_b;
    fastf_t radius;
    vect_t norm;

    BU_CK_LIST_HEAD(vhead);

    VSETALL(semi_a, 0);
    VSETALL(semi_b, 0);
    VSETALL(center, 0);
    lng = (uint32_t *)seg;
    switch (*lng) {
	case CURVE_LSEG_MAGIC:
	    lsg = (struct line_seg *)lng;
	    if ((size_t)lsg->start >= annot_ip->vert_count || (size_t)lsg->end >= annot_ip->vert_count) {
		ret++;
		break;
	    }
	    annot_map_2d(pt, V, annot_ip->verts[lsg->start], annot_ip);
	    BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_MOVE);
	    annot_map_2d(pt, V, annot_ip->verts[lsg->end], annot_ip);
	    BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_DRAW);
	    break;
	case ANN_TSEG_MAGIC:
	    tsg = (struct txt_seg *)lng;
	    if((size_t)tsg->ref_pt >= annot_ip->vert_count) {
		ret++;
		break;
	    }
	    if((size_t)tsg->rel_pos > 9 || (size_t)tsg->rel_pos < 1) {
		ret++;
		break;
	    }
	    if (annot_font_text(vlfree, vhead, V, annot_ip, tsg, style))
		break;
	    {
		point2d_t adjusted;
		ant_pos_adjs(adjusted, tsg, annot_ip, vlfree);
		annot_map_2d(pt, V, adjusted, annot_ip);
		if (annot_ip->flags & RT_ANNOT_MODEL_SPACE) {
		    mat_t basis, rotation, text_mat;
		    vect_t normal;
		    MAT_IDN(basis);
		    basis[0] = annot_ip->u_vec[X];
		    basis[4] = annot_ip->u_vec[Y];
		    basis[8] = annot_ip->u_vec[Z];
		    basis[1] = annot_ip->v_vec[X];
		    basis[5] = annot_ip->v_vec[Y];
		    basis[9] = annot_ip->v_vec[Z];
		    VCROSS(normal, annot_ip->u_vec, annot_ip->v_vec);
		    VUNITIZE(normal);
		    basis[2] = normal[X];
		    basis[6] = normal[Y];
		    basis[10] = normal[Z];
		    bn_mat_angles(rotation, 0.0, 0.0, tsg->txt_rot_angle);
		    bn_mat_mul(text_mat, basis, rotation);
		    bv_vlist_3string(vhead, vlfree, tsg->label.vls_str, pt,
			text_mat, tsg->txt_size);
		} else {
		    bv_vlist_2string(vhead, vlfree, tsg->label.vls_str,
			pt[0], pt[1], tsg->txt_size, tsg->txt_rot_angle);
		}
	    }
	    break;
	case ANN_FSEG_MAGIC:
	    fsg = (struct fill_seg *)lng;
	    ret += annot_fill_vlist(vlfree, vhead, V, annot_ip, fsg);
	    break;
	case CURVE_CARC_MAGIC:
	    {
		point2d_t mid_pt, start2d, end2d, center2d, s2m, dir, new_uv;
		fastf_t s2m_len_sq, len_sq, tmp_len, cross_z;
		fastf_t start_ang, end_ang, tot_ang, cosdel, sindel;
		fastf_t oldu, oldv, newu, newv;
		int nsegs;

		csg = (struct carc_seg *)lng;
		if ((size_t)csg->start >= annot_ip->vert_count || (size_t)csg->end >= annot_ip->vert_count) {
		    ret++;
		    break;
		}

		delta = M_PI_4;
		if (csg->radius <= 0.0) {
		    annot_map_2d(center, V, annot_ip->verts[csg->end], annot_ip);
		    annot_map_2d(pt, V, annot_ip->verts[csg->start], annot_ip);

		    VSUB2(semi_a, pt, center);
		    if (annot_ip->flags & RT_ANNOT_MODEL_SPACE) {
			VCROSS(norm, annot_ip->u_vec, annot_ip->v_vec);
			VUNITIZE(norm);
		    } else {
			VSET(norm, 0, 0, 1);
		    }
		    VCROSS(semi_b, norm, semi_a);
		    VUNITIZE(semi_b);
		    radius = MAGNITUDE(semi_a);
		    VSCALE(semi_b, semi_b, radius);
		} else if (csg->radius <= SMALL_FASTF) {
		    bu_log("Radius too small in annotation!\n");
		    break;
		} else {
		    radius = csg->radius;
		}

		if (ttol->abs > 0.0) {
		    fastf_t tmp_delta, ratio;

		    ratio = ttol->abs / radius;
		    if (ratio < 1.0) {
			tmp_delta = 2.0 * acos(1.0 - ratio);
			if (tmp_delta < delta)
			    delta = tmp_delta;
		    }
		}
		if (ttol->rel > 0.0 && ttol->rel < 1.0) {
		    fastf_t tmp_delta;

		    tmp_delta = 2.0 * acos(1.0 - ttol->rel);
		    if (tmp_delta < delta)
			delta = tmp_delta;
		}
		if (ttol->norm > 0.0) {
		    fastf_t normal;

		    normal = ttol->norm * DEG2RAD;
		    if (normal < delta)
			delta = normal;
		}
		if (csg->radius <= 0.0) {
		    /* this is a full circle */
		    nsegs = ceil(M_2PI / delta);
		    delta = M_2PI / (double)nsegs;
		    cosdel = cos(delta);
		    sindel = sin(delta);
		    oldu = 1.0;
		    oldv = 0.0;
		    VJOIN2(start_pt, center, oldu, semi_a, oldv, semi_b);
		    BV_ADD_VLIST(vlfree, vhead, start_pt, BV_VLIST_LINE_MOVE);
		    for (i=1; i<nsegs; i++) {
			newu = oldu * cosdel - oldv * sindel;
			newv = oldu * sindel + oldv * cosdel;
			VJOIN2(pt, center, newu, semi_a, newv, semi_b);
			BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_DRAW);
			oldu = newu;
			oldv = newv;
		    }
		    BV_ADD_VLIST(vlfree, vhead, start_pt, BV_VLIST_LINE_DRAW);
		    break;
		}

		/* this is an arc (not a full circle) */
		V2MOVE(start2d, annot_ip->verts[csg->start]);
		V2MOVE(end2d, annot_ip->verts[csg->end]);
		mid_pt[0] = (start2d[0] + end2d[0]) * 0.5;
		mid_pt[1] = (start2d[1] + end2d[1]) * 0.5;
		V2SUB2(s2m, mid_pt, start2d);
		dir[0] = -s2m[1];
		dir[1] = s2m[0];
		s2m_len_sq =  s2m[0]*s2m[0] + s2m[1]*s2m[1];
		if (s2m_len_sq <= SMALL_FASTF) {
		    bu_log("start and end points are too close together in circular arc of annotation\n");
		    break;
		}
		len_sq = radius*radius - s2m_len_sq;
		if (len_sq < 0.0) {
		    bu_log("Impossible radius for specified start and end points in circular arc\n");
		    break;
		}
		tmp_len = sqrt(dir[0]*dir[0] + dir[1]*dir[1]);
		dir[0] = dir[0] / tmp_len;
		dir[1] = dir[1] / tmp_len;
		tmp_len = sqrt(len_sq);
		V2JOIN1(center2d, mid_pt, tmp_len, dir);

		/* check center location */
		cross_z = (end2d[X] - start2d[X])*(center2d[Y] - start2d[Y]) -
		    (end2d[Y] - start2d[Y])*(center2d[X] - start2d[X]);
		if (!(cross_z > 0.0 && csg->center_is_left))
		    V2JOIN1(center2d, mid_pt, -tmp_len, dir);
		start_ang = atan2(start2d[Y]-center2d[Y], start2d[X]-center2d[X]);
		end_ang = atan2(end2d[Y]-center2d[Y], end2d[X]-center2d[X]);
		if (csg->orientation) {
		    /* clock-wise */
		    while (end_ang > start_ang)
			end_ang -= M_2PI;
		} else {
		    /* counter-clock-wise */
		    while (end_ang < start_ang)
			end_ang += M_2PI;
		}
		tot_ang = end_ang - start_ang;
		nsegs = ceil(tot_ang / delta);
		if (nsegs < 0)
		    nsegs = -nsegs;
		if (nsegs < 3)
		    nsegs = 3;
		delta = tot_ang / nsegs;
		cosdel = cos(delta);
		sindel = sin(delta);

		annot_map_2d(center, V, center2d, annot_ip);
		annot_map_2d(start_pt, V, start2d, annot_ip);
		oldu = (start2d[0] - center2d[0]);
		oldv = (start2d[1] - center2d[1]);
		BV_ADD_VLIST(vlfree, vhead, start_pt, BV_VLIST_LINE_MOVE);
		for (i=0; i<nsegs; i++) {
		    newu = oldu * cosdel - oldv * sindel;
		    newv = oldu * sindel + oldv * cosdel;
		    V2SET(new_uv, newu, newv);
		    annot_map_2d(pt, center, new_uv, annot_ip);
		    BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_DRAW);
		    oldu = newu;
		    oldv = newv;
		}
		break;
	    }
	case CURVE_NURB_MAGIC:
	    {
		struct edge_g_cnurb eg;
		int coords;
		fastf_t inv_weight;
		int num_intervals;
		fastf_t param_delta, epsilon;

		nsg = (struct nurb_seg *)lng;
		for (i=0; i<nsg->c_size; i++) {
		    if ((size_t)nsg->ctl_points[i] >= annot_ip->vert_count) {
			ret++;
			break;
		    }
		}
		if (nsg->order < 3) {
		    /* just straight lines */
		    annot_map_2d(start_pt, V, annot_ip->verts[nsg->ctl_points[0]], annot_ip);

		    if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
			inv_weight = 1.0/nsg->weights[0];
			VSCALE(start_pt, start_pt, inv_weight);
		    }
		    BV_ADD_VLIST(vlfree, vhead, start_pt, BV_VLIST_LINE_MOVE);
		    for (i=1; i<nsg->c_size; i++) {
			annot_map_2d(pt, V, annot_ip->verts[nsg->ctl_points[i]], annot_ip);
			if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
			    inv_weight = 1.0/nsg->weights[i];
			    VSCALE(pt, pt, inv_weight);
			}
			BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_DRAW);
		    }
		    break;
		}
		eg.l.magic = NMG_EDGE_G_CNURB_MAGIC;
		eg.order = nsg->order;
		eg.k.k_size = nsg->k.k_size;
		eg.k.knots = nsg->k.knots;
		eg.c_size = nsg->c_size;
		coords = 3 + RT_NURB_IS_PT_RATIONAL(nsg->pt_type);
		eg.pt_type = RT_NURB_MAKE_PT_TYPE(coords, 2, RT_NURB_IS_PT_RATIONAL(nsg->pt_type));
		eg.ctl_points = (fastf_t *)bu_malloc(nsg->c_size * coords * sizeof(fastf_t), "eg.ctl_points");
		if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
		    for (i=0; i<nsg->c_size; i++) {
			annot_map_2d(&eg.ctl_points[i*coords], V,
			    annot_ip->verts[nsg->ctl_points[i]], annot_ip);
			eg.ctl_points[(i+1)*coords - 1] = nsg->weights[i];
		    }
		} else {
		    for (i=0; i<nsg->c_size; i++) {
			annot_map_2d(&eg.ctl_points[i*coords], V,
			    annot_ip->verts[nsg->ctl_points[i]], annot_ip);
		    }
		}
		epsilon = MAX_FASTF;
		if (ttol->abs > 0.0 && ttol->abs < epsilon)
		    epsilon = ttol->abs;
		if (ttol->rel > 0.0) {
		    point2d_t min_pt, max_pt, tmp_pt;
		    point2d_t diff;
		    fastf_t tmp_epsilon;

		    min_pt[0] = MAX_FASTF;
		    min_pt[1] = MAX_FASTF;
		    max_pt[0] = -MAX_FASTF;
		    max_pt[1] = -MAX_FASTF;

		    for (i=0; i<nsg->c_size; i++) {
			V2MOVE(tmp_pt, annot_ip->verts[nsg->ctl_points[i]]);
			if (tmp_pt[0] > max_pt[0])
			    max_pt[0] = tmp_pt[0];
			if (tmp_pt[1] > max_pt[1])
			    max_pt[1] = tmp_pt[1];
			if (tmp_pt[0] < min_pt[0])
			    min_pt[0] = tmp_pt[0];
			if (tmp_pt[1] < min_pt[1])
			    min_pt[1] = tmp_pt[1];
		    }

		    V2SUB2(diff, max_pt, min_pt);
		    tmp_epsilon = ttol->rel * sqrt(MAG2SQ(diff));
		    if (tmp_epsilon < epsilon)
			epsilon = tmp_epsilon;

		}
		param_delta = rt_cnurb_par_edge(&eg, epsilon);
		num_intervals = ceil((nsg->k.knots[nsg->k.k_size-1] - nsg->k.knots[0])/param_delta);
		if (num_intervals < 3)
		    num_intervals = 3;
		if (num_intervals > 500) {
		    bu_log("num_intervals was %d, clamped to 500\n", num_intervals);
		    num_intervals = 500;
		}
		param_delta = (nsg->k.knots[nsg->k.k_size-1] - nsg->k.knots[0])/(double)num_intervals;
		for (i=0; i<=num_intervals; i++) {
		    fastf_t t;
		    int j;

		    t = nsg->k.knots[0] + i*param_delta;
		    nmg_nurb_c_eval(&eg, t, pt);
		    if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
			for (j=0; j<coords-1; j++)
			    pt[j] /= pt[coords-1];
		    }
		    if (i == 0)
			BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_MOVE);
		    else
			BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_DRAW);
		}
		bu_free((char *)eg.ctl_points, "eg.ctl_points");
		break;
	    }
	case CURVE_BEZIER_MAGIC: {
	    struct bezier_2d_list *bezier_hd, *bz;
	    fastf_t epsilon;

	    bsg = (struct bezier_seg *)lng;

	    for (i=0; i<=bsg->degree; i++) {
		if ((size_t)bsg->ctl_points[i] >= annot_ip->vert_count) {
		    ret++;
		    break;
		}
	    }

	    if (bsg->degree < 1) {
		bu_log("g_annot: ERROR: Bezier curve with illegal degree (%d)\n",
		       bsg->degree);
		ret++;
		break;
	    }

	    if (bsg->degree == 1) {
		/* straight line */
		annot_map_2d(start_pt, V, annot_ip->verts[bsg->ctl_points[0]], annot_ip);
		BV_ADD_VLIST(vlfree, vhead, start_pt, BV_VLIST_LINE_MOVE);
		for (i=1; i<=bsg->degree; i++) {
		    annot_map_2d(pt, V, annot_ip->verts[bsg->ctl_points[i]], annot_ip);
		    BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_DRAW);
		}
		break;
	    }

	    /* use tolerance to determine coarseness of plot */
	    epsilon = MAX_FASTF;
	    if (ttol->abs > 0.0 && ttol->abs < epsilon)
		epsilon = ttol->abs;
	    if (ttol->rel > 0.0) {
		point2d_t min_pt, max_pt, tmp_pt;
		point2d_t diff;
		fastf_t tmp_epsilon;

		min_pt[0] = MAX_FASTF;
		min_pt[1] = MAX_FASTF;
		max_pt[0] = -MAX_FASTF;
		max_pt[1] = -MAX_FASTF;

		for (i=0; i<=bsg->degree; i++) {
		    V2MOVE(tmp_pt, annot_ip->verts[bsg->ctl_points[i]]);
		    if (tmp_pt[0] > max_pt[0])
			max_pt[0] = tmp_pt[0];
		    if (tmp_pt[1] > max_pt[1])
			max_pt[1] = tmp_pt[1];
		    if (tmp_pt[0] < min_pt[0])
			min_pt[0] = tmp_pt[0];
		    if (tmp_pt[1] < min_pt[1])
			min_pt[1] = tmp_pt[1];
		}

		V2SUB2(diff, max_pt, min_pt);
		tmp_epsilon = ttol->rel * sqrt(MAG2SQ(diff));
		if (tmp_epsilon < epsilon)
		    epsilon = tmp_epsilon;

	    }


	    /* Create an initial bezier_2d_list */
	    BU_ALLOC(bezier_hd, struct bezier_2d_list);

	    BU_LIST_INIT(&bezier_hd->l);
	    bezier_hd->ctl = (point2d_t *)bu_calloc(bsg->degree + 1, sizeof(point2d_t),
						    "g_annot.c: bezier_hd->ctl");
	    for (i=0; i<=bsg->degree; i++) {
		V2MOVE(bezier_hd->ctl[i], annot_ip->verts[bsg->ctl_points[i]]);
	    }

	    /* now do subdivision as necessary */
	    bezier_hd = bezier_subdivide(bezier_hd, bsg->degree, epsilon, 0);

	    /* plot the results */
	    bz = BU_LIST_FIRST(bezier_2d_list, &bezier_hd->l);
	    annot_map_2d(pt, V, bz->ctl[0], annot_ip);
	    BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_MOVE);

	    while (BU_LIST_WHILE(bz, bezier_2d_list, &(bezier_hd->l))) {
		BU_LIST_DEQUEUE(&bz->l);
		for (i=1; i<=bsg->degree; i++) {
		    annot_map_2d(pt, V, bz->ctl[i], annot_ip);
		    BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_DRAW);
		}
		bu_free((char *)bz->ctl, "g_annot.c: bz->ctl");
		bu_free((char *)bz, "g_annot.c: bz");
	    }
	    bu_free((char *)bezier_hd, "g_annot.c: bezier_hd");
	    break;
	}
	default:
	    bu_log("seg_to_vlist: ERROR: unrecognized segment type!\n");
	    break;
    }

    return ret;
}


static int
annot_is_fill_compatibility_outline(const struct rt_ant *ant, size_t seg_no)
{
    size_t i;

    if (!ant) return 0;
    for (i = 0; i < ant->count; ++i) {
	const struct fill_seg *fsg;
	if (!ant->segments[i] ||
		*(uint32_t *)ant->segments[i] != ANN_FSEG_MAGIC)
	    continue;
	fsg = (const struct fill_seg *)ant->segments[i];
	if (fsg->legacy_start >= 0 && fsg->legacy_count > 0 &&
		seg_no >= (size_t)fsg->legacy_start &&
		seg_no < (size_t)fsg->legacy_start +
		(size_t)fsg->legacy_count)
	    return 1;
    }
    return 0;
}


static int
ant_to_vlist(struct bu_list *vlfree, struct bu_list *vhead, const struct bg_tess_tol *ttol, fastf_t *V, struct rt_annot_internal *annot_ip, struct rt_ant *ant)
{
    size_t seg_no;
    int ret=0;
    point_t base = VINIT_ZERO;

    BU_CK_LIST_HEAD(vhead);


    if (annot_ip->flags & RT_ANNOT_MODEL_SPACE)
	VMOVE(base, annot_ip->V);
    else
	BV_VLIST_SET_DISP_MAT(vlfree, vhead, annot_ip->V);

    /* Filled areas, in particular OpenNURBS-style text masks and STEP
     * blanking boxes, are backgrounds.  Emit them before strokes and text
     * regardless of their storage position. */
    for (int fill_pass = 1; fill_pass >= 0; --fill_pass) {
	for (seg_no=0; seg_no < ant->count; seg_no++) {
	    const int is_fill = ant->segments[seg_no] &&
		*(uint32_t *)ant->segments[seg_no] == ANN_FSEG_MAGIC;
	    int custom_width = 0;
	    fastf_t width = 1.0;
	    if (is_fill != fill_pass)
		continue;
	    /* These line segments are deliberately kept in ANT2 so main-era
	     * readers have a visible fallback.  ANP2-aware readers render the
	     * actual fill and must not add an unintended border (notably around
	     * text blanking masks). */
	    if (!fill_pass && annot_is_fill_compatibility_outline(ant, seg_no))
		continue;
	    if (annot_ip->styles) {
		const struct rt_annot_seg_style *style =
		    &annot_ip->styles[seg_no];
		if (style->flags & RT_ANNOT_STYLE_WIDTH) {
		    custom_width = 1;
		    width = style->line_width;
		} else if ((style->flags & RT_ANNOT_STYLE_BOLD) &&
			ant->segments[seg_no] &&
			*(uint32_t *)ant->segments[seg_no] == ANN_TSEG_MAGIC) {
		    custom_width = 1;
		    width = 2.0;
		}
	    }
	    if (custom_width)
		BV_VLIST_SET_LINE_WIDTH(vlfree, vhead, width);
	    ret += seg_to_vlist(vlfree, vhead, ttol, base, annot_ip,
		ant->segments[seg_no], annot_ip->styles ?
		&annot_ip->styles[seg_no] : NULL);
	    if (custom_width)
		BV_VLIST_SET_LINE_WIDTH(vlfree, vhead, 1.0);
	}
    }

    if (!(annot_ip->flags & RT_ANNOT_MODEL_SPACE))
	BV_VLIST_SET_MODEL_MAT(vlfree, vhead);

    (void)V;

    return ret;
}


static void
annot_mesh_free(struct annot_mesh *mesh)
{
    if (!mesh)
	return;
    if (mesh->vertices)
	bu_free(mesh->vertices, "annotation mesh vertices");
    if (mesh->faces)
	bu_free(mesh->faces, "annotation mesh faces");
    if (mesh->thickness)
	bu_free(mesh->thickness, "annotation mesh thickness");
    memset(mesh, 0, sizeof(*mesh));
}


static int
annot_mesh_reserve_vertices(struct annot_mesh *mesh, size_t additional)
{
    size_t capacity;
    size_t required;

    if (additional > SIZE_MAX - mesh->vertex_count)
	return 1;
    required = mesh->vertex_count + additional;
    if (required <= mesh->vertex_capacity)
	return 0;
    capacity = mesh->vertex_capacity ? mesh->vertex_capacity : 64;
    while (capacity < required) {
	if (capacity > SIZE_MAX / 2) {
	    capacity = required;
	    break;
	}
	capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(point_t))
	return 1;
    mesh->vertices = (point_t *)bu_realloc(mesh->vertices,
	capacity * sizeof(point_t), "annotation mesh vertices");
    mesh->vertex_capacity = capacity;
    return 0;
}


static int
annot_mesh_reserve_faces(struct annot_mesh *mesh, size_t additional)
{
    size_t capacity;
    size_t required;

    if (additional > SIZE_MAX - mesh->face_count)
	return 1;
    required = mesh->face_count + additional;
    if (required <= mesh->face_capacity)
	return 0;
    capacity = mesh->face_capacity ? mesh->face_capacity : 96;
    while (capacity < required) {
	if (capacity > SIZE_MAX / 2) {
	    capacity = required;
	    break;
	}
	capacity *= 2;
    }
    if (capacity > SIZE_MAX / (3 * sizeof(int)) ||
	capacity > SIZE_MAX / sizeof(fastf_t))
	return 1;
    mesh->faces = (int *)bu_realloc(mesh->faces,
	capacity * 3 * sizeof(int), "annotation mesh faces");
    mesh->thickness = (fastf_t *)bu_realloc(mesh->thickness,
	capacity * sizeof(fastf_t), "annotation mesh thickness");
    mesh->face_capacity = capacity;
    return 0;
}


static int
annot_mesh_add_triangle(struct annot_mesh *mesh, const point_t a,
	const point_t b, const point_t c, const vect_t normal, fastf_t thickness,
	const struct bn_tol *tol)
{
    point_t ordered[3];
    vect_t ab, ac, bc, face_normal;
    size_t base;

    if (VINVALID(a) || VINVALID(b) || VINVALID(c)) {
	bu_log("annotation mesh contains a non-finite triangle\n");
	return 1;
    }
    VSUB2(ab, b, a);
    VSUB2(ac, c, a);
    VSUB2(bc, c, b);
    VCROSS(face_normal, ab, ac);
    if (INVALID(MAGSQ(ab)) || INVALID(MAGSQ(ac)) || INVALID(MAGSQ(bc)) ||
	INVALID(MAGSQ(face_normal))) {
	bu_log("annotation mesh triangle exceeds the numeric range\n");
	return 1;
    }
    if (MAGNITUDE(face_normal) <= tol->dist_sq)
	return 0;
    VMOVE(ordered[0], a);
    if (VDOT(face_normal, normal) >= 0.0) {
	VMOVE(ordered[1], b);
	VMOVE(ordered[2], c);
    } else {
	VMOVE(ordered[1], c);
	VMOVE(ordered[2], b);
    }

    if (!mesh || mesh->vertex_count > (size_t)INT_MAX - 3 ||
	annot_mesh_reserve_vertices(mesh, 3) ||
	annot_mesh_reserve_faces(mesh, 1))
	return 1;
    base = mesh->vertex_count;
    VMOVE(mesh->vertices[base], ordered[0]);
    VMOVE(mesh->vertices[base + 1], ordered[1]);
    VMOVE(mesh->vertices[base + 2], ordered[2]);
    mesh->vertex_count += 3;
    mesh->faces[3 * mesh->face_count] = (int)base;
    mesh->faces[3 * mesh->face_count + 1] = (int)base + 1;
    mesh->faces[3 * mesh->face_count + 2] = (int)base + 2;
    mesh->thickness[mesh->face_count++] = thickness;
    return 0;
}


static int
annot_mesh_add_stroke_span(struct annot_mesh *mesh, const point_t start,
	const point_t end, const vect_t normal, fastf_t width,
	const struct bn_tol *tol)
{
    point_t corners[4];
    point_t first, last;
    vect_t direction, side;
    fastf_t half_width;
    fastf_t length;

    VSUB2(direction, end, start);
    length = MAGNITUDE(direction);
    if (INVALID(length))
	return 1;
    if (length <= tol->dist)
	return 0;
    VSCALE(direction, direction, 1.0 / length);
    VCROSS(side, normal, direction);
    if (VINVALID(side))
	return 1;
    if (MAGNITUDE(side) <= SMALL_FASTF)
	return 0;
    VUNITIZE(side);

    half_width = 0.5 * width;
    VJOIN1(first, start, -half_width, direction);
    VJOIN1(last, end, half_width, direction);

    VJOIN1(corners[0], first, -half_width, side);
    VJOIN1(corners[1], last, -half_width, side);
    VJOIN1(corners[2], last, half_width, side);
    VJOIN1(corners[3], first, half_width, side);

    if (annot_mesh_add_triangle(mesh, corners[0], corners[1], corners[2],
	    normal, width, tol))
	return 1;
    return annot_mesh_add_triangle(mesh, corners[0], corners[2], corners[3],
	normal, width, tol);
}


static void
annot_line_pattern(uint32_t pattern, const fastf_t **intervals,
	size_t *interval_count)
{
    /* Entries alternate mark and gap lengths in stroke-width units. */
    static const fastf_t dashed[] = {8.0, 4.0};
    static const fastf_t dotted[] = {1.0, 3.0};
    static const fastf_t center[] = {8.0, 3.0, 2.0, 3.0};
    static const fastf_t phantom[] = {8.0, 3.0, 2.0, 3.0, 2.0, 3.0};

    *intervals = NULL;
    *interval_count = 0;
    switch (pattern) {
	case RT_ANNOT_LINE_DASHED:
	    *intervals = dashed;
	    *interval_count = sizeof(dashed) / sizeof(dashed[0]);
	    break;
	case RT_ANNOT_LINE_DOTTED:
	    *intervals = dotted;
	    *interval_count = sizeof(dotted) / sizeof(dotted[0]);
	    break;
	case RT_ANNOT_LINE_CENTER:
	    *intervals = center;
	    *interval_count = sizeof(center) / sizeof(center[0]);
	    break;
	case RT_ANNOT_LINE_PHANTOM:
	    *intervals = phantom;
	    *interval_count = sizeof(phantom) / sizeof(phantom[0]);
	    break;
	default:
	    break;
    }
}


static int
annot_mesh_add_stroke(struct annot_mesh *mesh, const point_t start,
	const point_t end, const vect_t normal, fastf_t width, uint32_t pattern,
	fastf_t *path_distance, const struct bn_tol *tol)
{
    const fastf_t *intervals;
    size_t interval_count;
    vect_t delta;
    fastf_t length;
    fastf_t period = 0.0;
    fastf_t position = 0.0;
    fastf_t phase;
    size_t interval = 0;
    size_t i;

    VSUB2(delta, end, start);
    length = MAGNITUDE(delta);
    if (INVALID(length))
	return 1;
    if (length <= tol->dist)
	return 0;
    annot_line_pattern(pattern, &intervals, &interval_count);
    if (!interval_count) {
	*path_distance += length;
	return annot_mesh_add_stroke_span(mesh, start, end, normal, width, tol);
    }

    for (i = 0; i < interval_count; ++i)
	period += intervals[i] * width;
    phase = fmod(*path_distance, period);
    while (phase >= intervals[interval] * width) {
	phase -= intervals[interval] * width;
	interval = (interval + 1) % interval_count;
    }

    while (position < length) {
	fastf_t available = intervals[interval] * width - phase;
	fastf_t run = available < length - position ? available : length - position;
	if (!(interval & 1) && run > tol->dist) {
	    point_t run_start, run_end;
	    VJOIN1(run_start, start, position / length, delta);
	    VJOIN1(run_end, start, (position + run) / length, delta);
	    if (annot_mesh_add_stroke_span(mesh, run_start, run_end, normal,
		    width, tol))
		return 1;
	}
	position += run;
	phase = 0.0;
	interval = (interval + 1) % interval_count;
    }
    *path_distance += length;
    return 0;
}


static fastf_t
annot_default_width(const struct rt_annot_internal *annot_ip,
	const struct bn_tol *tol)
{
    fastf_t largest_text = 0.0;
    fastf_t local_width = 0.0;
    fastf_t plane_scale;
    point2d_t minimum, maximum;
    vect_t normal;
    size_t i;

    for (i = 0; i < annot_ip->ant.count; ++i) {
	if (annot_ip->ant.segments[i] &&
		*(uint32_t *)annot_ip->ant.segments[i] == ANN_TSEG_MAGIC) {
	    const struct txt_seg *text =
		(const struct txt_seg *)annot_ip->ant.segments[i];
	    if (text->txt_size > largest_text)
		largest_text = text->txt_size;
	}
    }
    if (largest_text > 0.0) {
	local_width = largest_text * ANNOT_TEXT_STROKE_RATIO;
    } else if (annot_ip->vert_count) {
	V2MOVE(minimum, annot_ip->verts[0]);
	V2MOVE(maximum, annot_ip->verts[0]);
	for (i = 1; i < annot_ip->vert_count; ++i) {
	    V_MIN(minimum[X], annot_ip->verts[i][X]);
	    V_MIN(minimum[Y], annot_ip->verts[i][Y]);
	    V_MAX(maximum[X], annot_ip->verts[i][X]);
	    V_MAX(maximum[Y], annot_ip->verts[i][Y]);
	}
	local_width = hypot(maximum[X] - minimum[X],
	    maximum[Y] - minimum[Y]) * ANNOT_GEOMETRY_STROKE_RATIO;
    }

    VCROSS(normal, annot_ip->u_vec, annot_ip->v_vec);
    plane_scale = sqrt(MAGNITUDE(normal));
    local_width *= plane_scale;
    if (local_width < ANNOT_MIN_WIDTH_TOL_FACTOR * tol->dist)
	local_width = ANNOT_MIN_WIDTH_TOL_FACTOR * tol->dist;
    return local_width;
}


static int
annot_segment_mesh(struct annot_mesh *mesh,
	const struct rt_annot_internal *annot_ip, size_t segment,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	const vect_t normal, fastf_t base_width)
{
    struct bu_list vhead;
    struct bu_list vlfree;
    struct bv_vlist *vp;
    const struct rt_annot_seg_style *style = annot_ip->styles ?
	&annot_ip->styles[segment] : NULL;
    uint32_t magic = *(uint32_t *)annot_ip->ant.segments[segment];
    uint32_t pattern = style ? style->line_pattern : RT_ANNOT_LINE_CONTINUOUS;
    fastf_t width = base_width;
    point_t line_start = VINIT_ZERO;
    point_t polygon[3];
    fastf_t path_distance = 0.0;
    size_t polygon_count = 0;
    int have_line_start = 0;
    int ret = 0;

    if (style && (style->flags & RT_ANNOT_STYLE_WIDTH))
	width *= style->line_width;
    else if (style && magic == ANN_TSEG_MAGIC &&
	    (style->flags & RT_ANNOT_STYLE_BOLD))
	width *= 2.0;
    if (!isfinite(width) || width <= 0.0)
	return 1;

    BU_LIST_INIT(&vhead);
    BU_LIST_INIT(&vlfree);
    ret = seg_to_vlist(&vlfree, &vhead, ttol,
	(fastf_t *)annot_ip->V, (struct rt_annot_internal *)annot_ip,
	annot_ip->ant.segments[segment], style);
    if (ret)
	goto cleanup;

    for (BU_LIST_FOR(vp, bv_vlist, &vhead)) {
	size_t i;
	for (i = 0; i < vp->nused; ++i) {
	    switch (vp->cmd[i]) {
		case BV_VLIST_LINE_MOVE:
		    VMOVE(line_start, vp->pt[i]);
		    have_line_start = 1;
		    path_distance = 0.0;
		    break;
		case BV_VLIST_LINE_DRAW:
		    if (have_line_start && annot_mesh_add_stroke(mesh,
			    line_start, vp->pt[i], normal, width, pattern,
			    &path_distance, tol)) {
			ret = 1;
			goto cleanup;
		    }
		    VMOVE(line_start, vp->pt[i]);
		    have_line_start = 1;
		    break;
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_TRI_MOVE:
		    VMOVE(polygon[0], vp->pt[i]);
		    polygon_count = 1;
		    break;
		case BV_VLIST_POLY_DRAW:
		case BV_VLIST_TRI_DRAW:
		    if (polygon_count < 3)
			VMOVE(polygon[polygon_count++], vp->pt[i]);
		    break;
		case BV_VLIST_POLY_END:
		case BV_VLIST_TRI_END:
		    if (polygon_count == 3 && annot_mesh_add_triangle(mesh,
			    polygon[0], polygon[1], polygon[2], normal,
			    base_width, tol)) {
			ret = 1;
			goto cleanup;
		    }
		    polygon_count = 0;
		    break;
		default:
		    break;
	    }
	}
    }

cleanup:
    BV_FREE_VLIST(&vlfree, &vhead);
    bv_vlist_cleanup(&vlfree);
    return ret;
}


static int
annot_to_bot(struct rt_bot_internal **result,
	const struct rt_annot_internal *annot_ip,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	unsigned char bot_mode)
{
    struct annot_mesh mesh = {0};
    struct rt_bot_internal *bot;
    vect_t normal;
    fastf_t base_width;
    size_t i;

    if (!result)
	return 1;
    *result = NULL;

    if (!annot_ip || !ttol || !tol ||
	(bot_mode != RT_BOT_PLATE && bot_mode != RT_BOT_SURFACE) ||
	!(annot_ip->flags & RT_ANNOT_MODEL_SPACE))
	return 1;
    VCROSS(normal, annot_ip->u_vec, annot_ip->v_vec);
    if (VINVALID(normal) || MAGNITUDE(normal) <= SMALL_FASTF)
	return 1;
    VUNITIZE(normal);
    base_width = annot_default_width(annot_ip, tol);
    if (!isfinite(base_width) || base_width <= 0.0)
	return 1;

    for (i = 0; i < annot_ip->ant.count; ++i) {
	if (!annot_ip->ant.segments[i] ||
		annot_is_fill_compatibility_outline(&annot_ip->ant, i))
	    continue;
	if (annot_segment_mesh(&mesh, annot_ip, i, ttol, tol, normal,
		base_width)) {
	    annot_mesh_free(&mesh);
	    return 1;
	}
    }
    if (!mesh.vertex_count || !mesh.face_count) {
	annot_mesh_free(&mesh);
	return 1;
    }

    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = bot_mode;
    bot->orientation = RT_BOT_UNORIENTED;
    bot->num_vertices = mesh.vertex_count;
    bot->vertices = (fastf_t *)mesh.vertices;
    bot->num_faces = mesh.face_count;
    bot->faces = mesh.faces;
    if (bot_mode == RT_BOT_PLATE)
	bot->thickness = mesh.thickness;
    else
	bu_free(mesh.thickness, "annotation mesh thickness");
    mesh.vertices = NULL;
    mesh.faces = NULL;
    mesh.thickness = NULL;
    *result = bot;
    return 0;
}


C_DECL int
rt_annot_tess(struct nmgregion **r, struct model *m,
	struct rt_db_internal *ip, const struct bg_tess_tol *ttol,
	const struct bn_tol *tol)
{
    struct rt_bot_internal *bot = NULL;
    struct rt_db_internal bot_intern;
    const struct rt_annot_internal *annot_ip;
    int ret;

    if (!r || !m || !ip || !ttol || !tol)
	return -1;
    RT_CK_DB_INTERNAL(ip);
    annot_ip = (const struct rt_annot_internal *)ip->idb_ptr;
    if (rt_annot_validate(annot_ip, NULL) ||
	annot_to_bot(&bot, annot_ip, ttol, tol, RT_BOT_SURFACE))
	return -1;

    RT_DB_INTERNAL_INIT(&bot_intern);
    bot_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    bot_intern.idb_type = ID_BOT;
    bot_intern.idb_meth = &OBJ[ID_BOT];
    bot_intern.idb_ptr = bot;
    ret = rt_bot_tess(r, m, &bot_intern, ttol, tol);
    rt_bot_ifree(&bot_intern);
    return ret;
}


C_DECL int
rt_annot_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct rt_annot_internal *annot_ip;
    int ret;
    int myret=0;
    struct bu_list *vlfree = &rt_vlfree;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    annot_ip = (struct rt_annot_internal *)ip->idb_ptr;
    RT_ANNOT_CK_MAGIC(annot_ip);

    ret=ant_to_vlist(vlfree, vhead, ttol, annot_ip->V, annot_ip, &annot_ip->ant);
    if (ret) {
	myret--;
	bu_log("WARNING: Errors in annotation (%d segments reference non-existent vertices)\n",
	       ret);
    }

    return myret;
}

C_DECL int
rt_annot_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip)
{
    if (!rop || !ip || !mat)
	return BRLCAD_OK;

    struct rt_annot_internal *tip = (struct rt_annot_internal *)ip->idb_ptr;
    RT_ANNOT_CK_MAGIC(tip);
    struct rt_annot_internal *top = (struct rt_annot_internal *)rop->idb_ptr;
    RT_ANNOT_CK_MAGIC(top);

    vect_t v;
    VMOVE(v, tip->V);
    MAT4X3PNT(top->V, mat, v);
    if (tip->flags & RT_ANNOT_MODEL_SPACE) {
	VMOVE(v, tip->u_vec);
	MAT4X3VEC(top->u_vec, mat, v);
	VMOVE(v, tip->v_vec);
	MAT4X3VEC(top->v_vec, mat, v);
    }

    return BRLCAD_OK;
}

/**
 * Import an annotation from the database format to the internal format.
 * Apply modeling transformations as well.
 */
C_DECL int
rt_annot_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_annot_internal *annot_ip;
    size_t seg_no;
    unsigned char *ptr;
    struct rt_ant *ant;
    size_t i;

    /* must be double for import and export */
    double v[ELEMENTS_PER_VECT];
    double *vp;

    if (dbip) RT_CK_DBI(dbip);
    BU_CK_EXTERNAL(ep);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_ANNOT;
    ip->idb_meth = &OBJ[ID_ANNOT];
    BU_ALLOC(ip->idb_ptr, struct rt_annot_internal);

    annot_ip = (struct rt_annot_internal *)ip->idb_ptr;
    annot_ip->magic = RT_ANNOT_INTERNAL_MAGIC;

    ptr = ep->ext_buf;
    bu_cv_ntohd((unsigned char *)v, ptr, ELEMENTS_PER_VECT);
    VMOVE(annot_ip->V, v);

    ptr += SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT;
    annot_ip->vert_count = annot_get_uint32(ptr);
    ptr += SIZEOF_NETWORK_LONG;
    annot_ip->ant.count = annot_get_uint32(ptr);
    ptr += SIZEOF_NETWORK_LONG;

    if (annot_ip->vert_count) {
	annot_ip->verts = (point2d_t *)bu_calloc(annot_ip->vert_count, sizeof(point2d_t), "annot_ip->verts");
	vp = (double *)bu_calloc(annot_ip->vert_count, sizeof(double)*ELEMENTS_PER_VECT2D, "vp");
	bu_cv_ntohd((unsigned char *)vp, ptr, annot_ip->vert_count*2);

	/* convert double to fastf_t */
	for (i=0; i<annot_ip->vert_count; i++) {
	    annot_ip->verts[i][X] = vp[(i*ELEMENTS_PER_VECT2D)+0];
	    annot_ip->verts[i][Y] = vp[(i*ELEMENTS_PER_VECT2D)+1];
	}

	bu_free(vp, "vp");
	ptr += SIZEOF_NETWORK_DOUBLE * 2 * annot_ip->vert_count;
    }

    if (annot_ip->ant.count)
	annot_ip->ant.segments = (void **)bu_calloc(annot_ip->ant.count, sizeof(void *), "segs");
    else
	annot_ip->ant.segments = (void **)NULL;
    for (seg_no=0; seg_no < annot_ip->ant.count; seg_no++) {
	uint32_t magic;
	struct line_seg *lsg;
	struct txt_seg *tsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	struct bezier_seg *bsg;


	/* must be double for import and export */
	double scan;
	double *scanp;

	magic = annot_get_uint32(ptr);
	ptr += SIZEOF_NETWORK_LONG;
	switch (magic) {
	    case CURVE_LSEG_MAGIC:
		BU_ALLOC(lsg, struct line_seg);
		lsg->magic = magic;
		lsg->start = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		lsg->end = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		annot_ip->ant.segments[seg_no] = (void *)lsg;
		break;
	    case ANN_TSEG_MAGIC:
		BU_ALLOC(tsg, struct txt_seg);
		tsg->magic = magic;
		tsg->ref_pt = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		tsg->rel_pos = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		bu_vls_init(&tsg->label);
		bu_vls_strcpy(&tsg->label, (const char*)ptr);
		ptr += bu_vls_strlen(&tsg->label) + 1;
		bu_cv_ntohd((unsigned char*)&scan, ptr, 1);
		tsg->txt_size = scan;	/* double to fastf_t */
		ptr += SIZEOF_NETWORK_DOUBLE;
		bu_cv_ntohd((unsigned char*)&scan, ptr, 1);
		tsg->txt_rot_angle = scan;	/* double to fastf_t */
		ptr += SIZEOF_NETWORK_DOUBLE;
		annot_ip->ant.segments[seg_no] = (void *)tsg;
		break;
	    case CURVE_CARC_MAGIC:
		BU_ALLOC(csg, struct carc_seg);
		csg->magic = magic;
		csg->start = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		csg->end = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		csg->orientation = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		csg->center_is_left = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		bu_cv_ntohd((unsigned char *)&scan, ptr, 1);
		csg->radius = scan; /* double to fastf_t */
		ptr += SIZEOF_NETWORK_DOUBLE;
		annot_ip->ant.segments[seg_no] = (void *)csg;
		break;
	    case CURVE_NURB_MAGIC:
		BU_ALLOC(nsg, struct nurb_seg);
		nsg->magic = magic;
		nsg->order = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		nsg->pt_type = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		nsg->k.k_size = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;

		nsg->k.knots = (fastf_t *)bu_malloc(nsg->k.k_size * sizeof(fastf_t), "nsg->k.knots");
		scanp = (double *)bu_malloc(nsg->k.k_size * sizeof(double), "scanp");
		bu_cv_ntohd((unsigned char *)scanp, ptr, nsg->k.k_size);

		/* convert double to fastf_t */
		for (i=0; i<(size_t)nsg->k.k_size; i++) {
		    nsg->k.knots[i] = scanp[i];
		}
		bu_free(scanp, "scanp");

		ptr += SIZEOF_NETWORK_DOUBLE * nsg->k.k_size;
		nsg->c_size = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		nsg->ctl_points = (int *)bu_malloc(nsg->c_size * sizeof(int), "nsg->ctl_points");
		for (i=0; i<(size_t)nsg->c_size; i++) {
		    nsg->ctl_points[i] = annot_get_uint32(ptr);
		    ptr += SIZEOF_NETWORK_LONG;
		}
		if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
		    nsg->weights = (fastf_t *)bu_malloc(nsg->c_size * sizeof(fastf_t), "nsg->weights");
		    scanp = (double *)bu_malloc(nsg->c_size * sizeof(double), "scanp");
		    bu_cv_ntohd((unsigned char *)scanp, ptr, nsg->c_size);

		    /* convert double to fastf_t */
		    for (i=0; i<(size_t)nsg->c_size; i++) {
			nsg->weights[i] = scanp[i];
		    }
		    bu_free(scanp, "scanp");

		    ptr += SIZEOF_NETWORK_DOUBLE * nsg->c_size;
		} else
		    nsg->weights = (fastf_t *)NULL;
		annot_ip->ant.segments[seg_no] = (void *)nsg;
		break;
	    case CURVE_BEZIER_MAGIC:
		BU_ALLOC(bsg, struct bezier_seg);
		bsg->magic = magic;
		bsg->degree = annot_get_uint32(ptr);
		ptr += SIZEOF_NETWORK_LONG;
		bsg->ctl_points = (int *)bu_calloc(bsg->degree+1, sizeof(int), "bsg->ctl_points");
		for (i=0; i<=(size_t)bsg->degree; i++) {
		    bsg->ctl_points[i] = annot_get_uint32(ptr);
		    ptr += SIZEOF_NETWORK_LONG;
		}
		annot_ip->ant.segments[seg_no] = (void *)bsg;
		break;
	    default:
		bu_bomb("rt_annot_import5: ERROR: unrecognized segment type!\n");
		break;
	}
    }

    ant = &annot_ip->ant;

    if (ant->count) {
	ant->reverse = (int *)bu_calloc(ant->count, sizeof(int), "ant->reverse");
    }

    for (i=0; i<ant->count; i++) {
	ant->reverse[i] = annot_get_uint32(ptr);
	ptr += SIZEOF_NETWORK_LONG;
    }

    if ((size_t)((ep->ext_buf + ep->ext_nbytes) - ptr) >=
	    SIZEOF_NETWORK_LONG && annot_get_uint32(ptr) == ANNOT_EXT_MAGIC) {
	const unsigned char *end = ep->ext_buf + ep->ext_nbytes;
	uint32_t magic;
	uint32_t version;
	uint32_t style_count;
	double basis[6];

#define ANNOT_IMPORT_NEED(_n) do { \
	    if ((size_t)(end - ptr) < (size_t)(_n)) goto import_error; \
	} while (0)
#define ANNOT_IMPORT_UINT32(_v) do { \
	    ANNOT_IMPORT_NEED(SIZEOF_NETWORK_LONG); \
	    (_v) = annot_get_uint32(ptr); \
	    ptr += SIZEOF_NETWORK_LONG; \
	} while (0)

	ANNOT_IMPORT_UINT32(magic);
	ANNOT_IMPORT_UINT32(version);
	ANNOT_IMPORT_UINT32(annot_ip->flags);
	if (magic != ANNOT_EXT_MAGIC || version != ANNOT_EXT_VERSION)
	    goto import_error;
	ANNOT_IMPORT_NEED(6 * SIZEOF_NETWORK_DOUBLE);
	bu_cv_ntohd((unsigned char *)basis, ptr, 6);
	ptr += 6 * SIZEOF_NETWORK_DOUBLE;
	VMOVE(annot_ip->u_vec, basis);
	VMOVE(annot_ip->v_vec, &basis[3]);
	ANNOT_IMPORT_UINT32(style_count);
	if (style_count && style_count != ant->count)
	    goto import_error;
	if (style_count)
	    annot_ip->styles = (struct rt_annot_seg_style *)bu_calloc(
		style_count, sizeof(struct rt_annot_seg_style),
		"annotation segment styles");
	for (i = 0; i < style_count; ++i) {
	    struct rt_annot_seg_style *style = &annot_ip->styles[i];
	    uint32_t color;
	    uint32_t font_len;
	    uint32_t symbol_len;
	    double line_width;
	    ANNOT_IMPORT_UINT32(style->role);
	    ANNOT_IMPORT_UINT32(style->flags);
	    ANNOT_IMPORT_UINT32(style->line_pattern);
	    ANNOT_IMPORT_UINT32(color);
	    style->color[0] = (unsigned char)((color >> 24) & 0xff);
	    style->color[1] = (unsigned char)((color >> 16) & 0xff);
	    style->color[2] = (unsigned char)((color >> 8) & 0xff);
	    style->color[3] = (unsigned char)(color & 0xff);
	    ANNOT_IMPORT_NEED(SIZEOF_NETWORK_DOUBLE);
	    bu_cv_ntohd((unsigned char *)&line_width, ptr, 1);
	    ptr += SIZEOF_NETWORK_DOUBLE;
	    style->line_width = line_width;
	    ANNOT_IMPORT_UINT32(font_len);
	    ANNOT_IMPORT_UINT32(symbol_len);
	    ANNOT_IMPORT_NEED((size_t)font_len + symbol_len);
	    if (font_len) {
		style->font = (char *)bu_calloc(font_len + 1, 1,
		    "annotation font");
		memcpy(style->font, ptr, font_len);
		ptr += font_len;
	    }
	    if (symbol_len) {
		style->symbol = (char *)bu_calloc(symbol_len + 1, 1,
		    "annotation symbol");
		memcpy(style->symbol, ptr, symbol_len);
		ptr += symbol_len;
	    }
	}

#undef ANNOT_IMPORT_UINT32
#undef ANNOT_IMPORT_NEED
    }

    /* ANP2 follows the original ANT2 extension.  Main-branch readers stop
     * after ANT2 and therefore continue to load the compatibility outlines
     * and unscaled text. */
    if ((size_t)((ep->ext_buf + ep->ext_nbytes) - ptr) >=
	    SIZEOF_NETWORK_LONG && annot_get_uint32(ptr) == ANNOT_PRES_MAGIC) {
	const unsigned char *end = ep->ext_buf + ep->ext_nbytes;
	uint32_t magic, version, legacy_count, fill_count;
	size_t old_count = ant->count;
	size_t new_count;

#define ANNOT_PRES_NEED(_n) do { \
	    if ((size_t)(end - ptr) < (size_t)(_n)) goto import_error; \
	} while (0)
#define ANNOT_PRES_UINT32(_v) do { \
	    ANNOT_PRES_NEED(SIZEOF_NETWORK_LONG); \
	    (_v) = annot_get_uint32(ptr); \
	    ptr += SIZEOF_NETWORK_LONG; \
	} while (0)

	ANNOT_PRES_UINT32(magic);
	ANNOT_PRES_UINT32(version);
	ANNOT_PRES_UINT32(legacy_count);
	ANNOT_PRES_UINT32(fill_count);
	if (magic != ANNOT_PRES_MAGIC || version != ANNOT_PRES_VERSION ||
		legacy_count != old_count || fill_count > INT_MAX ||
		old_count > SIZE_MAX - fill_count)
	    goto import_error;
	new_count = old_count + fill_count;
	if (!annot_ip->styles && new_count)
	    annot_ip->styles = (struct rt_annot_seg_style *)bu_calloc(
		new_count, sizeof(struct rt_annot_seg_style),
		"annotation presentation styles");
	else if (new_count)
	    annot_ip->styles = (struct rt_annot_seg_style *)bu_realloc(
		annot_ip->styles, new_count * sizeof(struct rt_annot_seg_style),
		"annotation presentation styles");
	if (new_count > old_count)
	    memset(&annot_ip->styles[old_count], 0,
		(new_count - old_count) * sizeof(struct rt_annot_seg_style));
	for (i = 0; i < old_count; ++i) {
	    uint32_t role, extra_flags;
	    double scales[4];
	    ANNOT_PRES_UINT32(role);
	    ANNOT_PRES_UINT32(extra_flags);
	    ANNOT_PRES_NEED(4 * SIZEOF_NETWORK_DOUBLE);
	    bu_cv_ntohd((unsigned char *)scales, ptr, 4);
	    ptr += 4 * SIZEOF_NETWORK_DOUBLE;
	    annot_ip->styles[i].role = role;
	    annot_ip->styles[i].flags |= extra_flags;
	    annot_ip->styles[i].x_scale = scales[0];
	    annot_ip->styles[i].xy_scale = scales[1];
	    annot_ip->styles[i].yx_scale = scales[2];
	    annot_ip->styles[i].y_scale = scales[3];
	}
	if (new_count > old_count) {
	    ant->segments = (void **)bu_realloc(ant->segments,
		new_count * sizeof(void *), "annotation presentation segments");
	    ant->reverse = (int *)bu_realloc(ant->reverse,
		new_count * sizeof(int), "annotation presentation reverse flags");
	    memset(&ant->segments[old_count], 0,
		(new_count - old_count) * sizeof(void *));
	    memset(&ant->reverse[old_count], 0,
		(new_count - old_count) * sizeof(int));
	}
	ant->count = new_count;
	for (i = 0; i < fill_count; ++i) {
	    struct fill_seg *fsg;
	    uint32_t legacy_start, outline_count, loop_count, point_count;
	    size_t j;
	    ANNOT_PRES_UINT32(legacy_start);
	    ANNOT_PRES_UINT32(outline_count);
	    ANNOT_PRES_UINT32(loop_count);
	    ANNOT_PRES_UINT32(point_count);
	    if (!loop_count || loop_count > INT_MAX || point_count > INT_MAX ||
		    legacy_start >= old_count || outline_count > old_count -
		    legacy_start)
		goto import_error;
	    BU_ALLOC(fsg, struct fill_seg);
	    ant->segments[old_count + i] = fsg;
	    fsg->magic = ANN_FSEG_MAGIC;
	    fsg->legacy_start = (int)legacy_start;
	    fsg->legacy_count = (int)outline_count;
	    fsg->loop_count = (int)loop_count;
	    fsg->point_count = (int)point_count;
	    fsg->loop_ends = (int *)bu_calloc(loop_count, sizeof(int),
		"annotation fill loop ends");
	    fsg->points = (int *)bu_calloc(point_count, sizeof(int),
		"annotation fill points");
	    for (j = 0; j < loop_count; ++j) {
		uint32_t value;
		ANNOT_PRES_UINT32(value);
		if (value > INT_MAX) goto import_error;
		fsg->loop_ends[j] = (int)value;
	    }
	    for (j = 0; j < point_count; ++j) {
		uint32_t value;
		ANNOT_PRES_UINT32(value);
		if (value > INT_MAX) goto import_error;
		fsg->points[j] = (int)value;
	    }
	    ant->reverse[old_count + i] = 0;
	    {
		struct rt_annot_seg_style *style =
		    &annot_ip->styles[old_count + i];
		uint32_t color, font_len, symbol_len;
		double values[5];
		ANNOT_PRES_UINT32(style->role);
		ANNOT_PRES_UINT32(style->flags);
		ANNOT_PRES_UINT32(style->line_pattern);
		ANNOT_PRES_UINT32(color);
		ANNOT_PRES_NEED(5 * SIZEOF_NETWORK_DOUBLE);
		bu_cv_ntohd((unsigned char *)values, ptr, 5);
		ptr += 5 * SIZEOF_NETWORK_DOUBLE;
		style->line_width = values[0];
		style->x_scale = values[1];
		style->xy_scale = values[2];
		style->yx_scale = values[3];
		style->y_scale = values[4];
		style->color[0] = (unsigned char)((color >> 24) & 0xff);
		style->color[1] = (unsigned char)((color >> 16) & 0xff);
		style->color[2] = (unsigned char)((color >> 8) & 0xff);
		style->color[3] = (unsigned char)(color & 0xff);
		ANNOT_PRES_UINT32(font_len);
		ANNOT_PRES_UINT32(symbol_len);
		ANNOT_PRES_NEED((size_t)font_len + symbol_len);
		if (font_len) {
		    style->font = (char *)bu_calloc(font_len + 1, 1,
			"annotation fill font");
		    memcpy(style->font, ptr, font_len);
		    ptr += font_len;
		}
		if (symbol_len) {
		    style->symbol = (char *)bu_calloc(symbol_len + 1, 1,
			"annotation fill symbol");
		    memcpy(style->symbol, ptr, symbol_len);
		    ptr += symbol_len;
		}
		style->flags |= RT_ANNOT_STYLE_FILLED;
	    }
	}
#undef ANNOT_PRES_UINT32
#undef ANNOT_PRES_NEED
    }

    if (rt_annot_validate(annot_ip, NULL))
	goto import_error;

    /* Apply transform */
    if (mat == NULL) mat = bn_mat_identity;
    return rt_annot_mat(ip, mat, ip);

import_error:
    rt_annot_ifree(ip);
    return -1;
}


/**
 * The name is added by the caller, in the usual place.
 */
C_DECL int
rt_annot_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_annot_internal *annot_ip;
    unsigned char *cp;
    size_t seg_no;
    size_t i;
    size_t extension_size = 0;
    size_t presentation_size = 0;
    size_t legacy_count = 0;
    size_t fill_count = 0;
    size_t *legacy_map = NULL;
    size_t *memory_to_legacy = NULL;
    int enhanced;
    double coordinate_scale;

    /* must be double for import and export */
    double tmp_vec[ELEMENTS_PER_VECT];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_ANNOT) return -1;
    annot_ip = (struct rt_annot_internal *)ip->idb_ptr;
    RT_ANNOT_CK_MAGIC(annot_ip);

    BU_CK_EXTERNAL(ep);
    if (rt_annot_validate(annot_ip, NULL))
	return -1;

    legacy_map = (size_t *)bu_calloc(annot_ip->ant.count,
	sizeof(size_t), "annotation legacy segment map");
    memory_to_legacy = (size_t *)bu_calloc(annot_ip->ant.count,
	sizeof(size_t), "annotation memory segment map");
    for (seg_no = 0; seg_no < annot_ip->ant.count; ++seg_no) {
	if (*(uint32_t *)annot_ip->ant.segments[seg_no] == ANN_FSEG_MAGIC) {
	    ++fill_count;
	    memory_to_legacy[seg_no] = SIZE_MAX;
	    continue;
	}
	legacy_map[legacy_count] = seg_no;
	memory_to_legacy[seg_no] = legacy_count++;
    }
    enhanced = annot_has_extension(annot_ip);
    coordinate_scale = (annot_ip->flags & RT_ANNOT_MODEL_SPACE) ?
	1.0 : local2mm;

    /* tally up size of buffer needed */
    ep->ext_nbytes =  (ELEMENTS_PER_VECT * SIZEOF_NETWORK_DOUBLE)	/* V*/
	+ 2 * SIZEOF_NETWORK_LONG		/* vert_count and count */
	+ 2 * annot_ip->vert_count * SIZEOF_NETWORK_DOUBLE	/* 2D-vertices */
	+ legacy_count * SIZEOF_NETWORK_LONG;	/* reverse flags */

    for (seg_no=0; seg_no < annot_ip->ant.count; seg_no++) {
	uint32_t *lng;
	struct nurb_seg *nseg;
	struct bezier_seg *bseg;
	struct txt_seg *tseg;

	lng = (uint32_t *)annot_ip->ant.segments[seg_no];
	if (*lng == ANN_FSEG_MAGIC)
	    continue;
	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		/* magic + start + end */
		ep->ext_nbytes += 3 * SIZEOF_NETWORK_LONG;
		break;
	    case ANN_TSEG_MAGIC:
		tseg = (struct txt_seg*)lng;
		/* magic + pt_rel_pos + (double) txt_size + (double) txt_rot_angle + label->vls_str length + 1 for the null terminator */
		ep->ext_nbytes += 3 * SIZEOF_NETWORK_LONG + 2 * SIZEOF_NETWORK_DOUBLE + bu_vls_strlen(&tseg->label) + 1;
		break;
	    case CURVE_CARC_MAGIC:
		/* magic + start + end + orientation + center_is_left + (double)radius */
		ep->ext_nbytes += 5 * SIZEOF_NETWORK_LONG + SIZEOF_NETWORK_DOUBLE;
		break;
	    case CURVE_NURB_MAGIC:
		nseg = (struct nurb_seg *)lng;
		/* magic + order + pt_type + c_size */
		ep->ext_nbytes += 4 * SIZEOF_NETWORK_LONG;
		/* (double)knots */
		ep->ext_nbytes += SIZEOF_NETWORK_LONG + nseg->k.k_size * SIZEOF_NETWORK_DOUBLE;
		/* control point count */
		ep->ext_nbytes += nseg->c_size * SIZEOF_NETWORK_LONG;
		if (RT_NURB_IS_PT_RATIONAL(nseg->pt_type))
		    /* (double)weights */
		    ep->ext_nbytes += nseg->c_size * SIZEOF_NETWORK_DOUBLE;
		break;
	    case CURVE_BEZIER_MAGIC:
		bseg = (struct bezier_seg *)lng;
		/* magic + degree */
		ep->ext_nbytes += 2 * SIZEOF_NETWORK_LONG;
		/* control points */
		ep->ext_nbytes += (bseg->degree + 1) * SIZEOF_NETWORK_LONG;
		break;
	    default:
		bu_log("rt_annot_export5: unsupported segment type (x%x)\n", *lng);
		bu_bomb("rt_annot_export5: unsupported segment type\n");
	}
    }
    if (enhanced) {
	extension_size = 4 * SIZEOF_NETWORK_LONG +
	    6 * SIZEOF_NETWORK_DOUBLE;
	if (annot_ip->styles || fill_count) {
	    for (i = 0; i < legacy_count; ++i) {
		seg_no = legacy_map[i];
		const struct rt_annot_seg_style *style =
		    annot_ip->styles ? &annot_ip->styles[seg_no] : NULL;
		extension_size += 6 * SIZEOF_NETWORK_LONG +
		    SIZEOF_NETWORK_DOUBLE;
		if (style && style->font)
		    extension_size += strlen(style->font);
		if (style && style->symbol)
		    extension_size += strlen(style->symbol);
	    }
	}
    }
    if (fill_count || annot_ip->styles) {
	presentation_size = 4 * SIZEOF_NETWORK_LONG +
	    legacy_count * (2 * SIZEOF_NETWORK_LONG +
		4 * SIZEOF_NETWORK_DOUBLE);
	for (seg_no = 0; seg_no < annot_ip->ant.count; ++seg_no) {
	    const struct fill_seg *fsg;
	    if (*(uint32_t *)annot_ip->ant.segments[seg_no] != ANN_FSEG_MAGIC)
		continue;
	    fsg = (const struct fill_seg *)annot_ip->ant.segments[seg_no];
	    presentation_size += (10 + (size_t)fsg->loop_count +
		(size_t)fsg->point_count) * SIZEOF_NETWORK_LONG +
		5 * SIZEOF_NETWORK_DOUBLE;
	    if (annot_ip->styles) {
		const struct rt_annot_seg_style *style =
		    &annot_ip->styles[seg_no];
		if (style->font) presentation_size += strlen(style->font);
		if (style->symbol) presentation_size += strlen(style->symbol);
	    }
	}
    }
    ep->ext_nbytes += extension_size;
    ep->ext_nbytes += presentation_size;
    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "annotation external");

    cp = (unsigned char *)ep->ext_buf;

    /* scale and export */
    VSCALE(tmp_vec, annot_ip->V, local2mm);
    bu_cv_htond(cp, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT);
    cp += ELEMENTS_PER_VECT * SIZEOF_NETWORK_DOUBLE;


    annot_put_uint32(cp, annot_ip->vert_count);
    cp += SIZEOF_NETWORK_LONG;
    annot_put_uint32(cp, (uint32_t)legacy_count);
    cp += SIZEOF_NETWORK_LONG;

    /* convert 2D points to mm */
    for (i=0; i<annot_ip->vert_count; i++) {
	/* must be double for import and export */
	double pt2d[ELEMENTS_PER_VECT2D];

	V2SCALE(pt2d, annot_ip->verts[i], coordinate_scale);
	bu_cv_htond(cp, (const unsigned char *)pt2d, ELEMENTS_PER_VECT2D);
	cp += 2 * SIZEOF_NETWORK_DOUBLE;
    }

    for (seg_no=0; seg_no < annot_ip->ant.count; seg_no++) {
	struct line_seg *lseg;
	struct txt_seg *tseg;
	struct carc_seg *cseg;
	struct nurb_seg *nseg;
	struct bezier_seg *bseg;
	uint32_t *lng;

	/* must be double for import and export */
	double scan;
	double *scanp;

	/* write segment type ID, and segment parameters */
	lng = (uint32_t *)annot_ip->ant.segments[seg_no];
	if (*lng == ANN_FSEG_MAGIC)
	    continue;
	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lseg = (struct line_seg *)lng;
		annot_put_uint32(cp, CURVE_LSEG_MAGIC);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, lseg->start);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, lseg->end);
		cp += SIZEOF_NETWORK_LONG;
		break;
	    case ANN_TSEG_MAGIC:
		tseg = (struct txt_seg *)lng;
		annot_put_uint32(cp, ANN_TSEG_MAGIC);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, tseg->ref_pt);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, tseg->rel_pos);
		cp += SIZEOF_NETWORK_LONG;

		bu_strlcpy((char *)cp, bu_vls_addr(&tseg->label), bu_vls_strlen(&tseg->label) + 1);

		cp += bu_vls_strlen(&tseg->label) + 1;
		scan = tseg->txt_size;
		bu_cv_htond(cp, (unsigned char*)&scan, 1);
		cp += SIZEOF_NETWORK_DOUBLE;
		scan = tseg->txt_rot_angle;
		bu_cv_htond(cp, (unsigned char*)&scan, 1);
		cp += SIZEOF_NETWORK_DOUBLE;
		break;
	    case CURVE_CARC_MAGIC:
		cseg = (struct carc_seg *)lng;
		annot_put_uint32(cp, CURVE_CARC_MAGIC);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, cseg->start);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, cseg->end);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, cseg->orientation);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, cseg->center_is_left);
		cp += SIZEOF_NETWORK_LONG;
		scan = cseg->radius * coordinate_scale;
		bu_cv_htond(cp, (unsigned char *)&scan, 1);
		cp += SIZEOF_NETWORK_DOUBLE;
		break;
	    case CURVE_NURB_MAGIC:
		nseg = (struct nurb_seg *)lng;
		annot_put_uint32(cp, CURVE_NURB_MAGIC);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, nseg->order);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, nseg->pt_type);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, nseg->k.k_size);
		cp += SIZEOF_NETWORK_LONG;
		scanp = (double *)bu_malloc(nseg->k.k_size * sizeof(double), "scanp");
		/* convert fastf_t to double */
		for (i=0; i<(size_t)nseg->k.k_size; i++) {
		    scanp[i] = nseg->k.knots[i];
		}
		bu_cv_htond(cp, (const unsigned char *)nseg->k.knots, nseg->k.k_size);
		bu_free(scanp, "scanp");
		cp += nseg->k.k_size * SIZEOF_NETWORK_DOUBLE;
		annot_put_uint32(cp, nseg->c_size);
		cp += SIZEOF_NETWORK_LONG;
		for (i=0; i<(size_t)nseg->c_size; i++) {
		    annot_put_uint32(cp, nseg->ctl_points[i]);
		    cp += SIZEOF_NETWORK_LONG;
		}
		if (RT_NURB_IS_PT_RATIONAL(nseg->pt_type)) {
		    scanp = (double *)bu_malloc(nseg->c_size * sizeof(double), "scanp");
		    /* convert fastf_t to double */
		    for (i=0; i<(size_t)nseg->c_size; i++) {
			scanp[i] = nseg->weights[i];
		    }
		    bu_cv_htond(cp, (const unsigned char *)scanp, nseg->c_size);
		    bu_free(scanp, "scanp");
		    cp += SIZEOF_NETWORK_DOUBLE * nseg->c_size;
		}
		break;
	    case CURVE_BEZIER_MAGIC:
		bseg = (struct bezier_seg *)lng;
		annot_put_uint32(cp, CURVE_BEZIER_MAGIC);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, bseg->degree);
		cp += SIZEOF_NETWORK_LONG;
		for (i=0; i<=(size_t)bseg->degree; i++) {
		    annot_put_uint32(cp, bseg->ctl_points[i]);
		    cp += SIZEOF_NETWORK_LONG;
		}
		break;
	    default:
		bu_bomb("rt_annot_export5: ERROR: unrecognized segment type!\n");
		break;

	}
    }

    for (seg_no=0; seg_no < annot_ip->ant.count; seg_no++) {
	if (*(uint32_t *)annot_ip->ant.segments[seg_no] == ANN_FSEG_MAGIC)
	    continue;
	annot_put_uint32(cp, annot_ip->ant.reverse[seg_no]);
	cp += SIZEOF_NETWORK_LONG;
    }

    if (enhanced) {
	double basis[6];
	uint32_t style_count = (annot_ip->styles || fill_count) ?
	    (uint32_t)legacy_count : 0;
	annot_put_uint32(cp, ANNOT_EXT_MAGIC);
	cp += SIZEOF_NETWORK_LONG;
	annot_put_uint32(cp, ANNOT_EXT_VERSION);
	cp += SIZEOF_NETWORK_LONG;
	annot_put_uint32(cp, annot_ip->flags);
	cp += SIZEOF_NETWORK_LONG;
	VSCALE(basis, annot_ip->u_vec, local2mm);
	VSCALE(&basis[3], annot_ip->v_vec, local2mm);
	bu_cv_htond(cp, (unsigned char *)basis, 6);
	cp += 6 * SIZEOF_NETWORK_DOUBLE;
	annot_put_uint32(cp, style_count);
	cp += SIZEOF_NETWORK_LONG;
	for (i = 0; i < style_count; ++i) {
	    const struct rt_annot_seg_style empty_style = {0};
	    seg_no = legacy_map[i];
	    const struct rt_annot_seg_style *style =
		annot_ip->styles ? &annot_ip->styles[seg_no] : &empty_style;
	    uint32_t color = ((uint32_t)style->color[0] << 24) |
		((uint32_t)style->color[1] << 16) |
		((uint32_t)style->color[2] << 8) |
		(uint32_t)style->color[3];
	    size_t font_len = style->font ? strlen(style->font) : 0;
	    size_t symbol_len = style->symbol ? strlen(style->symbol) : 0;
	    double line_width = style->line_width;
	    annot_put_uint32(cp, style->role <= RT_ANNOT_ROLE_SYMBOL ?
		style->role : RT_ANNOT_ROLE_GEOMETRY);
	    cp += SIZEOF_NETWORK_LONG;
	    annot_put_uint32(cp, style->flags &
		(RT_ANNOT_STYLE_WIDTH | RT_ANNOT_STYLE_COLOR));
	    cp += SIZEOF_NETWORK_LONG;
	    annot_put_uint32(cp, style->line_pattern);
	    cp += SIZEOF_NETWORK_LONG;
	    annot_put_uint32(cp, color);
	    cp += SIZEOF_NETWORK_LONG;
	    bu_cv_htond(cp, (unsigned char *)&line_width, 1);
	    cp += SIZEOF_NETWORK_DOUBLE;
	    annot_put_uint32(cp, (uint32_t)font_len);
	    cp += SIZEOF_NETWORK_LONG;
	    annot_put_uint32(cp, (uint32_t)symbol_len);
	    cp += SIZEOF_NETWORK_LONG;
	    if (font_len) {
		memcpy(cp, style->font, font_len);
		cp += font_len;
	    }
	    if (symbol_len) {
		memcpy(cp, style->symbol, symbol_len);
		cp += symbol_len;
	    }
	}
    }

    if (presentation_size) {
	annot_put_uint32(cp, ANNOT_PRES_MAGIC);
	cp += SIZEOF_NETWORK_LONG;
	annot_put_uint32(cp, ANNOT_PRES_VERSION);
	cp += SIZEOF_NETWORK_LONG;
	annot_put_uint32(cp, (uint32_t)legacy_count);
	cp += SIZEOF_NETWORK_LONG;
	annot_put_uint32(cp, (uint32_t)fill_count);
	cp += SIZEOF_NETWORK_LONG;
	for (i = 0; i < legacy_count; ++i) {
	    const struct rt_annot_seg_style *style = annot_ip->styles ?
		&annot_ip->styles[legacy_map[i]] : NULL;
	    double scales[4] = {1.0, 0.0, 0.0, 1.0};
	    uint32_t extra_flags = 0;
	    if (style) {
		extra_flags = style->flags &
		    ~(RT_ANNOT_STYLE_WIDTH | RT_ANNOT_STYLE_COLOR);
		if (style->flags & RT_ANNOT_STYLE_SCALE) {
		    scales[0] = style->x_scale;
		    scales[1] = style->xy_scale;
		    scales[2] = style->yx_scale;
		    scales[3] = style->y_scale;
		}
	    }
	    annot_put_uint32(cp, style ? style->role :
		RT_ANNOT_ROLE_UNSPECIFIED);
	    cp += SIZEOF_NETWORK_LONG;
	    annot_put_uint32(cp, extra_flags);
	    cp += SIZEOF_NETWORK_LONG;
	    bu_cv_htond(cp, (unsigned char *)scales, 4);
	    cp += 4 * SIZEOF_NETWORK_DOUBLE;
	}
	for (seg_no = 0; seg_no < annot_ip->ant.count; ++seg_no) {
	    const struct fill_seg *fsg;
	    size_t j;
	    size_t mapped_start;
	    if (*(uint32_t *)annot_ip->ant.segments[seg_no] != ANN_FSEG_MAGIC)
		continue;
	    fsg = (const struct fill_seg *)annot_ip->ant.segments[seg_no];
	    mapped_start = memory_to_legacy[(size_t)fsg->legacy_start];
	    annot_put_uint32(cp, (uint32_t)mapped_start);
	    cp += SIZEOF_NETWORK_LONG;
	    annot_put_uint32(cp, (uint32_t)fsg->legacy_count);
	    cp += SIZEOF_NETWORK_LONG;
	    annot_put_uint32(cp, (uint32_t)fsg->loop_count);
	    cp += SIZEOF_NETWORK_LONG;
	    annot_put_uint32(cp, (uint32_t)fsg->point_count);
	    cp += SIZEOF_NETWORK_LONG;
	    for (j = 0; j < (size_t)fsg->loop_count; ++j) {
		annot_put_uint32(cp, (uint32_t)fsg->loop_ends[j]);
		cp += SIZEOF_NETWORK_LONG;
	    }
	    for (j = 0; j < (size_t)fsg->point_count; ++j) {
		annot_put_uint32(cp, (uint32_t)fsg->points[j]);
		cp += SIZEOF_NETWORK_LONG;
	    }
	    {
		const struct rt_annot_seg_style empty_style = {0};
		const struct rt_annot_seg_style *style = annot_ip->styles ?
		    &annot_ip->styles[seg_no] : &empty_style;
		const uint32_t color = ((uint32_t)style->color[0] << 24) |
		    ((uint32_t)style->color[1] << 16) |
		    ((uint32_t)style->color[2] << 8) |
		    (uint32_t)style->color[3];
		const size_t font_len = style->font ? strlen(style->font) : 0;
		const size_t symbol_len = style->symbol ? strlen(style->symbol) : 0;
		double values[5] = {style->line_width, style->x_scale,
		    style->xy_scale, style->yx_scale, style->y_scale};
		annot_put_uint32(cp, style->role);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, style->flags | RT_ANNOT_STYLE_FILLED);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, style->line_pattern);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, color);
		cp += SIZEOF_NETWORK_LONG;
		bu_cv_htond(cp, (unsigned char *)values, 5);
		cp += 5 * SIZEOF_NETWORK_DOUBLE;
		annot_put_uint32(cp, (uint32_t)font_len);
		cp += SIZEOF_NETWORK_LONG;
		annot_put_uint32(cp, (uint32_t)symbol_len);
		cp += SIZEOF_NETWORK_LONG;
		if (font_len) {
		    memcpy(cp, style->font, font_len);
		    cp += font_len;
		}
		if (symbol_len) {
		    memcpy(cp, style->symbol, symbol_len);
		    cp += symbol_len;
		}
	    }
	}
    }

    bu_free(memory_to_legacy, "annotation memory segment map");
    bu_free(legacy_map, "annotation legacy segment map");

    return 0;
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
C_DECL int
rt_annot_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_annot_internal *annot_ip =
	(struct rt_annot_internal *)ip->idb_ptr;
    char *rel_pos = NULL;
    size_t i;
    size_t seg_no;
    char buf[256];
    point_t V;

    RT_ANNOT_CK_MAGIC(annot_ip);
    bu_vls_strcat(str, "Annotations (annotation)\n");

    VSCALE(V, annot_ip->V, mm2local);

    sprintf(buf, "\tV = (%g %g %g)\n\t%lu vertices\n",
	    V3INTCLAMPARGS(V),
	    (long unsigned)annot_ip->vert_count);
    bu_vls_strcat(str, buf);

    if (annot_ip->flags & RT_ANNOT_MODEL_SPACE) {
	bu_vls_printf(str, "\tPlacement: model space\n"
	    "\tU-axis = (%g %g %g)\n\tV-axis = (%g %g %g)\n",
	    V3ARGS(annot_ip->u_vec), V3ARGS(annot_ip->v_vec));
    } else {
	bu_vls_strcat(str, "\tPlacement: screen space\n");
    }

    if (!verbose)
	return 0;

    if (annot_ip->vert_count) {
	bu_vls_strcat(str, "\tVertices:\n\t");
	for (i=0; i<annot_ip->vert_count; i++) {
	    sprintf(buf, " %lu-(%g %g)", (long unsigned)i, V2INTCLAMPARGS(annot_ip->verts[i]));
	    bu_vls_strcat(str, buf);
	    if (i && (i+1)%3 == 0)
		bu_vls_strcat(str, "\n\t");
	}
    }
    bu_vls_strcat(str, "\n");

    sprintf(buf, "\n\tAnt:\n");
    bu_vls_strcat(str, buf);
    for (seg_no=0; seg_no < annot_ip->ant.count; seg_no++) {
	struct line_seg *lsg;
	struct txt_seg *tsg;
	struct fill_seg *fsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	struct bezier_seg *bsg;

	lsg = (struct line_seg *)annot_ip->ant.segments[seg_no];
	if (annot_ip->styles) {
	    const struct rt_annot_seg_style *style =
		&annot_ip->styles[seg_no];
	    bu_vls_printf(str, "\t\tStyle: role %u, pattern %u",
		style->role, style->line_pattern);
	    if (style->flags & RT_ANNOT_STYLE_WIDTH)
		bu_vls_printf(str, ", width %g", style->line_width);
	    if (style->flags & RT_ANNOT_STYLE_COLOR)
		bu_vls_printf(str, ", color %u/%u/%u/%u",
		    style->color[0], style->color[1], style->color[2],
		    style->color[3]);
	    if (style->font)
		bu_vls_printf(str, ", font %s", style->font);
	    if (style->symbol)
		bu_vls_printf(str, ", symbol %s", style->symbol);
	    if (style->flags & RT_ANNOT_STYLE_SCALE)
		bu_vls_printf(str, ", transform %g/%g/%g/%g",
		    style->x_scale, style->xy_scale, style->yx_scale,
		    style->y_scale);
	    if (style->flags & RT_ANNOT_STYLE_UNDERLINE)
		bu_vls_strcat(str, ", underline");
	    if (style->flags & RT_ANNOT_STYLE_OVERLINE)
		bu_vls_strcat(str, ", overline");
	    if (style->flags & RT_ANNOT_STYLE_STRIKETHROUGH)
		bu_vls_strcat(str, ", strikethrough");
	    if (style->flags & RT_ANNOT_STYLE_BOLD)
		bu_vls_strcat(str, ", bold");
	    if (style->flags & RT_ANNOT_STYLE_ITALIC)
		bu_vls_strcat(str, ", italic");
	    if (style->flags & RT_ANNOT_STYLE_FILLED)
		bu_vls_strcat(str, ", filled");
	    bu_vls_putc(str, '\n');
	}
	switch (lsg->magic) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)annot_ip->ant.segments[seg_no];
		if ((size_t)lsg->start >= annot_ip->vert_count || (size_t)lsg->end >= annot_ip->vert_count) {
		    if (annot_ip->ant.reverse[seg_no])
			sprintf(buf, "\t\tLine segment from vertex #%d to #%d\n",
				lsg->end, lsg->start);
		    else
			sprintf(buf, "\t\tLine segment from vertex #%d to #%d\n",
				lsg->start, lsg->end);
		} else {
		    if (annot_ip->ant.reverse[seg_no])
			sprintf(buf, "\t\tLine segment (%g %g) <-> (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[lsg->end]),
				V2INTCLAMPARGS(annot_ip->verts[lsg->start]));
		    else
			sprintf(buf, "\t\tLine segment (%g %g) <-> (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[lsg->start]),
				V2INTCLAMPARGS(annot_ip->verts[lsg->end]));
		}
		bu_vls_strcat(str, buf);
		break;
	    case ANN_TSEG_MAGIC:
		tsg = (struct txt_seg *)annot_ip->ant.segments[seg_no];
		if((size_t)tsg->ref_pt >= annot_ip->vert_count) {
		    sprintf(buf, "\t\tReference point vertex #%d\n", tsg->ref_pt);
		}
		else {
		    sprintf(buf, "\t\tReference point (%g %g)\n",
			    V2INTCLAMPARGS(annot_ip->verts[tsg->ref_pt]));
		}
		bu_vls_strcat(str, buf);
		ant_check_pos(tsg, (const char **)&rel_pos);
		sprintf(buf, "\t\tRelative position: %s\n", rel_pos);
		bu_vls_strcat(str, buf);
		/* Annotation text is unbounded (for example, imported multi-line
		 * drawing notes), so it must not pass through the fixed-size
		 * formatting scratch buffer used for the numeric descriptions. */
		bu_vls_printf(str, "\tLabel text: %s\n", bu_vls_addr(&tsg->label));
		sprintf(buf, "\tText size: %.1f\n", tsg->txt_size);
		bu_vls_strcat(str, buf);
		sprintf(buf, "\tText rotation angle: %.1f\n", tsg->txt_rot_angle);
		bu_vls_strcat(str, buf);
		break;
	    case ANN_FSEG_MAGIC:
		fsg = (struct fill_seg *)annot_ip->ant.segments[seg_no];
		bu_vls_printf(str,
		    "\t\tFilled area: %d loop%s, %d indexed points\n",
		    fsg->loop_count, fsg->loop_count == 1 ? "" : "s",
		    fsg->point_count);
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)annot_ip->ant.segments[seg_no];
		if (csg->radius < 0.0) {
		    bu_vls_strcat(str, "\t\tFull Circle:\n");

		    if ((size_t)csg->end >= annot_ip->vert_count || (size_t)csg->start >= annot_ip->vert_count) {
			sprintf(buf, "\t\tcenter at vertex #%d\n",
				csg->end);
			bu_vls_strcat(str, buf);
			sprintf(buf, "\t\tpoint on circle at vertex #%d\n",
				csg->start);
		    } else {
			sprintf(buf, "\t\t\tcenter: (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[csg->end]));
			bu_vls_strcat(str, buf);
			sprintf(buf, "\t\t\tpoint on circle: (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[csg->start]));
		    }
		    bu_vls_strcat(str, buf);
		} else {
		    bu_vls_strcat(str, "\t\tCircular Arc:\n");

		    if ((size_t)csg->end >= annot_ip->vert_count || (size_t)csg->start >= annot_ip->vert_count) {
			sprintf(buf, "\t\t\tstart at vertex #%d\n",
				csg->start);
			bu_vls_strcat(str, buf);
			sprintf(buf, "\t\t\tend at vertex #%d\n",
				csg->end);
			bu_vls_strcat(str, buf);
		    } else {
			sprintf(buf, "\t\t\tstart: (%g, %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[csg->start]));
			bu_vls_strcat(str, buf);
			sprintf(buf, "\t\t\tend: (%g, %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[csg->end]));
			bu_vls_strcat(str, buf);
		    }
		    sprintf(buf, "\t\t\tradius: %g\n", csg->radius*mm2local);
		    bu_vls_strcat(str, buf);
		    if (csg->orientation)
			bu_vls_strcat(str, "\t\t\tcurve is clock-wise\n");
		    else
			bu_vls_strcat(str, "\t\t\tcurve is counter-clock-wise\n");
		    if (csg->center_is_left)
			bu_vls_strcat(str, "\t\t\tcenter of curvature is left of the line from start point to end point\n");
		    else
			bu_vls_strcat(str, "\t\t\tcenter of curvature is right of the line from start point to end point\n");
		    if (annot_ip->ant.reverse[seg_no])
			bu_vls_strcat(str, "\t\t\tarc is reversed\n");
		}
		break;
	    case CURVE_NURB_MAGIC:
		nsg = (struct nurb_seg *)annot_ip->ant.segments[seg_no];
		bu_vls_strcat(str, "\t\tNURB Curve:\n");
		if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
		    sprintf(buf, "\t\t\tCurve is rational\n");
		    bu_vls_strcat(str, buf);
		}
		sprintf(buf, "\t\t\torder = %d, number of control points = %d\n",
			nsg->order, nsg->c_size);
		bu_vls_strcat(str, buf);
		if ((size_t)nsg->ctl_points[0] >= annot_ip->vert_count ||
		    (size_t)nsg->ctl_points[nsg->c_size-1] >= annot_ip->vert_count) {
		    if (annot_ip->ant.reverse[seg_no])
			sprintf(buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
				nsg->ctl_points[nsg->c_size-1],
				nsg->ctl_points[0]);
		    else
			sprintf(buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
				nsg->ctl_points[0],
				nsg->ctl_points[nsg->c_size-1]);
		} else {
		    if (annot_ip->ant.reverse[seg_no])
			sprintf(buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[nsg->ctl_points[nsg->c_size-1]]),
				V2INTCLAMPARGS(annot_ip->verts[nsg->ctl_points[0]]));
		    else
			sprintf(buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[nsg->ctl_points[0]]),
				V2INTCLAMPARGS(annot_ip->verts[nsg->ctl_points[nsg->c_size-1]]));
		}
		bu_vls_strcat(str, buf);
		sprintf(buf, "\t\t\tknot values are %g to %g\n",
			INTCLAMP(nsg->k.knots[0]), INTCLAMP(nsg->k.knots[nsg->k.k_size-1]));
		bu_vls_strcat(str, buf);
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)annot_ip->ant.segments[seg_no];
		bu_vls_strcat(str, "\t\tBezier segment:\n");
		sprintf(buf, "\t\t\tdegree = %d\n", bsg->degree);
		bu_vls_strcat(str, buf);
		if ((size_t)bsg->ctl_points[0] >= annot_ip->vert_count ||
		    (size_t)bsg->ctl_points[bsg->degree] >= annot_ip->vert_count) {
		    if (annot_ip->ant.reverse[seg_no]) {
			sprintf(buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
				bsg->ctl_points[bsg->degree],
				bsg->ctl_points[0]);
		    } else {
			sprintf(buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
				bsg->ctl_points[0],
				bsg->ctl_points[bsg->degree]);
		    }
		} else {
		    if (annot_ip->ant.reverse[seg_no])
			sprintf(buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[bsg->ctl_points[bsg->degree]]),
				V2INTCLAMPARGS(annot_ip->verts[bsg->ctl_points[0]]));
		    else
			sprintf(buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[bsg->ctl_points[0]]),
				V2INTCLAMPARGS(annot_ip->verts[bsg->ctl_points[bsg->degree]]));
		}
		bu_vls_strcat(str, buf);
		break;
	    default:
		bu_bomb("rt_annot_describe: ERROR: unrecognized segment type\n");
	}
    }

    return 0;
}


void
rt_ant_free(struct rt_ant *ant)
{
    size_t i;

    if (ant->count)
	bu_free((char *)ant->reverse, "ant->reverse");
    for (i=0; i<ant->count; i++) {
	uint32_t *lng;
	struct nurb_seg *nsg;
	struct bezier_seg *bsg;
	struct txt_seg *tsg;
	struct fill_seg *fsg;

	lng = (uint32_t *)ant->segments[i];
	if (!lng)
	    continue;
	switch (*lng) {
	    case CURVE_NURB_MAGIC:
		nsg = (struct nurb_seg *)lng;
		bu_free((char *)nsg->ctl_points, "nsg->ctl_points");
		if (nsg->weights)
		    bu_free((char *)nsg->weights, "nsg->weights");
		bu_free((char *)nsg->k.knots, "nsg->k.knots");
		bu_free((char *)lng, "annotation segment");
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
		bu_free((char *)bsg->ctl_points, "bsg->ctl_points");
		bu_free((char *)lng, "annotation segment");
		break;
	    case CURVE_LSEG_MAGIC:
		bu_free((char *)lng, "annotation segment");
		break;
	    case ANN_TSEG_MAGIC:
		tsg = (struct txt_seg *)lng;
		if (BU_VLS_IS_INITIALIZED(&tsg->label))
		    bu_vls_free(&tsg->label);
		bu_free((char *)lng, "annotation segment");
		break;
	    case ANN_FSEG_MAGIC:
		fsg = (struct fill_seg *)lng;
		if (fsg->loop_ends)
		    bu_free(fsg->loop_ends, "annotation fill loop ends");
		if (fsg->points)
		    bu_free(fsg->points, "annotation fill points");
		bu_free((char *)lng, "annotation segment");
		break;
	    case CURVE_CARC_MAGIC:
		bu_free((char *)lng, "annotation segment");
		break;
	    default:
		bu_log("ERROR: rt_annot_free: unrecognized annotation segment type!\n");
		break;
	}
    }

    if (ant->count > 0)
	bu_free((char *)ant->segments, "ant->segments");

    ant->count = 0;
    ant->reverse = (int *)NULL;
    ant->segments = (void **)NULL;
}


static void
annot_styles_free(struct rt_annot_internal *annot_ip)
{
    size_t i;

    if (!annot_ip->styles)
	return;
    for (i = 0; i < annot_ip->ant.count; ++i) {
	if (annot_ip->styles[i].font)
	    bu_free(annot_ip->styles[i].font, "annotation font");
	if (annot_ip->styles[i].symbol)
	    bu_free(annot_ip->styles[i].symbol, "annotation symbol");
    }
    bu_free(annot_ip->styles, "annotation segment styles");
    annot_ip->styles = NULL;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
C_DECL void
rt_annot_ifree(struct rt_db_internal *ip)
{
    struct rt_annot_internal *annot_ip;
    struct rt_ant *ant;

    RT_CK_DB_INTERNAL(ip);

    annot_ip = (struct rt_annot_internal *)ip->idb_ptr;
    RT_ANNOT_CK_MAGIC(annot_ip);
    annot_ip->magic = 0;			/* sanity */

    if (annot_ip->verts)
	bu_free((char *)annot_ip->verts, "annot_ip->verts");

    ant = &annot_ip->ant;
    annot_styles_free(annot_ip);

    rt_ant_free(ant);

    bu_free((char *)annot_ip, "annotation ifree");
    ip->idb_ptr = ((void *)0);	/* sanity */
}


static void
ant_copy(struct rt_ant *ant_out, const struct rt_ant *ant_in)
{
    size_t i, j;

    ant_out->count = ant_in->count;
    if (ant_out->count) {
	ant_out->reverse = (int *)bu_calloc(ant_out->count, sizeof(int), "ant->reverse");
	ant_out->segments = (void **)bu_calloc(ant_out->count, sizeof(void *), "ant->segments");
    }

    for (j=0; j<ant_out->count; j++) {
	uint32_t *lng;
	struct line_seg *lsg_out, *lsg_in;
	struct txt_seg *tsg_out, *tsg_in;
	struct fill_seg *fsg_out, *fsg_in;
	struct carc_seg *csg_out, *csg_in;
	struct nurb_seg *nsg_out, *nsg_in;
	struct bezier_seg *bsg_out, *bsg_in;

	ant_out->reverse[j] = ant_in->reverse[j];
	lng = (uint32_t *)ant_in->segments[j];
	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lsg_in = (struct line_seg *)lng;
		BU_ALLOC(lsg_out, struct line_seg);
		ant_out->segments[j] = (void *)lsg_out;
		*lsg_out = *lsg_in;
		break;
	    case ANN_TSEG_MAGIC:
		tsg_in = (struct txt_seg *)lng;
		BU_ALLOC(tsg_out, struct txt_seg);
		ant_out->segments[j] = (void *)tsg_out;
		*tsg_out = *tsg_in;
		/* The struct-copy above is a shallow copy; re-init the VLS
		 * so tsg_out owns its own label string and does not alias
		 * tsg_in->label.vls_str. */
		bu_vls_init(&tsg_out->label);
		bu_vls_strcpy(&tsg_out->label, bu_vls_cstr(&tsg_in->label));
		break;
	    case ANN_FSEG_MAGIC:
		fsg_in = (struct fill_seg *)lng;
		BU_ALLOC(fsg_out, struct fill_seg);
		ant_out->segments[j] = (void *)fsg_out;
		*fsg_out = *fsg_in;
		fsg_out->loop_ends = (int *)bu_calloc(
		    (size_t)fsg_in->loop_count, sizeof(int),
		    "annotation fill loop ends");
		fsg_out->points = (int *)bu_calloc(
		    (size_t)fsg_in->point_count, sizeof(int),
		    "annotation fill points");
		memcpy(fsg_out->loop_ends, fsg_in->loop_ends,
		    (size_t)fsg_in->loop_count * sizeof(int));
		memcpy(fsg_out->points, fsg_in->points,
		    (size_t)fsg_in->point_count * sizeof(int));
		break;
	    case CURVE_CARC_MAGIC:
		csg_in = (struct carc_seg *)lng;
		BU_ALLOC(csg_out, struct carc_seg);
		ant_out->segments[j] = (void *)csg_out;
		*csg_out = *csg_in;
		break;
	    case CURVE_NURB_MAGIC:
		nsg_in = (struct nurb_seg *)lng;
		BU_ALLOC(nsg_out, struct nurb_seg);
		ant_out->segments[j] = (void *)nsg_out;
		*nsg_out = *nsg_in;
		nsg_out->ctl_points = (int *)bu_calloc(nsg_in->c_size, sizeof(int), "nsg_out->ctl_points");
		for (i=0; i<(size_t)nsg_out->c_size; i++)
		    nsg_out->ctl_points[i] = nsg_in->ctl_points[i];
		if (RT_NURB_IS_PT_RATIONAL(nsg_in->pt_type)) {
		    nsg_out->weights = (fastf_t *)bu_malloc(nsg_out->c_size * sizeof(fastf_t), "nsg_out->weights");
		    for (i=0; i<(size_t)nsg_out->c_size; i++)
			nsg_out->weights[i] = nsg_in->weights[i];
		} else
		    nsg_out->weights = (fastf_t *)NULL;
		nsg_out->k.knots = (fastf_t *)bu_malloc(nsg_in->k.k_size * sizeof(fastf_t), "nsg_out->k.knots");
		for (i=0; i<(size_t)nsg_in->k.k_size; i++)
		    nsg_out->k.knots[i] = nsg_in->k.knots[i];
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg_in = (struct bezier_seg *)lng;
		BU_ALLOC(bsg_out, struct bezier_seg);
		ant_out->segments[j] = (void *)bsg_out;
		*bsg_out = *bsg_in;
		bsg_out->ctl_points = (int *)bu_calloc(bsg_out->degree + 1,
						       sizeof(int), "bsg_out->ctl_points");
		for (i=0; i<=(size_t)bsg_out->degree; i++) {
		    bsg_out->ctl_points[i] = bsg_in->ctl_points[i];
		}
		break;
	    default:
		bu_bomb("ERROR: unrecognized segment type encountered while copying annotation\n");
	}
    }

}


struct rt_annot_internal *
rt_copy_annot(const struct rt_annot_internal *annot_ip)
{
    struct rt_annot_internal *out;
    size_t i;
    struct rt_ant *ant_out;

    RT_ANNOT_CK_MAGIC(annot_ip);

    BU_ALLOC(out, struct rt_annot_internal);
    *out = *annot_ip;	/* struct copy */

    out->styles = NULL;
    if (annot_ip->styles && annot_ip->ant.count) {
	out->styles = (struct rt_annot_seg_style *)bu_calloc(
	    annot_ip->ant.count, sizeof(struct rt_annot_seg_style),
	    "annotation segment styles");
	for (i = 0; i < annot_ip->ant.count; ++i) {
	    out->styles[i] = annot_ip->styles[i];
	    out->styles[i].font = annot_ip->styles[i].font ?
		bu_strdup(annot_ip->styles[i].font) : NULL;
	    out->styles[i].symbol = annot_ip->styles[i].symbol ?
		bu_strdup(annot_ip->styles[i].symbol) : NULL;
	}
    }

    if (out->vert_count)
	out->verts = (point2d_t *)bu_calloc(out->vert_count, sizeof(point2d_t), "out->verts");

    for (i=0; annot_ip->verts && i<out->vert_count; i++) {
	V2MOVE(out->verts[i], annot_ip->verts[i]);
    }

    ant_out = &out->ant;
    if (ant_out)
	ant_copy(ant_out, &annot_ip->ant);

    return out;
}


static int
ant_to_tcl_list(struct bu_vls *vls, struct rt_ant *ant)
{
    size_t i, j;

    bu_vls_printf(vls, " SL {");
    for (j=0; j<ant->count; j++) {
	switch ((*(uint32_t *)ant->segments[j])) {
	    case CURVE_LSEG_MAGIC:
		{
		    struct line_seg *lsg = (struct line_seg *)ant->segments[j];
		    bu_vls_printf(vls, " { line S %d E %d }", lsg->start, lsg->end);
		}
		break;
	    case ANN_TSEG_MAGIC:
		{
		    struct txt_seg *tsg = (struct txt_seg *)ant->segments[j];
		    bu_vls_printf(vls, " { txt R %d P %d L {%s} S %.25g A %.25g }", tsg->ref_pt, tsg->rel_pos, bu_vls_addr(&tsg->label), tsg->txt_size, tsg->txt_rot_angle);
		}
		break;
	    case ANN_FSEG_MAGIC:
		{
		    struct fill_seg *fsg = (struct fill_seg *)ant->segments[j];
		    bu_vls_printf(vls, " { fill E {");
		    for (i = 0; i < (size_t)fsg->loop_count; ++i)
			bu_vls_printf(vls, " %d", fsg->loop_ends[i]);
		    bu_vls_strcat(vls, " } P {");
		    for (i = 0; i < (size_t)fsg->point_count; ++i)
			bu_vls_printf(vls, " %d", fsg->points[i]);
		    bu_vls_printf(vls, " } S %d C %d }", fsg->legacy_start,
			fsg->legacy_count);
		}
		break;
	    case CURVE_CARC_MAGIC:
		{
		    struct carc_seg *csg = (struct carc_seg *)ant->segments[j];
		    bu_vls_printf(vls, " { carc S %d E %d R %.25g L %d O %d }",
				  csg->start, csg->end, csg->radius,
				  csg->center_is_left, csg->orientation);
		}
		break;
	    case CURVE_BEZIER_MAGIC:
		{
		    struct bezier_seg *bsg = (struct bezier_seg *)ant->segments[j];
		    bu_vls_printf(vls, " { bezier D %d P {", bsg->degree);
		    for (i=0; i<=(size_t)bsg->degree; i++)
			bu_vls_printf(vls, " %d", bsg->ctl_points[i]);
		    bu_vls_printf(vls, " } }");
		}
		break;
	    case CURVE_NURB_MAGIC:
		{
		    size_t k;
		    struct nurb_seg *nsg = (struct nurb_seg *)ant->segments[j];
		    bu_vls_printf(vls, " { nurb O %d T %d K {",
				  nsg->order, nsg->pt_type);
		    for (k=0; k<(size_t)nsg->k.k_size; k++)
			bu_vls_printf(vls, " %.25g", nsg->k.knots[k]);
		    bu_vls_strcat(vls, "} P {");
		    for (k=0; k<(size_t)nsg->c_size; k++)
			bu_vls_printf(vls, " %d", nsg->ctl_points[k]);
		    if (nsg->weights) {
			bu_vls_strcat(vls, "} W {");
			for (k=0; k<(size_t)nsg->c_size; k++)
			    bu_vls_printf(vls, " %.25g", nsg->weights[k]);
		    }
		    bu_vls_strcat(vls, "} }");
		}
		break;
	}
    }
    bu_vls_strcat(vls, " }");	/* end of segment list */

    return 0;
}


C_DECL int
rt_annot_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    BU_CK_VLS(logstr);
    RT_CK_FUNCTAB(ftp);

    bu_vls_printf(logstr, "V {%%f %%f %%f} mode {screen|model} "
	"A {%%f %%f %%f} B {%%f %%f %%f} "
	"VL {{%%f %%f} {%%f %%f} ...} SL {{segment_data} {segment_data}}");

    return BRLCAD_OK;
}


C_DECL int
rt_annot_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct rt_annot_internal *ann=(struct rt_annot_internal *)intern->idb_ptr;
    size_t i;
    struct rt_ant *ant;

    BU_CK_VLS(logstr);
    RT_ANNOT_CK_MAGIC(ann);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "annot");
	bu_vls_printf(logstr, " V {%.25g %.25g %.25g}", V3ARGS(ann->V));
	bu_vls_printf(logstr, " mode %s", (ann->flags & RT_ANNOT_MODEL_SPACE) ?
	    "model" : "screen");
	bu_vls_printf(logstr, " A {%.25g %.25g %.25g}", V3ARGS(ann->u_vec));
	bu_vls_printf(logstr, " B {%.25g %.25g %.25g}", V3ARGS(ann->v_vec));
	bu_vls_strcat(logstr, " VL {");
	for (i=0; i<ann->vert_count; i++)
	    bu_vls_printf(logstr, " {%.25g %.25g}", V2ARGS(ann->verts[i]));
	bu_vls_strcat(logstr, " }");

	ant = &ann->ant;
	if (ant_to_tcl_list(logstr, ant)) {
	    return BRLCAD_ERROR;
	}
    } else if (BU_STR_EQUAL(attr, "V")) {
	bu_vls_printf(logstr, "%.25g %.25g %.25g", V3ARGS(ann->V));
    } else if (BU_STR_EQUAL(attr, "mode")) {
	bu_vls_strcat(logstr, (ann->flags & RT_ANNOT_MODEL_SPACE) ?
	    "model" : "screen");
    } else if (BU_STR_EQUAL(attr, "A")) {
	bu_vls_printf(logstr, "%.25g %.25g %.25g", V3ARGS(ann->u_vec));
    } else if (BU_STR_EQUAL(attr, "B")) {
	bu_vls_printf(logstr, "%.25g %.25g %.25g", V3ARGS(ann->v_vec));
    } else if (BU_STR_EQUAL(attr, "VL")) {
	for (i=0; i<ann->vert_count; i++)
	    bu_vls_printf(logstr, " {%.25g %.25g}", V2ARGS(ann->verts[i]));
    } else if (BU_STR_EQUAL(attr, "SL")) {
	ant = &ann->ant;
	if (ant_to_tcl_list(logstr, ant)) {
	    return BRLCAD_ERROR;
	}
    } else if (*attr == 'V') {
	long lval = atol((attr+1));
	if (lval < 0 || (size_t)lval >= ann->vert_count) {
	    bu_vls_printf(logstr, "ERROR: Illegal vertex number\n");
	    return BRLCAD_ERROR;
	}
	bu_vls_printf(logstr, "%.25g %.25g", V2ARGS(ann->verts[lval]));
    } else {
	/* unrecognized attribute */
	bu_vls_printf(logstr, "ERROR: Unknown attribute, choices are V, mode, A, B, VL, SL, or V#\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
ant_get_tcl(struct bu_vls *logstr, struct rt_ant *ant, const char *argv1)
{
    int count;
    int j;
    const char **seg_list = NULL;

    /* split initial list */
    if (bu_argv_from_tcl_list(argv1, &count, (const char ***)&seg_list) != 0) {
	return -1;
    }

    if (count) {
	ant->count = count;
	ant->reverse = (int *)bu_calloc(count, sizeof(int), "ant->reverse");
	ant->segments = (void **)bu_calloc(count, sizeof(void *), "ant->segments");
    }

    /* loop through all the segments */
    for (j = 0; j < count; j++) {
	int seg_argc;
	const char **seg_argv = NULL;
	const char *elem, *sval;
	int k;

	/* get the next segment */
	if (bu_argv_from_tcl_list(seg_list[j], &seg_argc, (const char ***)&seg_argv) != 0) {
	    bu_free((char *)seg_list, "free seg list");
	    return -1;
	}

	if (seg_argc < 1) {
	    bu_free((char *)seg_argv, "free seg argv");
	    bu_free((char *)seg_list, "free seg list");
	    return 0;
	}

	/* get the next segment */
	if (BU_STR_EQUAL(seg_argv[0], "line")) {
	    struct line_seg *lsg;

	    BU_ALLOC(lsg, struct line_seg);
	    for (k=1; k<seg_argc; k += 2) {
		elem = seg_argv[k];
		sval = seg_argv[k+1];
		switch (*elem) {
		    case 'S':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&lsg->start);
			break;
		    case 'E':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&lsg->end);
			break;
		}
	    }
	    lsg->magic = CURVE_LSEG_MAGIC;
	    ant->segments[j] = (void *)lsg;
	} else if (BU_STR_EQUAL(seg_argv[0], "txt")) {
	    struct txt_seg *tsg;

	    BU_ALLOC(tsg, struct txt_seg);
	    bu_vls_init(&tsg->label);
	    for (k=1; k<seg_argc; k+= 2) {
		elem = seg_argv[k];
		sval = seg_argv[k+1];
		switch (*elem) {
		    case 'R': /* ref point */
			(void)bu_opt_int(NULL, 1, &sval, (void *)&tsg->ref_pt);
			break;
		    case 'P': /* position relative */
			(void)bu_opt_int(NULL, 1, &sval, (void *)&tsg->rel_pos);
			break;
		    case 'L': /* label text */
			(void)bu_opt_vls(NULL, 1, &sval, (void *)&tsg->label);
			break;
		    case 'S': /* text size */
			(void)bu_opt_fastf_t(NULL, 1, &sval, (void *)&tsg->txt_size);
			break;
		    case 'A': /* text rotation angle */
			(void)bu_opt_fastf_t(NULL, 1, &sval, (void *)&tsg->txt_rot_angle);
			break;
		}
	    }
	    tsg->magic = ANN_TSEG_MAGIC;
	    ant->segments[j] = (void *)tsg;
	} else if (BU_STR_EQUAL(seg_argv[0], "fill")) {
	    struct fill_seg *fsg;

	    BU_ALLOC(fsg, struct fill_seg);
	    fsg->legacy_start = -1;
	    for (k = 1; k < seg_argc; k += 2) {
		elem = seg_argv[k];
		sval = seg_argv[k + 1];
		switch (*elem) {
		    case 'E':
			(void)_rt_tcl_list_to_int_array(sval,
			    &fsg->loop_ends, &fsg->loop_count);
			break;
		    case 'P':
			(void)_rt_tcl_list_to_int_array(sval,
			    &fsg->points, &fsg->point_count);
			break;
		    case 'S':
			(void)bu_opt_int(NULL, 1, &sval,
			    (void *)&fsg->legacy_start);
			break;
		    case 'C':
			(void)bu_opt_int(NULL, 1, &sval,
			    (void *)&fsg->legacy_count);
			break;
		}
	    }
	    fsg->magic = ANN_FSEG_MAGIC;
	    ant->segments[j] = (void *)fsg;
	} else if (BU_STR_EQUAL(seg_argv[0], "bezier")) {
	    struct bezier_seg *bsg;
	    int num_points;

	    BU_ALLOC(bsg, struct bezier_seg);
	    for (k=1; k<seg_argc; k+= 2) {
		elem = seg_argv[k];
		sval = seg_argv[k+1];
		switch (*elem) {
		    case 'D': /* degree */
			(void)bu_opt_int(NULL, 1, &sval, (void *)&bsg->degree);
			break;
		    case 'P': /* list of control points */
			num_points = 0;
			(void)_rt_tcl_list_to_int_array(sval, &bsg->ctl_points, &num_points);
			if (num_points != bsg->degree + 1) {
			    bu_vls_printf(logstr, "ERROR: degree and number of control points disagree for a Bezier segment\n");
			    if (bsg->ctl_points)
				bu_free((char *)bsg->ctl_points, "bsg->ctl_points");
			    bu_free((char *)bsg, "bsg");
			    bu_free((char *)seg_argv, "free seg argv");
			    bu_free((char *)seg_list, "free seg list");
			    return 1;
			}
		}
	    }
	    bsg->magic = CURVE_BEZIER_MAGIC;
	    ant->segments[j] = (void *)bsg;
	} else if (BU_STR_EQUAL(seg_argv[0], "carc")) {
	    struct carc_seg *csg;

	    BU_ALLOC(csg, struct carc_seg);
	    for (k=1; k<seg_argc; k += 2) {
		elem = seg_argv[k];
		sval = seg_argv[k+1];
		switch (*elem) {
		    case 'S':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&csg->start);
			break;
		    case 'E':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&csg->end);
			break;
		    case 'R':
			(void)bu_opt_fastf_t(NULL, 1, &sval, (void *)&csg->radius);
			break;
		    case 'L' :
			(void)bu_opt_bool(NULL, 1, &sval, (void *)&csg->center_is_left);
			break;
		    case 'O':
			(void)bu_opt_bool(NULL, 1, &sval, (void *)&csg->orientation);
			break;
		}
	    }
	    csg->magic = CURVE_CARC_MAGIC;
	    ant->segments[j] = (void *)csg;
	} else if (BU_STR_EQUAL(seg_argv[0], "nurb")) {
	    struct nurb_seg *nsg;

	    BU_ALLOC(nsg, struct nurb_seg);
	    for (k=1; k<seg_argc; k += 2) {
		elem = seg_argv[k];
		sval = seg_argv[k+1];
		switch (*elem) {
		    case 'O':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&nsg->order);
			break;
		    case 'T':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&nsg->pt_type);
			break;
		    case 'K':
			(void)_rt_tcl_list_to_fastf_array(sval, &nsg->k.knots, &nsg->k.k_size);
			break;
		    case 'P' :
			(void)_rt_tcl_list_to_int_array(sval, &nsg->ctl_points, &nsg->c_size);
			break;
		    case 'W':
			{
			    /* Use a local length: c_size may already be set by
			     * the 'P' case and the func only allocates when 
			     * the last value is '0'. We intentionally 
			     * throw away the value as it is == nsg->c_size */
			    int wlen = 0;
			    (void)_rt_tcl_list_to_fastf_array(sval, &nsg->weights, &wlen);
			}
			break;
		}
	    }
	    nsg->magic = CURVE_NURB_MAGIC;
	    ant->segments[j] = (void *)nsg;
	} else {
	    bu_vls_sprintf(logstr, "ERROR: Unrecognized segment type: %s\n", seg_argv[0]);
	    bu_free((char *)seg_argv, "free seg argv");
	    bu_free((char *)seg_list, "free seg argv");
	    return 1;
	}

	bu_free((char *)seg_argv, "free seg argv");
    }

    bu_free((char *)seg_list, "free seg argv");

    return 0;
}


C_DECL int
rt_annot_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_annot_internal *annot_ip;
    int ret, array_len;
    fastf_t *newval;

    RT_CK_DB_INTERNAL(intern);
    annot_ip = (struct rt_annot_internal *)intern->idb_ptr;
    RT_ANNOT_CK_MAGIC(annot_ip);

    while (argc >= 2) {
	if (BU_STR_EQUAL(argv[0], "V")) {
	    newval = annot_ip->V;
	    array_len = 3;
	    if (_rt_tcl_list_to_fastf_array(argv[1], &newval, &array_len) !=
		array_len) {
		bu_vls_printf(logstr, "ERROR: Incorrect number of coordinates for vertex\n");
		return BRLCAD_ERROR;
	    }
	} else if (BU_STR_EQUAL(argv[0], "mode")) {
	    if (BU_STR_EQUAL(argv[1], "model")) {
		annot_ip->flags |= RT_ANNOT_MODEL_SPACE;
		if (MAGNITUDE(annot_ip->u_vec) <= SMALL_FASTF)
		    VSET(annot_ip->u_vec, 1.0, 0.0, 0.0);
		if (MAGNITUDE(annot_ip->v_vec) <= SMALL_FASTF)
		    VSET(annot_ip->v_vec, 0.0, 1.0, 0.0);
	    } else if (BU_STR_EQUAL(argv[1], "screen")) {
		annot_ip->flags &= ~RT_ANNOT_MODEL_SPACE;
	    } else {
		bu_vls_printf(logstr, "ERROR: mode must be screen or model\n");
		return BRLCAD_ERROR;
	    }
	} else if (BU_STR_EQUAL(argv[0], "A") ||
		BU_STR_EQUAL(argv[0], "B")) {
	    newval = BU_STR_EQUAL(argv[0], "A") ?
		annot_ip->u_vec : annot_ip->v_vec;
	    array_len = 3;
	    if (_rt_tcl_list_to_fastf_array(argv[1], &newval, &array_len) !=
		    array_len) {
		bu_vls_printf(logstr, "ERROR: Incorrect number of basis coordinates\n");
		return BRLCAD_ERROR;
	    }
	} else if (BU_STR_EQUAL(argv[0], "VL")) {
	    fastf_t *new_verts=(fastf_t *)NULL;
	    int len;
	    char *ptr;
	    char *dupstr;

	    /* the vertex list is a list of lists (each element is a
	     * list of two coordinates) so eliminate all the '{' and
	     * '}' chars in the list
	     */
	    dupstr = bu_strdup(argv[1]);

	    ptr = dupstr;
	    while (*ptr != '\0') {
		if (*ptr == '{' || *ptr == '}')
		    *ptr = ' ';
		ptr++;
	    }

	    len = 0;
	    (void)_rt_tcl_list_to_fastf_array(dupstr, &new_verts, &len);
	    bu_free(dupstr, "annotation adjust strdup");

	    if (len%2) {
		bu_vls_printf(logstr, "ERROR: Incorrect number of coordinates for vertices\n");
		return BRLCAD_ERROR;
	    }

	    if (annot_ip->verts)
		bu_free((char *)annot_ip->verts, "verts");
	    annot_ip->verts = (point2d_t *)new_verts;
	    annot_ip->vert_count = len / 2;
	} else if (BU_STR_EQUAL(argv[0], "SL")) {
	    /* the entire segment list */
	    struct rt_ant *ant;

	    ant = &annot_ip->ant;
	    /* free any previously-populated segment list before rebuilding
	     * (rt_ant_free is a safe no-op when count == 0) */
	    annot_styles_free(annot_ip);
	    rt_ant_free(ant);

	    if ((ret=ant_get_tcl(logstr, ant, argv[1])) != 0)
		return ret;
	} else if (*argv[0] == 'V' && isdigit((int)*(argv[0]+1))) {
	    /* changing a specific vertex */
	    long vert_no;
	    fastf_t *new_vert;

	    vert_no = atol(argv[0] + 1);
	    new_vert = annot_ip->verts[vert_no];
	    if (vert_no < 0 || (size_t)vert_no > annot_ip->vert_count) {
		bu_vls_printf(logstr, "ERROR: Illegal vertex number\n");
		return BRLCAD_ERROR;
	    }
	    array_len = 2;
	    if (_rt_tcl_list_to_fastf_array(argv[1], &new_vert, &array_len) != array_len) {
		bu_vls_printf(logstr, "ERROR: Incorrect number of coordinates for vertex\n");
		return BRLCAD_ERROR;
	    }
	}

	argc -= 2;
	argv += 2;
    }

    return BRLCAD_OK;
}


C_DECL int
rt_annot_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}

C_DECL const char *
rt_annot_keypoint(point_t *pt, const char *keystr, const mat_t mat, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    if (!pt || !ip)
	return NULL;

    point_t mpt = VINIT_ZERO;
    struct rt_annot_internal *annot = (struct rt_annot_internal *)ip->idb_ptr;
    RT_ANNOT_CK_MAGIC(annot);

    static const char *default_keystr = "V";
    const char *k = (keystr) ? keystr : default_keystr;

    if (BU_STR_EQUAL(k, default_keystr)) {
	VMOVE(mpt, annot->V);
	goto annot_kpt_end;
    }

    // No keystr matches - failed
    return NULL;

annot_kpt_end:

    MAT4X3PNT(*pt, mat, mpt);

    return k;
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
