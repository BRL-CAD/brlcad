
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

int process_nv_faces(struct ga_t *ga, 
                     struct rt_wdb *outfp, 
                     int grp_mode, 
                     size_t grp_indx,
                     fastf_t conversion_factor) {
    size_t size = 0 ;
    size_t setsize = 0 ;
    size_t vert = 0;
    size_t vert2 = 0;
    const size_t (*index_arr_nv_faces)[2]; /* used by nv_faces */
    const size_t *indexset_arr;
    size_t groupid = 0;
    size_t numNorTriangles_in_current_bot = 0;
    int found = 0;
    size_t i = 0;
    int ret_val = 0;  /* 0=success !0=fail */

    fastf_t *vertices = NULL;
    fastf_t *vertices_tmp = NULL;
    size_t vertices_next_size = 0;
    int *faces = NULL;
    fastf_t *thickness = NULL;
    long int j = 0;

    struct bu_vls outputObjectName;

    size_t numRealloc = 0;

    int skip_degenerate_faces = 1; /* boolean */
    int degenerate_face = 0; /* boolean */

#if 0
    const size_t coordinates_per_triangle = 9 ; /* 3 coordinates per vertex, 3 vertex per face; i.e. 3*3=9 */
    size_t bot_vertex_array_size = 0;
#endif

    const size_t num_triangles_per_alloc = 128 ;
    const size_t num_indexes_per_triangle = 6 ; /* 3 vert/tri, 1 norm/vert, 2 idx/vert */
    size_t triangle_indexes_size = 0;
    size_t (*triangle_indexes)[3][2] = NULL ;
    size_t (*triangle_indexes_tmp)[3][2] = NULL ;

#if 0
    /* compute memory required for initial bot vertices array */
    bot_vertex_array_size = num_triangles_per_alloc * coordinates_per_triangle ;
    /* allocate memory for initial bot vertices array */
    vertices = (fastf_t *)bu_calloc((size_t)bot_vertex_array_size, sizeof(fastf_t), "vertices");
#endif


    /* compute memory required for initial triangle_indexes array */
    triangle_indexes_size = num_triangles_per_alloc * num_indexes_per_triangle ;

    /* allocate memory for initial triangle_indexes array */
    triangle_indexes = bu_calloc(triangle_indexes_size, sizeof(*triangle_indexes), "triangle_indexes");
#if 0
    triangle_indexes = (size_t *)bu_calloc((size_t)triangle_indexes_size, 
                                                     sizeof(size_t), "triangle_indexes");
#endif

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

        /* test for and force the skipp of degenerate faces */
        /* in this case degenrate faces are those with duplicate vertices */
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


#if 0
            /* load the bot structure with vertices */
            for (vert = 0 ; vert < size ; vert++) {
                bu_log("(%lu)(%lu)(%f)(%f)(%f)(%f)(%f)(%f)(%f)\n", 
                   vert,
                   index_arr_nv_faces[vert][0],
                   (ga->vert_list[index_arr_nv_faces[vert][0]][0]) * conversion_factor,
                   (ga->vert_list[index_arr_nv_faces[vert][0]][1]) * conversion_factor,
                   (ga->vert_list[index_arr_nv_faces[vert][0]][2]) * conversion_factor,
                   ga->vert_list[index_arr_nv_faces[vert][0]][3],
                   ga->norm_list[index_arr_nv_faces[vert][1]][0],
                   ga->norm_list[index_arr_nv_faces[vert][1]][1],
                   ga->norm_list[index_arr_nv_faces[vert][1]][2]);


                   /* place vertix indexes into face_tmp array structure */
                   /* a single face is 3 size_t */

                   /* places xyz vertices into bot structure */
                   vertices[((numNorTriangles_in_current_bot-1)*9)+(vert*3)+0] =
                       (ga->vert_list[index_arr_nv_faces[vert][0]][0]) * conversion_factor;
                   vertices[((numNorTriangles_in_current_bot-1)*9)+(vert*3)+1] =
                       (ga->vert_list[index_arr_nv_faces[vert][0]][1]) * conversion_factor;
                   vertices[((numNorTriangles_in_current_bot-1)*9)+(vert*3)+2] =
                       (ga->vert_list[index_arr_nv_faces[vert][0]][2]) * conversion_factor;

                   /* test if need a larger array */
                   if (numNorTriangles_in_current_bot >= (bot_vertex_array_size / coordinates_per_triangle)) {
                       /* compute how large to make next alloc */
                       bot_vertex_array_size = bot_vertex_array_size +
                                               (num_triangles_per_alloc * coordinates_per_triangle) ;

                       numRealloc++;
                       bu_log("about to perform realloc number (%lu) with size (%lu)\n",
                          numRealloc, bot_vertex_array_size);

                       vertices_tmp = (fastf_t *)bu_realloc(vertices, 
                                                 sizeof(fastf_t) * bot_vertex_array_size,
                                                 "vertices_tmp");
                       vertices = vertices_tmp;
                   }

            } /* old: loop exits when all vertices for current face are written to the bot vertex array */
              /* new: loop exits when all vertices for current face are written to face_tmp, and
                      will need to be wriiten to the bot vertex array later */
#endif
        }

    }  /* numNorFaces loop, when loop exits, all nv_faces have been reviewed */

/* need to process the triangle_indexes to find only the indexes needed */

#if 0
    if ( numNorTriangles_in_current_bot > 0 ) {
        bu_log("Found %ld numNorFaces in current bot\n", numNorTriangles_in_current_bot);
        mk_id(outfp, "RAW BOT");

        /* allocate memory for faces and thickness arrays */
        faces = (int *)bu_calloc(numNorTriangles_in_current_bot * 3, sizeof(int), "faces");
        thickness = (fastf_t *)bu_calloc(numNorTriangles_in_current_bot * 3, sizeof(fastf_t), "thickness");

        /* populate faces and thickness arrays */
        for ( j = 0 ; j < numNorTriangles_in_current_bot ; j++ ) {
            faces[(j*3)] = (j*3);
            faces[(j*3)+1] = (j*3) + 1;
            faces[(j*3)+2] = (j*3) + 2;
            thickness[(j*3)] = thickness[(j*3)+1] = thickness[(j*3)+2] = 1.0;
        }

        /* write bot to ".g" file */
        ret_val = mk_bot(outfp, bu_vls_addr(&outputObjectName), RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0, 
                         numNorTriangles_in_current_bot*3, numNorTriangles_in_current_bot, vertices,
                         faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);

#if 0
        bu_free(outputObjectName, "outputObjectName");
#endif

        bu_vls_free(&outputObjectName);

        bu_free(vertices, "vertices");
        bu_free(faces, "faces");
        bu_free(thickness, "thickness");

    }
#endif
        /* duplicate code to allow above to be commented */
        bu_vls_free(&outputObjectName);
        bu_free(triangle_indexes, "triangle_indexes");

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

    char grouping_option = 'm'; /* to be selected by user from command line */

    int weiss_result;

    const char *parse_messages = (char *)0;
    int parse_err = 0;

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

#if 0
    bu_log("obj_parse_error command conpleted\n");
    if ( ret_val != 0 ) {
        if (ret_val == ENOMEM) {
            if ( parse_messages ) {
                bu_log("%s\n", *parse_messages);
            }
            bu_log("obj_fparse, Out of Memory.\n");
            perror(prog);
            return EXIT_FAILURE;
        } else {
            if ( parse_messages ) {
                bu_log("%s\n", *parse_messages);
            }
            bu_log("obj_fparse, Other Error.\n");
            perror(prog);
            return EXIT_FAILURE;
        }
    }

    if ( parse_messages ) {
        bu_log("%s\n", *parse_messages);
    }
#endif

    if ((fd_out = wdb_fopen("diamond.g")) == NULL) {
        bu_log("Cannot create new BRL-CAD file (%s)\n", "diamond.g");
        perror(prog);
        bu_exit(1, NULL);
    }


collect_global_obj_file_attributes(&ga);



/**********************/
/* processing v_faces */
/**********************/

#if 0
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

/***********************/
/* processing nv_faces */
/***********************/


        switch (grouping_option) {
            case 'g':
                bu_log("ENTERED 'g' PROCESSING\n");
                for (i = 0 ; i < ga.numGroups ; i++) {
                    bu_log("(%lu) current group name is (%s)\n", i, ga.str_arr_obj_groups[i]);

                    weiss_result = process_nv_faces(&ga, fd_out, GRP_GROUP, i, 1000.00);
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

                    weiss_result = process_nv_faces(&ga, fd_out, GRP_OBJECT, i, 1000.00);

                } /* numObjects loop */
                bu_log("EXITED 'o' PROCESSING\n");
                break;
            case 'm':
                bu_log("ENTERED 'm' PROCESSING\n");
                for (i = 0 ; i < ga.numMaterials ; i++) {
                    bu_log("(%lu) current object group name is (%s)\n", i, ga.str_arr_obj_materials[i]);

                    weiss_result = process_nv_faces(&ga, fd_out, GRP_MATERIAL, i, 1000.00);

                } /* numMaterials loop */
                bu_log("EXITED 'm' PROCESSING\n");
                break;
            case 't':
                bu_log("ENTERED 't' PROCESSING\n");
                    weiss_result = process_nv_faces(&ga, fd_out, GRP_TEXTURE, i, 1000.00);
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
