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
    const int LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8;
    auto code = [w, h, LEFT, RIGHT, BOTTOM, TOP](int x, int y) {
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
    bu_log("dlist cleanup\n");
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(&s->children, i);
	dlist_free_callback(cg);
    }
    if (s->s_dlist) {
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
static int
gl_draw_tri(struct dm *dmp, struct bv_mesh_lod *lod)
{
    int fcnt = lod->fcnt;
    int pcnt = lod->pcnt;
    const int *faces = lod->faces;
    const point_t *points = lod->points;
    const point_t *points_orig = lod->points_orig;
    const vect_t *normals = lod->normals;
    struct bv_scene_obj *s = lod->s;
    int mode = s->s_os->s_dmode;
    mat_t save_mat, draw_mat;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    GLdouble dpt[3];
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalLineWidth;

    if (mode < 0 || mode > 1)
	return BRLCAD_ERROR;

    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    gl_debug_print(dmp, "gl_draw_tri", dmp->i->dm_debugLevel);

    // If the dlist is stale, clear it
    if (s->s_dlist_stale) {
	glDeleteLists(s->s_dlist, 1);
	s->s_dlist = 0;

	if (!pcnt || !fcnt) {
	    // If we've had a memshrink, the loaded data isn't
	    // going to be correct to generate new draw info.
	    // First, find out the current level:
	    int curr_level = bv_mesh_lod_level(s, -1, 0);

	    // Trigger a load operation to restore it
	    bv_mesh_lod_level(s, curr_level, 1);

	    fcnt = lod->fcnt;
	    pcnt = lod->pcnt;
	    faces = lod->faces;
	    points = lod->points;
	    points_orig = lod->points_orig;
	    normals = lod->normals;
	}
    }

    // We don't want color to be part of the dlist, to allow the app
    // to change it without regeneration - hence, we need to do it
    // up front
    if (mode == 0) {
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
    } else {
	if (s->s_iflag == UP) {
	    dm_set_fg(dmp, 255, 255, 255, 0, s->s_os->transparency);
	}
	if (mvars->lighting_on) {
	    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mvars->i.ambientColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mvars->i.specularColor);
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
	if (mvars->lighting_on) {
	    if (mvars->transparency_on) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	    }
	}
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
	    if (mvars->transparency_on)
		glDisable(GL_BLEND);
	    return BRLCAD_OK;
	} else {
	    // Display list mode is incorrect (wireframe when we
	    // want shaded, or vice versa.)
	    glDeleteLists(s->s_dlist, 1);
	    s->s_dlist = 0;
	}
    }

    // Figure out if we need a new dlist (and if so whether this triangle set
    // is a candidate.)  OpenGL Display Lists are faster than immediate
    // triangle drawing when we can use them, but they require more memory
    // usage while they are being generated.  If we're tight on memory and the
    // triangle set is large, accept the slower drawing to avoid memory stress
    // - otherwise, we want the list
    ssize_t avail_mem = 0.5*bu_mem(BU_MEM_AVAIL, NULL);
    size_t size_est = (size_t)(fcnt*3*sizeof(point_t));
    bool gen_dlist = false;
    if (!s->s_dlist && avail_mem > 0 && size_est < (size_t)avail_mem) {
	gen_dlist = true;
	s->s_dlist = glGenLists(1);
	s->s_dlist_mode = mode;
	//bu_log("gen_dlist: %d\n", s->s_dlist);
	s->s_dlist_free_callback = &dlist_free_callback;
	glNewList(s->s_dlist, GL_COMPILE);
    } else {
	bu_log("Not using dlist\n");
	// Straight-up drawing - set up the matrix
	MAT_COPY(save_mat, s->s_v->gv_model2view);
	bn_mat_mul(draw_mat, s->s_v->gv_model2view, s->s_mat);
	dm_loadmatrix(dmp, draw_mat, 0);
    }

    // Wireframe
    if (mode == 0) {
	// Draw all the triangles in faces array
	for (int i = 0; i < fcnt; i++) {

	    bool bad_face = false;
	    for (int j = 0; j < 3; j++) {
		int f_ind = faces[3*i+j];
		if (f_ind >= pcnt || f_ind < 0) {
		    bu_log("bad face %d - skipping\n", i);
		    bad_face = true;
		    break;
		}
	    }
	    if (bad_face)
		continue;

	    glBegin(GL_LINE_STRIP);
	    VMOVE(dpt, points[faces[3*i+0]]);
	    glVertex3dv(dpt);
	    VMOVE(dpt, points[faces[3*i+1]]);
	    glVertex3dv(dpt);
	    VMOVE(dpt, points[faces[3*i+2]]);
	    glVertex3dv(dpt);
	    VMOVE(dpt, points[faces[3*i+0]]);
	    glVertex3dv(dpt);
	    glEnd();
	}
	if (gen_dlist) {
	    glEndList();
	    s->s_dlist_stale = 0;
	    if (size_est > (avail_mem * 0.01)) {
		// If the original data is sizable, clear it to save system memory.
		// The dlist has what it needs, and the LoD code will re-load info
		// as needed for updates.
		bv_mesh_lod_memshrink(s);
	    }

	    MAT_COPY(save_mat, s->s_v->gv_model2view);
	    bn_mat_mul(draw_mat, s->s_v->gv_model2view, s->s_mat);
	    dm_loadmatrix(dmp, draw_mat, 0);
	    glCallList(s->s_dlist);
	    dm_loadmatrix(dmp, save_mat, 0);
	}

	glLineWidth(originalLineWidth);

	if (mvars->transparency_on)
	    glDisable(GL_BLEND);

	// Without dlist, we had to set the matrix - restore
	if (!s->s_dlist)
	    dm_loadmatrix(dmp, save_mat, 0);

	return BRLCAD_OK;
    }

    // Shaded
    if (mode == 1) {

	// For LoD drawing, we need to use two sided shading - thin objects or
	// very low detail triangle objects will sometimes draw multiple
	// triangles in the same position, which can result in the wrong "side"
	// being visible from some views.
	GLint two_sided;
	glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &two_sided);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

	glBegin(GL_TRIANGLES);

	// Draw all the triangles in faces array
	for (int i = 0; i < fcnt; i++) {

	    bool bad_face = false;
	    for (int j = 0; j < 3; j++) {
		int f_ind = faces[3*i+j];
		if (f_ind >= pcnt || f_ind < 0) {
		    bu_log("bad face %d - skipping\n", i);
		    bad_face = true;
		    break;
		}
	    }
	    if (bad_face)
		continue;

	    // Set surface normal
	    vect_t ab, ac, norm;
	    VSUB2(ab, points_orig[faces[3*i+0]], points_orig[faces[3*i+1]]);
	    VSUB2(ac, points_orig[faces[3*i+0]], points_orig[faces[3*i+2]]);
	    VCROSS(norm, ab, ac);
	    VUNITIZE(norm);

	    if (normals) {
		vect_t vnorm;
		VMOVE(vnorm, normals[3*i+0]);
		if (((int)(vnorm[0]*10+vnorm[1]*10+vnorm[2]*10)) != 0) {
		    glNormal3dv(vnorm);
		} else {
		    glNormal3dv(norm);
		}
		VMOVE(dpt, points[faces[3*i+0]]);
		glVertex3dv(dpt);

		VMOVE(vnorm, normals[3*i+1]);
		if (((int)(vnorm[0]*10+vnorm[1]*10+vnorm[2]*10)) != 0) {
		    glNormal3dv(vnorm);
		} else {
		    glNormal3dv(norm);
		}
		VMOVE(dpt, points[faces[3*i+1]]);
		glVertex3dv(dpt);

		VMOVE(vnorm, normals[3*i+2]);
		if (((int)(vnorm[0]*10+vnorm[1]*10+vnorm[2]*10)) != 0) {
		    glNormal3dv(vnorm);
		} else {
		    glNormal3dv(norm);
		}
		VMOVE(dpt, points[faces[3*i+2]]);
		glVertex3dv(dpt);

	    } else {
		glNormal3dv(norm);
		VMOVE(dpt, points[faces[3*i+0]]);
		glVertex3dv(dpt);
		VMOVE(dpt, points[faces[3*i+1]]);
		glVertex3dv(dpt);
		VMOVE(dpt, points[faces[3*i+2]]);
		glVertex3dv(dpt);
	    }

	}

	glEnd();

	if (gen_dlist) {
	    glEndList();
	    s->s_dlist_stale = 0;
	    if (size_est > (avail_mem * 0.01)) {
		// If the original data is sizable, clear it to save system memory.
		// The dlist has what it needs, and the LoD code will re-load info
		// as needed for updates.
		bv_mesh_lod_memshrink(s);
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

	// Put the lighting model back where it was prior to this operation
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, two_sided);

	glLineWidth(originalLineWidth);

	// If we're not using a pre-baked dlist, restore matrix
	if (!s->s_dlist)
	    dm_loadmatrix(dmp, save_mat, 0);

	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
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
