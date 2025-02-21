/*                            O B J . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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

#include "./ged_bot_dump.h"

static struct _ged_obj_material *
obj_get_material(struct bot_dump_obj *o, int red, int green, int blue, fastf_t transparency)
{
    struct _ged_obj_material *gomp;

    for (BU_LIST_FOR(gomp, _ged_obj_material, &o->HeadObjMaterials)) {
	if (gomp->r == red &&
	    gomp->g == green &&
	    gomp->b == blue &&
	    ZERO(gomp->a - transparency)) {
	    return gomp;
	}
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

    if (d->using_dbot_dump) {
	gomp = obj_get_material(&d->obj, d->obj.curr_obj_red,
				    d->obj.curr_obj_green,
				    d->obj.curr_obj_blue,
				    d->obj.curr_obj_alpha);
	fprintf(fp, "usemtl %s\n", bu_vls_addr(&gomp->name));
    }

    num_vertices = bot->num_vertices;
    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    fprintf(fp, "g %s\n", name);

    for (i = 0; i < num_vertices; i++) {
	fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(&vertices[3*i]));
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

	    fprintf(fp, "vn %f %f %f\n", V3ARGS(norm));
	}
    }

    if (d->normals) {
	for (i = 0; i < num_faces; i++) {
	    fprintf(fp, "f %d//%d %d//%d %d//%d\n", faces[3*i]+d->v_offset, i+1, faces[3*i+1]+d->v_offset, i+1, faces[3*i+2]+d->v_offset, i+1);
	}
    } else {
	for (i = 0; i < num_faces; i++) {
	    fprintf(fp, "f %d %d %d\n", faces[3*i]+d->v_offset, faces[3*i+1]+d->v_offset, faces[3*i+2]+d->v_offset);
	}
    }

    d->v_offset += num_vertices;
}


static void
write_data_arrows(struct _ged_bot_dump_client_data *d, struct bv_data_arrow_state *gdasp, FILE *fp, int sflag)
{
    register int i;

    if (gdasp->gdas_draw) {
	struct _ged_obj_material *gomp;

	gomp = obj_get_material(&d->obj, gdasp->gdas_color[0],
				    gdasp->gdas_color[1],
				    gdasp->gdas_color[2],
				    1);
	fprintf(fp, "usemtl %s\n", bu_vls_addr(&gomp->name));

	if (sflag)
	    fprintf(fp, "g sdata_arrows\n");
	else
	    fprintf(fp, "g data_arrows\n");

	for (i = 0; i < gdasp->gdas_num_points; i += 2) {
	    point_t A, B;
	    point_t BmA;
	    point_t offset;
	    point_t perp1, perp2;
	    point_t a_base;
	    point_t a_pt1, a_pt2, a_pt3, a_pt4;

	    VMOVE(A, gdasp->gdas_points[i]);
	    VMOVE(B, gdasp->gdas_points[i+1]);

	    VSUB2(BmA, B, A);

	    VUNITIZE(BmA);
	    VSCALE(offset, BmA, -gdasp->gdas_tip_length);

	    bn_vec_perp(perp1, BmA);
	    VUNITIZE(perp1);

	    VCROSS(perp2, BmA, perp1);
	    VUNITIZE(perp2);

	    VSCALE(perp1, perp1, gdasp->gdas_tip_width);
	    VSCALE(perp2, perp2, gdasp->gdas_tip_width);

	    VADD2(a_base, B, offset);
	    VADD2(a_pt1, a_base, perp1);
	    VADD2(a_pt2, a_base, perp2);
	    VSUB2(a_pt3, a_base, perp1);
	    VSUB2(a_pt4, a_base, perp2);

	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(A));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(B));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(a_pt1));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(a_pt2));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(a_pt3));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(a_pt4));
	}

	for (i = 0; i < gdasp->gdas_num_points; i += 2) {
	    fprintf(fp, "l %d %d\n", (i/2*6)+d->v_offset, (i/2*6)+d->v_offset+1);
	    fprintf(fp, "l %d %d\n", (i/2*6)+d->v_offset+1, (i/2*6)+d->v_offset+2);
	    fprintf(fp, "l %d %d\n", (i/2*6)+d->v_offset+1, (i/2*6)+d->v_offset+3);
	    fprintf(fp, "l %d %d\n", (i/2*6)+d->v_offset+1, (i/2*6)+d->v_offset+4);
	    fprintf(fp, "l %d %d\n", (i/2*6)+d->v_offset+1, (i/2*6)+d->v_offset+5);
	    fprintf(fp, "l %d %d\n", (i/2*6)+d->v_offset+2, (i/2*6)+d->v_offset+3);
	    fprintf(fp, "l %d %d\n", (i/2*6)+d->v_offset+3, (i/2*6)+d->v_offset+4);
	    fprintf(fp, "l %d %d\n", (i/2*6)+d->v_offset+4, (i/2*6)+d->v_offset+5);
	    fprintf(fp, "l %d %d\n", (i/2*6)+d->v_offset+5, (i/2*6)+d->v_offset+2);
	}

	d->v_offset += ((gdasp->gdas_num_points/2)*6);
    }
}


static void
write_data_axes(struct _ged_bot_dump_client_data *d, struct bv_data_axes_state *bndasp, FILE *fp, int sflag)
{
    register int i;

    if (bndasp->draw) {
	fastf_t halfAxesSize;
	struct _ged_obj_material *gomp;

	halfAxesSize = bndasp->size * 0.5;

	gomp = obj_get_material(&d->obj, bndasp->color[0],
				    bndasp->color[1],
				    bndasp->color[2],
				    1);
	fprintf(fp, "usemtl %s\n", bu_vls_addr(&gomp->name));

	if (sflag)
	    fprintf(fp, "g sdata_axes\n");
	else
	    fprintf(fp, "g data_axes\n");

	for (i = 0; i < bndasp->num_points; ++i) {
	    point_t A, B;

	    /* draw X axis with x/y offsets */
	    VSET(A,
		 bndasp->points[i][X] - halfAxesSize,
		 bndasp->points[i][Y],
		 bndasp->points[i][Z]);
	    VSET(B,
		 bndasp->points[i][X] + halfAxesSize,
		 bndasp->points[i][Y],
		 bndasp->points[i][Z]);

	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(A));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(B));

	    /* draw Y axis with x/y offsets */
	    VSET(A,
		 bndasp->points[i][X],
		 bndasp->points[i][Y] - halfAxesSize,
		 bndasp->points[i][Z]);
	    VSET(B,
		 bndasp->points[i][X],
		 bndasp->points[i][Y] + halfAxesSize,
		 bndasp->points[i][Z]);

	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(A));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(B));

	    /* draw Z axis with x/y offsets */
	    VSET(A,
		 bndasp->points[i][X],
		 bndasp->points[i][Y],
		 bndasp->points[i][Z] - halfAxesSize);
	    VSET(B,
		 bndasp->points[i][X],
		 bndasp->points[i][Y],
		 bndasp->points[i][Z] + halfAxesSize);

	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(A));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(B));
	}

	for (i = 0; i < bndasp->num_points; ++i) {
	    fprintf(fp, "l %d %d\n", (i*6)+d->v_offset, (i*6)+d->v_offset+1);
	    fprintf(fp, "l %d %d\n", (i*6)+d->v_offset+2, (i*6)+d->v_offset+3);
	    fprintf(fp, "l %d %d\n", (i*6)+d->v_offset+4, (i*6)+d->v_offset+5);
	}


	d->v_offset += (bndasp->num_points*6);
    }
}


static void
write_data_lines(struct _ged_bot_dump_client_data *d, struct bv_data_line_state *gdlsp, FILE *fp, int sflag)
{
    register int i;

    if (gdlsp->gdls_draw) {
	struct _ged_obj_material *gomp;

	gomp = obj_get_material(&d->obj, gdlsp->gdls_color[0],
				    gdlsp->gdls_color[1],
				    gdlsp->gdls_color[2],
				    1);
	fprintf(fp, "usemtl %s\n", bu_vls_addr(&gomp->name));

	if (sflag)
	    fprintf(fp, "g sdata_lines\n");
	else
	    fprintf(fp, "g data_lines\n");

	for (i = 0; i < gdlsp->gdls_num_points; i += 2) {
	    point_t A, B;

	    VMOVE(A, gdlsp->gdls_points[i]);
	    VMOVE(B, gdlsp->gdls_points[i+1]);

	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(A));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(B));
	}

	for (i = 0; i < gdlsp->gdls_num_points; i += 2) {
	    fprintf(fp, "l %d %d\n", i+d->v_offset, i+d->v_offset+1);
	}

	d->v_offset += gdlsp->gdls_num_points;
    }
}


void
obj_write_data(struct _ged_bot_dump_client_data *d, struct ged *gedp, FILE *fp)
{
    write_data_arrows(d, &gedp->ged_gvp->gv_tcl.gv_data_arrows, fp, 0);
    write_data_arrows(d, &gedp->ged_gvp->gv_tcl.gv_sdata_arrows, fp, 1);

    write_data_axes(d, &gedp->ged_gvp->gv_tcl.gv_data_axes, fp, 0);
    write_data_axes(d, &gedp->ged_gvp->gv_tcl.gv_sdata_axes, fp, 1);

    write_data_lines(d, &gedp->ged_gvp->gv_tcl.gv_data_lines, fp, 0);
    write_data_lines(d, &gedp->ged_gvp->gv_tcl.gv_sdata_lines, fp, 1);
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
