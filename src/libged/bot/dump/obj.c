/*                            O B J . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/bot/dump/obj.c
 *
 * bot_dump OBJ format specific logic.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/path.h"

#include "./ged_bot_dump.h"

static struct _ged_obj_material *
obj_get_material(struct _ged_bot_dump_client_data *d, FILE *fp, int red, int green, int blue, fastf_t transparency)
{
    struct bot_dump_obj *o = &d->obj;
    struct _ged_obj_material *gomp;

    for (BU_LIST_FOR(gomp, _ged_obj_material, &o->HeadObjMaterials)) {
	if (gomp->r == red &&
	    gomp->g == green &&
	    gomp->b == blue &&
	    ZERO(gomp->a - transparency)) {
	    return gomp;
	}
    }

    if (!o->obj_materials_fp) {
	o->obj_materials_fp = fopen(bu_vls_cstr(&o->obj_materials_file), "wb+");
	if (o->obj_materials_fp == NULL) {
	    bu_vls_printf(d->gedp->ged_result_str, "obj_get_material: failed to open %s\n", bu_vls_cstr(&o->obj_materials_file));
	    return NULL;
	}
    }

    if (fp && !o->mtllib_written) {
	/* The mtllib directive must reference the material file by a name
	 * resolvable relative to the .obj file (the .mtl is written
	 * alongside it), not by the absolute output path.  Emitting the
	 * absolute path produced non-portable .obj files that embedded the
	 * build machine's directory layout.  Use just the basename. */
	char mtl_base[MAXPATHLEN] = {0};
	bu_path_basename(bu_vls_cstr(&o->obj_materials_file), mtl_base);
	fprintf(fp, "mtllib %s\n", mtl_base);
	o->mtllib_written = 1;
    }

    BU_GET(gomp, struct _ged_obj_material);
    BU_LIST_APPEND(&o->HeadObjMaterials, &gomp->l);
    gomp->r = red;
    gomp->g = green;
    gomp->b = blue;
    gomp->a = transparency;
    bu_vls_init(&gomp->name);
    bu_vls_printf(&gomp->name, "matl_%d", ++o->num_obj_materials);

    /* Write out newmtl to mtl file */
    fprintf(o->obj_materials_fp, "newmtl %s\n", bu_vls_addr(&gomp->name));
    fprintf(o->obj_materials_fp, "Kd %f %f %f\n",
	    (fastf_t)gomp->r / 255.0,
	    (fastf_t)gomp->g / 255.0,
	    (fastf_t)gomp->b / 255.0);
    fprintf(o->obj_materials_fp, "d %f\n", gomp->a);
    fprintf(o->obj_materials_fp, "illum 1\n");

    return gomp;
}

int
obj_setup(struct _ged_bot_dump_client_data *d, const char *fname, int mtl_only)
{
    if (!d)
	return BRLCAD_ERROR;

    struct ged *gedp = d->gedp;

    if (!mtl_only) {
	d->fp = fopen(fname, "wb+");
	if (d->fp == NULL) {
	    perror("obj_setup");
	    bu_vls_printf(gedp->ged_result_str, "Cannot open OBJ ascii output file (%s) for writing\n", fname);
	    return BRLCAD_ERROR;
	}
    }

    if (d->material_info) {
	char *cp;

	bu_vls_init(&d->obj.obj_materials_file);

	bu_vls_trunc(&d->obj.obj_materials_file, 0);

	cp = strrchr(fname, '.');
	if (!cp)
	    bu_vls_printf(&d->obj.obj_materials_file, "%s.mtl", fname);
	else {
	    /* ignore everything after the last '.' */
	    *cp = '\0';
	    bu_vls_printf(&d->obj.obj_materials_file, "%s.mtl", fname);
	    *cp = '.';
	}

	BU_LIST_INIT(&d->obj.HeadObjMaterials);
	d->obj.obj_materials_fp = NULL;

	d->obj.num_obj_materials = 0;
	d->obj.mtllib_written = 0;
    }

    d->obj.v_offset = 1;

    return BRLCAD_OK;
}


int
obj_finish(struct _ged_bot_dump_client_data *d)
{
    if (!d)
	return BRLCAD_ERROR;

    if (!bu_vls_strlen(&d->output_directory))
	fclose(d->fp);

    if (d->material_info) {
	bu_vls_free(&d->obj.obj_materials_file);
	obj_free_materials(&d->obj);
	if (d->obj.obj_materials_fp)
	    fclose(d->obj.obj_materials_fp);
    }

    return BRLCAD_OK;
}


void
obj_free_materials(struct bot_dump_obj *o)
{
    struct _ged_obj_material *gomp;

    while (BU_LIST_WHILE(gomp, _ged_obj_material, &o->HeadObjMaterials)) {
	BU_LIST_DEQUEUE(&gomp->l);
	bu_vls_free(&gomp->name);
	BU_PUT(gomp, struct _ged_obj_material);
    }
}

void
obj_write_bot(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, FILE *fp, char *name)
{
    int num_vertices;
    fastf_t *vertices;
    int num_faces, *faces;
    point_t A;
    point_t B;
    point_t C;
    vect_t BmA;
    vect_t CmA;
    vect_t norm;
    int i, vi;
    struct _ged_obj_material *gomp;

    if (d->curr_obj_color_valid) {
	gomp = obj_get_material(d, fp, d->curr_obj_red,
				    d->curr_obj_green,
				    d->curr_obj_blue,
				    d->curr_obj_alpha);
	if (!gomp)
	    return;
	fprintf(fp, "usemtl %s\n", bu_vls_addr(&gomp->name));
    }

    num_vertices = bot->num_vertices;
    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    fprintf(fp, "g %s\n", name);

    for (i = 0; i < num_vertices; i++) {
	if (d->full_precision) {
	    fprintf(fp, "v %0.17f %0.17f %0.17f\n", V3ARGS_SCALE(&vertices[3*i]));
	} else {
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(&vertices[3*i]));
	}
    }

    if (d->normals) {
	for (i = 0; i < num_faces; i++) {
	    vi = 3*faces[3*i];
	    VSET(A, vertices[vi], vertices[vi+1], vertices[vi+2]);
	    vi = 3*faces[3*i+1];
	    VSET(B, vertices[vi], vertices[vi+1], vertices[vi+2]);
	    vi = 3*faces[3*i+2];
	    VSET(C, vertices[vi], vertices[vi+1], vertices[vi+2]);

	    VSUB2(BmA, B, A);
	    VSUB2(CmA, C, A);
	    if (bot->orientation != RT_BOT_CW) {
		VCROSS(norm, BmA, CmA);
	    } else {
		VCROSS(norm, CmA, BmA);
	    }
	    VUNITIZE(norm);

	    if (d->full_precision) {
		fprintf(fp, "vn %0.17f %0.17f %0.17f\n", V3ARGS(norm));
	    } else {
		fprintf(fp, "vn %f %f %f\n", V3ARGS(norm));
	    }
	}
    }

    if (d->normals) {
	for (i = 0; i < num_faces; i++) {
	    fprintf(fp, "f %d//%d %d//%d %d//%d\n", faces[3*i]+d->obj.v_offset, i+1, faces[3*i+1]+d->obj.v_offset, i+1, faces[3*i+2]+d->obj.v_offset, i+1);
	}
    } else {
	for (i = 0; i < num_faces; i++) {
	    fprintf(fp, "f %d %d %d\n", faces[3*i]+d->obj.v_offset, faces[3*i+1]+d->obj.v_offset, faces[3*i+2]+d->obj.v_offset);
	}
    }

    d->obj.v_offset += num_vertices;
}


/* Phase T-final (drawing_stack_modernization): obj_write_data formerly
 * serialized the gv_tcl-resident data_arrows / data_axes / data_lines
 * overlay state into the OBJ file alongside the BoT geometry.  After T1
 * those overlays live in BSG VIEW_SCOPE objects (`_tcl_data_arrows`,
 * `_tcl_data_axes`, `_tcl_data_lines` and their sdata twins), and BSG is
 * the source of truth.  The legacy gv_tcl-coupled exporter has been
 * removed; obj_write_data is now a no-op stub.  A future replacement
 * could iterate `_tcl_data_*` BSG vlists if this OBJ-side overlay
 * bake-out is ever needed again. */
void
obj_write_data(struct _ged_bot_dump_client_data *d, struct ged *gedp, FILE *fp)
{
    (void)d;
    (void)gedp;
    (void)fp;
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
