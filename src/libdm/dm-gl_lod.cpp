/*                    D M - G L _ L O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 1988-2026 United States Government as represented by
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
/** @file libdm/dm-gl_lod.cpp
 *
 * OpenGL logic for rendering LoD structures.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "vmath.h"
#include "bu.h"
#include "bn.h"
extern "C" {
#include "bv/defines.h"
#include "bv/lod.h"
#include "dm.h"
#include "./dm-gl.h"
#include "./include/private.h"
}

struct swrast_vars_fast {
    struct bview *v;
    void *ctx;
    void *os_b;
};

class gl_texture_binding {
public:
    gl_texture_binding(struct dm *dmp, struct bv_scene_obj *s) : dm(dmp)
    {
	const struct bv_texture *image = s->s_texture;
	int mode = s->s_os->s_dmode;
	if (!image || !image->rgb || (mode != 1 && mode != 2) ||
	    s->s_iflag == UP || s->s_os->color_override)
	    return;

	// Keep texture binding outside geometry display lists so that image
	// edits only require uploading the replacement image.
	glPushAttrib(GL_TEXTURE_BIT | GL_CURRENT_BIT);
	if (!gl_bind_texture(dmp, image)) {
	    glPopAttrib();
	    return;
	}

	bound = true;
	opacity = ((struct gl_vars *)dmp->i->m_vars)->i.wireColor[3];
	const unsigned char *fg = dm_get_fg(dmp);
	VMOVE(saved_fg, fg);
	const unsigned char white = 255;
	dm_set_fg(dmp, white, white, white, 0, opacity);
    }

    ~gl_texture_binding()
    {
	if (bound) {
	    dm_set_fg(dm, saved_fg[0], saved_fg[1], saved_fg[2], 0, opacity);
	    glPopAttrib();
	}
    }

private:
    struct dm *dm;
    unsigned char saved_fg[3];
    fastf_t opacity = 1.0;
    bool bound = false;
};

static int
gl_swrast_database_wireframe(struct dm *dmp, struct bv_scene_obj *s)
{
    if (!dmp || !s)
	return 0;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    if (!mvars || !mvars->fast_wireframe_active)
	return 0;

    if (!(s->s_type_flags & BV_DB_OBJS))
	return 0;

    return (s->s_os->s_dmode == 0 || s->s_os->s_dmode == 3);
}

static int
gl_swrast_wireframe_obj(struct dm *dmp, struct bv_scene_obj *s)
{
    if (!dmp || !s || !dm_get_dm_name(dmp) || !BU_STR_EQUAL(dm_get_dm_name(dmp), "swrast"))
	return 0;
    if (!(s->s_type_flags & BV_DB_OBJS))
	return 0;
    return (s->s_os->s_dmode == 0 || s->s_os->s_dmode == 3);
}

static inline void
swrast_put_pixel_rgba(struct swrast_vars_fast *pv, int w, int h, int x, int y, const unsigned char *fg)
{
    if (!pv || !pv->os_b || x < 0 || y < 0 || x >= w || y >= h)
	return;
    unsigned char *pix = ((unsigned char *)pv->os_b) + (((h - 1 - y) * w + x) * 4);
    pix[0] = fg[0];
    pix[1] = fg[1];
    pix[2] = fg[2];
    pix[3] = 255;
}

static void
swrast_draw_line_rgba(struct swrast_vars_fast *pv, int w, int h, int x0, int y0, int x1, int y1, const unsigned char *fg)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
	swrast_put_pixel_rgba(pv, w, h, x0, y0, fg);
	if (x0 == x1 && y0 == y1)
	    break;
	int e2 = 2 * err;
	if (e2 >= dy) {
	    err += dy;
	    x0 += sx;
	}
	if (e2 <= dx) {
	    err += dx;
	    y0 += sy;
	}
    }
}

static int
clip_line_to_win(int *x0, int *y0, int *x1, int *y1, int w, int h)
{
    enum { LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8 };

    auto code = [w, h](int x, int y) {
	int c = 0;
	if (x < 0) c |= LEFT;
	else if (x >= w) c |= RIGHT;
	if (y < 0) c |= BOTTOM;
	else if (y >= h) c |= TOP;
	return c;
    };

    int c0 = code(*x0, *y0);
    int c1 = code(*x1, *y1);
    while (1) {
	if (!(c0 | c1)) return 1;
	if (c0 & c1) return 0;

	int c = c0 ? c0 : c1;
	int x = 0, y = 0;
	if (c & TOP) {
	    y = h - 1;
	    x = *x0 + (*x1 - *x0) * (y - *y0) / ((*y1 - *y0) ? (*y1 - *y0) : 1);
	} else if (c & BOTTOM) {
	    y = 0;
	    x = *x0 + (*x1 - *x0) * (y - *y0) / ((*y1 - *y0) ? (*y1 - *y0) : 1);
	} else if (c & RIGHT) {
	    x = w - 1;
	    y = *y0 + (*y1 - *y0) * (x - *x0) / ((*x1 - *x0) ? (*x1 - *x0) : 1);
	} else {
	    x = 0;
	    y = *y0 + (*y1 - *y0) * (x - *x0) / ((*x1 - *x0) ? (*x1 - *x0) : 1);
	}

	if (c == c0) {
	    *x0 = x;
	    *y0 = y;
	    c0 = code(*x0, *y0);
	} else {
	    *x1 = x;
	    *y1 = y;
	    c1 = code(*x1, *y1);
	}
    }
}

static int
swrast_drawVList_fast(struct dm *dmp, struct bv_vlist *vp)
{
    if (!dmp || !vp)
	return BRLCAD_ERROR;

    struct swrast_vars_fast *pv = (struct swrast_vars_fast *)dmp->i->dm_vars.priv_vars;
    if (!pv || !pv->os_b || !pv->v)
	return BRLCAD_ERROR;

    int w = dmp->i->dm_width;
    int h = dmp->i->dm_height;
    if (w <= 0 || h <= 0)
	return BRLCAD_ERROR;

    fastf_t *xmat = pv->v->gv_model2view;
    point_t lpnt, pnt;
    int have_lpnt = 0;
    point_t *pt_prev = NULL;
    fastf_t dist_prev = 1.0;
    fastf_t delta = xmat[15] * 0.0001;
    if (delta < 0.0)
	delta = -delta;
    if (delta < SQRT_SMALL_FASTF)
	delta = SQRT_SMALL_FASTF;

    const unsigned char *fg = dmp->i->dm_fg;
    struct bv_vlist *tvp;
    for (BU_LIST_FOR(tvp, bv_vlist, &vp->l)) {
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (size_t i = 0; i < tvp->nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BV_VLIST_MODEL_MAT:
		    xmat = pv->v->gv_model2view;
		    continue;
		case BV_VLIST_DISPLAY_MAT:
		    xmat = pv->v->gv_model2view;
		    continue;
		case BV_VLIST_POLY_START:
		case BV_VLIST_POLY_VERTNORM:
		case BV_VLIST_TRI_START:
		case BV_VLIST_TRI_VERTNORM:
		    continue;
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_LINE_MOVE:
		case BV_VLIST_TRI_MOVE: {
		    if (dmp->i->dm_perspective > 0) {
			fastf_t dist = VDOT(*pt, &xmat[12]) + xmat[15];
			if (dist <= 0.0) {
			    pt_prev = pt;
			    dist_prev = dist;
			    continue;
			}
			dist_prev = dist;
			pt_prev = pt;
		    }
		    MAT4X3PNT(lpnt, xmat, *pt);
		    lpnt[0] *= 2047;
		    lpnt[1] *= 2047 * dmp->i->dm_aspect;
		    have_lpnt = 1;
		    continue;
		}
		case BV_VLIST_POLY_DRAW:
		case BV_VLIST_POLY_END:
		case BV_VLIST_LINE_DRAW:
		case BV_VLIST_TRI_DRAW:
		case BV_VLIST_TRI_END: {
		    if (!have_lpnt)
			continue;
		    if (dmp->i->dm_perspective > 0) {
			fastf_t dist = VDOT(*pt, &xmat[12]) + xmat[15];
			if (dist <= 0.0 && dist_prev <= 0.0) {
			    dist_prev = dist;
			    pt_prev = pt;
			    continue;
			}
			if (dist <= 0.0 && pt_prev) {
			    vect_t diff;
			    point_t tmp_pt;
			    fastf_t alpha = (dist_prev - delta) / (dist_prev - dist);
			    VSUB2(diff, *pt, *pt_prev);
			    VJOIN1(tmp_pt, *pt_prev, alpha, diff);
			    MAT4X3PNT(pnt, xmat, tmp_pt);
			} else {
			    MAT4X3PNT(pnt, xmat, *pt);
			}
			dist_prev = dist;
			pt_prev = pt;
		    } else {
			MAT4X3PNT(pnt, xmat, *pt);
		    }
		    pnt[0] *= 2047;
		    pnt[1] *= 2047 * dmp->i->dm_aspect;

		    int x0 = GED_TO_Xx(dmp, lpnt[0]);
		    int y0 = GED_TO_Xy(dmp, lpnt[1]);
		    int x1 = GED_TO_Xx(dmp, pnt[0]);
		    int y1 = GED_TO_Xy(dmp, pnt[1]);
		    if (clip_line_to_win(&x0, &y0, &x1, &y1, w, h)) {
			swrast_draw_line_rgba(pv, w, h, x0, y0, x1, y1, fg);
		    }
		    VMOVE(lpnt, pnt);
		    continue;
		}
		default:
		    continue;
	    }
	}
    }

    return BRLCAD_OK;
}

static void
dlist_free_callback(struct bv_scene_obj *s)
{
    if (!s)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(&s->children, i);
	dlist_free_callback(cg);
    }
    if (s->s_dlist) {
	struct dm *dmp = s->s_v ? (struct dm *)s->s_v->dmp : NULL;
	if (dmp && dm_make_current(dmp) == BRLCAD_OK)
	    glDeleteLists(s->s_dlist, 1);
	s->s_dlist = 0;
    }
    s->s_dlist_stale = 0;
    s->s_dlist_mode = 0;
}

// TODO - We can't currently use display lists for really large meshes, as we
// won't be able to hold both the original data and the compiled display list
// in memory at the same time.  For that scenario, we would first need to break
// down the big mesh into smaller pieces as in the earlier LoD experiments in
// order to keep using display lists...
static void
gl_mesh_triangles(struct bv_mesh_lod *lod, bool shaded)
{
    const bool clockwise = shaded && lod->clockwise;
    const int corners[3] = {0, clockwise ? 2 : 1, clockwise ? 1 : 2};
    const point2d_t *snapped_uvs = shaded ? bv_mesh_lod_snapped_uvs(lod) : NULL;
    if (shaded)
	glBegin(GL_TRIANGLES);

    for (int i = 0; i < lod->fcnt; i++) {
	const size_t offset = (size_t)i * 3;
	const int *face = &lod->faces[offset];
	bool valid = true;
	for (int j = 0; j < 3; j++) {
	    if (face[j] < 0 || face[j] >= lod->pcnt) {
		bu_log("bad face %d - skipping\n", i);
		valid = false;
		break;
	    }
	}
	if (!valid)
	    continue;

	if (!shaded) {
	    glBegin(GL_LINE_STRIP);
	    for (int j = 0; j < 3; j++)
		glVertex3dv(lod->points[face[j]]);
	    glVertex3dv(lod->points[face[0]]);
	    glEnd();
	    continue;
	}

	vect_t ab, ac, norm;
	VSUB2(ab, lod->points_orig[face[0]], lod->points_orig[face[corners[1]]]);
	VSUB2(ac, lod->points_orig[face[0]], lod->points_orig[face[corners[2]]]);
	VCROSS(norm, ab, ac);
	VUNITIZE(norm);
	glNormal3dv(norm);

	// The LoD cache preserves corner order even when it snaps two
	// different vertices to the same position.
	const int source = lod->face_sources ? lod->face_sources[i] : i;
	const int *indices = NULL;
	if (lod->face_uvs && source >= 0 && (size_t)source < lod->uv_face_count)
	    indices = &lod->face_uvs[(size_t)source * 3];

	for (int j : corners) {
	    if (lod->normals) {
		const fastf_t *n = lod->normals[offset + j];
		glNormal3dv(VNEAR_ZERO(n, SMALL_FASTF) ? norm : n);
	    }
	    if (snapped_uvs)
		glTexCoord2dv(snapped_uvs[offset + j]);
	    else if (indices)
		glTexCoord2dv(lod->uvs[indices[j]]);
	    glVertex3dv(lod->points[face[j]]);
	}
    }

    if (shaded)
	glEnd();
}

static int
gl_draw_tri(struct dm *dmp, struct bv_mesh_lod *lod)
{
    struct bv_scene_obj *s = lod->s;
    const int mode = s->s_os->s_dmode;
    if (mode < 0 || mode > 2)
	return BRLCAD_ERROR;
    const bool shaded = mode != 0;
    auto *mvars = (struct gl_vars *)dmp->i->m_vars;

    gl_debug_print(dmp, "gl_draw_tri", dmp->i->dm_debugLevel);

    if (s->s_dlist && (s->s_dlist_stale || s->s_dlist_mode != mode)) {
	glDeleteLists(s->s_dlist, 1);
	s->s_dlist = 0;
    }
    if (!s->s_dlist && !lod->points) {
	const int level = bv_mesh_lod_level(s, -1, 0);
	bv_mesh_lod_level(s, level, 1);
    }
    if (!s->s_dlist && !lod->fcnt)
	return BRLCAD_OK;

    // Material and texture state must apply on both immediate and cached draws.
    glPushAttrib(GL_LIGHTING_BIT | GL_COLOR_BUFFER_BIT);
    if (mvars->lighting_on) {
	static const float black[4] = {0.0, 0.0, 0.0, 0.0};
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, shaded ? black : mvars->i.wireColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, shaded ? mvars->i.ambientColor : black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, shaded ? mvars->i.specularColor : black);
	if (!shaded) {
	    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);
	} else {
	    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	    glMaterialfv(GL_FRONT, GL_DIFFUSE, mvars->i.diffuseColor);
	    switch (mvars->lighting_on) {
		case 1:
		    break;
		case 2:
		    glMaterialfv(GL_BACK, GL_DIFFUSE, mvars->i.diffuseColor);
		    break;
		case 3:
		    glMaterialfv(GL_BACK, GL_DIFFUSE, mvars->i.backDiffuseColorDark);
		    break;
		default:
		    glMaterialfv(GL_BACK, GL_DIFFUSE, mvars->i.backDiffuseColorLight);
		    break;
	    }
	}
	if (mvars->transparency_on) {
	    if (shaded) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	    } else {
		glDisable(GL_BLEND);
	    }
	}
    }

    if (!s->s_dlist) {
	// Large meshes may require more memory to compile than is available.
	const ssize_t avail_mem = bu_mem(BU_MEM_AVAIL, NULL) / 2;
	const size_t vertex_size = sizeof(point_t) + ((shaded && lod->uvs) ? 2 * sizeof(GLdouble) : 0);
	const size_t size_est = (size_t)lod->fcnt * 3 * vertex_size;
	if (avail_mem > 0 && size_est < (size_t)avail_mem)
	    s->s_dlist = glGenLists(1);
	if (s->s_dlist) {
	    glNewList(s->s_dlist, GL_COMPILE);
	    gl_mesh_triangles(lod, shaded);
	    glEndList();
	    s->s_dlist_mode = mode;
	    s->s_dlist_stale = 0;
	    s->s_dlist_free_callback = &dlist_free_callback;
	    if (size_est > (size_t)avail_mem / 100)
		bv_mesh_lod_memshrink(s);
	    dm_fire_dlist_sensors(dmp);
	}
    }

    mat_t draw_mat;
    bn_mat_mul(draw_mat, s->s_v->gv_model2view, s->s_mat);
    dm_loadmatrix(dmp, draw_mat, 0);
    if (s->s_dlist)
	glCallList(s->s_dlist);
    else
	gl_mesh_triangles(lod, shaded);
    dm_loadmatrix(dmp, s->s_v->gv_model2view, 0);
    glPopAttrib();

    return BRLCAD_OK;
}


static int
gl_csg_lod(struct dm *dmp, struct bv_scene_obj *s)
{
    int mode = s->s_os->s_dmode;
    mat_t save_mat, draw_mat;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    GLdouble dpt[3];
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalLineWidth, originalPointSize;

    if (mode)
	return BRLCAD_ERROR;

    glGetFloatv(GL_POINT_SIZE, &originalPointSize);
    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    gl_debug_print(dmp, "gl_csg_lod", dmp->i->dm_debugLevel);

    // If the dlist is stale, clear it
    if (s->s_dlist_stale) {
	glDeleteLists(s->s_dlist, 1);
	s->s_dlist = 0;
    }

    // We don't want color to be part of the dlist, to allow the app
    // to change it without regeneration - hence, we need to do it
    // up front
    if (s->s_iflag == UP) {
	dm_set_fg(dmp, 255, 255, 255, 0, s->s_os->transparency);
    }
    if (mvars->lighting_on) {
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mvars->i.wireColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);
	if (mvars->transparency_on)
	    glDisable(GL_BLEND);
    }

    // If we have a dlist in the correct mode, use it
    if (s->s_dlist) {
	if (mode == s->s_dlist_mode) {
	    //bu_log("use dlist %d\n", s->s_dlist);
	    MAT_COPY(save_mat, s->s_v->gv_model2view);
	    bn_mat_mul(draw_mat, s->s_v->gv_model2view, s->s_mat);
	    dm_loadmatrix(dmp, draw_mat, 0);
	    glCallList(s->s_dlist);
	    dm_loadmatrix(dmp, save_mat, 0);
	    glLineWidth(originalLineWidth);
	    return BRLCAD_OK;
	} else {
	    // Display list mode is incorrect (wireframe when we
	    // want shaded, or vice versa.)
	    glDeleteLists(s->s_dlist, 1);
	    s->s_dlist = 0;
	}
    }

    // Figure out if we need a new dlist (and if so whether this vlist is a
    // candidate.)  OpenGL Display Lists are faster than immediate drawing when
    // we can use them, but they require more memory usage while they are being
    // generated.  If we're tight on memory and the vlist is large, accept the
    // slower drawing to avoid memory stress - otherwise, use display lists
    ssize_t avail_mem = 0.5*bu_mem(BU_MEM_AVAIL, NULL);
    size_t size_est = (bu_list_len(&s->s_vlist)*sizeof(point_t));
    bool gen_dlist = false;
    if (!s->s_dlist && avail_mem > 0 && size_est < (size_t)avail_mem) {
	gen_dlist = true;
	s->s_dlist = glGenLists(1);
	s->s_dlist_mode = mode;
	bu_log("gen_dlist: %d\n", s->s_dlist);
	s->s_dlist_free_callback = &dlist_free_callback;
	glNewList(s->s_dlist, GL_COMPILE);
    } else {
	bu_log("Not using dlist\n");
	// Straight-up drawing - set up the matrix
	MAT_COPY(save_mat, s->s_v->gv_model2view);
	bn_mat_mul(draw_mat, s->s_v->gv_model2view, s->s_mat);
	dm_loadmatrix(dmp, draw_mat, 0);
    }

    int first = 1;
    struct bv_vlist *tvp;
    GLfloat pointSize = 0.0;
    for (BU_LIST_FOR(tvp, bv_vlist, &s->s_vlist)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    VMOVE(dpt, *pt);
	    switch (*cmd) {
		case BV_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;
		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(dpt);
		    break;
		case BV_VLIST_LINE_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BV_VLIST_POINT_DRAW:
		    if (first == 0)
			glEnd();
		    first = 0;
		    if (pointSize > 0.0)
			glPointSize(pointSize);
		    glVertex3dv(dpt);
		    break;
		case BV_VLIST_POINT_SIZE:
		    {
			pointSize = (GLfloat)(*pt)[0];
			if (pointSize > 0.0) {
			    glPointSize(pointSize);
			}
			break;
		    }

	    }
	}
    }
    if (first == 0)
	glEnd();

    if (gen_dlist) {
	glEndList();
	s->s_dlist_stale = 0;
	if (size_est > (avail_mem * 0.01)) {
	    // If the original data is sizable, clear it to save system memory.
	    // The dlist has what it needs, and the LoD code will re-load info
	    // as needed for updates.
	    //
	    // Note that unlike most view vlist "free" operations this is
	    // intentionally a real free instead of putting the vlists onto the
	    // vlfree list for reuse - since we're trying to conserve system
	    // memory, using vlfree here wouldn't improve the situation...
	    struct bu_list *p;
	    while (BU_LIST_WHILE(p, bu_list, &s->s_vlist)) {
		BU_LIST_DEQUEUE(p);
		struct bv_vlist *pv = (struct bv_vlist *)p;
		BU_FREE(pv, struct bv_vlist);
	    }
	}

	/* notify registered sensors that the dlist was regenerated */
	dm_fire_dlist_sensors(dmp);

	MAT_COPY(save_mat, s->s_v->gv_model2view);
	bn_mat_mul(draw_mat, s->s_v->gv_model2view, s->s_mat);
	dm_loadmatrix(dmp, draw_mat, 0);
	glCallList(s->s_dlist);
	dm_loadmatrix(dmp, save_mat, 0);
    }

    if (mvars->lighting_on && mvars->transparency_on)
	glDisable(GL_BLEND);

    glPointSize(originalPointSize);
    glLineWidth(originalLineWidth);

    // Without dlist, we had to set the matrix - restore
    if (!s->s_dlist)
	dm_loadmatrix(dmp, save_mat, 0);

    return BRLCAD_OK;
}

extern "C"
int gl_draw_obj(struct dm *dmp, struct bv_scene_obj *s)
{
    GLint originalShadeModel = 0;
    int restoreShadeModel = 0;
    GLboolean lightingWasEnabled = GL_FALSE;
    int restoreLighting = 0;
    gl_texture_binding texture(dmp, s);

    if (s->s_type_flags & BV_MESH_LOD) {
	struct bv_mesh_lod *lod = (struct bv_mesh_lod *)s->draw_data;
	return gl_draw_tri(dmp, lod);
    }

    if (s->s_type_flags & BV_CSG_LOD) {
	return gl_csg_lod(dmp, s);
    }

    // "Standard" vlist object drawing
    if (bu_list_len(&s->s_vlist)) {
	if (gl_swrast_wireframe_obj(dmp, s)) {
	    lightingWasEnabled = glIsEnabled(GL_LIGHTING);
	    if (lightingWasEnabled) {
		unsigned char *fg = dm_get_fg(dmp);
		glDisable(GL_LIGHTING);
		glColor3ub((GLubyte)fg[0], (GLubyte)fg[1], (GLubyte)fg[2]);
		restoreLighting = 1;
	    }
	    if (gl_swrast_database_wireframe(dmp, s) && swrast_drawVList_fast(dmp, (struct bv_vlist *)&s->s_vlist) == BRLCAD_OK) {
		if (restoreLighting)
		    glEnable(GL_LIGHTING);
		return BRLCAD_OK;
	    }
	    glGetIntegerv(GL_SHADE_MODEL, &originalShadeModel);
	    if (originalShadeModel != GL_FLAT) {
		glShadeModel(GL_FLAT);
		restoreShadeModel = 1;
	    }
	}
	if (s->s_os->s_dmode == 4) {
	    dm_draw_vlist_hidden_line(dmp, (struct bv_vlist *)&s->s_vlist);
	} else {
	    dm_draw_vlist(dmp, (struct bv_vlist *)&s->s_vlist);
	}
	if (restoreShadeModel)
	    glShadeModel((GLenum)originalShadeModel);
	if (restoreLighting)
	    glEnable(GL_LIGHTING);
	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
