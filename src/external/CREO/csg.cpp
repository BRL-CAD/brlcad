/*                   C S G . C P P
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
/** @file csg.cpp
 *
 */

#include "common.h"
#include "creo-brl.h"

/* structure to hold info about CSG operations for current part */
struct csg_ops {
    char op;
    struct bu_vls name;
    struct bu_vls dbput;
    struct csg_ops *next;
};

struct csg_ops *csg_root;
static int hole_no=0;	/* hole counter for unique names */
static char *tgc_format="tgc V {%.25G %.25G %.25G} H {%.25G %.25G %.25G} A {%.25G %.25G %.25G} B {%.25G %.25G %.25G} C {%.25G %.25G %.25G} D {%.25G %.25G %.25G}\n";

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


extern "C" void
Add_to_feature_delete_list( int id )
{
    if ( feat_id_count >= feat_id_len ) {
	feat_id_len += FEAT_ID_BLOCK;
	feat_ids_to_delete = (int *)bu_realloc( (char *)feat_ids_to_delete,
		feat_id_len * sizeof( int ),
		"feature ids to delete");

    }
    feat_ids_to_delete[feat_id_count++] = id;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Adding feature %d to list of features to delete (list length = %d)\n",
		id, feat_id_count );
    }
}

extern "C" ProError
geomitem_visit( ProGeomitem *item, ProError status, ProAppData data )
{
    ProGeomitemdata *geom;
    ProCurvedata *crv;
    ProError ret;

    if ( (ret=ProGeomitemdataGet( item, &geom )) != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get geomitem for type %d\n",
		item->type );
	return ret;
    }

    crv = PRO_CURVE_DATA( geom );
    if ( (ret=ProLinedataGet( crv, end1, end2 ) ) != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get line data for axis\n" );
	return ret;
    }

    return PRO_TK_NO_ERROR;
}

extern "C" ProError
geomitem_filter( ProGeomitem *item, ProAppData data )
{
    return PRO_TK_NO_ERROR;
}

extern "C" ProError
hole_elem_visit( ProElement elem_tree, ProElement elem, ProElempath elem_path, ProAppData data )
{
    ProError ret;
    ProElemId elem_id;
    ProValue val_junk;
    ProValueData val;

    if ( (ret=ProElementIdGet( elem, &elem_id ) ) != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get element id!!!\n" );
	return ret;
    }

    switch ( elem_id ) {
	case PRO_E_HLE_ADD_CBORE:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    add_cbore = val.v.i;
	    break;
	case PRO_E_HLE_ADD_CSINK:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    add_csink = val.v.i;
	    break;
	case PRO_E_DIAMETER:
	    /* diameter of straight hole */
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    hole_diam = val.v.d;
	    break;
	case PRO_E_HLE_HOLEDIAM:
	    /* diameter of main portion of standard drilled hole */
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    hole_diam = val.v.d;
	    break;
	case PRO_E_HLE_CBOREDEPTH:
	    /* depth of counterbore */
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    cb_depth = val.v.d;
	    break;
	case PRO_E_HLE_CBOREDIAM:
	    /* diameter of counterbore */
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    cb_diam = val.v.d;
	    break;
	case PRO_E_HLE_CSINKANGLE:
	    /* angle of countersink (degrees ) */
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    cs_angle = val.v.d;
	    break;
	case PRO_E_HLE_CSINKDIAM:
	    /* diameter of countersink */
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    cs_diam = val.v.d;
	    break;
	case PRO_E_HLE_DRILLDEPTH:
	    /* overall depth of standard drilled hole without drill tip */
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    hole_depth = val.v.d;
	    break;
	case PRO_E_HLE_DRILLANGLE:
	    /* drill tip angle (degrees) */
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    drill_angle = val.v.d;
	    break;
	case PRO_E_HLE_DEPTH:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    hole_depth_type = val.v.i;
	    break;
	case PRO_E_HLE_TYPE_NEW:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    hole_type = val.v.i;
	    break;
	case PRO_E_HLE_STAN_TYPE:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    break;
	case PRO_E_STD_EDGE_CHAMF_DIM1:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    break;
	case PRO_E_STD_EDGE_CHAMF_DIM2:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    break;
	case PRO_E_STD_EDGE_CHAMF_ANGLE:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    break;
	case PRO_E_STD_EDGE_CHAMF_DIM:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    break;
	case PRO_E_STD_EDGE_CHAMF_SCHEME:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    break;
	case PRO_E_EXT_DEPTH_FROM_VALUE:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    break;
	case PRO_E_EXT_DEPTH_TO_VALUE:
	    if ( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value\n" );
		return ret;
	    }
	    if ( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get value data\n" );
		return ret;
	    }
	    break;
    }

    return PRO_TK_NO_ERROR;
}

extern "C" ProError
hole_elem_filter( ProElement elem_tree, ProElement elem, ProElempath elem_path, ProAppData data )
{
    return PRO_TK_NO_ERROR;
}

/* Subtract_hole()
 *	routine to create TGC primitives to make holes
 *
 *	return value:
 *		0 - do not delete this hole feature before tessellating
 *		1 - delete this hole feature before tessellating
 */
extern "C" int
Subtract_hole()
{
    struct csg_ops *csg;
    vect_t a, b, c, d, h;

    if ( do_facets_only ) {
	if ( diameter < min_hole_diameter )
	    return 1;
	else
	    return 0;
    }

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Doing a CSG hole subtraction\n" );
    }

    /* make a replacement hole using CSG */
    if ( hole_type == PRO_HLE_NEW_TYPE_STRAIGHT ) {
	/* plain old straight hole */

	if ( diameter < min_hole_diameter )
	    return 1;
	if ( !csg_root ) {
	    BU_ALLOC(csg_root, struct csg_ops);
	    csg = csg_root;
	    csg->next = NULL;
	} else {
	    BU_ALLOC(csg, struct csg_ops);
	    csg->next = csg_root;
	    csg_root = csg;
	}
	bu_vls_init( &csg->name );
	bu_vls_init( &csg->dbput );

	csg->op = '-';
	hole_no++;
	bu_vls_printf( &csg->name, "hole.%d ", hole_no );
	VSUB2( h, end1, end2 );
	bn_vec_ortho( a, h );
	VCROSS( b, a, h );
	VUNITIZE( b );
	VSCALE( end2, end2, creo_to_brl_conv );
	VSCALE( a, a, radius*creo_to_brl_conv );
	VSCALE( b, b, radius*creo_to_brl_conv );
	VSCALE( h, h, creo_to_brl_conv );
	bu_vls_printf( &csg->dbput, tgc_format,
		V3ARGS( end2 ),
		V3ARGS( h ),
		V3ARGS( a ),
		V3ARGS( b ),
		V3ARGS( a ),
		V3ARGS( b ) );
    } else if ( hole_type == PRO_HLE_NEW_TYPE_STANDARD ) {
	/* drilled hole with possible countersink and counterbore */
	point_t start;
	vect_t dir;
	double cb_radius;
	double accum_depth=0.0;
	double hole_radius=hole_diam / 2.0;

	if ( hole_diam < min_hole_diameter )
	    return 1;

	VSUB2( dir, end1, end2 );
	VUNITIZE( dir );

	VMOVE( start, end2 );
	VSCALE( start, start, creo_to_brl_conv );

	if ( add_cbore == PRO_HLE_ADD_CBORE ) {

	    if ( !csg_root ) {
		BU_ALLOC(csg_root, struct csg_ops);
		csg = csg_root;
		csg->next = NULL;
	    } else {
		BU_ALLOC(csg, struct csg_ops);
		csg->next = csg_root;
		csg_root = csg;
	    }
	    bu_vls_init( &csg->name );
	    bu_vls_init( &csg->dbput );

	    csg->op = '-';
	    hole_no++;
	    bu_vls_printf( &csg->name, "hole.%d ", hole_no );
	    bn_vec_ortho( a, dir );
	    VCROSS( b, a, dir );
	    VUNITIZE( b );
	    cb_radius = cb_diam * creo_to_brl_conv / 2.0;
	    VSCALE( a, a, cb_radius );
	    VSCALE( b, b, cb_radius );
	    VSCALE( h, dir, cb_depth * creo_to_brl_conv );
	    bu_vls_printf( &csg->dbput, tgc_format,
		    V3ARGS( start ),
		    V3ARGS( h ),
		    V3ARGS( a ),
		    V3ARGS( b ),
		    V3ARGS( a ),
		    V3ARGS( b ) );
	    VADD2( start, start, h );
	    accum_depth += cb_depth;
	    cb_diam = 0.0;
	    cb_depth = 0.0;
	}
	if ( add_csink == PRO_HLE_ADD_CSINK ) {
	    double cs_depth;
	    double cs_radius=cs_diam / 2.0;

	    if ( !csg_root ) {
		BU_ALLOC(csg_root, struct csg_ops);
		csg = csg_root;
		csg->next = NULL;
	    } else {
		BU_ALLOC(csg, struct csg_ops);
		csg->next = csg_root;
		csg_root = csg;
	    }
	    bu_vls_init( &csg->name );
	    bu_vls_init( &csg->dbput );

	    csg->op = '-';
	    hole_no++;
	    bu_vls_printf( &csg->name, "hole.%d ", hole_no );
	    cs_depth = (cs_diam - hole_diam) / (2.0 * tan( cs_angle * M_PI / 360.0 ) );
	    bn_vec_ortho( a, dir );
	    VCROSS( b, a, dir );
	    VUNITIZE( b );
	    VMOVE( c, a );
	    VMOVE( d, b );
	    VSCALE( h, dir, cs_depth * creo_to_brl_conv );
	    VSCALE( a, a, cs_radius * creo_to_brl_conv );
	    VSCALE( b, b, cs_radius * creo_to_brl_conv );
	    VSCALE( c, c, hole_diam * creo_to_brl_conv / 2.0 );
	    VSCALE( d, d, hole_diam * creo_to_brl_conv / 2.0 );
	    bu_vls_printf( &csg->dbput, tgc_format,
		    V3ARGS( start ),
		    V3ARGS( h ),
		    V3ARGS( a ),
		    V3ARGS( b ),
		    V3ARGS( c ),
		    V3ARGS( d ) );
	    VADD2( start, start, h );
	    accum_depth += cs_depth;
	    cs_diam = 0.0;
	    cs_angle = 0.0;
	}

	if ( !csg_root ) {
	    BU_ALLOC(csg_root, struct csg_ops);
	    csg = csg_root;
	    csg->next = NULL;
	} else {
	    BU_ALLOC(csg, struct csg_ops);
	    csg->next = csg_root;
	    csg_root = csg;
	}
	bu_vls_init( &csg->name );
	bu_vls_init( &csg->dbput );

	csg->op = '-';
	hole_no++;
	bu_vls_printf( &csg->name, "hole.%d ", hole_no );
	bn_vec_ortho( a, dir );
	VCROSS( b, a, dir );
	VUNITIZE( b );
	VMOVE( c, a );
	VMOVE( d, b );
	VSCALE( a, a, hole_radius * creo_to_brl_conv );
	VSCALE( b, b, hole_radius * creo_to_brl_conv );
	VSCALE( c, c, hole_radius * creo_to_brl_conv );
	VSCALE( d, d, hole_radius * creo_to_brl_conv );
	VSCALE( h, dir, (hole_depth - accum_depth) * creo_to_brl_conv );
	bu_vls_printf( &csg->dbput, tgc_format,
		V3ARGS( start ),
		V3ARGS( h ),
		V3ARGS( a ),
		V3ARGS( b ),
		V3ARGS( c ),
		V3ARGS( d ) );
	VADD2( start, start, h );
	hole_diam = 0.0;
	hole_depth = 0.0;
	if ( hole_depth_type == PRO_HLE_STD_VAR_DEPTH ) {
	    double tip_depth;

	    if ( !csg_root ) {
		BU_ALLOC(csg_root, struct csg_ops);
		csg = csg_root;
		csg->next = NULL;
	    } else {
		BU_ALLOC(csg, struct csg_ops);
		csg->next = csg_root;
		csg_root = csg;
	    }
	    bu_vls_init( &csg->name );
	    bu_vls_init( &csg->dbput );

	    csg->op = '-';
	    hole_no++;
	    bu_vls_printf( &csg->name, "hole.%d ", hole_no );
	    bn_vec_ortho( a, dir );
	    VCROSS( b, a, dir );
	    VUNITIZE( b );
	    VMOVE( c, a );
	    VMOVE( d, b );
	    tip_depth = (hole_radius - MIN_RADIUS) / tan( drill_angle * M_PI / 360.0 );
	    VSCALE( h, dir, tip_depth * creo_to_brl_conv );
	    VSCALE( a, a, hole_radius * creo_to_brl_conv );
	    VSCALE( b, b, hole_radius * creo_to_brl_conv );
	    VSCALE( c, c, MIN_RADIUS );
	    VSCALE( d, d, MIN_RADIUS );
	    bu_vls_printf( &csg->dbput, tgc_format,
		    V3ARGS( start ),
		    V3ARGS( h ),
		    V3ARGS( a ),
		    V3ARGS( b ),
		    V3ARGS( c ),
		    V3ARGS( d ) );
	    drill_angle = 0.0;
	}
    } else {
	fprintf( stderr, "Unrecognized hole type\n" );
	return 0;
    }

    return 1;
}

extern "C" ProError
do_feature_visit( ProFeature *feat, ProError status, ProAppData data )
{
    ProError ret;
    ProElement elem_tree;
    ProElempath elem_path=NULL;

    if ( (ret=ProFeatureElemtreeCreate( feat, &elem_tree ) ) == PRO_TK_NO_ERROR ) {
	if ( (ret=ProElemtreeElementVisit( elem_tree, elem_path,
			hole_elem_filter, hole_elem_visit,
			data ) ) != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Element visit failed for feature (%d) of %s\n",
		    feat->id, curr_part_name );
	    if ( ProElementFree( &elem_tree ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Error freeing element tree\n" );
	    }
	    return ret;
	}
	if ( ProElementFree( &elem_tree ) != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Error freeing element tree\n" );
	}
    }

    radius = 0.0;
    diameter = 0.0;
    distance1 = 0.0;
    distance2 = 0.0;
    got_diameter = 0;
    got_distance1 = 0;

    if ( (ret=ProFeatureDimensionVisit( feat, check_dimension, dimension_filter, data ) ) !=
	    PRO_TK_NO_ERROR ) {
	return ret;
    }

    if ( curr_feat_type == PRO_FEAT_HOLE ) {
	/* need more info to recreate holes */
	if ( (ret=ProFeatureGeomitemVisit( feat, PRO_AXIS, geomitem_visit,
			geomitem_filter, data ) ) != PRO_TK_NO_ERROR ) {
	    return ret;
	}
    }

    switch ( curr_feat_type ) {
	case PRO_FEAT_HOLE:
	    if ( Subtract_hole() )
		Add_to_feature_delete_list( feat->id );
	    break;
	case PRO_FEAT_ROUND:
	    if ( got_diameter && radius < min_round_radius ) {
		Add_to_feature_delete_list( feat->id );
	    }
	    break;
	case PRO_FEAT_CHAMFER:
	    if ( got_distance1 && distance1 < min_chamfer_dim &&
		    distance2 < min_chamfer_dim ) {
		Add_to_feature_delete_list( feat->id );
	    }
	    break;
    }


    return ret;
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

    free_csg_ops();		/* these are only holes */
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




/* routine to output the top level object that is currently displayed in Pro/E */
extern "C" void
output_top_level_object( ProMdl model, ProMdlType type )
{
    ProName name;
    ProCharName top_level;
    char buffer[1024] = {0};

    /* get its name */
    if ( ProMdlNameGet( model, name ) != PRO_TK_NO_ERROR ) {
	(void)ProMessageDisplay(MSGFIL, "USER_ERROR",
		"Could not get name for part!!" );
	ProMessageClear();
	fprintf( stderr, "Could not get name for part" );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	bu_strlcpy( curr_part_name, "noname", PRO_NAME_SIZE );
    } else {
	(void)ProWstringToString( curr_part_name, name );
    }

    /* save name */
    bu_strlcpy( top_level, curr_part_name, sizeof(top_level) );

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Output top level object (%s)\n", top_level );
    }

    /* output the object */
    if ( type == PRO_MDL_PART ) {
	/* tessellate part and output triangles */
	output_part( model );
    } else if ( type == PRO_MDL_ASSEMBLY ) {
	/* visit all members of assembly */
	output_assembly( model );
    } else {
	snprintf( astr, sizeof(astr), "Object %s is neither PART nor ASSEMBLY, skipping",
		curr_part_name );
	(void)ProMessageDisplay(MSGFIL, "USER_WARNING", astr );
	ProMessageClear();
	fprintf( stderr, "%s\n", astr );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
    }

    if ( type == PRO_MDL_ASSEMBLY ) {
	snprintf(buffer, 1024, "put $topname comb region no tree {l %s.c {0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1}}", get_brlcad_name(top_level) );
    } else {
	snprintf(buffer, 1024, "put $topname comb region no tree {l %s {0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1}}", get_brlcad_name(top_level) );
    }

    /* make a top level combination named "top", if there is not
     * already one.  if one does already exist, try "top.#" where
     * "#" is the first available number.  this combination
     * contains the xform to rotate the model into BRL-CAD
     * standard orientation.
     */
    fprintf(outfp,
	    "set topname \"top\"\n"
	    "if { ! [catch {get $topname} ret] } {\n"
	    "  set num 0\n"
	    "  while { $num < 1000 } {\n"
	    "    set topname \"top.$num\"\n"
	    "    if { [catch {get $name} ret ] } {\n"
	    "      break\n"
	    "    }\n"
	    "    incr num\n"
	    "  }\n"
	    "}\n"
	    "if { [catch {get $topname} ret] } {\n"
	    "  %s\n"
	    "}\n",
	    buffer
	   );
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
