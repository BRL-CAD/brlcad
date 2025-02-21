/*                            D X F . C
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
/** @file libged/bot/dump/dxf.c
 *
 * bot_dump DXF format specific logic.
 *
 */

#include "common.h"

#include "./ged_bot_dump.h"

int
dxf_setup(struct _ged_bot_dump_client_data *d, const char *fname, const char *objname, const char *gname)
{
    if (!d)
	return BRLCAD_ERROR;

    struct ged *gedp = d->gedp;

    d->fp = fopen(fname, "wb+");
    if (d->fp == NULL) {
	perror("dxf_setup");
	bu_vls_printf(gedp->ged_result_str, "Cannot open DXF ascii output file (%s) for writing\n", fname);
	return BRLCAD_ERROR;
    }

    /* output DXF header and start of TABLES section */
    if (gname) {
	fprintf(d->fp,
		"0\nSECTION\n2\nHEADER\n999\n%s (BOT from %s)\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",
		objname, gname);
    } else {
	fprintf(d->fp,
		"0\nSECTION\n2\nHEADER\n999\n%s (All Bots)\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",
		objname);
    }

    return BRLCAD_OK;
}

int
dxf_finish(struct _ged_bot_dump_client_data *d)
{
    if (!d)
	return BRLCAD_ERROR;

    fprintf(d->fp, "0\nENDSEC\n0\nEOF\n");
    fclose(d->fp);

    return BRLCAD_OK;
}

void
dxf_write_bot(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, FILE *fp, char *name)
{
    fastf_t *vertices;
    int num_faces, *faces;
    point_t A;
    point_t B;
    point_t C;
    int i, vi;

    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    for (i = 0; i < num_faces; i++) {
	vi = 3*faces[3*i];
	VSET(A, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+1];
	VSET(B, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+2];
	VSET(C, vertices[vi], vertices[vi+1], vertices[vi+2]);

	VSCALE(A, A, d->cfactor);
	VSCALE(B, B, d->cfactor);
	VSCALE(C, C, d->cfactor);

	fprintf(fp, "0\n3DFACE\n8\n%s\n62\n7\n", name);
	fprintf(fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
		10, A[X], 20, A[Y], 30, A[Z]);
	fprintf(fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
		11, B[X], 21, B[Y], 31, B[Z]);
	fprintf(fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
		12, C[X], 22, C[Y], 32, C[Z]);
	fprintf(fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
		12, C[X], 22, C[Y], 32, C[Z]);
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
