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
#include "bsg/defines.h"
#include "bsg/appearance.h"
#include "bsg/draw_source.h"
#include "bsg/geometry.h"
#include "bsg/lod.h"
#include "bsg/render_item.h"
#include "dm.h"
#include "./dm-gl.h"
#include "./include/private.h"
}

static int
gl_item_draw_matrix(const struct bsg_render_item *item, mat_t save_mat, mat_t draw_mat)
{
    if (!item)
	return 0;
    MAT_COPY(save_mat, item->context.model2view);
    bn_mat_mul(draw_mat, item->context.model2view, item->model_mat);
    return 1;
}

static int
gl_draw_line_arrays_retained_immediate(struct dm *dmp,
				       const point_t *points,
				       const int *cmds,
				       size_t point_cnt,
				       size_t cmd_cnt)
{
    if (!dmp || point_cnt < 1 || !points)
	return BRLCAD_OK;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalLineWidth = 0.0;
    int material_set = 0;
    int first = 1;

    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    for (size_t i = 0; i < point_cnt; i++) {
	int cmd = (cmds && i < cmd_cnt) ? cmds[i] : ((i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE);
	GLdouble dpt[3];
	VMOVE(dpt, points[i]);
	switch (cmd) {
	    case BSG_GEOMETRY_LINE_MOVE:
		if (!first)
		    glEnd();
		first = 0;
		if (mvars->lighting_on && !material_set) {
		    material_set = 1;
		    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mvars->i.wireColor);
		    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
		    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
		    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);
		    if (mvars->transparency_on)
			glDisable(GL_BLEND);
		}
		glBegin(GL_LINE_STRIP);
		glVertex3dv(dpt);
		break;
	    case BSG_GEOMETRY_LINE_DRAW:
		if (first) {
		    first = 0;
		    if (mvars->lighting_on && !material_set) {
			material_set = 1;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mvars->i.wireColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);
			if (mvars->transparency_on)
			    glDisable(GL_BLEND);
		    }
		    glBegin(GL_LINE_STRIP);
		}
		glVertex3dv(dpt);
		break;
	    case BSG_GEOMETRY_POINT_DRAW:
		if (!first)
		    glEnd();
		first = 1;
		if (mvars->lighting_on && !material_set) {
		    material_set = 1;
		    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mvars->i.wireColor);
		    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
		    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
		    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);
		    if (mvars->transparency_on)
			glDisable(GL_BLEND);
		}
		glBegin(GL_POINTS);
		glVertex3dv(dpt);
		glEnd();
		break;
	    default:
		break;
	}
    }
    if (!first)
	glEnd();

    glLineWidth(originalLineWidth);

    return BRLCAD_OK;
}

static int
gl_draw_indexed_line_arrays_retained_immediate(struct dm *dmp,
					       const point_t *points,
					       size_t point_cnt,
					       const int *indices,
					       size_t index_cnt)
{
    if (!dmp || point_cnt < 2 || !points || !indices || !index_cnt)
	return BRLCAD_OK;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalLineWidth = 0.0;
    int material_set = 0;
    int strip_open = 0;

    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    for (size_t i = 0; i < index_cnt; i++) {
	int idx = indices[i];
	if (idx < 0) {
	    if (strip_open) {
		glEnd();
		strip_open = 0;
	    }
	    continue;
	}
	if ((size_t)idx >= point_cnt)
	    continue;
	if (!strip_open) {
	    if (mvars->lighting_on && !material_set) {
		material_set = 1;
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mvars->i.wireColor);
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);
		if (mvars->transparency_on)
		    glDisable(GL_BLEND);
	    }
	    glBegin(GL_LINE_STRIP);
	    strip_open = 1;
	}
	GLdouble dpt[3];
	VMOVE(dpt, points[idx]);
	glVertex3dv(dpt);
    }
    if (strip_open)
	glEnd();

    glLineWidth(originalLineWidth);

    return BRLCAD_OK;
}

static const point_t *
gl_surface_normal_for_vertex(const point_t *normals,
			     size_t normal_cnt,
			     bsg_render_surface_normal_kind normal_kind,
			     int vertex_idx,
			     size_t face_idx,
			     size_t corner_idx)
{
    if (!normals || !normal_cnt)
	return NULL;

    switch (normal_kind) {
	case BSG_RENDER_SURFACE_NORMALS_PER_INDEX:
	    return (corner_idx < normal_cnt) ? &normals[corner_idx] : NULL;
	case BSG_RENDER_SURFACE_NORMALS_PER_VERTEX:
	    return (vertex_idx >= 0 && (size_t)vertex_idx < normal_cnt) ?
		&normals[vertex_idx] : NULL;
	case BSG_RENDER_SURFACE_NORMALS_PER_FACE:
	    return (face_idx < normal_cnt) ? &normals[face_idx] : NULL;
	default:
	    break;
    }

    return NULL;
}

static int
gl_draw_indexed_face_arrays_retained_immediate(struct dm *dmp,
					       const struct bsg_render_item *item,
					       const struct bsg_resolved_appearance *appearance,
					       const point_t *points,
					       size_t point_cnt,
					       const int *indices,
					       size_t index_cnt,
					       const point_t *normals,
					       size_t normal_cnt,
					       bsg_render_surface_normal_kind normal_kind)
{
    if (!dmp || point_cnt < 3 || !points || !indices || !index_cnt)
	return BRLCAD_OK;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    mat_t save_mat, draw_mat;
    int matrix_loaded = 0;
    int material_set = 0;
    int drawing = 0;
    GLint two_sided = 0;
    int two_sided_set = 0;
    int mesh_lod = item && item->source.geometry_role == BSG_RENDER_GEOMETRY_ROLE_MESH;

    if (item) {
	if (!gl_item_draw_matrix(item, save_mat, draw_mat))
	    return BRLCAD_ERROR;
	dm_loadmatrix(dmp, draw_mat, 0);
	matrix_loaded = 1;
    }
    if (appearance && appearance->highlighted) {
	fastf_t transparency = appearance->transparency;
	dm_set_fg(dmp, 255, 255, 255, 0, transparency);
    }
    if (mesh_lod && mvars->lighting_on) {
	glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &two_sided);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	two_sided_set = 1;
    }

    if (mesh_lod) {
	if (mvars->lighting_on) {
	    material_set = 1;
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
	    if (mvars->transparency_on) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	    }
	}
	glBegin(GL_TRIANGLES);
	size_t face_idx = 0;
	size_t corner_idx = 0;
	for (size_t i = 0; i + 2 < index_cnt; i += 4) {
	    for (size_t j = 0; j < 3; j++) {
		int idx = indices[i + j];
		if (idx < 0)
		    continue;
		const point_t *normal = gl_surface_normal_for_vertex(normals,
			normal_cnt, normal_kind, idx, face_idx, corner_idx);
		corner_idx++;
		if ((size_t)idx >= point_cnt)
		    continue;
		if (normal) {
		    GLdouble npt[3];
		    VMOVE(npt, *normal);
		    glNormal3dv(npt);
		}
		GLdouble dpt[3];
		VMOVE(dpt, points[idx]);
		glVertex3dv(dpt);
	    }
	    face_idx++;
	}
	glEnd();
	if (mvars->lighting_on && mvars->transparency_on)
	    glDisable(GL_BLEND);
	if (two_sided_set)
	    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, two_sided);
	if (matrix_loaded)
	    dm_loadmatrix(dmp, save_mat, 0);
	return BRLCAD_OK;
    }

    size_t face_idx = 0;
    size_t corner_idx = 0;
    for (size_t i = 0; i < index_cnt; i++) {
	int idx = indices[i];
	if (idx < 0) {
	    if (drawing) {
		glEnd();
		drawing = 0;
		face_idx++;
	    }
	    continue;
	}
	const point_t *normal = gl_surface_normal_for_vertex(normals,
		normal_cnt, normal_kind, idx, face_idx, corner_idx);
	corner_idx++;
	if ((size_t)idx >= point_cnt)
	    continue;
	if (!drawing) {
	    if (mvars->lighting_on && !material_set) {
		material_set = 1;
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

		if (mvars->transparency_on) {
		    glEnable(GL_BLEND);
		    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	    }
	    glBegin(GL_POLYGON);
	    drawing = 1;
	}
	if (normal) {
	    GLdouble npt[3];
	    VMOVE(npt, *normal);
	    glNormal3dv(npt);
	}
	GLdouble dpt[3];
	VMOVE(dpt, points[idx]);
	glVertex3dv(dpt);
    }
    if (drawing)
	glEnd();

    if (mvars->lighting_on && mvars->transparency_on)
	glDisable(GL_BLEND);
    if (two_sided_set)
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, two_sided);
    if (matrix_loaded)
	dm_loadmatrix(dmp, save_mat, 0);

    return BRLCAD_OK;
}

static int
gl_draw_indexed_face_arrays_wire_retained_immediate(struct dm *dmp,
						    const struct bsg_render_item *item,
						    const struct bsg_resolved_appearance *appearance,
						    const point_t *points,
						    size_t point_cnt,
						    const int *indices,
						    size_t index_cnt)
{
    if (!dmp || point_cnt < 3 || !points || !indices || !index_cnt)
	return BRLCAD_OK;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    mat_t save_mat, draw_mat;
    int matrix_loaded = 0;
    GLboolean lighting_was_enabled = glIsEnabled(GL_LIGHTING);

    if (item) {
	if (!gl_item_draw_matrix(item, save_mat, draw_mat))
	    return BRLCAD_ERROR;
	dm_loadmatrix(dmp, draw_mat, 0);
	matrix_loaded = 1;
    }
    if (appearance && appearance->highlighted) {
	fastf_t transparency = appearance->transparency;
	dm_set_fg(dmp, 255, 255, 255, 0, transparency);
    }

    if (lighting_was_enabled)
	glDisable(GL_LIGHTING);
    if (mvars->transparency_on)
	glDisable(GL_BLEND);

    int face_start = -1;
    int last_idx = -1;
    int drawing = 0;
    for (size_t i = 0; i < index_cnt; i++) {
	int idx = indices[i];
	if (idx < 0) {
	    if (drawing) {
		if (face_start >= 0 && (size_t)face_start < point_cnt) {
		    GLdouble dpt[3];
		    VMOVE(dpt, points[face_start]);
		    glVertex3dv(dpt);
		}
		glEnd();
	    }
	    face_start = -1;
	    last_idx = -1;
	    drawing = 0;
	    continue;
	}
	if ((size_t)idx >= point_cnt)
	    continue;
	if (!drawing) {
	    glBegin(GL_LINE_STRIP);
	    face_start = idx;
	    drawing = 1;
	}
	GLdouble dpt[3];
	VMOVE(dpt, points[idx]);
	glVertex3dv(dpt);
	last_idx = idx;
    }
    if (drawing) {
	if (face_start >= 0 && last_idx != face_start && (size_t)face_start < point_cnt) {
	    GLdouble dpt[3];
	    VMOVE(dpt, points[face_start]);
	    glVertex3dv(dpt);
	}
	glEnd();
    }

    if (lighting_was_enabled)
	glEnable(GL_LIGHTING);
    if (matrix_loaded)
	dm_loadmatrix(dmp, save_mat, 0);
    return BRLCAD_OK;
}

static int
gl_draw_indexed_face_arrays_hidden_line_retained(struct dm *dmp,
						 const struct bsg_render_item *item,
						 const point_t *points,
						 size_t point_cnt,
						 const int *indices,
						 size_t index_cnt,
						 const point_t *normals,
						 size_t normal_cnt,
						 bsg_render_surface_normal_kind normal_kind)
{
    if (!dmp || point_cnt < 3 || !points || !indices || !index_cnt)
	return BRLCAD_OK;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    mat_t save_mat, draw_mat;
    int matrix_loaded = 0;
    int drawing = 0;
    int first_idx = -1;

    if (item) {
	if (!gl_item_draw_matrix(item, save_mat, draw_mat))
	    return BRLCAD_ERROR;
	dm_loadmatrix(dmp, draw_mat, 0);
	matrix_loaded = 1;
    }

    if (mvars->lighting_on)
	glDisable(GL_LIGHTING);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPolygonOffset(1.0, 1.0);
    glColor3f(mvars->i.r1, mvars->i.g1, mvars->i.b1);

    size_t face_idx = 0;
    size_t corner_idx = 0;
    for (size_t i = 0; i < index_cnt; i++) {
	int idx = indices[i];
	if (idx < 0) {
	    if (drawing) {
		glEnd();
		drawing = 0;
		face_idx++;
	    }
	    first_idx = -1;
	    continue;
	}
	const point_t *normal = gl_surface_normal_for_vertex(normals,
		normal_cnt, normal_kind, idx, face_idx, corner_idx);
	corner_idx++;
	if ((size_t)idx >= point_cnt)
	    continue;
	if (!drawing) {
	    glBegin(GL_POLYGON);
	    drawing = 1;
	    first_idx = idx;
	}
	if (normal) {
	    GLdouble npt[3];
	    VMOVE(npt, *normal);
	    glNormal3dv(npt);
	}
	GLdouble dpt[3];
	VMOVE(dpt, points[idx]);
	glVertex3dv(dpt);
    }
    if (drawing)
	glEnd();

    glColor3f(mvars->i.wireColor[0], mvars->i.wireColor[1], mvars->i.wireColor[2]);

    drawing = 0;
    first_idx = -1;
    for (size_t i = 0; i < index_cnt; i++) {
	int idx = indices[i];
	if (idx < 0) {
	    if (drawing) {
		if (first_idx >= 0 && (size_t)first_idx < point_cnt) {
		    GLdouble first_pt[3];
		    VMOVE(first_pt, points[first_idx]);
		    glVertex3dv(first_pt);
		}
		glEnd();
		drawing = 0;
	    }
	    first_idx = -1;
	    continue;
	}
	if ((size_t)idx >= point_cnt)
	    continue;
	if (!drawing) {
	    glBegin(GL_LINE_STRIP);
	    drawing = 1;
	    first_idx = idx;
	}
	GLdouble dpt[3];
	VMOVE(dpt, points[idx]);
	glVertex3dv(dpt);
    }
    if (drawing) {
	if (first_idx >= 0 && (size_t)first_idx < point_cnt) {
	    GLdouble first_pt[3];
	    VMOVE(first_pt, points[first_idx]);
	    glVertex3dv(first_pt);
	}
	glEnd();
    }

    if (mvars->lighting_on)
	glEnable(GL_LIGHTING);
    if (!mvars->zbuffer_on)
	glDisable(GL_DEPTH_TEST);
    if (!dmp->i->dm_depthMask)
	glDepthMask(GL_FALSE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (matrix_loaded)
	dm_loadmatrix(dmp, save_mat, 0);

    return BRLCAD_OK;
}

/* ---------------------------------------------------------------------
 * Retained GL-backend resource cache.
 *
 * The GL family of display managers (dm-gl, dm-qtgl, dm-glx, dm-wgl,
 * dm-swrast) caches backend-private GL command resources in the display
 * manager's resource cache.  The cache key is derived entirely from the
 * resolved render item/backend record; command-visible scene state never owns
 * GL resource identifiers.
 */
struct gl_backend_handle {
    unsigned int command_list;
    uint64_t resource_key;
};

enum gl_retained_transform_policy {
    GL_RETAINED_TRANSFORM_DEFERRED_TO_ITEM_MATRIX = 1
};

static uint64_t
gl_hash_u64(uint64_t h, uint64_t v)
{
    h ^= v;
    h *= 1099511628211ULL;
    return h;
}

static uint64_t
gl_backend_resource_identity(const struct bsg_render_item *item, int mode)
{
    uint64_t h = 1469598103934665603ULL;
    if (!item)
	return 0;
    h = gl_hash_u64(h, item->cache_identity);
    h = gl_hash_u64(h, item->source.source_id);
    h = gl_hash_u64(h, item->source.geometry_id);
    h = gl_hash_u64(h, item->payload_revision);
    h = gl_hash_u64(h, item->geometry.source_identity);
    h = gl_hash_u64(h, item->geometry.revision);
    h = gl_hash_u64(h, item->geometry.kind);
    h = gl_hash_u64(h, (uint64_t)mode);
    h = gl_hash_u64(h, item->appearance.material_rev);
    h = gl_hash_u64(h, item->appearance.appearance_rev);
    h = gl_hash_u64(h, (uint64_t)GL_RETAINED_TRANSFORM_DEFERRED_TO_ITEM_MATRIX);
    return h ? h : 1;
}

static uint64_t
gl_backend_variant_identity(const struct bsg_render_item *item)
{
    uint64_t h = 1469598103934665603ULL;
    h = gl_hash_u64(h, BSG_BACKEND_GL);
    if (!item)
	return h;
    h = gl_hash_u64(h, item->context.backend_capabilities);
    h = gl_hash_u64(h, item->context.settings_hash);
    h = gl_hash_u64(h, item->context.lighting);
    h = gl_hash_u64(h, item->context.zclip);
    h = gl_hash_u64(h, item->context.has_projection);
    h = gl_hash_u64(h, item->context.request_flags);
    return h ? h : BSG_BACKEND_GL;
}

static uint64_t
gl_item_source_identity(const struct bsg_render_item *item)
{
    if (!item)
	return 0;
    if (item->source.source_id)
	return item->source.source_id;
    if (item->source.geometry_id)
	return item->source.geometry_id;
    return item->cache_identity;
}

static void
gl_backend_resource_free(struct dm *dmp, struct dm_backend_resource *resource)
{
    if (!resource)
	return;
    struct gl_backend_handle *h = (struct gl_backend_handle *)resource->handle;
    if (h) {
	if (h->command_list) {
	    if (dmp)
		gl_command_list_delete_enqueue(dmp, h->command_list);
	    else
		glDeleteLists(h->command_list, 1);
	}
	BU_PUT(h, struct gl_backend_handle);
	resource->handle = NULL;
    }
}

static struct gl_backend_handle *
gl_backend_handle_get(struct dm *dmp, const struct bsg_render_item *item, int mode, bool create)
{
    if (!dmp || !item || !item->cache_identity)
	return NULL;
    uint64_t resource_identity = gl_backend_resource_identity(item, mode);
    struct dm_backend_resource *r = dm_backend_resource_get(dmp,
	    resource_identity,
	    gl_item_source_identity(item),
	    gl_backend_variant_identity(item),
	    create,
	    gl_backend_resource_free);
    if (!r)
	return NULL;
    dm_backend_resource_touch(dmp, r);
    if (!r->handle && create) {
	struct gl_backend_handle *h;
	BU_GET(h, struct gl_backend_handle);
	h->command_list = 0;
	h->resource_key = resource_identity;
	r->handle = h;
    }
    return (struct gl_backend_handle *)r->handle;
}

extern "C" int gl_draw_item(struct dm *dmp, const struct bsg_render_item *item);

static void
gl_backend_invalidate_item(struct dm *dmp, const struct bsg_render_item *item, unsigned int UNUSED(reason_mask))
{
    if (!dmp || !item)
	return;
    dm_backend_resource_invalidate(dmp, gl_item_source_identity(item));
}

static void
gl_backend_release_source(struct dm *dmp, uint64_t source_identity)
{
    dm_backend_resource_release_source(dmp, source_identity);
}

static int
gl_backend_begin_frame(struct dm *UNUSED(dmp))
{
    return 0;
}

static void
gl_backend_end_frame(struct dm *dmp)
{
    dm_backend_resource_reclaim_unseen(dmp);
}

extern "C" const struct dm_backend_ops gl_backend_ops = {
    BSG_BACKEND_GL,
    gl_backend_begin_frame,
    gl_draw_item,
    gl_backend_invalidate_item,
    gl_backend_release_source,
    gl_backend_end_frame
};


// TODO - We can't currently compile really large meshes, as we won't be able
// to hold both the original data and the backend command resource in memory
// at the same time.  For that scenario, we would first need to break down the
// big mesh into smaller pieces as in the earlier LoD experiments.
static int
gl_draw_tri(struct dm *dmp, struct bsg_mesh_lod *lod, const struct bsg_render_item *item, const struct bsg_resolved_appearance *appearance)
{
    int fcnt = lod->fcnt;
    int pcnt = lod->pcnt;
    const int *faces = lod->faces;
    const point_t *points = lod->points;
    const point_t *points_orig = lod->points_orig;
    const vect_t *normals = lod->normals;
    int mode = appearance ? appearance->draw_mode : 0;
    mat_t save_mat, draw_mat;

    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    GLdouble dpt[3];
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalLineWidth;

    if (mode < 0 || mode > 1)
	return BRLCAD_ERROR;

    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    gl_debug_print(dmp, "gl_draw_tri", dmp->i->dm_debugLevel);

    struct gl_backend_handle *h = item ? gl_backend_handle_get(dmp, item, mode, false) : NULL;

    // We don't want color to be part of the backend command resource, to allow the app
    // to change it without regeneration - hence, we need to do it
	// up front
    if (mode == 0) {
	if (appearance && appearance->highlighted) {
	    fastf_t transparency = appearance->transparency;
	    dm_set_fg(dmp, 255, 255, 255, 0, transparency);
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
	if (appearance && appearance->highlighted) {
	    fastf_t transparency = appearance->transparency;
	    dm_set_fg(dmp, 255, 255, 255, 0, transparency);
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

    if (h && h->command_list) {
	if (!gl_item_draw_matrix(item, save_mat, draw_mat))
	    return BRLCAD_ERROR;
	dm_loadmatrix(dmp, draw_mat, 0);
	glCallList(h->command_list);
	dm_loadmatrix(dmp, save_mat, 0);
	glLineWidth(originalLineWidth);
	if (mvars->transparency_on)
	    glDisable(GL_BLEND);
	return BRLCAD_OK;
    }

    // Figure out if we should compile a backend command resource.  Compilation
    // is faster on repeated frames but temporarily increases memory pressure,
    // so large one-off meshes use direct retained-item drawing.
    ssize_t avail_mem = 0.5*bu_mem(BU_MEM_AVAIL, NULL);
    size_t size_est = (size_t)(fcnt*3*sizeof(point_t));
    bool compile_resource = false;
    bool direct_matrix_loaded = false;
    if (item && (!h || !h->command_list) && avail_mem > 0 && size_est < (size_t)avail_mem) {
	compile_resource = true;
	if (!h)
	    h = gl_backend_handle_get(dmp, item, mode, true);
	if (h)
	    h->command_list = glGenLists(1);
	if (h && h->command_list)
	    glNewList(h->command_list, GL_COMPILE);
	else
	    compile_resource = false;
    }
    if (!compile_resource) {
	if (!gl_item_draw_matrix(item, save_mat, draw_mat))
	    return BRLCAD_ERROR;
	dm_loadmatrix(dmp, draw_mat, 0);
	direct_matrix_loaded = true;
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
	if (compile_resource) {
	    glEndList();
	    if (size_est > (avail_mem * 0.01)) {
		// If the original data is sizable, future LoD work can clear it
		// after the backend resource has what it needs.
	    }

	    if (!gl_item_draw_matrix(item, save_mat, draw_mat))
		return BRLCAD_ERROR;
	    dm_loadmatrix(dmp, draw_mat, 0);
	    glCallList(h->command_list);
	    dm_loadmatrix(dmp, save_mat, 0);
	}

	glLineWidth(originalLineWidth);

	if (mvars->transparency_on)
	    glDisable(GL_BLEND);

	if (direct_matrix_loaded)
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

	if (compile_resource) {
	    glEndList();
	    if (size_est > (avail_mem * 0.01)) {
		// If the original data is sizable, future LoD work can clear it
		// after the backend resource has what it needs.
	    }

	    if (!gl_item_draw_matrix(item, save_mat, draw_mat))
		return BRLCAD_ERROR;
	    dm_loadmatrix(dmp, draw_mat, 0);
	    glCallList(h->command_list);
	    dm_loadmatrix(dmp, save_mat, 0);
	}

	if (mvars->lighting_on && mvars->transparency_on)
	    glDisable(GL_BLEND);

	// Put the lighting model back where it was prior to this operation
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, two_sided);

	glLineWidth(originalLineWidth);

	if (direct_matrix_loaded)
	    dm_loadmatrix(dmp, save_mat, 0);

	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

static int
gl_draw_item_resolved(struct dm *dmp, const struct bsg_render_item *item, const struct bsg_resolved_appearance *appearance)
{
    if (!item)
	return BRLCAD_ERROR;
    int mode = appearance ? appearance->draw_mode : 0;

    gl_command_list_delete_flush(dmp);

    const struct bsg_mesh_lod *lod = NULL;
    if (item) {
	if (item->geometry.kind == BSG_RENDER_GEOMETRY_MESH)
	    lod = item->geometry.data.mesh;
    }
    if (item->geometry.kind == BSG_RENDER_GEOMETRY_MESH && lod) {
	return gl_draw_tri(dmp, (struct bsg_mesh_lod *)lod, item, appearance);
    }

    if (item->geometry.kind == BSG_RENDER_GEOMETRY_LINE_SET) {
	if (item->geometry.arrays.point_count && item->geometry.arrays.points)
	    return gl_draw_line_arrays_retained_immediate(dmp,
		    (const point_t *)item->geometry.arrays.points,
		    item->geometry.arrays.commands,
		    item->geometry.arrays.point_count,
		    item->geometry.arrays.command_count);
	return BRLCAD_OK;
    }

    if (item->geometry.kind == BSG_RENDER_GEOMETRY_INDEXED_LINE_SET) {
	if (item->geometry.arrays.point_count && item->geometry.arrays.points &&
		item->geometry.arrays.index_count && item->geometry.arrays.indices)
	    return gl_draw_indexed_line_arrays_retained_immediate(dmp,
		    (const point_t *)item->geometry.arrays.points,
		    item->geometry.arrays.point_count,
		    item->geometry.arrays.indices,
		    item->geometry.arrays.index_count);
	return BRLCAD_OK;
    }

    if (item->geometry.kind == BSG_RENDER_GEOMETRY_INDEXED_FACE_SET) {
	const struct bsg_render_geometry *geometry = &item->geometry;
	if (geometry->surface.point_count && geometry->surface.points &&
		geometry->surface.index_count && geometry->surface.indices) {
	    if (mode == 0)
		return gl_draw_indexed_face_arrays_wire_retained_immediate(dmp,
			item,
			appearance,
			(const point_t *)geometry->surface.points,
			geometry->surface.point_count,
			geometry->surface.indices,
			geometry->surface.index_count);
	    if (mode == 4)
		return gl_draw_indexed_face_arrays_hidden_line_retained(dmp,
			item,
			(const point_t *)geometry->surface.points,
			geometry->surface.point_count,
			geometry->surface.indices,
			geometry->surface.index_count,
			(const point_t *)geometry->surface.normals,
			geometry->surface.normal_count,
			geometry->surface.normal_kind);
	    return gl_draw_indexed_face_arrays_retained_immediate(dmp,
		    item,
		    appearance,
		    (const point_t *)geometry->surface.points,
		    geometry->surface.point_count,
		    geometry->surface.indices,
		    geometry->surface.index_count,
		    (const point_t *)geometry->surface.normals,
		    geometry->surface.normal_count,
		    geometry->surface.normal_kind);
	}
	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

extern "C"
int gl_draw_item(struct dm *dmp, const struct bsg_render_item *item)
{
    if (!item || item->geometry.kind == BSG_RENDER_GEOMETRY_NONE)
	return BRLCAD_ERROR;
    return gl_draw_item_resolved(dmp, item, &item->appearance);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
