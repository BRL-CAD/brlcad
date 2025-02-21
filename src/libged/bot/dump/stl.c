/*                            S T L . C
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
/** @file libged/bot/dump/stl.c
 *
 * bot_dump STL format specific logic.
 *
 */

#include "common.h"

#include <string.h>

#include "./ged_bot_dump.h"

/* Byte swaps a four byte value */
static void
lswap(unsigned int *v)
{
    unsigned int r;

    r =*v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}

int
stl_setup(struct _ged_bot_dump_client_data *d, const char *fname)
{
    int ret = BRLCAD_OK;

    if (!d)
	return BRLCAD_ERROR;

    struct ged *gedp = d->gedp;

    if (d->binary) {
	char buf[81];       /* need exactly 80 chars for header */

	/* Open binary output file */
	d->fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (d->fd < 0) {
	    perror("stl_setup open");
	    bu_vls_printf(gedp->ged_result_str, "Cannot open STL binary output file (%s) for writing\n", fname);
	    return BRLCAD_ERROR;
	}

	/* Write out STL header if output file is binary */
	memset(buf, 0, sizeof(buf));
	bu_strlcpy(buf, "BRL-CAD generated STL FILE", sizeof(buf));
	ret = write(d->fd, &buf, 80);
	if (ret < 0) {
	    perror("write");
	}

	/* write a place keeper for the number of triangles */
	memset(buf, 0, 4);
	ret = write(d->fd, &buf, 4);
	if (ret < 0) {
	    perror("write");
	}
    } else {
	d->fp = fopen(fname, "wb+");
	if (d->fp == NULL) {
	    perror("stl_setup fopen");
	    bu_vls_printf(gedp->ged_result_str, "Cannot open STL ascii output file (%s) for writing\n", fname);
	    return BRLCAD_ERROR;
	}
    }

    return (ret != BRLCAD_OK) ? BRLCAD_ERROR : BRLCAD_OK;
}

int
stl_finish(struct _ged_bot_dump_client_data *d)
{
    int ret = BRLCAD_OK;

    if (!d)
	return BRLCAD_ERROR;

    if (d->binary) {
	unsigned char tot_buffer[4];

	/* Re-position pointer to 80th byte */
	bu_lseek(d->fd, 80, SEEK_SET);

	/* Write out number of triangles */
	*(uint32_t *)tot_buffer = htonl((unsigned long)d->total_faces);
	lswap((unsigned int *)tot_buffer);
	ret = write(d->fd, tot_buffer, 4);
	if (ret < 0) {
	    perror("write");
	}

	close(d->fd);
    } else {
	fclose(d->fp);
    }

    return (ret != BRLCAD_OK) ? BRLCAD_ERROR : BRLCAD_OK;
}

void
stl_write_bot(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, FILE *fp, char *name)
{
    fastf_t *vertices;
    int num_faces, *faces;
    point_t A;
    point_t B;
    point_t C;
    vect_t BmA;
    vect_t CmA;
    vect_t norm;
    int i, vi;

    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    fprintf(fp, "solid %s\n", name);
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

	fprintf(fp, "  facet normal %f %f %f\n", V3ARGS(norm));
	fprintf(fp, "    outer loop\n");
	fprintf(fp, "      vertex %f %f %f\n", V3ARGS_SCALE(A));
	fprintf(fp, "      vertex %f %f %f\n", V3ARGS_SCALE(B));
	fprintf(fp, "      vertex %f %f %f\n", V3ARGS_SCALE(C));
	fprintf(fp, "    endloop\n");
	fprintf(fp, "  endfacet\n");
    }
    fprintf(fp, "endsolid %s\n", name);
}


void
stl_write_bot_binary(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, int fd, char *UNUSED(name))
{
    fastf_t *vertices;
    size_t num_faces;
    int *faces;
    point_t A;
    point_t B;
    point_t C;
    vect_t BmA;
    vect_t CmA;
    vect_t norm;
    unsigned long i, j, vi;

    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    /* Write out the vertex data for each triangle */
    for (i = 0; (size_t)i < num_faces; i++) {
	float flts[12];
	float *flt_ptr;
	unsigned char vert_buffer[50];
	int ret;

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

	VSCALE(A, A, d->cfactor);
	VSCALE(B, B, d->cfactor);
	VSCALE(C, C, d->cfactor);

	memset(vert_buffer, 0, sizeof(vert_buffer));

	flt_ptr = flts;
	VMOVE(flt_ptr, norm);
	flt_ptr += 3;
	VMOVE(flt_ptr, A);
	flt_ptr += 3;
	VMOVE(flt_ptr, B);
	flt_ptr += 3;
	VMOVE(flt_ptr, C);

	bu_cv_htonf(vert_buffer, (const unsigned char *)flts, 12);
	for (j = 0; j < 12; j++) {
	    lswap((unsigned int *)&vert_buffer[j*4]);
	}
	ret = write(fd, vert_buffer, 50);
	if (ret < 0) {
	    perror("write");
	}
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
