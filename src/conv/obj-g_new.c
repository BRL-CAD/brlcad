/*                     O B J - G _ N E W . C
 * BRL-CAD
 *
 * Copyright (c) 2010 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "bu.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "obj_parser.h"

static char *usage = "%s [-d] [-x rt_debug_flag] input.obj output.g\n\
                         where input.obj is a WaveFront Object file\n\
                         and output.g is the name of a BRL-CAD database file to receive the conversion.\n\
                         The -d option prints additional debugging information.\n\
                         The -x option specifies an RT debug flags (see raytrace.h).\n";
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

typedef const size_t (**arr_1D_t);    /* 1 dimensional array type */
typedef const size_t (**arr_2D_t)[2]; /* 2 dimensional array type */
typedef const size_t (**arr_3D_t)[3]; /* 3 dimensional array type */

typedef size_t (*tri_arr_1D_t)[3];    /* triangle index array 1 dimensional type (v) */
typedef size_t (*tri_arr_2D_t)[3][2]; /* triangle index array 2 dimensional type (tv) or (nv) */
typedef size_t (*tri_arr_3D_t)[3][3]; /* triangle index array 3 dimensional type (tnv) */

typedef size_t (*edge_arr_2D_t)[2]; /* edge array type */

/* grouping face indices type */
struct gfi_t { 
    void      *index_arr_faces;           /* face indices into vertex, normal ,texture vertex lists */
    size_t    *num_vertices_arr;          /* number of vertices for each face within index_arr_faces */
    size_t    *obj_file_face_idx_arr;     /* corresponds to the index of the face within the obj file. this
                                             value is useful to trace face errors back to the obj file. */
    struct     bu_vls *raw_grouping_name; /* raw name of grouping from obj file; group/object/material/texture */
    size_t     num_faces;                 /* number of faces represented by index_arr_faces and num_vertices_arr */
    size_t     max_faces;                 /* maximum number of faces based on current memory allocation */
    short int *face_status;               /* test_face result, 0 = untested, 1 = valid, >1 = degenerate */
    short int  closure_status;            /* i.e. SURF_UNTESTED, SURF_CLOSED, SURF_OPEN */
    size_t     tot_vertices;              /* sum of contents of num_vertices_arr. note: if the face_type
                                             includes normals and/or texture vertices, each vertex must have
                                             an associated normal and/or texture vertice. therefore this
                                             total is also the total of the associated normals and/or texture
                                             vertices. */
    int        face_type;                 /* i.e. FACE_V, FACE_TV, FACE_NV or FACE_TNV */
    int        grouping_type;             /* i.e. GRP_NONE, GRP_GROUP, GRP_OBJECT, GRP_MATERIAL or GRP_TEXTURE */
    size_t     grouping_index;            /* corresponds to the index of the grouping name within the obj file.  
                                             this value is useful to append to the raw_grouping_name to
                                             ensure the resulting name is unique. */
    size_t    *vertex_fuse_map;           /* maps the vertex index of duplicate (i.e. within tolerance) vertices
                                             into a single vertex index. i.e. points all vertices close enough
                                             together to a single vertex */
    short int *vertex_fuse_flag;          /* used during creation of vertex_fuse_map, indicates if the vertex
                                             has already been tested for duplicates or the vertex is a duplicate
                                             of an already processed vertex and does not need to be processed again */
    size_t     vertex_fuse_offset;        /* subtract this value from libobj vertex index to index into the fuse
                                             array to find the libobj vertex index this vertex was fused to */
    size_t     num_vertex_fuse;           /* number of elements in vertex_fuse_map and vertex_fuse_flag arrays */
    size_t    *texture_vertex_fuse_map;   /* same as vertex_fuse_map but for texture vertex */
    short int *texture_vertex_fuse_flag;  /* same as vertex_fuse_flag but for texture vertex */
    size_t     texture_vertex_fuse_offset; /* same as vertex_fuse_offset but for texture vertex */
    size_t     num_texture_vertex_fuse;   /* same as num_vertex_fuse but for texture vertex */
};

/* triangle indices type */
struct ti_t { 
    void    *index_arr_tri; /* triangle indices into vertex, normal, texture vertex lists */
    size_t   num_tri;       /* number of triangles represented by index_arr_triangles */
    size_t   max_tri;       /* maximum number of triangles based on current memory allocation */
    int      tri_type;      /* i.e. FACE_V, FACE_TV, FACE_NV or FACE_TNV */
    size_t  *vsi;           /* triangle vertex sort index array */
    size_t  *vnsi;          /* triangle vertex normal sort index array */
    size_t  *tvsi;          /* triangle texture vertex sort index array */
    size_t  *uvi;           /* unique triangle vertex index array */
    size_t  *uvni;          /* unique triangle vertex normal index array */
    size_t  *utvi;          /* unique triangle texture vertex index array */
    size_t   num_uvi;       /* number of unique triangle vertex indexes in uvi array */
    size_t   num_uvni;      /* number of unique triangle vertex normal index in uvni array */
    size_t   num_utvi;      /* number of unique triangle texture vertex index in utvi array */
    fastf_t *bot_vertices;             /* array of floats for bot vertices [bot_num_vertices*3] */
    fastf_t *bot_thickness;            /* array of floats for face thickness [bot_num_faces] */
    fastf_t *bot_normals;              /* array of floats for bot normals [bot_num_normals*3] */
    fastf_t *bot_texture_vertices;     /* array of floats for texture vertices [bot_num_texture_vertices*3] */
    int     *bot_faces;                /* array of indices into bot_vertices array [bot_num_faces*3] */
    int     *bot_face_normals;         /* array of indices into bot_normals array [bot_num_faces*3] */
    int     *bot_textures;             /* array indices into bot_texture_vertices array */
    int      bot_num_vertices;         /* number of vertices in bot_vertices array */
    int      bot_num_faces;            /* number of faces in bot_faces array */
    int      bot_num_normals;          /* number of normals in bot_normals array */
    int      bot_num_texture_vertices; /* number of textures in bot_texture_vertices array */
};

/* obj file global attributes type */
struct ga_t {                                    /* assigned by ... */
    const obj_polygonal_attributes_t *polyattr_list; /* obj_polygonal_attributes */
    obj_parser_t         parser;                 /* obj_parser_create */
    obj_contents_t       contents;               /* obj_fparse */
    size_t               numPolyAttr;            /* obj_polygonal_attributes */
    size_t               numGroups;              /* obj_groups */
    size_t               numObjects;             /* obj_objects */
    size_t               numMaterials;           /* obj_materials */
    size_t               numTexmaps;             /* obj_texmaps */
    size_t               numVerts;               /* obj_vertices */
    size_t               numNorms;               /* obj_normals */
    size_t               numTexCoords;           /* obj_texture_coord */
    size_t               numNorFaces;            /* obj_polygonal_nv_faces */
    size_t               numFaces;               /* obj_polygonal_v_faces */
    size_t               numTexFaces;            /* obj_polygonal_tv_faces */ 
    size_t               numTexNorFaces;         /* obj_polygonal_tnv_faces */ 
    const char * const  *str_arr_obj_groups;     /* obj_groups */
    const char * const  *str_arr_obj_objects;    /* obj_objects */
    const char * const  *str_arr_obj_materials;  /* obj_materials */
    const char * const  *str_arr_obj_texmaps;    /* obj_texmaps */
    const float        (*vert_list)[4];          /* obj_vertices */
    const float        (*norm_list)[3];          /* obj_normals */
    const float        (*texture_coord_list)[3]; /* obj_texture_coord */
    const size_t        *attindex_arr_nv_faces;  /* obj_polygonal_nv_faces */
    const size_t        *attindex_arr_v_faces;   /* obj_polygonal_v_faces */
    const size_t        *attindex_arr_tv_faces;  /* obj_polygonal_tv_faces */
    const size_t        *attindex_arr_tnv_faces; /* obj_polygonal_tnv_faces */
};

    /* global definition */
    size_t *tmp_ptr = NULL ;
    size_t (*triangle_indexes)[3][2] = NULL ;

void collect_global_obj_file_attributes(struct ga_t *ga) {
    size_t i = 0;

    ga->numPolyAttr = obj_polygonal_attributes(ga->contents, &ga->polyattr_list);

    ga->numGroups = obj_groups(ga->contents, &ga->str_arr_obj_groups);
    bu_log("total number of groups in OBJ file; numGroups = (%lu)\n", ga->numGroups);

    if (verbose) {
        bu_log("list of all groups i.e. 'g' in OBJ file\n");
        for (i = 0 ; i < ga->numGroups ; i++)
            bu_log("(%lu)(%s)\n", i, ga->str_arr_obj_groups[i]);
    }

    ga->numObjects = obj_objects(ga->contents, &ga->str_arr_obj_objects);
    bu_log("total number of object groups in OBJ file; numObjects = (%lu)\n", ga->numObjects);

    if (verbose) {
        bu_log("list of all object groups i.e. 'o' in OBJ file\n");
        for (i = 0 ; i < ga->numObjects ; i++)
            bu_log("(%lu)(%s)\n", i, ga->str_arr_obj_objects[i]);
    }

    ga->numMaterials = obj_materials(ga->contents, &ga->str_arr_obj_materials);
    bu_log("total number of material names in OBJ file; numMaterials = (%lu)\n", ga->numMaterials);

    if (verbose) {
        bu_log("list of all material names i.e. 'usemtl' in OBJ file\n");
        for (i = 0 ; i < ga->numMaterials ; i++)
            bu_log("(%lu)(%s)\n", i, ga->str_arr_obj_materials[i]);
    }

    ga->numTexmaps = obj_texmaps(ga->contents, &ga->str_arr_obj_texmaps);
    bu_log("total number of texture map names in OBJ file; numTexmaps = (%lu)\n", ga->numTexmaps);

    if (verbose) {
        bu_log("list of all texture map names i.e. 'usemap' in OBJ file\n");
        for (i = 0 ; i < ga->numTexmaps ; i++)
            bu_log("(%lu)(%s)\n", i, ga->str_arr_obj_texmaps[i]);
    }

    ga->numVerts = obj_vertices(ga->contents, &ga->vert_list);
    bu_log("numVerts = (%lu)\n", ga->numVerts);

    ga->numNorms = obj_normals(ga->contents, &ga->norm_list);
    bu_log("numNorms = (%lu)\n", ga->numNorms);

    ga->numTexCoords = obj_texture_coord(ga->contents, &ga->texture_coord_list);
    bu_log("numTexCoords = (%lu)\n", ga->numTexCoords);

    ga->numNorFaces = obj_polygonal_nv_faces(ga->contents, &ga->attindex_arr_nv_faces);
    bu_log("number of oriented polygonal faces; numNorFaces = (%lu)\n", ga->numNorFaces);

    ga->numFaces = obj_polygonal_v_faces(ga->contents, &ga->attindex_arr_v_faces);
    bu_log("number of polygonal faces only identifed by vertices; numFaces = (%lu)\n", ga->numFaces);

    ga->numTexFaces = obj_polygonal_tv_faces(ga->contents, &ga->attindex_arr_tv_faces);
    bu_log("number of textured polygonal faces; numTexFaces = (%lu)\n", ga->numTexFaces);

    ga->numTexNorFaces = obj_polygonal_tnv_faces(ga->contents, &ga->attindex_arr_tnv_faces);
    bu_log("number of oriented textured polygonal faces; numTexNorFaces = (%lu)\n", ga->numTexNorFaces);

    return;
}

void cleanup_name(struct bu_vls *outputObjectName_ptr) {
    char *temp_str;
    int outputObjectName_length;
    int i;

    temp_str = bu_vls_addr(outputObjectName_ptr);

    /* length does not include null */
    outputObjectName_length = bu_vls_strlen(outputObjectName_ptr);

    for ( i = 0 ; i < outputObjectName_length ; i++ )
        if (strchr("/\\=",(int)temp_str[i]) != (char *)NULL )
            temp_str[i] = '_';

    return;
}

/* compare function for bsearch for triangle indexes */
/* compare function for qsort for function 'remove_duplicates' */
static int comp_b(const void *p1, const void *p2) {
   size_t i = * (size_t *) p1;
   size_t j = * (size_t *) p2;

   return (int)(i - j);
}

/* compare function for qsort for triangle indexes */
static int comp(const void *p1, const void *p2) {
   size_t i = * (size_t *) p1;
   size_t j = * (size_t *) p2;

   return (int)(tmp_ptr[i] - tmp_ptr[j]);
}

/* compare function for qsort for edges */
static int comp_c(const void *p1, const void *p2) {
   edge_arr_2D_t i = (edge_arr_2D_t) p1;
   edge_arr_2D_t j = (edge_arr_2D_t) p2;

   if ((i[0][0] - j[0][0]) != 0)
       return (int)(i[0][0] - j[0][0]);
   else
       return (int)(i[0][1] - j[0][1]);
}

/* retrieves the coordinates and obj file indexes for the 
   vertex, normal and texture vertex for the current grouping
   and indicated by the face index number of grouping and
   vertex index number of face into the grouping array.
   note: if the face_type indicates a type which some of 
   this information is not applicable, the not applicable
   information will not be set. */             /* inputs: */
void retrieve_coord_index(struct   ga_t *ga,   /* obj file global attributes */
                          struct   gfi_t *gfi, /* grouping face indices */
                          size_t   fi,         /* face index number of grouping */
                          size_t   vi,         /* vertex index number of face */
                                               /* outputs: */
                          fastf_t *vc,         /* vertex coordinates */
                          fastf_t *nc,         /* normal coordinates */
                          fastf_t *tc,         /* texture vertex coordinates */
                          fastf_t *w,          /* vertex weight */
                          size_t  *vofi,       /* vertex obj file index */
                          size_t  *nofi,       /* normal obj file index */
                          size_t  *tofi) {     /* texture vertex obj file index */

    const size_t (*index_arr_v_faces) = NULL ;      /* used by v_faces */
    const size_t (*index_arr_tv_faces)[2] = NULL ;  /* used by tv_faces */
    const size_t (*index_arr_nv_faces)[2] = NULL ;  /* used by nv_faces */
    const size_t (*index_arr_tnv_faces)[3] = NULL ; /* used by tnv_faces */

    arr_1D_t index_arr_faces_1D = NULL; 
    arr_2D_t index_arr_faces_2D = NULL; 
    arr_3D_t index_arr_faces_3D = NULL; 

    size_t  fofi = gfi->obj_file_face_idx_arr[fi]; /* face obj file index */

    switch (gfi->face_type) {
        case FACE_V :
            index_arr_faces_1D = (arr_1D_t)(gfi->index_arr_faces);

            /* copy current vertex obj file index into vofi */ 
            *vofi = index_arr_faces_1D[fi][vi] ;

            /* used fused vertex index if available */
            if (gfi->vertex_fuse_map != NULL)
                *vofi = gfi->vertex_fuse_map[*vofi - gfi->vertex_fuse_offset];

            /* copy current vertex coordinates into vc */
            VMOVE(vc, ga->vert_list[*vofi]);
            /* copy current vertex weight into w */
            *w = ga->vert_list[*vofi][3] ;

            if (verbose)
                bu_log("fi=(%lu)vi=(%lu)fofi=(%lu)vofi=(%lu)v=(%f)(%f)(%f)w=(%f)\n", 
                   fi, vi, fofi+1, *vofi+1, vc[0], vc[1], vc[2], *w);
            break;
        case FACE_TV :
            index_arr_faces_2D = (arr_2D_t)(gfi->index_arr_faces);
            /* copy current vertex coordinates into vc */
            VMOVE(vc, ga->vert_list[index_arr_faces_2D[fi][vi][0]]);
            /* copy current vertex weight into w */
            *w = ga->vert_list[index_arr_faces_2D[fi][vi][0]][3] ;
            /* copy current texture coordinate into tc */
            VMOVE(tc, ga->texture_coord_list[index_arr_faces_2D[fi][vi][1]]);
            /* copy current vertex obj file index into vofi */ 
            *vofi = index_arr_faces_2D[fi][vi][0];
            /* copy current texture coordinate obj file index into tofi */ 
            *tofi = index_arr_faces_2D[fi][vi][1];
            if (verbose)
                bu_log("fi=(%lu)vi=(%lu)fofi=(%lu)vofi=(%lu)tofi=(%lu)v=(%f)(%f)(%f)w=(%f)t=(%f)(%f)(%f)\n", 
                   fi, vi, fofi+1, *vofi+1, *tofi+1, vc[0], vc[1], vc[2], *w, tc[0], tc[1], tc[2]);
            break;
        case FACE_NV :
            index_arr_faces_2D = (arr_2D_t)(gfi->index_arr_faces);

            /* copy current vertex obj file index into vofi */ 
            *vofi = index_arr_faces_2D[fi][vi][0];
            /* copy current normal obj file index into nofi */ 
            *nofi = index_arr_faces_2D[fi][vi][1];

            /* use fused vertex index if available */
            if (gfi->vertex_fuse_map != NULL)
                *vofi = gfi->vertex_fuse_map[*vofi - gfi->vertex_fuse_offset];

            /* copy current vertex coordinates into vc */
            VMOVE(vc, ga->vert_list[*vofi]);
            /* copy current vertex weight into w */
            *w = ga->vert_list[*vofi][3] ;
            /* copy current normal into nc */
            VMOVE(nc, ga->norm_list[*nofi]);

            if (verbose)
                bu_log("fi=(%lu)vi=(%lu)fofi=(%lu)vofi=(%lu)nofi=(%lu)v=(%f)(%f)(%f)w=(%f)n=(%f)(%f)(%f)\n", 
                   fi, vi, fofi+1, *vofi+1, *nofi+1, vc[0], vc[1], vc[2], *w, nc[0], nc[1], nc[2]);
            break;
        case FACE_TNV :
            index_arr_faces_3D = (arr_3D_t)(gfi->index_arr_faces);
            /* copy current vertex coordinates into vc */
            VMOVE(vc, ga->vert_list[index_arr_faces_3D[fi][vi][0]]);
            /* copy current vertex weight into w */
            *w = ga->vert_list[index_arr_faces_3D[fi][vi][0]][3] ;
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
            if (verbose)
                bu_log("fi=(%lu)vi=(%lu)fofi=(%lu)vofi=(%lu)tofi=(%lu)nofi=(%lu)v=(%f)(%f)(%f)w=(%f)t=(%f)(%f)(%f)n=(%f)(%f)(%f)\n", 
                   fi, vi, fofi+1, *vofi+1, *tofi+1, *nofi+1, vc[0], vc[1], vc[2], *w, 
                   tc[0], tc[1], tc[2], nc[0], nc[1], nc[2]);
            break;
        default:
            bu_log("ERROR: logic error, invalid face_type in function 'retrieve_coord_index'\n");
            return;
    }
    return;
}

/* forces new population of face_status array */
size_t retest_grouping_faces(struct ga_t *ga,
                        struct gfi_t *gfi,
                        fastf_t conv_factor,  /* conversion factor from obj file units to mm */
                        struct bn_tol *tol) {

    size_t face_idx = 0;
    size_t failed_face_count = 0;

    /* face_status is populated within test_face function */
    for ( face_idx = 0 ; face_idx < gfi->num_faces ; face_idx++ )
        /* 1 passed into test_face indicates forced retest */
        if ( test_face(ga, gfi, face_idx, conv_factor, tol, 1) )
            failed_face_count++;

    return failed_face_count;
}


/* return values 
   0 valid face
   1 degenerate, <3 vertices
   2 degenerate, duplicate vertex indexes
   3 degenerate, vertices too close */
int test_face(struct ga_t *ga,
               struct gfi_t *gfi,
               size_t face_idx,
               fastf_t conv_factor,  /* conversion factor from obj file units to mm */
               struct bn_tol *tol,
               int force_retest) {   /* force the retest of this face if it was already
                                        tested, this is useful when we know a retest is
                                        required such as when the previous test was performed
                                        without fused vertices and now vertices are fused */

    fastf_t tmp_v_o[3] = { 0.0, 0.0, 0.0 }; /* temporary vertex, referenced from outer loop */
    fastf_t tmp_v_i[3] = { 0.0, 0.0, 0.0 }; /* temporary vertex, referenced from inner loop */
    fastf_t tmp_w = 0.0 ; /* temporary weight */
    fastf_t tmp_n[3] = { 0.0, 0.0, 0.0 }; /* temporary normal */
    fastf_t tmp_t[3] = { 0.0, 0.0, 0.0 }; /* temporary texture vertex */
    size_t  nofi = 0; /* normal obj file index */
    size_t  tofi = 0; /* texture vertex obj file index */
    fastf_t distance_between_vertices = 0.0 ;
    size_t  vofi_o = 0; /* vertex obj file index, referenced from outer loop */
    size_t  vofi_i = 0; /* vertex obj file index, referenced from inner loop */

    size_t vert = 0;
    size_t vert2 = 0;
    int degenerate_face = 0;

    /* if this face has already been tested, return the previous
       results unless the force_retest flag is set */
    if (!force_retest) {
        if ( gfi->face_status[face_idx] ) {
            /* subtract 1 since face_status value of 1 indicates
               success and 0 indicates untested */
            return (gfi->face_status[face_idx] - 1);
        }
    }

    /* added 1 to internal index values so warning message index values
       matches obj file index numbers. this is because obj file indexes
       start at 1, internally indexes start at 0.  changed the warning
       message if grouping_type is GRP_NONE because the group name and
       grouping index has no meaning to the user if grouping type is
       GRP_NONE */

    if ( gfi->num_vertices_arr[face_idx] < 3 ) {
        degenerate_face = 1;
        if ( gfi->grouping_type != GRP_NONE ) 
            bu_log("WARNING: removed degenerate face (reason: < 3 vertices); obj file face group name = (%s) obj file face grouping index = (%lu) obj file face index = (%lu)\n",
               bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index,
               gfi->obj_file_face_idx_arr[face_idx] + 1);
        else
            bu_log("WARNING: removed degenerate face (reason: < 3 vertices); obj file face index = (%lu)\n",
               gfi->obj_file_face_idx_arr[face_idx] + 1);
    }

    while ( (vert < gfi->num_vertices_arr[face_idx]) && !degenerate_face ) {
        vert2 = vert+1;
        while ( (vert2 < gfi->num_vertices_arr[face_idx]) && !degenerate_face ) {
            retrieve_coord_index(ga, gfi, face_idx, vert, tmp_v_o,
                                 tmp_n, tmp_t, &tmp_w, &vofi_o, &nofi, &tofi); 
            retrieve_coord_index(ga, gfi, face_idx, vert2, tmp_v_i,
                                 tmp_n, tmp_t, &tmp_w, &vofi_i, &nofi, &tofi); 
            if ( vofi_o == vofi_i ) {
                /* test for duplicate vertex indexes in face */
                degenerate_face = 2;
                if ( gfi->grouping_type != GRP_NONE )
                    bu_log("WARNING: removed degenerate face (reason: duplicate vertex index); obj file face group name = (%s) obj file face grouping index = (%lu) obj file face index = (%lu) obj file vertex index = (%lu)\n",
                       bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index,
                       gfi->obj_file_face_idx_arr[face_idx] + 1, vofi_o + 1);
                else
                    bu_log("WARNING: removed degenerate face (reason: duplicate vertex index); obj file face index = (%lu) obj file vertex index = (%lu)\n",
                       gfi->obj_file_face_idx_arr[face_idx] + 1, vofi_o + 1);
            } else {
                /* test for vertices closer than tol.dist */
                /* tol.dist is assumed to be mm */
                VSCALE(tmp_v_o, tmp_v_o, conv_factor);
                VSCALE(tmp_v_i, tmp_v_i, conv_factor);
                if ( bn_pt3_pt3_equal(tmp_v_o, tmp_v_i, tol) ) {
                    distance_between_vertices = DIST_PT_PT(tmp_v_o, tmp_v_i) ;
                    degenerate_face = 3;
                    if ( gfi->grouping_type != GRP_NONE )
                        bu_log("WARNING: removed degenerate face (reason: vertices too close); obj file face group name = (%s) obj file face grouping index = (%lu) obj file face index = (%lu) obj file vertice indexes (%lu) vs (%lu) tol.dist = (%lfmm) dist = (%fmm)\n",
                           bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index,
                           gfi->obj_file_face_idx_arr[face_idx] + 1, vofi_o + 1,
                           vofi_i + 1, tol->dist, distance_between_vertices);
                    else
                        bu_log("WARNING: removed degenerate face (reason: vertices too close); obj file face index = (%lu) obj file vertice indexes (%lu) vs (%lu) tol.dist = (%lfmm) dist = (%fmm)\n",
                           gfi->obj_file_face_idx_arr[face_idx] + 1, vofi_o + 1, vofi_i + 1,
                           tol->dist, distance_between_vertices);
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

void free_gfi(struct gfi_t **gfi) {
    if (*gfi != NULL) {
        bu_vls_free((*gfi)->raw_grouping_name);
        bu_free((*gfi)->raw_grouping_name, "(*gfi)->raw_grouping_name");
        bu_free((*gfi)->index_arr_faces, "(*gfi)->index_arr_faces");
        bu_free((*gfi)->num_vertices_arr, "(*gfi)->num_vertices_arr");
        bu_free((*gfi)->obj_file_face_idx_arr, "(*gfi)->obj_file_face_idx_arr");
        bu_free((*gfi)->face_status, "(*gfi)->face_status");
        if ((*gfi)->vertex_fuse_map != NULL)
            bu_free((*gfi)->vertex_fuse_map,"(*gfi)->vertex_fuse_map");
        if ((*gfi)->texture_vertex_fuse_map != NULL)
            bu_free((*gfi)->texture_vertex_fuse_map,"(*gfi)->texture_vertex_fuse_map");
        bu_free(*gfi, "*gfi");
        *gfi = NULL;
    }
    return;
}

/* this function allocates all memory needed for the
   gfi structure and its contents. gfi is expected to
   be a null pointer when passed to this function. the
   gfi structure and its contents is expected to be
   freed outside this function.
 */
void collect_grouping_faces_indexes(struct ga_t *ga,
                                    struct gfi_t **gfi,
                                    int face_type,
                                    int grouping_type,
                                    size_t grouping_index) { /* grouping_index is ignored if grouping_type
                                                                is set to GRP_NONE */

    size_t numFaces = 0; /* number of faces of the current face_type in the entire obj file */
    size_t i = 0;
    const size_t *attindex_arr_faces = (const size_t *)NULL;
    int found = 0;
    const char *name_str = (char *)NULL;
    size_t setsize = 0 ;
    const size_t *indexset_arr;
    size_t groupid = 0;
    size_t *num_vertices_arr_tmp = NULL ;
    size_t *obj_file_face_idx_arr_tmp = NULL ;

    const size_t (*index_arr_v_faces) = NULL ;      /* used by v_faces */
    const size_t (*index_arr_tv_faces)[2] = NULL ;  /* used by tv_faces */
    const size_t (*index_arr_nv_faces)[2] = NULL ;  /* used by nv_faces */
    const size_t (*index_arr_tnv_faces)[3] = NULL ; /* used by tnv_faces */

    arr_1D_t index_arr_faces_1D = NULL; 
    arr_2D_t index_arr_faces_2D = NULL; 
    arr_3D_t index_arr_faces_3D = NULL; 

    /* number of faces of the current face_type from the entire obj file
       which is found in the current grouping_type and current group */
    size_t numFacesFound = 0;

    /* number of additional elements to allocate memory
       for when the currently allocated memory is exhausted */ 
    const size_t max_faces_increment = 128;

    if ( *gfi != NULL ) {
        bu_log("ERROR: function collect_grouping_faces_indexes passed non-null for gfi\n");
        return;
    }

    switch (face_type) {
        case FACE_V :
            numFaces = ga->numFaces; 
            attindex_arr_faces = ga->attindex_arr_v_faces;
            break;
        case FACE_TV :
            numFaces = ga->numTexFaces; 
            attindex_arr_faces = ga->attindex_arr_tv_faces;
            break;
        case FACE_NV :
            numFaces = ga->numNorFaces;
            attindex_arr_faces = ga->attindex_arr_nv_faces;
            break;
        case FACE_TNV :
            numFaces = ga->numTexNorFaces;
            attindex_arr_faces = ga->attindex_arr_tnv_faces;
            break;
        default:
            bu_log("ERROR: logic error, invalid face_type in function 'collect_grouping_faces_indexes'\n");
            return;
    }

    /* traverse list of all faces in OBJ file of current face_type */
    for (i = 0 ; i < numFaces ; i++) {

        const obj_polygonal_attributes_t *face_attr;
        face_attr = ga->polyattr_list + attindex_arr_faces[i];

        /* reset for next face */
        found = 0;

        /* for each type of grouping, check if current face is in current grouping */
        switch (grouping_type) {
            case GRP_NONE :
                found = 1;
                /* since there is no grouping, still need a somewhat useful name 
                   for the brlcad primitive and region, set the name to the face_type
                   which is the inherent grouping */
                switch (face_type) {
                    case FACE_V :
                        name_str = "v" ;
                        break;
                    case FACE_TV :
                        name_str = "tv" ;
                        break;
                    case FACE_NV :
                        name_str = "nv" ;
                        break;
                    case FACE_TNV :
                        name_str = "tnv" ;
                        break;
                    default:
                        bu_log("ERROR: logic error, invalid face_type in function 'collect_grouping_faces_indexes'\n");
                        return;
                }
                break;
            case GRP_GROUP :
                /* setsize is the number of groups the current nv_face belongs to */
                setsize = obj_groupset(ga->contents,face_attr->groupset_index,&indexset_arr);

                /* loop through each group this face is in */
                for (groupid = 0 ; groupid < setsize ; groupid++) {
                    /* if true, current face is in current group grouping */
                    if ( grouping_index == indexset_arr[groupid] ) {
                        found = 1;
                        name_str = ga->str_arr_obj_groups[indexset_arr[groupid]];
                    }
                }
                break;
            case GRP_OBJECT :
                /* if true, current face is in current object grouping */
                if ( grouping_index == face_attr->object_index ) {
                    found = 1;
                    name_str = ga->str_arr_obj_objects[face_attr->object_index];
                }
                break;
            case GRP_MATERIAL :
                /* if true, current face is in current material grouping */
                if ( grouping_index == face_attr->material_index ) {
                    found = 1;
                    name_str = ga->str_arr_obj_materials[face_attr->material_index];
                }
                break;
            case GRP_TEXTURE :
                /* if true, current face is in current texture map grouping */
                if ( grouping_index == face_attr->texmap_index ) {
                    found = 1;
                    name_str = ga->str_arr_obj_texmaps[face_attr->texmap_index];
                }
                break;
            default:
                bu_log("ERROR: logic error, invalid grouping_type in function 'collect_grouping_faces_indexes'\n");
                return;
        }

        /* if found the first face allocate the output structure and initial allocation
           of the index_arr_faces, num_vertices_arr and obj_file_face_idx_arr arrays */
        if ( found && (numFacesFound == 0)) {
            /* allocate memory for gfi structure */
            *gfi = (struct gfi_t *)bu_calloc(1, sizeof(struct gfi_t), "gfi");

            /* initialize gfi structure */
            memset((void *)*gfi, 0, sizeof(struct gfi_t)); /* probably redundant */
            (*gfi)->index_arr_faces = (void *)NULL;
            (*gfi)->num_vertices_arr = (size_t *)NULL;
            (*gfi)->obj_file_face_idx_arr = (size_t *)NULL;
            (*gfi)->raw_grouping_name = (struct  bu_vls *)NULL;
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

            /* set face_type, grouping_type, grouping_index inside gfi structure,
               the purpose of this is so functions called later do not need to pass
               this in seperately */
            (*gfi)->face_type = face_type;
            (*gfi)->grouping_type = grouping_type;
            if ( grouping_type != GRP_NONE ) {
                (*gfi)->grouping_index = grouping_index;
            } else {
                /* set grouping_index to face_type since if grouping_type is
                   GRP_NONE, inherently we are grouping by face_type and we
                   need a number for unique naming of the brlcad objects. */
                (*gfi)->grouping_index = (size_t)abs(face_type);

            }
            /* allocate and initialize variable length string (vls) for raw_grouping_name */
            (*gfi)->raw_grouping_name = (struct bu_vls *)bu_calloc(1, sizeof(struct bu_vls), "raw_grouping_name");
            bu_vls_init((*gfi)->raw_grouping_name);

            /* only need to copy in the grouping name for the first face found within the
               grouping since all the faces in the grouping will have the same grouping name */
            bu_vls_strcpy((*gfi)->raw_grouping_name, name_str);

            /* sets initial number of elements to allocate memory for */
            (*gfi)->max_faces = max_faces_increment;

            (*gfi)->num_vertices_arr = (size_t *)bu_calloc((*gfi)->max_faces, 
                                                           sizeof(size_t), "num_vertices_arr");

            (*gfi)->obj_file_face_idx_arr = (size_t *)bu_calloc((*gfi)->max_faces, 
                                                           sizeof(size_t), "obj_file_face_idx_arr");

            /* allocate initial memory for (*gfi)->index_arr_faces based on face_type */
            switch (face_type) {
                case FACE_V :
                    (*gfi)->index_arr_faces = (void *)bu_calloc((*gfi)->max_faces, 
                                                                sizeof(arr_1D_t), "index_arr_faces");
                    index_arr_faces_1D = (arr_1D_t)((*gfi)->index_arr_faces);
                    break;
                case FACE_TV :
                case FACE_NV :
                    (*gfi)->index_arr_faces = (void *)bu_calloc((*gfi)->max_faces, 
                                                                sizeof(arr_2D_t), "index_arr_faces");
                    index_arr_faces_2D = (arr_2D_t)((*gfi)->index_arr_faces);
                    break;
                case FACE_TNV :
                    (*gfi)->index_arr_faces = (void *)bu_calloc((*gfi)->max_faces, 
                                                                sizeof(arr_3D_t), "index_arr_faces");
                    index_arr_faces_3D = (arr_3D_t)((*gfi)->index_arr_faces);
                    break;
                default:
                    bu_log("ERROR: logic error, invalid face_type in function 'collect_grouping_faces_indexes'\n");
                    return;
            }
        }

        if ( found ) {
            /* assign obj file face index into array for tracking errors back
               to the face within the obj file */
            (*gfi)->obj_file_face_idx_arr[numFacesFound] = attindex_arr_faces[i];

            switch (face_type) {
                case FACE_V :
                    (*gfi)->num_vertices_arr[numFacesFound] = \
                            obj_polygonal_v_face_vertices(ga->contents,i,&index_arr_v_faces);
                    index_arr_faces_1D[numFacesFound] = index_arr_v_faces;
                    break;
                case FACE_TV :
                    (*gfi)->num_vertices_arr[numFacesFound] = \
                            obj_polygonal_tv_face_vertices(ga->contents,i,&index_arr_tv_faces);
                    index_arr_faces_2D[numFacesFound] = index_arr_tv_faces;
                    break;
                case FACE_NV :
                    (*gfi)->num_vertices_arr[numFacesFound] = \
                            obj_polygonal_nv_face_vertices(ga->contents,i,&index_arr_nv_faces);
                    index_arr_faces_2D[numFacesFound] = index_arr_nv_faces;
                    break;
                case FACE_TNV :
                    (*gfi)->num_vertices_arr[numFacesFound] = \
                            obj_polygonal_tnv_face_vertices(ga->contents,i,&index_arr_tnv_faces);
                    index_arr_faces_3D[numFacesFound] = index_arr_tnv_faces;
                    break;
                default:
                    bu_log("ERROR: logic error, invalid face_type in function 'collect_grouping_faces_indexes'\n");
                    return;
            }
            (*gfi)->tot_vertices += (*gfi)->num_vertices_arr[numFacesFound];

            /* if needed, increase size of (*gfi)->num_vertices_arr and (*gfi)->index_arr_faces */
            if ( numFacesFound >= (*gfi)->max_faces ) {
                (*gfi)->max_faces += max_faces_increment;

                num_vertices_arr_tmp = (size_t *)bu_realloc((*gfi)->num_vertices_arr,
                                       sizeof(size_t) * (*gfi)->max_faces, "num_vertices_arr_tmp");
                (*gfi)->num_vertices_arr = num_vertices_arr_tmp;

                obj_file_face_idx_arr_tmp = (size_t *)bu_realloc((*gfi)->obj_file_face_idx_arr,
                                       sizeof(size_t) * (*gfi)->max_faces, "obj_file_face_idx_arr_tmp");
                (*gfi)->obj_file_face_idx_arr = obj_file_face_idx_arr_tmp;

                switch (face_type) {
                    case FACE_V :
                        (*gfi)->index_arr_faces = (void *)bu_realloc(index_arr_faces_1D,
                                                  sizeof(arr_1D_t) * (*gfi)->max_faces, "index_arr_faces");
                        index_arr_faces_1D = (arr_1D_t)((*gfi)->index_arr_faces);
                        break;
                    case FACE_TV :
                    case FACE_NV :
                        (*gfi)->index_arr_faces = (void *)bu_realloc(index_arr_faces_2D,
                                                  sizeof(arr_2D_t) * (*gfi)->max_faces, "index_arr_faces");
                        index_arr_faces_2D = (arr_2D_t)((*gfi)->index_arr_faces);
                        break;
                    case FACE_TNV :
                        (*gfi)->index_arr_faces = (void *)bu_realloc(index_arr_faces_3D,
                                                  sizeof(arr_3D_t) * (*gfi)->max_faces, "index_arr_faces");
                        index_arr_faces_3D = (arr_3D_t)((*gfi)->index_arr_faces);
                        break;
                    default:
                        bu_log("ERROR: logic error, invalid face_type in function 'collect_grouping_faces_indexes'\n");
                        return;
                }
            }

            numFacesFound++; /* increment this at the end since arrays start at zero */
        }

    }  /* numFaces loop, when loop exits, all faces have been reviewed */

    if ( numFacesFound ) { 
        (*gfi)->num_faces = numFacesFound ;

        (*gfi)->face_status = (short int *)bu_calloc((*gfi)->num_faces, sizeof(short int), "face_status");

        /* initialize array */
        for ( i = 0 ; i < (*gfi)->num_faces ; i++ )
            (*gfi)->face_status[i] = 0;

    } else {
        *gfi = NULL ; /* this should already be null if no faces were found */
    }

    return;
}

void populate_triangle_indexes(struct ga_t *ga,
                               struct gfi_t *gfi,
                               struct ti_t *ti,
                               size_t face_idx) {

    /* it is assumed that when this function is called,
       the number of face vertices is >= 3 */

    size_t vert_idx = 0; /* index into vertices within for-loop */
    size_t idx = 0; /* sub index */
    size_t num_new_tri = 0; /* number of new triangles to create */
    fastf_t tmp_v[3] = { 0.0, 0.0, 0.0 }; /* temporary vertex */
    fastf_t tmp_n[3] = { 0.0, 0.0, 0.0 }; /* temporary normal */
    fastf_t tmp_rn[3] = { 0.0, 0.0, 0.0 }; /* temporary reverse normal */
    fastf_t tmp_t[3] = { 0.0, 0.0, 0.0 }; /* temporary texture vertex */
    fastf_t tmp_w = 0.0 ; /* temporary weight */
    size_t  vofi = 0; /* vertex obj file index */
    size_t  nofi = 0; /* normal obj file index */
    size_t  tofi = 0; /* texture vertex obj file index */
    size_t  svofi = 0; /* start vertex obj file index */
    size_t  snofi = 0; /* start normal obj file index */
    size_t  stofi = 0; /* start texture vertex obj file index */
    size_t max_tri_increment = 128;

    tri_arr_1D_t index_arr_tri_1D = NULL;
    tri_arr_2D_t index_arr_tri_2D = NULL;
    tri_arr_3D_t index_arr_tri_3D = NULL;

    if ( ti->index_arr_tri == (void *)NULL ) {
        /* assign triangle type to the same type as the face */
        ti->tri_type = gfi->face_type;

        /* the initial size will be the number of faces in the grouping */
        ti->max_tri = gfi->num_faces;

        /* allocate memory for initial 'ti->index_arr_tri' array */
        switch (gfi->face_type) {
            case FACE_V :
                /* 3 vertice indexes */
                ti->index_arr_tri = (void *)bu_calloc(ti->max_tri * 3, sizeof(size_t), "triangle_indexes");
                break;
            case FACE_TV :
                /* 3 vertice indexes + 3 texture vertice indexes */
            case FACE_NV :
                /* 3 vertice indexes + 3 normal indexes */
                ti->index_arr_tri = (void *)bu_calloc(ti->max_tri * 6, sizeof(size_t), "triangle_indexes");
                break;
            case FACE_TNV :
                /* 3 vertice indexes + 3 normal indexes + 3 texture vertice indexes */
                ti->index_arr_tri = (void *)bu_calloc(ti->max_tri * 9, sizeof(size_t), "triangle_indexes");
                break;
            default:
                bu_log("ERROR: logic error, invalid face_type in function 'populate_triangle_indexes'\n");
                break;
        }
    }

    switch (gfi->face_type) {
        case FACE_V :
            index_arr_tri_1D = (tri_arr_1D_t)ti->index_arr_tri;
            break;
        case FACE_TV :
        case FACE_NV :
            index_arr_tri_2D = (tri_arr_2D_t)ti->index_arr_tri;
            break;
        case FACE_TNV :
            index_arr_tri_3D = (tri_arr_3D_t)ti->index_arr_tri;
            break;
        default:
            break;
    }

    /* compute number of new triangles to create */
    if ( gfi->num_vertices_arr[face_idx] > 3 )
        num_new_tri = gfi->num_vertices_arr[face_idx] - 2;
    else
        num_new_tri = 1;

    /* find start indexes */
    retrieve_coord_index(ga, gfi, face_idx, 0, tmp_v, tmp_n, tmp_t, &tmp_w, &svofi, &snofi, &stofi); 

    /* populate triangle indexes array, if necessary, triangulate face */
    for ( vert_idx = 0 ; vert_idx < num_new_tri ; vert_idx++ ) {
        for ( idx = 0 ; idx < 3 ; idx++ ) {
            if ( !idx ) { /* use the same vertex for the start of each triangle */ 
                vofi = svofi;
                nofi = snofi;
                tofi = stofi;
            } else {
                retrieve_coord_index(ga, gfi, face_idx, vert_idx + idx, tmp_v, tmp_n, 
                                     tmp_t, &tmp_w, &vofi, &nofi, &tofi); 
            }
            switch (gfi->face_type) {
                case FACE_V :
                    index_arr_tri_1D[ti->num_tri][idx] = vofi;
                    break;
                case FACE_TV :
                    index_arr_tri_2D[ti->num_tri][idx][0] = vofi;
                    index_arr_tri_2D[ti->num_tri][idx][1] = tofi;
                    break;
                case FACE_NV :
                    index_arr_tri_2D[ti->num_tri][idx][0] = vofi;
                    index_arr_tri_2D[ti->num_tri][idx][1] = nofi;
                    break;
                case FACE_TNV :
                    index_arr_tri_3D[ti->num_tri][idx][0] = vofi;
                    index_arr_tri_3D[ti->num_tri][idx][1] = tofi;
                    index_arr_tri_3D[ti->num_tri][idx][2] = nofi;
                    break;
                default:
                    break;
            }
        }
        ti->num_tri++; /* increment this at the end since arrays start at zero */

        /* if needed, increase size of 'ti->index_arr_tri' array */
        if ( ti->num_tri >= ti->max_tri ) {
            ti->max_tri += max_tri_increment;
            switch (gfi->face_type) {
                case FACE_V :
                    ti->index_arr_tri = (void *)bu_realloc(index_arr_tri_1D,
                                         sizeof(size_t) * ti->max_tri * 3, "index_arr_tri");
                    index_arr_tri_1D = (tri_arr_1D_t)(ti->index_arr_tri);
                    break;
                case FACE_TV :
                case FACE_NV :
                    ti->index_arr_tri = (void *)bu_realloc(index_arr_tri_2D,
                                         sizeof(size_t) * ti->max_tri * 6, "index_arr_tri");
                    index_arr_tri_2D = (tri_arr_2D_t)(ti->index_arr_tri);
                    break;
                case FACE_TNV :
                    ti->index_arr_tri = (void *)bu_realloc(index_arr_tri_3D,
                                         sizeof(size_t) * ti->max_tri * 9, "index_arr_tri");
                    index_arr_tri_3D = (tri_arr_3D_t)(ti->index_arr_tri);
                    break;
                default:
                    break;
            }
        }
    }
}

void populate_sort_indexes(struct ti_t *ti) {

    size_t idx = 0;
    size_t num_indexes = ti->num_tri * 3;

    switch (ti->tri_type) {
        case FACE_V :
            /* vsi ... vertex sort index */
            ti->vsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vsi");
            for ( idx = 0 ; idx < num_indexes ; idx++ )
                ti->vsi[idx] = idx;
            break;
        case FACE_TV :
            /* vsi ... vertex sort index */
            ti->vsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vsi");
            /* tvsi ... texture vertex sort index */
            ti->tvsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->tvsi");
            for ( idx = 0 ; idx < num_indexes ; idx++ ) {
                ti->vsi[idx] = idx * 2;
                ti->tvsi[idx] = (idx * 2) + 1;
            }
            break;
        case FACE_NV :
            /* vsi ... vertex sort index */
            ti->vsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vsi");
            /* vnsi ... vertex normal sort index */
            ti->vnsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vnsi");
            for ( idx = 0 ; idx < num_indexes ; idx++ ) {
                ti->vsi[idx] = idx * 2;
                ti->vnsi[idx] = (idx * 2) + 1;
            }
            break;
        case FACE_TNV :
            /* vsi ... vertex sort index */
            ti->vsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vsi");
            /* tvsi ... texture vertex sort index */
            ti->tvsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->tvsi");
            /* vnsi ... vertex normal sort index */
            ti->vnsi = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "ti->vnsi");
            for ( idx = 0 ; idx < num_indexes ; idx++ ) {
                ti->vsi[idx] = idx * 3;
                ti->tvsi[idx] = (idx * 3) + 1;
                ti->vnsi[idx] = (idx * 3) + 2;
            }
            break;
        default:
            break;
    }
    return;
}

void sort_indexes(struct ti_t *ti) {
    size_t num_indexes = ti->num_tri * 3;

    /* tmp_ptr is global which is required for qsort */
    tmp_ptr = (size_t *)ti->index_arr_tri;

    /* process vertex indexes */
    qsort(ti->vsi, num_indexes, sizeof ti->vsi[0],
         (int (*)(const void *a, const void *b))comp);

    /* process vertex normal indexes */
    if ( ti->tri_type == FACE_NV || ti->tri_type == FACE_TNV )
        qsort(ti->vnsi, num_indexes, sizeof ti->vnsi[0],
             (int (*)(const void *a, const void *b))comp);

    /* process texture vertex indexes */
    if ( ti->tri_type == FACE_TV || ti->tri_type == FACE_TNV )
        qsort(ti->tvsi, num_indexes, sizeof ti->tvsi[0],
             (int (*)(const void *a, const void *b))comp);

    return;
}

void create_unique_indexes(struct ti_t *ti) {
    size_t last = 0;
    size_t idx = 0;
    size_t counter = 0;
    size_t num_indexes = ti->num_tri * 3;
    size_t *tmp_arr = (size_t *)ti->index_arr_tri;

    /* process vertex indexes */
    /* count sorted and unique libobj vertex indexes */
    last = tmp_arr[ti->vsi[0]];
    ti->num_uvi = 1;
    for (idx = 1; idx < num_indexes ; ++idx)
        if (tmp_arr[ti->vsi[idx]] != last) {
            last = tmp_arr[ti->vsi[idx]];
            ti->num_uvi++;
        }

    /* allocate unique triangle vertex index array */
    ti->uvi = (size_t *)bu_calloc(ti->num_uvi, sizeof(size_t), "ti->uvi");

    /* store unique triangle vertex index array */
    counter = 0;
    last = tmp_arr[ti->vsi[0]];
    ti->uvi[counter] = last ;
    for (idx = 1; idx < num_indexes ; ++idx)
        if (tmp_arr[ti->vsi[idx]] != last) {
            last = tmp_arr[ti->vsi[idx]];
            counter++;
            ti->uvi[counter] = last ;
        }

    /* free 'triangle vertex sort index array' (ti->vsi) since
       it is no longer needed since 'unique triangle vertex index
       array' has been created. */
    bu_free(ti->vsi, "ti->vsi");
    /* end of process vertex indexes */

    /* process vertex normal indexes */
    if ( ti->tri_type == FACE_NV || ti->tri_type == FACE_TNV ) {
        /* count sorted and unique libobj vertex normal indexes */
        last = tmp_arr[ti->vnsi[0]];
        ti->num_uvni = 1;
        for (idx = 1; idx < num_indexes ; ++idx)
            if (tmp_arr[ti->vnsi[idx]] != last) {
                last = tmp_arr[ti->vnsi[idx]];
                ti->num_uvni++;
            }

        /* allocate unique triangle vertex normal index array */
        ti->uvni = (size_t *)bu_calloc(ti->num_uvni, sizeof(size_t), "ti->uvni");

        /* store unique triangle vertex normal index array */
        counter = 0;
        last = tmp_arr[ti->vnsi[0]];
        ti->uvni[counter] = last ;
        for (idx = 1; idx < num_indexes ; ++idx)
            if (tmp_arr[ti->vnsi[idx]] != last) {
                last = tmp_arr[ti->vnsi[idx]];
                counter++;
                ti->uvni[counter] = last ;
            }

        /* free 'triangle vertex normal sort index array' (ti->vnsi) since
           it is no longer needed since 'unique triangle vertex normal index
           array' has been created. */
        bu_free(ti->vnsi, "ti->vnsi");
    } /* end of process vertex normal indexes */

    /* process texture vertex indexes */
    if ( ti->tri_type == FACE_TV || ti->tri_type == FACE_TNV ) {
        /* count sorted and unique libobj texture vertex indexes */
        last = tmp_arr[ti->tvsi[0]];
        ti->num_utvi = 1;
        for (idx = 1; idx < num_indexes ; ++idx)
            if (tmp_arr[ti->tvsi[idx]] != last) {
                last = tmp_arr[ti->tvsi[idx]];
                ti->num_utvi++;
            }

        /* allocate unique triangle texture vertex index array */
        ti->utvi = (size_t *)bu_calloc(ti->num_utvi, sizeof(size_t), "ti->utvi");

        /* store unique triangle texture vertex index array */
        counter = 0;
        last = tmp_arr[ti->tvsi[0]];
        ti->utvi[counter] = last ;
        for (idx = 1; idx < num_indexes ; ++idx)
            if (tmp_arr[ti->tvsi[idx]] != last) {
                last = tmp_arr[ti->tvsi[idx]];
                counter++;
                ti->utvi[counter] = last ;
            }

        /* free 'triangle texture vertex sort index array' (ti->tvsi) since
           it is no longer needed since 'unique triangle texture vertex index
           array' has been created. */
        bu_free(ti->tvsi, "ti->tvsi");
    } /* end of process texture vertex indexes */
    return;
}

void free_ti(struct ti_t *ti) {
    if ( ti == (struct ti_t *)NULL ) {
        bu_log("function free_ti was passed a null pointer\n");
        return;
    }

    /* do not free sort indexes here since they have already
       been freed in the 'create_unique_indexes' function */

    /* do not free unique indexes here since they have already
       been freed in the 'create_bot_int_arrays' function */

    /* free 'triangle indices into vertex, normal, texture vertex lists' */
    bu_free(ti->index_arr_tri, "ti->index_arr_tri");

    /* free 'array of floats for bot vertices' */
    bu_free(ti->bot_vertices, "ti->bot_vertices");

    /* free 'array of floats for face thickness' */
    bu_free(ti->bot_thickness, "ti->bot_thickness");

    /* free 'array of floats for bot normals' */
    if ( ti->tri_type == FACE_NV || ti->tri_type == FACE_TNV )
        bu_free(ti->bot_normals, "ti->bot_normals");

    /* free 'array of floats for texture vertices' */
    if ( ti->tri_type == FACE_TV || ti->tri_type == FACE_TNV )
        bu_free(ti->bot_texture_vertices, "ti->bot_texture_vertices");

    return;
}

void create_bot_float_arrays(struct ga_t *ga,
                       struct ti_t *ti,
                       fastf_t conv_factor) {
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
    for ( i = 0 ; i < ti->bot_num_vertices ; i++ )
        for ( j = 0 ; j < 3 ; j++ )
            ti->bot_vertices[(i*3)+j] = ga->vert_list[ti->uvi[i]][j] * conv_factor;

    /* allocate bot_thickness array */
    ti->bot_thickness = (fastf_t *)bu_calloc(ti->bot_num_faces, sizeof(fastf_t), "ti->bot_thickness");

    /* populate bot_thickness array */
    for ( i = 0 ; i < ti->bot_num_faces ; i++ )
        ti->bot_thickness[i] = 1.0;

    /* normals */
    if ( ti->tri_type == FACE_NV || ti->tri_type == FACE_TNV ) {
        /* allocate bot_normals array */
        ti->bot_normals = (fastf_t *)bu_calloc(ti->bot_num_normals * 3, sizeof(fastf_t), "ti->bot_normals");

        /* populate bot_normals array */
        for ( i = 0 ; i < ti->bot_num_normals ; i++ ) {
            for ( j = 0 ; j < 3 ; j++ )
                tmp_norm[j] = ga->norm_list[ti->uvni[i]][j];
 
            if ( MAGNITUDE(tmp_norm) < VDIVIDE_TOL ) {
                bu_log("WARNING: unable to unitize normal (%f)(%f)(%f)\n", tmp_norm[0], tmp_norm[1], tmp_norm[2]);
                VMOVE(&(ti->bot_normals[i*3]),tmp_norm);
            } else {
                VUNITIZE(tmp_norm);
                VMOVE(&(ti->bot_normals[i*3]),tmp_norm);
            }
        }
    }

    /* textures */
    if ( ti->tri_type == FACE_TV || ti->tri_type == FACE_TNV ) {
        /* allocate bot_texture_vertices array */
        ti->bot_texture_vertices = (fastf_t *)bu_calloc(ti->bot_num_texture_vertices * 3,
                                              sizeof(fastf_t), "ti->bot_texture_vertices");

        /* populate bot_texture_vertices array */
        for ( i = 0 ; i < ti->bot_num_texture_vertices ; i++ ) 
            for ( j = 0 ; j < 3 ; j++ )
                ti->bot_texture_vertices[(i*3)+j] = ga->texture_coord_list[ti->utvi[i]][j] * conv_factor;
    }
    return;
}

/* returns 1 if bsearch failure, otherwise return 0 for success */
int create_bot_int_arrays(struct ti_t *ti) {
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
    if ( ti->tri_type == FACE_NV || ti->tri_type == FACE_TNV )
        ti->bot_face_normals = (int *)bu_calloc(ti->bot_num_faces * 3, sizeof(int), "ti->bot_face_normals");

    /* allocate memory for 'array indices into bot_texture_vertices array' */
    if ( ti->tri_type == FACE_TV || ti->tri_type == FACE_TNV )
        ti->bot_textures = (int *)bu_calloc(ti->bot_num_faces * 3, sizeof(int), "ti->bot_textures");

    if ( ti->tri_type == FACE_V ) {
        index_arr_tri_1D = (tri_arr_1D_t)ti->index_arr_tri;
        for ( i = 0 ; i < ti->bot_num_faces ; i++ )
            for ( j = 0 ; j < 3 ; j++ )
                if ((res_v = bsearch(&(index_arr_tri_1D[i][j]),ti->uvi, ti->num_uvi, sizeof(size_t),
                                    (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
                    bu_log("ERROR: FACE_V bsearch returned null, face=(%lu)idx=(%lu)\n", i, j);
                    return 1;
                } else 
                    ti->bot_faces[(i*3)+j] = (int)(res_v - ti->uvi);
        bu_free(ti->uvi, "ti->uvi");
    }

    if ( ti->tri_type == FACE_TV ) {
        index_arr_tri_2D = (tri_arr_2D_t)ti->index_arr_tri;
        for ( i = 0 ; i < ti->bot_num_faces ; i++ )
            for ( j = 0 ; j < 3 ; j++ ) {
                if ((res_v = bsearch(&(index_arr_tri_2D[i][j][0]),ti->uvi, ti->num_uvi, sizeof(size_t),
                                    (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
                    bu_log("ERROR: FACE_TV bsearch returned vertex null, face=(%lu)idx=(%lu)\n", i, j);
                    return 1;
                } else 
                    ti->bot_faces[(i*3)+j] = (int)(res_v - ti->uvi);
                if ((res_t = bsearch(&(index_arr_tri_2D[i][j][1]),ti->utvi, ti->num_utvi, sizeof(size_t),
                                    (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
                    bu_log("ERROR: FACE_TV bsearch returned texture null, face=(%lu)idx=(%lu)\n", i, j);
                    return 1;
                } else 
                    ti->bot_textures[(i*3)+j] = (int)(res_t - ti->utvi);
            }
        bu_free(ti->uvi, "ti->uvi");
        bu_free(ti->utvi, "ti->utvi");
    }

    if ( ti->tri_type == FACE_NV ) {
        index_arr_tri_2D = (tri_arr_2D_t)ti->index_arr_tri;
        for ( i = 0 ; i < ti->bot_num_faces ; i++ )
            for ( j = 0 ; j < 3 ; j++ ) {
                if ((res_v = bsearch(&(index_arr_tri_2D[i][j][0]),ti->uvi, ti->num_uvi, sizeof(size_t),
                                    (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
                    bu_log("ERROR: FACE_NV bsearch returned vertex null, face=(%lu)idx=(%lu)\n", i, j);
                    return 1;
                } else 
                    ti->bot_faces[(i*3)+j] = (int)(res_v - ti->uvi);
                if ((res_n = bsearch(&(index_arr_tri_2D[i][j][1]),ti->uvni, ti->num_uvni, sizeof(size_t),
                                    (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
                    bu_log("ERROR: FACE_NV bsearch returned normal null, face=(%lu)idx=(%lu)\n", i, j);
                    return 1;
                } else 
                    ti->bot_face_normals[(i*3)+j] = (int)(res_n - ti->uvni);
            }
        bu_free(ti->uvi, "ti->uvi");
        bu_free(ti->uvni, "ti->uvni");
    }

    if ( ti->tri_type == FACE_TNV ) {
        index_arr_tri_3D = (tri_arr_3D_t)ti->index_arr_tri;
        for ( i = 0 ; i < ti->bot_num_faces ; i++ )
            for ( j = 0 ; j < 3 ; j++ ) {
                if ((res_v = bsearch(&(index_arr_tri_3D[i][j][0]),ti->uvi, ti->num_uvi, sizeof(size_t),
                                    (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
                    bu_log("ERROR: FACE_TNV bsearch returned vertex null, face=(%lu)idx=(%lu)\n", i, j);
                    return 1;
                } else 
                    ti->bot_faces[(i*3)+j] = (int)(res_v - ti->uvi);
                if ((res_t = bsearch(&(index_arr_tri_3D[i][j][1]),ti->utvi, ti->num_utvi, sizeof(size_t),
                                    (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
                    bu_log("ERROR: FACE_TNV bsearch returned texture null, face=(%lu)idx=(%lu)\n", i, j);
                    return 1;
                } else 
                    ti->bot_textures[(i*3)+j] = (int)(res_t - ti->utvi);
                if ((res_n = bsearch(&(index_arr_tri_3D[i][j][2]),ti->uvni, ti->num_uvni, sizeof(size_t),
                                    (int (*)(const void *a, const void *b))comp_b)) == (size_t)NULL) {
                    bu_log("ERROR: FACE_TNV bsearch returned normal null, face=(%lu)idx=(%lu)\n", i, j);
                    return 1;
                } else 
                    ti->bot_face_normals[(i*3)+j] = (int)(res_n - ti->uvni);
            }
        bu_free(ti->uvi, "ti->uvi");
        bu_free(ti->utvi, "ti->utvi");
        bu_free(ti->uvni, "ti->uvni");
    }
    return 0; /* return success */
}

/* remove duplicates from list. count is updated to indicate the
   new size of list after duplicates are removed. the original
   input array is freed and the new unique array is returned in
   its place. the new returned array will be sorted */
remove_duplicates_and_sort(size_t **list, size_t *count) {
    size_t last = 0;
    size_t idx = 0;
    size_t idx2 = 0;
    size_t unique_count = 0;
    size_t *unique_arr = (size_t *)NULL;

    qsort(*list, *count, sizeof(size_t), (int (*)(const void *a, const void *b))comp_b);

    /* process list */
    /* count sorted and unique list elements */
    last = (*list)[0];
    unique_count = 1;
    for (idx = 1; idx < *count ; ++idx)
        if ((*list)[idx] != last) {
            last = (*list)[idx];
            unique_count++;
        }

    /* allocate unique array */
    unique_arr = (size_t *)bu_calloc(unique_count, sizeof(size_t), "unique_arr");

    /* store unique array */
    idx2 = 0;
    last = (*list)[0];
    unique_arr[idx2] = last;
    for (idx = 1; idx < *count ; ++idx)
        if ((*list)[idx] != last) {
            last = (*list)[idx];
            idx2++;
            unique_arr[idx2] = last;
        }

    bu_free(*list, "*list");
    *list = unique_arr;
    *count = unique_count;
}

size_t populate_fuse_map(struct ga_t *ga,
                         struct gfi_t *gfi,
                         fastf_t conv_factor, 
                         struct bn_tol *tol,
                         size_t *unique_index_list,
                         size_t num_unique_index_list,
                         int compare_type,  /* FUSE_WI_TOL or FUSE_EQUAL */
                         int vertex_type) { /* FUSE_VERT or FUSE_TEX_VERT */

    size_t     idx1 = 0;
    size_t     idx2 = 0;
    size_t     fuse_count = 0;
    fastf_t    tmp_v1[3];
    fastf_t    tmp_v2[3];
    fastf_t    distance_between_vertices = 0.0;
    size_t    *fuse_map = (size_t *)NULL;
    short int *fuse_flag = (short int *)NULL;
    size_t     fuse_offset = 0; 

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
        if ( fuse_flag[unique_index_list[idx1] - fuse_offset] == 0 ) {
            /* true if current vertex has not already been identified as a duplicate */
            idx2 = idx1 + 1;
            VMOVE(tmp_v1, ga->vert_list[unique_index_list[idx1]]);
            VSCALE(tmp_v1, tmp_v1, conv_factor);
            fuse_map[unique_index_list[idx1] - fuse_offset] = unique_index_list[idx1];
            while (idx2 < num_unique_index_list) {
                if ( fuse_flag[unique_index_list[idx2] - fuse_offset] == 0 ) {
                    VMOVE(tmp_v2, ga->vert_list[unique_index_list[idx2]]);
                    VSCALE(tmp_v2, tmp_v2, conv_factor);
                    if ( (compare_type == FUSE_EQUAL) ? VEQUAL(tmp_v1, tmp_v2) : bn_pt3_pt3_equal(tmp_v1, tmp_v2, tol) ) {
                        if (debug) {
                            distance_between_vertices = DIST_PT_PT(tmp_v1, tmp_v2);
                            bu_log("found equal i1=(%lu)vi1=(%lu)v1=(%f)(%f)(%f),i2=(%lu)vi2=(%lu)v2=(%f)(%f)(%f), dist = (%fmm)\n", 
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

    if (debug) 
        for (idx1 = 0 ; idx1 < num_unique_index_list ; idx1++ )
            bu_log("fused unique_index_list = (%lu)->(%lu)\n", unique_index_list[idx1],
                   fuse_map[unique_index_list[idx1] - fuse_offset]);

    return fuse_count;
}

/* returns number of fused vertices */
size_t fuse_vertex(struct ga_t *ga,
                struct gfi_t *gfi,
                fastf_t conv_factor, 
                struct bn_tol *tol,
                int vertex_type,   /* FUSE_VERT or FUSE_TEX_VERT */
                int compare_type) {  /* FUSE_WI_TOL or FUSE_EQUAL */

    size_t face_idx = 0; /* index into faces within for-loop */
    size_t vert_idx = 0; /* index into vertices within for-loop */
    fastf_t tmp_v[3] = { 0.0, 0.0, 0.0 }; /* temporary vertex */
    fastf_t tmp_n[3] = { 0.0, 0.0, 0.0 }; /* temporary normal */
    fastf_t tmp_t[3] = { 0.0, 0.0, 0.0 }; /* temporary texture vertex */
    fastf_t tmp_w = 0.0 ; /* temporary weight */
    size_t  vofi = 0;     /* vertex obj file index */
    size_t  nofi = 0;     /* normal obj file index */
    size_t  tofi = 0;     /* texture vertex obj file index */
    size_t  min_vert_idx = 0;
    size_t  max_vert_idx = 0;
    size_t  min_tex_vert_idx = 0;
    size_t  max_tex_vert_idx = 0;

    size_t  *vertex_index_list = (size_t *)NULL;
    size_t  *texture_vertex_index_list = (size_t *)NULL;
    size_t  *index_list_tmp = (size_t *)NULL;
    size_t  num_index_list = 0;
    size_t  max_index_list = 0;
    size_t  max_index_list_increment = 128;

    size_t  num_unique_vertex_index_list = 0;
    size_t  num_unique_texture_vertex_index_list = 0;

    size_t  idx1 = 0;
    size_t  idx2 = 0;
    size_t  idx3 = 0;

    size_t  fuse_count = 0;

    if ((gfi->face_type == FACE_V || gfi->face_type == FACE_NV) && (vertex_type == FUSE_TEX_VERT))
        return; /* nothing to process */

    /* test if fuse vertices has already been run, if so free old arrays */
    if ((vertex_type == FUSE_VERT) && (gfi->vertex_fuse_map != NULL)) {
        bu_free(gfi->vertex_fuse_map,"gfi->vertex_fuse_map");
        gfi->num_vertex_fuse = 0;
        gfi->vertex_fuse_offset = 0;
    }

    /* test if fuse texture vertices has already been run, if so free old arrays */
    if (((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) && (vertex_type == FUSE_TEX_VERT)) &&
         (gfi->texture_vertex_fuse_map != NULL)) {
        bu_free(gfi->texture_vertex_fuse_map,"gfi->texture_vertex_fuse_map");
        gfi->num_texture_vertex_fuse = 0;
        gfi->texture_vertex_fuse_offset = 0;
    }

    max_index_list += max_index_list_increment;
    /* initial allocation for vertex_index_list, and as applicable,
       the normal_index_list, texture_vertex_index_list */

    if (vertex_type == FUSE_VERT)
        vertex_index_list = (size_t *)bu_calloc(max_index_list, sizeof(size_t), "vertex_index_list");

    if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) && (vertex_type == FUSE_TEX_VERT))
        texture_vertex_index_list = (size_t *)bu_calloc(max_index_list, sizeof(size_t), "texture_vertex_index_list");

    /* loop thru all polygons and collect max and min index values and
       fill the vertex_index_list and as applicable, the texture_vertex_index_list */
    for ( face_idx = 0 ; face_idx < gfi->num_faces ; face_idx++ ) {
        for ( vert_idx = 0 ; vert_idx < gfi->num_vertices_arr[face_idx] ; vert_idx++ ) {
            retrieve_coord_index(ga, gfi, face_idx, vert_idx, tmp_v, tmp_n, tmp_t, &tmp_w, &vofi, &nofi, &tofi);
            if ( face_idx == 0 && vert_idx == 0 ) {
            /* set initial max and min */
                if (vertex_type == FUSE_VERT) {
                    vertex_index_list[num_index_list] = vofi;
                    min_vert_idx = vofi;
                    max_vert_idx = vofi;
                }
                if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) && (vertex_type == FUSE_TEX_VERT)) {
                    texture_vertex_index_list[num_index_list] = tofi;
                    min_tex_vert_idx = tofi;
                    max_tex_vert_idx = tofi;
                }
            } else {
            /* set max and min */
                if (vertex_type == FUSE_VERT) {
                    vertex_index_list[num_index_list] = vofi;
                    if (vofi > max_vert_idx)
                        max_vert_idx = vofi;
                    if (vofi < min_vert_idx)
                        min_vert_idx = vofi;
                }
                if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) && (vertex_type == FUSE_TEX_VERT)) {
                    texture_vertex_index_list[num_index_list] = tofi;
                    if (tofi > max_tex_vert_idx)
                        max_tex_vert_idx = tofi;
                    if (tofi < min_tex_vert_idx)
                        min_tex_vert_idx = tofi;
                }
            }
            num_index_list++;
            if ( num_index_list >= max_index_list ) {
                max_index_list += max_index_list_increment;
                if (vertex_type == FUSE_VERT) {
                    index_list_tmp = (size_t *)bu_realloc(vertex_index_list, sizeof(size_t) * max_index_list,
                                                         "index_list_tmp");
                    vertex_index_list = index_list_tmp;
                }
                if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) && (vertex_type == FUSE_TEX_VERT)) {
                    index_list_tmp = (size_t *)bu_realloc(texture_vertex_index_list, sizeof(size_t) * max_index_list,
                                                         "index_list_tmp");
                    texture_vertex_index_list = index_list_tmp;
                }
            }
        } /* end of vert_idx for-loop */
    } /* end of face_idx for-loop */

    /* compute size of fuse_map and fuse_flag arrays and 
       allocate these arrays. set fuse_offset values */
    if (vertex_type == FUSE_VERT) {
        gfi->num_vertex_fuse = max_vert_idx - min_vert_idx + 1;
        gfi->vertex_fuse_offset = min_vert_idx;
        gfi->vertex_fuse_map = (size_t *)bu_calloc(gfi->num_vertex_fuse, sizeof(size_t),
                                         "gfi->vertex_fuse_map");
        gfi->vertex_fuse_flag = (short int *)bu_calloc(gfi->num_vertex_fuse, sizeof(short int),
                                             "gfi->vertex_fuse_flag");
        /* initialize arrays */
        for (idx1 = 0 ; idx1 < gfi->num_vertex_fuse ; idx1++) {
            gfi->vertex_fuse_map[idx1] = 0;
            gfi->vertex_fuse_flag[idx1] = 0;
        }
    }

    if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) && (vertex_type == FUSE_TEX_VERT)) {
        gfi->num_texture_vertex_fuse = max_tex_vert_idx - min_tex_vert_idx + 1;
        gfi->texture_vertex_fuse_offset = min_tex_vert_idx;
        gfi->texture_vertex_fuse_map = (size_t *)bu_calloc(gfi->num_texture_vertex_fuse,
                                        sizeof(size_t), "gfi->texture_vertex_fuse_map");
        gfi->texture_vertex_fuse_flag = (short int *)bu_calloc(gfi->num_texture_vertex_fuse,
                                         sizeof(short int), "gfi->texture_vertex_fuse_flag");
        /* initialize arrays */
        for (idx1 = 0 ; idx1 < gfi->num_texture_vertex_fuse ; idx1++) {
            gfi->texture_vertex_fuse_map[idx1] = 0;
            gfi->texture_vertex_fuse_flag[idx1] = 0;
        }
    }

    /* initialize unique counts, these will be reduced
       as duplicates are removed from the lists */
    num_unique_vertex_index_list = num_index_list;
    num_unique_texture_vertex_index_list = num_index_list;

    if (debug) {
        for (idx1 = 0 ; idx1 < num_index_list ; idx1++ )
            bu_log("non-unique sorted vertex_index_list[idx1] = (%lu)\n", vertex_index_list[idx1]);
        bu_log("num non-unique sorted vertex_index_list[idx1] = (%lu)\n", num_index_list);
    }

    /* remove duplicates from lists and return a sorted list */
    /* pass in the number of elements in the non-unique array and 
       receive the number of element in the unique array, a new 
       list is created and passed back by this function. the new
       list will be returned sorted */
    if (vertex_type == FUSE_VERT)
        remove_duplicates_and_sort(&vertex_index_list, &num_unique_vertex_index_list);

    if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) && (vertex_type == FUSE_TEX_VERT))
        remove_duplicates_and_sort(&texture_vertex_index_list, &num_unique_texture_vertex_index_list);

    if (debug) {
        for (idx1 = 0 ; idx1 < num_unique_vertex_index_list ; idx1++ )
            bu_log("unique sorted vertex_index_list[idx1] = (%lu)\n", vertex_index_list[idx1]);
        bu_log("num unique sorted vertex_index_list[idx1] = (%lu)\n", num_unique_vertex_index_list);
    }

    if (vertex_type == FUSE_VERT) {
        fuse_count = populate_fuse_map(ga, gfi, conv_factor, tol, vertex_index_list,
                                       num_unique_vertex_index_list, compare_type, vertex_type);
        bu_free(vertex_index_list, "vertex_index_list");
        bu_free(gfi->vertex_fuse_flag,"gfi->vertex_fuse_flag");
    }

    if ((gfi->face_type == FACE_TV || gfi->face_type == FACE_TNV) && (vertex_type == FUSE_TEX_VERT)) {
        fuse_count = populate_fuse_map(ga, gfi, conv_factor, tol, texture_vertex_index_list, 
                                       num_unique_texture_vertex_index_list, compare_type, vertex_type);
        bu_free(texture_vertex_index_list, "texture_vertex_index_list");
        bu_free(gfi->texture_vertex_fuse_flag,"gfi->texture_vertex_fuse_flag");
    }

    bu_log("num fused vertex (%lu)\n", fuse_count);

    return fuse_count;
}

/* returns number of open edges, open edges >0 indicates closure failure */
size_t test_closure(struct ga_t *ga,
                 struct gfi_t *gfi,
                 fastf_t conv_factor, 
                 struct bn_tol *tol) {

    size_t face_idx = 0; /* index into faces within for-loop */
    size_t vert_idx = 0; /* index into vertices within for-loop */
    fastf_t tmp_v[3] = { 0.0, 0.0, 0.0 }; /* temporary vertex */
    fastf_t tmp_n[3] = { 0.0, 0.0, 0.0 }; /* temporary normal */
    fastf_t tmp_t[3] = { 0.0, 0.0, 0.0 }; /* temporary texture vertex */
    fastf_t tmp_w = 0.0 ; /* temporary weight */
    size_t  vofi0 = 0;    /* vertex obj file index */
    size_t  vofi1 = 0;    /* vertex obj file index */
    size_t  nofi = 0;     /* normal obj file index */
    size_t  tofi = 0;     /* texture vertex obj file index */
 
    size_t first_idx = 0;
    edge_arr_2D_t edges = (edge_arr_2D_t)NULL;
    edge_arr_2D_t edges_tmp = (edge_arr_2D_t)NULL;
    size_t previous_edge[2] = { 0, 0 };
    size_t max_edges = 0;
    size_t max_edges_increment = 128;
    size_t edge_count = 0;
    size_t idx = 0;
    size_t match = 0;
    size_t open_edges = 0;

    FILE *plotfp;
    vect_t pnt1;
    vect_t pnt2;
    int create_plot = 1;
    struct bu_vls plot_file_name;

    max_edges += max_edges_increment;
    edges = (edge_arr_2D_t)bu_calloc(max_edges * 2, sizeof(size_t), "edges");

    /* loop thru all polygons */
    for ( face_idx = 0 ; face_idx < gfi->num_faces ; face_idx++ ) {

        if (!test_face(ga, gfi, face_idx, conv_factor, tol, 0)) {
            for ( vert_idx = 0 ; vert_idx < gfi->num_vertices_arr[face_idx] ; vert_idx++ ) {

                retrieve_coord_index(ga, gfi, face_idx, vert_idx,
                                     tmp_v, tmp_n, tmp_t, &tmp_w, &vofi0, &nofi, &tofi);
                if ( vert_idx == 0 )
                    first_idx = vofi0;
                if ( (vert_idx + 1) < gfi->num_vertices_arr[face_idx] )
                    retrieve_coord_index(ga, gfi, face_idx, vert_idx+1,
                                         tmp_v, tmp_n, tmp_t, &tmp_w, &vofi1, &nofi, &tofi);
                else
                    vofi1 = first_idx;

                if (vofi0 <= vofi1) {
                    edges[edge_count][0] = vofi0;
                    edges[edge_count][1] = vofi1;
                } else {
                    edges[edge_count][0] = vofi1;
                    edges[edge_count][1] = vofi0;
                }

                if (debug)
                    bu_log("edges (%lu)(%lu)(%lu)(%lu)\n", face_idx, edge_count,
                            edges[edge_count][0], edges[edge_count][1]);
                edge_count++;

                if ( edge_count >= max_edges ) {
                    max_edges += max_edges_increment;
                    edges_tmp = (edge_arr_2D_t)bu_realloc(edges, sizeof(size_t) * max_edges * 2, "edges_tmp");
                    edges = edges_tmp;
                }
            } 
        }
    } /* ends when edges list is complete */

    qsort(edges, edge_count, sizeof(size_t) * 2,
         (int (*)(const void *a, const void *b))comp_c);

    if (1)
        for ( idx = 0 ; idx < edge_count ; idx++ )
            bu_log("sorted edges (%lu)(%lu)(%lu)\n", idx, edges[idx][0], edges[idx][1]);

    previous_edge[0] = edges[0][0];
    previous_edge[1] = edges[0][1];
    for ( idx = 1 ; idx < edge_count ; idx++ ) {
        if ( (previous_edge[0] == edges[idx][0]) && (previous_edge[1] == edges[idx][1]) ) {
            match++;
        } else {
            if ( match == 0 ) {
                if (1)
                    bu_log("open edge (%lu)= %f %f %f (%lu)= %f %f %f \n",
                           previous_edge[0],
                           ga->vert_list[previous_edge[0]][0] * conv_factor,
                           ga->vert_list[previous_edge[0]][1] * conv_factor,
                           ga->vert_list[previous_edge[0]][2] * conv_factor,
                           previous_edge[1],
                           ga->vert_list[previous_edge[1]][0] * conv_factor,
                           ga->vert_list[previous_edge[1]][1] * conv_factor,
                           ga->vert_list[previous_edge[1]][2] * conv_factor);

                if (create_plot && (open_edges == 0)) {
                    bu_vls_init(&plot_file_name);
                    bu_vls_sprintf(&plot_file_name, "%s.%lu.o.pl", 
                                   bu_vls_addr(gfi->raw_grouping_name), gfi->grouping_index);
                    cleanup_name(&plot_file_name);
                    if ((plotfp = fopen(bu_vls_addr(&plot_file_name), "wb")) == (FILE *)NULL) {
                        bu_log("unable to open plot file\n");
                        return;
                    }
                }
                if (create_plot) {
                    VMOVE(pnt1,ga->vert_list[previous_edge[0]]);
                    VMOVE(pnt2,ga->vert_list[previous_edge[1]]);
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

    bu_free(edges,"edges");

    if ( open_edges ) {
        gfi->closure_status = SURF_OPEN;
        bu_log("surface closure failed (%s), (%lu) open edges\n", bu_vls_addr(gfi->raw_grouping_name), open_edges);
    } else {
        gfi->closure_status = SURF_CLOSED;
        bu_log("surface closure success (%s)\n", bu_vls_addr(gfi->raw_grouping_name));
    }

    if (create_plot && (open_edges > 0)) {
        bu_vls_free(&plot_file_name);
        (void)fclose(plotfp);
    }

    return open_edges;
}

void output_to_bot(struct ga_t *ga,
                   struct gfi_t *gfi,
                   struct rt_wdb *outfp, 
                   fastf_t conv_factor, 
                   struct bn_tol *tol) {

    struct ti_t ti; /* triangle indices structure */

    size_t num_faces_killed = 0; /* number of degenerate faces killed in the current bot */
    size_t face_idx = 0; /* index into faces within for-loop */

    int ret_val = 0;  /* 0=success !0=fail */
    fastf_t *bot_vertices = NULL;
    int *bot_faces_array = NULL; /* bot face vertex index array, passed to mk_bot function */

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
    ti.bot_vertices = (fastf_t *)NULL;         /* array of float for bot vertices [bot_num_vertices*3] */
    ti.bot_thickness = (fastf_t *)NULL;        /* array of floats for face thickness [bot_num_faces] */
    ti.bot_normals = (fastf_t *)NULL;          /* array of floats for bot normals [bot_num_normals*3] */
    ti.bot_texture_vertices = (fastf_t *)NULL; /* array of floats for texture vertices [bot_num_texture_vertices*3] */
    ti.bot_faces = (int *)NULL;                /* array of indices into bot_vertices array [bot_num_faces*3] */
    ti.bot_face_normals = (int *)NULL;         /* array of indices into bot_normals array [bot_num_faces*3] */
    ti.bot_textures  = (int *)NULL;            /* array of indices into bot_texture_vertices array */
    ti.bot_num_vertices = 0;                   /* number of vertices in bot_vertices array */
    ti.bot_num_faces = 0;                      /* number of faces in bot_faces array */
    ti.bot_num_normals = 0;                    /* number of normals in bot_normals array */
    ti.bot_num_texture_vertices = 0;           /* number of textures in bot_texture_vertices array */

    /* loop thru all the polygons (i.e. faces) to be placed in the current bot */
    for ( face_idx = 0 ; face_idx < gfi->num_faces ; face_idx++ ) {
        if (verbose)
            bu_log("num vertices in current polygon = (%lu)\n", gfi->num_vertices_arr[face_idx]);

        /* test for degenerate face */
        if (test_face(ga, gfi, face_idx, conv_factor, tol, 0)) {
            num_faces_killed++;
        } else {

            /* populates triangle indexes for the current face */
            /* if necessary, allocates additional memory for the triangle indexes array */
            /* if necessary, triangulates the face */
            populate_triangle_indexes(ga, gfi, &ti, face_idx);
        }
    } /* loop exits when all polygons within the current grouping
         have been triangulated (if necessary) and the vertex, normal and
         texture vertices indexes have been placed in the array
         'ti.index_arr_tri'. */

    populate_sort_indexes(&ti);

    sort_indexes(&ti);

    create_unique_indexes(&ti);

    create_bot_float_arrays(ga, &ti, conv_factor);

    if ( create_bot_int_arrays(&ti) )
        bu_log("bsearch failure\n");

    switch (gfi->closure_status) {
        case SURF_UNTESTED :
            bu_vls_printf(gfi->raw_grouping_name, ".%lu.u.s", gfi->grouping_index);
            break;
        case SURF_CLOSED :
            bu_vls_printf(gfi->raw_grouping_name, ".%lu.c.s", gfi->grouping_index);
            break;
        case SURF_OPEN :
            bu_vls_printf(gfi->raw_grouping_name, ".%lu.o.s", gfi->grouping_index);
            break;
    }

    cleanup_name(gfi->raw_grouping_name);

    /* write bot to ".g" file */
    if ( ti.tri_type == FACE_NV || ti.tri_type == FACE_TNV ) {
        ret_val = mk_bot_w_normals(outfp, bu_vls_addr(gfi->raw_grouping_name), RT_BOT_SURFACE,
                         RT_BOT_UNORIENTED, RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS, 
                         ti.bot_num_vertices, ti.bot_num_faces, ti.bot_vertices,
                         ti.bot_faces, (fastf_t *)NULL, (struct bu_bitv *)NULL,
                         ti.bot_num_normals, ti.bot_normals, ti.bot_face_normals);
    } else {
        ret_val = mk_bot(outfp, bu_vls_addr(gfi->raw_grouping_name), RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0, 
                         ti.bot_num_vertices, ti.bot_num_faces, ti.bot_vertices,
                         ti.bot_faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);
    }

    free_ti(&ti);
    return;
}

void output_to_nmg(struct ga_t *ga,
                   struct gfi_t *gfi,
                   struct rt_wdb *outfp, 
                   fastf_t conv_factor, 
                   struct bn_tol *tol) {

    struct model *m = (struct model *)NULL;
    struct nmgregion *r = (struct nmgregion *)NULL;
    struct shell *s = (struct shell *)NULL;
    struct bu_ptbl faces;  /* table of faces for one element */
    struct faceuse *fu;
    struct vertexuse *vu = NULL;
    struct vertexuse *vu2 = NULL;
    struct loopuse *lu = NULL;
    struct edgeuse *eu = NULL;
    struct loopuse *lu2 = NULL;
    struct edgeuse *eu2 = NULL;
    size_t face_idx = 0; /* index into faces within for-loop */
    size_t vert_idx = 0; /* index into vertices within for-loop */
    size_t shell_vert_idx = 0; /* index into vertices for entire nmg shell */
    int total_fused_vertex = 0 ;
    plane_t pl; /* plane equation for face */
    fastf_t tmp_v[3] = { 0.0, 0.0, 0.0 }; /* temporary vertex */
    fastf_t tmp_w = 0.0 ; /* temporary weight */
    fastf_t tmp_rn[3] = { 0.0, 0.0, 0.0 }; /* temporary reverse normal */
    fastf_t tmp_n[3] = { 0.0, 0.0, 0.0 }; /* temporary normal */
    fastf_t tmp_t[3] = { 0.0, 0.0, 0.0 }; /* temporary texture vertex */
    vect_t  norm;
    fastf_t dot;
    size_t  vofi = 0; /* vertex obj file index */
    size_t  nofi = 0; /* normal obj file index */
    size_t  tofi = 0; /* texture vertex obj file index */

    struct vertex  **verts = NULL;

    size_t num_faces_killed = 0 ; /* number of degenerate faces killed in the current shell */

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

    shell_vert_idx = 0;
    NMG_CK_SHELL(s);
    /* loop thru all the polygons (i.e. faces) to be placed in the current shell/region/model */
    for ( face_idx = 0 ; face_idx < gfi->num_faces ; face_idx++ ) {
        if (verbose)
            bu_log("num vertices in current polygon = (%lu)\n", gfi->num_vertices_arr[face_idx]);

        /* test for degenerate face */
        if (test_face(ga, gfi, face_idx, conv_factor, tol, 0)) {
            num_faces_killed++;
        } else {
            fu = nmg_cface(s, (struct vertex **)&(verts[shell_vert_idx]),
                              (int)(gfi->num_vertices_arr[face_idx]));
            lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
            eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
            lu2 = BU_LIST_FIRST(loopuse, &fu->fumate_p->lu_hd);
            eu2 = BU_LIST_FIRST(edgeuse, &lu2->down_hd);

            for ( vert_idx = 0 ; vert_idx < gfi->num_vertices_arr[face_idx] ; vert_idx++ ) {
                retrieve_coord_index(ga, gfi, face_idx, vert_idx, tmp_v, tmp_n, tmp_t,
                                     &tmp_w, &vofi, &nofi, &tofi); 

                VSCALE(tmp_v, tmp_v, conv_factor);
                VUNITIZE(tmp_n);
                VREVERSE(tmp_rn, tmp_n);

                NMG_CK_VERTEX(eu->vu_p->v_p);
                nmg_vertex_gv(eu->vu_p->v_p, tmp_v);

                switch (gfi->face_type) {
                    case FACE_NV :
                    case FACE_TNV :
                        /* assign this normal to all uses of this vertex */
                        for (BU_LIST_FOR(vu, vertexuse, &eu->vu_p->v_p->vu_hd)) {
                            NMG_CK_VERTEXUSE(vu);
                            nmg_vertexuse_nv(vu, tmp_n);
                        }
                        for (BU_LIST_FOR(vu2, vertexuse, &eu2->vu_p->v_p->vu_hd)) {
                            NMG_CK_VERTEXUSE(vu2);
                            nmg_vertexuse_nv(vu2, tmp_rn);
                        }
                        break;
                    default:
                        break;
                }

                eu = BU_LIST_NEXT(edgeuse, &eu->l);
                eu2 = BU_LIST_NEXT(edgeuse, &eu2->l);
                shell_vert_idx++;

            } /* this loop exits when all the vertices and their normals
                 for the current polygon/faceuse has been inserted into
                 their appropriate structures */

            NMG_CK_FACEUSE(fu);
            if (nmg_loop_plane_area(BU_LIST_FIRST(loopuse, &fu->lu_hd), pl) < 0.0) {
                bu_log("Failed nmg_loop_plane_area\n");
                nmg_pr_fu_briefly( fu, "" );
                nmg_kfu(fu);
                fu = (struct faceuse *)NULL;
                num_faces_killed++;
            } else {
                nmg_face_g(fu, pl); /* return is void */
                nmg_face_bb(fu->f_p, tol); /* return is void */
                bu_ptbl_ins(&faces, (long *)fu);
            }

        } /* close of degenerate_face if test */

    } /* loop exits when all polygons within the current grouping
         has been placed within one nmg shell, inside one nmg region
         and inside one nmg model */

    bu_log("num_faces_killed = (%lu)\n", num_faces_killed);

    if (!BU_PTBL_END(&faces)){
        bu_log("Object %s has no faces\n", bu_vls_addr(gfi->raw_grouping_name));
        nmg_km(m);
    } else {
#if 0
        /* run nmg_rebound before nmg_model_vertex_fuse */
        bu_log("about to run nmg_model_vertex_fuse\n");
        total_fused_vertex = nmg_model_vertex_fuse(m, tol);
        bu_log("total_fused_vertex = (%d)\n", total_fused_vertex);
#endif

        bu_log("about to run nmg_model_fuse\n");
        bu_log("total entities fused = (%d)\n", nmg_model_fuse(m, tol));

        /* run nmg_model_vertex_fuse before nmg_gluefaces */
        bu_log("about to run nmg_gluefaces\n");
        nmg_gluefaces((struct faceuse **)BU_PTBL_BASEADDR(&faces), BU_PTBL_END(&faces), tol);

        /* Mark edges as real */
        bu_log("about to run nmg_mark_edges_real\n");
        (void)nmg_mark_edges_real(&s->l.magic);

        /* Compute "geometry" for region and shell */
        bu_log("about to run nmg_region_a\n");
        nmg_region_a(r, tol);

        bu_log("about to run nmg_kill_cracks\n");
        if (nmg_kill_cracks(s)) {
            bu_log("Object %s has no faces\n", bu_vls_addr(gfi->raw_grouping_name));
            nmg_km(m);
        } else {
#if 0
            /* Some faces may not be within tolerance, so triangulate faces where needed */
            /* run nmg_gluefaces before nmg_make_faces_within_tol */
            bu_log("about to run nmg_make_faces_within_tol\n");
            nmg_make_faces_within_tol(s, tol);
#endif

            bu_log("about to run nmg_rebound\n");
            nmg_rebound(m, tol);

            /* run nmg_model_vertex_fuse before nmg_check_closed_shell */
            if ( nmg_check_closed_shell(s, tol) ) {
                /* make bot for a open shell */
                bu_vls_printf(gfi->raw_grouping_name, ".%lu.bot.s", gfi->grouping_index);
                cleanup_name(gfi->raw_grouping_name);
                bu_log("about to mk_bot_from_nmg\n");
                mk_bot_from_nmg(outfp, bu_vls_addr(gfi->raw_grouping_name), s);
                nmg_km(m);
            } else {
                /* make nmg for a closed shell */
                bu_vls_printf(gfi->raw_grouping_name, ".%lu.nmg.s", gfi->grouping_index);
                cleanup_name(gfi->raw_grouping_name);
                bu_log("about to run mk_nmg\n");
                /* the model (m) is freed when mk_nmg completes */
                if (mk_nmg(outfp, bu_vls_addr(gfi->raw_grouping_name), m)) {
                    bu_log("mk_nmg failed\n");
                }
            }
        }
    } 

    bu_free(verts,"verts");
    bu_ptbl_reset(&faces);
}

/*
 * validate unit string and output conversion factor to millimeters.
 * if string is not a standard units identifier, the function assumes
 * a custom conversion factor was specified. a valid null terminated
 * string is expected as input. return of 0 is success, return of 1
 * is failure.
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

int
main(int argc, char **argv)
{
    char *prog = *argv, buf[BUFSIZ];
    static char *input_file_name;    /* input file name */
    static char *brlcad_file_name;   /* output file name */
    FILE *fd_in;	             /* input file */
    struct rt_wdb *fd_out;	     /* Resulting BRL-CAD file */
    struct region_s *region = NULL;
    int ret_val = 0;
    FILE *my_stream;
    struct ga_t ga;
    struct gfi_t *gfi = (struct gfi_t *)NULL; /* grouping face indices */
    size_t i = 0;
    int c;
    char option;
    char grouping_option = 'g'; /* to be selected by user from command line */
    char mode_option = 'b'; /* to be selected by user from command line */
    fastf_t conv_factor = 1.0; /* to be selected by user from command line */
    int weiss_result;
    const char *parse_messages = (char *)0;
    int parse_err = 0;
    struct bn_tol tol_struct ;
    struct bn_tol *tol ;
    int face_type_idx = 0;
    double dist_tmp = 0.0;

    /* the raytracer tolerance values (rtip->rti_tol) need to match
       these otherwise raytrace errors will result. the defaults
       for the rti_tol are set in the function rt_new_rti.
       either use here the rti_tol defaults or when raytracing
       change the raytracer values to these. */
    tol = &tol_struct;
    tol->magic = BN_TOL_MAGIC;
    tol->dist = 0.0005 ;   /* to be selected by user from command line */
    tol->dist_sq = tol->dist * tol->dist;
    tol->perp = 1e-6;
    tol->para = 1 - tol->perp;

#if 0
    rt_g.NMG_debug = 16777216; /* turn on nmg debug_tri */
#endif

    if (argc < 2)
        bu_exit(1, usage, argv[0]);

    while ((c = bu_getopt(argc, argv, "dx:vt:m:u:g:")) != EOF) {
        switch (c) {
            case 'd':
                debug = 1;
                break;
            case 'x':
                sscanf(bu_optarg, "%x", (unsigned int *) &rt_g.debug);
                bu_printb("librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT);
                bu_log("\n");
                break;
            case 'v':
                verbose++;
                break;
            case 't':
                dist_tmp = (double)atof(bu_optarg);
                if ( dist_tmp < tol->dist ) {
                    bu_log("distance tolerance too small, must be > %fmm\n", tol->dist);
                    bu_exit(EXIT_FAILURE,  usage, argv[0]);
                }
                tol->dist = dist_tmp;
                tol->dist_sq = tol->dist * tol->dist;
                break;
            case 'm': /* mode */
                switch (bu_optarg[0]) {
                    case 'b': 
                    case 'n':
                        mode_option = bu_optarg[0];
                        break;
                    default:
                        bu_log("Invalid mode option '%c'.\n", bu_optarg[0]);
                        bu_exit(EXIT_FAILURE,  usage, argv[0]);
                        break;
                }
                break;
            case 'u': /* units */
                if (str2mm(bu_optarg, &conv_factor))
                    bu_exit(EXIT_FAILURE,  usage, argv[0]);
                break;
            case 'g': /* grouping */
                switch (bu_optarg[0]) {
                    case 'g': 
                    case 'o':
                    case 'm':
                    case 't':
                    case 'n':
                        grouping_option = bu_optarg[0];
                        break;
                    default:
                        bu_log("Invalid grouping option '%c'.\n", bu_optarg[0]);
                        bu_exit(EXIT_FAILURE,  usage, argv[0]);
                        break;
                }
                break;
            default:
                bu_exit(1, usage, argv[0]);
                break;
        }
    }

    bu_log("using distance tolerance (%fmm)\n", tol->dist);
    bu_log("using grouping option (%c)\n", grouping_option);
    bu_log("using mode option (%c)\n", mode_option);
    bu_log("using conversion factor (%f)\n", conv_factor);

    /* initialize ga structure */
    memset((void *)&ga, 0, sizeof(struct ga_t));

    input_file_name = argv[bu_optind];
    if ((my_stream = fopen(input_file_name,"r")) == NULL) {
        bu_log("Cannot open input file (%s)\n", input_file_name);
        perror(prog);
        return EXIT_FAILURE;
    }

    bu_optind++;
    brlcad_file_name = argv[bu_optind];
    if ((fd_out = wdb_fopen(brlcad_file_name)) == NULL) {
        bu_log("Cannot create new BRL-CAD file (%s)\n", brlcad_file_name);
        perror(prog);
        bu_exit(1, NULL);
    }

    if ((ret_val = obj_parser_create(&ga.parser)) != 0) {
        if (ret_val == ENOMEM) {
            bu_log("Can not allocate an obj_parser_t object, Out of Memory.\n");
        } else {
            bu_log("Can not allocate an obj_parser_t object, Undefined Error (%d)\n", ret_val);
        }

        /* it is unclear if obj_parser_destroy must be run if obj_parser_create failed */
        obj_parser_destroy(ga.parser);

        if (fclose(my_stream) != 0) {
            bu_log("Unable to close file.\n");
        }

        perror(prog);
        return EXIT_FAILURE;
    }

    if (parse_err = obj_fparse(my_stream,ga.parser,&ga.contents)) {
        if ( parse_err < 0 ) {
            /* syntax error */
            parse_messages = obj_parse_error(ga.parser); 
            bu_log("obj_fparse, Syntax Error.\n");
            bu_log("%s\n", parse_messages); 
        } else {
            /* parser error */
            if (parse_err == ENOMEM) {
                bu_log("obj_fparse, Out of Memory.\n");
            } else {
                bu_log("obj_fparse, Other Error.\n");
            }
        }

        /* it is unclear if obj_contents_destroy must be run if obj_fparse failed */
        obj_contents_destroy(ga.contents);

        obj_parser_destroy(ga.parser);

        if (fclose(my_stream) != 0) {
            bu_log("Unable to close file.\n");
        }

        perror(prog);
        return EXIT_FAILURE;
    }

    collect_global_obj_file_attributes(&ga);

    switch (grouping_option) {
        case 'n':
            bu_log("ENTERED 'n' PROCESSING\n");
            for ( face_type_idx = FACE_V ; face_type_idx <= FACE_TNV ; face_type_idx++ ) {
                collect_grouping_faces_indexes(&ga, &gfi, face_type_idx, GRP_NONE, 0);
                if (gfi != NULL) {
                    bu_log("name=(%s) #faces=(%lu)\n", bu_vls_addr(gfi->raw_grouping_name),
                       gfi->num_faces);
                    fuse_vertex(&ga, gfi, conv_factor, tol, FUSE_VERT, FUSE_WI_TOL);
                    retest_grouping_faces(&ga, gfi, conv_factor, tol);
                    test_closure(&ga, gfi, conv_factor, tol);
                    switch (mode_option) {
                        case 'b':
                            output_to_bot(&ga, gfi, fd_out, conv_factor, tol);
                            break;
                        case 'n':
                            output_to_nmg(&ga, gfi, fd_out, conv_factor, tol);
                            break;
                        default:
                            break;
                    }
                    free_gfi(&gfi);
                }
            }
            bu_log("EXITED 'n' PROCESSING\n");
            break;
        case 'g':
            bu_log("ENTERED 'g' PROCESSING\n");
            for (i = 0 ; i < ga.numGroups ; i++) {
                for ( face_type_idx = FACE_V ; face_type_idx <= FACE_TNV ; face_type_idx++ ) {
                    collect_grouping_faces_indexes(&ga, &gfi, face_type_idx, GRP_GROUP, i);
                    if (gfi != NULL) {
                        bu_log("name=(%s) #faces=(%lu)\n", bu_vls_addr(gfi->raw_grouping_name),
                           gfi->num_faces);
                        fuse_vertex(&ga, gfi, conv_factor, tol, FUSE_VERT, FUSE_WI_TOL);
                        retest_grouping_faces(&ga, gfi, conv_factor, tol);
                        test_closure(&ga, gfi, conv_factor, tol);
                        switch (mode_option) {
                            case 'b':
                                output_to_bot(&ga, gfi, fd_out, conv_factor, tol);
                                break;
                            case 'n':
                                output_to_nmg(&ga, gfi, fd_out, conv_factor, tol);
                                break;
                            default:
                                break;
                        }
                        free_gfi(&gfi);
                    }
                }
            }
            bu_log("EXITED 'g' PROCESSING\n");
            break;
        case 'o':
            bu_log("ENTERED 'o' PROCESSING\n");
            for (i = 0 ; i < ga.numObjects ; i++) {
                for ( face_type_idx = FACE_V ; face_type_idx <= FACE_TNV ; face_type_idx++ ) {
                    collect_grouping_faces_indexes(&ga, &gfi, face_type_idx, GRP_OBJECT, i);
                    if (gfi != NULL) {
                        bu_log("name=(%s) #faces=(%lu)\n", bu_vls_addr(gfi->raw_grouping_name),
                           gfi->num_faces);
                        fuse_vertex(&ga, gfi, conv_factor, tol, FUSE_VERT, FUSE_WI_TOL);
                        retest_grouping_faces(&ga, gfi, conv_factor, tol);
                        test_closure(&ga, gfi, conv_factor, tol);
                        switch (mode_option) {
                            case 'b':
                                output_to_bot(&ga, gfi, fd_out, conv_factor, tol);
                                break;
                            case 'n':
                                output_to_nmg(&ga, gfi, fd_out, conv_factor, tol);
                                break;
                            default:
                                break;
                        }
                        free_gfi(&gfi);
                    }
                }
            }
            bu_log("EXITED 'o' PROCESSING\n");
            break;
        case 'm':
            bu_log("ENTERED 'm' PROCESSING\n");
            for (i = 0 ; i < ga.numMaterials ; i++) {
                for ( face_type_idx = FACE_V ; face_type_idx <= FACE_TNV ; face_type_idx++ ) {
                    collect_grouping_faces_indexes(&ga, &gfi, face_type_idx, GRP_MATERIAL, i);
                    if (gfi != NULL) {
                        bu_log("name=(%s) #faces=(%lu)\n", bu_vls_addr(gfi->raw_grouping_name),
                           gfi->num_faces);
                        fuse_vertex(&ga, gfi, conv_factor, tol, FUSE_VERT, FUSE_WI_TOL);
                        retest_grouping_faces(&ga, gfi, conv_factor, tol);
                        test_closure(&ga, gfi, conv_factor, tol);
                        switch (mode_option) {
                            case 'b':
                                output_to_bot(&ga, gfi, fd_out, conv_factor, tol);
                                break;
                            case 'n':
                                output_to_nmg(&ga, gfi, fd_out, conv_factor, tol);
                                break;
                            default:
                                break;
                        }
                        free_gfi(&gfi);
                    }
                }
            }
            bu_log("EXITED 'm' PROCESSING\n");
            break;
        case 't':
            bu_log("ENTERED 't' PROCESSING\n");
            for (i = 0 ; i < ga.numTexmaps ; i++) {
                for ( face_type_idx = FACE_V ; face_type_idx <= FACE_TNV ; face_type_idx++ ) {
                    collect_grouping_faces_indexes(&ga, &gfi, face_type_idx, GRP_TEXTURE, i);
                    if (gfi != NULL) {
                        bu_log("name=(%s) #faces=(%lu)\n", bu_vls_addr(gfi->raw_grouping_name),
                           gfi->num_faces);
                        fuse_vertex(&ga, gfi, conv_factor, tol, FUSE_VERT, FUSE_WI_TOL);
                        retest_grouping_faces(&ga, gfi, conv_factor, tol);
                        test_closure(&ga, gfi, conv_factor, tol);
                        switch (mode_option) {
                            case 'b':
                                output_to_bot(&ga, gfi, fd_out, conv_factor, tol);
                                break;
                            case 'n':
                                output_to_nmg(&ga, gfi, fd_out, conv_factor, tol);
                                break;
                            default:
                                break;
                        }
                        free_gfi(&gfi);
                    }
                }
            }
            bu_log("EXITED 't' PROCESSING\n");
            break;
        default:
            break;
    }

    /* running cleanup functions */
    bu_log("obj_contents_destroy\n");
    obj_contents_destroy(ga.contents);

    bu_log("obj_parser_destroy\n");
    obj_parser_destroy(ga.parser);

    bu_log("running fclose\n");
    if (fclose(my_stream) != 0) {
        bu_log("Unable to close file.\n");
        perror(prog);
        return EXIT_FAILURE;
    }

    wdb_close(fd_out);

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
