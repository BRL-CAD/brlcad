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

#if 0
/* routine to check for bad triangles
 * only checks for triangles with duplicate vertices
 */
extern "C" int
bad_triangle(struct creo_conv_info *cinfo,  int v1, int v2, int v3 )
{
    double dist;
    double coord;
    int i;

    if ( v1 == v2 || v2 == v3 || v1 == v3 )
	return 1;

    dist = 0;
    for ( i=0; i<3; i++ ) {
	coord = cinfo->vert_tree_root->the_array[v1*3+i] - cinfo->vert_tree_root->the_array[v2*3+i];
	dist += coord * coord;
    }
    dist = sqrt( dist );
    if ( dist < cinfo->local_tol ) {
	return 1;
    }

    dist = 0;
    for ( i=0; i<3; i++ ) {
	coord = cinfo->vert_tree_root->the_array[v2*3+i] - cinfo->vert_tree_root->the_array[v3*3+i];
	dist += coord * coord;
    }
    dist = sqrt( dist );
    if ( dist < cinfo->local_tol ) {
	return 1;
    }

    dist = 0;
    for ( i=0; i<3; i++ ) {
	coord = cinfo->vert_tree_root->the_array[v1*3+i] - cinfo->vert_tree_root->the_array[v3*3+i];
	dist += coord * coord;
    }
    dist = sqrt( dist );
    if ( dist < cinfo->local_tol ) {
	return 1;
    }

    return 0;
}

extern "C" void
add_to_empty_list(struct creo_conv_info *cinfo,  char *name )
{
    struct empty_parts *ptr;
    int found=0;

    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Adding %s to list of empty parts\n", name );
    }

    if ( cinfo->empty_parts_root == NULL ) {
	BU_ALLOC(cinfo->empty_parts_root, struct empty_parts);
	ptr = cinfo->empty_parts_root;
    } else {
	ptr = cinfo->empty_parts_root;
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
add_to_done_part(struct creo_conv_info *cinfo,  wchar_t *name )
{
    ProCharLine astr;
    wchar_t *name_copy;

    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Added %s to list of done parts\n", ProWstringToString( astr, name ) );
    }

    if (cinfo->done_list_part->find(name) == cinfo->done_list_part->end()) {
	name_copy = ( wchar_t *)bu_calloc( wcslen( name ) + 1, sizeof( wchar_t ),
		"part name for done list" );
	wcsncpy( name_copy, name, wcslen(name)+1 );
	cinfo->done_list_part->insert(name_copy);
    }
}

extern "C" int
already_done_part(struct creo_conv_info *cinfo,  wchar_t *name )
{
    if (cinfo->done_list_part->find(name) != cinfo->done_list_part->end()) {
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
remove_holes_from_id_list(struct creo_conv_info *cinfo,  ProMdl model )
{
    int i;
    ProFeature feat;
    ProError status;
    ProFeattype type;

    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Removing any holes from CSG list and from features to delete\n" );
    }

    free_csg_ops(cinfo);             /* these are only holes */
    for ( i=0; i < cinfo->feat_id_count; i++ ) {
	status = ProFeatureInit( ProMdlToSolid(model),
		cinfo->feat_ids_to_delete[i],
		&feat );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to get handle for id %d\n",
		    cinfo->feat_ids_to_delete[i] );
	    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
		fprintf(cinfo->logger, "Failed to get handle for id %d\n",
			cinfo->feat_ids_to_delete[i] );
	    }
	}
	status = ProFeatureTypeGet( &feat, &type );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to get feature type for id %d\n",
		    cinfo->feat_ids_to_delete[i] );
	    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
		fprintf(cinfo->logger, "Failed to get feature type for id %d\n",
			cinfo->feat_ids_to_delete[i] );
	    }
	}
	if ( type == PRO_FEAT_HOLE ) {
	    /* remove this from the list */
	    int j;

	    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
		fprintf(cinfo->logger, "\tRemoving feature id %d from deletion list\n",
			cinfo->feat_ids_to_delete[i] );
	    }
	    cinfo->feat_id_count--;
	    for ( j=i; j<cinfo->feat_id_count; j++ ) {
		cinfo->feat_ids_to_delete[j] = cinfo->feat_ids_to_delete[j+1];
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
    struct feature_data *fdata = (struct feature_data *)data;
    struct creo_conv_info *cinfo = fdata->cinfo;
    ProMdl model = fdata->model;

    if ( (ret=ProFeatureTypeGet( feat, &cinfo->curr_feat_type )) != PRO_TK_NO_ERROR ) {

	fprintf( stderr, "ProFeatureTypeGet Failed for %s!!\n", cinfo->curr_part_name );
	return ret;
    }

    /* handle holes, chamfers, and rounds only */
    if ( cinfo->curr_feat_type == PRO_FEAT_HOLE ||
	    cinfo->curr_feat_type == PRO_FEAT_CHAMFER ||
	    cinfo->curr_feat_type == PRO_FEAT_ROUND ) {
	return PRO_TK_NO_ERROR;
    }

    /* if we encounter a protrusion (or any feature that adds material) after a hole,
     * we cannot convert previous holes to CSG
     */
    if ( feat_adds_material( cinfo->curr_feat_type ) ) {
	/* any holes must be removed from the list */
	remove_holes_from_id_list(cinfo, model );
    }

    /* skip everything else */
    return PRO_TK_CONTINUE;
}


extern "C" void
output_csg_prims(struct creo_conv_info *cinfo)
{
    struct csg_ops *ptr;

    ptr = cinfo->csg_root;

    while ( ptr ) {
	if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf(cinfo->logger, "Creating primitive: %s %s\n",
		    bu_vls_addr( &ptr->name ), bu_vls_addr( &ptr->dbput ) );
	}

	fprintf( cinfo->outfp, "put {%s} %s", bu_vls_addr( &ptr->name ), bu_vls_addr( &ptr->dbput ) );
	ptr = ptr->next;
    }
}

/* routine to free the list of CSG operations */
extern "C" void
free_csg_ops(struct creo_conv_info *cinfo)
{
    struct csg_ops *ptr1, *ptr2;

    ptr1 = cinfo->csg_root;

    while ( ptr1 ) {
	ptr2 = ptr1->next;
	bu_vls_free( &ptr1->name );
	bu_vls_free( &ptr1->dbput );
	bu_free( ptr1, "csg op" );
	ptr1 = ptr2;
    }

    cinfo->csg_root = NULL;
}


/* routine to add a new triangle and its normals to the current part */
extern "C" void
add_triangle_and_normal(struct creo_conv_info *cinfo, int v1, int v2, int v3, int n1, int n2, int n3 )
{
    ProFileName msgfil;
    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    if ( cinfo->curr_tri >= cinfo->max_tri ) {
	/* allocate more memory for triangles and normals */
	cinfo->max_tri += TRI_BLOCK;
	cinfo->part_tris = (ProTriangle *)bu_realloc( cinfo->part_tris, sizeof( ProTriangle ) * cinfo->max_tri,
		"part triangles");
	if ( !cinfo->part_tris ) {
	    (void)ProMessageDisplay(msgfil, "USER_ERROR",
		    "Failed to allocate memory for part triangles" );
	    fprintf( stderr, "Failed to allocate memory for part triangles\n" );
	    (void)ProWindowRefresh( PRO_VALUE_UNUSED );
	    bu_exit( 1, NULL );
	}
	cinfo->part_norms = (int *)bu_realloc( cinfo->part_norms, sizeof( int ) * cinfo->max_tri * 3,
		"part normals");
    }

    /* fill in triangle info */
    cinfo->part_tris[cinfo->curr_tri][0] = v1;
    cinfo->part_tris[cinfo->curr_tri][1] = v2;
    cinfo->part_tris[cinfo->curr_tri][2] = v3;

    cinfo->part_norms[cinfo->curr_tri*3]     = n1;
    cinfo->part_norms[cinfo->curr_tri*3 + 1] = n2;
    cinfo->part_norms[cinfo->curr_tri*3 + 2] = n3;

    /* increment count */
    cinfo->curr_tri++;
}


/* routine to add a new triangle to the current part */
extern "C" void
add_triangle(struct creo_conv_info *cinfo, int v1, int v2, int v3 )
{
    ProFileName msgfil;
    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);
    if ( cinfo->curr_tri >= cinfo->max_tri ) {
	/* allocate more memory for triangles */
	cinfo->max_tri += TRI_BLOCK;
	cinfo->part_tris = (ProTriangle *)bu_realloc(cinfo->part_tris, sizeof( ProTriangle ) * cinfo->max_tri,
		"part triangles");
	if ( !cinfo->part_tris ) {
	    (void)ProMessageDisplay(msgfil, "USER_ERROR",
		    "Failed to allocate memory for part triangles" );
	    fprintf( stderr, "Failed to allocate memory for part triangles\n" );
	    (void)ProWindowRefresh( PRO_VALUE_UNUSED );
	    bu_exit( 1, NULL );
	}
    }

    /* fill in triangle info */
    cinfo->part_tris[cinfo->curr_tri][0] = v1;
    cinfo->part_tris[cinfo->curr_tri][1] = v2;
    cinfo->part_tris[cinfo->curr_tri][2] = v3;

    /* increment count */
    cinfo->curr_tri++;
}


extern "C" char *feat_status_to_str(ProFeatStatus feat_stat)
{
    static char *feat_status[]={
	"PRO_FEAT_ACTIVE",
	"PRO_FEAT_INACTIVE",
	"PRO_FEAT_FAMTAB_SUPPRESSED",
	"PRO_FEAT_SIMP_REP_SUPPRESSED",
	"PRO_FEAT_PROG_SUPPRESSED",
	"PRO_FEAT_SUPPRESSED",
	"PRO_FEAT_UNREGENERATED"
    };
    return feat_status[feat_stat];
}

extern "C" void
build_tree(struct creo_conv_info *cinfo,  char *sol_name, struct bu_vls *tree )
{
    struct csg_ops *ptr;

    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Building CSG tree for %s\n", sol_name );
    }
    ptr = cinfo->csg_root;
    while ( ptr ) {
	bu_vls_printf( tree, "{%c ", ptr->op );
	ptr = ptr->next;
    }

    bu_vls_strcat( tree, "{ l {" );
    bu_vls_strcat( tree, sol_name );
    bu_vls_strcat( tree, "} }" );
    ptr = cinfo->csg_root;
    while ( ptr ) {
	if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf(cinfo->logger, "Adding %c %s\n", ptr->op, bu_vls_addr( &ptr->name ) );
	}
	bu_vls_printf( tree, " {l {%s}}}", bu_vls_addr( &ptr->name ) );
	ptr = ptr->next;
    }

    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Final tree: %s\n", bu_vls_addr( tree ) );
    }
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
output_part(struct creo_conv_info *cinfo, ProMdl model)
{
    ProName part_name;
    ProModelitem mitm;
    ProSurfaceAppearanceProps aprops;
    char *brl_name=NULL;
    char *sol_name=NULL;
    ProSurfaceTessellationData *tess=NULL;
    ProError status;
    ProMdlType type;
    ProFileName msgfil;
    ProCharLine astr;
    char str[PRO_NAME_SIZE + 1];
    int ret=0;
    int ret_status=0;
    char err_mess[512];
    wchar_t werr_mess[512];
    double curr_error;
    double curr_angle;
    register int i;

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    /* if this part has already been output, do not do it again */
    if ( ProMdlNameGet( model, part_name ) != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get name for a part\n" );
	return 1;
    }

    if ( already_done_part(cinfo, part_name ) )
	return 0;

    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Processing %s:\n", ProWstringToString( astr, part_name ) );
    }

    if ( ProMdlTypeGet( model, &type ) != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get part type\n" );
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
    if ( !cinfo->do_facets_only || cinfo->do_elims ) {
	struct feature_data fdata;
	fdata.cinfo = cinfo;
	fdata.model = model;
	free_csg_ops(cinfo);
	ProSolidFeatVisit( ProMdlToSolid(model), do_feature_visit,
		feature_filter, (ProAppData)&fdata);

	if ( cinfo->feat_id_count ) {
	    int i;

	    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
		fprintf(cinfo->logger, "suppressing %d features of %s:\n",
			cinfo->feat_id_count, cinfo->curr_part_name );
		for ( i=0; i<cinfo->feat_id_count; i++ ) {
		    fprintf(cinfo->logger, "\t%d\n", cinfo->feat_ids_to_delete[i] );
		}
	    }
	    fprintf( stderr, "suppressing %d features\n", cinfo->feat_id_count );
	    ret = ProFeatureSuppress( ProMdlToSolid(model),
		    cinfo->feat_ids_to_delete, cinfo->feat_id_count,
		    NULL, 0 );
	    if ( ret != PRO_TK_NO_ERROR ) {
		ProFeatureResumeOptions resume_opts[1];
		ProFeatStatus feat_stat;
		ProFeature feat;

		resume_opts[0] = PRO_FEAT_RESUME_INCLUDE_PARENTS;

		if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
		    fprintf(cinfo->logger, "Failed to suppress features!!!\n" );
		}
		fprintf( stderr, "Failed to delete %d features from %s\n",
			cinfo->feat_id_count, cinfo->curr_part_name );

		for ( i=0; i<cinfo->feat_id_count; i++ ) {
		    status = ProFeatureInit( ProMdlToSolid(model),
			    cinfo->feat_ids_to_delete[i],
			    &feat );
		    if ( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to get handle for id %d\n",
				cinfo->feat_ids_to_delete[i] );
			if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
			    fprintf(cinfo->logger, "Failed to get handle for id %d\n",
				    cinfo->feat_ids_to_delete[i] );
			}
		    } else {
			status = ProFeatureStatusGet( &feat, &feat_stat );
			if ( status != PRO_TK_NO_ERROR ) {
			    fprintf( stderr,
				    "Failed to get status for feature %d\n",
				    cinfo->feat_ids_to_delete[i] );
			    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
				fprintf(cinfo->logger,
					"Failed to get status for feature %d\n",
					cinfo->feat_ids_to_delete[i] );
			    }
			} else {
			    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
				if ( feat_stat < 0 ) {
				    fprintf(cinfo->logger,
					    "invalid feature (%d)\n",
					    cinfo->feat_ids_to_delete[i] );
				} else {
				    fprintf(cinfo->logger, "feat %d status = %s\n",
					    cinfo->feat_ids_to_delete[i],
					    feat_status_to_str(feat_stat) );
				}
			    }
			    if ( feat_stat == PRO_FEAT_SUPPRESSED ) {
				/* unsuppress this one */
				if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
				    fprintf(cinfo->logger,
					    "Unsuppressing feature %d\n",
					    cinfo->feat_ids_to_delete[i] );
				}
				status = ProFeatureResume( ProMdlToSolid(model),
					&cinfo->feat_ids_to_delete[i],
					1, resume_opts, 1 );
				if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
				    if ( status == PRO_TK_NO_ERROR ) {
					fprintf(cinfo->logger,
						"\tfeature id %d unsuppressed\n",
						cinfo->feat_ids_to_delete[i] );
				    } else if ( status == PRO_TK_SUPP_PARENTS ) {
					fprintf(cinfo->logger,
						"\tsuppressed parents for feature %d not found\n",
						cinfo->feat_ids_to_delete[i] );

				    } else {
					fprintf(cinfo->logger,
						"\tfeature id %d unsuppression failed\n",
						cinfo->feat_ids_to_delete[i] );
				    }
				}
			    }
			}
		    }
		}

		cinfo->feat_id_count = 0;
		free_csg_ops(cinfo);
	    } else {
		fprintf( stderr, "features suppressed!!\n" );
	    }
	}
    }

    /* can get bounding box of a solid using "ProSolidOutlineGet"
     * may want to use this to implement relative facetization tolerance
     */

    /* tessellate part */
    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Tessellate part (%s)\n", cinfo->curr_part_name );
    }

    /* Going from coarse to fine tessellation */
    for (i = 0; i <= cinfo->max_to_min_steps; ++i) {
	curr_error = cinfo->max_error - (i * cinfo->error_increment);
	curr_angle = cinfo->min_angle_cntrl + (i * cinfo->angle_increment);

	if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf(cinfo->logger, "max_error = %g, min_error - %g, error_increment - %g\n", cinfo->max_error, cinfo->min_error, cinfo->error_increment);
	    fprintf(cinfo->logger, "max_angle_cntrl = %g, min_angle_cntrl - %g, angle_increment - %g\n", cinfo->max_angle_cntrl, cinfo->min_angle_cntrl, cinfo->angle_increment);
	    fprintf(cinfo->logger, "curr_error = %g, curr_angle - %g\n", curr_error, curr_angle);
	    fprintf(cinfo->logger, "Trying to tessellate %s using:  tessellation error - %g, angle - %g\n", cinfo->curr_part_name, curr_error, curr_angle);
	}

	status = ProPartTessellate( ProMdlToPart(model), curr_error/cinfo->creo_to_brl_conv,
		curr_angle, PRO_B_TRUE, &tess  );

	if ( status == PRO_TK_NO_ERROR )
	    break;

	if (cinfo->logger_type == LOGGER_TYPE_ALL ||cinfo->logger_type == LOGGER_TYPE_FAILURE ||cinfo->logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) {
	    fprintf(cinfo->logger, "Failed to tessellate %s using:  tessellation error - %g, angle - %g\n", cinfo->curr_part_name, curr_error, curr_angle);
	}
    }

    if ( status != PRO_TK_NO_ERROR ) {
	/* Failed!!! */

	if (cinfo->logger_type == LOGGER_TYPE_ALL ||cinfo->logger_type == LOGGER_TYPE_FAILURE ||cinfo->logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) {
	    fprintf(cinfo->logger, "Failed to tessellate %s!!!\n", cinfo->curr_part_name );
	}
	snprintf( astr, sizeof(astr), "Failed to tessellate part (%s)", cinfo->curr_part_name);
	(void)ProMessageDisplay(msgfil, "USER_ERROR", astr );
	ProMessageClear();
	fprintf( stderr, "%s\n", astr );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	add_to_empty_list(cinfo, get_brlcad_name(cinfo, cinfo->curr_part_name ) );
	ret = 1;
    } else if ( !tess ) {
	/* not a failure, just an empty part */
	if (cinfo->logger_type == LOGGER_TYPE_ALL ||cinfo->logger_type == LOGGER_TYPE_SUCCESS ||cinfo->logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) {
	    fprintf(cinfo->logger, "Empty part. (%s) has no surfaces!!!\n", cinfo->curr_part_name );
	}
	snprintf( astr, sizeof(astr), "%s has no surfaces, ignoring", cinfo->curr_part_name );
	(void)ProMessageDisplay(msgfil, "USER_WARNING", astr );
	ProMessageClear();
	fprintf( stderr, "%s\n", astr );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	add_to_empty_list(cinfo, get_brlcad_name(cinfo, cinfo->curr_part_name ) );
	ret = 2;
    } else {
	/* output the triangles */
	int surface_count;

	if (cinfo->logger_type == LOGGER_TYPE_ALL ||cinfo->logger_type == LOGGER_TYPE_SUCCESS ||cinfo->logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) {
	    fprintf(cinfo->logger, "Successfully tessellated %s using: tessellation error - %g, angle - %g!!!\n",
		    cinfo->curr_part_name, curr_error, curr_angle );
	}

	status = ProArraySizeGet( (ProArray)tess, &surface_count );
	if ( status != PRO_TK_NO_ERROR ) {
	    (void)ProMessageDisplay(msgfil, "USER_ERROR", "Failed to get array size" );
	    ProMessageClear();
	    fprintf( stderr, "Failed to get array size\n" );
	    (void)ProWindowRefresh( PRO_VALUE_UNUSED );
	    ret = 1;
	} else if ( surface_count < 1 ) {
	    if (cinfo->logger_type == LOGGER_TYPE_ALL ||cinfo->logger_type == LOGGER_TYPE_SUCCESS ||cinfo->logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) {
		fprintf(cinfo->logger, "Empty part. (%s) has no surfaces!!!\n", cinfo->curr_part_name );
	    }
	    snprintf( astr, sizeof(astr), "%s has no surfaces, ignoring", cinfo->curr_part_name );
	    (void)ProMessageDisplay(msgfil, "USER_WARNING", astr );
	    ProMessageClear();
	    fprintf( stderr, "%s\n", astr );
	    (void)ProWindowRefresh( PRO_VALUE_UNUSED );
	    add_to_empty_list( cinfo, get_brlcad_name(cinfo, cinfo->curr_part_name ) );
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

	    cinfo->curr_tri = 0;
	    clean_vert_tree(cinfo->vert_tree_root);
	    clean_vert_tree(cinfo->norm_tree_root);

	    /* add all vertices and triangles to our lists */
	    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
		fprintf(cinfo->logger, "Processing surfaces of part %s\n", cinfo->curr_part_name );
	    }
	    for ( surfno=0; surfno<surface_count; surfno++ ) {
		for ( i=0; i<tess[surfno].n_facets; i++ ) {
		    /* grab the triangle */
		    vert_no = tess[surfno].facets[i][0];
		    v1 = Add_vert( tess[surfno].vertices[vert_no][0], tess[surfno].vertices[vert_no][1],
			    tess[surfno].vertices[vert_no][2], cinfo->vert_tree_root, cinfo->local_tol_sq );
		    vert_no = tess[surfno].facets[i][1];
		    v2 = Add_vert( tess[surfno].vertices[vert_no][0], tess[surfno].vertices[vert_no][1],
			    tess[surfno].vertices[vert_no][2], cinfo->vert_tree_root, cinfo->local_tol_sq );
		    vert_no = tess[surfno].facets[i][2];
		    v3 = Add_vert( tess[surfno].vertices[vert_no][0], tess[surfno].vertices[vert_no][1],
			    tess[surfno].vertices[vert_no][2], cinfo->vert_tree_root, cinfo->local_tol_sq );
		    if ( bad_triangle(cinfo, v1, v2, v3 ) ) {
			continue;
		    }

		    if ( !cinfo->get_normals ) {
			add_triangle(cinfo, v1, v2, v3 );
			continue;
		    }

		    /* grab the surface normals */
		    vert_no = tess[surfno].facets[i][0];
		    VUNITIZE( tess[surfno].normals[vert_no] );
		    n1 = Add_vert( tess[surfno].normals[vert_no][0], tess[surfno].normals[vert_no][1],
			    tess[surfno].normals[vert_no][2], cinfo->norm_tree_root, cinfo->local_tol_sq );
		    vert_no = tess[surfno].facets[i][1];
		    VUNITIZE( tess[surfno].normals[vert_no] );
		    n2 = Add_vert( tess[surfno].normals[vert_no][0], tess[surfno].normals[vert_no][1],
			    tess[surfno].normals[vert_no][2], cinfo->norm_tree_root, cinfo->local_tol_sq );
		    vert_no = tess[surfno].facets[i][2];
		    VUNITIZE( tess[surfno].normals[vert_no] );
		    n3 = Add_vert( tess[surfno].normals[vert_no][0], tess[surfno].normals[vert_no][1],
			    tess[surfno].normals[vert_no][2], cinfo->norm_tree_root, cinfo->local_tol_sq );

		    add_triangle_and_normal(cinfo, v1, v2, v3, n1, n2, n3 );
		}
	    }

	    /* actually output the part */
	    /* first the BOT solid with a made-up name */
	    brl_name = get_brlcad_name(cinfo, cinfo->curr_part_name );
	    sol_name = (char *)bu_malloc( strlen( brl_name ) + 3, "aol_name" );
	    snprintf( sol_name, strlen(brl_name)+3, "s.%s", brl_name );
	    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
		fprintf(cinfo->logger, "Creating bot primitive (%s) for part %s\n",
			sol_name, brl_name );
	    }

	    fprintf( cinfo->outfp, "put {%s} bot mode volume orient no V { ", sol_name );

	    for ( i=0; i < cinfo->vert_tree_root->curr_vert; i++ ) {
		fprintf( cinfo->outfp, " {%.12e %.12e %.12e}",
			cinfo->vert_tree_root->the_array[i*3] * cinfo->creo_to_brl_conv,
			cinfo->vert_tree_root->the_array[i*3+1] * cinfo->creo_to_brl_conv,
			cinfo->vert_tree_root->the_array[i*3+2] * cinfo->creo_to_brl_conv );
	    }
	    fprintf( cinfo->outfp, " } F {" );
	    for ( i=0; i < cinfo->curr_tri; i++ ) {
		/* Proe orders things using left-hand rule, so reverse the order */
		fprintf( cinfo->outfp, " {%d %d %d}", cinfo->part_tris[i][2],
			cinfo->part_tris[i][1], cinfo->part_tris[i][0] );
	    }
	    if ( cinfo->get_normals ) {
		if (cinfo->logger ) {
		    fprintf(cinfo->logger, "Getting vertex normals for part %s\n",
			    cinfo->curr_part_name );
		}
		fprintf( cinfo->outfp, " } flags { has_normals use_normals } N {" );
		for ( i=0; i<cinfo->norm_tree_root->curr_vert; i++ ) {
		    fprintf( cinfo->outfp, " {%.12e %.12e %.12e}",
			    cinfo->norm_tree_root->the_array[i*3] * cinfo->creo_to_brl_conv,
			    cinfo->norm_tree_root->the_array[i*3+1] * cinfo->creo_to_brl_conv,
			    cinfo->norm_tree_root->the_array[i*3+2] * cinfo->creo_to_brl_conv );
		}
		fprintf( cinfo->outfp, " } fn {" );
		for ( i=0; i < cinfo->curr_tri; i++ ) {
		    fprintf( cinfo->outfp, " {%d %d %d}", cinfo->part_norms[i*3],
			    cinfo->part_norms[i*3+1], cinfo->part_norms[i*3+2] );
		}
	    }
	    fprintf( cinfo->outfp, " }\n" );

	    /* Set the CREO_NAME attributes for the solid/primitive */
	    fprintf( cinfo->outfp, "attr set {%s} %s %s\n", sol_name, CREO_NAME_ATTR, cinfo->curr_part_name );

	    /* build the tree for this region */
	    build_tree(cinfo, sol_name, &tree );
	    bu_free( sol_name, "sol_name" );
	    output_csg_prims(cinfo);

	    /* get the surface properties for the part
	     * and create a region using the actual part name
	     */
	    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
		fprintf(cinfo->logger, "Creating region for part %s\n", cinfo->curr_part_name );
	    }


	    ProMdlToModelitem(model, &mitm);
	    ProError aerr = ProSurfaceAppearancepropsGet(&mitm, &aprops);
	    if (aerr  == PRO_TK_NOT_EXIST ) {
		/* no surface properties */
		fprintf( cinfo->outfp,
			"put {%s} comb region yes id %d los 100 GIFTmater 1 tree %s\n",
			get_brlcad_name(cinfo, cinfo->curr_part_name ), cinfo->reg_id, bu_vls_addr( &tree) );
	    } else if ( aerr == PRO_TK_NO_ERROR ) {
		/* use the colors, ... that was set in Pro/E */
		fprintf( cinfo->outfp,
			"put {%s} comb region yes id %d los 100 GIFTmater 1 rgb {%d %d %d} shader {plastic {",
			get_brlcad_name(cinfo, cinfo->curr_part_name ),
			cinfo->reg_id,
			(int)(aprops.color_rgb[0]*255.0),
			(int)(aprops.color_rgb[1]*255.0),
			(int)(aprops.color_rgb[2]*255.0) );
		if ( aprops.transparency != 0.0 ) {
		    fprintf( cinfo->outfp, " tr %g", aprops.transparency );
		}
		if ( aprops.shininess != 1.0 ) {
		    fprintf( cinfo->outfp, " sh %d", (int)(aprops.shininess * 18 + 2.0) );
		}
		if ( aprops.diffuse != 0.3 ) {
		    fprintf( cinfo->outfp, " di %g", aprops.diffuse );
		}
		if ( aprops.highlite != 0.7 ) {
		    fprintf( cinfo->outfp, " sp %g", aprops.highlite );
		}
		fprintf( cinfo->outfp, "} }" );
		fprintf( cinfo->outfp, " tree %s\n", bu_vls_addr( &tree ) );
	    } else {
		/* something is wrong, but just ignore the missing properties */
		fprintf( stderr, "Error getting surface properties for %s\n",
			cinfo->curr_part_name );
		fprintf( cinfo->outfp, "put {%s} comb region yes id %d los 100 GIFTmater 1 tree %s\n",
			get_brlcad_name(cinfo, cinfo->curr_part_name ), cinfo->reg_id, bu_vls_addr( &tree ) );
	    }

	    /* Set the CREO_NAME attributes for the region */
	    fprintf( cinfo->outfp, "attr set {%s} %s %s\n", get_brlcad_name(cinfo, cinfo->curr_part_name ), CREO_NAME_ATTR, cinfo->curr_part_name );

	    /* if the part has a material, add it as an attribute */
	    got_density = 0;
	    status = ProPartMaterialNameGet( ProMdlToPart(model), material );
	    if ( status == PRO_TK_NO_ERROR ) {
		fprintf( cinfo->outfp, "attr set {%s} material_name {%s}\n",
			get_brlcad_name(cinfo, cinfo->curr_part_name ),
			ProWstringToString( str, material ) );

		/* get the density for this material */
		status = ProPartMaterialdataGet( ProMdlToPart(model), material, &material_props );
		if ( status == PRO_TK_NO_ERROR ) {
		    got_density = 1;
		    fprintf( cinfo->outfp, "attr set {%s} density %g\n",
			    get_brlcad_name(cinfo, cinfo->curr_part_name ),
			    material_props.mass_density );
		}
	    }

	    /* calculate mass properties */
	    status = ProSolidMassPropertyGet( ProMdlToSolid( model ), NULL, &mass_prop );
	    if ( status == PRO_TK_NO_ERROR ) {
		if ( !got_density ) {
		    if ( mass_prop.density > 0.0 ) {
			fprintf( cinfo->outfp, "attr set {%s} density %g\n",
				get_brlcad_name(cinfo, cinfo->curr_part_name ),
				mass_prop.density );
		    }
		}
		if ( mass_prop.mass > 0.0 ) {
		    fprintf( cinfo->outfp, "attr set {%s} mass %g\n",
			    get_brlcad_name(cinfo, cinfo->curr_part_name ),
			    mass_prop.mass );
		}
		if ( mass_prop.volume > 0.0 ) {
		    fprintf( cinfo->outfp, "attr set {%s} volume %g\n",
			    get_brlcad_name(cinfo, cinfo->curr_part_name ),
			    mass_prop.volume );
		}
	    }

	    /* increment the region id */
	    cinfo->reg_id++;

	    /* free the tree */
	    bu_vls_free( &tree );
	}
    }


    /* free the tessellation memory */
    ProPartTessellationFree( &tess );
    tess = NULL;

    /* add this part to the list of objects already output */
    add_to_done_part(cinfo, part_name );

    /* unsuppress anything we suppressed */
    if ( cinfo->feat_id_count ) {
	if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf(cinfo->logger, "Unsuppressing %d features\n", cinfo->feat_id_count );
	}
	fprintf( stderr, "Unsuppressing %d features\n", cinfo->feat_id_count );
	if ( (ret=ProFeatureResume( ProMdlToSolid(model),
			cinfo->feat_ids_to_delete, cinfo->feat_id_count,
			NULL, 0 )) != PRO_TK_NO_ERROR) {

	    fprintf( stderr, "Failed to unsuppress %d features from %s\n",
		    cinfo->feat_id_count, cinfo->curr_part_name );

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
		    "changes so that this problem will not persist.", cinfo->feat_id_count, cinfo->curr_part_name );

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
	    cinfo->feat_id_count = 0;
	    return 0;
	}
	fprintf( stderr, "features unsuppressed!!\n" );
	cinfo->feat_id_count = 0;
    }

    return ret;
}

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
