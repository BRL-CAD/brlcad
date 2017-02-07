/*                   P A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file part.cpp
 *
 */

#include "common.h"
#include "creo-brl.h"


/* routine to check for bad triangles
 * only checks for triangles with duplicate vertices
 */
extern "C" int
bad_triangle( int v1, int v2, int v3 )
{
    double dist;
    double coord;
    int i;

    if ( v1 == v2 || v2 == v3 || v1 == v3 )
	return 1;

    dist = 0;
    for ( i=0; i<3; i++ ) {
	coord = vert_tree_root->the_array[v1*3+i] - vert_tree_root->the_array[v2*3+i];
	dist += coord * coord;
    }
    dist = sqrt( dist );
    if ( dist < local_tol ) {
	return 1;
    }

    dist = 0;
    for ( i=0; i<3; i++ ) {
	coord = vert_tree_root->the_array[v2*3+i] - vert_tree_root->the_array[v3*3+i];
	dist += coord * coord;
    }
    dist = sqrt( dist );
    if ( dist < local_tol ) {
	return 1;
    }

    dist = 0;
    for ( i=0; i<3; i++ ) {
	coord = vert_tree_root->the_array[v1*3+i] - vert_tree_root->the_array[v3*3+i];
	dist += coord * coord;
    }
    dist = sqrt( dist );
    if ( dist < local_tol ) {
	return 1;
    }

    return 0;
}

extern "C" void
add_to_empty_list( char *name )
{
    struct empty_parts *ptr;
    int found=0;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Adding %s to list of empty parts\n", name );
    }

    if ( empty_parts_root == NULL ) {
	BU_ALLOC(empty_parts_root, struct empty_parts);
	ptr = empty_parts_root;
    } else {
	ptr = empty_parts_root;
	while ( !found && ptr->next ) {
	    if ( BU_STR_EQUAL( name, ptr->name ) ) {
		found = 1;
		break;
	    }
	    ptr = ptr->next;
	}
	if ( !found ) {
	    BU_ALLOC(ptr->next, struct empty_parts);
	    ptr = ptr->next;
	}
    }

    ptr->next = NULL;
    ptr->name = (char *)bu_strdup( name );
}


extern "C" void
add_to_done_part( wchar_t *name )
{
    wchar_t *name_copy;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Added %s to list of done parts\n", ProWstringToString( astr, name ) );
    }

    if (done_list_part.find(name) == done_list_part.end()) {
	name_copy = ( wchar_t *)bu_calloc( wcslen( name ) + 1, sizeof( wchar_t ),
		"part name for done list" );
	wcsncpy( name_copy, name, wcslen(name)+1 );
	done_list_part.insert(name_copy);
    }
}

extern "C" int
already_done_part( wchar_t *name )
{
    if (done_list_part.find(name) != done_list_part.end()) {
	return 1;
    }
    return 0;
}

extern "C" int
feat_adds_material( ProFeattype feat_type )
{
    if ( feat_type >= PRO_FEAT_UDF_THREAD ) {
	return 1;
    }

    switch ( feat_type ) {
	case PRO_FEAT_SHAFT:
	case PRO_FEAT_PROTRUSION:
	case PRO_FEAT_NECK:
	case PRO_FEAT_FLANGE:
	case PRO_FEAT_RIB:
	case PRO_FEAT_EAR:
	case PRO_FEAT_DOME:
	case PRO_FEAT_LOC_PUSH:
	case PRO_FEAT_UDF:
	case PRO_FEAT_DRAFT:
	case PRO_FEAT_SHELL:
	case PRO_FEAT_DOME2:
	case PRO_FEAT_IMPORT:
	case PRO_FEAT_MERGE:
	case PRO_FEAT_MOLD:
	case PRO_FEAT_OFFSET:
	case PRO_FEAT_REPLACE_SURF:
	case PRO_FEAT_PIPE:
	    return 1;
	    break;
	default:
	    return 0;
	    break;
    }

    return 0;
}

extern "C" void
remove_holes_from_id_list( ProMdl model )
{
    int i;
    ProFeature feat;
    ProError status;
    ProFeattype type;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Removing any holes from CSG list and from features to delete\n" );
    }

    free_csg_ops();             /* these are only holes */
    for ( i=0; i<feat_id_count; i++ ) {
	status = ProFeatureInit( ProMdlToSolid(model),
		feat_ids_to_delete[i],
		&feat );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to get handle for id %d\n",
		    feat_ids_to_delete[i] );
	    if ( logger_type == LOGGER_TYPE_ALL ) {
		fprintf( logger, "Failed to get handle for id %d\n",
			feat_ids_to_delete[i] );
	    }
	}
	status = ProFeatureTypeGet( &feat, &type );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to get feature type for id %d\n",
		    feat_ids_to_delete[i] );
	    if ( logger_type == LOGGER_TYPE_ALL ) {
		fprintf( logger, "Failed to get feature type for id %d\n",
			feat_ids_to_delete[i] );
	    }
	}
	if ( type == PRO_FEAT_HOLE ) {
	    /* remove this from the list */
	    int j;

	    if ( logger_type == LOGGER_TYPE_ALL ) {
		fprintf( logger, "\tRemoving feature id %d from deletion list\n",
			feat_ids_to_delete[i] );
	    }
	    feat_id_count--;
	    for ( j=i; j<feat_id_count; j++ ) {
		feat_ids_to_delete[j] = feat_ids_to_delete[j+1];
	    }
	    i--;
	}
    }
}

/* this routine filters out which features should be visited by the feature visit initiated in
 * the output_part() routine.
 */
extern "C" ProError
feature_filter( ProFeature *feat, ProAppData data )
{
    ProError ret;
    ProMdl model = (ProMdl)data;

    if ( (ret=ProFeatureTypeGet( feat, &curr_feat_type )) != PRO_TK_NO_ERROR ) {

	fprintf( stderr, "ProFeatureTypeGet Failed for %s!!\n", curr_part_name );
	return ret;
    }
    if ( curr_feat_type > 0 ) {
	feat_type_count[curr_feat_type - FEAT_TYPE_OFFSET]++;
    } else if ( curr_feat_type == 0 ) {
	feat_type_count[0]++;
    }

    /* handle holes, chamfers, and rounds only */
    if ( curr_feat_type == PRO_FEAT_HOLE ||
	    curr_feat_type == PRO_FEAT_CHAMFER ||
	    curr_feat_type == PRO_FEAT_ROUND ) {
	return PRO_TK_NO_ERROR;
    }

    /* if we encounter a protrusion (or any feature that adds material) after a hole,
     * we cannot convert previous holes to CSG
     */
    if ( feat_adds_material( curr_feat_type ) ) {
	/* any holes must be removed from the list */
	remove_holes_from_id_list( model );
    }

    /* skip everything else */
    return PRO_TK_CONTINUE;
}

extern "C" void
build_tree( char *sol_name, struct bu_vls *tree )
{
    struct csg_ops *ptr;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Building CSG tree for %s\n", sol_name );
    }
    ptr = csg_root;
    while ( ptr ) {
	bu_vls_printf( tree, "{%c ", ptr->op );
	ptr = ptr->next;
    }

    bu_vls_strcat( tree, "{ l {" );
    bu_vls_strcat( tree, sol_name );
    bu_vls_strcat( tree, "} }" );
    ptr = csg_root;
    while ( ptr ) {
	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "Adding %c %s\n", ptr->op, bu_vls_addr( &ptr->name ) );
	}
	bu_vls_printf( tree, " {l {%s}}}", bu_vls_addr( &ptr->name ) );
	ptr = ptr->next;
    }

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Final tree: %s\n", bu_vls_addr( tree ) );
    }
}

extern "C" void
output_csg_prims()
{
    struct csg_ops *ptr;

    ptr = csg_root;

    while ( ptr ) {
	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "Creating primitive: %s %s\n",
		    bu_vls_addr( &ptr->name ), bu_vls_addr( &ptr->dbput ) );
	}

	fprintf( outfp, "put {%s} %s", bu_vls_addr( &ptr->name ), bu_vls_addr( &ptr->dbput ) );
	ptr = ptr->next;
    }
}

/* routine to free the list of CSG operations */
extern "C" void
free_csg_ops()
{
    struct csg_ops *ptr1, *ptr2;

    ptr1 = csg_root;

    while ( ptr1 ) {
	ptr2 = ptr1->next;
	bu_vls_free( &ptr1->name );
	bu_vls_free( &ptr1->dbput );
	bu_free( ptr1, "csg op" );
	ptr1 = ptr2;
    }

    csg_root = NULL;
}


/* routine to add a new triangle and its normals to the current part */
extern "C" void
add_triangle_and_normal( int v1, int v2, int v3, int n1, int n2, int n3 )
{
    if ( curr_tri >= max_tri ) {
	/* allocate more memory for triangles and normals */
	max_tri += TRI_BLOCK;
	part_tris = (ProTriangle *)bu_realloc( part_tris, sizeof( ProTriangle ) * max_tri,
		"part triangles");
	if ( !part_tris ) {
	    (void)ProMessageDisplay(MSGFIL, "USER_ERROR",
		    "Failed to allocate memory for part triangles" );
	    fprintf( stderr, "Failed to allocate memory for part triangles\n" );
	    (void)ProWindowRefresh( PRO_VALUE_UNUSED );
	    bu_exit( 1, NULL );
	}
	part_norms = (int *)bu_realloc( part_norms, sizeof( int ) * max_tri * 3,
		"part normals");
    }

    /* fill in triangle info */
    part_tris[curr_tri][0] = v1;
    part_tris[curr_tri][1] = v2;
    part_tris[curr_tri][2] = v3;

    part_norms[curr_tri*3]     = n1;
    part_norms[curr_tri*3 + 1] = n2;
    part_norms[curr_tri*3 + 2] = n3;

    /* increment count */
    curr_tri++;
}


/* routine to add a new triangle to the current part */
extern "C" void
add_triangle( int v1, int v2, int v3 )
{
    if ( curr_tri >= max_tri ) {
	/* allocate more memory for triangles */
	max_tri += TRI_BLOCK;
	part_tris = (ProTriangle *)bu_realloc( part_tris, sizeof( ProTriangle ) * max_tri,
		"part triangles");
	if ( !part_tris ) {
	    (void)ProMessageDisplay(MSGFIL, "USER_ERROR",
		    "Failed to allocate memory for part triangles" );
	    fprintf( stderr, "Failed to allocate memory for part triangles\n" );
	    (void)ProWindowRefresh( PRO_VALUE_UNUSED );
	    bu_exit( 1, NULL );
	}
    }

    /* fill in triangle info */
    part_tris[curr_tri][0] = v1;
    part_tris[curr_tri][1] = v2;
    part_tris[curr_tri][2] = v3;

    /* increment count */
    curr_tri++;
}



/* routine to output a part as a BRL-CAD region with one BOT solid
 * The region will have the name from Pro/E.
 * The solid will have the same name with "s." prefix.
 *
 *	returns:
 *		0 - OK
 *		1 - Failure
 *		2 - empty part, nothing output
 */
extern "C" int
output_part( ProMdl model )
{
    ProName part_name;
    ProModelitem mitm;
    ProSurfaceAppearanceProps aprops;
    char *brl_name=NULL;
    char *sol_name=NULL;
    ProSurfaceTessellationData *tess=NULL;
    ProError status;
    ProMdlType type;
    char str[PRO_NAME_SIZE + 1];
    int ret=0;
    int ret_status=0;
    char err_mess[512];
    wchar_t werr_mess[512];
    double curr_error;
    double curr_angle;
    register int i;

    /* if this part has already been output, do not do it again */
    if ( ProMdlNameGet( model, part_name ) != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get name for a part\n" );
	return 1;
    }

    if ( already_done_part( part_name ) )
	return 0;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Processing %s:\n", ProWstringToString( astr, part_name ) );
    }

    if ( ProMdlTypeGet( model, &type ) != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get part type\n" );
    } else {
	obj_type_count[type]++;
    }
    /* let user know we are doing something */

    status = ProUILabelTextSet( "creo_brl", "curr_proc", part_name );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to update dialog label for currently processed part\n" );
	return 1;
    }
#if 0
    status = ProUIDialogActivate( "creo_brl", &ret_status );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Error in creo-brl Dialog, error = %d\n",
		status );
	fprintf( stderr, "\t dialog returned %d\n", ret_status );
    }
#endif
    if ( !do_facets_only || do_elims ) {
	free_csg_ops();
	ProSolidFeatVisit( ProMdlToSolid(model), do_feature_visit,
		feature_filter, (ProAppData)model );

	if ( feat_id_count ) {
	    int i;

	    if ( logger_type == LOGGER_TYPE_ALL ) {
		fprintf( logger, "suppressing %d features of %s:\n",
			feat_id_count, curr_part_name );
		for ( i=0; i<feat_id_count; i++ ) {
		    fprintf( logger, "\t%d\n", feat_ids_to_delete[i] );
		}
	    }
	    fprintf( stderr, "suppressing %d features\n", feat_id_count );
	    ret = ProFeatureSuppress( ProMdlToSolid(model),
		    feat_ids_to_delete, feat_id_count,
		    NULL, 0 );
	    if ( ret != PRO_TK_NO_ERROR ) {
		ProFeatureResumeOptions resume_opts[1];
		ProFeatStatus feat_stat;
		ProFeature feat;

		resume_opts[0] = PRO_FEAT_RESUME_INCLUDE_PARENTS;

		if ( logger_type == LOGGER_TYPE_ALL ) {
		    fprintf( logger, "Failed to suppress features!!!\n" );
		}
		fprintf( stderr, "Failed to delete %d features from %s\n",
			feat_id_count, curr_part_name );

		for ( i=0; i<feat_id_count; i++ ) {
		    status = ProFeatureInit( ProMdlToSolid(model),
			    feat_ids_to_delete[i],
			    &feat );
		    if ( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to get handle for id %d\n",
				feat_ids_to_delete[i] );
			if ( logger_type == LOGGER_TYPE_ALL ) {
			    fprintf( logger, "Failed to get handle for id %d\n",
				    feat_ids_to_delete[i] );
			}
		    } else {
			status = ProFeatureStatusGet( &feat, &feat_stat );
			if ( status != PRO_TK_NO_ERROR ) {
			    fprintf( stderr,
				    "Failed to get status for feature %d\n",
				    feat_ids_to_delete[i] );
			    if ( logger_type == LOGGER_TYPE_ALL ) {
				fprintf( logger,
					"Failed to get status for feature %d\n",
					feat_ids_to_delete[i] );
			    }
			} else {
			    if ( logger_type == LOGGER_TYPE_ALL ) {
				if ( feat_stat < 0 ) {
				    fprintf( logger,
					    "invalid feature (%d)\n",
					    feat_ids_to_delete[i] );
				} else {
				    fprintf( logger, "feat %d status = %s\n",
					    feat_ids_to_delete[i],
					    feat_status[ feat_stat ] );
				}
			    }
			    if ( feat_stat == PRO_FEAT_SUPPRESSED ) {
				/* unsuppress this one */
				if ( logger_type == LOGGER_TYPE_ALL ) {
				    fprintf( logger,
					    "Unsuppressing feature %d\n",
					    feat_ids_to_delete[i] );
				}
				status = ProFeatureResume( ProMdlToSolid(model),
					&feat_ids_to_delete[i],
					1, resume_opts, 1 );
				if ( logger_type == LOGGER_TYPE_ALL ) {
				    if ( status == PRO_TK_NO_ERROR ) {
					fprintf( logger,
						"\tfeature id %d unsuppressed\n",
						feat_ids_to_delete[i] );
				    } else if ( status == PRO_TK_SUPP_PARENTS ) {
					fprintf( logger,
						"\tsuppressed parents for feature %d not found\n",
						feat_ids_to_delete[i] );

				    } else {
					fprintf( logger,
						"\tfeature id %d unsuppression failed\n",
						feat_ids_to_delete[i] );
				    }
				}
			    }
			}
		    }
		}

		feat_id_count = 0;
		free_csg_ops();
	    } else {
		fprintf( stderr, "features suppressed!!\n" );
	    }
	}
    }

    /* can get bounding box of a solid using "ProSolidOutlineGet"
     * may want to use this to implement relative facetization tolerance
     */

    /* tessellate part */
    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Tessellate part (%s)\n", curr_part_name );
    }

    /* Going from coarse to fine tessellation */
    for (i = 0; i <= max_to_min_steps; ++i) {
	curr_error = max_error - (i * error_increment);
	curr_angle = min_angle_cntrl + (i * angle_increment);

	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf(logger, "max_error = %g, min_error - %g, error_increment - %g\n", max_error, min_error, error_increment);
	    fprintf(logger, "max_angle_cntrl = %g, min_angle_cntrl - %g, angle_increment - %g\n", max_angle_cntrl, min_angle_cntrl, angle_increment);
	    fprintf(logger, "curr_error = %g, curr_angle - %g\n", curr_error, curr_angle);
	    fprintf(logger, "Trying to tessellate %s using:  tessellation error - %g, angle - %g\n", curr_part_name, curr_error, curr_angle);
	}

	status = ProPartTessellate( ProMdlToPart(model), curr_error/creo_to_brl_conv,
		curr_angle, PRO_B_TRUE, &tess  );

	if ( status == PRO_TK_NO_ERROR )
	    break;

	if ( logger_type == LOGGER_TYPE_ALL || logger_type == LOGGER_TYPE_FAILURE || logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) {
	    fprintf(logger, "Failed to tessellate %s using:  tessellation error - %g, angle - %g\n", curr_part_name, curr_error, curr_angle);
	}
    }

    if ( status != PRO_TK_NO_ERROR ) {
	/* Failed!!! */

	if ( logger_type == LOGGER_TYPE_ALL || logger_type == LOGGER_TYPE_FAILURE || logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) {
	    fprintf( logger, "Failed to tessellate %s!!!\n", curr_part_name );
	}
	snprintf( astr, sizeof(astr), "Failed to tessellate part (%s)", curr_part_name);
	(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
	ProMessageClear();
	fprintf( stderr, "%s\n", astr );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	add_to_empty_list( get_brlcad_name( curr_part_name ) );
	ret = 1;
    } else if ( !tess ) {
	/* not a failure, just an empty part */
	if ( logger_type == LOGGER_TYPE_ALL || logger_type == LOGGER_TYPE_SUCCESS || logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) {
	    fprintf( logger, "Empty part. (%s) has no surfaces!!!\n", curr_part_name );
	}
	snprintf( astr, sizeof(astr), "%s has no surfaces, ignoring", curr_part_name );
	(void)ProMessageDisplay(MSGFIL, "USER_WARNING", astr );
	ProMessageClear();
	fprintf( stderr, "%s\n", astr );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	add_to_empty_list( get_brlcad_name( curr_part_name ) );
	ret = 2;
    } else {
	/* output the triangles */
	int surface_count;

	if ( logger_type == LOGGER_TYPE_ALL || logger_type == LOGGER_TYPE_SUCCESS || logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) {
	    fprintf( logger, "Successfully tessellated %s using: tessellation error - %g, angle - %g!!!\n",
		    curr_part_name, curr_error, curr_angle );
	}

	status = ProArraySizeGet( (ProArray)tess, &surface_count );
	if ( status != PRO_TK_NO_ERROR ) {
	    (void)ProMessageDisplay(MSGFIL, "USER_ERROR", "Failed to get array size" );
	    ProMessageClear();
	    fprintf( stderr, "Failed to get array size\n" );
	    (void)ProWindowRefresh( PRO_VALUE_UNUSED );
	    ret = 1;
	} else if ( surface_count < 1 ) {
	    if ( logger_type == LOGGER_TYPE_ALL || logger_type == LOGGER_TYPE_SUCCESS || logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) {
		fprintf( logger, "Empty part. (%s) has no surfaces!!!\n", curr_part_name );
	    }
	    snprintf( astr, sizeof(astr), "%s has no surfaces, ignoring", curr_part_name );
	    (void)ProMessageDisplay(MSGFIL, "USER_WARNING", astr );
	    ProMessageClear();
	    fprintf( stderr, "%s\n", astr );
	    (void)ProWindowRefresh( PRO_VALUE_UNUSED );
	    add_to_empty_list( get_brlcad_name( curr_part_name ) );
	    ret = 2;
	} else {
	    int i;
	    int v1, v2, v3, n1, n2, n3;
	    int surfno;
	    int vert_no;
	    ProName material;
	    ProMassProperty mass_prop;
	    ProMaterialProps material_props;
	    int got_density;
	    struct bu_vls tree = BU_VLS_INIT_ZERO;

	    curr_tri = 0;
	    clean_vert_tree(vert_tree_root);
	    clean_vert_tree(norm_tree_root);

	    /* add all vertices and triangles to our lists */
	    if ( logger_type == LOGGER_TYPE_ALL ) {
		fprintf( logger, "Processing surfaces of part %s\n", curr_part_name );
	    }
	    for ( surfno=0; surfno<surface_count; surfno++ ) {
		for ( i=0; i<tess[surfno].n_facets; i++ ) {
		    /* grab the triangle */
		    vert_no = tess[surfno].facets[i][0];
		    v1 = Add_vert( tess[surfno].vertices[vert_no][0], tess[surfno].vertices[vert_no][1],
			    tess[surfno].vertices[vert_no][2], vert_tree_root, local_tol_sq );
		    vert_no = tess[surfno].facets[i][1];
		    v2 = Add_vert( tess[surfno].vertices[vert_no][0], tess[surfno].vertices[vert_no][1],
			    tess[surfno].vertices[vert_no][2], vert_tree_root, local_tol_sq );
		    vert_no = tess[surfno].facets[i][2];
		    v3 = Add_vert( tess[surfno].vertices[vert_no][0], tess[surfno].vertices[vert_no][1],
			    tess[surfno].vertices[vert_no][2], vert_tree_root, local_tol_sq );
		    if ( bad_triangle( v1, v2, v3 ) ) {
			continue;
		    }

		    if ( !get_normals ) {
			add_triangle( v1, v2, v3 );
			continue;
		    }

		    /* grab the surface normals */
		    vert_no = tess[surfno].facets[i][0];
		    VUNITIZE( tess[surfno].normals[vert_no] );
		    n1 = Add_vert( tess[surfno].normals[vert_no][0], tess[surfno].normals[vert_no][1],
			    tess[surfno].normals[vert_no][2], norm_tree_root, local_tol_sq );
		    vert_no = tess[surfno].facets[i][1];
		    VUNITIZE( tess[surfno].normals[vert_no] );
		    n2 = Add_vert( tess[surfno].normals[vert_no][0], tess[surfno].normals[vert_no][1],
			    tess[surfno].normals[vert_no][2], norm_tree_root, local_tol_sq );
		    vert_no = tess[surfno].facets[i][2];
		    VUNITIZE( tess[surfno].normals[vert_no] );
		    n3 = Add_vert( tess[surfno].normals[vert_no][0], tess[surfno].normals[vert_no][1],
			    tess[surfno].normals[vert_no][2], norm_tree_root, local_tol_sq );

		    add_triangle_and_normal( v1, v2, v3, n1, n2, n3 );
		}
	    }

	    /* actually output the part */
	    /* first the BOT solid with a made-up name */
	    brl_name = get_brlcad_name( curr_part_name );
	    sol_name = (char *)bu_malloc( strlen( brl_name ) + 3, "aol_name" );
	    snprintf( sol_name, strlen(brl_name)+3, "s.%s", brl_name );
	    if ( logger_type == LOGGER_TYPE_ALL ) {
		fprintf( logger, "Creating bot primitive (%s) for part %s\n",
			sol_name, brl_name );
	    }

	    fprintf( outfp, "put {%s} bot mode volume orient no V { ", sol_name );

	    for ( i=0; i<vert_tree_root->curr_vert; i++ ) {
		fprintf( outfp, " {%.12e %.12e %.12e}",
			vert_tree_root->the_array[i*3] * creo_to_brl_conv,
			vert_tree_root->the_array[i*3+1] * creo_to_brl_conv,
			vert_tree_root->the_array[i*3+2] * creo_to_brl_conv );
	    }
	    fprintf( outfp, " } F {" );
	    for ( i=0; i<curr_tri; i++ ) {
		/* Proe orders things using left-hand rule, so reverse the order */
		fprintf( outfp, " {%d %d %d}", part_tris[i][2],
			part_tris[i][1], part_tris[i][0] );
	    }
	    if ( get_normals ) {
		if ( logger ) {
		    fprintf( logger, "Getting vertex normals for part %s\n",
			    curr_part_name );
		}
		fprintf( outfp, " } flags { has_normals use_normals } N {" );
		for ( i=0; i<norm_tree_root->curr_vert; i++ ) {
		    fprintf( outfp, " {%.12e %.12e %.12e}",
			    norm_tree_root->the_array[i*3] * creo_to_brl_conv,
			    norm_tree_root->the_array[i*3+1] * creo_to_brl_conv,
			    norm_tree_root->the_array[i*3+2] * creo_to_brl_conv );
		}
		fprintf( outfp, " } fn {" );
		for ( i=0; i<curr_tri; i++ ) {
		    fprintf( outfp, " {%d %d %d}", part_norms[i*3],
			    part_norms[i*3+1], part_norms[i*3+2] );
		}
	    }
	    fprintf( outfp, " }\n" );

	    /* Set the CREO_NAME attributes for the solid/primitive */
	    fprintf( outfp, "attr set {%s} %s %s\n", sol_name, CREO_NAME_ATTR, curr_part_name );

	    /* build the tree for this region */
	    build_tree( sol_name, &tree );
	    bu_free( sol_name, "sol_name" );
	    output_csg_prims();

	    /* get the surface properties for the part
	     * and create a region using the actual part name
	     */
	    if ( logger_type == LOGGER_TYPE_ALL ) {
		fprintf( logger, "Creating region for part %s\n", curr_part_name );
	    }


	    ProMdlToModelitem(model, &mitm);
	    ProError aerr = ProSurfaceAppearancepropsGet(&mitm, &aprops);
	    if (aerr  == PRO_TK_NOT_EXIST ) {
		/* no surface properties */
		fprintf( outfp,
			"put {%s} comb region yes id %d los 100 GIFTmater 1 tree %s\n",
			get_brlcad_name( curr_part_name ), reg_id, bu_vls_addr( &tree) );
	    } else if ( aerr == PRO_TK_NO_ERROR ) {
		/* use the colors, ... that was set in Pro/E */
		fprintf( outfp,
			"put {%s} comb region yes id %d los 100 GIFTmater 1 rgb {%d %d %d} shader {plastic {",
			get_brlcad_name( curr_part_name ),
			reg_id,
			(int)(aprops.color_rgb[0]*255.0),
			(int)(aprops.color_rgb[1]*255.0),
			(int)(aprops.color_rgb[2]*255.0) );
		if ( aprops.transparency != 0.0 ) {
		    fprintf( outfp, " tr %g", aprops.transparency );
		}
		if ( aprops.shininess != 1.0 ) {
		    fprintf( outfp, " sh %d", (int)(aprops.shininess * 18 + 2.0) );
		}
		if ( aprops.diffuse != 0.3 ) {
		    fprintf( outfp, " di %g", aprops.diffuse );
		}
		if ( aprops.highlite != 0.7 ) {
		    fprintf( outfp, " sp %g", aprops.highlite );
		}
		fprintf( outfp, "} }" );
		fprintf( outfp, " tree %s\n", bu_vls_addr( &tree ) );
	    } else {
		/* something is wrong, but just ignore the missing properties */
		fprintf( stderr, "Error getting surface properties for %s\n",
			curr_part_name );
		fprintf( outfp, "put {%s} comb region yes id %d los 100 GIFTmater 1 tree %s\n",
			get_brlcad_name( curr_part_name ), reg_id, bu_vls_addr( &tree ) );
	    }

	    /* Set the CREO_NAME attributes for the region */
	    fprintf( outfp, "attr set {%s} %s %s\n", get_brlcad_name( curr_part_name ), CREO_NAME_ATTR, curr_part_name );

	    /* if the part has a material, add it as an attribute */
	    got_density = 0;
	    status = ProPartMaterialNameGet( ProMdlToPart(model), material );
	    if ( status == PRO_TK_NO_ERROR ) {
		fprintf( outfp, "attr set {%s} material_name {%s}\n",
			get_brlcad_name( curr_part_name ),
			ProWstringToString( str, material ) );

		/* get the density for this material */
		status = ProPartMaterialdataGet( ProMdlToPart(model), material, &material_props );
		if ( status == PRO_TK_NO_ERROR ) {
		    got_density = 1;
		    fprintf( outfp, "attr set {%s} density %g\n",
			    get_brlcad_name( curr_part_name ),
			    material_props.mass_density );
		}
	    }

	    /* calculate mass properties */
	    status = ProSolidMassPropertyGet( ProMdlToSolid( model ), NULL, &mass_prop );
	    if ( status == PRO_TK_NO_ERROR ) {
		if ( !got_density ) {
		    if ( mass_prop.density > 0.0 ) {
			fprintf( outfp, "attr set {%s} density %g\n",
				get_brlcad_name( curr_part_name ),
				mass_prop.density );
		    }
		}
		if ( mass_prop.mass > 0.0 ) {
		    fprintf( outfp, "attr set {%s} mass %g\n",
			    get_brlcad_name( curr_part_name ),
			    mass_prop.mass );
		}
		if ( mass_prop.volume > 0.0 ) {
		    fprintf( outfp, "attr set {%s} volume %g\n",
			    get_brlcad_name( curr_part_name ),
			    mass_prop.volume );
		}
	    }

	    /* increment the region id */
	    reg_id++;

	    /* free the tree */
	    bu_vls_free( &tree );
	}
    }


    /* free the tessellation memory */
    ProPartTessellationFree( &tess );
    tess = NULL;

    /* add this part to the list of objects already output */
    add_to_done_part( part_name );

    /* unsuppress anything we suppressed */
    if ( feat_id_count ) {
	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "Unsuppressing %d features\n", feat_id_count );
	}
	fprintf( stderr, "Unsuppressing %d features\n", feat_id_count );
	if ( (ret=ProFeatureResume( ProMdlToSolid(model),
			feat_ids_to_delete, feat_id_count,
			NULL, 0 )) != PRO_TK_NO_ERROR) {

	    fprintf( stderr, "Failed to unsuppress %d features from %s\n",
		    feat_id_count, curr_part_name );

	    /* use UI dialog */
	    status = ProUIDialogCreate( "creo_brl_error", "creo_brl_error" );
	    if ( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to create dialog box for creo-brl, error = %d\n", status );
		return 0;
	    }
	    snprintf( err_mess, 512,
		    "During the conversion %d features of part %s\n"
		    "were suppressed. After the conversion was complete, an\n"
		    "attempt was made to unsuppress these same features.\n"
		    "The unsuppression failed, so these features are still\n"
		    "suppressed. Please exit Pro/E without saving any\n"
		    "changes so that this problem will not persist.", feat_id_count, curr_part_name );

	    (void)ProStringToWstring( werr_mess, err_mess );
	    status = ProUITextareaValueSet( "creo_brl_error", "the_message", werr_mess );
	    if ( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to create dialog box for creo-brl, error = %d\n", status );
		return 0;
	    }
	    (void)ProUIPushbuttonActivateActionSet( "creo_brl_error", "ok", kill_error_dialog, NULL );
#if 0
	    status = ProUIDialogActivate( "creo_brl_error", &ret_status );
	    if ( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Error in creo-brl error Dialog, error = %d\n",
			status );
		fprintf( stderr, "\t dialog returned %d\n", ret_status );
	    }
#endif
	    feat_id_count = 0;
	    return 0;
	}
	fprintf( stderr, "features unsuppressed!!\n" );
	feat_id_count = 0;
    }

    return ret;
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
