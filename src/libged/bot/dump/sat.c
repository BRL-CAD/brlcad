/*                            S A T . C
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
/** @file libged/bot/dump/sat.c
 *
 * bot_dump SAT format specific logic.
 *
 */

#include "common.h"

#include <string.h>
#include <time.h>

#include "brlcad_version.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "./ged_bot_dump.h"


void
sat_write_header(FILE *fp)
{
    time_t now;

    /* SAT header consists of three lines:
     *
     * 1: SAT_version num_records num_objects history_boolean
     * 2: strlen product_id_str strlen version_str strlen date_str
     * 3: cnv_to_mm resabs_value resnor_value
     *
     * When num_records is zero, it looks for an end marker.
     */
    fprintf(fp, "400 0 1 0\n");

    time(&now);
    fprintf(fp, "%ld BRL-CAD(%s)-bot_dump 16 ACIS 8.0 Unknown %ld %s",
	    (long)strlen(brlcad_version())+18, brlcad_version(), (long)strlen(ctime(&now)) - 1, ctime(&now));

    /* FIXME: this includes abs tolerance info, should probably output ours */
    fprintf(fp, "1 9.9999999999999995e-007 1e-010\n");
}

int
sat_setup(struct _ged_bot_dump_client_data *d, const char *fname)
{
    if (!d)
	return BRLCAD_ERROR;

    struct ged *gedp = d->gedp;

    d->fp = fopen(fname, "wb+");
    if (d->fp == NULL) {
	perror("sat_setup");
	bu_vls_printf(gedp->ged_result_str, "Cannot open SAT ascii output file (%s) for writing\n", fname);
	return BRLCAD_ERROR;
    }

    sat_write_header(d->fp);

    return BRLCAD_OK;
}


int
sat_finish(struct _ged_bot_dump_client_data *d)
{
    if (!d)
	return BRLCAD_ERROR;

    fprintf(d->fp, "End-of-ACIS-data\n");
    fclose(d->fp);

    return BRLCAD_OK;
}

void
sat_write_bot(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, FILE *fp, char *UNUSED(name))
{
    int i, j;
    fastf_t *vertices;
    int *faces;
    int first_vertex;
    int first_coedge;
    int first_face;
    int num_vertices = bot->num_vertices;
    int num_faces = bot->num_faces;

    vertices = bot->vertices;
    faces = bot->faces;

    d->sat.curr_body_id = d->sat.curr_line_num;
    d->sat.curr_lump_id = d->sat.curr_body_id + 1;
    d->sat.curr_shell_id = d->sat.curr_lump_id + 1;
    d->sat.curr_face_id = d->sat.curr_shell_id + 1 + num_vertices*2 + num_faces*6;

    fprintf(fp, "-%d body $-1 $%d $-1 $-1 #\n", d->sat.curr_body_id, d->sat.curr_lump_id);
    fprintf(fp, "-%d lump $-1 $-1 $%d $%d #\n", d->sat.curr_lump_id, d->sat.curr_shell_id, d->sat.curr_body_id);
    fprintf(fp, "-%d shell $-1 $-1 $-1 $%d $-1 $%d #\n", d->sat.curr_shell_id, d->sat.curr_face_id, d->sat.curr_lump_id);

    d->sat.curr_line_num += 3;

    /* Dump out vertices */
    first_vertex = d->sat.curr_line_num;
    for (i = 0; i < num_vertices; i++) {
	d->sat.curr_edge_id = -1;
	for (j = 0; j < num_faces; j++) {
	    if (faces[3*j]+first_vertex == d->sat.curr_line_num) {
		d->sat.curr_edge_id = first_vertex + num_vertices*2 + num_faces*3 + j*3;
		break;
	    } else if (faces[3*j+1]+first_vertex == d->sat.curr_line_num) {
		d->sat.curr_edge_id = first_vertex + num_vertices*2 + num_faces*3 + j*3 + 1;
		break;
	    } else if (faces[3*j+2]+first_vertex == d->sat.curr_line_num) {
		d->sat.curr_edge_id = first_vertex + num_vertices*2 + num_faces*3 + j*3 + 2;
		break;
	    }
	}

	fprintf(fp, "-%d vertex $-1 $%d $%d #\n", d->sat.curr_line_num, d->sat.curr_edge_id, d->sat.curr_line_num+num_vertices);
	++d->sat.curr_line_num;
    }

    /* Dump out points */
    for (i = 0; i < num_vertices; i++) {
	fprintf(fp, "-%d point $-1 %f %f %f #\n", d->sat.curr_line_num, V3ARGS_SCALE(&vertices[3*i]));
	++d->sat.curr_line_num;
    }

    /* Dump out coedges */
    first_coedge = d->sat.curr_line_num;
    d->sat.curr_loop_id = first_coedge+num_faces*7;
    for (i = 0; i < num_faces; i++) {
	fprintf(fp, "-%d coedge $-1 $%d $%d $%d $%d forward $%d $-1 #\n",
		d->sat.curr_line_num, d->sat.curr_line_num+1, d->sat.curr_line_num+2, d->sat.curr_line_num,
		d->sat.curr_line_num+num_faces*3, d->sat.curr_loop_id);
	++d->sat.curr_line_num;
	fprintf(fp, "-%d coedge $-1 $%d $%d $%d $%d forward $%d $-1 #\n",
		d->sat.curr_line_num, d->sat.curr_line_num+1, d->sat.curr_line_num-1, d->sat.curr_line_num,
		d->sat.curr_line_num+num_faces*3, d->sat.curr_loop_id);
	++d->sat.curr_line_num;
	fprintf(fp, "-%d coedge $-1 $%d $%d $%d $%d forward $%d $-1 #\n",
		d->sat.curr_line_num, d->sat.curr_line_num-2, d->sat.curr_line_num-1, d->sat.curr_line_num,
		d->sat.curr_line_num+num_faces*3,  d->sat.curr_loop_id);
	++d->sat.curr_line_num;
	++d->sat.curr_loop_id;
    }

    /* Dump out edges */
    for (i = 0; i < num_faces; i++) {
	fprintf(fp, "-%d edge $-1 $%d $%d $%d $%d forward #\n", d->sat.curr_line_num,
		faces[3*i]+first_vertex, faces[3*i+1]+first_vertex,
		first_coedge + i*3, d->sat.curr_line_num + num_faces*5);
	++d->sat.curr_line_num;
	fprintf(fp, "-%d edge $-1 $%d $%d $%d $%d forward #\n", d->sat.curr_line_num,
		faces[3*i+1]+first_vertex, faces[3*i+2]+first_vertex,
		first_coedge + i*3 + 1, d->sat.curr_line_num + num_faces*5);
	++d->sat.curr_line_num;
	fprintf(fp, "-%d edge $-1 $%d $%d $%d $%d forward #\n", d->sat.curr_line_num,
		faces[3*i+2]+first_vertex, faces[3*i]+first_vertex,
		first_coedge + i*3 + 2, d->sat.curr_line_num + num_faces*5);
	++d->sat.curr_line_num;
    }

    /* Dump out faces */
    first_face = d->sat.curr_line_num;
    for (i = 0; i < num_faces-1; i++) {
	fprintf(fp, "-%d face $-1 $%d $%d $%d $-1 $%d forward single #\n",
		d->sat.curr_line_num, d->sat.curr_line_num+1, d->sat.curr_line_num+num_faces,
		d->sat.curr_shell_id, d->sat.curr_line_num + num_faces*5);
	++d->sat.curr_line_num;
    }
    fprintf(fp, "-%d face $-1 $-1 $%d $%d $-1 $%d forward single #\n",
	    d->sat.curr_line_num, d->sat.curr_line_num+num_faces, d->sat.curr_shell_id,
	    d->sat.curr_line_num + num_faces*5);
    ++d->sat.curr_line_num;

    /* Dump out loops */
    for (i = 0; i < num_faces; i++) {
	fprintf(fp, "-%d loop $-1 $-1 $%d $%d #\n",
		d->sat.curr_line_num, first_coedge+i*3, first_face+i);
	++d->sat.curr_line_num;
    }

    /* Dump out straight-curves for each edge */
    for (i = 0; i < num_faces; i++) {
	point_t A;
	point_t B;
	point_t C;
	vect_t BmA;
	vect_t CmB;
	vect_t AmC;
	int vi;

	vi = 3*faces[3*i];
	VSET(A, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+1];
	VSET(B, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+2];
	VSET(C, vertices[vi], vertices[vi+1], vertices[vi+2]);
	VSUB2(BmA, B, A);
	VSUB2(CmB, C, B);
	VSUB2(AmC, A, C);
	VUNITIZE(BmA);
	VUNITIZE(CmB);
	VUNITIZE(AmC);

	fprintf(fp, "-%d straight-curve $-1 %f %f %f %f %f %f I I #\n", d->sat.curr_line_num, V3ARGS_SCALE(A), V3ARGS(BmA));
	++d->sat.curr_line_num;
	fprintf(fp, "-%d straight-curve $-1 %f %f %f %f %f %f I I #\n", d->sat.curr_line_num, V3ARGS_SCALE(B), V3ARGS(CmB));
	++d->sat.curr_line_num;
	fprintf(fp, "-%d straight-curve $-1 %f %f %f %f %f %f I I #\n", d->sat.curr_line_num, V3ARGS_SCALE(C), V3ARGS(AmC));
	++d->sat.curr_line_num;
    }

    /* Dump out plane-surfaces for each face */
    for (i = 0; i < num_faces; i++) {
	point_t A;
	point_t B;
	point_t C;
	point_t center;
	vect_t BmA;
	vect_t CmA;
	vect_t norm;
	int vi;
	fastf_t sf = 1.0/3.0;

	vi = 3*faces[3*i];
	VSET(A, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+1];
	VSET(B, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+2];
	VSET(C, vertices[vi], vertices[vi+1], vertices[vi+2]);

	VADD3(center, A, B, C);
	VSCALE(center, center, sf);

	VSUB2(BmA, B, A);
	VSUB2(CmA, C, A);
	if (bot->orientation != RT_BOT_CW) {
	    VCROSS(norm, BmA, CmA);
	} else {
	    VCROSS(norm, CmA, BmA);
	}
	VUNITIZE(norm);

	VUNITIZE(BmA);

	fprintf(fp, "-%d plane-surface $-1 %f %f %f %f %f %f %f %f %f forward_v I I I I #\n",
		d->sat.curr_line_num, V3ARGS_SCALE(A), V3ARGS(norm), V3ARGS(BmA));

	++d->sat.curr_line_num;
    }
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
