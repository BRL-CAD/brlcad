/*                   C S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017-2020 United States Government as represented by
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

#define MIN_RADIUS      1.0e-7          /* BRL-CAD does not allow tgc's with zero radius */

/* Information needed to replace holes with CSG */
struct hole_info {
    ProFeature *feat;
    double radius;
    double diameter;
    int hole_type;
    int add_cbore;
    int add_csink;
    int hole_depth_type;
    double cb_depth;             /* counter-bore depth */
    double cb_diam;              /* counter-bore diam */
    double cs_diam;              /* counter-sink diam */
    double cs_angle;             /* counter-sink angle */
    double hole_diam;            /* drilled hle diameter */
    double hole_depth;           /* drilled hole depth */
    double drill_angle;          /* drill tip angle */
    Pro3dPnt end1, end2;         /* axis endpoints for holes */
};

extern "C" ProError
hole_elem_filter(ProElement UNUSED(elem_tree), ProElement UNUSED(elem), ProElempath UNUSED(elem_path), ProAppData UNUSED(data))
{
    return PRO_TK_NO_ERROR;
}

extern "C" ProError
hole_elem_visit(ProElement UNUSED(elem_tree), ProElement elem, ProElempath UNUSED(elem_path), ProAppData data)
{
    ProError ret;
    ProElemId elem_id;
    ProValue val_junk;
    ProValueData val;
    struct hole_info *hinfo = (struct hole_info *)data;

    if ((ret = ProElementIdGet(elem, &elem_id)) != PRO_TK_NO_ERROR) return ret;

    switch (elem_id) {
	case PRO_E_HLE_ADD_CBORE:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->add_cbore = val.v.i;
	    break;
	case PRO_E_HLE_ADD_CSINK:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->add_csink = val.v.i;
	    break;
	case PRO_E_DIAMETER:
	    /* diameter of straight hole */
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->hole_diam = val.v.d;
	    break;
	case PRO_E_HLE_HOLEDIAM:
	    /* diameter of main portion of standard drilled hole */
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->hole_diam = val.v.d;
	    break;
	case PRO_E_HLE_CBOREDEPTH:
	    /* depth of counterbore */
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->cb_depth = val.v.d;
	    break;
	case PRO_E_HLE_CBOREDIAM:
	    /* diameter of counterbore */
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->cb_diam = val.v.d;
	    break;
	case PRO_E_HLE_CSINKANGLE:
	    /* angle of countersink (degrees) */
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->cs_angle = val.v.d;
	    break;
	case PRO_E_HLE_CSINKDIAM:
	    /* diameter of countersink */
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->cs_diam = val.v.d;
	    break;
	case PRO_E_HLE_DRILLDEPTH:
	    /* overall depth of standard drilled hole without drill tip */
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->hole_depth = val.v.d;
	    break;
	case PRO_E_HLE_DRILLANGLE:
	    /* drill tip angle (degrees) */
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->drill_angle = val.v.d;
	    break;
	case PRO_E_HLE_DEPTH:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->hole_depth_type = val.v.i;
	    break;
	case PRO_E_HLE_TYPE_NEW:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    hinfo->hole_type = val.v.i;
	    break;
	case PRO_E_HLE_STAN_TYPE:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    break;
	case PRO_E_STD_EDGE_CHAMF_DIM1:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    break;
	case PRO_E_STD_EDGE_CHAMF_DIM2:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    break;
	case PRO_E_STD_EDGE_CHAMF_ANGLE:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    break;
	case PRO_E_STD_EDGE_CHAMF_DIM:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    break;
	case PRO_E_STD_EDGE_CHAMF_SCHEME:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    break;
	case PRO_E_EXT_DEPTH_FROM_VALUE:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    break;
	case PRO_E_EXT_DEPTH_TO_VALUE:
	    if ((ret = ProElementValueGet(elem, &val_junk)) != PRO_TK_NO_ERROR) return ret;
	    if ((ret = ProValueDataGet(val_junk, &val)) != PRO_TK_NO_ERROR) return ret;
	    break;
    }

    return PRO_TK_NO_ERROR;
}


extern "C" ProError
geomitem_filter(ProDimension *UNUSED(dim), ProAppData UNUSED(data)) {
    return PRO_TK_NO_ERROR;
}

extern "C" ProError
geomitem_visit(ProGeomitem *item, ProError UNUSED(status), ProAppData data)
{
    ProGeomitemdata *geom;
    ProCurvedata *crv;
    ProError ret;
    struct hole_info *hinfo = (struct hole_info *)data;

    if ((ret=ProGeomitemdataGet(item, &geom)) != PRO_TK_NO_ERROR) return ret;
    crv = PRO_CURVE_DATA(geom);
    if ((ret=ProLinedataGet(crv, hinfo->end1, hinfo->end2)) != PRO_TK_NO_ERROR) return ret;
    return PRO_TK_NO_ERROR;
}

extern "C" struct bu_vls *
tgc_hole_name(struct creo_conv_info *cinfo, wchar_t *wname, const char *suffix)
{
    struct directory *dp;
    char pname[CREO_NAME_MAX];
    struct bu_vls *cname;
    struct bu_vls *hname;
    long count = 0;
    BU_GET(hname, struct bu_vls);
    bu_vls_init(hname);
    cname = get_brlcad_name(cinfo, wname, NULL, N_CREO);
    bu_vls_sprintf(hname, "%s_hole_0.%s", bu_vls_addr(cname), suffix);
    while ((dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(hname), LOOKUP_QUIET)) != RT_DIR_NULL) {
	(void)bu_vls_incr(hname, NULL, "0:0:0:0:-", NULL, NULL);
	count++;
	creo_log(cinfo, MSG_DEBUG, "\t trying hole name : %s\n", bu_vls_addr(hname));
	if (count == LONG_MAX) {
	    bu_vls_free(hname);
	    BU_PUT(hname, struct bu_vls);
	    ProWstringToString(pname, wname);
	    creo_log(cinfo, MSG_DEBUG, "%s: hole name generation FAILED.\n", pname);
	    return NULL;
	}
    }
    return hname;
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
    wchar_t wname[CREO_NAME_MAX];
    struct bu_vls *hname;
    ProError ret;
    ProElement elem_tree;
    ProElempath elem_path=NULL;
    struct creo_conv_info *cinfo = pinfo->cinfo;
    struct directory *dp = RT_DIR_NULL;
    vect_t a, b, c, d, h;

    if (pinfo->cinfo->do_facets_only) {
	return (pinfo->diameter < cinfo->min_hole_diameter) ? 1 : 0;
    }

    struct hole_info *hinfo;
    BU_GET(hinfo, struct hole_info);
    hinfo->feat = pinfo->feat;
    hinfo->radius = pinfo->radius;
    hinfo->diameter = pinfo->diameter;

    /* Do a more detailed characterization of the hole elements */
    if ((ret=ProFeatureElemtreeCreate(hinfo->feat, &elem_tree )) == PRO_TK_NO_ERROR ) {
	if ((ret=ProElemtreeElementVisit( elem_tree, elem_path, hole_elem_filter, hole_elem_visit, (ProAppData)hinfo)) != PRO_TK_NO_ERROR ) {
	    if ( ProElementFree( &elem_tree ) != PRO_TK_NO_ERROR ) {fprintf( stderr, "Error freeing element tree\n" );}
	    BU_PUT(hinfo, struct hole_info);
	    return ret;
	}
	ProElementFree(&elem_tree);
    }

    /* need more info to recreate holes */
    if ((ret=ProFeatureGeomitemVisit(pinfo->feat, PRO_AXIS, geomitem_visit, geomitem_filter, (ProAppData)pinfo)) != PRO_TK_NO_ERROR) return ret;

    /* Will need parent object name for naming */
    ProMdlMdlnameGet(pinfo->model, wname);

    /* make a replacement hole using CSG */
    if ( hinfo->hole_type == PRO_HLE_NEW_TYPE_STRAIGHT ) {
	/* plain old straight hole */
	if (hinfo->diameter < cinfo->min_hole_diameter) return 1;
	VSUB2( h, hinfo->end1, hinfo->end2 );
	bn_vec_ortho( a, h );
	VCROSS( b, a, h );
	VUNITIZE( b );
	VSCALE( hinfo->end2, hinfo->end2, cinfo->creo_to_brl_conv );
	VSCALE( a, a, pinfo->radius*cinfo->creo_to_brl_conv );
	VSCALE( b, b, pinfo->radius*cinfo->creo_to_brl_conv );
	VSCALE( h, h, cinfo->creo_to_brl_conv );
	hname = tgc_hole_name(cinfo, wname, "s");
	mk_tgc(cinfo->wdbp, bu_vls_addr(hname), hinfo->end2, h, a, b, a, b);
	if ((dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(hname), LOOKUP_QUIET)) != RT_DIR_NULL) pinfo->subtractions->push_back(dp);
	bu_vls_free(hname);
	BU_PUT(hname, struct bu_vls);
    } else if ( hinfo->hole_type == PRO_HLE_NEW_TYPE_STANDARD ) {
	/* drilled hole with possible countersink and counterbore */
	point_t start;
	vect_t dir;
	double cb_radius;
	double accum_depth=0.0;
	double hole_radius=hinfo->hole_diam / 2.0;

	if (hinfo->hole_diam < cinfo->min_hole_diameter) return 1;

	VSUB2( dir, hinfo->end1, hinfo->end2 );
	VUNITIZE( dir );

	VMOVE( start, hinfo->end2 );
	VSCALE( start, start, cinfo->creo_to_brl_conv );

	if ( hinfo->add_cbore == PRO_HLE_ADD_CBORE ) {
	    bn_vec_ortho( a, dir );
	    VCROSS( b, a, dir );
	    VUNITIZE( b );
	    cb_radius = hinfo->cb_diam * cinfo->creo_to_brl_conv / 2.0;
	    VSCALE( a, a, cb_radius );
	    VSCALE( b, b, cb_radius );
	    VSCALE( h, dir, hinfo->cb_depth * cinfo->creo_to_brl_conv );
	    hname = tgc_hole_name(cinfo, wname, "s");
	    mk_tgc(cinfo->wdbp, bu_vls_addr(hname), start, h, a, b, a, b);
	    if ((dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(hname), LOOKUP_QUIET)) != RT_DIR_NULL) pinfo->subtractions->push_back(dp);
	    bu_vls_free(hname);
	    BU_PUT(hname, struct bu_vls);

	    VADD2( start, start, h );
	    accum_depth += hinfo->cb_depth;
	    hinfo->cb_diam = 0.0;
	    hinfo->cb_depth = 0.0;
	}
	if ( hinfo->add_csink == PRO_HLE_ADD_CSINK ) {
	    double cs_depth;
	    double cs_radius = hinfo->cs_diam / 2.0;
	    cs_depth = (hinfo->cs_diam - hinfo->hole_diam) / (2.0 * tan(hinfo->cs_angle * M_PI / 360.0 ));
	    bn_vec_ortho( a, dir );
	    VCROSS( b, a, dir );
	    VUNITIZE( b );
	    VMOVE( c, a );
	    VMOVE( d, b );
	    VSCALE( h, dir, cs_depth * cinfo->creo_to_brl_conv );
	    VSCALE( a, a, cs_radius * cinfo->creo_to_brl_conv );
	    VSCALE( b, b, cs_radius * cinfo->creo_to_brl_conv );
	    VSCALE( c, c, hinfo->hole_diam * cinfo->creo_to_brl_conv / 2.0 );
	    VSCALE( d, d, hinfo->hole_diam * cinfo->creo_to_brl_conv / 2.0 );
	    hname = tgc_hole_name(cinfo, wname, "s");
	    mk_tgc(cinfo->wdbp, bu_vls_addr(hname), start, h, a, b, c, d);
	    if ((dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(hname), LOOKUP_QUIET)) != RT_DIR_NULL) pinfo->subtractions->push_back(dp);
	    bu_vls_free(hname);
	    BU_PUT(hname, struct bu_vls);

	    VADD2( start, start, h );
	    accum_depth += cs_depth;
	    hinfo->cs_diam = 0.0;
	    hinfo->cs_angle = 0.0;
	}
	bn_vec_ortho( a, dir );
	VCROSS( b, a, dir );
	VUNITIZE( b );
	VMOVE( c, a );
	VMOVE( d, b );
	VSCALE( a, a, hole_radius * cinfo->creo_to_brl_conv );
	VSCALE( b, b, hole_radius * cinfo->creo_to_brl_conv );
	VSCALE( c, c, hole_radius * cinfo->creo_to_brl_conv );
	VSCALE( d, d, hole_radius * cinfo->creo_to_brl_conv );
	VSCALE( h, dir, (hinfo->hole_depth - accum_depth) * cinfo->creo_to_brl_conv );
	hname = tgc_hole_name(cinfo, wname, "s");
	mk_tgc(cinfo->wdbp, bu_vls_addr(hname), start, h, a, b, c, d);
	if ((dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(hname), LOOKUP_QUIET)) != RT_DIR_NULL) pinfo->subtractions->push_back(dp);
	bu_vls_free(hname);
	BU_PUT(hname, struct bu_vls);

	VADD2( start, start, h );
	hinfo->hole_diam = 0.0;
	hinfo->hole_depth = 0.0;

	if ( hinfo->hole_depth_type == PRO_HLE_STD_VAR_DEPTH ) {
	    double tip_depth;
	    bn_vec_ortho( a, dir );
	    VCROSS( b, a, dir );
	    VUNITIZE( b );
	    VMOVE( c, a );
	    VMOVE( d, b );
	    tip_depth = (hole_radius - MIN_RADIUS) / tan( hinfo->drill_angle * M_PI / 360.0 );
	    VSCALE( h, dir, tip_depth * cinfo->creo_to_brl_conv );
	    VSCALE( a, a, hole_radius * cinfo->creo_to_brl_conv );
	    VSCALE( b, b, hole_radius * cinfo->creo_to_brl_conv );
	    VSCALE( c, c, MIN_RADIUS );
	    VSCALE( d, d, MIN_RADIUS );
	    hname = tgc_hole_name(cinfo, wname, "s");
	    mk_tgc(cinfo->wdbp, bu_vls_addr(hname), start, h, a, b, c, d);
	    if ((dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(hname), LOOKUP_QUIET)) != RT_DIR_NULL) pinfo->subtractions->push_back(dp);
	    bu_vls_free(hname);
	    BU_PUT(hname, struct bu_vls);

	    hinfo->drill_angle = 0.0;
	}
    } else {
	char pname[CREO_NAME_MAX];
	ProWstringToString(pname, wname);
	creo_log(cinfo, MSG_DEBUG, "%s: unrecognized hole type\n", pname);
	BU_PUT(hinfo, struct hole_info);
	return 0;
    }

    BU_PUT(hinfo, struct hole_info);
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
