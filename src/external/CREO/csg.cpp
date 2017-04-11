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

/* global variables for dimension visits */
static double radius=0.0, diameter=0.0, distance1=0.0, distance2=0.0;
static int got_diameter=0, got_distance1=0;
static int hole_type;
static int add_cbore;
static int add_csink;
static int hole_depth_type;

static double cb_depth=0.0;             /* counter-bore depth */
static double cb_diam=0.0;              /* counter-bore diam */
static double cs_diam=0.0;              /* counter-sink diam */
static double cs_angle=0.0;             /* counter-sink angle */
static double hole_diam=0.0;            /* drilled hle diameter */
static double hole_depth=0.0;           /* drilled hole depth */
static double drill_angle=0.0;          /* drill tip angle */
#define MIN_RADIUS      1.0e-7          /* BRL-CAD does not allow tgc's with zero radius */
static Pro3dPnt end1, end2;             /* axis endpoints for holes */
static int hole_no=0;	/* hole counter for unique names */
static char *tgc_format="tgc V {%.25G %.25G %.25G} H {%.25G %.25G %.25G} A {%.25G %.25G %.25G} B {%.25G %.25G %.25G} C {%.25G %.25G %.25G} D {%.25G %.25G %.25G}\n";


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


/* Subtract_hole()
 *	routine to create TGC primitives to make holes
 *
 *	return value:
 *		0 - do not delete this hole feature before tessellating
 *		1 - delete this hole feature before tessellating
 */
extern "C" int
subtract_hole(struct part_conv_info *pinfo)
{
    struct csg_ops *csg;
    vect_t a, b, c, d, h;

    if ( cinfo->do_facets_only ) {
	if ( diameter < cinfo->min_hole_diameter )
	    return 1;
	else
	    return 0;
    }

    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Doing a CSG hole subtraction\n" );
    }

    /* make a replacement hole using CSG */
    if ( hole_type == PRO_HLE_NEW_TYPE_STRAIGHT ) {
	/* plain old straight hole */

	if ( diameter < cinfo->min_hole_diameter )
	    return 1;
	if ( !cinfo->csg_root ) {
	    BU_ALLOC(cinfo->csg_root, struct csg_ops);
	    csg = cinfo->csg_root;
	    csg->next = NULL;
	} else {
	    BU_ALLOC(csg, struct csg_ops);
	    csg->next = cinfo->csg_root;
	    cinfo->csg_root = csg;
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
	VSCALE( end2, end2, cinfo->creo_to_brl_conv );
	VSCALE( a, a, radius*cinfo->creo_to_brl_conv );
	VSCALE( b, b, radius*cinfo->creo_to_brl_conv );
	VSCALE( h, h, cinfo->creo_to_brl_conv );
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

	if ( hole_diam < cinfo->min_hole_diameter )
	    return 1;

	VSUB2( dir, end1, end2 );
	VUNITIZE( dir );

	VMOVE( start, end2 );
	VSCALE( start, start, cinfo->creo_to_brl_conv );

	if ( add_cbore == PRO_HLE_ADD_CBORE ) {

	    if ( !cinfo->csg_root ) {
		BU_ALLOC(cinfo->csg_root, struct csg_ops);
		csg = cinfo->csg_root;
		csg->next = NULL;
	    } else {
		BU_ALLOC(csg, struct csg_ops);
		csg->next = cinfo->csg_root;
		cinfo->csg_root = csg;
	    }
	    bu_vls_init( &csg->name );
	    bu_vls_init( &csg->dbput );

	    csg->op = '-';
	    hole_no++;
	    bu_vls_printf( &csg->name, "hole.%d ", hole_no );
	    bn_vec_ortho( a, dir );
	    VCROSS( b, a, dir );
	    VUNITIZE( b );
	    cb_radius = cb_diam * cinfo->creo_to_brl_conv / 2.0;
	    VSCALE( a, a, cb_radius );
	    VSCALE( b, b, cb_radius );
	    VSCALE( h, dir, cb_depth * cinfo->creo_to_brl_conv );
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

	    if ( !cinfo->csg_root ) {
		BU_ALLOC(cinfo->csg_root, struct csg_ops);
		csg = cinfo->csg_root;
		csg->next = NULL;
	    } else {
		BU_ALLOC(csg, struct csg_ops);
		csg->next = cinfo->csg_root;
		cinfo->csg_root = csg;
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
	    VSCALE( h, dir, cs_depth * cinfo->creo_to_brl_conv );
	    VSCALE( a, a, cs_radius * cinfo->creo_to_brl_conv );
	    VSCALE( b, b, cs_radius * cinfo->creo_to_brl_conv );
	    VSCALE( c, c, hole_diam * cinfo->creo_to_brl_conv / 2.0 );
	    VSCALE( d, d, hole_diam * cinfo->creo_to_brl_conv / 2.0 );
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

	if ( !cinfo->csg_root ) {
	    BU_ALLOC(cinfo->csg_root, struct csg_ops);
	    csg = cinfo->csg_root;
	    csg->next = NULL;
	} else {
	    BU_ALLOC(csg, struct csg_ops);
	    csg->next = cinfo->csg_root;
	    cinfo->csg_root = csg;
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
	VSCALE( a, a, hole_radius * cinfo->creo_to_brl_conv );
	VSCALE( b, b, hole_radius * cinfo->creo_to_brl_conv );
	VSCALE( c, c, hole_radius * cinfo->creo_to_brl_conv );
	VSCALE( d, d, hole_radius * cinfo->creo_to_brl_conv );
	VSCALE( h, dir, (hole_depth - accum_depth) * cinfo->creo_to_brl_conv );
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

	    if ( !cinfo->csg_root ) {
		BU_ALLOC(cinfo->csg_root, struct csg_ops);
		csg = cinfo->csg_root;
		csg->next = NULL;
	    } else {
		BU_ALLOC(csg, struct csg_ops);
		csg->next = cinfo->csg_root;
		cinfo->csg_root = csg;
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
	    VSCALE( h, dir, tip_depth * cinfo->creo_to_brl_conv );
	    VSCALE( a, a, hole_radius * cinfo->creo_to_brl_conv );
	    VSCALE( b, b, hole_radius * cinfo->creo_to_brl_conv );
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
