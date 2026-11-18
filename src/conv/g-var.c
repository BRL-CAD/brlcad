/*                     G - V A R . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2026 United States Government as represented by
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
 *
 */
/** @file conv/g-var.c
 *
 * BRL-CAD to (OpenGL) Vertex Array Exporter.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include <math.h>

/* interface headers */
#include "bu/app.h"
#include "bu/arg.h"
#include "bu/endian.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "wdb.h"
#include "raytrace.h"

/* version is a char where it is used below */
#define MESH_FORMAT_VERSION '2'


struct mesh {
    char *name;
    struct rt_bot_internal *bot;
    struct mesh *next;
};


struct g_var_args {
    int help;
    int verbose;
    int y_up;
    int flip_normals;
    fastf_t scale;
    const char *output;
};

static const struct bu_arg_option g_var_options[] = {
    BU_ARG_FLAG("h", "help", struct g_var_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct g_var_args, verbose,
	"Report mesh conversion progress"),
    BU_ARG_FLAG("y", "y-up", struct g_var_args, y_up,
	"Convert BRL-CAD Z-up coordinates to Y-up"),
    BU_ARG_FLAG("f", "flip-normals", struct g_var_args, flip_normals,
	"Reverse face winding and computed normals"),
    BU_ARG_VALUE("s", "scale", struct g_var_args, scale,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "factor",
	"Scale output vertices (default 0.001)"),
    BU_ARG_VALUE("o", "output", struct g_var_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file",
	"Write binary mesh output to a file instead of standard output"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_var_operands[] = {
    BU_ARG_OPERAND("database", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1, 1,
	"Database object to export", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema g_var_schema = BU_ARG_SCHEMA(
    "g-var", "Export BRL-CAD BOT geometry as a vertex array mesh",
    g_var_options, g_var_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar g_var_grammar = {
    &g_var_schema, NULL, NULL
};


static int verbose = 0;
static int yup = 0;
static int flip_normals = 0;
static fastf_t scale = 0.001;
static const char *out_file = NULL;
static const char *db_file = NULL;
static const char *object = NULL;

static FILE *fp_out;

static char format_version = MESH_FORMAT_VERSION;
static struct mesh *head = NULL;
static struct mesh *curr = NULL;
static uint32_t mesh_count = 0;
static uint32_t total_vertex_count = 0;
static uint32_t total_face_count = 0;


void mesh_tracker(struct db_i *dbip, struct directory *dp, void *UNUSED(ptr))
{
    struct rt_db_internal internal;

    /* leaf node must be a solid */
    if (!(dp->d_flags & RT_DIR_SOLID)) {
	fprintf(stderr, "warning: '%s' is not a solid! (not processed)\n", dp->d_namep);
	return;
    }

    /* solid must be a bot */
    if (rt_db_get_internal(&internal, dp, dbip, NULL) != ID_BOT) {
	fprintf(stderr, "warning: '%s' is not a bot! (not processed)\n", dp->d_namep);
	return;
    }
    /* track bot */
    if (NULL == curr) {
	BU_ALLOC(head, struct mesh);
	head->name = dp->d_namep;
	head->bot = (struct rt_bot_internal *)internal.idb_ptr;
	head->next = NULL;
	curr = head;
    } else {
	BU_ALLOC(curr->next, struct mesh);
	curr = curr->next;
	curr->name = dp->d_namep;
	curr->bot = (struct rt_bot_internal *)internal.idb_ptr;
	curr->next = NULL;
    }
    /* accumulate counts */
    total_vertex_count += (uint32_t)curr->bot->num_vertices;
    total_face_count += (uint32_t)curr->bot->num_faces;
    mesh_count++;
}


void dealloc_mesh_list(void)
{
    struct mesh* tmp;
    curr = head;
    while (curr != NULL) {
	tmp = curr;
	curr = curr->next;
	bu_free(tmp, "a mesh");
    }
}


void write_header(struct db_i *dbip)
{
    size_t ret;
    size_t len;
    char endian;
    /*
      Header format:
      Endian (1 byte) {0=little; !0=big}
      File format version (1 byte)
      Size of Model Name String (2 bytes - short)
      Model Name String (n bytes - char[])
      Number of Meshes (4 bytes - int)
      Total number of vertices (4 bytes - int)
      Total number of faces (4 bytes - int)
    */

    /* endian */
    if (bu_byteorder() == BU_BIG_ENDIAN) {
	endian = '1';
    } else {
	endian = '0';
    }
    ret = fwrite(&endian, 1, 1, fp_out);
    if (ret != 1)
	perror("fwrite");

    /* format version */
    ret = fwrite(&format_version, sizeof(char), 1, fp_out);
    if (ret != 1)
	perror("fwrite");
    len = strlen(dbip->dbi_title);
    /* model name string length */
    ret = fwrite(&len, sizeof(size_t), 1, fp_out);
    if (ret != 1)
	perror("fwrite");
    /* model name string */
    ret = fwrite(dbip->dbi_title, sizeof(char), len, fp_out);
    if (ret != len)
	perror("fwrite");
    /* mesh count */
    ret = fwrite(&mesh_count, sizeof(uint32_t), 1, fp_out);
    if (ret != 1)
	perror("fwrite");
    /* total number of vertices */
    ret = fwrite(&total_vertex_count, sizeof(uint32_t), 1, fp_out);
    if (ret != 1)
	perror("fwrite");
    /* total number of faces */
    ret = fwrite(&total_face_count, sizeof(uint32_t), 1, fp_out);
    if (ret != 1)
	perror("fwrite");
}


void get_vertex(struct rt_bot_internal *bot, size_t idx, float *dest)
{
    dest[0] = bot->vertices[3*idx] * scale;
    dest[1] = bot->vertices[3*idx+1] * scale;
    dest[2] = bot->vertices[3*idx+2] * scale;

    if (yup) {
	/* perform 90deg x-axis rotation */
	float q = (float)-(M_PI_2);
	float y = dest[1];
	float z = dest[2];
	dest[1] = y * cos(q) - z * sin(q);
	dest[2] = y * sin(q) + z * cos(q);
    }

}


void compute_normal(struct rt_bot_internal *bot, int p1, int p2,
		    int p3, float *dest)
{
    float v1[3];
    float v2[3];
    float v3[3];
    float vec1[3];
    float vec2[3];
    float fnorm[3];
    float temp[3];
    float *np1, *np2, *np3;

    /* get face normal */
    get_vertex(bot, p1, v1);
    if (flip_normals) {
	get_vertex(bot, p3, v2);
	get_vertex(bot, p2, v3);
    } else {
	get_vertex(bot, p2, v2);
	get_vertex(bot, p3, v3);
    }

    VSUB2(vec1, v1, v2);
    VSUB2(vec2, v1, v3);
    VCROSS(fnorm, vec1, vec2);
    VUNITIZE(fnorm);

    /* average existing normal with face normal per vertex */
    np1 = dest + 3*p1;
    np2 = dest + 3*p2;
    np3 = dest + 3*p3;
    VADD2(temp, fnorm, np1);
    VUNITIZE(temp);
    VMOVE(np1, temp);
    VADD2(temp, fnorm, np2);
    VUNITIZE(temp);
    VMOVE(np2, temp);
    VADD2(temp, fnorm, np3);
    VUNITIZE(temp);
    VMOVE(np3, temp);
}


void get_normals(struct rt_bot_internal *bot, float *dest)
{
    size_t i;
    for (i = 0; i < bot->num_faces; i++) {
	compute_normal(curr->bot, bot->faces[3*i], bot->faces[3*i+1],
		       bot->faces[3*i+2], dest);
    }
}


void write_mesh_data(void)
{
    size_t ret;

    /*
      Data format:
      Size of Mesh Name String (2 bytes - short)
      Mesh Name String (n chars)
      Number of vertices (4 bytes - int)
      Number of faces (4 bytes - int)
      Vertex triples (m*3*4 bytes - float[])
      Normal triples (m*3*4 bytes - float[])
      Face index format (1 byte) {0=1 byte; 1=2 bytes - short; 2=4 bytes - int}
      Face triples (m*3*4 bytes - int[])
    */

    curr = head;
    while (NULL != curr) {
	size_t len;
	uint32_t nvert, nface;
	size_t i;
	float vec[3];
	char format;

	/* face triples */
	unsigned char ind8[3] = {0, 0, 0};
	uint16_t ind16[3] = {0, 0, 0};
	uint32_t ind32[3] = {0, 0, 0};

	if (verbose) {
	    fprintf(stderr, ">> writing out mesh '%s' (%lu, %lu)\n", curr->name,
		    (long unsigned int)curr->bot->num_vertices, (long unsigned int)curr->bot->num_faces);
	}

	len = strlen(curr->name);
	/* mesh name string length */
	ret = fwrite(&len, sizeof(uint16_t), 1, fp_out);
	if (ret != 1)
	    perror("fwrite");
	/* mesh name string */
	ret = fwrite(curr->name, 1, len, fp_out);
	if (ret != len)
	    perror("fwrite");
	nvert = (uint32_t)curr->bot->num_vertices;
	nface = (uint32_t)curr->bot->num_faces;
	/* number of vertices */
	ret = fwrite(&nvert, sizeof(uint32_t), 1, fp_out);
	if (ret != 1)
	    perror("fwrite");
	/* number of faces */
	ret = fwrite(&nface, sizeof(uint32_t), 1, fp_out);
	if (ret != 1)
	    perror("fwrite");

	/* vertex triples */
	for (i = 0; i < curr->bot->num_vertices; i++) {
	    get_vertex(curr->bot, i, vec);
	    ret = fwrite(vec, sizeof(float), 3, fp_out);
	    if (ret != 3)
		perror("fwrite");
	}
	/* normal triples */
	if (curr->bot->num_normals == curr->bot->num_vertices) {
	    if (verbose)
		fprintf(stderr, ">> .. normals found!\n");
	    /* normals are provided */
	    ret = fwrite(curr->bot->normals, sizeof(float), curr->bot->num_normals * 3, fp_out);
	    if (ret != curr->bot->num_normals * 3)
		perror("fwrite");
	} else {
	    float *normals;
	    if (verbose) {
		fprintf(stderr, ">> .. normals will be computed\n");
	    }
	    /* normals need to be computed */
	    normals = (float *)bu_calloc(sizeof(float), curr->bot->num_vertices * 3, "normals");
	    get_normals(curr->bot, normals);
	    ret = fwrite(normals, sizeof(float), curr->bot->num_vertices * 3, fp_out);
	    if (ret != curr->bot->num_vertices * 3)
		perror("fwrite");
	    bu_free(normals, "normals");
	}

	if (nface < 1<<8) {
	    format = 0;
	} else if (nface < 1<<16) {
	    format = 1;
	} else {
	    format = 2;
	}
	/* face index format */
	ret = fwrite(&format, 1, 1, fp_out);
	if (ret != 1)
	    perror("fwrite");
	switch (format) {
	    default:
	    case 0:
		for (i = 0; i < nface; i++) {
		    ind8[0] = curr->bot->faces[3*i];
		    if (flip_normals) {
			ind8[1] = curr->bot->faces[3*i+2];
			ind8[2] = curr->bot->faces[3*i+1];
		    } else {
			ind8[1] = curr->bot->faces[3*i+1];
			ind8[2] = curr->bot->faces[3*i+2];
		    }
		    ret = fwrite(&ind8, 1, 3, fp_out);
		    if (ret != 3)
			perror("fwrite");
		}
		break;
	    case 1:
		for (i = 0; i < nface; i++) {
		    ind16[0] = curr->bot->faces[3*i];
		    if (flip_normals) {
			ind16[1] = curr->bot->faces[3*i+2];
			ind16[2] = curr->bot->faces[3*i+1];
		    } else {
			ind16[1] = curr->bot->faces[3*i+1];
			ind16[2] = curr->bot->faces[3*i+2];
		    }
		    ret = fwrite(&ind16, 2, 3, fp_out);
		    if (ret != 3)
			perror("fwrite");
		}
		break;
	    case 2:
		for (i = 0; i < nface; i++) {
		    ind32[0] = curr->bot->faces[3*i];
		    if (flip_normals) {
			ind32[1] = curr->bot->faces[3*i+2];
			ind32[2] = curr->bot->faces[3*i+1];
		    } else {
			ind32[1] = curr->bot->faces[3*i+1];
			ind32[2] = curr->bot->faces[3*i+2];
		    }
		    ret = fwrite(&ind32, 4, 3, fp_out);
		    if (ret != 3)
			perror("fwrite");
		}
		break;
	}

	curr = curr->next;
    }
}


int main(int argc, char *argv[])
{
    struct g_var_args args = {0, 0, 0, 0, 0.001, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int introspection;
    int operand_index;
    struct directory *dp;
    struct db_i *dbip;

    /* setup BRL-CAD environment */
    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    introspection = bu_arg_introspect(&msg, &g_var_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&g_var_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&g_var_schema, "g-var");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "g-var help");
	return 0;
    }

    verbose = args.verbose;
    yup = args.y_up;
    flip_normals = args.flip_normals;
    scale = args.scale;
    out_file = args.output;
    db_file = argv[operand_index + 1];
    object = argv[operand_index + 2];

    /* open BRL-CAD database */
    if ((dbip = db_open(db_file, DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open geometry database file %s\n", db_file);
    }
    if (db_dirbuild(dbip))
	bu_exit(1, "db_dirbuild() failed!\n");

    if (verbose)
	fprintf(stderr, ">> opened db '%s'\n", dbip->dbi_title);

    /* setup output stream */
    if (out_file == NULL) {
	fp_out = stdout;
	setmode(fileno(fp_out), O_BINARY);
    } else {
	if ((fp_out = fopen(out_file, "wb")) == NULL) {
	    bu_log("Cannot open %s\n", out_file);
	    perror(argv[0]);
	    return 2;
	}
    }

    /* find requested object */
    db_update_nref(dbip);

    dp = db_lookup(dbip, object, 0);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "Object %s not found in database!\n", object);

    /* generate mesh list */
    db_treewalk_basic(dbip, dp, NULL, mesh_tracker, NULL);
    if (verbose)
	fprintf(stderr, ">> mesh count: %d\n", mesh_count);

    /* write out header */
    write_header(dbip);

    /* write out meshes */
    write_mesh_data();

    /* finish */
    dealloc_mesh_list();
    if (fp_out != stdout)
	fclose(fp_out);
    db_close(dbip);
    return 0;
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
