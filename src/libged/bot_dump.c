/*                         B O T _ D U M P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file bot_dump.c
 *
 * The bot_dump command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"

#include "./ged_private.h"


#define V3ARGS_SCALE(_a)       (_a)[X]*cfactor, (_a)[Y]*cfactor, (_a)[Z]*cfactor

static char usage[] = "\
Usage: %s [-b] [-n] [-m directory] [-o file] [-t dxf|obj|sat|stl] [-u units] [bot1 bot2 ...]\n";

enum otype {
    OTYPE_DXF = 1,
    OTYPE_OBJ,
    OTYPE_SAT,
    OTYPE_STL
};

static enum otype output_type = OTYPE_STL;
static int binary = 0;
static int normals = 0;
static fastf_t cfactor = 1.0;
static int v_offset = 1;
static char *output_file = NULL;	/* output filename */
static char *output_directory = NULL;	/* directory name to hold output files */
static unsigned int total_faces = 0;

static int curr_line_num = 0;
static int curr_body_id;
static int curr_lump_id;
static int curr_shell_id;
static int curr_face_id;
static int curr_loop_id;
static int curr_coedge_id;
static int curr_edge_id;
static int curr_vertex_id;
static int curr_point_id;

/* Byte swaps a four byte value */
static void
lswap(unsigned int *v)
{
    unsigned int r;

    r =*v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}

static void
write_bot_sat(struct rt_bot_internal *bot, FILE *fp, char *name)
{
    register int i, j;
    fastf_t *vertices;
    int *faces;
    int first_vertex;
    int last_vertex;
    int first_pt;
    int last_pt;
    int first_coedge;
    int last_coedge;
    int first_edge;
    int last_edge;
    int first_face;
    int last_face;
    int first_loop;
    int last_loop;
    int num_vertices = bot->num_vertices;
    int num_faces = bot->num_faces;
    int num_edges = num_faces*3;

    vertices = bot->vertices;
    faces = bot->faces;

    curr_body_id = curr_line_num;
    curr_lump_id = curr_body_id + 1;
    curr_shell_id = curr_lump_id + 1;
    curr_face_id = curr_shell_id + 1 + num_vertices*2 + num_faces*6;

    fprintf(fp, "-%d body $-1 $%d $-1 $-1 #\n", curr_body_id, curr_lump_id);
    fprintf(fp, "-%d lump $-1 $-1 $%d $%d #\n", curr_lump_id, curr_shell_id, curr_body_id);
    fprintf(fp, "-%d shell $-1 $-1 $-1 $%d $-1 $%d #\n", curr_shell_id, curr_face_id, curr_lump_id);

    curr_line_num += 3;

    /* Dump out vertices */
    first_vertex = curr_line_num;
    for (i = 0; i < num_vertices; i++) {
	curr_edge_id = -1;
	for (j = 0; j < num_faces; j++) {
	    if (faces[3*j]+first_vertex == curr_line_num) {
		curr_edge_id = first_vertex + num_vertices*2 + num_faces*3 + j*3;
		break;
	    } else if (faces[3*j+1]+first_vertex == curr_line_num) {
		curr_edge_id = first_vertex + num_vertices*2 + num_faces*3 + j*3 + 1;
		break;
	    } else if (faces[3*j+2]+first_vertex == curr_line_num) {
		curr_edge_id = first_vertex + num_vertices*2 + num_faces*3 + j*3 + 2;
		break;
	    }
	}

	fprintf(fp, "-%d vertex $-1 $%d $%d #\n", curr_line_num, curr_edge_id, curr_line_num+num_vertices);
	++curr_line_num;
    }
    last_vertex = curr_line_num-1;

    /* Dump out points */
    first_pt = curr_line_num;
    for (i = 0; i < num_vertices; i++) {
	fprintf(fp, "-%d point $-1 %f %f %f #\n", curr_line_num, V3ARGS_SCALE(&vertices[3*i]));
	++curr_line_num;
    }
    last_pt = curr_line_num-1;

    /* Dump out coedges */
    first_coedge = curr_line_num;
    curr_loop_id = first_coedge+num_faces*7;
    for (i = 0; i < num_faces; i++) {
	fprintf( fp, "-%d coedge $-1 $%d $%d $%d $%d forward $%d $-1 #\n",
		 curr_line_num, curr_line_num+1, curr_line_num+2, curr_line_num,
		 curr_line_num+num_faces*3, curr_loop_id);
	++curr_line_num;
	fprintf( fp, "-%d coedge $-1 $%d $%d $%d $%d forward $%d $-1 #\n",
		 curr_line_num, curr_line_num+1, curr_line_num-1, curr_line_num,
		 curr_line_num+num_faces*3, curr_loop_id);
	++curr_line_num;
	fprintf( fp, "-%d coedge $-1 $%d $%d $%d $%d forward $%d $-1 #\n",
		 curr_line_num, curr_line_num-2, curr_line_num-1, curr_line_num,
		 curr_line_num+num_faces*3,  curr_loop_id);
	++curr_line_num;
	++curr_loop_id;
    }
    last_coedge = curr_line_num-1;

    /* Dump out edges */
    first_edge = curr_line_num;
    for (i = 0; i < num_faces; i++) {
	fprintf(fp, "-%d edge $-1 $%d $%d $%d $%d forward #\n", curr_line_num,
		faces[3*i]+first_vertex, faces[3*i+1]+first_vertex,
		first_coedge + i*3, curr_line_num + num_faces*5);
	++curr_line_num;
	fprintf(fp, "-%d edge $-1 $%d $%d $%d $%d forward #\n", curr_line_num,
		faces[3*i+1]+first_vertex, faces[3*i+2]+first_vertex,
		first_coedge + i*3 + 1, curr_line_num + num_faces*5);
	++curr_line_num;
	fprintf(fp, "-%d edge $-1 $%d $%d $%d $%d forward #\n", curr_line_num,
		faces[3*i+2]+first_vertex, faces[3*i]+first_vertex,
		first_coedge + i*3 + 2, curr_line_num + num_faces*5);
	++curr_line_num;
    }
    last_edge = curr_line_num-1;

    /* Dump out faces */
    first_face = curr_line_num;
    for (i = 0; i < num_faces-1; i++) {
	fprintf(fp, "-%d face $-1 $%d $%d $%d $-1 $%d forward single #\n",
		curr_line_num, curr_line_num+1, curr_line_num+num_faces,
		curr_shell_id, curr_line_num + num_faces*5);
	++curr_line_num;
    }
    fprintf(fp, "-%d face $-1 $-1 $%d $%d $-1 $%d forward single #\n",
	    curr_line_num, curr_line_num+num_faces, curr_shell_id,
	    curr_line_num + num_faces*5);
    ++curr_line_num;
    last_face = curr_line_num-1;

    /* Dump out loops */
    first_loop = curr_line_num;
    for (i = 0; i < num_faces; i++) {
	fprintf(fp, "-%d loop $-1 $-1 $%d $%d #\n",
		curr_line_num, first_coedge+i*3, first_face+i);
	++curr_line_num;
    }
    last_loop = curr_line_num-1;

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

	fprintf(fp, "-%d straight-curve $-1 %f %f %f %f %f %f I I #\n", curr_line_num, V3ARGS_SCALE(A), V3ARGS(BmA));
	++curr_line_num;
	fprintf(fp, "-%d straight-curve $-1 %f %f %f %f %f %f I I #\n", curr_line_num, V3ARGS_SCALE(B), V3ARGS(CmB));
	++curr_line_num;
	fprintf(fp, "-%d straight-curve $-1 %f %f %f %f %f %f I I #\n", curr_line_num, V3ARGS_SCALE(C), V3ARGS(AmC));
	++curr_line_num;
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
		curr_line_num, V3ARGS_SCALE(A), V3ARGS(norm), V3ARGS(BmA));

	++curr_line_num;
    }
}

static void
write_bot_dxf(struct rt_bot_internal *bot, FILE *fp, char *name)
{
    int num_vertices;
    fastf_t *vertices;
    int num_faces, *faces;
    point_t A;
    point_t B;
    point_t C;
    register int i, vi;

    num_vertices = bot->num_vertices;
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

	VSCALE(A, A, cfactor);
	VSCALE(B, B, cfactor);
	VSCALE(C, C, cfactor);

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

static void
write_bot_obj(struct rt_bot_internal *bot, FILE *fp, char *name)
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
    register int i,vi;

    num_vertices = bot->num_vertices;
    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    fprintf(fp, "g %s\n", name);

    for (i = 0; i < num_vertices; i++) {
	fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(&vertices[3*i]));
    }

    if (normals) {
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
	    
	    fprintf(fp, "vn %lf %lf %lf\n", V3ARGS(norm));
	}
    }

    if (normals) {
	for (i = 0; i < num_faces; i++) {
	    fprintf(fp, "f %d//%d %d//%d %d//%d\n", faces[3*i]+v_offset, i+1, faces[3*i+1]+v_offset, i+1, faces[3*i+2]+v_offset, i+1);
	}
    } else {
	for (i = 0; i < num_faces; i++) {
	    fprintf(fp, "f %d %d %d\n", faces[3*i]+v_offset, faces[3*i+1]+v_offset, faces[3*i+2]+v_offset);
	}
    }

    v_offset += num_vertices;
}

static void
write_bot_stl(struct rt_bot_internal *bot, FILE *fp, char *name)
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
    register int i, vi;

    num_vertices = bot->num_vertices;
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

	fprintf(fp, "  facet normal %lf %lf %lf\n", V3ARGS(norm));
	fprintf(fp, "    outer loop\n");
	fprintf(fp, "      vertex %lf %lf %lf\n", V3ARGS_SCALE(A));
	fprintf(fp, "      vertex %lf %lf %lf\n", V3ARGS_SCALE(B));
	fprintf(fp, "      vertex %lf %lf %lf\n", V3ARGS_SCALE(C));
	fprintf(fp, "    endloop\n");
	fprintf(fp, "  endfacet\n");
    }
    fprintf(fp, "endsolid %s\n", name);
}

static void
write_bot_stl_binary(struct rt_bot_internal *bot, int fd, char *name)
{
    unsigned long num_vertices;
    fastf_t *vertices;
    int num_faces, *faces;
    point_t A;
    point_t B;
    point_t C;
    vect_t BmA;
    vect_t CmA;
    vect_t norm;
    register unsigned long i, j, vi;

    num_vertices = bot->num_vertices;
    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    /* Write out the vertex data for each triangle */
    for (i = 0; i < num_faces; i++) {
	float flts[12];
	float *flt_ptr;
	unsigned char vert_buffer[50];

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

	VSCALE(A, A, cfactor);
	VSCALE(B, B, cfactor);
	VSCALE(C, C, cfactor);

	memset(vert_buffer, 0, sizeof(vert_buffer));

	flt_ptr = flts;
	VMOVE(flt_ptr, norm);
	flt_ptr += 3;
	VMOVE(flt_ptr, A);
	flt_ptr += 3;
	VMOVE(flt_ptr, B);
	flt_ptr += 3;
	VMOVE(flt_ptr, C);
	flt_ptr += 3;

	htonf(vert_buffer, (const unsigned char *)flts, 12);
	for (j=0; j<12; j++) {
	    lswap((unsigned int *)&vert_buffer[j*4]);
	}
	write(fd, vert_buffer, 50);
    }
}

static void
bot_dump(struct directory *dp, struct rt_bot_internal *bot, FILE *fp, int fd, struct bu_vls *file_name, const char *file_ext, const char *db_name)
{
    if (output_directory) {
	char *cp;

	bu_vls_trunc(file_name, 0);
	bu_vls_strcpy(file_name, output_directory);
	bu_vls_putc(file_name, '/');
	cp = dp->d_namep;
	cp++;
	while (*cp != '\0') {
	    if (*cp == '/') {
		bu_vls_putc(file_name, '@');
	    } else if (*cp == '.' || isspace(*cp)) {
		bu_vls_putc(file_name, '_');
	    } else {
		bu_vls_putc(file_name, *cp);
	    }
	    cp++;
	}
	bu_vls_strcat(file_name, file_ext);

	if (binary && output_type == OTYPE_STL) {
	    char buf[81];	/* need exactly 80 chars for header */
	    unsigned char tot_buffer[4];

	    if ((fd=open(bu_vls_addr(file_name), O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		perror(bu_vls_addr(file_name));
		bu_exit(1, "Cannot open binary output file (%s) for writing\n", bu_vls_addr(file_name));
	    }

	    /* Write out STL header */
	    memset(buf, 0, sizeof(buf));
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE", sizeof(buf));
	    write(fd, &buf, 80);

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    write(fd, &buf, 4);

	    write_bot_stl_binary(bot, fd, dp->d_namep);

	    /* Re-position pointer to 80th byte */
	    lseek(fd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    bu_plong(tot_buffer, (unsigned long)total_faces);
	    lswap((unsigned int *)tot_buffer);
	    write(fd, tot_buffer, 4);

	    close(fd);
	} else {
	    if ((fp=fopen(bu_vls_addr(file_name), "wb+")) == NULL) {
		perror(bu_vls_addr(file_name));
		bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", bu_vls_addr(file_name));
	    }

	    switch (output_type) {
	    case OTYPE_DXF:
		fprintf(fp,
			"0\nSECTION\n2\nHEADER\n999\n%s (BOT from %s)\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",
			dp->d_namep, db_name);
		write_bot_dxf(bot, fp, dp->d_namep);
		fprintf(fp, "0\nENDSEC\n0\nEOF\n");
		break;
	    case OTYPE_OBJ:
		v_offset = 1;
		write_bot_obj(bot, fp, dp->d_namep);
		break;
	    case OTYPE_SAT:
		curr_line_num = 0;

		fprintf(fp, "400 0 1 0\n");
		/*XXX Temporarily hardwired */
#if 1
		fprintf(fp, "37 SolidWorks(2008000)-Sat-Convertor-2.0 11 ACIS 8.0 NT 24 Wed Dec 03 09:26:53 2003\n");
#else
		fprintf(fp, "08 BRL-CAD-bot_dump-4.0 11 ACIS 4.0 NT 24 Thur Sep 25 15:00:00 2008\n");
#endif
		fprintf(fp, "1 9.9999999999999995e-007 1e-010\n");

		write_bot_sat(bot, fp, dp->d_namep);
		fprintf(fp, "End-of-ACIS-data\n");
		break;
	    case OTYPE_STL:
	    default:
		write_bot_stl(bot, fp, dp->d_namep);
		break;
	    }

	    fclose(fp);
	}
    } else {
	if (binary && output_type == OTYPE_STL) {
	    total_faces += bot->num_faces;
	    write_bot_stl_binary(bot, fd, dp->d_namep);
	} else {
	    switch (output_type) {
	    case OTYPE_DXF:
		write_bot_dxf(bot, fp, dp->d_namep);
		break;
	    case OTYPE_OBJ:
		write_bot_obj(bot, fp, dp->d_namep);
		break;
	    case OTYPE_SAT:
		write_bot_sat(bot, fp, dp->d_namep);
		break;
	    case OTYPE_STL:
	    default:
		write_bot_stl(bot, fp, dp->d_namep);
		break;
	    }
	}
    }
}

int
ged_bot_dump(struct ged *gedp, int argc, const char *argv[])
{
    char idbuf[132];		/* First ID record info */
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct directory *dp;
    struct bu_vls file_name;
    char *file_ext = '\0';
    FILE *fp = (FILE *)0;
    int fd = -1;
    char c;
    mat_t mat;
    register int i;
    const char *cmd_name;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, usage, argv[0]);
	return GED_HELP;
    }

    bu_optind = 1;

    /* Get command line options. */
    while ((c = bu_getopt(argc, (char * const *)argv, "bno:m:t:u:")) != EOF) {
	switch (c) {
	    case 'b':		/* Binary output file */
		binary=1;
		break;
	    case 'n':		/* Binary output file */
		normals=1;
		break;
	    case 'm':
		output_directory = bu_optarg;
		bu_vls_init(&file_name);
		break;
	    case 'o':		/* Output file name. */
		output_file = bu_optarg;
		break;
	    case 't':
		if (!strcmp("dxf", bu_optarg))
		    output_type = OTYPE_DXF;
		else if (!strcmp("obj", bu_optarg))
		    output_type = OTYPE_OBJ;
		else if (!strcmp("sat", bu_optarg))
		    output_type = OTYPE_SAT;
		else if (!strcmp("stl", bu_optarg))
		    output_type = OTYPE_STL;
		else
		    bu_exit(1, usage, argv[0]);
		break;
	    case 'u':
		if ((cfactor = bu_units_conversion(bu_optarg)) == 0.0)
		    cfactor = 1.0;
		else
		    cfactor = 1.0 / cfactor;

		break;
	    default:
		bu_exit(1, usage, argv[0]);
		break;
	}
    }

    if (bu_optind > argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (output_file && output_directory) {
	fprintf(stderr, "ERROR: options \"-o\" and \"-m\" are mutually exclusive\n");
	return GED_ERROR;
    }

    if (!output_file && !output_directory) {
	if (binary) {
	    bu_vls_printf(&gedp->ged_result_str, "Can't output binary to stdout\nUsage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
	fp = stdout;

	/* Set this to something non-null in order to possibly write eof */
	output_file = "stdout";
    }

    if (output_file) {
	if (binary && output_type == OTYPE_STL) {
	    char buf[81];	/* need exactly 80 chars for header */

	    /* Open binary output file */
	    if ((fd=open(output_file, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		perror(argv[0]);
		bu_vls_printf(&gedp->ged_result_str, "Cannot open binary output file (%s) for writing\n", output_file);
		return GED_ERROR;
	    }

	    /* Write out STL header if output file is binary */
	    memset(buf, 0, sizeof(buf));
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE", sizeof(buf));
	    write(fd, &buf, 80);

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    write(fd, &buf, 4);
	} else {
	    /* Open ASCII output file */
	    if ((fp=fopen(output_file, "wb+")) == NULL) {
		perror(argv[0]);
		bu_vls_printf(&gedp->ged_result_str, "Cannot open ascii output file (%s) for writing\n", output_file);
		return GED_ERROR;
	    }

	    switch (output_type) {
		case OTYPE_DXF:
		    /* output DXF header and start of TABLES section */
		    fprintf(fp,
			    "0\nSECTION\n2\nHEADER\n999\n%s (All Bots)\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",
			    argv[argc-1]);
		    break;
		case OTYPE_SAT:
		    fprintf(fp, "400 0 1 0\n");
		    /*XXX Temporarily hardwired */
#if 1
		    fprintf(fp, "37 SolidWorks(2008000)-Sat-Convertor-2.0 11 ACIS 8.0 NT 24 Wed Dec 03 09:26:53 2003\n");
#else
		    fprintf(fp, "08 BRL-CAD-bot_dump-4.0 11 ACIS 4.0 NT 24 Thur Sep 25 15:00:00 2008\n");
#endif
		    fprintf(fp, "1 9.9999999999999995e-007 1e-010\n");
		    curr_line_num = 0;
		    break;
		default:
		    break;
	    }
	}
    }

    /* save the command name */
    cmd_name = argv[0];

    /* skip past the command name and optional args */
    argc -= bu_optind;
    argv += bu_optind;

    RT_INIT_DB_INTERNAL(&intern);

#if 0
    /* skip past the database name */
    --argc;
    ++argv;
#endif

    MAT_IDN(mat);

    if (output_directory) {
	switch (output_type) {
	    case OTYPE_DXF:
		file_ext = ".dxf";
		break;
	    case OTYPE_OBJ:
		file_ext = ".obj";
		break;
	    case OTYPE_SAT:
		file_ext = ".sat";
		break;
	    case OTYPE_STL:
	    default:
		file_ext = ".stl";
		break;
	}
    }

    if (argc < 1) {
	/* dump all the bots */
	FOR_ALL_DIRECTORY_START(dp, gedp->ged_wdbp->dbip) {

	    /* we only dump BOT primitives, so skip some obvious exceptions */
	    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD) continue;
	    if (dp->d_flags & DIR_COMB) continue;

	    /* get the internal form */
	    i=rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, mat, &rt_uniresource);
	    if (i < 0) {
		fprintf(stderr, "%s: rt_get_internal failure %d on %s\n", cmd_name, i, dp->d_namep);
		continue;
	    }

	    if (i != ID_BOT) {
		continue;
	    }

	    bot = (struct rt_bot_internal *)intern.idb_ptr;
	    bot_dump(dp, bot, fp, fd, &file_name, file_ext, gedp->ged_wdbp->dbip->dbi_filename);

	} FOR_ALL_DIRECTORY_END;
    } else {
	/* dump only the specified bots */
	for (i = 0; i < argc; ++i) {
	    int ret;

	    if ((dp=db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_QUIET)) == DIR_NULL) {
		fprintf(stderr, "%s: db_lookup failed on %s\n", cmd_name, argv[i]);
		continue;
	    }

	    /* we only dump BOT primitives, so skip some obvious exceptions */
	    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD) continue;
	    if (dp->d_flags & DIR_COMB) continue;

	    /* get the internal form */
	    ret=rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, mat, &rt_uniresource);

	    if (ret < 0) {
		fprintf(stderr, "%s: rt_get_internal failure %d on %s\n", cmd_name, ret, dp->d_namep);
		continue;
	    }

	    if (ret != ID_BOT) {
		fprintf(stderr, "%s: %s is not a bot (ignored)\n", cmd_name, argv[i]);
		continue;
	    }

	    bot = (struct rt_bot_internal *)intern.idb_ptr;
	    bot_dump(dp, bot, fp, fd, &file_name, file_ext, gedp->ged_wdbp->dbip->dbi_filename);
	}
    }


    if (output_file) {
	if (binary && output_type == OTYPE_STL) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    lseek(fd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    bu_plong(tot_buffer, (unsigned long)total_faces);
	    lswap((unsigned int *)tot_buffer);
	    write(fd, tot_buffer, 4);

	    close(fd);
	} else {
	    /* end of layers section, start of ENTITIES SECTION */
	    switch (output_type) {
		case OTYPE_DXF:
		    fprintf(fp, "0\nENDSEC\n0\nEOF\n");
		    break;
		case OTYPE_SAT:
		    fprintf(fp, "End-of-ACIS-data\n");
		    break;
		default:
		    break;
	    }

	    fclose(fp);
	}
    }

    return GED_OK;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
