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

#define GRP_GROUP    1 /* create bot for each obj file 'g' grouping */
#define GRP_OBJECT   2 /* create bot for each obj file 'o' grouping */
#define GRP_MATERIAL 3 /* create bot for each obj file 'usemtl' grouping */
#define GRP_TEXTURE  4 /* create bot for eacg obj file 'usemap' grouping */

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

    bu_log("running obj_polygonal_attributes\n");
    ga->numPolyAttr = obj_polygonal_attributes(ga->contents, &ga->polyattr_list);

    bu_log("running obj_groups\n");
    ga->numGroups = obj_groups(ga->contents, &ga->str_arr_obj_groups);
    bu_log("total number of groups in OBJ file; numGroups = (%lu)\n", ga->numGroups);

    bu_log("list of all groups i.e. 'g' in OBJ file\n");
    for (i = 0 ; i < ga->numGroups ; i++) {
        bu_log("(%lu)(%s)\n", i, ga->str_arr_obj_groups[i]);
    }

    bu_log("running obj_objects\n");
    ga->numObjects = obj_objects(ga->contents, &ga->str_arr_obj_objects);
    bu_log("total number of object groups in OBJ file; numObjects = (%lu)\n", ga->numObjects);

    bu_log("list of all object groups i.e. 'o' in OBJ file\n");
    for (i = 0 ; i < ga->numObjects ; i++) {
        bu_log("(%lu)(%s)\n", i, ga->str_arr_obj_objects[i]);
    }

    bu_log("running obj_materials\n");
    ga->numMaterials = obj_materials(ga->contents, &ga->str_arr_obj_materials);
    bu_log("total number of material names in OBJ file; numMaterials = (%lu)\n", ga->numMaterials);

    bu_log("list of all material names i.e. 'usemtl' in OBJ file\n");
    for (i = 0 ; i < ga->numMaterials ; i++) {
        bu_log("(%lu)(%s)\n", i, ga->str_arr_obj_materials[i]);
    }

    bu_log("running obj_texmaps\n");
    ga->numTexmaps = obj_texmaps(ga->contents, &ga->str_arr_obj_texmaps);
    bu_log("total number of texture map names in OBJ file; numTexmaps = (%lu)\n", ga->numTexmaps);

    bu_log("list of all texture map names i.e. 'usemap' in OBJ file\n");
    for (i = 0 ; i < ga->numTexmaps ; i++) {
        bu_log("(%lu)(%s)\n", i, ga->str_arr_obj_texmaps[i]);
    }

    bu_log("running obj_vertices\n");
    ga->numVerts = obj_vertices(ga->contents, &ga->vert_list);
    bu_log("numVerts = (%lu)\n", ga->numVerts);

    bu_log("running obj_normals\n");
    ga->numNorms = obj_normals(ga->contents, &ga->norm_list);
    bu_log("numNorms = (%lu)\n", ga->numNorms);

    bu_log("running obj_texture_coord\n");
    ga->numTexCoords = obj_texture_coord(ga->contents, &ga->texture_coord_list);
    bu_log("numTexCoords = (%lu)\n", ga->numTexCoords);

    bu_log("running obj_polygonal_nv_faces\n");
    ga->numNorFaces = obj_polygonal_nv_faces(ga->contents, &ga->attindex_arr_nv_faces);
    bu_log("number of oriented polygonal faces; numNorFaces = (%lu)\n", ga->numNorFaces);

    bu_log("running obj_polygonal_v_faces\n");
    ga->numFaces = obj_polygonal_v_faces(ga->contents, &ga->attindex_arr_v_faces);
    bu_log("number of polygonal faces only identifed by vertices; numFaces = (%lu)\n", ga->numFaces);

    bu_log("running obj_polygonal_tv_faces\n");
    ga->numTexFaces = obj_polygonal_tv_faces(ga->contents, &ga->attindex_arr_tv_faces);
    bu_log("number of textured polygonal faces; numTexFaces = (%lu)\n", ga->numTexFaces);

    bu_log("running obj_polygonal_tnv_faces\n");
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

    for ( i = 0 ; i < outputObjectName_length ; i++ ) {
        if (temp_str[i] == '/') {
            temp_str[i] = '_';
        }
    }

    return;
}

/* compare function for bsearch for triangle indexes */
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

int process_nv_faces(struct ga_t *ga, 
                     struct rt_wdb *outfp, 
                     int grp_mode, 
                     size_t grp_indx,
                     fastf_t conversion_factor, 
                     struct model *m,
                     struct nmgregion *r,
                     struct shell *s,
                     struct bn_tol *tol,
                     int *empty) {

    size_t size = 0 ;
    size_t setsize = 0 ;
    size_t vert = 0;
    size_t vert2 = 0;
    const size_t (*index_arr_nv_faces)[2]; /* used by nv_faces */
    const size_t *indexset_arr;
    size_t groupid = 0;
    size_t numNorTriangles_in_current_bot = 0;
    /* NMG2s */
    const size_t (**index_arr_nv_faces_history)[2]; /* used by nv_faces */
    const size_t (**index_arr_nv_faces_history_tmp)[2]; /* used by nv_faces */
    size_t *size_history = NULL ; 
    size_t *size_history_tmp = NULL ;
    size_t history_arrays_size = 0; 
    size_t numNorPolygons_in_current_shell = 0;
    size_t numNorPolygonVertices_in_current_nmg = 0 ;
    struct bu_ptbl faces2;            /* table of faces for one element */
    struct bu_ptbl names2;            /* table of element names */
    struct faceuse *fu;
    size_t idx2 = 0;
    long   tmp_long = 0 ;
    /* NMG2e */
    int found = 0;
    size_t i = 0;
    int ret_val = 0;  /* 0=success !0=fail */

    fastf_t *vertices = NULL;
    fastf_t *vertices_tmp = NULL;
    size_t vertices_next_size = 0;
    int *faces = NULL;
    fastf_t *thickness = NULL;
    fastf_t *normals = NULL;
    int *face_normals = NULL;
    size_t j = 0;

    struct bu_vls outputObjectName;

    size_t numRealloc = 0;

    int skip_degenerate_faces = 1; /* boolean */
    int degenerate_face = 0; /* boolean */

    size_t bot_vertex_array_size = 0;
    size_t bot_normals_array_size = 0;

    const size_t num_triangles_per_alloc = 128 ;
    const size_t num_indexes_per_triangle = 6 ; /* 3 vert/tri, 1 norm/vert, 2 idx/vert */
    size_t triangle_indexes_size = 0;
    size_t (*triangle_indexes_tmp)[3][2] = NULL ;

    /* NMG2s */

    /* initial number of history elements to create, one is required
       for each polygon in the current nmg */
    history_arrays_size = 128 ; 

    /* allocate memory for initial index_arr_nv_faces_history array */
    index_arr_nv_faces_history = bu_calloc(history_arrays_size,
                                                             sizeof(size_t *) * 2,
                                                             "index_arr_nv_faces_history");

    /* allocate memory for initial size_history array */
    size_history = (size_t *)bu_calloc(history_arrays_size, sizeof(size_t), "size_history");
    /* NMG2e */

    /* compute memory required for initial triangle_indexes array */
    triangle_indexes_size = num_triangles_per_alloc * num_indexes_per_triangle ;

    /* allocate memory for initial triangle_indexes array */
    triangle_indexes = bu_calloc(triangle_indexes_size, sizeof(*triangle_indexes), "triangle_indexes");

    bu_vls_init(&outputObjectName);

    /* traverse list of all nv_faces in OBJ file */
    for (i = 0 ; i < ga->numNorFaces ; i++) {

        const obj_polygonal_attributes_t *face_attr;
        face_attr = ga->polyattr_list + ga->attindex_arr_nv_faces[i];

        /* reset for next face */
        found = 0;

        /* for each type of grouping, check if current face is in current group */
        switch (grp_mode) {
            case GRP_GROUP :
                /* setsize is the number of groups the current nv_face belongs to */
                setsize = obj_groupset(ga->contents,face_attr->groupset_index,&indexset_arr);

                /* loop through each group this face is in */
                for (groupid = 0 ; groupid < setsize ; groupid++) {
                    /* if true, current face is in current group */
                    if ( grp_indx == indexset_arr[groupid] ) {
                        found = 1;
                        bu_vls_sprintf(&outputObjectName, "%s.%lu.surface.s", 
                           ga->str_arr_obj_groups[indexset_arr[groupid]], indexset_arr[groupid] );
                        cleanup_name(&outputObjectName);
                    }
                }
                break;
            case GRP_OBJECT :
                /* if true, current face is in current object group */
                if ( grp_indx == face_attr->object_index ) {
                    found = 1;
                    bu_vls_sprintf(&outputObjectName, "%s.%lu.surface.s", 
                       ga->str_arr_obj_objects[face_attr->object_index], face_attr->object_index );
                    cleanup_name(&outputObjectName);
                }
                break;
            case GRP_MATERIAL :
                /* if true, current face is in current material group */
                if ( grp_indx == face_attr->material_index ) {
                    found = 1;
                    bu_vls_sprintf(&outputObjectName, "%s.%lu.surface.s", 
                       ga->str_arr_obj_materials[face_attr->material_index], face_attr->material_index );
                    cleanup_name(&outputObjectName);
                }
                break;
            case GRP_TEXTURE :
                break;
            default:
                bu_log("ERROR: logic error, invalid grp_mode in function 'process_nv_faces'\n");
                ret_val = 1;
                break;
        }

        /* only find number of vertices for current face if the current face */
        /* is in the current group */
        if ( found ) {
            size = obj_polygonal_nv_face_vertices(ga->contents,i,&index_arr_nv_faces);
        }

        /* test for and force the skip of degenerate faces */
        /* in this case degenerate faces are those with duplicate vertices */
        if ( found && skip_degenerate_faces ) {
            /* within the current face, compares vertice indices for duplicates */
            /* stops looking after found 1st duplicate */
            degenerate_face = 0;
            vert = 0;
            while ( (vert < size) && !degenerate_face ) {
                vert2 = vert+1;
                while ( (vert2 < size) && !degenerate_face ) {
                    if ( index_arr_nv_faces[vert][0] == index_arr_nv_faces[vert2][0] ) {
                        found = 0; /* i.e. unfind this face */
                        degenerate_face = 1;
                        /* add 1 to internal index value so warning message index value */
                        /* matches obj file index number. this is because obj file indexes */
                        /* start at 1, internally indexes start at 0. */
                        bu_log("WARNING: removed degenerate face; grp_indx (%lu) face (%lu) vert index (%lu) = (%lu)\n",
                           grp_indx,
                           i+1, 
                           (index_arr_nv_faces[vert][0])+1, 
                           (index_arr_nv_faces[vert2][0])+1); 
                    }
                    vert2++;
                }
                vert++;
            }
        }

        /* NMG2s */
        if ( found ) {

            index_arr_nv_faces_history[numNorPolygons_in_current_shell] = index_arr_nv_faces;
            size_history[numNorPolygons_in_current_shell] = size;

            numNorPolygonVertices_in_current_nmg = numNorPolygonVertices_in_current_nmg + size ;

                /* if needed, increase size of index_arr_nv_faces_history and size_history */
                if ( numNorPolygons_in_current_shell >= history_arrays_size ) {
                    history_arrays_size = history_arrays_size + 128 ;

                    index_arr_nv_faces_history_tmp = bu_realloc(index_arr_nv_faces_history,
                                                     sizeof(index_arr_nv_faces_history) * history_arrays_size,
                                                     "index_arr_nv_faces_history_tmp");

                    index_arr_nv_faces_history = index_arr_nv_faces_history_tmp;

                    size_history_tmp = bu_realloc(size_history, sizeof(size_t) * history_arrays_size,
                                       "size_history_tmp");

                    size_history = size_history_tmp;
                }

            /* increment number of polygons in current grouping (i.e. current nmg) */
            numNorPolygons_in_current_shell++;
        }
        /* NMG2e */

        /* execute this code when there is a face to process */
        if ( found ) {
            int idx = 0; 
            int numTriangles = 0; /* number of triangle faces which need to be created */

            /* size is the number of vertices in the current polygon */
            if ( size > 3 ) {
                numTriangles = size - 2 ;
            } else {
                numTriangles = 1 ;
            }

            /* numTriangles are the number of resulting triangles after triangulation */
            /* this loop triangulates the current polygon, only works if convex */
            for (idx = 0 ; idx < numTriangles ; idx++) {
                /* for each iteration of this loop, write all 6 indexes for the current triangle */

                /* triangle vertices indexes */
                triangle_indexes[numNorTriangles_in_current_bot][0][0] = index_arr_nv_faces[0][0] ;
                triangle_indexes[numNorTriangles_in_current_bot][1][0] = index_arr_nv_faces[idx+1][0] ;
                triangle_indexes[numNorTriangles_in_current_bot][2][0] = index_arr_nv_faces[idx+2][0] ;

                /* triangle vertices normals indexes */
                triangle_indexes[numNorTriangles_in_current_bot][0][1] = index_arr_nv_faces[0][1] ;
                triangle_indexes[numNorTriangles_in_current_bot][1][1] = index_arr_nv_faces[idx+1][1] ;
                triangle_indexes[numNorTriangles_in_current_bot][2][1] = index_arr_nv_faces[idx+2][1] ;

                bu_log("(%lu)(%lu)(%lu)(%lu)(%lu)(%lu)(%lu)(%lu)\n",
                   i,
                   numNorTriangles_in_current_bot,
                   triangle_indexes[numNorTriangles_in_current_bot][0][0],
                   triangle_indexes[numNorTriangles_in_current_bot][1][0],
                   triangle_indexes[numNorTriangles_in_current_bot][2][0],
                   triangle_indexes[numNorTriangles_in_current_bot][0][1],
                   triangle_indexes[numNorTriangles_in_current_bot][1][1],
                   triangle_indexes[numNorTriangles_in_current_bot][2][1]); 

                /* increment number of triangles in current grouping (i.e. current bot) */
                numNorTriangles_in_current_bot++;

                /* test if size of triangle_indexes needs to be increased */
                if (numNorTriangles_in_current_bot >= (triangle_indexes_size / num_indexes_per_triangle)) {
                    /* compute how large to make next alloc */
                    triangle_indexes_size = triangle_indexes_size +
                                                      (num_triangles_per_alloc * num_indexes_per_triangle);

                    numRealloc++;
                    bu_log("about to perform realloc number (%lu) with size (%lu)\n",
                       numRealloc, triangle_indexes_size);

                    triangle_indexes_tmp = bu_realloc(triangle_indexes, 
                                                      sizeof(*triangle_indexes) * triangle_indexes_size,
                                                      "triangle_indexes_tmp");
                    triangle_indexes = triangle_indexes_tmp;
                }
            } /* this loop exits when all the triangles from the current polygon 
                 are written to the triangle_indexes array */
        }

    }  /* numNorFaces loop, when loop exits, all nv_faces have been reviewed */

    /* need to process the triangle_indexes to find only the indexes needed */

    if ( numNorTriangles_in_current_bot > 0 ) {
    size_t num_indexes = 0; /* for vertices and normals */

    /* num_indexes is the number of vertex indexes in triangle_indexes array */
    /* num_indexes is also the number of vertex normal indexes in triangle_indexes array */
    num_indexes = (numNorTriangles_in_current_bot * num_indexes_per_triangle) / 2 ;
    bu_log("#ntri (%lu) #ni/tri (%lu) #elems (%lu)\n", 
       numNorTriangles_in_current_bot,
       num_indexes_per_triangle,
       num_indexes);

    /* replace "some_ints" with "triangle_indexes" */
    /* where the data-type for "some_ints" is used, change to "size_t" */

    size_t last = 0 ;
    size_t k = 0 ;

    size_t counter = 0;

    size_t num_unique_vertex_indexes = 0 ;
    size_t num_unique_vertex_normal_indexes = 0 ;

    /* array to store sorted unique libobj vertex index values for current bot */
    size_t *unique_vertex_indexes = NULL ;

    /* array to store sorted unique libobj vertex normal index values for current bot */
    size_t *unique_vertex_normal_indexes = NULL ;

    size_t *vertex_sort_index = NULL ; 
    size_t *vertex_normal_sort_index = NULL ;

    vertex_sort_index = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "vertex_sort_index");

    vertex_normal_sort_index = (size_t *)bu_calloc(num_indexes, sizeof(size_t), "vertex_normal_sort_index");

    /* populate index arrays */
    for (k = 0 ; k < num_indexes ; k++) {
        vertex_sort_index[k] = k*2 ;
        vertex_normal_sort_index[k] = (k*2)+1 ;
    }

    tmp_ptr = (size_t *)triangle_indexes;

    bu_log("non-sorted vertex_sort_index index contents ...\n");
    for (k = 0; k < num_indexes ; ++k) {
        bu_log("(%lu)\n", vertex_sort_index[k]);
    }

    /* sort vertex_sort_index containing indexes into vertex
       indexes within triangle_indexes array */
    qsort(vertex_sort_index, num_indexes, sizeof vertex_sort_index[0],
         (int (*)(const void *a, const void *b))comp);

    /* sort vertex_normal_sort_index containing indexes into vertex normal
       indexes within triangle_indexes array */
    qsort(vertex_normal_sort_index, num_indexes, sizeof vertex_normal_sort_index[0],
         (int (*)(const void *a, const void *b))comp);

    /* count sorted and unique libobj vertex indexes */
    last = tmp_ptr[vertex_sort_index[0]];
    num_unique_vertex_indexes = 1;
    for (k = 1; k < num_indexes ; ++k) {
        if (tmp_ptr[vertex_sort_index[k]] != last) {
            last = tmp_ptr[vertex_sort_index[k]];
            num_unique_vertex_indexes++;
        }
    }
    bu_log("num_unique_vertex_indexes = (%lu)\n", num_unique_vertex_indexes);

    /* count sorted and unique libobj vertex normal indexes */
    last = tmp_ptr[vertex_normal_sort_index[0]];
    num_unique_vertex_normal_indexes = 1;
    for (k = 1; k < num_indexes ; ++k) {
        if (tmp_ptr[vertex_normal_sort_index[k]] != last) {
            last = tmp_ptr[vertex_normal_sort_index[k]];
            num_unique_vertex_normal_indexes++;
        }
    }
    bu_log("num_unique_vertex_normal_indexes = (%lu)\n", num_unique_vertex_normal_indexes);

    unique_vertex_indexes = (size_t *)bu_calloc(num_unique_vertex_indexes, 
                                                sizeof(size_t), "unique_vertex_indexes");

    unique_vertex_normal_indexes = (size_t *)bu_calloc(num_unique_vertex_normal_indexes, 
                                                sizeof(size_t), "unique_vertex_normal_indexes");

    /* store sorted and unique libobj vertex indexes */
    bu_log("storing sorted and unique libobj vertex indexes\n");
    counter = 0;
    last = tmp_ptr[vertex_sort_index[0]];
    unique_vertex_indexes[counter] = last ;
    for (k = 1; k < num_indexes ; ++k) {
        if (tmp_ptr[vertex_sort_index[k]] != last) {
            last = tmp_ptr[vertex_sort_index[k]];
            counter++;
            unique_vertex_indexes[counter] = last ;
        }
    }

    /* store sorted and unique libobj vertex normal indexes */
    bu_log("storing sorted and unique libobj vertex normal indexes\n");
    counter = 0;
    last = tmp_ptr[vertex_normal_sort_index[0]];
    unique_vertex_normal_indexes[counter] = last ;
    for (k = 1; k < num_indexes ; ++k) {
        if (tmp_ptr[vertex_normal_sort_index[k]] != last) {
            last = tmp_ptr[vertex_normal_sort_index[k]];
            counter++;
            unique_vertex_normal_indexes[counter] = last ;
        }
    }

    /* output stored sorted and unique libobj vertex indexes */
    bu_log("stored sorted and unique libobj vertex indexes\n");
    for (k = 0; k < num_unique_vertex_indexes ; ++k)
        bu_log("(%lu)\n", unique_vertex_indexes[k]);

    /* output stored sorted and unique libobj vertex normal indexes */
    bu_log("stored sorted and unique libobj vertex normal indexes\n");
    for (k = 0; k < num_unique_vertex_normal_indexes ; ++k)
        bu_log("(%lu)\n", unique_vertex_normal_indexes[k]);

    bu_log("sorted vertex_sort_index & vertex_normal_sort_index index contents ...\n");
    for (k = 0; k < num_indexes ; ++k) {
        bu_log("(%lu)(%lu)\n", vertex_sort_index[k], vertex_normal_sort_index[k]);
    }

    bu_log("raw triangle_indexes contents ...\n");
    for (k = 0; k < (num_indexes * 2) ; ++k) {
        bu_log("(%lu)\n", tmp_ptr[k]);
    }

    bu_log("triangle_indexes vertex index contents ...\n");
    for (k = 0; k < (num_indexes * 2) ; k=k+2) {
        bu_log("(%lu)\n", tmp_ptr[k]);
    }

    bu_log("triangle_indexes vertex index normal contents ...\n");
    for (k = 1; k < (num_indexes * 2) ; k=k+2) {
        bu_log("(%lu)\n", tmp_ptr[k]);
    }

    /* compute memory required for bot vertices array */
    bot_vertex_array_size = num_unique_vertex_indexes * 3 ; /* i.e. 3 coordinates per vertex index */

    /* compute memory required for bot vertices normals array */
    bot_normals_array_size = num_unique_vertex_normal_indexes * 3 ; /* i.e. 3 fastf_t per normal */

    /* allocate memory for bot vertices array */
    vertices = (fastf_t *)bu_calloc((size_t)bot_vertex_array_size, sizeof(fastf_t), "vertices");

    /* allocate memory for bot normals array */
    normals = (fastf_t *)bu_calloc((size_t)bot_normals_array_size, sizeof(fastf_t), "normals");

    /* populate bot vertex array */
    /* places xyz vertices into bot structure */
    j = 0;
    for (k = 0 ; k < bot_vertex_array_size ; k=k+3 ) {
        vertices[k] =    (ga->vert_list[unique_vertex_indexes[j]][0]) * conversion_factor;
        vertices[k+1] =  (ga->vert_list[unique_vertex_indexes[j]][1]) * conversion_factor;
        vertices[k+2] =  (ga->vert_list[unique_vertex_indexes[j]][2]) * conversion_factor;
        j++;
    }

    /* populate bot normals array */
    /* places normals into bot structure */
    j = 0;
    for (k = 0 ; k < bot_normals_array_size ; k=k+3 ) {
        normals[k] =    (ga->norm_list[unique_vertex_normal_indexes[j]][0]) ;
        normals[k+1] =  (ga->norm_list[unique_vertex_normal_indexes[j]][1]) ;
        normals[k+2] =  (ga->norm_list[unique_vertex_normal_indexes[j]][2]) ;
        j++;
    }

    bu_log("raw populated bot vertices contents\n");
    for (k = 0 ; k < bot_vertex_array_size ; k=k+3 ) {
        bu_log("(%lu) (%f) (%f) (%f)\n", k, vertices[k], vertices[k+1], vertices[k+2]);
    }

    bu_log("raw populated bot normals contents\n");
    for (k = 0 ; k < bot_normals_array_size ; k=k+3 ) {
        bu_log("(%lu) (%f) (%f) (%f)\n", k, normals[k], normals[k+1], normals[k+2]);
    }

    /* allocate memory for faces and thickness arrays */
    faces = (int *)bu_calloc(numNorTriangles_in_current_bot * 3, sizeof(int), "faces");
    thickness = (fastf_t *)bu_calloc(numNorTriangles_in_current_bot * 3, sizeof(fastf_t), "thickness");

    /* allocate memory for bot face_normals, i.e. indices into normals array */
    face_normals = (int *)bu_calloc(numNorTriangles_in_current_bot * 3, sizeof(int), "face_normals");

    size_t *res0 = NULL;
    size_t *res1 = NULL;
    size_t *res2 = NULL;

    /* for each triangle, map libobj vertex indexes to bot vertex
       indexes, i.e. populate bot faces array */
    for (k = 0 ; k < numNorTriangles_in_current_bot ; k++ ) {

        res0 = bsearch(&(triangle_indexes[k][0][0]),unique_vertex_indexes,
                            num_unique_vertex_indexes, sizeof(size_t),
                            (int (*)(const void *a, const void *b))comp_b) ;
        res1 = bsearch(&(triangle_indexes[k][1][0]),unique_vertex_indexes,
                            num_unique_vertex_indexes, sizeof(size_t),
                            (int (*)(const void *a, const void *b))comp_b) ;
        res2 = bsearch(&(triangle_indexes[k][2][0]),unique_vertex_indexes,
                            num_unique_vertex_indexes, sizeof(size_t),
                            (int (*)(const void *a, const void *b))comp_b) ;

        /* should not need to test for null return from bsearch since we
           know all values are in the list we just don't know where */
        if ( res0 == NULL || res1 == NULL || res2 == NULL ) {
            bu_log("ERROR: bsearch returned null\n");
            return EXIT_FAILURE;
        }

        /* bsearch returns pointer to matching element, but need the index of
           the element, pointer subtraction computes the correct index value
           of the element */
        faces[k*3] = (int) (res0 - unique_vertex_indexes);
        faces[(k*3)+1] = (int) (res1 - unique_vertex_indexes);
        faces[(k*3)+2] = (int) (res2 - unique_vertex_indexes);
        thickness[(k*3)] = thickness[(k*3)+1] = thickness[(k*3)+2] = 1.0;

        bu_log("libobj to bot vert idx mapping (%lu), (%lu) --> (%d), (%lu) --> (%d), (%lu) --> (%d)\n",
           k,
           triangle_indexes[k][0][0], faces[k*3],
           triangle_indexes[k][1][0], faces[(k*3)+1],
           triangle_indexes[k][2][0], faces[(k*3)+2] );
    }

    /* write bot to ".g" file */
#if 0
    ret_val = mk_bot(outfp, bu_vls_addr(&outputObjectName), RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0, 
                     numNorTriangles_in_current_bot*3, numNorTriangles_in_current_bot, vertices,
                     faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);
#endif

    /* NMG2s */
    /* initialize tables */
    bu_ptbl_init(&faces2, 64, " &faces2 ");
    bu_ptbl_init(&names2, 64, " &names2 ");

    struct vertex  **nmg2_verts = NULL;
    nmg2_verts = (struct vertex **)bu_calloc(numNorPolygonVertices_in_current_nmg,
                                              sizeof(struct vertex *), "nmg2_verts");
    memset((void *)nmg2_verts, 0, sizeof(struct vertex *) * numNorPolygonVertices_in_current_nmg);

    size_t idx4 = 0;
    size_t idx5 = 0;
    size_t counter2 = 0;

    /* loop thru all the polygons (i.e. faces) to be placed in the current nmg */
    for ( idx4 = 0 ; idx4 < numNorPolygons_in_current_shell ; idx4++ ) {
        bu_log("about to run chk shell just before assign fu\n");
        NMG_CK_SHELL(s);

        /* call this once for each face */
        bu_log("about to assign fu\n");
        fu = nmg_cface(s, (struct vertex **)&(nmg2_verts[counter2]), (int)size_history[idx4] );
        counter2 = counter2 + size_history[idx4] ; 

        bu_ptbl_ins(&faces2, (long *)fu);
    }

    size_t polygon3 = 0;
    size_t vertex4 = 0;
    struct vertexuse *vu2 = NULL;
    fastf_t tmp5[3] = { 0, 0, 0 };
    counter2 = 0;
    bu_log("history: numNorPolygons_in_current_shell = (%lu)\n", numNorPolygons_in_current_shell);
    for ( polygon3 = 0 ; polygon3 < numNorPolygons_in_current_shell ; polygon3++ ) {
        bu_log("history: num vertices in current polygon = (%lu)\n", size_history[polygon3]);
        fu = (struct faceuse *)BU_PTBL_GET(&faces2, polygon3) ;
        for ( vertex4 = 0 ; vertex4 < size_history[polygon3] ; vertex4++ ) {
            bu_log("history: (%lu)(%lu)(%lu)(%f)(%f)(%f)(%f)(%f)(%f)(%f)\n", 
               polygon3,
               vertex4,
               index_arr_nv_faces_history[polygon3][vertex4][0],
               (ga->vert_list[index_arr_nv_faces_history[polygon3][vertex4][0]][0]) * conversion_factor,
               (ga->vert_list[index_arr_nv_faces_history[polygon3][vertex4][0]][1]) * conversion_factor,
               (ga->vert_list[index_arr_nv_faces_history[polygon3][vertex4][0]][2]) * conversion_factor,
               ga->vert_list[index_arr_nv_faces_history[polygon3][vertex4][0]][3],
               ga->norm_list[index_arr_nv_faces_history[polygon3][vertex4][1]][0],
               ga->norm_list[index_arr_nv_faces_history[polygon3][vertex4][1]][1],
               ga->norm_list[index_arr_nv_faces_history[polygon3][vertex4][1]][2]);

            VSCALE(tmp5, ga->vert_list[index_arr_nv_faces_history[polygon3][vertex4][0]] , conversion_factor);
            bu_log("about to run nmg_vertex_gv\n");
            NMG_CK_VERTEX(nmg2_verts[counter2]);
            nmg_vertex_gv(nmg2_verts[counter2], tmp5);   

            /* assign this normal to all uses of this vertex */
            for (BU_LIST_FOR(vu2, vertexuse, &nmg2_verts[counter2]->vu_hd)) {
                NMG_CK_VERTEXUSE(vu2);
                nmg_vertexuse_nv(vu2, (fastf_t *)ga->vert_list[index_arr_nv_faces_history[polygon3][vertex4][1]]);
            }
            counter2++;
        }
#if 1
        bu_log("about to run nmg_calc_face_g\n");
        if (nmg_calc_face_g(fu)) {
            bu_log("nmg_calc_face_g failed\n");
            nmg_kfu(fu);
            bu_ptbl_rm(&faces2, (const long *)fu);
            numNorPolygons_in_current_shell--;
        } else {
            bu_log("about to run nmg_fu_planeeqn\n");
            if (nmg_fu_planeeqn(fu, tol)) {
                bu_log("nmg_fu_planeeqn failed\n");
                nmg_kfu(fu);
                bu_ptbl_rm(&faces2, (const long *)fu);
                numNorPolygons_in_current_shell--;
            }
        }
#endif

    }

    (void)nmg_model_vertex_fuse(m, tol);

#if 0
    /* calculate plane equations for faces */
    NMG_CK_SHELL(s);
    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
    while (BU_LIST_NOT_HEAD(fu, &s->fu_hd)) {
        struct faceuse *kill_fu=(struct faceuse *)NULL;
        struct faceuse *next_fu;

        NMG_CK_FACEUSE(fu);

        next_fu = BU_LIST_NEXT(faceuse, &fu->l);
        if (fu->orientation == OT_SAME) {
            struct loopuse *lu;
            struct edgeuse *eu;
            fastf_t area;
            plane_t pl;

            lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
            NMG_CK_LOOPUSE(lu);
            for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
                NMG_CK_EDGEUSE(eu);
                if (eu->vu_p->v_p == eu->eumate_p->vu_p->v_p)
                    kill_fu = fu;
            }
            if (!kill_fu) {
                area = nmg_loop_plane_area(lu, pl);
                if (area <= 0.0) {
                    bu_log("ERROR: Can't get plane for face\n");
                    kill_fu = fu;
                }
            }
            if (kill_fu) {
                if (next_fu == kill_fu->fumate_p)
                    next_fu = BU_LIST_NEXT(faceuse, &next_fu->l);
                bu_ptbl_rm(&faces2, (long *)kill_fu);
                nmg_kfu(kill_fu);
            } else
                nmg_face_g(fu, pl);
        }
        fu = next_fu;
    }
#endif

    if (BU_PTBL_END(&faces2)) {
        bu_log("about to run nmg_gluefaces\n");
        nmg_gluefaces((struct faceuse **)BU_PTBL_BASEADDR(&faces2), BU_PTBL_END(&faces2), tol);

#if 1
        bu_log("about to run nmg_rebound 1\n");
        nmg_rebound(m, tol);

        bu_log("about to run nmg_fix_normals\n");
        nmg_fix_normals(s, tol);

        bu_log("about to run nmg_shell_coplanar_face_merge\n");
        nmg_shell_coplanar_face_merge(s, tol, 1);

        bu_log("about to run nmg_rebound 2\n");
        nmg_rebound(m, tol);
#endif

        bu_log("about to mk_bot_from_nmg\n");
        mk_bot_from_nmg(outfp, bu_vls_addr(&outputObjectName), s);
    } else {
        bu_log("Object %s has no faces\n", bu_vls_addr(&outputObjectName));
    } 
    /* NMG2e */

    bu_free(nmg2_verts,"nmg2_verts");
    bu_free(index_arr_nv_faces_history,"index_arr_nv_faces_history");
    bu_free(size_history,"size_history");
    bu_free(vertex_sort_index,"vertex_sort_index");
    bu_free(vertex_normal_sort_index,"vertex_normal_sort_index");
    bu_free(unique_vertex_indexes,"unique_vertex_indexes");
    bu_free(vertices,"vertices");
    bu_free(faces,"faces");
    bu_free(thickness,"thickness");
    bu_free(normals,"normals");
    bu_free(face_normals,"face_normals");
    bu_free(triangle_indexes, "triangle_indexes");

    bu_vls_free(&outputObjectName);

    bu_ptbl_reset(&faces2);

    }

    /* set empty to true if no faces were added
       to the current shell */
    *empty = !numNorPolygons_in_current_shell ;

    return ret_val;
}

int
main(int argc, char **argv)
{
    int c;
    char *prog = *argv, buf[BUFSIZ];
    FILE *fd_in;	/* input file */
    struct rt_wdb *fd_out;	/* Resulting BRL-CAD file */
    struct region_s *region = NULL;

    int ret_val = 0;
    FILE *my_stream;

#if 0
    struct ga_t ga = {NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
#endif
    struct ga_t ga;

    const size_t *index_arr_v_faces; /* used by v_faces */
    const size_t (*index_arr_tv_faces)[2]; /* used by tv_faces */

    size_t i = 0;

    char grouping_option = 'o'; /* to be selected by user from command line */

    int weiss_result;

    const char *parse_messages = (char *)0;
    int parse_err = 0;

    /* NMG2s */
    struct model *m ;
    struct nmgregion *r ;
    struct shell *s ;
    struct bn_tol tol_struct ;
    struct bn_tol *tol ;

    int empty = 0 ; /* boolean */

    tol = &tol_struct;

    m = nmg_mm();
    r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &r->s_hd);

    NMG_CK_MODEL(m);
    NMG_CK_REGION(r);
    NMG_CK_SHELL(s);

    tol->magic = BN_TOL_MAGIC;
    tol->dist = 0.00005;
    tol->dist_sq = tol->dist * tol->dist;
    tol->perp = 1e-6;
    tol->para = 1 - tol->perp;
    /* NMG2e */

    bu_log("running fopen\n");
    if ((my_stream = fopen("/home/rweiss/diamond.obj","r")) == NULL) {
        bu_log("Unable to open file.\n");
        perror(prog);
        return EXIT_FAILURE;
    }

    bu_log("running obj_parser_create\n");
    if ((ret_val = obj_parser_create(&ga.parser)) != 0) {
        if (ret_val == ENOMEM) {
            bu_log("Can not allocate an obj_parser_t object, Out of Memory.\n");
        } else {
            bu_log("Can not allocate an obj_parser_t object, Undefined Error (%d)\n", ret_val);
        }

        /* it is unclear if obj_parser_destroy must be run if obj_parser_create failed */
        bu_log("obj_parser_destroy\n");
        obj_parser_destroy(ga.parser);

        bu_log("running fclose\n");
        if (fclose(my_stream) != 0) {
            bu_log("Unable to close file.\n");
        }

        perror(prog);
        return EXIT_FAILURE;
    }

    bu_log("running obj_fparse\n");
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
        bu_log("obj_contents_destroy\n");
        obj_contents_destroy(ga.contents);

        bu_log("obj_parser_destroy\n");
        obj_parser_destroy(ga.parser);

        bu_log("running fclose\n");
        if (fclose(my_stream) != 0) {
            bu_log("Unable to close file.\n");
        }

        perror(prog);
        return EXIT_FAILURE;
    }

    if ((fd_out = wdb_fopen("diamond.g")) == NULL) {
        bu_log("Cannot create new BRL-CAD file (%s)\n", "diamond.g");
        perror(prog);
        bu_exit(1, NULL);
    }

#if 1
    fd_out->wdb_tol.dist    = tol->dist ;
    fd_out->wdb_tol.dist_sq = fd_out->wdb_tol.dist * fd_out->wdb_tol.dist ;
    fd_out->wdb_tol.perp    = tol->perp ;
    fd_out->wdb_tol.para    = 1 - fd_out->wdb_tol.perp ;
#endif

    collect_global_obj_file_attributes(&ga);

#if 0
/**********************/
/* processing v_faces */
/**********************/

    bu_log("running obj_polygonal_v_faces\n");
    numFaces = obj_polygonal_v_faces(contents, &attindex_arr_v_faces);
    bu_log("number of polygonal faces only identifed by vertices; numFaces = (%lu)\n", numFaces);

    for (i = 0 ; i < numFaces ; i++) {

        face_attr = polyattr_list + attindex_arr_v_faces[i];

        bu_log("running obj_groupset\n");
        setsize = obj_groupset(contents,face_attr->groupset_index,&indexset_arr);
        bu_log("number of groups v_face (%lu) belongs to = (%lu)\n",i ,setsize);

        /* list group names for v_faces */
        for (groupid = 0 ; groupid < setsize ; groupid++) {
            bu_log("group name number (%lu) = (%s)\n", groupid, str_arr_obj_groups[indexset_arr[groupid]]);
        }

        size = obj_polygonal_v_face_vertices(contents,i,&index_arr_v_faces);

        for (vert = 0 ; vert < size ; vert++) {
            bu_log("(%lu)(%lu)(%f)(%f)(%f)(%f)\n", vert, index_arr_v_faces[vert],   
               vert_list[index_arr_v_faces[vert]][0],   
               vert_list[index_arr_v_faces[vert]][1],   
               vert_list[index_arr_v_faces[vert]][2],   
               vert_list[index_arr_v_faces[vert]][3]);   
        }
    }
#endif

        empty = 0 ; /* set to false to force first region and
                       shell of model to be created */
        switch (grouping_option) {
            case 'g':
                bu_log("ENTERED 'g' PROCESSING\n");
                for (i = 0 ; i < ga.numGroups ; i++) {
                    bu_log("(%lu) current group name is (%s)\n", i, ga.str_arr_obj_groups[i]);

                    weiss_result = process_nv_faces(&ga, fd_out, GRP_GROUP, i, 1000.00, m, r, s, tol, &empty);
                }
                bu_log("EXITED 'g' PROCESSING\n");
                break;
            case 'o':
                bu_log("ENTERED 'o' PROCESSING\n");
                /* traverse list of all object groups i.e. 'o' in OBJ file 
                   create a region of each object group in this list */
                /* if object group name is "" use "default" instead. 
                   object group name already, create a new unique name
                   and use it.
                 */
                for (i = 0 ; i < ga.numObjects ; i++) {
                    bu_log("(%lu) current object group name is (%s)\n", i, ga.str_arr_obj_objects[i]);

                    /* if when process_nv_faces finished and the shell was not empty
                       then create a new region and shell for the next run of 
                       process_nv_faces */
                    if ( !empty ) {
                        r = nmg_mrsv(m);
                        s = BU_LIST_FIRST(shell, &r->s_hd);

                    }

                    weiss_result = process_nv_faces(&ga, fd_out, GRP_OBJECT, i, 1000.00, m, r, s, tol, &empty);

                } /* numObjects loop */

                /* if the last shell is empty, need to delete it */

                bu_log("EXITED 'o' PROCESSING\n");
                break;
            case 'm':
                bu_log("ENTERED 'm' PROCESSING\n");
                for (i = 0 ; i < ga.numMaterials ; i++) {
                    bu_log("(%lu) current object group name is (%s)\n", i, ga.str_arr_obj_materials[i]);

                    weiss_result = process_nv_faces(&ga, fd_out, GRP_MATERIAL, i, 1000.00, m, r, s, tol, &empty);

                } /* numMaterials loop */
                bu_log("EXITED 'm' PROCESSING\n");
                break;
            case 't':
                bu_log("ENTERED 't' PROCESSING\n");
                    weiss_result = process_nv_faces(&ga, fd_out, GRP_TEXTURE, i, 1000.00, m, r, s, tol, &empty);
                bu_log("EXITED 't' PROCESSING\n");
                break;
            default:
                break;
        }
 

/***********************/
/* processing tv_faces */
/***********************/

#if 0
    bu_log("running obj_polygonal_tv_faces\n");
    numTexFaces = obj_polygonal_tv_faces(contents, &attindex_arr_tv_faces);
    bu_log("number of textured polygonal faces; numTexFaces = (%lu)\n", numTexFaces);

    for (i = 0 ; i < numTexFaces ; i++) {

        face_attr = polyattr_list + attindex_arr_tv_faces[i];

        bu_log("running obj_groupset\n");
        setsize = obj_groupset(contents,face_attr->groupset_index,&indexset_arr);
        bu_log("number of groups tv_face (%lu) belongs to = (%lu)\n",i ,setsize);

        /* list group names for tv_faces */
        for (groupid = 0 ; groupid < setsize ; groupid++) {
            bu_log("group name number (%lu) = (%s)\n", groupid, str_arr_obj_groups[indexset_arr[groupid]]);
        }

        size = obj_polygonal_tv_face_vertices(contents,i,&index_arr_tv_faces);

        for (vert = 0 ; vert < size ; vert++) {
            bu_log("(%lu)(%lu)(%f)(%f)(%f)(%f)(%f)(%f)(%f)\n", 
               vert,
               index_arr_tv_faces[vert][0],
               vert_list[index_arr_tv_faces[vert][0]][0],
               vert_list[index_arr_tv_faces[vert][0]][1],
               vert_list[index_arr_tv_faces[vert][0]][2],
               vert_list[index_arr_tv_faces[vert][0]][3],
               texture_coord_list[index_arr_tv_faces[vert][1]][0],
               texture_coord_list[index_arr_tv_faces[vert][1]][1],
               texture_coord_list[index_arr_tv_faces[vert][1]][2]);
        }
    }
#endif

/*****************************/
/* running cleanup functions */
/*****************************/
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
