/*                         B O T _ D U M P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/bot_dump.c
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
#include "bin.h"

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"

#include "mater.h"
#include "solid.h"

#include "./ged_private.h"


#define V3ARGS_SCALE(_a) (_a)[X]*cfactor, (_a)[Y]*cfactor, (_a)[Z]*cfactor

static char usage[] = "\
Usage: %s [-b] [-n] [-m directory] [-o file] [-t dxf|obj|sat|stl] [-u units] [bot1 bot2 ...]\n";

enum otype {
    OTYPE_DXF = 1,
    OTYPE_OBJ,
    OTYPE_SAT,
    OTYPE_STL
};


struct _ged_bot_dump_client_data {
    struct ged *gedp;
    FILE *fp;
    int fd;
    char *file_ext;
};


struct _ged_obj_material {
    struct bu_list l;
    struct bu_vls name;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    fastf_t a;
};


static int using_dbot_dump;
struct bu_list HeadObjMaterials;
struct bu_vls obj_materials_file;
FILE *obj_materials_fp;
int num_obj_materials;
int curr_obj_red;
int curr_obj_green;
int curr_obj_blue;
fastf_t curr_obj_alpha;

static enum otype output_type;
static int binary;
static int normals;
static fastf_t cfactor;
static char *output_file;	/* output filename */
static char *output_directory;	/* directory name to hold output files */
static unsigned int total_faces;
static int v_offset;
static int curr_line_num;

static int curr_body_id;
static int curr_lump_id;
static int curr_shell_id;
static int curr_face_id;
static int curr_loop_id;
static int curr_edge_id;

/* Byte swaps a four byte value */
static void
lswap(unsigned int *v)
{
    unsigned int r;

    r =*v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}


static struct _ged_obj_material *
get_obj_material(int red, int green, int blue, fastf_t transparency)
{
    struct _ged_obj_material *gomp;

    for (BU_LIST_FOR(gomp, _ged_obj_material, &HeadObjMaterials)) {
	if (gomp->r == red &&
	    gomp->g == green &&
	    gomp->b == blue &&
	    ZERO(gomp->a - transparency)) {
	    return gomp;
	}
    }

    BU_GETSTRUCT(gomp, _ged_obj_material);
    BU_LIST_APPEND(&HeadObjMaterials, &gomp->l);
    gomp->r = red;
    gomp->g = green;
    gomp->b = blue;
    gomp->a = transparency;
    bu_vls_init(&gomp->name);
    bu_vls_printf(&gomp->name, "matl_%d", ++num_obj_materials);

    /* Write out newmtl to mtl file */
    fprintf(obj_materials_fp, "newmtl %s\n", bu_vls_addr(&gomp->name));
    fprintf(obj_materials_fp, "Kd %f %f %f\n",
	    (fastf_t)gomp->r / 255.0,
	    (fastf_t)gomp->g / 255.0,
	    (fastf_t)gomp->b / 255.0);
    fprintf(obj_materials_fp, "d %f\n", gomp->a);
    fprintf(obj_materials_fp, "illum 1\n");

    return gomp;
}


static void
free_obj_materials() {
    struct _ged_obj_material *gomp;

    while (BU_LIST_WHILE(gomp, _ged_obj_material, &HeadObjMaterials)) {
	BU_LIST_DEQUEUE(&gomp->l);
	bu_vls_free(&gomp->name);
	bu_free(gomp, "_ged_obj_material");
    }
}


static void
write_bot_sat(struct rt_bot_internal *bot, FILE *fp, char *UNUSED(name))
{
    int i, j;
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
	fprintf(fp, "-%d coedge $-1 $%d $%d $%d $%d forward $%d $-1 #\n",
		curr_line_num, curr_line_num+1, curr_line_num+2, curr_line_num,
		curr_line_num+num_faces*3, curr_loop_id);
	++curr_line_num;
	fprintf(fp, "-%d coedge $-1 $%d $%d $%d $%d forward $%d $-1 #\n",
		curr_line_num, curr_line_num+1, curr_line_num-1, curr_line_num,
		curr_line_num+num_faces*3, curr_loop_id);
	++curr_line_num;
	fprintf(fp, "-%d coedge $-1 $%d $%d $%d $%d forward $%d $-1 #\n",
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
    int i, vi;

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
    int i, vi;
    struct _ged_obj_material *gomp;

    if (using_dbot_dump) {
	gomp = get_obj_material(curr_obj_red,
				    curr_obj_green,
				    curr_obj_blue,
				    curr_obj_alpha);
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

	    fprintf(fp, "vn %f %f %f\n", V3ARGS(norm));
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
    int i, vi;

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


static void
write_bot_stl_binary(struct rt_bot_internal *bot, int fd, char *UNUSED(name))
{
    unsigned long num_vertices;
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

    num_vertices = bot->num_vertices;
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
	ret = write(fd, vert_buffer, 50);
	if (ret < 0) {
	    perror("write");
	}
    }
}


static void
bot_dump(struct directory *dp, struct rt_bot_internal *bot, FILE *fp, int fd, const char *file_ext, const char *db_name)
{
    int ret;

    if (output_directory) {
	char *cp;
	struct bu_vls file_name;

	bu_vls_init(&file_name);
	bu_vls_strcpy(&file_name, output_directory);
	bu_vls_putc(&file_name, '/');
	cp = dp->d_namep;
	while (*cp != '\0') {
	    if (*cp == '/') {
		bu_vls_putc(&file_name, '@');
	    } else if (*cp == '.' || isspace(*cp)) {
		bu_vls_putc(&file_name, '_');
	    } else {
		bu_vls_putc(&file_name, *cp);
	    }
	    cp++;
	}
	bu_vls_strcat(&file_name, file_ext);

	if (binary && output_type == OTYPE_STL) {
	    char buf[81];	/* need exactly 80 chars for header */
	    unsigned char tot_buffer[4];

	    if ((fd=open(bu_vls_addr(&file_name), O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		perror(bu_vls_addr(&file_name));
		bu_log("Cannot open binary output file (%s) for writing\n", bu_vls_addr(&file_name));
		bu_vls_free(&file_name);
		return;
	    }

	    /* Write out STL header */
	    memset(buf, 0, sizeof(buf));
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE", sizeof(buf));
	    ret = write(fd, &buf, 80);
	    if (ret < 0) {
		perror("write");
	    }

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    ret = write(fd, &buf, 4);
	    if (ret < 0) {
		perror("write");
	    }

	    write_bot_stl_binary(bot, fd, dp->d_namep);

	    /* Re-position pointer to 80th byte */
	    lseek(fd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)total_faces);
	    lswap((unsigned int *)tot_buffer);
	    ret = write(fd, tot_buffer, 4);
	    if (ret < 0) {
		perror("write");
	    }

	    close(fd);
	} else {
	    if ((fp=fopen(bu_vls_addr(&file_name), "wb+")) == NULL) {
		perror(bu_vls_addr(&file_name));
		bu_log("Cannot open ASCII output file (%s) for writing\n", bu_vls_addr(&file_name));
		bu_vls_free(&file_name);
		return;
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
		    fprintf(fp, "mtllib %s\n", bu_vls_addr(&obj_materials_file));
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

	bu_vls_free(&file_name);
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


static union tree *
bot_dump_leaf(struct db_tree_state *tsp,
		  const struct db_full_path *pathp,
		  struct rt_db_internal *ip,
		  genptr_t client_data)
{
    int ret;
    union tree *curtree;
    mat_t mat;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct _ged_bot_dump_client_data *gbdcdp = (struct _ged_bot_dump_client_data *)client_data;

    if (ip) RT_CK_DB_INTERNAL(ip);

    /* Indicate success by returning something other than TREE_NULL */
    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NOP;

    dp = pathp->fp_names[pathp->fp_len-1];

    /* we only dump BOT primitives, so skip some obvious exceptions */
    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD || dp->d_flags & RT_DIR_COMB)
	return curtree;

    MAT_IDN(mat);

    /* get the internal form */
    ret=rt_db_get_internal(&intern, dp, gbdcdp->gedp->ged_wdbp->dbip, mat, &rt_uniresource);

    if (ret < 0) {
	bu_log("ged_bot_leaf: rt_get_internal failure %d on %s\n", ret, dp->d_namep);
	return curtree;
    }

    if (ret != ID_BOT) {
	bu_log("ged_bot_leaf: %s is not a bot (ignored)\n", dp->d_namep);
	rt_db_free_internal(&intern);
	return curtree;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    bot_dump(dp, bot, gbdcdp->fp, gbdcdp->fd, gbdcdp->file_ext, gbdcdp->gedp->ged_wdbp->dbip->dbi_filename);
    rt_db_free_internal(&intern);

    return curtree;
}


static int
bot_dump_get_args(struct ged *gedp, int argc, const char *argv[])
{
    char c;

    output_type = OTYPE_STL;
    binary = 0;
    normals = 0;
    cfactor = 1.0;
    output_file = NULL;
    output_directory = NULL;
    total_faces = 0;
    v_offset = 1;
    curr_line_num = 0;
    bu_optind = 1;

    /* Get command line options. */
    while ((c = bu_getopt(argc, (char * const *)argv, "bno:m:t:u:")) != -1) {
	switch (c) {
	    case 'b':		/* Binary output file */
		binary=1;
		break;
	    case 'n':		/* Binary output file */
		normals=1;
		break;
	    case 'm':
		output_directory = bu_optarg;
		break;
	    case 'o':		/* Output file name. */
		output_file = bu_optarg;
		break;
	    case 't':
		if (BU_STR_EQUAL("dxf", bu_optarg))
		    output_type = OTYPE_DXF;
		else if (BU_STR_EQUAL("obj", bu_optarg))
		    output_type = OTYPE_OBJ;
		else if (BU_STR_EQUAL("sat", bu_optarg))
		    output_type = OTYPE_SAT;
		else if (BU_STR_EQUAL("stl", bu_optarg))
		    output_type = OTYPE_STL;
		else {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return GED_ERROR;
		}
		break;
	    case 'u':
		cfactor = bu_units_conversion(bu_optarg);
		if (ZERO(cfactor))
		    cfactor = 1.0;
		else
		    cfactor = 1.0 / cfactor;

		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_ERROR;
	}
    }

    return GED_OK;
}


int
ged_bot_dump(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct directory *dp;
    char *file_ext = '\0';
    FILE *fp = (FILE *)0;
    int fd = -1;
    mat_t mat;
    int i;
    const char *cmd_name;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, usage, argv[0]);
	return GED_HELP;
    }

    using_dbot_dump = 0;

    if (bot_dump_get_args(gedp, argc, argv) == GED_ERROR)
	return GED_ERROR;

    if (bu_optind > argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (output_file && output_directory) {
	fprintf(stderr, "ERROR: options \"-o\" and \"-m\" are mutually exclusive\n");
	return GED_ERROR;
    }

    if (!output_file && !output_directory) {
	if (binary) {
	    bu_vls_printf(gedp->ged_result_str, "Can't output binary to stdout\nUsage: %s %s", argv[0], usage);
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
		bu_vls_printf(gedp->ged_result_str, "Cannot open binary output file (%s) for writing\n", output_file);
		return GED_ERROR;
	    }

	    /* Write out STL header if output file is binary */
	    memset(buf, 0, sizeof(buf));
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE", sizeof(buf));
	    ret = write(fd, &buf, 80);
	    if (ret < 0) {
		perror("write");
	    }

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    ret = write(fd, &buf, 4);
	    if (ret < 0) {
		perror("write");
	    }
	} else {
	    /* Open ASCII output file */
	    if ((fp=fopen(output_file, "wb+")) == NULL) {
		perror(argv[0]);
		bu_vls_printf(gedp->ged_result_str, "Cannot open ascii output file (%s) for writing\n", output_file);
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

    MAT_IDN(mat);

    if (argc < 1) {
	/* dump all the bots */
	FOR_ALL_DIRECTORY_START(dp, gedp->ged_wdbp->dbip) {

	    /* we only dump BOT primitives, so skip some obvious exceptions */
	    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD) continue;
	    if (dp->d_flags & RT_DIR_COMB) continue;

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
	    bot_dump(dp, bot, fp, fd, file_ext, gedp->ged_wdbp->dbip->dbi_filename);
	    rt_db_free_internal(&intern);

	} FOR_ALL_DIRECTORY_END;
    } else {
	int ac = 1;
	int ncpu = 1;
	char *av[2];
	struct _ged_bot_dump_client_data *gbdcdp;

	av[1] = (char *)0;
	BU_GETSTRUCT(gbdcdp, _ged_bot_dump_client_data);
	gbdcdp->gedp = gedp;
	gbdcdp->fp = fp;
	gbdcdp->fd = fd;
	gbdcdp->file_ext = file_ext;

	for (i = 0; i < argc; ++i) {
	    av[0] = (char *)argv[i];
	    ret = db_walk_tree(gedp->ged_wdbp->dbip,
			       ac,
			       (const char **)av,
			       ncpu,
			       &gedp->ged_wdbp->wdb_initial_tree_state,
			       0,
			       0,
			       bot_dump_leaf,
			       (genptr_t)gbdcdp);
	}

	bu_free((genptr_t)gbdcdp, "ged_bot_dump: gbdcdp");
    }


    if (output_file) {
	if (binary && output_type == OTYPE_STL) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    lseek(fd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)total_faces);
	    lswap((unsigned int *)tot_buffer);
	    ret = write(fd, tot_buffer, 4);
	    if (ret < 0) {
		perror("write");
	    }

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


static void
write_data_arrows(struct ged_data_arrow_state *gdasp, FILE *fp, int sflag)
{
    register int i;

    if (gdasp->gdas_draw) {
	struct _ged_obj_material *gomp;

	gomp = get_obj_material(gdasp->gdas_color[0],
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
	    fprintf(fp, "l %d %d\n", (i/2*6)+v_offset, (i/2*6)+v_offset+1);
	    fprintf(fp, "l %d %d\n", (i/2*6)+v_offset+1, (i/2*6)+v_offset+2);
	    fprintf(fp, "l %d %d\n", (i/2*6)+v_offset+1, (i/2*6)+v_offset+3);
	    fprintf(fp, "l %d %d\n", (i/2*6)+v_offset+1, (i/2*6)+v_offset+4);
	    fprintf(fp, "l %d %d\n", (i/2*6)+v_offset+1, (i/2*6)+v_offset+5);
	    fprintf(fp, "l %d %d\n", (i/2*6)+v_offset+2, (i/2*6)+v_offset+3);
	    fprintf(fp, "l %d %d\n", (i/2*6)+v_offset+3, (i/2*6)+v_offset+4);
	    fprintf(fp, "l %d %d\n", (i/2*6)+v_offset+4, (i/2*6)+v_offset+5);
	    fprintf(fp, "l %d %d\n", (i/2*6)+v_offset+5, (i/2*6)+v_offset+2);
	}

	v_offset += ((gdasp->gdas_num_points/2)*6);
    }
}


static void
write_data_axes(struct ged_data_axes_state *gdasp, FILE *fp, int sflag)
{
    register int i;

    if (gdasp->gdas_draw) {
	fastf_t halfAxesSize;
	struct _ged_obj_material *gomp;

	halfAxesSize = gdasp->gdas_size * 0.5;

	gomp = get_obj_material(gdasp->gdas_color[0],
				    gdasp->gdas_color[1],
				    gdasp->gdas_color[2],
				    1);
	fprintf(fp, "usemtl %s\n", bu_vls_addr(&gomp->name));

	if (sflag)
	    fprintf(fp, "g sdata_axes\n");
	else
	    fprintf(fp, "g data_axes\n");

	for (i = 0; i < gdasp->gdas_num_points; ++i) {
	    point_t A, B;

	    /* draw X axis with x/y offsets */
	    VSET(A,
		 gdasp->gdas_points[i][X] - halfAxesSize,
		 gdasp->gdas_points[i][Y],
		 gdasp->gdas_points[i][Z]);
	    VSET(B,
		 gdasp->gdas_points[i][X] + halfAxesSize,
		 gdasp->gdas_points[i][Y],
		 gdasp->gdas_points[i][Z]);

	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(A));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(B));

	    /* draw Y axis with x/y offsets */
	    VSET(A,
		 gdasp->gdas_points[i][X],
		 gdasp->gdas_points[i][Y] - halfAxesSize,
		 gdasp->gdas_points[i][Z]);
	    VSET(B,
		 gdasp->gdas_points[i][X],
		 gdasp->gdas_points[i][Y] + halfAxesSize,
		 gdasp->gdas_points[i][Z]);

	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(A));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(B));

	    /* draw Z axis with x/y offsets */
	    VSET(A,
		 gdasp->gdas_points[i][X],
		 gdasp->gdas_points[i][Y],
		 gdasp->gdas_points[i][Z] - halfAxesSize);
	    VSET(B,
		 gdasp->gdas_points[i][X],
		 gdasp->gdas_points[i][Y],
		 gdasp->gdas_points[i][Z] + halfAxesSize);

	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(A));
	    fprintf(fp, "v %f %f %f\n", V3ARGS_SCALE(B));
	}

	for (i = 0; i < gdasp->gdas_num_points; ++i) {
	    fprintf(fp, "l %d %d\n", (i*6)+v_offset, (i*6)+v_offset+1);
	    fprintf(fp, "l %d %d\n", (i*6)+v_offset+2, (i*6)+v_offset+3);
	    fprintf(fp, "l %d %d\n", (i*6)+v_offset+4, (i*6)+v_offset+5);
	}


	v_offset += (gdasp->gdas_num_points*6);
    }
}


static void
write_data_lines(struct ged_data_line_state *gdlsp, FILE *fp, int sflag)
{
    register int i;

    if (gdlsp->gdls_draw) {
	struct _ged_obj_material *gomp;

	gomp = get_obj_material(gdlsp->gdls_color[0],
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
	    fprintf(fp, "l %d %d\n", i+v_offset, i+v_offset+1);
	}

	v_offset += gdlsp->gdls_num_points;
    }
}


static void
write_data_obj(struct ged *gedp, FILE *fp)
{
    write_data_arrows(&gedp->ged_gvp->gv_data_arrows, fp, 0);
    write_data_arrows(&gedp->ged_gvp->gv_sdata_arrows, fp, 1);

    write_data_axes(&gedp->ged_gvp->gv_data_axes, fp, 0);
    write_data_axes(&gedp->ged_gvp->gv_sdata_axes, fp, 1);

    write_data_lines(&gedp->ged_gvp->gv_data_lines, fp, 0);
    write_data_lines(&gedp->ged_gvp->gv_sdata_lines, fp, 1);
}


static int
data_dump(struct ged *gedp, FILE *fp)
{
    switch (output_type) {
	case OTYPE_DXF:
	    break;
	case OTYPE_OBJ:
	    if (output_directory) {
		char *cp;
		struct bu_vls filepath;
		FILE *data_fp;

		cp = strrchr(output_directory, '/');
		if (!cp)
		    cp = (char *)output_directory;
		else
		    ++cp;

		if (*cp == '\0') {
		    bu_vls_printf(gedp->ged_result_str, "data_dump: bad dirname - %s\n", output_directory);
		    return GED_ERROR;
		}

		bu_vls_init(&filepath);
		bu_vls_printf(&filepath, "%s/%s_data.obj", output_directory, cp);

		if ((data_fp=fopen(bu_vls_addr(&filepath), "wb+")) == NULL) {
		    bu_vls_printf(gedp->ged_result_str, "data_dump: failed to open %V\n", &filepath);
		    bu_vls_free(&filepath);
		    return GED_ERROR;
		}

		bu_vls_free(&filepath);
		write_data_obj(gedp, data_fp);
		fclose(data_fp);
	    } else
		write_data_obj(gedp, fp);
	    break;
	case OTYPE_SAT:
	    break;
	case OTYPE_STL:
	default:
	    break;
    }

    return GED_OK;
}


int
ged_dbot_dump(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    char *file_ext = '\0';
    FILE *fp = (FILE *)0;
    int fd = -1;
    mat_t mat;
    struct ged_display_list *gdlp;
    const char *cmd_name;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, usage, argv[0]);
	return GED_HELP;
    }

    using_dbot_dump = 1;

    if (bot_dump_get_args(gedp, argc, argv) == GED_ERROR)
	return GED_ERROR;

    if (bu_optind != argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (output_file && output_directory) {
	fprintf(stderr, "ERROR: options \"-o\" and \"-m\" are mutually exclusive\n");
	return GED_ERROR;
    }

    if (!output_file && !output_directory) {
	if (binary) {
	    bu_vls_printf(gedp->ged_result_str, "Can't output binary to stdout\nUsage: %s %s", argv[0], usage);
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
		bu_vls_printf(gedp->ged_result_str, "Cannot open binary output file (%s) for writing\n", output_file);
		return GED_ERROR;
	    }

	    /* Write out STL header if output file is binary */
	    memset(buf, 0, sizeof(buf));
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE", sizeof(buf));
	    ret = write(fd, &buf, 80);
	    if (ret < 0) {
		perror("write");
	    }

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    ret = write(fd, &buf, 4);
	    if (ret < 0) {
		perror("write");
	    }
	} else {
	    /* Open ASCII output file */
	    if ((fp=fopen(output_file, "wb+")) == NULL) {
		perror(argv[0]);
		bu_vls_printf(gedp->ged_result_str, "Cannot open ascii output file (%s) for writing\n", output_file);
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
		    break;
		default:
		    break;
	    }
	}
    }

    /* save the command name */
    cmd_name = argv[0];

    if (output_directory) {
	switch (output_type) {
	    case OTYPE_DXF:
		file_ext = ".dxf";
		break;
	    case OTYPE_OBJ:
		file_ext = ".obj";

		BU_LIST_INIT(&HeadObjMaterials);

		{
		    char *cp;
		    struct bu_vls filepath;

		    cp = strrchr(output_directory, '/');
		    if (!cp)
			cp = (char *)output_directory;
		    else
			++cp;

		    if (*cp == '\0') {
			bu_vls_printf(gedp->ged_result_str, "%s: bad dirname - %s\n", cmd_name, output_directory);
			return GED_ERROR;
		    }

		    bu_vls_init(&obj_materials_file);
		    bu_vls_printf(&obj_materials_file, "%s.mtl", cp);

		    bu_vls_init(&filepath);
		    bu_vls_printf(&filepath, "%s/%V", output_directory, &obj_materials_file);

		    if ((obj_materials_fp=fopen(bu_vls_addr(&filepath), "wb+")) == NULL) {
			bu_vls_printf(gedp->ged_result_str, "%s: failed to open %V\n", cmd_name, &filepath);
			bu_vls_free(&obj_materials_file);
			bu_vls_free(&filepath);
			return GED_ERROR;
		    }

		    bu_vls_free(&filepath);
		}

		num_obj_materials = 0;

		break;
	    case OTYPE_SAT:
		file_ext = ".sat";
		break;
	    case OTYPE_STL:
	    default:
		file_ext = ".stl";
		break;
	}
    } else if (output_type == OTYPE_OBJ) {
	char *cp;

	bu_vls_init(&obj_materials_file);

	cp = strrchr(output_file, '.');
	if (!cp)
	    bu_vls_printf(&obj_materials_file, "%s.mtl", output_file);
	else {
	    /* ignore everything after the last '.' */
	    *cp = '\0';
	    bu_vls_printf(&obj_materials_file, "%s.mtl", output_file);
	    *cp = '.';
	}

	BU_LIST_INIT(&HeadObjMaterials);

	if ((obj_materials_fp=fopen(bu_vls_addr(&obj_materials_file), "wb+")) == NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s: failed to open %V\n", cmd_name, &obj_materials_file);
	    bu_vls_free(&obj_materials_file);
	    return GED_ERROR;
	}

	num_obj_materials = 0;

	fprintf(fp, "mtllib %s\n", bu_vls_addr(&obj_materials_file));
    }

    MAT_IDN(mat);

    for (BU_LIST_FOR(gdlp, ged_display_list, &gedp->ged_gdp->gd_headDisplay)) {
	struct solid *sp;

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    struct directory *dp;
	    struct rt_db_internal intern;
	    struct rt_bot_internal *bot;

	    dp = sp->s_fullpath.fp_names[sp->s_fullpath.fp_len-1];

	    /* get the internal form */
	    ret=rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, mat, &rt_uniresource);

	    if (ret < 0) {
		bu_log("%s: rt_get_internal failure %d on %s\n", cmd_name, ret, dp->d_namep);
		continue;
	    }

	    if (ret != ID_BOT) {
		bu_log("%s: %s is not a bot (ignored)\n", cmd_name, dp->d_namep);
		rt_db_free_internal(&intern);
		continue;
	    }

	    /* Write out materials */
	    if (output_type == OTYPE_OBJ) {
#if 0
		struct _ged_obj_material *gomp;

		gomp = get_obj_material(sp->s_color[0],
					    sp->s_color[1],
					    sp->s_color[2],
					    sp->s_transparency);

		/* Write out usemtl to obj file */
		fprintf(fp, "usemtl %s\n", bu_vls_addr(&gomp->name));
#else
		curr_obj_red = sp->s_color[0];
		curr_obj_green = sp->s_color[1];
		curr_obj_blue = sp->s_color[2];
		curr_obj_alpha = sp->s_transparency;
#endif
	    }

	    bot = (struct rt_bot_internal *)intern.idb_ptr;
	    bot_dump(dp, bot, fp, fd, file_ext, gedp->ged_wdbp->dbip->dbi_filename);
	    rt_db_free_internal(&intern);
	}
    }

    data_dump(gedp, fp);

    if (output_file) {
	if (binary && output_type == OTYPE_STL) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    lseek(fd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)total_faces);
	    lswap((unsigned int *)tot_buffer);
	    ret = write(fd, tot_buffer, 4);
	    if (ret < 0) {
		perror("write");
	    }

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

    if (output_type == OTYPE_OBJ) {
	bu_vls_free(&obj_materials_file);
	free_obj_materials();
	fclose(obj_materials_fp);
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
