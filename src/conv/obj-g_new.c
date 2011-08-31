/*                     O B J - G _ N E W . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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
/**
 *
 * This is a program to convert a WaveFront Object file to a BRL-CAD
 * database file.
 *
 * Example usage: obj-g -u m -o s input.obj output.g
 *
 * Recommended Improvements (not already listed in function comments):
 * - Support obj file texture vertices and materials
 * - Optionally allow face grouping to be processed in parallel
 * - Allow user to select which obj file face groupings to convert
 * - Option to provide analysis of obj file edge lengths to help user
 *   estimate a reasonable distance tolerance or help user determine
 *   if the model should be re-tessellated in the source system using
 *   a different tessellation tolerance.
 * - The ability to parse the obj file group names with user
 *   selectable delimiters(s), to recreate a group/assembly structure
 *   in BRL-CAD. For example, group names which contain a '/' may
 *   indicate a assembly structure. This capability could be extended
 *   to create a group/assembly structure based on faces being
 *   contained in multiple groups in the obj file. This would correct
 *   the current behavior of when a face resides, for example, in two
 *   groups, there will be two copies of the face in the resulting
 *   model.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include "bu.h"
#include "vmath.h"
#include "nmg.h"
#include "plot3.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "obj_parser.h"

static char *usage =
"Usage: %s [options] -u units -o type input.obj output.g\n\n"

"Required options:\n"
"  -u units\tSelect units for the obj file: (m|cm|mm|ft|in).\n"
"  -o type\tSelect the type used for bots that aren't closed volumes:\n"
         "\t\t\tn = plate nocos\n"
         "\t\t\tp = plate\n"
         "\t\t\ts = surface\n\n"

"Other options:\n"
"  -c\t\tContinue processing on nmg-bomb. Conversion will fall-back to\n"
    "\t\tnative bot mode if a fatal error occurs when using the nmg or\n"
    "\t\tbot via nmg modes.\n"
"  -d\t\tOutput debug info to stderr.\n"
"  -g grouping\tSelect which OBJ face grouping is used to create BRL-CAD\n"
	   "\t\tprimitives:\n"
	   "\t\t\tg = group (default)\n"
	   "\t\t\tm = material\n"
	   "\t\t\tn = none\n"
	   "\t\t\to = object\n"
	   "\t\t\tt = texture\n"
"  -h mm\t\tThickness used when a bot is not a closed volume and it's\n"
       "\t\tconverted as a plate or plate-nocos bot.\n"
"  -i\t\tIgnore the normals defined in the input file when using native\n"
    "\t\tbot conversion mode.\n"
"  -m mode\tSelect the conversion mode:\n"
         "\t\t\tb = native bot\n"
         "\t\t\tn = nmg\n"
         "\t\t\tv = bot via nmg (default)\n"
"  -p\t\tCreates a plot/overlay (.pl) file of open edges for bots that\n"
    "\t\taren't closed volumes. <bot_name>.pl will be created in the\n"
    "\t\tcurrent directory and will overwrite any exisiting file with the\n"
    "\t\tsame name.\n"
"  -t mm\t\tDistance tolerance. Two vertices are considered to be the same\n"
       "\t\tif they are within this distance of one another. Default is\n"
       "\t\t.0005mm. You should not change this value without setting the\n"
       "\t\traytracer tolerance to match it.\n"
"  -v\t\tOut verbose user info to stderr. Each occurrance of this option\n"
    "\t\tin the option list increases the verbosity level.\n"
"  -x flag\tSpecify rt debug flag bits (see raytrace.h).\n"
"  -X flag\tSpecify nmg debug flag bits (see nmg.h).\n";
 
/* global definition */
size_t *tmp_ptr = NULL;
static int NMG_debug; /* saved arg of -X, for longjmp handling */
static int debug = 0;
static int verbose = 0;

/* grouping type */
#define GRP_NONE     0 /* perform no grouping */
#define GRP_GROUP    1 /* create bot for each obj file 'g' grouping */
#define GRP_OBJECT   2 /* create bot for each obj file 'o' grouping */
#define GRP_MATERIAL 3 /* create bot for each obj file 'usemtl' grouping */
#define GRP_TEXTURE  4 /* create bot for eacg obj file 'usemap' grouping */

/* face type */
#define FACE_V    1 /* polygonal faces only identified by vertices */
#define FACE_TV   2 /* textured polygonal faces */
#define FACE_NV   3 /* oriented polygonal faces */
#define FACE_TNV  4 /* textured oriented polygonal faces */

/* face test type */
#define TEST_ALL      0 /* perform all face tests */
#define TEST_NUM_VERT 1 /* test only number of vertices */

/* type of vertex to fuse */
#define FUSE_VERT     0
#define FUSE_TEX_VERT 1

/* type of vertex compare during vertex fuse */
#define FUSE_WI_TOL   0
#define FUSE_EQUAL    1

/* closure status */
#define SURF_UNTESTED 0
#define SURF_CLOSED   1
#define SURF_OPEN     2

/* normal vector processing */
#define PROC_NORM 0
#define IGNR_NORM 1

/* texture vertex processing */
#define PROC_TEX 0
#define IGNR_TEX 1

/* primitive output mode */
#define OUT_PBOT 0 /* output plate mode bot */
#define OUT_VBOT 1 /* output volume mode bot */
#define OUT_NMG  2 /* output nmg */

/* plot open edges, turn-on, turn-off */
#define PLOT_OFF 0
#define PLOT_ON  1 

typedef const size_t (**arr_1D_t);    /* 1 dimensional array type */
typedef const size_t (**arr_2D_t)[2]; /* 2 dimensional array type */
typedef const size_t (**arr_3D_t)[3]; /* 3 dimensional array type */

typedef size_t (*tri_arr_1D_t)[3];    /* triangle index array 1 dimensional type (v) */
typedef size_t (*tri_arr_2D_t)[3][2]; /* triangle index array 2 dimensional type (tv) or (nv) */
typedef size_t (*tri_arr_3D_t)[3][3]; /* triangle index array 3 dimensional type (tnv) */

typedef size_t (*edge_arr_2D_t)[2]; /* edge array type */

/* grouping face indices type */
struct gfi_t { 
    void *index_arr_faces;            /* face indices into vertex, normal , texture vertex lists */
    size_t *num_vertices_arr;         /* number of vertices for each face within index_arr_faces */
    size_t *obj_file_face_idx_arr;    /* corresponds to the index of the face within the obj file. this
                                       * value is useful to trace face errors back to the obj file.
                                       */
    struct bu_vls *raw_grouping_name; /* raw name of grouping from obj file; group/object/material/texture */
    struct bu_vls *primitive_name;    /* name of primitive to be created */
    size_t num_faces;                 /* number of faces represented by index_arr_faces and num_vertices_arr */
    size_t max_faces;                 /* maximum number of faces based on current memory allocation */
    short int *face_status;           /* test_face result, 0 = untested, 1 = valid, >1 = degenerate */
    short int closure_status;         /* i.e. SURF_UNTESTED, SURF_CLOSED, SURF_OPEN */
    size_t tot_vertices;              /* sum of contents of num_vertices_arr. note: if the face_type
                                       * includes normals and/or texture vertices, each vertex must have
                                       * an associated normal and/or texture vertice. therefore this
                                       * total is also the total of the associated normals and/or texture
                                       * vertices.
                                       */
    int face_type;                    /* i.e. FACE_V, FACE_TV, FACE_NV or FACE_TNV */
    int grouping_type;                /* i.e. GRP_NONE, GRP_GROUP, GRP_OBJECT, GRP_MATERIAL or GRP_TEXTURE */
    size_t grouping_index;            /* corresponds to the index of the grouping name within the obj file.  
                                       * this value is useful to append to the raw_grouping_name to
                                       * create the primitive_name and ensure the primitive_name is unique.
                                       */
    size_t *vertex_fuse_map;          /* maps the vertex index of duplicate (i.e. within tolerance) vertices
                                       * int
                                       * together to a single vertex
                                       */
    short int *vertex_fuse_flag;      /* used during creation of vertex_fuse_map, indicates if the vertex
                                       * has already been tested for duplicates or the vertex is a duplicate
                                       * of an already processed vertex and does not need to be processed again
                                       */
    size_t vertex_fuse_offset;        /* subtract this value from libobj vertex index to index into the fuse
                                       * array to find the libobj vertex index this vertex was fused to
                                       */
    size_t num_vertex_fuse;           /* number of elements in vertex_fuse_map and vertex_fuse_flag arrays */
    size_t *texture_vertex_fuse_map;  /* same as vertex_fuse_map but for texture vertex */
    short int *texture_vertex_fuse_flag; /* same as vertex_fuse_flag but for texture vertex */
    size_t texture_vertex_fuse_offset;/* same as vertex_fuse_offset but for texture vertex */
    size_t num_texture_vertex_fuse;   /* same as num_vertex_fuse but for texture vertex */
};


/* triangle indices type */
struct ti_t { 
    void *index_arr_tri; /* triangle indices into vertex, normal, texture vertex lists */
    size_t num_tri;      /* number of triangles represented by index_arr_triangles */
    size_t max_tri;      /* maximum number of triangles based on current memory allocation */
    int tri_type;        /* i.e. FACE_V, FACE_TV, FACE_NV or FACE_TNV */
    size_t *vsi;         /* triangle vertex sort index array */
    size_t *vnsi;        /* triangle vertex normal sort index array */
    size_t *tvsi;        /* triangle texture vertex sort index array */
    size_t *uvi;         /* unique triangle vertex index array */
    size_t *uvni;        /* unique triangle vertex normal index array */
    size_t *utvi;        /* unique triangle texture vertex index array */
    size_t num_uvi;      /* number of unique triangle vertex indexes in uvi array */
    size_t num_uvni;     /* number of unique triangle vertex normal index in uvni array */
    size_t num_utvi;     /* number of unique triangle texture vertex index in utvi array */
    unsigned char bot_mode;        /* bot mode RT_BOT_PLATE, RT_BOT_PLATE_NOCOS, RT_BOT_SOLID, RT_BOT_SURFACE */
    fastf_t *bot_vertices;         /* array of floats for bot vertices [bot_num_vertices*3] */
    fastf_t *bot_thickness;        /* array of floats for face thickness [bot_num_faces] */
    fastf_t *bot_normals;          /* array of floats for bot normals [bot_num_normals*3] */
    fastf_t *bot_texture_vertices; /* array of floats for texture vertices [bot_num_texture_vertices*3] */
    int *bot_faces;                /* array of indices into bot_vertices array [bot_num_faces*3] */
    struct bu_bitv *bot_face_mode; /* bu_list of face modes for plate-mode-bot [bot_num_faces] */
    int *bot_face_normals;         /* array of indices into bot_normals array [bot_num_faces*3] */
    int *bot_textures;             /* array indices into bot_texture_vertices array */
    size_t bot_num_vertices;          /* number of vertices in bot_vertices array */
    size_t bot_num_faces;             /* number of faces in bot_faces array */
    size_t bot_num_normals;           /* number of normals in bot_normals array */
    size_t bot_num_texture_vertices;  /* number of textures in bot_texture_vertices array */
};


/* obj file global attributes type */
struct ga_t {
    const obj_polygonal_attributes_t *polyattr_list; /* obj_polygonal_attributes */
    obj_parser_t parser;     /* obj_parser_create */
    obj_contents_t contents; /* obj_fparse */
    size_t numPolyAttr;      /* obj_polygonal_attributes */
    size_t numGroups;        /* obj_groups */
    size_t numObjects;       /* obj_objects */
    size_t numMaterials;     /* obj_materials */
    size_t numTexmaps;       /* obj_texmaps */
    size_t numVerts;         /* obj_vertices */
    size_t numNorms;         /* obj_normals */
    size_t numTexCoords;     /* obj_texture_coord */
    size_t numNorFaces;      /* obj_polygonal_nv_faces */
    size_t numFaces;         /* obj_polygonal_v_faces */
    size_t numTexFaces;      /* obj_polygonal_tv_faces */ 
    size_t numTexNorFaces;   /* obj_polygonal_tnv_faces */ 
    const char * const *str_arr_obj_groups;    /* obj_groups */
    const char * const *str_arr_obj_objects;   /* obj_objects */
    const char * const *str_arr_obj_materials; /* obj_materials */
    const char * const *str_arr_obj_texmaps;   /* obj_texmaps */
    const float (*vert_list)[4];          /* obj_vertices */
    const float (*norm_list)[3];          /* obj_normals */
    const float (*texture_coord_list)[3]; /* obj_texture_coord */
    const size_t *attindex_arr_nv_faces;  /* obj_polygonal_nv_faces */
    const size_t *attindex_arr_v_faces;   /* obj_polygonal_v_faces */
    const size_t *attindex_arr_tv_faces;  /* obj_polygonal_tv_faces */
    const size_t *attindex_arr_tnv_faces; /* obj_polygonal_tnv_faces */
};

int test_face(struct ga_t *ga, struct gfi_t *gfi, size_t face_idx,
    fastf_t conv_factor, struct bn_tol *tol, int face_test_type,
    int force_retest);

/*
 * C O L L E C T _ G L O B A L _ O B J _ F I L E _ A T T R I B U T E S
 *
 * Collects object file attributes from the libobj library for use by
 * functions called later. Examples of collected attributes are the
 * quantity of each face type, the quantity of each grouping type, the
 * quantity of vertices, texture vertices, normals etc.
 */
void
collect_global_obj_file_attributes(struct ga_t *ga)
{
    size_t i = 0;

    ga->numPolyAttr = obj_polygonal_attributes(ga->contents, &ga->polyattr_list);

    if ((verbose > 1) || debug) {
        bu_log("OBJ FILE CONTENTS:\n");
    } else {
        bu_log("OBJ FILE CONTENT SUMMARY:\n");
    }

    ga->numGroups = obj_groups(ga->contents, &ga->str_arr_obj_groups);
    bu_log("\tTotal number of groups in OBJ file; numGroups = (%zu)\n", ga->numGroups);

    if ((verbose > 1) || debug) {
        bu_log("\tList of all groups i.e. 'g' in OBJ file\n");
        for (i = 0 ; i < ga->numGroups ; i++) {
            bu_log("\t\t(%zu)(%s)\n", i + 1, ga->str_arr_obj_groups[i]);
        }
    }

    ga->numObjects = obj_objects(ga->contents, &ga->str_arr_obj_objects);
    bu_log("\tTotal number of object groups in OBJ file; numObjects = (%zu)\n", ga->numObjects);

    if ((verbose > 1) || debug) {
        bu_log("\tList of all object groups i.e. 'o' in OBJ file\n");
        for (i = 0 ; i < ga->numObjects ; i++) {
            bu_log("\t\t(%zu)(%s)\n", i + 1, ga->str_arr_obj_objects[i]);
        }
    }

    ga->numMaterials = obj_materials(ga->contents, &ga->str_arr_obj_materials);
    bu_log("\tTotal number of material names in OBJ file; numMaterials = (%zu)\n", ga->numMaterials);

    if ((verbose > 1) || debug) {
        bu_log("\tList of all material names i.e. 'usemtl' in OBJ file\n");
        for (i = 0 ; i < ga->numMaterials ; i++) {
            bu_log("\t\t(%zu)(%s)\n", i + 1, ga->str_arr_obj_materials[i]);
        }
    }

    ga->numTexmaps = obj_texmaps(ga->contents, &ga->str_arr_obj_texmaps);
    bu_log("\tTotal number of texture map names in OBJ file; numTexmaps = (%zu)\n", ga->numTexmaps);

    if ((verbose > 1) || debug) {
        bu_log("\tList of all texture map names i.e. 'usemap' in OBJ file\n");
        for (i = 0 ; i < ga->numTexmaps ; i++) {
            bu_log("\t\t(%zu)(%s)\n", i + 1, ga->str_arr_obj_texmaps[i]);
        }
    }

    ga->numVerts = obj_vertices(ga->contents, &ga->vert_list);
    bu_log("\tTotal number of vertices in OBJ file; numVerts = (%zu)\n", ga->numVerts);

    ga->numNorms = obj_normals(ga->contents, &ga->norm_list);
    bu_log("\tTotal number of normals in OBJ file; numNorms = (%zu)\n", ga->numNorms);

    ga->numTexCoords = obj_texture_coord(ga->contents, &ga->texture_coord_list);
    bu_log("\tTotal number of texture coordinates in OBJ file; numTexCoords = (%zu)\n", ga->numTexCoords);

    ga->numNorFaces = obj_polygonal_nv_faces(ga->contents, &ga->attindex_arr_nv_faces);
    bu_log("\tNumber of oriented polygonal faces; numNorFaces = (%zu)\n", ga->numNorFaces);

    ga->numFaces = obj_polygonal_v_faces(ga->contents, &ga->attindex_arr_v_faces);
    bu_log("\tNumber of polygonal faces only identifed by vertices; numFaces = (%zu)\n", ga->numFaces);

    ga->numTexFaces = obj_polygonal_tv_faces(ga->contents, &ga->attindex_arr_tv_faces);
    bu_log("\tNumber of textured polygonal faces; numTexFaces = (%zu)\n", ga->numTexFaces);

    ga->numTexNorFaces = obj_polygonal_tnv_faces(ga->contents, &ga->attindex_arr_tnv_faces);
    bu_log("\tNumber of oriented textured polygonal faces; numTexNorFaces = (%zu)\n\n", ga->numTexNorFaces);

    return;
}


/*
 * C L E A N U P _ N A M E
 *
 * Replaces with underscore characters which are invalid for BRL-CAD
 * primitive names and file names.
 */
void
cleanup_name(struct bu_vls *outputObjectName_ptr)
{
    char *temp_str;
    size_t outputObjectName_length;
    size_t i;

    temp_str = bu_vls_addr(outputObjectName_ptr);

    /* length does not include null */
    outputObjectName_length = bu_vls_strlen(outputObjectName_ptr);

    for (i = 0 ; i < outputObjectName_length ; i++) {
        if (strchr("/\\=", (int)temp_str[i]) != (char *)NULL) {
            temp_str[i] = '_';
        }
    }

    return;
}


/*
 * C O M P _ B
 *
 * Compare function used by the functions qsort and bsearch for
 * sorting and searching an array of numbers.
 */
static int
comp_b(const void *p1, const void *p2)
{
    size_t i = * (size_t *) p1;
    size_t j = * (size_t *) p2;

    return (int)(i - j);
}


/*
 * C O M P
 *
 * Compare function used by the function qsort for sorting an index
 * into a multi-dimensional array.
 */
static int 
comp(const void *p1, const void *p2)
{
    size_t i = * (size_t *) p1;
    size_t j = * (size_t *) p2;

    return (int)(tmp_ptr[i] - tmp_ptr[j]);
}


/*
 * C O M P _ C
 *
 * Compare function used by the function qsort for sorting a 2D array
 * of numbers.
 */
static int 
comp_c(const void *p1, const void *p2)
{
    edge_arr_2D_t i = (edge_arr_2D_t) p1;
    edge_arr_2D_t j = (edge_arr_2D_t) p2;

    if ((i[0][0] - j[0][0]) != 0) {
	return (int)(i[0][0] - j[0][0]);
    } else {
	return (int)(i[0][1] - j[0][1]);
    }
}


/*
 * R E T R I E V E _ C O O R D _ I N D E X
 *
 * For a grouping of faces, retrieve the coordinates and obj file
 * indexes of a specific vertex within a specific face in this
 * grouping. If the face type indicates a type where some return
 * information is non-applicable, then this non-applicable information
 * will be undefined when this function returns.
 */
void                                    /* inputs: */
retrieve_coord_index(struct ga_t *ga,   /* obj file global attributes */
                     struct gfi_t *gfi, /* grouping face indices */
                     size_t fi,         /* face index number of grouping */
                     size_t vi,         /* vertex index number of face */
                                        /* outputs: */
                     fastf_t *vc,       /* vertex coordinates */
                     fastf_t *nc,       /* normal coordinates */
                     fastf_t *tc,       /* texture vertex coordinates */
                     fastf_t *w,        /* vertex weight */
                     size_t *vofi,      /* vertex obj file index */
                     size_t *nofi,      /* normal obj file index */
                     size_t *tofi)      /* texture vertex obj file index */
{
    arr_1D_t index_arr_faces_1D = NULL; 
    arr_2D_t index_arr_faces_2D = NULL; 
    arr_3D_t index_arr_faces_3D = NULL; 

    size_t fofi = gfi->obj_file_face_idx_arr[fi]; /* face obj file index */

    switch (gfi->face_type) {
        case FACE_V:
            index_arr_faces_1D = (arr_1D_t)(gfi->index_arr_faces);

            /* copy current vertex obj file index into vofi */ 
            *vofi = index_arr_faces_1D[fi][vi];

            /* used fused vertex index if available */
            if (gfi->vertex_fuse_map != NULL) {
                *vofi = gfi->vertex_fuse_map[*vofi - gfi->vertex_fuse_offset];
            }

            /* copy current vertex coordinates into vc */
            VMOVE(vc, ga->vert_list[*vofi]);
            /* copy current vertex weight into w */
            *w = ga->vert_list[*vofi][3];

            if (debug) {
                bu_log("fi=(%zu)vi=(%zu)fofi=(%zu)vofi=(%zu)v=(%f)(%f)(%f)w=(%f)\n", 
		       fi, vi, fofi+1, *vofi+1, vc[0], vc[1], vc[2], *w);
            }
            break;
        case FACE_TV:
            index_arr_faces_2D = (arr_2D_t)(gfi->index_arr_faces);
            /* copy current vertex coordinates into vc */
            VMOVE(vc, ga->vert_list[index_arr_faces_2D[fi][vi][0]]);
            /* copy current vertex weight into w */
            *w = ga->vert_list[index_arr_faces_2D[fi][vi][0]][3];
            /* copy current texture coordinate into tc */
            VMOVE(tc, ga->texture_coord_list[index_arr_faces_2D[fi][vi][1]]);
            /* copy current vertex obj file index into vofi */ 
            *vofi = index_arr_faces_2D[fi][vi][0];
            /* copy current texture coordinate obj file index into tofi */ 
            *tofi = index_arr_faces_2D[fi][vi][1];
            if (debug) {
                bu_log("fi=(%zu)vi=(%zu)fofi=(%zu)vofi=(%zu)tofi=(%zu)v=(%f)(%f)(%f)w=(%f)t=(%f)(%f)(%f)\n", 
		       fi, vi, fofi+1, *vofi+1, *tofi+1, vc[0], vc[1], vc[2], *w, tc[0], tc[1], tc[2]);
            }
            break;
        case FACE_NV:
            index_arr_faces_2D = (arr_2D_t)(gfi->index_arr_faces);

            /* copy current vertex obj file index into vofi */ 
            *vofi = index_arr_faces_2D[fi][vi][0];
            /* copy current normal obj file index into nofi */ 
            *nofi = index_arr_faces_2D[fi][vi][1];

            /* use fused vertex index if available */
            if (gfi->vertex_fuse_map != NULL) {
                *vofi = gfi->vertex_fuse_map[*vofi - gfi->vertex_fuse_offset];
            }

            /* copy current vertex coordinates into vc */
            VMOVE(vc, ga->vert_list[*vofi]);
            /* copy current vertex weight into w */
            *w = ga->vert_list[*vofi][3];
            /* copy current normal into nc */
            VMOVE(nc, ga->norm_list[*nofi]);

            if (debug) {
                bu_log("fi=(%zu)vi=(%zu)fofi=(%zu)vofi=(%zu)nofi=(%zu)v=(%f)(%f)(%f)w=(%f)n=(%f)(%f)(%f)\n", 
		       fi, vi, fofi+1, *vofi+1, *nofi+1, vc[0], vc[1], vc[2], *w, nc[0], nc[1], nc[2]);
            }
            break;
        case FACE_TNV:
            index_arr_faces_3D = (arr_3D_t)(gfi->index_arr_faces);
            /* copy current vertex coordinates into vc */
            VMOVE(vc, ga->vert_list[index_arr_faces_3D[fi][vi][0]]);
            /* copy current vertex weight into w */
            *w = ga->vert_list[index_arr_faces_3D[fi][vi][0]][3];
            /* copy current texture coordinate into tc */
            VMOVE(tc, ga->texture_coord_list[index_arr_faces_3D[fi][vi][1]]);
            /* copy current normal into nc */
            VMOVE(nc, ga->norm_list[index_arr_faces_3D[fi][vi][2]]);
            /* copy current vertex obj file index into vofi */ 
            *vofi = index_arr_faces_3D[fi][vi][0];
            /* copy current texture coordinate obj file index into tofi */ 
            *tofi = index_arr_faces_3D[fi][vi][1];
            /* copy current normal obj file index into nofi */ 
            *nofi = index_arr_faces_3D[fi][vi][2];
            if (debug) {
                bu_log("fi=(%zu)vi=(%zu)fofi=(%zu)vofi=(%zu)tofi=(%zu)nofi=(%zu)v=(%f)(%f)(%f)w=(%f)t=(%f)(%f)(%f)n=(%f)(%f)(%f)\n", 
		       fi, vi, fofi+1, *vofi+1, *tofi+1, *nofi+1, vc[0], vc[1], vc[2], *w, 
		       tc[0], tc[1], tc[2], nc[0], nc[1], nc[2]);
            }
            break;
    }
    return;
}


/*
 * R E T E S T _ G R O U P I N G _ F A C E S
 *
 * Within a given grouping of faces, test all the faces for degenerate
 * conditions such as duplicate vertex indexes or the distance between
 * any pair of vertices of a individual face are equal to or less than
 * the distance tolerance. Test results for each face of the grouping
 * is recorded to be used later. If a face was previously tested, this
 * function will retest the face.  Retesting is useful if a vertex
 * fuse was performed after the last testing of the faces.
 */
size_t
retest_grouping_faces(struct ga_t *ga,
                      struct gfi_t *gfi,
                      fastf_t conv_factor,  /* conversion factor from obj file units to mm */
                      int face_test_type,
                      struct bn_tol *tol)
{
    size_t face_idx = 0;
    size_t failed_face_count = 0;

    /* face_status is populated within test_face function */
    for (face_idx = 0 ; face_idx < gfi->num_faces ; face_idx++) {
        /* 1 passed into test_face indicates forced retest */
        if (test_face(ga, gfi, face_idx, conv_factor, tol, face_test_type, 1)) {
            failed_face_count++;
        }
    }

    return failed_face_count;
}

/*
 * F I N D _ L A S T _ U N I Q U E _ V E R T E X
 *
 * Returns the number of vertices in a face where the last vertex in
 * the vertex list is not the same as the first vertex. Essentially
 * this function finds the last unique vertex of the face. If the
 * returned count is used to indicate the number of vertices in the
 * face, the result is trailing vertices in the face which are
 * identical to the first vertex will be removed. This function
 * is a support function for the test_face function and allows some
 * cases of zero length edges to be removed from a face.
 */
size_t
find_last_unique_vertex(struct ga_t *ga,   /* obj file global attributes */
                        struct gfi_t *gfi, /* grouping face indices */
                        size_t face_idx)   /* face index number of grouping */
{
    fastf_t tmp_v[3] = {0.0, 0.0, 0.0}; /* temporary vertex */
    fastf_t tmp_w = 0.0;                /* temporary weight */
    fastf_t tmp_n[3] = {0.0, 0.0, 0.0}; /* temporary normal */
    fastf_t tmp_t[3] = {0.0, 0.0, 0.0}; /* temporary texture vertex */
    size_t vofi = 0;                    /* vertex obj file index */
    size_t nofi = 0;                    /* normal obj file index */
    size_t tofi = 0;                    /* texture vertex obj file index */
    size_t num_vertex = 0;
    int done = 0;
    fastf_t first_vertex[3] = {0.0, 0.0, 0.0};
    fastf_t last_vertex[3] = {0.0, 0.0, 0.0};
    size_t first_vertex_index = 0;
    size_t last_vertex_index = 0;

    num_vertex = gfi->num_vertices_arr[face_idx];

    if (num_vertex >= 3) {
        retrieve_coord_index(ga, gfi, face_idx, 0, tmp_v, tmp_n, tmp_t,
			     &tmp_w, &vofi, &nofi, &tofi); 
        VMOVE(first_vertex, tmp_v);
        first_vertex_index = vofi;

        while ((num_vertex >= 3) && !done) {
            retrieve_coord_index(ga, gfi, face_idx, num_vertex - 1, tmp_v, tmp_n, tmp_t,
				 &tmp_w, &vofi, &nofi, &tofi); 
            VMOVE(last_vertex, tmp_v);
            last_vertex_index = vofi;

            if ((first_vertex_index == last_vertex_index) || VEQUAL(first_vertex, last_vertex)) {
                num_vertex--;
            } else {
                done = 1;
            } 
        }
    }

    return num_vertex;
}

/*
 * T E S T _ F A C E
 *
 * Within a given grouping of faces, test an individual face for
 * degenerate conditions such as duplicate vertex indexes or the
 * distance between any pair of vertices of a individual face are
 * equal to or less than the distance tolerance. The test result is
 * returned by this function and is recorded so it can be used later.
 * If a face was previously tested, this function will not retest the
 * face unless the force_retest flag is set. If the face test was
 * TEST_NUM_VERT, the stored result is untested since this is not an
 * extensive test. Return codes for tests are below. Note: the stored
 * test result value is the return code plus one, since 0 in the
 * results list indicates an untested face and one indicates a valid
 * face. This function also removes trailing vertices from a face
 * which are identical to the first vertex in the face resulting in
 * zero length edges being removed when this condition occurs. 
 *
 * Returns:
 * 0 valid face
 * 1 degenerate, <3 vertices
 * 2 degenerate, duplicate vertex indexes
 * 3 degenerate, vertices too close
 *
 * Recommended function improvements:
 * - Optionally remove all zero length edges from a face
 */
int 
test_face(struct ga_t *ga,
          struct gfi_t *gfi,
          size_t face_idx,
          fastf_t conv_factor,  /* conversion factor from obj file units to mm */
          struct bn_tol *tol,
          int face_test_type,   /* TEST_ALL, TEST_NUM_VERT */
          int force_retest)     /* force the retest of this face if it was already
                                 * tested, this is useful when we know a retest is
                                 * required such as when the previous test was performed
                                 * without fused vertices and now vertices are fused
                                 */
{
    fastf_t tmp_v_o[3] = {0.0, 0.0, 0.0}; /* temporary vertex, referenced from outer loop */
    fastf_t tmp_v_i[3] = {0.0, 0.0, 0.0}; /* temporary vertex, referenced from inner loop */
    fastf_t tmp_w = 0.0;                  /* temporary weight */
    fastf_t tmp_n[3] = {0.0, 0.0, 0.0};   /* temporary normal */
    fastf_t tmp_t[3] = {0.0, 0.0, 0.0};   /* temporary texture vertex */
    size_t nofi = 0;                      /* normal obj file index */
    size_t tofi = 0;                      /* texture vertex obj file index */
    fastf_t distance_between_vertices = 0.0;
    size_t vofi_o = 0; /* vertex obj file index, referenced from outer loop */
    size_t vofi_i = 0; /* vertex obj file index, referenced from inner loop */

    size_t vert = 0;
    size_t vert2 = 0;
    int degenerate_face = 0;

    /* if this face has already been tested, return the previous
     * results unless the force_retest flag is set
     */
    if (!force_retest) {
        if (gfi->face_status[face_idx]) {
            /* subtract 1 since face_status value of 1 indicates
             * success and 0 indicates untested
             */
            return (gfi->face_status[face_idx] - 1);
        }
    }

    /* removes trailing vertices identical to the first vertex */
    gfi->num_vertices_arr[face_idx] = find_last_unique_vertex(ga, gfi, face_idx);

    /* added 1 to internal index values so warning message index
     * values matches obj file index numbers. this is because obj file
     * indexes start at 1, internally indexes start at 0.  changed the
     * warning message if grouping_type is GRP_NONE because the group
     * name and grouping index has no meaning to the user if grouping
     * type is GRP_NONE
     */
    if (gfi->num_vertices_arr[face_idx] < 3) {
        degenerate_face = 1;
        if (gfi->grouping_type != GRP_NONE) { 
            if (verbose || debug) {
                bu_log("WARNING: removed degenerate face (reason: < 3 vertices); obj file face group name = (%s) obj file face grouping index = (%zu) obj file face index = (%zu)\n",
		       bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1,
		       gfi->obj_file_face_idx_arr[face_idx] + 1);
            }
        } else {
            if (verbose || debug) {
                bu_log("WARNING: removed degenerate face (reason: < 3 vertices); obj file face index = (%zu)\n",
		       gfi->obj_file_face_idx_arr[face_idx] + 1);
            }
        }
    }

    if (face_test_type == TEST_NUM_VERT) {
        /* set face test status to zero (i.e. untested) since just
         * counting the number of vertices is not a true test
         */
        gfi->face_status[face_idx] = 0;
        return degenerate_face;
    }

    while ((vert < gfi->num_vertices_arr[face_idx]) && !degenerate_face) {
        vert2 = vert+1;
        while ((vert2 < gfi->num_vertices_arr[face_idx]) && !degenerate_face) {
            retrieve_coord_index(ga, gfi, face_idx, vert, tmp_v_o,
                                 tmp_n, tmp_t, &tmp_w, &vofi_o, &nofi, &tofi); 
            retrieve_coord_index(ga, gfi, face_idx, vert2, tmp_v_i,
                                 tmp_n, tmp_t, &tmp_w, &vofi_i, &nofi, &tofi); 
            if (vofi_o == vofi_i) {
                /* test for duplicate vertex indexes in face */
                degenerate_face = 2;
                if (gfi->grouping_type != GRP_NONE) {
                    if (verbose || debug) {
                        bu_log("WARNING: removed degenerate face (reason: duplicate vertex index); obj file face group name = (%s) obj file face grouping index = (%zu) obj file face index = (%zu) obj file vertex index = (%zu)\n",
			       bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1,
			       gfi->obj_file_face_idx_arr[face_idx] + 1, vofi_o + 1);
                    }
                } else {
                    if (verbose || debug) {
                        bu_log("WARNING: removed degenerate face (reason: duplicate vertex index); obj file face index = (%zu) obj file vertex index = (%zu)\n",
			       gfi->obj_file_face_idx_arr[face_idx] + 1, vofi_o + 1);
                    }
                }
            } else {
                /* test for vertices closer than tol.dist
                 * tol.dist is assumed to be mm
                 */
                VSCALE(tmp_v_o, tmp_v_o, conv_factor);
                VSCALE(tmp_v_i, tmp_v_i, conv_factor);
                if (bn_pt3_pt3_equal(tmp_v_o, tmp_v_i, tol)) {
                    distance_between_vertices = DIST_PT_PT(tmp_v_o, tmp_v_i);
                    degenerate_face = 3;
                    if (gfi->grouping_type != GRP_NONE) {
                        if (verbose || debug) {
                            bu_log("WARNING: removed degenerate face (reason: vertices too close); obj file face group name = (%s) obj file face grouping index = (%zu) obj file face index = (%zu) obj file vertice indexes (%zu) vs (%zu) tol.dist = (%lfmm) dist = (%fmm)\n",
				   bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1,
				   gfi->obj_file_face_idx_arr[face_idx] + 1, vofi_o + 1,
				   vofi_i + 1, tol->dist, distance_between_vertices);
                        }
                    } else {
                        if (verbose || debug) {
                            bu_log("WARNING: removed degenerate face (reason: vertices too close); obj file face index = (%zu) obj file vertice indexes (%zu) vs (%zu) tol.dist = (%lfmm) dist = (%fmm)\n",
				   gfi->obj_file_face_idx_arr[face_idx] + 1, vofi_o + 1, vofi_i + 1,
				   tol->dist, distance_between_vertices);
                        }
                    }
                }
            }
            vert2++;
        }
        vert++;
    }

    /* add 1 since zero within face_status indicates untested */
    gfi->face_status[face_idx] = degenerate_face + 1;

    return degenerate_face;
}


/*
 * F R E E _ G F I
 *
 * Releases the memory allocated for the contents of the gfi structure
 * and the gfi structure itself. The gfi structure contains the
 * 'grouping face indices' into the libobj structures (and supporting
 * attributes and indexes) for all the faces of a grouping of faces. A
 * grouping of faces corresponds to an individual BRL-CAD primitive.
 */
void
free_gfi(struct gfi_t **gfi)
{
    if (*gfi != NULL) {
        bu_vls_free((*gfi)->raw_grouping_name);
        bu_vls_free((*gfi)->primitive_name);
        bu_free((*gfi)->raw_grouping_name, "(*gfi)->raw_grouping_name");
        bu_free((*gfi)->primitive_name, "(*gfi)->primitive_name");
        bu_free((*gfi)->index_arr_faces, "(*gfi)->index_arr_faces");
        bu_free((*gfi)->num_vertices_arr, "(*gfi)->num_vertices_arr");
        bu_free((*gfi)->obj_file_face_idx_arr, "(*gfi)->obj_file_face_idx_arr");
        bu_free((*gfi)->face_status, "(*gfi)->face_status");
        if ((*gfi)->vertex_fuse_map != NULL) {
            bu_free((*gfi)->vertex_fuse_map, "(*gfi)->vertex_fuse_map");
        }
        if ((*gfi)->texture_vertex_fuse_map != NULL) {
            bu_free((*gfi)->texture_vertex_fuse_map, "(*gfi)->texture_vertex_fuse_map");
        }
        bu_free(*gfi, "*gfi");
        *gfi = NULL;
    }
    return;
}


/*
 * C O L L E C T _ G R O U P I N G _ F A C E S _ I N D E X E S
 *
 * Collects the face indexes into the libobj structures for a specific
 * grouping of faces. The grouping_index identifies the grouping to be
 * collected and corresponds to the index of the grouping defined in the obj
 * file. It is ignored if grouping_type is GROUP_NONE. This function allocates
 * all memory needed for the gfi structure and its contents. Gfi is expected to
 * be a null pointer when passed to this function. The gfi structure and its
 * contents is expected to be freed outside this function. It is not possible
 * to directly retrieve a list of faces for a specific grouping from the libobj
 * structures, therefore all faces in the obj file must be traversed and tested
 * if they are a member of a grouping, if a face is a member, the index of the
 * found face is stored in a list to be processed later. This function creates
 * this list of indexes to the face indexes in the libobj structures.
 */
void
collect_grouping_faces_indexes(struct ga_t *ga,
                               struct gfi_t **gfi,
                               int face_type,
                               int grouping_type,
                               size_t grouping_index)
{
    /* number of faces of the current face_type in the entire obj file */
    size_t numFaces = 0;
    size_t i = 0;
    const size_t *attindex_arr_faces = (const size_t *)NULL;
    int found = 0;
    const char *name_str = (char *)NULL;
    size_t setsize = 0;
    const size_t *indexset_arr;
    size_t groupid = 0;
    size_t *num_vertices_arr_tmp = NULL;
    size_t *obj_file_face_idx_arr_tmp = NULL;

    const size_t (*index_arr_v_faces) = NULL;      /* used by v_faces */
    const size_t (*index_arr_tv_faces)[2] = NULL;  /* used by tv_faces */
    const size_t (*index_arr_nv_faces)[2] = NULL;  /* used by nv_faces */
    const size_t (*index_arr_tnv_faces)[3] = NULL; /* used by tnv_faces */

    arr_1D_t index_arr_faces_1D = NULL; 
    arr_2D_t index_arr_faces_2D = NULL; 
    arr_3D_t index_arr_faces_3D = NULL; 

    /* number of faces of the current face_type from the entire obj
     * file which is found in the current grouping_type and current
     * group
     */
    size_t numFacesFound = 0;
    size_t faceIndex = 0;

    /* number of additional elements to allocate memory for when the
     * currently allocated memory is exhausted
     */ 
    const size_t max_faces_increment = 128;

    if (*gfi != NULL) {
	bu_exit(1, "ERROR: function collect_grouping_faces_indexes passed "
		"non-null for gfi\n");

        return;
    }

    switch (face_type) {
        case FACE_V:
            numFaces = ga->numFaces; 
            attindex_arr_faces = ga->attindex_arr_v_faces;
            break;
        case FACE_TV:
            numFaces = ga->numTexFaces; 
            attindex_arr_faces = ga->attindex_arr_tv_faces;
            break;
        case FACE_NV:
            numFaces = ga->numNorFaces;
            attindex_arr_faces = ga->attindex_arr_nv_faces;
            break;
        case FACE_TNV:
            numFaces = ga->numTexNorFaces;
            attindex_arr_faces = ga->attindex_arr_tnv_faces;
            break;
    }

    /* traverse list of all faces in OBJ file of current face_type */
    for (i = 0; i < numFaces; i++) {

        const obj_polygonal_attributes_t *face_attr;
        face_attr = ga->polyattr_list + attindex_arr_faces[i];

        /* reset for next face */
        found = 0;

        /* for each type of grouping, check if current face is in
	 * current grouping.
	 */
        switch (grouping_type) {
            case GRP_NONE:
                found = 1;
                /* since there is no grouping, still need a somewhat
                 * useful name for the brlcad primitive and region,
                 * set the name to the face_type which is the inherent
                 * grouping
                 */
                switch (face_type) {
                    case FACE_V:
                        name_str = "v";
                        break;
                    case FACE_TV:
                        name_str = "tv";
                        break;
                    case FACE_NV:
                        name_str = "nv";
                        break;
                    case FACE_TNV:
                        name_str = "tnv";
                        break;
                }
                break;
            case GRP_GROUP:
                /* setsize is the number of groups the current nv_face
		 * belongs to.
		 */
		setsize =
		    obj_groupset(ga->contents, face_attr->groupset_index,
		    &indexset_arr);

                /* loop through each group this face is in */
                for (groupid = 0 ; groupid < setsize ; groupid++) {
                    /* if true, current face is in current group grouping */
                    if (grouping_index == indexset_arr[groupid]) {
			int index = indexset_arr[groupid];
                        name_str = ga->str_arr_obj_groups[index];
                        found = 1;
                    }
                }
                break;
            case GRP_OBJECT:
                /* if true, current face is in current object grouping */
                if (grouping_index == face_attr->object_index) {
                    found = 1;
                    name_str =
			ga->str_arr_obj_objects[face_attr->object_index];
                }
                break;
            case GRP_MATERIAL:
                /* if true, current face is in current material grouping */
                if (grouping_index == face_attr->material_index) {
                    found = 1;
                    name_str =
			ga->str_arr_obj_materials[face_attr->material_index];
                }
                break;
            case GRP_TEXTURE:
                /* if true, current face is in current texture map grouping */
                if (grouping_index == face_attr->texmap_index) {
                    found = 1;
                    name_str = ga->str_arr_obj_texmaps[face_attr->texmap_index];
                }
                break;
        } /* switch (grouping_type) */

        /* if found the first face allocate the output structure and
         * initial allocation of the index_arr_faces, num_vertices_arr
         * and obj_file_face_idx_arr arrays
         */
        if (found && (numFacesFound == 0)) {
            /* allocate memory for gfi structure */
            *gfi = (struct gfi_t *)bu_calloc(1, sizeof(struct gfi_t), "gfi");

            /* initialize gfi structure */
            (*gfi)->index_arr_faces = (void *)NULL;
            (*gfi)->num_vertices_arr = (size_t *)NULL;
            (*gfi)->obj_file_face_idx_arr = (size_t *)NULL;
            (*gfi)->raw_grouping_name = (struct bu_vls *)NULL;
            (*gfi)->primitive_name = (struct bu_vls *)NULL;
            (*gfi)->num_faces = 0;
            (*gfi)->max_faces = 0;
            (*gfi)->face_status = (short int *)NULL;
            (*gfi)->closure_status = SURF_UNTESTED;
            (*gfi)->tot_vertices = 0;
            (*gfi)->face_type = 0;
            (*gfi)->grouping_type = 0;
            (*gfi)->grouping_index = 0;
            (*gfi)->vertex_fuse_map = (size_t *)NULL;
            (*gfi)->vertex_fuse_flag = (short int *)NULL;
            (*gfi)->vertex_fuse_offset = 0;
            (*gfi)->num_vertex_fuse = 0;
            (*gfi)->texture_vertex_fuse_map = (size_t *)NULL;
            (*gfi)->texture_vertex_fuse_flag = (short int *)NULL;
            (*gfi)->texture_vertex_fuse_offset = 0;
            (*gfi)->num_texture_vertex_fuse = 0;

            /* set face_type, grouping_type, grouping_index inside gfi
             * structure, the purpose of this is so functions called
             * later do not need to pass this in seperately
             */
            (*gfi)->face_type = face_type;
            (*gfi)->grouping_type = grouping_type;
            if (grouping_type != GRP_NONE) {
                (*gfi)->grouping_index = grouping_index;
            } else {
                /* set grouping_index to face_type since if grouping_type is
                 * GRP_NONE, inherently we are grouping by face_type and we
                 * need a number for unique naming of the brlcad objects.
                 */
                (*gfi)->grouping_index = (size_t)abs(face_type);

            }
            /* allocate and initialize variable length string (vls) for
	     * raw_grouping_name
	     */
            (*gfi)->raw_grouping_name = bu_vls_vlsinit();

            /* allocate and initialize variable length string (vls) for
	     * primitive_name
	     */
            (*gfi)->primitive_name = bu_vls_vlsinit();

            /* only need to copy in the grouping name for the first
             * face found within the grouping since all the faces in
             * the grouping will have the same grouping name
             */
            bu_vls_strcpy((*gfi)->raw_grouping_name, name_str);
	    bu_free(name_str, "name_str");

            /* sets initial number of elements to allocate memory for */
            (*gfi)->max_faces = max_faces_increment;

            (*gfi)->num_vertices_arr =
		(size_t *)bu_calloc((*gfi)->max_faces, sizeof(size_t),
		"num_vertices_arr");

            (*gfi)->obj_file_face_idx_arr =
		(size_t *)bu_calloc((*gfi)->max_faces, sizeof(size_t),
		"obj_file_face_idx_arr");

            /* allocate initial memory for (*gfi)->index_arr_faces based on
	     * face_type
	     */
            switch (face_type) {
                case FACE_V:
                    (*gfi)->index_arr_faces =
			(void *)bu_calloc((*gfi)->max_faces, sizeof(arr_1D_t),
			"index_arr_faces");

                    index_arr_faces_1D = (arr_1D_t)((*gfi)->index_arr_faces);
                    break;
                case FACE_TV:
                case FACE_NV:
                    (*gfi)->index_arr_faces =
			(void *)bu_calloc((*gfi)->max_faces, sizeof(arr_2D_t),
			"index_arr_faces");

                    index_arr_faces_2D = (arr_2D_t)((*gfi)->index_arr_faces);
                    break;
                case FACE_TNV:
                    (*gfi)->index_arr_faces =
			(void *)bu_calloc((*gfi)->max_faces, sizeof(arr_3D_t),
				"index_arr_faces");

                    index_arr_faces_3D = (arr_3D_t)((*gfi)->index_arr_faces);
                    break;
            } /* switch (face_type) */
        } /* if (found && (numFacesFound == 0)) */

        if (found) {
	    numFacesFound++;
	    faceIndex = numFacesFound - 1;

            /* if needed, increase size of (*gfi)->num_vertices_arr and
	     * (*gfi)->index_arr_faces
	     */
            if (faceIndex >= (*gfi)->max_faces) {
                (*gfi)->max_faces += max_faces_increment;

                num_vertices_arr_tmp =
		    (size_t *)bu_realloc((*gfi)->num_vertices_arr,
		    sizeof(size_t) * (*gfi)->max_faces,
		    "num_vertices_arr_tmp");

                (*gfi)->num_vertices_arr = num_vertices_arr_tmp;

                obj_file_face_idx_arr_tmp =
		    (size_t *)bu_realloc((*gfi)->obj_file_face_idx_arr,
		    sizeof(size_t) * (*gfi)->max_faces,
		    "obj_file_face_idx_arr_tmp");

                (*gfi)->obj_file_face_idx_arr = obj_file_face_idx_arr_tmp;

                switch (face_type) {
                    case FACE_V:
                        (*gfi)->index_arr_faces =
			    (void *)bu_realloc(index_arr_faces_1D,
			    sizeof(arr_1D_t) * (*gfi)->max_faces,
			    "index_arr_faces");

                        index_arr_faces_1D =
			    (arr_1D_t)((*gfi)->index_arr_faces);

                        break;
                    case FACE_TV:
                    case FACE_NV:
                        (*gfi)->index_arr_faces =
			    (void *)bu_realloc(index_arr_faces_2D,
			    sizeof(arr_2D_t) * (*gfi)->max_faces,
			    "index_arr_faces");

                        index_arr_faces_2D =
			    (arr_2D_t)((*gfi)->index_arr_faces);

                        break;
                    case FACE_TNV:
                        (*gfi)->index_arr_faces =
			    (void *)bu_realloc(index_arr_faces_3D,
			    sizeof(arr_3D_t) * (*gfi)->max_faces,
			    "index_arr_faces");

                        index_arr_faces_3D =
			    (arr_3D_t)((*gfi)->index_arr_faces);

                        break;
                }
            }

            /* assign obj file face index into array for tracking
             * errors back to the face within the obj file
             */
            (*gfi)->obj_file_face_idx_arr[faceIndex] =
		attindex_arr_faces[i];

            switch (face_type) {
                case FACE_V:
                    (*gfi)->num_vertices_arr[faceIndex] =
			obj_polygonal_v_face_vertices(ga->contents, i,
				&index_arr_v_faces);

                    index_arr_faces_1D[faceIndex] = index_arr_v_faces;
                    break;
                case FACE_TV:
                    (*gfi)->num_vertices_arr[faceIndex] =
			obj_polygonal_tv_face_vertices(ga->contents, i,
				&index_arr_tv_faces);

                    index_arr_faces_2D[faceIndex] = index_arr_tv_faces;
                    break;
                case FACE_NV:
                    (*gfi)->num_vertices_arr[faceIndex] =
			obj_polygonal_nv_face_vertices(ga->contents, i,
				&index_arr_nv_faces);

                    index_arr_faces_2D[faceIndex] = index_arr_nv_faces;
                    break;
                case FACE_TNV:
                    (*gfi)->num_vertices_arr[faceIndex] =
			obj_polygonal_tnv_face_vertices(ga->contents, i,
				&index_arr_tnv_faces);

                    index_arr_faces_3D[faceIndex] = index_arr_tnv_faces;
                    break;
            }
            (*gfi)->tot_vertices += (*gfi)->num_vertices_arr[faceIndex];
        } /* if (found) */
    } /* for (i = 0; i < numFaces; i++) */

    if (numFacesFound) { 
        (*gfi)->num_faces = numFacesFound;

        (*gfi)->face_status =
	    (short int *)bu_calloc((*gfi)->num_faces, sizeof(short int),
	    "face_status");

        /* initialize array */
        for (i = 0; i < (*gfi)->num_faces; i++) {
            (*gfi)->face_status[i] = 0;
        }
    } else {
        *gfi = NULL; /* this should already be null if no faces were found */
    }

    return;
} /* collect_grouping_faces_indexes */


/*
 * P O P U L A T E _ T R I A N G L E _ I N D E X E S
 *
 * Populate the triangle index structure with the vertex indexes of
 * one or more triangles which represent a single face. This function
 * triangulates a face and places these triangles into the triangle
 * indexes (ti) structure to be processed later. Only convex
 * faces/polygons with coplanar vertices can be accurately
 * triangulated. It is assumed that when this function is called, the
 * number of face vertices is >= 3. This function allocates the memory
 * for the contents of the triangle indexes (ti) structure and this
 * memory is expected to be freed outside this function.
 *
 * Recommended function improvements:
 * - Correctly triangulate more than convex faces
 * - Warn of faces which may not be triangulated correctly
 */
void 
populate_triangle_indexes(struct ga_t *ga,
                          struct gfi_t *gfi,
                          struct ti_t *ti,
                          size_t face_idx,
                          int texture_mode,  /* PROC_TEX, IGNR_TEX */    
                          int normal_mode)   /* PROC_NORM, IGNR_NORM */
{
    size_t vert_idx = 0;                 /* index into vertices within for-loop */
    size_t idx = 0;                      /* sub index */
    size_t num_new_tri = 0;              /* number of new triangles to create */
    fastf_t tmp_v[3] = {0.0, 0.0, 0.0};  /* temporary vertex */
    fastf_t tmp_n[3] = {0.0, 0.0, 0.0};  /* temporary normal */
    fastf_t tmp_t[3] = {0.0, 0.0, 0.0};  /* temporary texture vertex */
    fastf_t tmp_w = 0.0; /* temporary weight */
    size_t vofi = 0;     /* vertex obj file index */
    size_t nofi = 0;     /* normal obj file index */
    size_t tofi = 0;     /* texture vertex obj file index */
    size_t svofi = 0;    /* start vertex obj file index */
    size_t snofi = 0;    /* start normal obj file index */
    size_t stofi = 0;    /* start texture vertex obj file index */
    size_t max_tri_increment = 128;

    tri_arr_1D_t index_arr_tri_1D = NULL;
    tri_arr_2D_t index_arr_tri_2D = NULL;
    tri_arr_3D_t index_arr_tri_3D = NULL;

    if (ti->index_arr_tri == (void *)NULL) {

        /* adjust ti->tri_type for texture_mode and normal_mode */
        switch (gfi->face_type) {
            case FACE_V:
                ti->tri_type = gfi->face_type;
                break;
            case FACE_TV:
                if (texture_mode == PROC_TEX) {
                    ti->tri_type = gfi->face_type;
                } else {
                    ti->tri_type = FACE_V;
                }
                break;
            case FACE_NV:
                if (normal_mode == PROC_NORM) {
                    ti->tri_type = gfi->face_type;
                } else {
                    ti->tri_type = FACE_V;
                }
                break;
            case FACE_TNV:
                if ((texture_mode == PROC_TEX) && (normal_mode == PROC_NORM)) {
                    ti->tri_type = gfi->face_type;
                } else if ((texture_mode == IGNR_TEX) && (normal_mode == IGNR_NORM)) {
                    ti->tri_type = FACE_V;
                } else if ((texture_mode == PROC_TEX) && (normal_mode == IGNR_NORM)) {
                    ti->tri_type = FACE_TV;
                } else {
                    ti->tri_type = FACE_NV;
                }
                break;
        }

        /* the initial size will be the number of faces in the grouping */
        ti->max_tri = gfi->num_faces;

        /* allocate memory for initial 'ti->index_arr_tri' array */
        switch (ti->tri_type) {
            case FACE_V:
                /* 3 vertice indexes */
                ti->index_arr_tri = (void *)bu_calloc(ti->max_tri * 3, sizeof(size_t), "triangle_indexes");
                break;
            case FACE_TV:
                /* 3 vertice indexes + 3 texture vertice indexes */
            case FACE_NV:
                /* 3 vertice indexes + 3 normal indexes */
                ti->index_arr_tri = (void *)bu_calloc(ti->max_tri * 6, sizeof(size_t), "triangle_indexes");
                break;
            case FACE_TNV:
                /* 3 vertice indexes + 3 normal indexes + 3 texture vertice indexes */
                ti->index_arr_tri = (void *)bu_calloc(ti->max_tri * 9, sizeof(size_t), "triangle_indexes");
                break;
        }
    }

    switch (ti->tri_type) {
        case FACE_V:
            index_arr_tri_1D = (tri_arr_1D_t)ti->index_arr_tri;
            break;
        case FACE_TV:
        case FACE_NV:
            index_arr_tri_2D = (tri_arr_2D_t)ti->index_arr_tri;
            break;
        case FACE_TNV:
            index_arr_tri_3D = (tri_arr_3D_t)ti->index_arr_tri;
            break;
    }

    /* compute number of new triangles to create */
    if (gfi->num_vertices_arr[face_idx] > 3) {
        num_new_tri = gfi->num_vertices_arr[face_idx] - 2;
    } else {
        num_new_tri = 1;
    }

    /* find start indexes */
    retrieve_coord_index(ga, gfi, face_idx, 0, tmp_v, tmp_n, tmp_t, &tmp_w, &svofi, &snofi, &stofi); 

    /* populate triangle indexes array, if necessary, triangulate face */
    for (vert_idx = 0 ; vert_idx < num_new_tri ; vert_idx++) {
        for (idx = 0 ; idx < 3 ; idx++) {
            if (!idx) {
                /* use the same vertex for the start of each triangle */
                vofi = svofi;
                nofi = snofi;
                tofi = stofi;
            } else {
                retrieve_coord_index(ga, gfi, face_idx, vert_idx + idx, tmp_v, tmp_n, 
                                     tmp_t, &tmp_w, &vofi, &nofi, &tofi); 
            }
            switch (ti->tri_type) {
                case FACE_V:
                    index_arr_tri_1D[ti->num_tri][idx] = vofi;
                    break;
                case FACE_TV:
                    index_arr_tri_2D[ti->num_tri][idx][0] = vofi;
                    index_arr_tri_2D[ti->num_tri][idx][1] = tofi;
                    break;
                case FACE_NV:
                    index_arr_tri_2D[ti->num_tri][idx][0] = vofi;
                    index_arr_tri_2D[ti->num_tri][idx][1] = nofi;
                    break;
                case FACE_TNV:
                    index_arr_tri_3D[ti->num_tri][idx][0] = vofi;
                    index_arr_tri_3D[ti->num_tri][idx][1] = tofi;
                    index_arr_tri_3D[ti->num_tri][idx][2] = nofi;
                    break;
            }
        }
        ti->num_tri++; /* increment this at the end since arrays start at zero */

        /* if needed, increase size of 'ti->index_arr_tri' array */
        if (ti->num_tri >= ti->max_tri) {
            ti->max_tri += max_tri_increment;
            switch (ti->tri_type) {
                case FACE_V:
                    ti->index_arr_tri = (void *)bu_realloc(index_arr_tri_1D,
							   sizeof(size_t) * ti->max_tri * 3, "index_arr_tri");
                    index_arr_tri_1D = (tri_arr_1D_t)(ti->index_arr_tri);
                    break;
                case FACE_TV:
                case FACE_NV:
                    ti->index_arr_tri = (void *)bu_realloc(index_arr_tri_2D,
							   sizeof(size_t) * ti->max_tri * 6, "index_arr_tri");
                    index_arr_tri_2D = (tri_arr_2D_t)(ti->index_arr_tri);
                    break;
                case FACE_TNV:
                    ti->index_arr_tri = (void *)bu_realloc(index_arr_tri_3D,
							   sizeof(size_t) * ti->max_tri * 9, "index_arr_tri");
                    index_arr_tri_3D = (tri_arr_3D_t)(ti->index_arr_tri);
                    break;
            }
        }
    }
    return;
}


/*
 * P O P U L A T E _ S O R T _ I N D E X E S
 *
 * Allocate the memory for and populate the sort-indexes used to sort
 * the necessary libobj vertex indexes. The sort-indexes created are
 * determined by the current face_type. Sorting of these indexes is
 * necessary as part of the process of creating a unique set of
 * vertices to build a bot primitive.
 */
void
populate_sort_indexes(struct ti_t *ti)
{
    size_t idx = 0;
    size_t num_indexes = ti->num_tri * 3;

    switch (ti->tri_type) {
        case FACE_V:
            /* vsi ... vertex sort index */
            ti->vsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vsi");
            for (idx = 0 ; idx < num_indexes ; idx++) {
                ti->vsi[idx] = idx;
            }
            break;
        case FACE_TV:
            /* vsi ... vertex sort index */
            ti->vsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vsi");
            /* tvsi ... texture vertex sort index */
            ti->tvsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->tvsi");
            for (idx = 0 ; idx < num_indexes ; idx++) {
                ti->vsi[idx] = idx * 2;
                ti->tvsi[idx] = (idx * 2) + 1;
            }
            break;
        case FACE_NV:
            /* vsi ... vertex sort index */
            ti->vsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vsi");
            /* vnsi ... vertex normal sort index */
            ti->vnsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vnsi");
            for (idx = 0 ; idx < num_indexes ; idx++) {
                ti->vsi[idx] = idx * 2;
                ti->vnsi[idx] = (idx * 2) + 1;
            }
            break;
        case FACE_TNV:
            /* vsi ... vertex sort index */
            ti->vsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vsi");
            /* tvsi ... texture vertex sort index */
            ti->tvsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->tvsi");
            /* vnsi ... vertex normal sort index */
            ti->vnsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vnsi");
            for (idx = 0 ; idx < num_indexes ; idx++) {
                ti->vsi[idx] = idx * 3;
                ti->tvsi[idx] = (idx * 3) + 1;
                ti->vnsi[idx] = (idx * 3) + 2;
            }
            break;
    }
    return;
}


/*
 * S O R T _ I N D E X E S
 *
 * Sort the sort-indexes used to sort the necessary libobj vertex
 * indexes. Sorting of these indexes is necessary as part of the
 * process of creating a unique set of vertices to build a bot
 * primitive.
 */
void 
sort_indexes(struct ti_t *ti)
{
    size_t num_indexes = ti->num_tri * 3;

    /* tmp_ptr is global which is required for qsort */
    tmp_ptr = (size_t *)ti->index_arr_tri;

    /* process vertex indexes */
    qsort(ti->vsi, num_indexes, sizeof ti->vsi[0],
	  (int (*)(const void *a, const void *b))comp);

    /* process vertex normal indexes */
    if (ti->tri_type == FACE_NV || ti->tri_type == FACE_TNV) {
        qsort(ti->vnsi, num_indexes, sizeof ti->vnsi[0],
	      (int (*)(const void *a, const void *b))comp);
    }

    /* process texture vertex indexes */
    if (ti->tri_type == FACE_TV || ti->tri_type == FACE_TNV) {
        qsort(ti->tvsi, num_indexes, sizeof ti->tvsi[0],
	      (int (*)(const void *a, const void *b))comp);
    }

    return;
}


/*
 * C R E A T E _ U N I Q U E _ I N D E X E S
 *
 * Create a unique sort-index from a provided non-unique sorted
 * sort-index by removing duplicates in the index. The non-unique
 * sort-indexes are freed when they are no longer needed.
 */
void
create_unique_indexes(struct ti_t *ti)
{
    size_t last = 0;
    size_t idx = 0;
    size_t counter = 0;
    size_t num_indexes = ti->num_tri * 3;
    size_t *tmp_arr = (size_t *)ti->index_arr_tri;

    /* process vertex indexes, count sorted
     * and unique libobj vertex indexes
     */
    last = tmp_arr[ti->vsi[0]];
    ti->num_uvi = 1;
    for (idx = 1; idx < num_indexes ; ++idx) {
        if (tmp_arr[ti->vsi[idx]] != last) {
            last = tmp_arr[ti->vsi[idx]];
            ti->num_uvi++;
        }
    }

    /* allocate unique triangle vertex index array */
    ti->uvi = (size_t *)bu_calloc(ti->num_uvi, sizeof(size_t), "ti->uvi");

    /* store unique triangle vertex index array */
    counter = 0;
    last = tmp_arr[ti->vsi[0]];
    ti->uvi[counter] = last;
    for (idx = 1; idx < num_indexes ; ++idx) {
        if (tmp_arr[ti->vsi[idx]] != last) {
            last = tmp_arr[ti->vsi[idx]];
            counter++;
            ti->uvi[counter] = last;
        }
    }

    /* free 'triangle vertex sort index array' (ti->vsi) since it is
     * no longer needed since 'unique triangle vertex index array' has
     * been created.
     */
    bu_free(ti->vsi, "ti->vsi");
    /* end of process vertex indexes */

    /* process vertex normal indexes */
    if (ti->tri_type == FACE_NV || ti->tri_type == FACE_TNV) {
        /* count sorted and unique libobj vertex normal indexes */
        last = tmp_arr[ti->vnsi[0]];
        ti->num_uvni = 1;
        for (idx = 1; idx < num_indexes ; ++idx) {
            if (tmp_arr[ti->vnsi[idx]] != last) {
                last = tmp_arr[ti->vnsi[idx]];
                ti->num_uvni++;
            }
        }

        /* allocate unique triangle vertex normal index array */
        ti->uvni = (size_t *)bu_calloc(ti->num_uvni, sizeof(size_t), "ti->uvni");

        /* store unique triangle vertex normal index array */
        counter = 0;
        last = tmp_arr[ti->vnsi[0]];
        ti->uvni[counter] = last;
        for (idx = 1; idx < num_indexes ; ++idx) {
            if (tmp_arr[ti->vnsi[idx]] != last) {
                last = tmp_arr[ti->vnsi[idx]];
                counter++;
                ti->uvni[counter] = last;
            }
        }

        /* free 'triangle vertex normal sort index array' (ti->vnsi)
         * since it is no longer needed since 'unique triangle vertex
         * normal index array' has been created.
         */
        bu_free(ti->vnsi, "ti->vnsi");
    } /* end of process vertex normal indexes */

    /* process texture vertex indexes */
    if (ti->tri_type == FACE_TV || ti->tri_type == FACE_TNV) {
        /* count sorted and unique libobj texture vertex indexes */
        last = tmp_arr[ti->tvsi[0]];
        ti->num_utvi = 1;
        for (idx = 1; idx < num_indexes ; ++idx) {
            if (tmp_arr[ti->tvsi[idx]] != last) {
                last = tmp_arr[ti->tvsi[idx]];
                ti->num_utvi++;
            }
        }

        /* allocate unique triangle texture vertex index array */
        ti->utvi = (size_t *)bu_calloc(ti->num_utvi, sizeof(size_t), "ti->utvi");

        /* store unique triangle texture vertex index array */
        counter = 0;
        last = tmp_arr[ti->tvsi[0]];
        ti->utvi[counter] = last;
        for (idx = 1; idx < num_indexes ; ++idx) {
            if (tmp_arr[ti->tvsi[idx]] != last) {
                last = tmp_arr[ti->tvsi[idx]];
                counter++;
                ti->utvi[counter] = last;
            }
        }

        /* free 'triangle texture vertex sort index array' (ti->tvsi)
         * since it is no longer needed since 'unique triangle texture
         * vertex index array' has been created.
         */
        bu_free(ti->tvsi, "ti->tvsi");
    } /* end of process texture vertex indexes */
    return;
}


/*
 * F R E E _ T I
 *
 * Free memory allocated for the contents of the triangle index
 * structure.
 */
void 
free_ti(struct ti_t *ti)
{
    if (ti == (struct ti_t *)NULL) {
        bu_log("function free_ti was passed a null pointer\n");
        return;
    }

    bu_free(ti->bot_faces,"ti->bot_faces");

    /* free 'triangle indices into vertex, normal, texture vertex lists' */
    bu_free(ti->index_arr_tri, "ti->index_arr_tri");

    /* free 'array of floats for bot vertices' */
    bu_free(ti->bot_vertices, "ti->bot_vertices");

    /* free 'array of floats for bot normals' */
    if (ti->tri_type == FACE_NV || ti->tri_type == FACE_TNV) {
	bu_free(ti->bot_normals, "ti->bot_normals");
	bu_free(ti->bot_face_normals, "ti->bot_face_normals");
    }

    /* free 'array of floats for texture vertices' */
    if (ti->tri_type == FACE_TV || ti->tri_type == FACE_TNV) {
        bu_free(ti->bot_texture_vertices, "ti->bot_texture_vertices");
    }

    if ((ti->bot_mode == RT_BOT_PLATE) || (ti->bot_mode == RT_BOT_PLATE_NOCOS)) {
        /* free array of bot_face_mode */
        bu_free((char *)ti->bot_face_mode, "ti->bot_face_mode");

        /* free 'array of floats for face thickness' */
        bu_free(ti->bot_thickness, "ti->bot_thickness");
    }

    return;
}


/*
 * C R E A T E _ B O T _ F L O A T _ A R R A Y S
 *
 * Create the arrays used by the bot primitive which contain
 * floating-point values. The unique sorted-indexes are used to
 * retrieve the values from the libobj structures.
 *
 * Recommended function improvements:
 * - If unable to unitize a normal, deal with this.
 */
void
create_bot_float_arrays(struct ga_t *ga,
                        struct ti_t *ti,
                        fastf_t bot_thickness, /* plate-mode-bot thickness in mm units */
                        fastf_t conv_factor)
{
    size_t i = 0;
    size_t j = 0;
    fastf_t tmp_norm[3] = {0.0, 0.0, 0.0};

    ti->bot_num_vertices = ti->num_uvi;
    ti->bot_num_faces = ti->num_tri;
    ti->bot_num_normals = ti->num_uvni;
    ti->bot_num_texture_vertices = ti->num_utvi;

    /* allocate bot_vertices array */
    ti->bot_vertices = (fastf_t *)bu_calloc(ti->bot_num_vertices * 3, sizeof(fastf_t), "ti->bot_vertices");

    /* populate bot_vertices array */
    for (i = 0 ; i < ti->bot_num_vertices ; i++) {
        for (j = 0 ; j < 3 ; j++) {
            ti->bot_vertices[(i*3)+j] = ga->vert_list[ti->uvi[i]][j] * conv_factor;
        }
    }

    if ((ti->bot_mode == RT_BOT_PLATE) || (ti->bot_mode == RT_BOT_PLATE_NOCOS)) {
        /* allocate and polulate bot_thickness array */
        ti->bot_thickness = (fastf_t *)bu_calloc(ti->bot_num_faces, sizeof(fastf_t), "ti->bot_thickness");
        for (i = 0 ; i < ti->bot_num_faces ; i++) {
            ti->bot_thickness[i] = bot_thickness;
        }

        /* allocate and polulate bot_face_mode array */
        ti->bot_face_mode = bu_bitv_new(ti->bot_num_faces);
        BU_BITSET(ti->bot_face_mode, 1); /* 1 indicates thickness is appended to hit
                                          * point in ray direction, 0 indicates thickness
                                          * is centered about hit point
                                          */
    }

    /* normals */
    if (ti->tri_type == FACE_NV || ti->tri_type == FACE_TNV) {
        /* allocate bot_normals array */
        ti->bot_normals = (fastf_t *)bu_calloc(ti->bot_num_normals * 3, sizeof(fastf_t), "ti->bot_normals");

        /* populate bot_normals array */
        for (i = 0 ; i < ti->bot_num_normals ; i++) {
            for (j = 0 ; j < 3 ; j++) {
                tmp_norm[j] = ga->norm_list[ti->uvni[i]][j];
            }
 
            if (MAGNITUDE(tmp_norm) < VDIVIDE_TOL) {
                bu_log("ERROR: unable to unitize normal (%f)(%f)(%f)\n", tmp_norm[0], tmp_norm[1], tmp_norm[2]);
                VMOVE(&(ti->bot_normals[i*3]), tmp_norm);
            } else {
                VUNITIZE(tmp_norm);
                VMOVE(&(ti->bot_normals[i*3]), tmp_norm);
            }
        }
    }

    /* textures */
    if (ti->tri_type == FACE_TV || ti->tri_type == FACE_TNV) {
        /* allocate bot_texture_vertices array */
        ti->bot_texture_vertices = (fastf_t *)bu_calloc(ti->bot_num_texture_vertices * 3,
							sizeof(fastf_t), "ti->bot_texture_vertices");

        /* populate bot_texture_vertices array */
        for (i = 0 ; i < ti->bot_num_texture_vertices ; i++) {
            for (j = 0 ; j < 3 ; j++) {
                ti->bot_texture_vertices[(i*3)+j] = ga->texture_coord_list[ti->utvi[i]][j] * conv_factor;
            }
        }
    }
    return;
}


/*
 * C R E A T E _ B O T _ I N T _ A R R A Y S
 *
 * Create the arrays used by the bot primitive which contain integer
 * values. These integer values are the index values of the triangle
 * vertices and normals within the bot primitive. The unique
 * sorted-indexes are searched to convert the libobj index values of
 * each vertex to the appropriate index value in the bot primitive.
 * The memory for the bot integer arrays is allocated within this
 * function and is expected to be freed once the bot primitive is
 * created. The unique sorted-indexes are freed by this function once
 * they are no longer needed. This function returns a non-zero value
 * (i.e. fails) if bsearch is unable to find a value. This function
 * should never fail unless there is a logic bug in this code.
 */
int 
create_bot_int_arrays(struct ti_t *ti)
{
    size_t i = 0;
    size_t j = 0;
    size_t *res_v = 0;
    size_t *res_n = 0;
    size_t *res_t = 0;

    tri_arr_1D_t index_arr_tri_1D = NULL;
    tri_arr_2D_t index_arr_tri_2D = NULL;
    tri_arr_3D_t index_arr_tri_3D = NULL;

    /* allocate memory for 'array of indices into bot_vertices array' */
    ti->bot_faces = (int *)bu_calloc(ti->bot_num_faces * 3, sizeof(int), "ti->bot_faces");

    /* allocate memory for 'array of indices into bot_normals array' */
    if (ti->tri_type == FACE_NV || ti->tri_type == FACE_TNV) {
        ti->bot_face_normals = (int *)bu_calloc(ti->bot_num_faces * 3, sizeof(int), "ti->bot_face_normals");
    }

    /* allocate memory for 'array indices into bot_texture_vertices array' */
    if (ti->tri_type == FACE_TV || ti->tri_type == FACE_TNV) {
        ti->bot_textures = (int *)bu_calloc(ti->bot_num_faces * 3, sizeof(int), "ti->bot_textures");
    }

    if (ti->tri_type == FACE_V) {
        index_arr_tri_1D = (tri_arr_1D_t)ti->index_arr_tri;
        for (i = 0 ; i < ti->bot_num_faces ; i++) {
            for (j = 0 ; j < 3 ; j++) {
                if ((res_v = (size_t *)bsearch(&(index_arr_tri_1D[i][j]),
		    ti->uvi, ti->num_uvi, sizeof(size_t),
		    (int (*)(const void *a, const void *b))comp_b)) ==
			(size_t)NULL)
		{
		    bu_log("ERROR: FACE_V bsearch returned null, "
			    "face=(%zu)idx=(%zu)\n", i, j);
		    return 1;
                } else {
                    ti->bot_faces[(i*3)+j] = (int)(res_v - ti->uvi);
                }
            }
        }
        bu_free(ti->uvi, "ti->uvi");
    }

    if (ti->tri_type == FACE_TV) {
        index_arr_tri_2D = (tri_arr_2D_t)ti->index_arr_tri;
        for (i = 0 ; i < ti->bot_num_faces ; i++) {
            for (j = 0 ; j < 3 ; j++) {
                if ((res_v = (size_t *)bsearch(&(index_arr_tri_2D[i][j][0]),
			ti->uvi, ti->num_uvi, sizeof(size_t),
			(int (*)(const void *a, const void *b))comp_b)) ==
			(size_t)NULL)
		{
		    bu_log("ERROR: FACE_TV bsearch returned vertex null, "
			    "face=(%zu)idx=(%zu)\n", i, j);
		    return 1;
                } else {
                    ti->bot_faces[(i*3)+j] = (int)(res_v - ti->uvi);
                }
                if ((res_t = (size_t*)bsearch(&(index_arr_tri_2D[i][j][1]), ti->utvi, ti->num_utvi, sizeof(size_t),
				     (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
		    bu_log("ERROR: FACE_TV bsearch returned texture null, face=(%zu)idx=(%zu)\n", i, j);
		    return 1;
                } else {
                    ti->bot_textures[(i*3)+j] = (int)(res_t - ti->utvi);
                }
            }
        }
        bu_free(ti->uvi, "ti->uvi");
        bu_free(ti->utvi, "ti->utvi");
    }

    if (ti->tri_type == FACE_NV) {
        index_arr_tri_2D = (tri_arr_2D_t)ti->index_arr_tri;
        for (i = 0 ; i < ti->bot_num_faces ; i++) {
            for (j = 0 ; j < 3 ; j++) {
                if ((res_v = (size_t*)bsearch(&(index_arr_tri_2D[i][j][0]), ti->uvi, ti->num_uvi, sizeof(size_t),
				     (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
		    bu_log("ERROR: FACE_NV bsearch returned vertex null, face=(%zu)idx=(%zu)\n", i, j);
		    return 1;
                } else {
                    ti->bot_faces[(i*3)+j] = (int)(res_v - ti->uvi);
                }
                if ((res_n = (size_t*)bsearch(&(index_arr_tri_2D[i][j][1]), ti->uvni, ti->num_uvni, sizeof(size_t),
				     (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
		    bu_log("ERROR: FACE_NV bsearch returned normal null, face=(%zu)idx=(%zu)\n", i, j);
		    return 1;
                } else {
                    ti->bot_face_normals[(i*3)+j] = (int)(res_n - ti->uvni);
                }
            }
        }
        bu_free(ti->uvi, "ti->uvi");
        bu_free(ti->uvni, "ti->uvni");
    }

    if (ti->tri_type == FACE_TNV) {
        index_arr_tri_3D = (tri_arr_3D_t)ti->index_arr_tri;
        for (i = 0 ; i < ti->bot_num_faces ; i++) {
            for (j = 0 ; j < 3 ; j++) {
                if ((res_v = (size_t*)bsearch(&(index_arr_tri_3D[i][j][0]), ti->uvi, ti->num_uvi, sizeof(size_t),
				     (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
		    bu_log("ERROR: FACE_TNV bsearch returned vertex null, face=(%zu)idx=(%zu)\n", i, j);
		    return 1;
                } else {
                    ti->bot_faces[(i*3)+j] = (int)(res_v - ti->uvi);
                }
                if ((res_t = (size_t*)bsearch(&(index_arr_tri_3D[i][j][1]), ti->utvi, ti->num_utvi, sizeof(size_t),
				     (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
		    bu_log("ERROR: FACE_TNV bsearch returned texture null, face=(%zu)idx=(%zu)\n", i, j);
		    return 1;
                } else {
                    ti->bot_textures[(i*3)+j] = (int)(res_t - ti->utvi);
                }
                if ((res_n = (size_t*)bsearch(&(index_arr_tri_3D[i][j][2]), ti->uvni, ti->num_uvni, sizeof(size_t),
				     (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
		    bu_log("ERROR: FACE_TNV bsearch returned normal null, face=(%zu)idx=(%zu)\n", i, j);
		    return 1;
                } else {
                    ti->bot_face_normals[(i*3)+j] = (int)(res_n - ti->uvni);
                }
            }
        }
        bu_free(ti->uvi, "ti->uvi");
        bu_free(ti->utvi, "ti->utvi");
        bu_free(ti->uvni, "ti->uvni");
    }
    return 0;
}


/*
 * R E M O V E _ D U P L I C A T E S _ A N D _ S O R T
 *
 * Given a one-dimensional array of numbers of type size_t, the array
 * is sorted and duplicate entries removed, resulting in a sorted list
 * of unique values. For speed, a new list is allocated where the
 * unique sorted list is stored and the original list is freed.
 */
void
remove_duplicates_and_sort(size_t **list, size_t *count)
{
    size_t last = 0;
    size_t idx = 0;
    size_t idx2 = 0;
    size_t unique_count = 0;
    size_t *unique_arr = (size_t *)NULL;

    qsort(*list, *count, sizeof(size_t), (int (*)(const void *a, const void *b))comp_b);

    /* process list, count sorted and unique list elements */
    last = (*list)[0];
    unique_count = 1;
    for (idx = 1; idx < *count ; ++idx) {
        if ((*list)[idx] != last) {
            last = (*list)[idx];
            unique_count++;
        }
    }

    /* allocate unique array */
    unique_arr = (size_t *)bu_calloc(unique_count, sizeof(size_t), "unique_arr");

    /* store unique array */
    idx2 = 0;
    last = (*list)[0];
    unique_arr[idx2] = last;
    for (idx = 1; idx < *count ; ++idx) {
        if ((*list)[idx] != last) {
            last = (*list)[idx];
            idx2++;
            unique_arr[idx2] = last;
        }
    }
    bu_free(*list, "*list");

    *list = unique_arr;
    *count = unique_count;

    return;
}


/*
 * P O P U L A T E _ F U S E _ M A P
 *
 * Populate the fuse map array which maps vertex indexes to their
 * fused equivalent. This function is a support function for the
 * fuse_vertex function. This function performs the distance compare
 * operations between the vertices and populates the fuse map array.
 * The number of fused vertices is returned by this function.
 */
size_t
populate_fuse_map(struct ga_t *ga,
                  struct gfi_t *gfi,
                  fastf_t conv_factor, 
                  struct bn_tol *tol,
                  size_t *unique_index_list,
                  size_t num_unique_index_list,
                  int compare_type,  /* FUSE_WI_TOL or FUSE_EQUAL */
                  int vertex_type)   /* FUSE_VERT or FUSE_TEX_VERT */
{
    size_t idx1 = 0;
    size_t idx2 = 0;
    size_t fuse_count = 0;
    fastf_t tmp_v1[3];
    fastf_t tmp_v2[3];
    fastf_t distance_between_vertices = 0.0;
    size_t *fuse_map = (size_t *)NULL;
    short int *fuse_flag = (short int *)NULL;
    size_t fuse_offset = 0; 

    if (vertex_type == FUSE_TEX_VERT) {
        fuse_map = gfi->texture_vertex_fuse_map;
        fuse_flag = gfi->texture_vertex_fuse_flag;
        fuse_offset = gfi->texture_vertex_fuse_offset;
    } else {
        fuse_map = gfi->vertex_fuse_map;
        fuse_flag = gfi->vertex_fuse_flag;
        fuse_offset = gfi->vertex_fuse_offset;
    }

    /* only set the flag for duplicates */
    idx1 = 0;
    while (idx1 < num_unique_index_list) {
        if (fuse_flag[unique_index_list[idx1] - fuse_offset] == 0) {
            /* true if current vertex has not already been identified as a duplicate */
            idx2 = idx1 + 1;
            VMOVE(tmp_v1, ga->vert_list[unique_index_list[idx1]]);
            VSCALE(tmp_v1, tmp_v1, conv_factor);
            fuse_map[unique_index_list[idx1] - fuse_offset] = unique_index_list[idx1];
            while (idx2 < num_unique_index_list) {
                if (fuse_flag[unique_index_list[idx2] - fuse_offset] == 0) {
                    VMOVE(tmp_v2, ga->vert_list[unique_index_list[idx2]]);
                    VSCALE(tmp_v2, tmp_v2, conv_factor);
                    if ((compare_type == FUSE_EQUAL) ? VEQUAL(tmp_v1, tmp_v2) :
			bn_pt3_pt3_equal(tmp_v1, tmp_v2, tol)) {
			if (debug) {
			    distance_between_vertices = DIST_PT_PT(tmp_v1, tmp_v2);
			    bu_log("found equal i1=(%zu)vi1=(%zu)v1=(%f)(%f)(%f), i2=(%zu)vi2=(%zu)v2=(%f)(%f)(%f), dist = (%fmm)\n", 
				   idx1, unique_index_list[idx1], tmp_v1[0], tmp_v1[1], tmp_v1[2],
				   idx2, unique_index_list[idx2], tmp_v2[0], tmp_v2[1], tmp_v2[2],
				   unique_index_list[idx2], distance_between_vertices);
			}
			fuse_map[unique_index_list[idx2] - fuse_offset] = unique_index_list[idx1];
			fuse_flag[unique_index_list[idx2] - fuse_offset] = 1;
			fuse_count++;
                    }
                }
                idx2++;
            }
        }
        idx1++;
    }

    if (debug) {
        for (idx1 = 0 ; idx1 < num_unique_index_list ; idx1++) {
            bu_log("fused unique_index_list = (%zu)->(%zu)\n", unique_index_list[idx1],
                   fuse_map[unique_index_list[idx1] - fuse_offset]);
        }
    }

    return fuse_count;
}


/*
 * F U S E _ V E R T E X
 *
 * Perform a vertex fuse of the given face grouping. Vertices which
 * are close enough together to be considered the same vertex are
 * joined. A mapping is created to convert each vertex index to their
 * fused equivalent. The number of fused vertices is returned by this
 * function. Both vertices and texture vertices can be fused by this
 * function and the determination of equal vertices can be performed
 * using the distance tolerance or VEQUAL. Fusing the vertices is
 * useful when the obj file was formed improperly where identical or
 * virtually identical vertices have different indexes and it is
 * desired to identify faces sharing an edge by comparing the index
 * values of the edge vertices. For this same reason, vertex fusing is
 * necessary for testing surface closure where closure is defined by
 * no open edges. A closed edge is where the edge is shared by exactly
 * two faces, a open edge is where an edge is used by only one face.
 */
size_t
fuse_vertex(struct ga_t *ga,
            struct gfi_t *gfi,
            fastf_t conv_factor, 
            struct bn_tol *tol,
            int vertex_type,   /* FUSE_VERT or FUSE_TEX_VERT */
            int compare_type)  /* FUSE_WI_TOL or FUSE_EQUAL */
{
    size_t face_idx = 0; /* index into faces within for-loop */
    size_t vert_idx = 0; /* index into vertices within for-loop */
    fastf_t tmp_v[3] = {0.0, 0.0, 0.0}; /* temporary vertex */
    fastf_t tmp_n[3] = {0.0, 0.0, 0.0}; /* temporary normal */
    fastf_t tmp_t[3] = {0.0, 0.0, 0.0}; /* temporary texture vertex */
    fastf_t tmp_w = 0.0; /* temporary weight */
    size_t vofi = 0;     /* vertex obj file index */
    size_t nofi = 0;     /* normal obj file index */
    size_t tofi = 0;     /* texture vertex obj file index */
    size_t min_vert_idx = 0;
    size_t max_vert_idx = 0;
    size_t min_tex_vert_idx = 0;
    size_t max_tex_vert_idx = 0;

    size_t *vertex_index_list = (size_t *)NULL;
    size_t *texture_vertex_index_list = (size_t *)NULL;
    size_t *index_list_tmp = (size_t *)NULL;
    size_t num_index_list = 0;
    size_t max_index_list = 0;
    size_t max_index_list_increment = 128;

    size_t num_unique_vertex_index_list = 0;
    size_t num_unique_texture_vertex_index_list = 0;

    size_t idx1 = 0;

    size_t fuse_count = 0;

    if ((gfi->face_type == FACE_V || gfi->face_type == FACE_NV) &&
	    (vertex_type == FUSE_TEX_VERT))
    {
        return 0; /* nothing to process */
    }

    /* test if fuse vertices has already been run, if so free old arrays */
    if ((vertex_type == FUSE_VERT) && (gfi->vertex_fuse_map != NULL)) {
        bu_free(gfi->vertex_fuse_map, "gfi->vertex_fuse_map");
        gfi->num_vertex_fuse = 0;
        gfi->vertex_fuse_offset = 0;
    }

    /* test if fuse texture vertices has already been run, if so free
     * old arrays.
     */
    if (((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) &&
	(vertex_type == FUSE_TEX_VERT)) &&
	(gfi->texture_vertex_fuse_map != NULL))
    {
	bu_free(gfi->texture_vertex_fuse_map, "gfi->texture_vertex_fuse_map");
	gfi->num_texture_vertex_fuse = 0;
	gfi->texture_vertex_fuse_offset = 0;
    }

    max_index_list += max_index_list_increment;
    /* initial allocation for vertex_index_list, and as applicable,
     * the normal_index_list, texture_vertex_index_list
     */

    if (vertex_type == FUSE_VERT) {
        vertex_index_list = (size_t *)bu_calloc(max_index_list, sizeof(size_t),
	    "vertex_index_list");
    }

    if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) &&
	(vertex_type == FUSE_TEX_VERT))
    {
        texture_vertex_index_list = (size_t *)bu_calloc(max_index_list,
	    sizeof(size_t), "texture_vertex_index_list");
    }

    /* loop thru all polygons and collect max and min index values and
     * fill the vertex_index_list and as applicable, the
     * texture_vertex_index_list
     */
    for (face_idx = 0; face_idx < gfi->num_faces; face_idx++) {
        for (vert_idx = 0; vert_idx < gfi->num_vertices_arr[face_idx];
	     vert_idx++)
	{
            retrieve_coord_index(ga, gfi, face_idx, vert_idx, tmp_v, tmp_n,
		tmp_t, &tmp_w, &vofi, &nofi, &tofi);

            if (face_idx == 0 && vert_idx == 0) {
		/* set initial max and min */
                if (vertex_type == FUSE_VERT) {
                    vertex_index_list[num_index_list] = vofi;
                    min_vert_idx = vofi;
                    max_vert_idx = vofi;
                }
                if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) &&
		    (vertex_type == FUSE_TEX_VERT))
		{
                    texture_vertex_index_list[num_index_list] = tofi;
                    min_tex_vert_idx = tofi;
                    max_tex_vert_idx = tofi;
                }
            } else {
		/* set max and min */
                if (vertex_type == FUSE_VERT) {
                    vertex_index_list[num_index_list] = vofi;
                    if (vofi > max_vert_idx) {
                        max_vert_idx = vofi;
                    }
                    if (vofi < min_vert_idx) {
                        min_vert_idx = vofi;
                    }
                }
                if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) &&
		    (vertex_type == FUSE_TEX_VERT))
		{
                    texture_vertex_index_list[num_index_list] = tofi;
                    if (tofi > max_tex_vert_idx) {
                        max_tex_vert_idx = tofi;
                    }
                    if (tofi < min_tex_vert_idx) {
                        min_tex_vert_idx = tofi;
                    }
                }
            }
            num_index_list++;
            if (num_index_list >= max_index_list) {
                max_index_list += max_index_list_increment;
                if (vertex_type == FUSE_VERT) {
                    index_list_tmp = (size_t *)bu_realloc(vertex_index_list,
			    sizeof(size_t) * max_index_list, "index_list_tmp");

                    vertex_index_list = index_list_tmp;
                }
                if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) &&
		    (vertex_type == FUSE_TEX_VERT))
		{
                    index_list_tmp =
			(size_t *) bu_realloc(texture_vertex_index_list,
				sizeof(size_t) * max_index_list,
				"index_list_tmp");

                    texture_vertex_index_list = index_list_tmp;
                }
            }
        } /* end of vert_idx for-loop */
    } /* end of face_idx for-loop */

    /* compute size of fuse_map and fuse_flag arrays and allocate
     * these arrays. set fuse_offset values
     */
    if (vertex_type == FUSE_VERT) {
        gfi->num_vertex_fuse = max_vert_idx - min_vert_idx + 1;
        gfi->vertex_fuse_offset = min_vert_idx;
        gfi->vertex_fuse_map = (size_t *)bu_calloc(gfi->num_vertex_fuse,
		sizeof(size_t), "gfi->vertex_fuse_map");

        gfi->vertex_fuse_flag = (short int *)bu_calloc(gfi->num_vertex_fuse,
		sizeof(short int), "gfi->vertex_fuse_flag");

        /* initialize arrays */
        for (idx1 = 0 ; idx1 < gfi->num_vertex_fuse ; idx1++) {
            gfi->vertex_fuse_map[idx1] = 0;
            gfi->vertex_fuse_flag[idx1] = 0;
        }
    }

    if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) &&
	    (vertex_type == FUSE_TEX_VERT))
    {
        gfi->num_texture_vertex_fuse = max_tex_vert_idx - min_tex_vert_idx + 1;
        gfi->texture_vertex_fuse_offset = min_tex_vert_idx;
        gfi->texture_vertex_fuse_map =
	    (size_t *)bu_calloc(gfi->num_texture_vertex_fuse, sizeof(size_t),
		    "gfi->texture_vertex_fuse_map");

        gfi->texture_vertex_fuse_flag =
	    (short int *)bu_calloc(gfi->num_texture_vertex_fuse,
		    sizeof(short int), "gfi->texture_vertex_fuse_flag");

        /* initialize arrays */
        for (idx1 = 0 ; idx1 < gfi->num_texture_vertex_fuse ; idx1++) {
            gfi->texture_vertex_fuse_map[idx1] = 0;
            gfi->texture_vertex_fuse_flag[idx1] = 0;
        }
    }

    /* initialize unique counts, these will be reduced as duplicates
     * are removed from the lists
     */
    num_unique_vertex_index_list = num_index_list;
    num_unique_texture_vertex_index_list = num_index_list;

    if (debug) {
        for (idx1 = 0 ; idx1 < num_index_list ; idx1++) {
            bu_log("non-unique sorted vertex_index_list[idx1] = (%zu)\n",
		    vertex_index_list[idx1]);
        }
        bu_log("num non-unique sorted vertex_index_list[idx1] = (%zu)\n",
		num_index_list);
    }

    /* remove duplicates from lists and return a sorted list, pass in
     * the number of elements in the non-unique array and receive the
     * number of element in the unique array, a new list is created
     * and passed back by this function. the new list will be returned
     * sorted
     */
    if (vertex_type == FUSE_VERT) {
        remove_duplicates_and_sort(&vertex_index_list,
		&num_unique_vertex_index_list);
    }

    if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) &&
	    (vertex_type == FUSE_TEX_VERT))
    {
        remove_duplicates_and_sort(&texture_vertex_index_list,
		&num_unique_texture_vertex_index_list);
    }

    if (debug) {
        for (idx1 = 0 ; idx1 < num_unique_vertex_index_list ; idx1++) {
            bu_log("unique sorted vertex_index_list[idx1] = (%zu)\n",
		    vertex_index_list[idx1]);
        }
        bu_log("num unique sorted vertex_index_list[idx1] = (%zu)\n",
		num_unique_vertex_index_list);
    }

    if (vertex_type == FUSE_VERT) {
        fuse_count = populate_fuse_map(ga, gfi, conv_factor, tol,
		vertex_index_list, num_unique_vertex_index_list, compare_type,
		vertex_type);

        bu_free(vertex_index_list, "vertex_index_list");
        bu_free(gfi->vertex_fuse_flag, "gfi->vertex_fuse_flag");
    }

    if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) &&
	    (vertex_type == FUSE_TEX_VERT))
    {
        fuse_count = populate_fuse_map(ga, gfi, conv_factor, tol,
		texture_vertex_index_list, num_unique_texture_vertex_index_list,
	       	compare_type, vertex_type);

        bu_free(texture_vertex_index_list, "texture_vertex_index_list");
        bu_free(gfi->texture_vertex_fuse_flag, "gfi->texture_vertex_fuse_flag");
    }

    if (verbose || debug) {
        bu_log("Fused (%zu) vertex for obj file face grouping name (%s),"
		"obj file face grouping index (%zu)\n",
	       fuse_count, bu_vls_addr(gfi->raw_grouping_name),
	       gfi->grouping_index + 1);
    }

    return fuse_count;
}


/*
 * T E S T _ C L O S U R E
 *
 * For a grouping of faces, test if the surface is closed. This
 * function returns the number of open edges. Zero open edges
 * indicates a closed surface. This function traverses all the faces
 * in the face grouping and generates a list of edges using the libobj
 * index values of the edge vertices to identify the edges.  For a
 * closed surface, there must be two of each edge in the list.  A
 * BRL-CAD overlay file can be generated containing the open edges
 * when the plot_mode parameter is set to PLOT_ON. The overlay file
 * will be of the same name as the BRL-CAD primitive except the file
 * extension will be ".pl" instead of ".s". The overlay files will be
 * placed in the same directory as where the "obj-g" command was
 * executed. It is not required but highly recommended that the
 * fuse_vertex function be run on the face grouping before
 * test_closure otherwise some shared edges may be missed. If this
 * function returns zero open edges, this surface should be closed.
 * If this function indicates the surface is open, further testing is
 * necessary, not done by this function, to verify the surface is
 * open. The overlay of open edges may be useful for analysis of
 * surfaces reported as open.
 *
 * Recommended function improvements:
 * - Improve algorithm for identifying shared edges.
 *
 * For example the right edge of triangle 1 would be identified by the
 * current algorithm as an open edge since there is no single matching
 * edge to pair with it. An improved algorithm should identify that
 * the left edges of triangles 2 and 3 should be paired with the right
 * edge of triangle 1 to determine this is a closed edge. Note: In the
 * diagram below, the top vertex of triangle 1 is the same vertex as
 * the top left vertex of triangle 2, the same goes for the bottom
 * right vertex of trinagle 1 and the bottom vertex of triangle 3. The
 * bottom vertex of triangle 2 (i.e. top left vertex of triangle 3)
 * does not exist in triangle 1.
 *
 *            * *___*
 *           / \ \2/
 *          /   \ *___*
 *         /  1  \ \3/
 *        *_______* *
 *
 */
size_t
test_closure(struct ga_t *ga,
             struct gfi_t *gfi,
             fastf_t conv_factor, 
             int plot_mode,        /* PLOT_OFF, PLOT_ON */
             int face_test_type,
             struct bn_tol *tol)
{
    size_t face_idx = 0; /* index into faces within for-loop */
    size_t vert_idx = 0; /* index into vertices within for-loop */
    fastf_t tmp_v[3] = {0.0, 0.0, 0.0}; /* temporary vertex */
    fastf_t tmp_n[3] = {0.0, 0.0, 0.0}; /* temporary normal */
    fastf_t tmp_t[3] = {0.0, 0.0, 0.0}; /* temporary texture vertex */
    fastf_t tmp_w = 0.0;  /* temporary weight */
    size_t vofi0 = 0;    /* vertex obj file index */
    size_t vofi1 = 0;    /* vertex obj file index */
    size_t nofi = 0;     /* normal obj file index */
    size_t tofi = 0;     /* texture vertex obj file index */
 
    size_t first_idx = 0;
    edge_arr_2D_t edges = (edge_arr_2D_t)NULL;
    edge_arr_2D_t edges_tmp = (edge_arr_2D_t)NULL;
    size_t previous_edge[2] = {0, 0};
    size_t max_edges = 0;
    size_t max_edges_increment = 128;
    size_t edge_count = 0;
    size_t idx = 0;
    size_t match = 0;
    size_t open_edges = 0;

    FILE *plotfp = NULL;
    vect_t pnt1;
    vect_t pnt2;

    struct bu_vls plot_file_name;

    max_edges += max_edges_increment;
    edges = (edge_arr_2D_t)bu_calloc(max_edges * 2, sizeof(size_t), "edges");

    /* loop thru all polygons */
    for (face_idx = 0 ; face_idx < gfi->num_faces ; face_idx++) {
        if (!test_face(ga, gfi, face_idx, conv_factor, tol, face_test_type, 0)) {
            for (vert_idx = 0 ; vert_idx < gfi->num_vertices_arr[face_idx] ; vert_idx++) {
                retrieve_coord_index(ga, gfi, face_idx, vert_idx,
                                     tmp_v, tmp_n, tmp_t, &tmp_w, &vofi0, &nofi, &tofi);
                if (vert_idx == 0) {
                    first_idx = vofi0;
                }
                if ((vert_idx + 1) < gfi->num_vertices_arr[face_idx]) {
                    retrieve_coord_index(ga, gfi, face_idx, vert_idx+1,
                                         tmp_v, tmp_n, tmp_t, &tmp_w, &vofi1, &nofi, &tofi);
                } else {
                    vofi1 = first_idx;
                }

                if (vofi0 <= vofi1) {
                    edges[edge_count][0] = vofi0;
                    edges[edge_count][1] = vofi1;
                } else {
                    edges[edge_count][0] = vofi1;
                    edges[edge_count][1] = vofi0;
                }

                if (debug) {
                    bu_log("edges (%zu)(%zu)(%zu)(%zu)\n", face_idx, edge_count,
			   edges[edge_count][0], edges[edge_count][1]);
                }
                edge_count++;

                if (edge_count >= max_edges) {
                    max_edges += max_edges_increment;
                    edges_tmp = (edge_arr_2D_t)bu_realloc(edges, sizeof(size_t) * max_edges * 2, "edges_tmp");
                    edges = edges_tmp;
                }
            } 
        }
    } /* ends when edges list is complete */

    qsort(edges, edge_count, sizeof(size_t) * 2, (int (*)(const void *a, const void *b))comp_c);

    if (debug) {
        for (idx = 0 ; idx < edge_count ; idx++) {
            bu_log("sorted edges (%zu)(%zu)(%zu)\n", idx, edges[idx][0], edges[idx][1]);
        }
    }

    previous_edge[0] = edges[0][0];
    previous_edge[1] = edges[0][1];
    for (idx = 1 ; idx < edge_count ; idx++) {
        if ((previous_edge[0] == edges[idx][0]) && (previous_edge[1] == edges[idx][1])) {
            match++;
        } else {
            if (match == 0) {
                if ((verbose > 1) || debug) {
                    bu_log("open edge (%zu)= %f %f %f (%zu)= %f %f %f \n",
                           previous_edge[0],
                           ga->vert_list[previous_edge[0]][0] * conv_factor,
                           ga->vert_list[previous_edge[0]][1] * conv_factor,
                           ga->vert_list[previous_edge[0]][2] * conv_factor,
                           previous_edge[1],
                           ga->vert_list[previous_edge[1]][0] * conv_factor,
                           ga->vert_list[previous_edge[1]][1] * conv_factor,
                           ga->vert_list[previous_edge[1]][2] * conv_factor);
                }
                if ((plot_mode == PLOT_ON) && (open_edges == 0)) {
                    bu_vls_init(&plot_file_name);
                    bu_vls_sprintf(&plot_file_name, "%s.%zu.%d.o.pl", 
                                   bu_vls_addr(gfi->raw_grouping_name), (long unsigned)gfi->grouping_index + 1,
                                   gfi->face_type);
                    cleanup_name(&plot_file_name);
                    if ((plotfp = fopen(bu_vls_addr(&plot_file_name), "wb")) == (FILE *)NULL) {
                        bu_log("ERROR: unable to create plot file (%s)\n", bu_vls_addr(&plot_file_name));
                        bu_vls_free(&plot_file_name);
                        plot_mode = PLOT_OFF;
                    }
                }
                if (plot_mode == PLOT_ON) {
                    VMOVE(pnt1, ga->vert_list[previous_edge[0]]);
                    VMOVE(pnt2, ga->vert_list[previous_edge[1]]);
                    VSCALE(pnt1, pnt1, conv_factor);
                    VSCALE(pnt2, pnt2, conv_factor);
                    pdv_3line(plotfp, pnt1, pnt2);
                }
                open_edges++;
            } else {
                match = 0;
            }
        }
        previous_edge[0] = edges[idx][0];
        previous_edge[1] = edges[idx][1];
    }

    bu_free(edges, "edges");

    if (open_edges) {
        gfi->closure_status = SURF_OPEN;
        if (verbose || debug) {
            bu_log("Surface closure failed for obj file face grouping name (%s), obj file face grouping index (%zu), (%zu) open edges\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1, open_edges);
        }
    } else {
        gfi->closure_status = SURF_CLOSED;
        if (verbose || debug) {
            bu_log("Surface closure success for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }
    }

    if ((plot_mode == PLOT_ON) && (open_edges > 0)) {
        bu_vls_free(&plot_file_name);
        (void)fclose(plotfp);
    }

    return open_edges;
}


/*
 * O U T P U T _ T O _ B O T
 *
 * For a grouping of faces, write a bot primitive to a BRL-CAD
 * database file (i.e. ".g" file). Texture_mode should be set to
 * IGNR_TEX since the ability to use the obj file texture vertices is
 * not implemented. Texture_mode can be set to PROC_TEX but extra
 * processing will be performed and will provide no benefit.  If
 * normal_mode is set to PROC_NORM, any normals provided in the obj
 * file for the current grouping of faces will be included in the bot
 * primitive. If the provided normals are smoothing normals, the
 * resulting raytrace of the bot will be smooth. The type of bot
 * created is choosen by the bot_output_mode. If the type of bot
 * selected is RT_BOT_PLATE or RT_BOT_PLATE_NOCOS the thickness of the
 * bot plates is defined by the parameter bot_thickness. This function
 * will return a non-zero value if an error occured. Bot primitives
 * will be named according to the grouping name in the obj file with
 * the following exceptions. Characters invalid for BRL-CAD primitive
 * names and operating system file names are converted to
 * underscore. The remaining portions of the name indicate the decimal
 * index number of the grouping in the obj file, face_type number of
 * the grouping {1=v|2=tv|3=nv|4=tnv}, primitive type {b=bot} and
 * closure status {u=untested|c=closed|o=open}.
 */
int
output_to_bot(struct ga_t *ga,
              struct gfi_t *gfi,
              struct rt_wdb *outfp, 
              fastf_t conv_factor, 
              struct bn_tol *tol,
              fastf_t bot_thickness,         /* plate-mode-bot thickness in mm units */
              int texture_mode,              /* PROC_TEX, IGNR_TEX */    
              int normal_mode,               /* PROC_NORM, IGNR_NORM */
              unsigned char bot_output_mode, /* RT_BOT_PLATE, RT_BOT_PLATE_NOCOS, RT_BOT_SOLID, RT_BOT_SURFACE */
              int face_test_type)
{
    struct ti_t ti;              /* triangle indices structure */
    size_t num_faces_killed = 0; /* number of degenerate faces killed in the current bot */
    size_t face_idx = 0;         /* index into faces within for-loop */
    int ret = 0;                 /* 0=success !0=fail */

    /* initialize triangle indices structure */
    memset((void *)&ti, 0, sizeof(struct ti_t)); /* probably redundant */
    ti.index_arr_tri = (void *)NULL; /* triangle indices into vertex, normal, texture vertex lists */
    ti.num_tri = 0;                  /* number of triangles represented by index_arr_triangles */
    ti.max_tri = 0;                  /* maximum number of triangles based on current memory allocation */
    ti.tri_type = 0;                 /* i.e. FACE_V, FACE_TV, FACE_NV or FACE_TNV */
    ti.vsi = (size_t *)NULL;         /* triangle vertex sort index array */
    ti.vnsi = (size_t *)NULL;        /* triangle vertex normal sort index array */
    ti.tvsi = (size_t *)NULL;        /* triangle texture vertex sort index array */
    ti.uvi = (size_t *)NULL;         /* unique triangle vertex index array */
    ti.uvni = (size_t *)NULL;        /* unique triangle vertex normal index array */
    ti.utvi = (size_t *)NULL;        /* unique triangle texture vertex index array */
    ti.num_uvi = 0;                  /* number of unique triangle vertex indexes in uvi array */
    ti.num_uvni = 0;                 /* number of unique triangle vertex normal index in uvni array */
    ti.num_utvi = 0;                 /* number of unique triangle texture vertex index in utvi array */
    ti.bot_mode = 0;                 /* bot mode RT_BOT_PLATE, RT_BOT_PLATE_NOCOS, RT_BOT_SOLID,
                                                * RT_BOT_SURFACE
                                                */
    ti.bot_vertices = (fastf_t *)NULL;         /* array of float for bot vertices [bot_num_vertices*3] */
    ti.bot_thickness = (fastf_t *)NULL;        /* array of floats for face thickness [bot_num_faces] */
    ti.bot_normals = (fastf_t *)NULL;          /* array of floats for bot normals [bot_num_normals*3] */
    ti.bot_texture_vertices = (fastf_t *)NULL; /* array of floats for texture vertices [bot_num_texture_vertices*3] */
    ti.bot_faces = (int *)NULL;                /* array of indices into bot_vertices array [bot_num_faces*3] */
    ti.bot_face_mode = (struct bu_bitv *)NULL; /* bu_list of face modes for plate-mode-bot [bot_num_faces] */
    ti.bot_face_normals = (int *)NULL;         /* array of indices into bot_normals array [bot_num_faces*3] */
    ti.bot_textures  = (int *)NULL;            /* array of indices into bot_texture_vertices array */
    ti.bot_num_vertices = 0;                   /* number of vertices in bot_vertices array */
    ti.bot_num_faces = 0;                      /* number of faces in bot_faces array */
    ti.bot_num_normals = 0;                    /* number of normals in bot_normals array */
    ti.bot_num_texture_vertices = 0;           /* number of textures in bot_texture_vertices array */

    ti.bot_mode = bot_output_mode;

    /* loop thru all the polygons (i.e. faces) to be placed in the current bot */
    for (face_idx = 0 ; face_idx < gfi->num_faces ; face_idx++) {
        if (debug) {
            bu_log("num vertices in current polygon = (%zu)\n", gfi->num_vertices_arr[face_idx]);
        }

        /* test for degenerate face */
        if (test_face(ga, gfi, face_idx, conv_factor, tol, face_test_type, 0)) {
            num_faces_killed++;
        } else {
            /* populates triangle indexes for the current face.  if
             * necessary, allocates additional memory for the triangle
             * indexes array. if necessary, triangulates the face.
             */
            populate_triangle_indexes(ga, gfi, &ti, face_idx, texture_mode, normal_mode);
        }
    } /* loop exits when all polygons within the current grouping have
       * been triangulated (if necessary) and the vertex, normal and
       * texture vertices indexes have been placed in the array
       * 'ti.index_arr_tri'.
       */

    /* if no triangles exist to process, report this and return */
    if (ti.num_tri == 0) {
        bu_log("WARNING: No triangles to output, dropped (%zu) of (%zu) faces for obj file face grouping name (%s), obj file face grouping index (%zu)\n",
	       num_faces_killed, gfi->num_faces, bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        return 0;
    }

    populate_sort_indexes(&ti);

    sort_indexes(&ti);

    create_unique_indexes(&ti);

    create_bot_float_arrays(ga, &ti, bot_thickness, conv_factor);

    /* added 1 to grouping_index so the index number used in the
     * primitive name matches the obj file index since internal
     * indexes from libobj start at 0 but obj file indexes start at 1.
     */
    switch (gfi->closure_status) {
        case SURF_UNTESTED:
            bu_vls_sprintf(gfi->primitive_name, "%s.%zu.%d.b.u.s", bu_vls_addr(gfi->raw_grouping_name),
			   gfi->grouping_index + 1, gfi->face_type);
            break;
        case SURF_CLOSED:
            bu_vls_sprintf(gfi->primitive_name, "%s.%zu.%d.b.c.s", bu_vls_addr(gfi->raw_grouping_name),
			   gfi->grouping_index + 1, gfi->face_type);
            break;
        case SURF_OPEN:
            bu_vls_sprintf(gfi->primitive_name, "%s.%zu.%d.b.o.s", bu_vls_addr(gfi->raw_grouping_name),
			   gfi->grouping_index + 1, gfi->face_type);
            break;
    }

    cleanup_name(gfi->primitive_name);

    if (create_bot_int_arrays(&ti)) {
        bu_log("ERROR: function create_bot_int_arrays returned an error, aborting bot creation for object (%s)\n",
               bu_vls_addr(gfi->raw_grouping_name));
        free_ti(&ti);
        return 1;
    }

    /* write bot to ".g" file */
    if (ti.tri_type == FACE_NV || ti.tri_type == FACE_TNV) {
        ret = mk_bot_w_normals(outfp, bu_vls_addr(gfi->primitive_name),
		ti.bot_mode, RT_BOT_UNORIENTED,
		RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS, 
		ti.bot_num_vertices, ti.bot_num_faces, ti.bot_vertices,
		ti.bot_faces, ti.bot_thickness, ti.bot_face_mode,
		ti.bot_num_normals, ti.bot_normals, ti.bot_face_normals);
    } else {
        ret = mk_bot(outfp, bu_vls_addr(gfi->primitive_name), ti.bot_mode,
		RT_BOT_UNORIENTED, 0, ti.bot_num_vertices, ti.bot_num_faces,
		ti.bot_vertices, ti.bot_faces, ti.bot_thickness,
		ti.bot_face_mode);
    }

    if (ret) {
        bu_log("ERROR: Make BOT failed for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
    }

    free_ti(&ti);

    return ret;
}


/*
 * O U T P U T _ T O _ N M G
 *
 * For a grouping of faces, write a nmg primitive or volume-mode-bot
 * primitive to a BRL-CAD database file (i.e. ".g" file). This
 * function will return a non-zero value if no primitive was output.
 * Only face groupings with closed surfaces will be output. Closure is
 * determined by the function nmg_check_closed_shell. A primitive will
 * not be output if the face grouping fails the closure test or an
 * error occured during processing. A return value of 1 indicates no
 * primitive was output because the geometry was not closed. A return
 * value of 2 indicates no primitive was output because an nmg bomb
 * occured. The output_mode option determines if an nmg or
 * volume-mode-bot will be output. The normal_mode will determine if
 * normals provided in the obj file will be inserted into the nmg
 * structure. Normal_mode should be set to IGNR_NORM since, inserted
 * normals are not used for computing face normals and appear to
 * provide no benefit. The tolerance values passed to this function
 * (within bn_tol) must be those which will be used when raytracing
 * (as defined in rtip->rti_tol) otherwise raytracing errors will
 * occur. Primitives will be named according to the grouping name in
 * the obj file with the following exceptions. Characters invalid for
 * BRL-CAD primitive names and operating system file names are
 * converted to underscore. The remaining portions of the name
 * indicate the decimal index number of the grouping in the obj file,
 * face_type number of the grouping {1=v|2=tv|3=nv|4=tnv}, primitive
 * type {n=nmg|v=bot-via-nmg} and closure status {c=closed}.
 *
 * Recommended function improvements:
 * - Profile the called nmg functions to determine where the
 *   performance bottlenecks are occurring.
 * - Provide cleaner status to the user while processing.
 * - Investigate usefulness of inserting obj file face normals into
 *   nmg structure.
 * - If unable to unitize a normal, determine how to deal with this.
 */
int
output_to_nmg(struct ga_t *ga,
              struct gfi_t *gfi,
              struct rt_wdb *outfp, 
              fastf_t conv_factor, 
              struct bn_tol *tol,
              int normal_mode,      /* PROC_NORM, IGNR_NORM */
              int output_mode,      /* OUT_NMG, OUT_VBOT */
              int face_test_type)
{
    struct model *m = (struct model *)NULL;
    struct nmgregion *r = (struct nmgregion *)NULL;
    struct shell *s = (struct shell *)NULL;
    struct bu_ptbl faces;  /* table of faces for one element */
    struct faceuse *fu = (struct faceuse *)NULL;
    struct vertexuse *vu = (struct vertexuse *)NULL;
    struct vertexuse *vu2 = (struct vertexuse *)NULL;
    struct loopuse *lu = (struct loopuse *)NULL;
    struct edgeuse *eu = (struct edgeuse *)NULL;
    struct loopuse *lu2 = (struct loopuse *)NULL;
    struct edgeuse *eu2 = (struct edgeuse *)NULL;
    size_t face_idx = 0;                 /* index into faces within for-loop */
    size_t vert_idx = 0;                 /* index into vertices within for-loop */
    size_t shell_vert_idx = 0;           /* index into vertices for entire nmg shell */
    size_t num_entities_fused = 0;
    int ret = 1;                         /* function return value, default to failure */
    plane_t pl;                          /* plane equation for face */
    fastf_t tmp_v[3] = {0.0, 0.0, 0.0};  /* temporary vertex */
    fastf_t tmp_w = 0.0;                 /* temporary weight */
    fastf_t tmp_rn[3] = {0.0, 0.0, 0.0}; /* temporary reverse normal */
    fastf_t tmp_n[3] = {0.0, 0.0, 0.0};  /* temporary normal */
    fastf_t tmp_t[3] = {0.0, 0.0, 0.0};  /* temporary texture vertex */
    size_t vofi = 0;                     /* vertex obj file index */
    size_t nofi = 0;                     /* normal obj file index */
    size_t tofi = 0;                     /* texture vertex obj file index */

    struct vertex **verts = (struct vertex **)NULL;

    size_t num_faces_killed = 0; /* number of degenerate faces killed in the current shell */

    m = nmg_mm();
    r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &r->s_hd);
    NMG_CK_MODEL(m);
    NMG_CK_REGION(r);
    NMG_CK_SHELL(s);

    /* initialize tables */
    bu_ptbl_init(&faces, 64, " &faces ");

    verts = (struct vertex **)bu_calloc(gfi->tot_vertices, sizeof(struct vertex *), "verts");
    memset((void *)verts, 0, sizeof(struct vertex *) * gfi->tot_vertices);

    /* begin bomb protection */
    if (BU_SETJUMP) {
        BU_UNSETJUMP; /* relinquish bomb protection */

        /* Sometimes the NMG library adds debugging bits when it
         * detects an internal error, before before bombing out.
         */
        rt_g.NMG_debug = NMG_debug; /* restore mode */

        bu_ptbl_reset(&faces);
        if (verts != (struct vertex **)NULL) {
            /* sanity check */
            bu_free(verts, "verts");
        }
        if (m != (struct model *)NULL) {
            /* sanity check */
            nmg_km(m);
        }

        bu_log("ERROR: caught nmg bomb for obj file face grouping name (%s), obj file face grouping index (%zu)\n",
	       bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);

        return 2; /* return code 2 indicates nmg bomb occured */
    }

    shell_vert_idx = 0;
    NMG_CK_SHELL(s);
    /* loop thru all the polygons (i.e. faces) to be placed in the current shell/region/model */
    for (face_idx = 0 ; face_idx < gfi->num_faces ; face_idx++) {

        /* test for degenerate face */
        if (test_face(ga, gfi, face_idx, conv_factor, tol, face_test_type, 0)) {
            num_faces_killed++;
        } else {
            fu = nmg_cface(s, (struct vertex **)&(verts[shell_vert_idx]),
			   (int)(gfi->num_vertices_arr[face_idx]));
            lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
            eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
            lu2 = BU_LIST_FIRST(loopuse, &fu->fumate_p->lu_hd);
            eu2 = BU_LIST_FIRST(edgeuse, &lu2->down_hd);


            for (vert_idx = 0 ; vert_idx < gfi->num_vertices_arr[face_idx] ; vert_idx++) {
                retrieve_coord_index(ga, gfi, face_idx, vert_idx, tmp_v, tmp_n, tmp_t,
                                     &tmp_w, &vofi, &nofi, &tofi); 

                VSCALE(tmp_v, tmp_v, conv_factor);

                NMG_CK_VERTEX(eu->vu_p->v_p);
                nmg_vertex_gv(eu->vu_p->v_p, tmp_v);

                if ((gfi->face_type == FACE_NV || gfi->face_type == FACE_TNV) && (normal_mode == PROC_NORM)) {
                    if (MAGNITUDE(tmp_n) < VDIVIDE_TOL) {
                        bu_log("ERROR: Unable to unitize normal (%f)(%f)(%f) for obj file face index (%zu), obj file face grouping name (%s), obj file face grouping index (%zu)\n",
                               tmp_n[0], tmp_n[1], tmp_n[2],
                               gfi->obj_file_face_idx_arr[face_idx] + 1,
                               bu_vls_addr(gfi->raw_grouping_name),
                               gfi->grouping_index + 1);
                    } else {
                        VUNITIZE(tmp_n);
                    }

                    VREVERSE(tmp_rn, tmp_n);

                    /* assign this normal to all uses of this vertex */
                    for (BU_LIST_FOR(vu, vertexuse, &eu->vu_p->v_p->vu_hd)) {
                        NMG_CK_VERTEXUSE(vu);
                        nmg_vertexuse_nv(vu, tmp_n);
                    }
                    for (BU_LIST_FOR(vu2, vertexuse, &eu2->vu_p->v_p->vu_hd)) {
                        NMG_CK_VERTEXUSE(vu2);
                        nmg_vertexuse_nv(vu2, tmp_rn);
                    }
                }

                eu = BU_LIST_NEXT(edgeuse, &eu->l);
                eu2 = BU_LIST_NEXT(edgeuse, &eu2->l);
                shell_vert_idx++;

            } /* this loop exits when all the vertices and their normals
               * for the current polygon/faceuse has been inserted into
               * their appropriate structures
               */

            NMG_CK_FACEUSE(fu);
            if (nmg_loop_plane_area(BU_LIST_FIRST(loopuse, &fu->lu_hd), pl) < 0.0) {
                if (verbose || debug) {
                    bu_log("WARNING: Failed nmg_loop_plane_area for obj file face index (%zu), obj file face grouping name (%s), obj file face grouping index (%zu)\n",
                           gfi->obj_file_face_idx_arr[face_idx] + 1,
                           bu_vls_addr(gfi->raw_grouping_name),
                           gfi->grouping_index + 1);
                    nmg_pr_fu_briefly(fu, "");
                }
                (void)nmg_kfu(fu);
                fu = (struct faceuse *)NULL;
                num_faces_killed++;
            } else {
                nmg_face_g(fu, pl);
                nmg_face_bb(fu->f_p, tol);
                bu_ptbl_ins(&faces, (long *)fu);
            }

        } /* close of degenerate_face if test */

    } /* loop exits when all polygons within the current grouping
       * has been placed within one nmg shell, inside one nmg region
       * and inside one nmg model
       */

    if (verbose || debug) {
        bu_log("Killed (%zu) faces in obj file face grouping name (%s), obj file face grouping index (%zu)\n", num_faces_killed, bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
    }

    if (!BU_PTBL_END(&faces)) {
        if (verbose || debug) {
            bu_log("No faces in obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }
    } else {
        /* run nmg_model_fuse */
        if ((verbose > 1) || debug) {
            bu_log("Running nmg_model_fuse on (%ld) faces from obj file face grouping name (%s), obj file face grouping index (%zu)\n", BU_PTBL_END(&faces), bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }
        num_entities_fused = nmg_model_fuse(m, tol);
        if ((verbose > 1) || debug) {
            bu_log("Completed nmg_model_fuse for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
            bu_log("Fused (%d) entities in obj file face grouping name (%s), obj file face grouping index (%zu)\n", num_entities_fused, bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }

        /* run nmg_gluefaces, run nmg_model_vertex_fuse before nmg_gluefaces */
        if ((verbose > 1) || debug) {
            bu_log("Running nmg_gluefaces on (%ld) faces from obj file face grouping name (%s), obj file face grouping index (%zu)\n", BU_PTBL_END(&faces), bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }
        nmg_gluefaces((struct faceuse **)BU_PTBL_BASEADDR(&faces), BU_PTBL_END(&faces), tol);
        if ((verbose > 1) || debug) {
            bu_log("Completed nmg_gluefaces for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }

        /* mark edges as real */
        if ((verbose > 1) || debug) {
            bu_log("Running nmg_mark_edges_real with approx (%ld) faces from obj file face grouping name (%s), obj file face grouping index (%zu)\n", BU_PTBL_END(&faces), bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }
        (void)nmg_mark_edges_real(&s->l.magic);
        if ((verbose > 1) || debug) {
            bu_log("Completed nmg_mark_edges_real for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }
        /* compute geometry for region and shell */
        if ((verbose > 1) || debug) {
            bu_log("Running nmg_region_a with approx (%ld) faces from obj file face grouping name (%s), obj file face grouping index (%zu)\n", BU_PTBL_END(&faces), bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }
        nmg_region_a(r, tol);
        if ((verbose > 1) || debug) {
            bu_log("Completed nmg_region_a for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }
        if ((verbose > 1) || debug) {
            bu_log("Running nmg_kill_cracks with approx (%ld) faces from obj file face grouping name (%s), obj file face grouping index (%zu)\n", BU_PTBL_END(&faces), bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
        }
        if (nmg_kill_cracks(s)) {
            if ((verbose > 1) || debug) {
                bu_log("Completed nmg_kill_cracks for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
            }
            bu_log("Object %s has no faces\n", bu_vls_addr(gfi->raw_grouping_name));
        } else {
            if ((verbose > 1) || debug) {
                bu_log("Completed nmg_kill_cracks for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
            }
            if ((verbose > 1) || debug) {
                bu_log("Running nmg_rebound with approx (%ld) faces from obj file face grouping name (%s), obj file face grouping index (%zu)\n", BU_PTBL_END(&faces), bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
            }
            nmg_rebound(m, tol);
            if ((verbose > 1) || debug) {
                bu_log("Completed nmg_rebound for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
            }
            /* run nmg_model_vertex_fuse before nmg_check_closed_shell */
            if ((verbose > 1) || debug) {
                bu_log("Running nmg_check_closed_shell with approx (%ld) faces from obj file face grouping name (%s), obj file face grouping index (%zu)\n", BU_PTBL_END(&faces), bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
            }
            if (!nmg_check_closed_shell(s, tol)) {
                /* true when surface is closed */
                if ((verbose > 1) || debug) {
                    bu_log("Completed nmg_check_closed_shell for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
                }
                if ((verbose > 1) || debug) {
                    bu_log("Running nmg_fix_normals with approx (%ld) faces from obj file face grouping name (%s), obj file face grouping index (%zu)\n", BU_PTBL_END(&faces), bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
                }
                nmg_fix_normals(s, tol);
                if ((verbose > 1) || debug) {
                    bu_log("Completed nmg_fix_normals for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
                }
                if (output_mode == OUT_NMG) {
                    /* make nmg */
                    /* added 1 to grouping_index so the index number
                     * used in the primitive name matches the obj file
                     * index since internal indexes from libobj start
                     * at 0 but obj file indexes start at 1.
                     */
                    bu_vls_sprintf(gfi->primitive_name, "%s.%zu.%d.n.c.s",
				   bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1, gfi->face_type);
                    cleanup_name(gfi->primitive_name);
                    if ((verbose > 1) || debug) {
                        bu_log("Running mk_nmg with approx (%ld) faces from obj file face grouping name (%s), obj file face grouping index (%zu)\n", BU_PTBL_END(&faces), bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
                    }
                    /* the model (m) is freed when mk_nmg completes */
                    if (mk_nmg(outfp, bu_vls_addr(gfi->primitive_name), m) < 0) {
                        bu_log("ERROR: Completed mk_nmg but FAILED for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
                    } else {
                        if ((verbose > 1) || debug) {
                            bu_log("Completed mk_nmg for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
                        }
                        ret = 0; /* set return success */
                    }
                } else {
                    /* make volume bot */
                    if (output_mode != OUT_VBOT) {
                        bu_log("NOTE: Currently bot-via-nmg can only create volume bots, ignoring plate mode option and creating volume bot instead.\n");
                    }
                    /* added 1 to grouping_index so the index number
                     * used in the primitive name matches the obj file
                     * index since internal indexes from libobj start
                     * at 0 but obj file indexes start at 1.
                     */
                    bu_vls_sprintf(gfi->primitive_name, "%s.%zu.%d.v.c.s",
				   bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1, gfi->face_type);
                    cleanup_name(gfi->primitive_name);
                    if ((verbose > 1) || debug) {
                        bu_log("Running mk_bot_from_nmg with approx (%ld) faces from obj file face grouping name (%s), obj file face grouping index (%zu)\n", BU_PTBL_END(&faces), bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
                    }
                    if (mk_bot_from_nmg(outfp, bu_vls_addr(gfi->primitive_name), s) < 0) {
                        bu_log("ERROR: Completed mk_bot_from_nmg but FAILED for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
                    } else {
                        if ((verbose > 1) || debug) {
                            bu_log("Completed mk_bot_from_nmg for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
                        }
                        ret = 0; /* set return success */
                        nmg_km(m); /* required for mk_bot_from_nmg but not mk_nmg */
                    }
                }
            } else {
                if (verbose || debug) {
                    bu_log("WARNING: Function nmg_check_closed_shell found no surface closure for obj file face grouping name (%s), obj file face grouping index (%zu)\n", bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index + 1);
                }
            }
        }
    } 

    BU_UNSETJUMP; /* relinquish bomb catch protection */

    bu_ptbl_reset(&faces);
    if (verts != (struct vertex **)NULL) {
        /* sanity check */
        bu_free(verts, "verts");
    }
    if (ret) {
        /* if failure kill model */
        nmg_km(m);
    }

    return ret;
}


/*
 * S T R 2 M M
 *
 * Validate unit string and output conversion factor to millimeters.
 * If the string is not a standard units identifier, the function
 * assumes a custom conversion factor was specified. A valid null
 * terminated string is expected as input. Return of 0 is success,
 * return of 1 is failure.
 */
int
str2mm(const char *units_string, fastf_t *conv_factor)
{
    struct bu_vls str;
    fastf_t tmp_value = 0.0;
    char *endp = (char *)NULL;
    int ret = 0;

    bu_vls_init(&str);

    if ((units_string == (char *)NULL) || (conv_factor == (fastf_t *)NULL)) {
        bu_log("NULL pointer(s) passed to function 'str2mm'.\n");
        ret = 1;
    } else {
        bu_vls_strcat(&str, units_string);
        bu_vls_trimspace(&str);

        tmp_value = (fastf_t)strtod(bu_vls_addr(&str), &endp);
        if ((endp != bu_vls_addr(&str)) && (*endp == '\0')) {
            /* convert to double success */
            *conv_factor = tmp_value;
        } else if ((tmp_value = (fastf_t)bu_mm_value(bu_vls_addr(&str))) > 0.0) {
            *conv_factor = tmp_value;
        } else {
            bu_log("Invalid units string '%s'\n", units_string);
            ret = 1;
        }
    }
    bu_vls_free(&str);
    return ret;
}


/*
 * P R O C E S S _ B _ M O D E _ O P T I O N
 *
 * This function is executed from main when the user selects mode
 * option 'b' from the command line. (i.e. output to "native bot")
 */
void
process_b_mode_option(struct ga_t *ga,
                      struct gfi_t *gfi,
                      struct rt_wdb *outfp, 
                      fastf_t conv_factor, 
                      struct bn_tol *tol,
                      fastf_t bot_thickness, /* bot plate thickness (mm) */
                      int normal_mode,       /* PROC_NORM, IGNR_NORM */
                      int plot_mode,         /* PLOT_OFF, PLOT_ON */
		      /* RT_BOT_SURFACE, RT_BOT_PLATE, RT_BOT_PLATE_NOCOS */
                      unsigned char open_bot_output_mode,
                      int face_test_type)    /* TEST_ALL, TEST_NUM_VERT */
{
    (void)fuse_vertex(ga, gfi, conv_factor, tol, FUSE_VERT, FUSE_EQUAL);
    (void)retest_grouping_faces(ga, gfi, conv_factor, face_test_type, tol);
    if (!test_closure(ga, gfi, conv_factor, plot_mode, face_test_type, tol)) {
        (void)output_to_bot(ga, gfi, outfp, conv_factor, tol, bot_thickness,
		IGNR_TEX, normal_mode, RT_BOT_SOLID, face_test_type);
    } else {
        (void)output_to_bot(ga, gfi, outfp, conv_factor, tol, bot_thickness,
		IGNR_TEX, normal_mode, open_bot_output_mode, face_test_type);
    }
}

/*
 * P R O C E S S _ N V _ M O D E _ O P T I O N
 *
 * This function is executed from main when the user selects mode
 * option 'n' or 'v' from the command line. The 'n' indicates output
 * to nmg and 'v' indicates output to volume-mode-bot via nmg. If
 * during nmg or volume-mode-bot creation a 'no closure' condition is
 * encountered, output of the grouping will be attempted to
 * 'native bot'. If during nmg or volume-mode-bot creation an nmg-bomb
 * occurs then no primitive will be output unless cont_on_nmg_bomb is
 * set to true which will then cause output of the grouping to be
 * attempted to 'native bot'. A return value of 0 indicates no
 * nmg-bomb occured, a return of 1 indicates an nmg-bomb occured
 * and cont_on_nmg_bomb_flag was set to false.
 */
int
process_nv_mode_option(struct ga_t *ga,
                       struct gfi_t *gfi,
                       struct rt_wdb *outfp, 
                       fastf_t conv_factor, 
                       struct bn_tol *tol,
                       fastf_t bot_thickness, /* bot plate thickness (mm) */
                       int nmg_output_mode,   /* OUT_NMG, OUT_VBOT */
                       int plot_mode,         /* PLOT_OFF, PLOT_ON */
		       /* RT_BOT_SURFACE, RT_BOT_PLATE, RT_BOT_PLATE_NOCOS */
                       unsigned char open_bot_output_mode,
		       /* output_to_nmg (nmg or bot-via-nmg) face_test_type
			* TEST_ALL, TEST_NUM_VERT
			*/
                       int nmg_face_test_type, 
		       /* output_to_bot (i.e. native) face_test_type
			* TEST_ALL, TEST_NUM_VERT
			*/
                       int native_face_test_type,
                       int cont_on_nmg_bomb_flag)
{
    int ret = 0;
    ret = output_to_nmg(ga, gfi, outfp, conv_factor, tol, IGNR_NORM,
	nmg_output_mode, nmg_face_test_type);

    if ((ret == 1) || ((ret == 2) && cont_on_nmg_bomb_flag)) {
        (void)fuse_vertex(ga, gfi, conv_factor, tol, FUSE_VERT, FUSE_WI_TOL);
        (void)retest_grouping_faces(ga, gfi, conv_factor, native_face_test_type,
		tol);
        if (!test_closure(ga, gfi, conv_factor, plot_mode,
		    native_face_test_type, tol))
	{
            (void)output_to_bot(ga, gfi, outfp, conv_factor, tol, bot_thickness,
		    IGNR_TEX, IGNR_NORM, RT_BOT_SOLID, native_face_test_type);
        } else {
            (void)output_to_bot(ga, gfi, outfp, conv_factor, tol, bot_thickness,
		    IGNR_TEX, IGNR_NORM, open_bot_output_mode,
		    native_face_test_type);
        }
    }

    /* convert output_to_nmg return code to process_nv_mode_option
     * return code
     */
    if ((ret == 2) && !cont_on_nmg_bomb_flag) {
        ret = 1;
    } else {
        ret = 0;
    }

    return ret;
}


int
main(int argc, char **argv)
{
    char *prog = *argv;
    static char *input_file_name;    /* input file name */
    static char *brlcad_file_name;   /* output file name */
    struct rt_wdb *fd_out;	     /* Resulting BRL-CAD file */
    int ret_val = 0;
    FILE *my_stream;
    struct ga_t ga;
    struct gfi_t *gfi = (struct gfi_t *)NULL; /* grouping face indices */
    size_t i = 0;
    int c;
    const char *parse_messages = (char *)0;
    int parse_err = 0;
    int face_type_idx = 0;
    double dist_tmp = 0.0;
    struct bn_tol tol_struct;
    struct bn_tol *tol;
    int cont_on_nmg_bomb_flag = 0;
    int stop_processing_flag = 0;
    int nmg_face_test_type = TEST_NUM_VERT;
    int native_face_test_type = TEST_ALL;
    int nmg_output_mode = OUT_VBOT;
    int open_bot_output_mode = RT_BOT_PLATE;
    /* default: do not create plot files of open edges for groupings */
    int plot_mode = PLOT_OFF;
    /* default: import face normals if included in obj file */
    int normal_mode = PROC_NORM;
    char grouping_option = 'g';  /* default: group by obj file groups */
    char mode_option = 'v';      /* default: import as bot-via-nmg */
    fastf_t conv_factor = 0.0;
    fastf_t bot_thickness = 0.0;
    int user_defined_units_flag = 0;
    int user_bot_thickness_flag = 0;
    int user_open_bot_output_mode_flag = 0;
    time_t start_time;
    time_t end_time;
    time_t elapsed_time;
    time_t overall_start_time;
    time_t overall_end_time;
    time_t overall_elapsed_time;
    struct tm *timep;

    /* the raytracer tolerance values (rtip->rti_tol) need to match
     * these otherwise raytrace errors will result. the defaults for
     * the rti_tol are set in the function rt_new_rti.  either use
     * here the rti_tol defaults or when raytracing change the
     * raytracer values to these.
     */
    tol = &tol_struct;
    tol->magic = BN_TOL_MAGIC;
    /* default which should be equal to the raytracer default tolerance */
    tol->dist = 0.0005;
    tol->dist_sq = tol->dist * tol->dist;
    /* default which should be equal to the raytracer default tolerance */
    tol->perp = 1e-6;
    tol->para = 1 - tol->perp;

    if (argc < 2) {
        bu_exit(1, usage, argv[0]);
    }

    while ((c = bu_getopt(argc, argv, "cpidx:X:vt:h:m:u:g:o:")) != -1) {
        switch (c) {
            case 'c': /* continue processing on nmg bomb */
                cont_on_nmg_bomb_flag = 1;
                break;
            case 'p': /* create plot files for open edges */
                plot_mode = PLOT_ON;
                break;
            case 'i': /* ignore normals if provided in obj file */
                normal_mode = IGNR_NORM;
                break;
            case 'd':
		/* turn on local debugging, displays additional information of
		 * developer interest on stderr
		 */
                debug = 1;
                break;
            case 'x': /* set librt debug level */
                sscanf(bu_optarg, "%x", (unsigned int *)&rt_g.debug);
                bu_printb("librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT);
                bu_log("\n");
                break;
            case 'X': /* set nmg debug level */
                sscanf(bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug);
                NMG_debug = rt_g.NMG_debug;
                bu_printb("librt rt_g.NMG_debug", rt_g.NMG_debug,
			NMG_DEBUG_FORMAT);
                bu_log("\n");
                break;
            case 'v': /* displays additional information of user interest */
                verbose++;
                break;
            case 't': /* distance tolerance in mm units */
                dist_tmp = (double)atof(bu_optarg);
                if (dist_tmp <= 0.0) {
                    bu_log("Distance tolerance must be greater then zero.\n");
                    bu_exit(EXIT_FAILURE, "Type '%s' for usage.\n", argv[0]);
                }
                tol->dist = dist_tmp;
                tol->dist_sq = tol->dist * tol->dist;
                break;
            case 'h': /* plate-mode-bot thickness in mm units */
                bot_thickness = (fastf_t)atof(bu_optarg);
                user_bot_thickness_flag = 1;
                break;
            case 'm': /* output mode */
                switch (bu_optarg[0]) {
                    case 'b': /* native bot */
                        mode_option = bu_optarg[0];
                        break;
                    case 'n': /* nmg */
                        nmg_output_mode = OUT_NMG;
                        mode_option = bu_optarg[0];
                        break;
                    case 'v': /* bot via nmg */
                        nmg_output_mode = OUT_VBOT;
                        mode_option = bu_optarg[0];
                        break;
                    default:
                        bu_log("Invalid mode option '%c'.\n", bu_optarg[0]);
                        bu_exit(EXIT_FAILURE, "Type '%s' for usage.\n",
				argv[0]);
                        break;
                }
                break;
            case 'u': /* units */
                if (str2mm(bu_optarg, &conv_factor)) {
                    bu_exit(EXIT_FAILURE, "Type '%s' for usage.\n", argv[0]);
                }
                user_defined_units_flag = 1;
                break;
            case 'g': /* face grouping */
                switch (bu_optarg[0]) {
                    case 'g': /* group grouping */
                    case 'o': /* object grouping */
                    case 'm': /* material grouping */
                    case 't': /* texture grouping */
                    case 'n': /* no grouping */
                        grouping_option = bu_optarg[0];
                        break;
                    default:
                        bu_log("Invalid grouping option '%c'.\n", bu_optarg[0]);
                        bu_exit(EXIT_FAILURE, "Type '%s' for usage.\n",
				argv[0]);
                        break;
                }
                break;
            case 'o': /* open surface output bot type */
                switch (bu_optarg[0]) {
                    case 's': /* surface bot */
                        open_bot_output_mode = RT_BOT_SURFACE;
                        user_open_bot_output_mode_flag = 1;
                        break;
                    case 'p': /* plate bot */
                        open_bot_output_mode = RT_BOT_PLATE;
                        user_open_bot_output_mode_flag = 1;
                        break;
                    case 'n': /* plate bot nocos */
                        open_bot_output_mode = RT_BOT_PLATE_NOCOS;
                        user_open_bot_output_mode_flag = 1;
                        break;
                    default:
                        bu_log("Invalid open surface 'native-bot' output bot "
			       "type '%c'.\n", bu_optarg[0]);
                        bu_exit(EXIT_FAILURE, "Type '%s' for usage.\n",
				argv[0]);
                        break;
                }
                break;
            default:
                bu_log("Invalid option '%c'.\n", c);
                bu_exit(EXIT_FAILURE, "Type '%s' for usage.\n", argv[0]);
                break;
        }
    }

    if (argc - bu_optind != 2) {
	bu_exit(EXIT_FAILURE, "Invalid number of arguments.\nType '%s' for "
	   "usage.\n", argv[0]); 
    }

    input_file_name = argv[bu_optind];
    bu_optind++;
    brlcad_file_name = argv[bu_optind];

    /* if user did not specify units, abort since units are required */
    if (!user_defined_units_flag) {
        bu_log("'units_str' was not specified but is required\n");
        bu_exit(EXIT_FAILURE, "Type '%s' for usage.\n", argv[0]);
    }

    /* if user did not specify bot type of open bots, abort since this is
     * required
     */
    if (!user_open_bot_output_mode_flag) {
        bu_log("'open_bot_type' was not specified but is required\n");
        bu_exit(EXIT_FAILURE, "Type '%s' for usage.\n", argv[0]);
    }

    /* if plate bots were selected as the open bot output type but the
     * user did not specify plate bot thickness, abort since thickness
     * is required
     */
    if ((!user_bot_thickness_flag) && ((open_bot_output_mode == RT_BOT_PLATE)
        || (open_bot_output_mode == RT_BOT_PLATE_NOCOS)))
    {
	bu_log("'plate_thickness' was not specified but is required\n");
	bu_exit(EXIT_FAILURE, "Type '%s' for usage.\n", argv[0]);
    }

    /* initialize ga structure */
    memset((void *)&ga, 0, sizeof(struct ga_t));

    (void)time(&overall_start_time);

    bu_log("Start time %s", asctime(localtime(&overall_start_time)));

    bu_log("\nSELECTED OPTIONS:\n");
    bu_log("\tUsing distance_tolerance (%fmm)\n", tol->dist);

    bu_log("\t");
    switch (grouping_option) {
        case 'g':
            bu_log("Using grouping_option (g) group\n");
            break;
        case 'o':
            bu_log("Using grouping_option (o) object\n");
            break;
        case 'm':
            bu_log("Using grouping_option (m) material\n");
            break;
        case 't':
            bu_log("Using grouping_option (t) texture\n");
            break;
        case 'n':
            bu_log("Using grouping_option (n) none\n");
            break;
    }

    bu_log("\t");
    switch (mode_option) {
        case 'b':
            bu_log("Using mode_option (b) native-bot\n");
            break;
        case 'n':
            bu_log("Using mode_option (n) nmg\n");
            break;
        case 'v':
            bu_log("Using mode_option (v) bot-via-nmg\n");
            break;
    }

    bu_log("\tUsing conversion factor (%f)\n", conv_factor);

    bu_log("\t");
    switch (open_bot_output_mode) {
        case RT_BOT_SURFACE:
            bu_log("Using open_bot_type option (s) surface bot\n");
            break;
        case RT_BOT_PLATE:
            bu_log("Using open_bot_type option (p) plate bot\n");
            break;
        case RT_BOT_PLATE_NOCOS:
            bu_log("Using open_bot_type option (n) plate-nocos bot\n");
            break;
    }

    if ((user_bot_thickness_flag) && ((open_bot_output_mode == RT_BOT_PLATE)
        || (open_bot_output_mode == RT_BOT_PLATE_NOCOS)))
    {
	bu_log("\tUsing bot_thickness of (%lfmm)\n", bot_thickness);
    }
    
    if ((normal_mode == PROC_NORM) && (mode_option == 'b')) {
        bu_log("\tObj file normals processed\n");
    } else {
        bu_log("\tObj file normals ignored\n");
    }

    if (mode_option != 'b') {
        if (cont_on_nmg_bomb_flag) {
            bu_log("\tContinue on nmg-bomb enabled\n");
        } else {
            bu_log("\tContinue on nmg-bomb disabled\n");
        }
    }

    if (plot_mode == PLOT_ON) {
        bu_log("\tPlot open edges enabled\n");
    } else {
        bu_log("\tPlot open edges disabled\n");
    }

    if (verbose) {
        bu_log("\tVerbose message level (%d) enabled\n", verbose);
    } else {
        bu_log("\tVerbose messages disabled\n");
    }

    if (debug) {
        bu_log("\tDebug messages enabled\n");
    } else {
        bu_log("\tDebug messages disabled\n");
    }

    bu_log("\tInput file name (%s)\n", input_file_name);
    bu_log("\tOutput file name (%s)\n\n", brlcad_file_name);
 
    if ((my_stream = fopen(input_file_name, "r")) == NULL) {
        bu_log("Cannot open input file (%s)\n", input_file_name);
        perror(prog);
        return EXIT_FAILURE;
    }

    if ((fd_out = wdb_fopen(brlcad_file_name)) == NULL) {
        bu_log("Cannot create new BRL-CAD file (%s)\n", brlcad_file_name);
        perror(prog);
        bu_exit(1, NULL);
    }

    if ((ret_val = obj_parser_create(&ga.parser)) != 0) {
        if (ret_val == ENOMEM) {
            bu_log("Can not allocate an obj_parser_t object, Out of Memory.\n");
        } else {
            bu_log("Can not allocate an obj_parser_t object, "
		   "Undefined Error (%d)\n", ret_val);
        }

        /* it is unclear if obj_parser_destroy must be run if
	 * obj_parser_create failed
	 */
        obj_parser_destroy(ga.parser);

        if (fclose(my_stream) != 0) {
            bu_log("Unable to close file.\n");
        }

        perror(prog);
	return EXIT_FAILURE;
    }

    if ((parse_err = obj_fparse(my_stream, ga.parser, &ga.contents)) != 0) {
	if (parse_err < 0) {
	    /* syntax error */
	    parse_messages = obj_parse_error(ga.parser); 
	    bu_log("obj_fparse, Syntax Error.\n");
	    bu_log("%s\n", parse_messages); 
	} else if (parse_err == ENOMEM) {
	    bu_log("obj_fparse, Out of Memory.\n");
	} else {
	    bu_log("obj_fparse, Other Error.\n");
	}

	/* it is unclear if obj_contents_destroy must be run if obj_fparse
	 * failed */
	obj_contents_destroy(ga.contents);

	obj_parser_destroy(ga.parser);

	if (fclose(my_stream) != 0) {
	    bu_log("Unable to close file.\n");
	}

	return EXIT_FAILURE;
    }

    collect_global_obj_file_attributes(&ga);

    switch (grouping_option) {
        case 'n':
            face_type_idx = FACE_V;
            while ((face_type_idx <= FACE_TNV) && !stop_processing_flag) {
                (void)time(&start_time);
                timep = localtime(&start_time);
                collect_grouping_faces_indexes(&ga, &gfi, face_type_idx,
			GRP_NONE, 0);

                if (gfi != NULL) {
                    bu_log("Grouping start time: %02d:%02d:%02d "
			    "Grouping index: (1 of 1) "
			    "Facetype: (%d) Grouping name: (%s)\n",
			   timep->tm_hour, timep->tm_min, timep->tm_sec,
			   face_type_idx, bu_vls_addr(gfi->raw_grouping_name));

                    switch (mode_option) {
                        case 'b':
                            process_b_mode_option(&ga, gfi, fd_out, conv_factor,
				    tol, bot_thickness, normal_mode, plot_mode,
				    open_bot_output_mode,
				    native_face_test_type);
                            break;
                        case 'n':
                        case 'v':
                            stop_processing_flag = process_nv_mode_option(&ga,
				    gfi, fd_out, conv_factor, tol,
				    bot_thickness, nmg_output_mode, plot_mode,
				    open_bot_output_mode, nmg_face_test_type,
				    native_face_test_type,
				    cont_on_nmg_bomb_flag);
                            break;
                    }

                    if (verbose || debug) {
                        (void)time(&end_time);
                        timep = localtime(&end_time);
                        elapsed_time = end_time - start_time;
                        bu_log("Grouping end time: %02d:%02d:%02d "
				"Duration: %02dh %02dm %02ds "
				"Grouping index: (1 of 1) "
				"Facetype: (%d) "
				"Grouping name: (%s) Primitive name: (%s)\n",
			       timep->tm_hour, timep->tm_min, timep->tm_sec,
			       elapsed_time/3600, (elapsed_time%3600)/60,
			       (elapsed_time%60), face_type_idx,
			       bu_vls_addr(gfi->raw_grouping_name),
			       bu_vls_addr(gfi->primitive_name));
                    }

                    free_gfi(&gfi);
                }
                face_type_idx++;
            }
            break;
        case 'g':
            i = 0;
            while ((i < ga.numGroups) && !stop_processing_flag) {
                face_type_idx = FACE_V;
                while ((face_type_idx <= FACE_TNV) && !stop_processing_flag) {
                    (void)time(&start_time);
                    timep = localtime(&start_time);
                    collect_grouping_faces_indexes(&ga, &gfi, face_type_idx,
			    GRP_GROUP, i);
                    if (gfi != NULL) {
                        bu_log("Grouping start time: %02d:%02d:%02d "
				"Grouping index: (%zu of %zu) "
				"Facetype: (%d) Grouping name: (%s)\n",
			       timep->tm_hour, timep->tm_min, timep->tm_sec,
			       gfi->grouping_index + 1, ga.numGroups,
			       face_type_idx,
			       gfi->raw_grouping_name->vls_str);

                        switch (mode_option) {
                            case 'b':
                                process_b_mode_option(&ga, gfi, fd_out, conv_factor, tol, bot_thickness, 
						      normal_mode, plot_mode, open_bot_output_mode, native_face_test_type);
                                break;
                            case 'n':
                            case 'v':
                                stop_processing_flag = process_nv_mode_option(&ga, gfi, fd_out, conv_factor,
									      tol, bot_thickness, nmg_output_mode, plot_mode, open_bot_output_mode,
									      nmg_face_test_type, native_face_test_type, cont_on_nmg_bomb_flag);
                                break;
                        }

                        if (verbose || debug) {
                            (void)time(&end_time);
                            timep = localtime(&end_time);
                            elapsed_time = end_time - start_time;
                            bu_log("Grouping end time: %02d:%02d:%02d Duration: %02dh %02dm %02ds Grouping index: (%zu of %zu) Facetype: (%d) Grouping name: (%s) Primitive name: (%s)\n",
				   timep->tm_hour, timep->tm_min, timep->tm_sec,
				   elapsed_time/3600, (elapsed_time%3600)/60, (elapsed_time%60),
				   gfi->grouping_index + 1, ga.numGroups, face_type_idx,
				   bu_vls_addr(gfi->raw_grouping_name), bu_vls_addr(gfi->primitive_name));
                        }

                        free_gfi(&gfi);
                    }
                    face_type_idx++;
                }
                i++;
            }
            break;
        case 'o':
            i = 0;
            while ((i < ga.numObjects) && !stop_processing_flag) {
                face_type_idx = FACE_V;
                while ((face_type_idx <= FACE_TNV) && !stop_processing_flag) {
                    (void)time(&start_time);
                    timep = localtime(&start_time);
                    collect_grouping_faces_indexes(&ga, &gfi, face_type_idx, GRP_OBJECT, i);
                    if (gfi != NULL) {
                        bu_log("Grouping start time: %02d:%02d:%02d Grouping index: (%zu of %zu) Facetype: (%d) Grouping name: (%s)\n",
			       timep->tm_hour, timep->tm_min, timep->tm_sec,
			       gfi->grouping_index + 1, ga.numObjects, face_type_idx,
			       bu_vls_addr(gfi->raw_grouping_name));

                        switch (mode_option) {
                            case 'b':
                                process_b_mode_option(&ga, gfi, fd_out, conv_factor, tol, bot_thickness, 
						      normal_mode, plot_mode, open_bot_output_mode, native_face_test_type);
                                break;
                            case 'n':
                            case 'v':
                                stop_processing_flag = process_nv_mode_option(&ga, gfi, fd_out, conv_factor,
									      tol, bot_thickness, nmg_output_mode, plot_mode, open_bot_output_mode,
									      nmg_face_test_type, native_face_test_type, cont_on_nmg_bomb_flag);
                                break;
                        }

                        if (verbose || debug) {
                            (void)time(&end_time);
                            timep = localtime(&end_time);
                            elapsed_time = end_time - start_time;
                            bu_log("Grouping end time: %02d:%02d:%02d Duration: %02dh %02dm %02ds Grouping index: (%zu of %zu) Facetype: (%d) Grouping name: (%s) Primitive name: (%s)\n",
				   timep->tm_hour, timep->tm_min, timep->tm_sec,
				   elapsed_time/3600, (elapsed_time%3600)/60, (elapsed_time%60),
				   gfi->grouping_index + 1, ga.numObjects, face_type_idx,
				   bu_vls_addr(gfi->raw_grouping_name), bu_vls_addr(gfi->primitive_name));
                        }

                        free_gfi(&gfi);
                    }
                    face_type_idx++;
                }
                i++;
            }
            break;
        case 'm':
            i = 0;
            while ((i < ga.numMaterials) && !stop_processing_flag) {
                face_type_idx = FACE_V;
                while ((face_type_idx <= FACE_TNV) && !stop_processing_flag) {
                    (void)time(&start_time);
                    timep = localtime(&start_time);
                    collect_grouping_faces_indexes(&ga, &gfi, face_type_idx, GRP_MATERIAL, i);
                    if (gfi != NULL) {
                        bu_log("Grouping start time: %02d:%02d:%02d Grouping index: (%zu of %zu) Facetype: (%d) Grouping name: (%s)\n",
			       timep->tm_hour, timep->tm_min, timep->tm_sec,
			       gfi->grouping_index + 1, ga.numMaterials, face_type_idx,
			       bu_vls_addr(gfi->raw_grouping_name));

                        switch (mode_option) {
                            case 'b':
                                process_b_mode_option(&ga, gfi, fd_out, conv_factor, tol, bot_thickness, 
						      normal_mode, plot_mode, open_bot_output_mode, native_face_test_type);
                                break;
                            case 'n':
                            case 'v':
                                stop_processing_flag = process_nv_mode_option(&ga, gfi, fd_out, conv_factor,
									      tol, bot_thickness, nmg_output_mode, plot_mode, open_bot_output_mode,
									      nmg_face_test_type, native_face_test_type, cont_on_nmg_bomb_flag);
                                break;
                        }

                        if (verbose || debug) {
                            (void)time(&end_time);
                            timep = localtime(&end_time);
                            elapsed_time = end_time - start_time;
                            bu_log("Grouping end time: %02d:%02d:%02d Duration: %02dh %02dm %02ds Grouping index: (%zu of %zu) Facetype: (%d) Grouping name: (%s) Primitive name: (%s)\n",
				   timep->tm_hour, timep->tm_min, timep->tm_sec,
				   elapsed_time/3600, (elapsed_time%3600)/60, (elapsed_time%60),
				   gfi->grouping_index + 1, ga.numMaterials, face_type_idx,
				   bu_vls_addr(gfi->raw_grouping_name), bu_vls_addr(gfi->primitive_name));
                        }

                        free_gfi(&gfi);
                    }
                    face_type_idx++;
                }
                i++;
            }
            break;
        case 't':
            i = 0;
            while ((i < ga.numTexmaps) && !stop_processing_flag) {
                face_type_idx = FACE_V;
                while ((face_type_idx <= FACE_TNV) && !stop_processing_flag) {
                    (void)time(&start_time);
                    timep = localtime(&start_time);
                    collect_grouping_faces_indexes(&ga, &gfi, face_type_idx, GRP_TEXTURE, i);
                    if (gfi != NULL) {
                        bu_log("Grouping start time: %02d:%02d:%02d Grouping index: (%zu of %zu) Facetype: (%d) Grouping name: (%s)\n",
			       timep->tm_hour, timep->tm_min, timep->tm_sec,
			       gfi->grouping_index + 1, ga.numTexmaps, face_type_idx,
			       bu_vls_addr(gfi->raw_grouping_name));

                        switch (mode_option) {
                            case 'b':
                                process_b_mode_option(&ga, gfi, fd_out, conv_factor, tol, bot_thickness, 
						      normal_mode, plot_mode, open_bot_output_mode, native_face_test_type);
                                break;
                            case 'n':
                            case 'v':
                                stop_processing_flag = process_nv_mode_option(&ga, gfi, fd_out, conv_factor,
									      tol, bot_thickness, nmg_output_mode, plot_mode, open_bot_output_mode,
									      nmg_face_test_type, native_face_test_type, cont_on_nmg_bomb_flag);
                                break;
                        }

                        if (verbose || debug) {
                            (void)time(&end_time);
                            timep = localtime(&end_time);
                            elapsed_time = end_time - start_time;
                            bu_log("Grouping end time: %02d:%02d:%02d Duration: %02dh %02dm %02ds Grouping index: (%zu of %zu) Facetype: (%d) Grouping name: (%s) Primitive name: (%s)\n",
				   timep->tm_hour, timep->tm_min, timep->tm_sec,
				   elapsed_time/3600, (elapsed_time%3600)/60, (elapsed_time%60),
				   gfi->grouping_index + 1, ga.numTexmaps, face_type_idx,
				   bu_vls_addr(gfi->raw_grouping_name), bu_vls_addr(gfi->primitive_name));
                        }

                        free_gfi(&gfi);
                    }
                    face_type_idx++;
                }
                i++;
            }
            break;
    }

    /* running cleanup functions */
    if (debug) {
        bu_log("obj_contents_destroy\n");
    }
    obj_contents_destroy(ga.contents);

    if (debug) {
        bu_log("obj_parser_destroy\n");
    }
    obj_parser_destroy(ga.parser);

    if (debug) {
        bu_log("running fclose\n");
    }
    if (fclose(my_stream) != 0) {
        bu_log("Unable to close file.\n");
        perror(prog);
        return EXIT_FAILURE;
    }

    wdb_close(fd_out);
    rt_clean_resource_complete(NULL, &rt_uniresource);

    (void)time(&overall_end_time);
    overall_elapsed_time = overall_end_time - overall_start_time;

    bu_log("\nDone\n");

    bu_log("Duration %02dh %02dm %02ds\n", overall_elapsed_time/3600,
	   (overall_elapsed_time%3600)/60, (overall_elapsed_time%60));

    bu_log("End time %s", asctime(localtime(&overall_end_time)));

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
