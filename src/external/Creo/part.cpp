/**
 *                   P A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017-2023 United States Government as represented by
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


#include "common.h"
#include <algorithm>
#include "creo-brl.h"


extern "C" ProError
generic_filter(ProDimension *UNUSED(dim), ProAppData UNUSED(data)) {
    return PRO_TK_NO_ERROR;
}


/**
 * Routine to check for bad triangles, only checks for triangles with
 * duplicate vertices
 */
extern "C" int
bad_triangle(struct creo_conv_info *cinfo,  size_t v1, size_t v2, size_t v3, struct bg_vert_tree *tree)
{
    double dist;
    double coord;

    if (v1 == v2 || v2 == v3 || v1 == v3)
        return 1;

    dist = 0;
    for (size_t i = 0; i < 3; i++) {
        coord = tree->the_array[v1*3+i] - tree->the_array[v2*3+i];
        dist += coord * coord;
    }
    dist = sqrt(dist);
    if (dist < cinfo->local_tol)
        return 1;

    dist = 0;
    for (size_t i = 0; i < 3; i++) {
        coord = tree->the_array[v2*3+i] - tree->the_array[v3*3+i];
        dist += coord * coord;
    }
    dist = sqrt(dist);
    if (dist < cinfo->local_tol)
        return 1;

    dist = 0;
    for (size_t i = 0; i < 3; i++) {
        coord = tree->the_array[v1*3+i] - tree->the_array[v3*3+i];
        dist += coord * coord;
    }
    dist = sqrt(dist);
    if (dist < cinfo->local_tol)
        return 1;

    return 0;
}


/**
 * If we have a Creo modeling feature that adds material, it can
 * impact the decision on which conversion methods are practical
 */
extern "C" int
feat_adds_material(ProFeattype feat_type)
{

    /* 
     * Assume all features at or above a certain threshold add material?
     *
     * TODO - requires further investigation
     */
    if (feat_type >= PRO_FEAT_UDF_THREAD)
        return 1;

    switch (feat_type) {
        case PRO_FEAT_FIRST_FEAT:    break;  /* solid types  */
        case PRO_FEAT_SHAFT:         break;  /* add material */
        case PRO_FEAT_PROTRUSION:    break;  /*  (return 1)  */
        case PRO_FEAT_NECK:          break;
        case PRO_FEAT_FLANGE:        break;
        case PRO_FEAT_RIB:           break;
        case PRO_FEAT_EAR:           break;
        case PRO_FEAT_DOME:          break;
        case PRO_FEAT_LOC_PUSH:      break;  /*     TODO     */
        case PRO_FEAT_UDF:           break;  /* review these */
        case PRO_FEAT_DRAFT:         break;  /*   add more?  */
        case PRO_FEAT_SHELL:         break;
        case PRO_FEAT_DOME2:         break;
        case PRO_FEAT_IMPORT:        break;
        case PRO_FEAT_MERGE:         break;
        case PRO_FEAT_MOLD:          break;
        case PRO_FEAT_OFFSET:        break;
        case PRO_FEAT_REPLACE_SURF:  break;
        case PRO_FEAT_PIPE:          break;
        default:                             /* other types  */
            return 0;                        /*  (return 0)  */
    }
    return 1;                                /* solid types  */
}


/**
 * This function returns a string for the given ProFeatStatus
 */
extern "C" const char *feat_status_to_str(ProFeatStatus feat_stat)
{
    static const char *feat_status[] = {     /* ProFeatStatus */
        "PRO_FEAT_ACTIVE",                   /*    0 thru 6   */
        "PRO_FEAT_INACTIVE",                 /*    refer to */
        "PRO_FEAT_FAMTAB_SUPPRESSED",        /*  ProFeature.h */
        "PRO_FEAT_SIMP_REP_SUPPRESSED",
        "PRO_FEAT_PROG_SUPPRESSED",
        "PRO_FEAT_SUPPRESSED",
        "PRO_FEAT_UNREGENERATED"
    };

    if (feat_stat < 0 || feat_stat > 6)     /* out of range? */
        return NULL;
    else
        return feat_status[feat_stat];
}


/**
 * This routine filters out which features should be visited by the
 * feature visit initiated in the output_part() routine.
 */
extern "C" ProError
feature_filter(ProFeature *feat, ProAppData data)
{
    ProError err = PRO_TK_GENERAL_ERROR;
    struct part_conv_info *pinfo = (struct part_conv_info *)data;

    err = ProFeatureTypeGet(feat, &pinfo->type);
    if (err != PRO_TK_NO_ERROR)
        return err;

    /* Handle holes, chamfers, and rounds only */
    if (pinfo->type == PRO_FEAT_HOLE    ||
        pinfo->type == PRO_FEAT_CHAMFER ||
        pinfo->type == PRO_FEAT_ROUND)
        return PRO_TK_NO_ERROR;

    /*
     * If we encounter a protrusion (or any feature that adds material)
     * after a hole, we cannot convert previous holes to CSG
     */
    if (feat_adds_material(pinfo->type))
        pinfo->csg_holes_supported = 0;

    /* Skip everything else */
    return PRO_TK_CONTINUE;
}


extern "C" ProError
check_dimension(ProDimension *dim, ProError UNUSED(status), ProAppData data)
{
    ProDimensiontype dim_type;
    ProError err = PRO_TK_GENERAL_ERROR;

    double val;
    struct part_conv_info *pinfo = (struct part_conv_info *)data;

    err = ProDimensionTypeGet(dim, &dim_type);
    if (err != PRO_TK_NO_ERROR)
        return err;

    switch (dim_type) {           /* refer to ProDimension.h */
        case PRODIMTYPE_RADIUS:
            err = ProDimensionValueGet(dim, &pinfo->radius);
            if (err == PRO_TK_NO_ERROR) {
                pinfo->diameter = 2.0 * pinfo->radius;
                pinfo->got_diameter = 1;
            }
            break;
        case PRODIMTYPE_DIAMETER:
            err = ProDimensionValueGet(dim, &pinfo->diameter);
            if (err == PRO_TK_NO_ERROR) {
                pinfo->radius = pinfo->diameter / 2.0;
                pinfo->got_diameter = 1;
            }
            break;
        case PRODIMTYPE_LINEAR:
            err = ProDimensionValueGet(dim, &val);
            if (err == PRO_TK_NO_ERROR) {
                if (pinfo->got_distance1)
                    pinfo->distance2 = val;
                else {
                    pinfo->got_distance1 = 1;
                    pinfo->distance1 = val;
                }
            }
            break;
    }
    return err;
}


extern "C" ProError
feature_visit(ProFeature *feat, ProError UNUSED(status), ProAppData data)
{
    ProError err = PRO_TK_GENERAL_ERROR;
    ProFeattype feat_type;

    struct part_conv_info *pinfo = (struct part_conv_info *)data;

    pinfo->feat          = feat;
    pinfo->radius        = 0.0;
    pinfo->diameter      = 0.0;
    pinfo->distance1     = 0.0;
    pinfo->distance2     = 0.0;
    pinfo->got_diameter  = 0;
    pinfo->got_distance1 = 0;

    err = ProFeatureDimensionVisit(feat, check_dimension, generic_filter, (ProAppData)pinfo);
    if (err != PRO_TK_NO_ERROR)
        return err;

    feat_type = pinfo->type;

    switch (feat_type) { 
        case PRO_FEAT_HOLE:
            if (pinfo->diameter < pinfo->cinfo->min_hole_diameter)
                pinfo->suppressed_features->push_back(feat->id);        /* Suppress small holes */
            else if (pinfo->cinfo->facets_only)
                goto skip;                                              /* Skip hole when Facetizing */
            else if (subtract_hole(pinfo))
                pinfo->suppressed_features->push_back(feat->id);        /* Suppress hole before tessellating */
            break;
        case PRO_FEAT_ROUND:
            if (pinfo->got_diameter && pinfo->radius < pinfo->cinfo->min_round_radius)
                pinfo->suppressed_features->push_back(feat->id);        /* Suppress small rounds */
            break;
        case PRO_FEAT_CHAMFER:
            if (pinfo->got_distance1 && pinfo->distance1 < pinfo->cinfo->min_chamfer_dim &&
                pinfo->distance2 < pinfo->cinfo->min_chamfer_dim)
                pinfo->suppressed_features->push_back(feat->id);        /* Suppress small dimension chamfers */
            break;
    }
skip: return PRO_TK_NO_ERROR;
}


extern "C" void
unsuppress_features(struct part_conv_info *pinfo)
{
    char    pname[CREO_NAME_MAX];
    wchar_t wname[CREO_NAME_MAX];

    ProError      err = PRO_TK_GENERAL_ERROR;
    ProFeature    feat;
    ProFeatStatus feat_stat;

    ProFeatureResumeOptions *resume_opts;
    resume_opts = (ProFeatureResumeOptions *) PRO_FEAT_RESUME_INCLUDE_PARENTS;

    (void)ProMdlMdlnameGet(pinfo->model, wname);
    ProWstringToString(pname, wname);
    lower_case(pname);

    for (unsigned int i = 0; i < pinfo->suppressed_features->size(); i++) {

        err = ProFeatureInit(ProMdlToSolid(pinfo->model),
                             pinfo->suppressed_features->at(i),
                             &feat);
        if (err != PRO_TK_NO_ERROR)
            continue;

        err = ProFeatureStatusGet(&feat, &feat_stat);
        if (err != PRO_TK_NO_ERROR)
            continue;

        if (feat_stat != PRO_FEAT_SUPPRESSED)
            continue;

        /* Resume suppressed feature */
        creo_log(pinfo->cinfo, MSG_PLAIN, "   FEAT: \"%s\" unsuppressing feature \"%d\"\n",
                 pname, pinfo->suppressed_features->at(i));

        err = ProFeatureWithoptionsResume(ProMdlToSolid(pinfo->model),
                                          &(pinfo->suppressed_features->at(i)),
                                          resume_opts,
                                          PRO_REGEN_NO_FLAGS);

        /* Report the outcome */
        switch (err) {
            case PRO_TK_NO_ERROR:
                creo_log(pinfo->cinfo, MSG_PLAIN, "   FEAT: Part \"%s\" feature \"%d\" unsuppressed\n",
                         pname, pinfo->suppressed_features->at(i));
                break;
            case PRO_TK_SUPP_PARENTS:
                creo_log(pinfo->cinfo, MSG_PLAIN, "   FEAT: Part \"%s\" suppressed parents for feature \"%d\" not found\n",
                         pname, pinfo->suppressed_features->at(i));
                break;
            default:
                creo_log(pinfo->cinfo, MSG_FAIL,  "Part \"%s\" feature id \"%d\" failed to resume\n",
                         pname, pinfo->suppressed_features->at(i));
        }

    }
}


/*
 * TODO - will probably need maps from Creo data items to ON data items here...
 */
struct brep_data {
    ON_Brep *brep;
    ProSurface s;
    int curr_loop_id;
    std::map<int, int> *cs_to_onf;
    std::map<int, int> *ce_to_one;
    std::map<int, int> *cc3d_to_on3dc;
    std::map<int, int> *cc2d_to_on2dc;
};


extern "C" ProError
edge_filter(ProEdge UNUSED(e), ProAppData UNUSED(app_data)) {
    return PRO_TK_NO_ERROR;
}


/**
 * ProEdge appears to correspond to the ON_BrepEdge.
 */
extern "C" ProError
edge_process(ProEdge UNUSED(e), ProError UNUSED(status), ProAppData app_data) {
    int current_surf_id;
    struct brep_data *bdata = (struct brep_data *)app_data;
    ProSurface s = bdata->s;
    ProSurfaceIdGet(s, &current_surf_id);

    /*
     * ON_BrepLoop &nl = bdata->brep->m_L[bdata->curr_loop_id];
     *
     * TODO - there appears to be no way to get at ProEdgedata from an
     * existing edge???  If that's the case, and we're limited to
     * the ProEdgeToNURBS() capability, then we may have to use the
     * step-g routines for creating our own trimming curves (arrgh!)
     *
     *     ProGeomitemdata *edata;
     *     ProEdgeDataGet(e, &edata);
     *
     */

    return PRO_TK_NO_ERROR;
}


extern "C" ProError
contour_filter(ProContour UNUSED(c), ProAppData UNUSED(app_data)) {
    return PRO_TK_NO_ERROR;
}


/**
 * Contours appear to correspond to Loops in OpenNURBS.
 */
extern "C" ProError
contour_process(ProContour c, ProError UNUSED(status), ProAppData app_data) {
    struct brep_data *bdata = (struct brep_data *)app_data;
    int f_id   = -1;
    int ptc_id = -1;
    ProSurface s = bdata->s;
    ProSurfaceIdGet(s, &ptc_id);
    if (bdata->cs_to_onf->find(ptc_id) != bdata->cs_to_onf->end())
        f_id = bdata->cs_to_onf->find(ptc_id)->second;
    if (f_id == -1)
        return PRO_TK_GENERAL_ERROR;
    ON_BrepLoop &nl = bdata->brep->NewLoop(ON_BrepLoop::outer, bdata->brep->m_F[f_id]);
    bdata->curr_loop_id = nl.m_loop_index;
    /* Does ProContourTraversal indicate inner vs outer loop? */
    ProContourTraversal tv;
    ProContourTraversalGet(c, &tv);
    if (tv == PRO_CONTOUR_TRAV_INTERNAL)
        nl.m_type = ON_BrepLoop::inner;

    ProContourEdgeVisit(s, c, edge_process, edge_filter, app_data);

    return PRO_TK_NO_ERROR;
}


extern "C" ProError
surface_process(ProSurface s, ProError UNUSED(status), ProAppData app_data) {

    ProError        err = PRO_TK_GENERAL_ERROR;
    ProSurfacedata *ndata;
    ProSrftype      s_type;
    ProUvParam      uvmin = {DBL_MAX, DBL_MAX};
    ProUvParam      uvmax = {DBL_MIN, DBL_MIN};

    ProSurfaceOrient    s_orient;
    ProSurfaceshapedata s_shape;

    int c_id;

    struct brep_data *bdata = (struct brep_data *)app_data;
    ON_Brep *nbrep = bdata->brep;

    ProSurfaceIdGet(s, &c_id);
    if (bdata->cs_to_onf->find(c_id) == bdata->cs_to_onf->end()) {
        int s_id;
        ProSurfaceToNURBS(s, &ndata);
        ProSurfacedataGet(ndata, &s_type, uvmin, uvmax, &s_orient, &s_shape, &s_id);
        if (s_type == PRO_SRF_B_SPL) {
            int degree[2];
            double *up_array = NULL;
            double *vp_array = NULL;
            double *w_array = NULL;
            ProVector *cntl_pnts;
            int ucnt, vcnt, ccnt;
            err = ProBsplinesrfdataGet(&s_shape, degree, &up_array, &vp_array, &w_array, &cntl_pnts, &ucnt, &vcnt, &ccnt);
            if (err == PRO_TK_NO_ERROR) {
                int ucvmax = ucnt - degree[0] - 2;
                int vcvmax = vcnt - degree[1] - 2;

                ON_NurbsSurface *ns = ON_NurbsSurface::New(3, (w_array) ? 1 : 0, degree[0]+1, degree[1]+1, ucvmax+1, vcvmax+1);

                /* knot index (>= 0 and < Order + CV_count - 2) */

                /* Generate u-knots */
                int n = ucvmax+1;
                int p = degree[0];
                int m = n + p - 1;

                for (int i = 0; i < p; i++)
                    ns->SetKnot(0, i, 0.0);

                for (int j = 1; j < n - p; j++) {
                    double x = (double)j / (double)(n - p);
                    int knot_index = j + p - 1;
                    ns->SetKnot(0, knot_index, x);
                }

                for (int i = m - p; i < m; i++)
                    ns->SetKnot(0, i, 1.0);

                /* Generate v-knots */
                n = vcvmax+1;
                p = degree[1];
                m = n + p - 1;

                for (int i = 0; i < p; i++)
                    ns->SetKnot(1, i, 0.0);

                for (int j = 1; j < n - p; j++) {
                    double x = (double)j / ((double)n - (double)p);
                    int knot_index = j + p - 1;
                    ns->SetKnot(1, knot_index, x);
                }

                for (int i = m - p; i < m; i++)
                    ns->SetKnot(1, i, 1.0);

                if (ns->m_is_rat)
                    for (int i = 0; i <= ucvmax; i++)
                        for (int j = 0; j <= vcvmax; j++) {
                            ON_4dPoint cv;
                            cv[0] = cntl_pnts[i*(vcvmax+1)+j][0];
                            cv[1] = cntl_pnts[i*(vcvmax+1)+j][1];
                            cv[2] = cntl_pnts[i*(vcvmax+1)+j][2];
                            cv[3] = (w_array) ? w_array[i] : 0;
                            ns->SetCV(i, j, cv);
                        }
                else
                    for (int i = 0; i <= ucvmax; i++)
                        for (int j = 0; j <= vcvmax; j++) {
                            ON_3dPoint cv;
                            cv[0] = cntl_pnts[i*(vcvmax+1)+j][0];
                            cv[1] = cntl_pnts[i*(vcvmax+1)+j][1];
                            cv[2] = cntl_pnts[i*(vcvmax+1)+j][2];
                            ns->SetCV(i, j, cv);
                        }

                int n_id = nbrep->AddSurface(ns);
                ON_BrepFace &nface = nbrep->NewFace(n_id);
                (*bdata->cs_to_onf)[c_id] = nface.m_face_index;
                if (s_orient == PRO_SURF_ORIENT_IN)
                    nbrep->FlipFace(nface);
            }
            ProSurfaceContourVisit(s, contour_process, contour_filter, app_data);
        }
    }

    return PRO_TK_NO_ERROR;
}


extern "C" ProError
opennurbs_part(struct creo_conv_info *cinfo, ProMdl model, struct bu_vls **sname)
{
    ProError       err = PRO_TK_GENERAL_ERROR;
    ProSolid      psol = ProMdlToSolid(model);
    ProSolidBody *pbdy = NULL;

    ON_Brep *nbrep = ON_Brep::New();
    struct brep_data bdata;
    bdata.brep = nbrep;

    wchar_t wname[CREO_NAME_MAX];

    err = ProMdlMdlnameGet(model, wname);
    if (err != PRO_TK_NO_ERROR) {
        return err;
    }

    err = ProSolidBodiesCollect(psol, &pbdy);
    if (err != PRO_TK_NO_ERROR) {
        return err;
    }

    /* Allocate the brep data */
    bdata.cs_to_onf = new std::map<int, int>;
    bdata.ce_to_one = new std::map<int, int>;
    bdata.cc3d_to_on3dc = new std::map<int, int>;
    bdata.cc2d_to_on2dc = new std::map<int, int>;

    err = ProSolidBodySurfaceVisit(pbdy, surface_process, (ProAppData)&bdata);
    if (err == PRO_TK_NO_ERROR) {
        /* Output the solid */
        *sname = get_brlcad_name(cinfo, wname, "brep", N_SOLID);
        mk_brep(cinfo->wdbp, bu_vls_addr(*sname), nbrep);
    }

    /*
     * Things to investigate:
     *
     * ProFeatureElemtreeExtract()
     *  PRO_E_SRF_TRIM_TYPE
     *   PRO_SURF_TRIM_USE_CRV
     *
     * ProSldsurfaceShellsAndVoidsFind
     * ProSolidFeatVisit + ProFeatureVisibilityGet
     * ProSolidBodySurfaceVisit
     * ProSurfaceContourVisit
     * ProContourEdgeVisit
     * ProContourTraversalGet
     * ProEdgeDirGet
     * ProEdgeNeighborsGet
     * ProGeomitemdata
     * ProSurfaceToNURBS()
     * ProEdgeToNURBS()
     * ProCurveToNURBS()
     */

    /* Clean-up before returning */
    delete bdata.cs_to_onf;
    delete bdata.ce_to_one;
    delete bdata.cc3d_to_on3dc;
    delete bdata.cc2d_to_on2dc;

    return err;
}


extern "C" ProError
tessellate_part(struct creo_conv_info *cinfo, ProMdl model, struct bu_vls **sname)
{
    ProSurfaceTessellationData *tess = NULL;
    ProError err = PRO_TK_GENERAL_ERROR;

    wchar_t wname[CREO_NAME_MAX];
    char    pname[CREO_NAME_MAX];

    struct bg_vert_tree *vert_tree = NULL;
    struct bg_vert_tree *norm_tree = NULL;
    std::vector<int> faces;
    std::vector<int> face_normals;

    /* tessellation settings */
    int    max_steps;
    double max_angle;
    double min_angle;
    double max_chord;
    double min_chord;
    double angle_incr;
    double chord_incr;

    /* tessellation values */
    double curr_chord;
    double curr_angle;

    /* tessellation results */
    int success     = 0;
    int facet_count = 0;
    int surf_count  = 0;
    size_t v1, v2, v3;
    size_t n1, n2, n3;
    size_t vert_no;

    double conv_scale = cinfo->creo_to_brl_conv;

    /*
     * Note: The below code works, but we can't use model units - Creo
     * "corrects" object sizes with matricies in the parent hierarchies
     * and correcting it here results in problems with those matricies.
     *
     *     ProError ustatus = creo_model_units(&conv_scale, model);
     */

    (void)ProMdlMdlnameGet(model, wname);
    ProWstringToString(pname, wname);
    lower_case(pname);

    if (cinfo->max_steps < 1) {
        creo_log(cinfo, MSG_PLAIN, "   TESS: \"%s\" has invalid settings with max_steps = %d\n",
                                   pname, cinfo->max_steps);
        return err;
    } else
        max_steps = (cinfo->max_steps);

    if (cinfo->tess_adapt) {
        /* Apply adaptive tessellation settings */
        max_angle = 1.0;
        min_angle = 1.0/double(cinfo->max_steps);
        max_chord = (cinfo->bbox_diag)/double(cinfo->max_steps);
        min_chord = (cinfo->bbox_diag)/pow(double(cinfo->max_steps), 2);
        creo_log(cinfo, MSG_PLAIN, "   TESS: ========== \"%s\" ==========\n", pname);
        creo_log(cinfo, MSG_PLAIN, "   TESS:  Max angle control = %g\n"     , max_angle);
        creo_log(cinfo, MSG_PLAIN, "   TESS:  Min angle control = %g\n"     , min_angle);
        creo_log(cinfo, MSG_PLAIN, "   TESS:   Max chord height = %g\n"     , max_chord);
        creo_log(cinfo, MSG_PLAIN, "   TESS    Min chord height = %g\n"     , min_chord);
        creo_log(cinfo, MSG_PLAIN, "   TESS:          Max steps = %d\n"     , max_steps);
        creo_log(cinfo, MSG_PLAIN, "   TESS: ---------- \"%s\" ----------\n", pname);
    } else {
        /* Adopt existing tessellation settings */
        max_angle = (cinfo->max_angle);
        min_angle = (cinfo->min_angle);
        max_chord = (cinfo->max_chord)/conv_scale;  /* mm to in */
        min_chord = (cinfo->min_chord)/conv_scale;  /* mm to in */
    }

    if (max_steps > 1) {
        angle_incr = (ZERO(max_angle - min_angle)) ? 0.0 : (max_angle - min_angle) / (double)(max_steps - 1);
        chord_incr = (ZERO(max_chord - min_chord)) ? 0.0 : (max_chord - min_chord) / (double)(max_steps - 1);
    } else {
        angle_incr = (ZERO(max_angle - min_angle)) ? 0.0 : (max_angle - min_angle);
        chord_incr = (ZERO(max_chord - min_chord)) ? 0.0 : (max_chord - min_chord);
    }

    if (ZERO(angle_incr) && ZERO(chord_incr))
        max_steps = 0;

    creo_log(cinfo, MSG_PLAIN, "   TESS: Processing part \"%s\"...\n", pname);

    /* Tessellate part, going from coarse to fine tessellation */
    for (int i = 0; i < max_steps; i++) {
        curr_chord = max_chord - (i * chord_incr);
        curr_angle = min_angle + (i * angle_incr);

        vert_tree = bg_vert_tree_create();
        norm_tree = bg_vert_tree_create();

        err = ProPartTessellate(ProMdlToPart(model), curr_chord, curr_angle,
                                 PRO_B_TRUE, &tess);

        if (err != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_PLAIN, "   TESS: \"%s\" failed with %g and %g\n",
                     pname, curr_chord, curr_angle);
            continue;
        }

        /* We got through the initial tests - how many surfaces do we have? */
        err = ProArraySizeGet((ProArray)tess, &surf_count);
        if (err != PRO_TK_NO_ERROR)
            goto tess_cleanup;
        else if (surf_count < 3) {
            err = PRO_TK_NOT_EXIST;
            goto tess_cleanup;
        }

        facet_count = 0;
        for (int surfno = 0; surfno < surf_count; surfno++)
            for (int k = 0; k < tess[surfno].n_facets; k++) {
                /* Grab the triangle */
                vert_no = (size_t)tess[surfno].facets[k][0];
                v1 = bg_vert_tree_add(vert_tree,
                                      tess[surfno].vertices[vert_no][0],
                                      tess[surfno].vertices[vert_no][1],
                                      tess[surfno].vertices[vert_no][2],
                                      cinfo->local_tol_sq);
                vert_no = (size_t)tess[surfno].facets[k][1];
                v2 = bg_vert_tree_add(vert_tree,
                                      tess[surfno].vertices[vert_no][0],
                                      tess[surfno].vertices[vert_no][1],
                                      tess[surfno].vertices[vert_no][2],
                                      cinfo->local_tol_sq);
                vert_no = (size_t)tess[surfno].facets[k][2];
                v3 = bg_vert_tree_add(vert_tree,
                                      tess[surfno].vertices[vert_no][0],
                                      tess[surfno].vertices[vert_no][1],
                                      tess[surfno].vertices[vert_no][2],
                                      cinfo->local_tol_sq);

                if (bad_triangle(cinfo, v1, v2, v3, vert_tree))
                    continue;

                faces.push_back((int)v1);
                faces.push_back((int)v3);
                faces.push_back((int)v2);

                /* Grab the surface normals */
                if (cinfo->get_normals) {
                    vert_no = (size_t)tess[surfno].facets[k][0];
                    VUNITIZE(tess[surfno].normals[vert_no]);
                    n1 = bg_vert_tree_add(norm_tree,
                                          tess[surfno].normals[vert_no][0],
                                          tess[surfno].normals[vert_no][1],
                                          tess[surfno].normals[vert_no][2],
                                          cinfo->local_tol_sq);
                    vert_no = (size_t)tess[surfno].facets[k][1];
                    VUNITIZE(tess[surfno].normals[vert_no]);
                    n2 = bg_vert_tree_add(norm_tree,
                                          tess[surfno].normals[vert_no][0],
                                          tess[surfno].normals[vert_no][1],
                                          tess[surfno].normals[vert_no][2],
                                          cinfo->local_tol_sq);
                    vert_no = (size_t)tess[surfno].facets[k][2];
                    VUNITIZE(tess[surfno].normals[vert_no]);
                    n3 = bg_vert_tree_add(norm_tree,
                                          tess[surfno].normals[vert_no][0],
                                          tess[surfno].normals[vert_no][1],
                                          tess[surfno].normals[vert_no][2],
                                          cinfo->local_tol_sq);
                    face_normals.push_back((int)n1);
                    face_normals.push_back((int)n3);
                    face_normals.push_back((int)n2);
                }
            facet_count++;
            }

        /* Check solidity */
        success = bg_trimesh_solid((int)vert_tree->curr_vert, (size_t)faces.size() / 3,
                                        vert_tree->the_array, &faces[0], NULL);

        /* If it's not solid and we're testing solidity, keep trying... */
        if (!success) {
            if (cinfo->check_solidity) {
                /* Free the tessellation memory */
                ProPartTessellationFree(&tess);
                tess = NULL;

                /* Clean trees */
                bg_vert_tree_clean(vert_tree);
                bg_vert_tree_clean(norm_tree);

                creo_log(cinfo, MSG_PLAIN, "   TESS: \"%s\" failed solidity test "
                                           "with %g and %g, trying next level\n", 
                                           pname, curr_chord, curr_angle);

                /* failed bot, solidity enforced, so keep trying */
                continue;

            } else
                creo_log(cinfo, MSG_PLAIN, "   TESS: \"%s\" failed solidity test, "
                                           "but solidity requirement is disabled\n", 
                                           pname);
        } else
            break;
    }

    if (success) {
        cinfo->tess_count = facet_count;
        cinfo->tess_chord = curr_chord;
        cinfo->tess_angle = curr_angle;
        err = PRO_TK_NO_ERROR;
    } else {
        err = PRO_TK_NOT_EXIST;
        goto tess_cleanup;
    }

    /* Make sure we have non-zero faces (and if needed, face_normals) vectors */
    if (faces.size() == 0 || !vert_tree->curr_vert) {
        err = PRO_TK_NOT_EXIST;
        goto tess_cleanup;
    }
    if (cinfo->get_normals && face_normals.size() == 0) {
        err = PRO_TK_NOT_EXIST;
        goto tess_cleanup;
    }

    creo_log(cinfo, MSG_PLAIN, "   TESS: Tessellation of \"%s\" successful with %g and %g\n",
                                pname, curr_chord, curr_angle);
    creo_log(cinfo, MSG_PLAIN, "   TESS: Tessellation of \"%s\" produced %d facets\n",
                                pname, facet_count);

    for (unsigned int i = 0; i < vert_tree->curr_vert * 3; i++)
        vert_tree->the_array[i] = vert_tree->the_array[i] * conv_scale;

    /* Output the solid - TODO - what is the correct ordering??? does CCW always work? */
    *sname = get_brlcad_name(cinfo, wname, "s", N_SOLID);
    if (cinfo->get_normals)
        mk_bot_w_normals(cinfo->wdbp, bu_vls_addr(*sname), RT_BOT_SOLID, RT_BOT_CCW, 0,
                         vert_tree->curr_vert, (size_t)(faces.size()/3),
                         vert_tree->the_array, &faces[0], NULL,
                         NULL, (size_t)(face_normals.size()/3),
                         norm_tree->the_array, &face_normals[0]);
    else
        mk_bot(cinfo->wdbp, bu_vls_addr(*sname), RT_BOT_SOLID, RT_BOT_CCW, 0,
               vert_tree->curr_vert, (size_t)(faces.size()/3),
               vert_tree->the_array, &faces[0], NULL, NULL);

tess_cleanup:

    /* Free the tessellation memory */
    ProPartTessellationFree(&tess);
    tess = NULL;

    /* Free trees */
    bg_vert_tree_destroy(vert_tree);
    bg_vert_tree_destroy(norm_tree);

    return err;
}


/**
 * Routine to output a part as a BRL-CAD region with one BOT solid
 * The region will have the name from Creo with a .r suffix.
 * The solid will have the same name with ".bot" prefix.
 *
 *     returns:
 *       PRO_TK_NO_ERROR - OK
 *       PRO_TK_NOT_EXIST - Object not solid
 *       other ProError returns - error
 */
extern "C" ProError
output_part(struct creo_conv_info *cinfo, ProMdl model)
{
    Pro3dPnt        bboxpnts[2];
    ProError        tess_err = PRO_TK_GENERAL_ERROR;
    ProError        supp_err = PRO_TK_GENERAL_ERROR;
    ProFileName     msgfil = {'\0'};
    ProMassProperty massprops;
    ProMaterial     material;
    ProMdlType      type;
    ProModelitem    mitm;
    ProWVerstamp    cstamp;

    ProFeatureDeleteOptions   *delete_opts;
    ProFeatureResumeOptions   *resume_opts;
    ProSurfaceAppearanceProps aprops;

    char    mname[MAX_MATL_NAME + 1];
    char    pname[CREO_NAME_MAX];
    wchar_t wname[CREO_NAME_MAX];

    char *verstr;

    double  prtdens = 1.0;

    /*  bounding box flags */
    int have_bbox = 0;
    int have_rpp  = 0;

    fastf_t rgbflts[3];

    int  surface_side =  0;  /* 0 = surface normal;  1 = other side */
    int  rgb_mode     = -1;

    unsigned char rgb[3], creo_rgb[3];

    struct directory *rdp;
    struct directory *sdp;
    struct bu_vls vstr = BU_VLS_INIT_ZERO;
    struct bu_color color;
    struct wmember  wcomb;
    struct bu_vls  *rname = NULL;
    struct bu_vls  *sname = NULL;
    struct bu_vls  *ptc_name;
    struct part_conv_info *pinfo;

    BU_GET(pinfo, struct part_conv_info);
    pinfo->cinfo = cinfo;
    pinfo->csg_holes_supported = 1;
    pinfo->model = model;
    pinfo->suppressed_features = new std::vector<int>;
    pinfo->subtractions = new std::vector<struct directory *>;

    delete_opts = (ProFeatureDeleteOptions *) PRO_FEAT_DELETE_NO_OPTS;
    resume_opts = (ProFeatureResumeOptions *) PRO_FEAT_RESUME_INCLUDE_PARENTS;

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    (void)ProMdlTypeGet(model, &type);
    (void)ProMdlMdlnameGet(model, wname);
    ProWstringToString(pname, wname);
    lower_case(pname);

    creo_log(cinfo, MSG_PLAIN, "   PART: Processing \"%s\"\n", pname);

    /*
     * TODO - add some up-front logic to detect parts with no
     * associated geometry.
     */

    /* Collect info about things that might be eliminated */
    if (cinfo->elim_small) {
        /*
         * Apply user supplied criteria to see if we have anything
         * to suppress
         */
        ProSolidFeatVisit(ProMdlToSolid(model), feature_visit, feature_filter, (ProAppData)pinfo);

        /* If we've got anything to suppress, go ahead and do it. */
        if (!pinfo->suppressed_features->empty()) {
            supp_err = ProFeatureWithoptionsSuppress(ProMdlToSolid(model),
                                                &(pinfo->suppressed_features->at(0)),
                                                delete_opts,
                                                PRO_REGEN_NO_FLAGS);
            /* If something went wrong, need to undo just the suppressions we added */
            if (supp_err != PRO_TK_NO_ERROR) {
                creo_log(cinfo, MSG_PLAIN, "   FEAT: \"%s\" failed to suppress features\n", pname);
                unsuppress_features(pinfo);
            } else
                creo_log(cinfo, MSG_PLAIN, "   FEAT: \"%s\" features suppressed, continuing conversion\n", pname);
        }
    }

    /*
     * Get bounding box of a solid using "ProSolidOutlineGet"
     * TODO - may want to use this to implement relative facetization tolerance...
     */
    have_bbox = (ProSolidOutlineGet(ProMdlToSolid(model), bboxpnts) == PRO_TK_NO_ERROR);

    /* Retain bounding box results */
    if (have_bbox) {
        double dx, dy, dz;
        dx = abs(bboxpnts[1][0] - bboxpnts[0][0]);
        dy = abs(bboxpnts[1][1] - bboxpnts[0][1]);
        dz = abs(bboxpnts[1][2] - bboxpnts[0][2]);
        cinfo->bbox_area = 2.0*(dx*dy + dx*dz + dy*dz);
        cinfo->bbox_diag = sqrt(pow(dx,2) + pow(dy,2) + pow(dz,2));
        cinfo->bbox_vol  = dx*dy*dz;
        creo_log(cinfo, MSG_PLAIN, "   PART: ==============================\n");
        creo_log(cinfo, MSG_PLAIN, "   PART:          Bounding Box         \n");
        creo_log(cinfo, MSG_PLAIN, "   PART: ------------------------------\n");
        creo_log(cinfo, MSG_PLAIN, "   PART:             dx = %g\n", dx);
        creo_log(cinfo, MSG_PLAIN, "   PART:             dy = %g\n", dy);
        creo_log(cinfo, MSG_PLAIN, "   PART:             dz = %g\n", dy);
        creo_log(cinfo, MSG_PLAIN, "   PART:       Diagonal = %g\n", cinfo->bbox_diag);
        creo_log(cinfo, MSG_PLAIN, "   PART:   Surface area = %g\n", cinfo->bbox_area);
        creo_log(cinfo, MSG_PLAIN, "   PART:         Volume = %g\n", cinfo->bbox_vol);
        creo_log(cinfo, MSG_PLAIN, "   PART: ------------------------------\n");
    } else {
        creo_log(cinfo, MSG_FAIL, "Part \"%s\" has no bounding box\n", pname);
        tess_err = PRO_TK_NOT_EXIST;
        goto cleanup;
    }

    /*
     * TODO - support an option to convert to ON_Brep
     *
     *    err = opennurbs_part(cinfo, model, &sname);
     */

    /* Tessellate */
    tess_err = tessellate_part(cinfo, model, &sname);

    /* Deal with the solid conversion results */
    if (tess_err == PRO_TK_NOT_EXIST) {
        /* Failed!!! */
        creo_log(cinfo, MSG_PLAIN, "Tessellation for \"%s\" failed\n", pname);
        if (cinfo->create_boxes && have_bbox) {
            /*
             * A failed solid conversion with a bounding box indicates a
             * problem.  Rather than ignore it, put the bbox in the .g file
             * as a placeholder.
             */
            point_t rmin, rmax;

            rmin[0] = bboxpnts[0][0];
            rmin[1] = bboxpnts[0][1];
            rmin[2] = bboxpnts[0][2];
            rmax[0] = bboxpnts[1][0];
            rmax[1] = bboxpnts[1][1];
            rmax[2] = bboxpnts[1][2];

            sname = get_brlcad_name(cinfo, wname, "rpp", N_SOLID);
            mk_rpp(cinfo->wdbp, bu_vls_addr(sname), rmin, rmax);
            creo_log(cinfo, MSG_PLAIN, "   PART: \"%s\" replaced with bounding box\n", pname);
            have_rpp = 1;
            goto have_part;
        } else {
            wchar_t *stable = stable_wchar(cinfo, wname);
            if (!stable)
                creo_log(cinfo, MSG_PLAIN, "   NAME: Part \"%s\" no stable version of name found\n", pname);
            else 
                cinfo->empty->insert(stable);
            creo_log(cinfo, MSG_FAIL, "Part \"%s\" not converted\n", pname);
            goto cleanup;
        }
    }

    /*
     * We've got a part - the output for a part is a parent region and
     * the solid underneath it.
     */
have_part:

    BU_LIST_INIT(&wcomb.l);
    rname = get_brlcad_name(cinfo, wname, "r", N_REGION);

    /* Add the solid to the region comb */
    (void)mk_addmember(bu_vls_addr(sname), &wcomb.l, NULL, WMOP_UNION);

    /* Add any subtraction solids created by the hole handling */
    for (unsigned int i = 0; i < pinfo->subtractions->size(); i++)
        (void)mk_addmember(pinfo->subtractions->at(i)->d_namep, &wcomb.l, NULL, WMOP_SUBTRACT);

    /*
     * Get the surface properties from the part,
     * output the region comb
     */
    (void)ProMdlToModelitem(model, &mitm);
    if (ProSurfaceSideAppearancepropsGet(&mitm, surface_side, &aprops) == PRO_TK_NO_ERROR) {
        /* 
         *    Shader args
         *
         *    TODO Make exporting material optional
         *
         *    struct bu_vls shader_args = BU_VLS_INIT_ZERO;
         *
         *    bu_vls_sprintf(&shader_args, "{");
         *    if (!NEAR_ZERO(aprops.transparency,    SMALL_FASTF))
         *        bu_vls_printf(&shader_args, " tr %g", aprops.transparency);
         *    if (!NEAR_EQUAL(aprops.shininess, 1.0, SMALL_FASTF))
         *        bu_vls_printf(&shader_args, " sh %d", (int)(aprops.shininess * 18 + 2.0));
         *    if (!NEAR_EQUAL(aprops.diffuse,   0.3, SMALL_FASTF))
         *        bu_vls_printf(&shader_args, " di %g", aprops.diffuse);
         *    if (!NEAR_EQUAL(aprops.highlite,  0.7, SMALL_FASTF))
         *        bu_vls_printf(&shader_args, " sp %g", aprops.highlite);
         *    bu_vls_printf(&shader_args, "}");
         *
         */

        /* Use the colors, ... that were set in Creo */
        rgbflts[0] = aprops.color_rgb[0];
        rgbflts[1] = aprops.color_rgb[1];
        rgbflts[2] = aprops.color_rgb[2];

        bu_color_from_rgb_floats(&color, rgbflts);
        bu_color_to_rgb_chars(&color, rgb);

        creo_log(cinfo, MSG_PLAIN, "  COLOR: Part \"%s\" has rgb = %u/%u/%u\n",
                                   pname, rgb[0], rgb[1], rgb[2]);

        /* Adjust rgb values for minimum luminance thresold */
        if (cinfo->lmin > 0) { 
            rgb_mode = rgb4lmin(rgbflts, cinfo->lmin);
            if (rgb_mode < 0)
                creo_log(cinfo, MSG_PLAIN, "  COLOR: Part \"%s\" has unknown color error\n", pname);
            else if (rgb_mode == 0)
                creo_log(cinfo, MSG_PLAIN, "  COLOR: Part \"%s\" luminance exceeds the %d%s minimum\n",
                         pname, cinfo->lmin, "%");
            else {
                bu_color_to_rgb_chars(&color, creo_rgb);       /* Preserve existing Creo rgb values */
                bu_color_from_rgb_floats(&color, rgbflts);     /* Adopt the enhanced rgb values */
                creo_log(cinfo, MSG_PLAIN, "  COLOR: Part \"%s\" luminance has increased to %d%s\n",
                         pname, cinfo->lmin, "%");
                creo_log(cinfo, MSG_PLAIN, "  COLOR: Part \"%s\" color changed to rgb = %u/%u/%u\n",
                         pname, rgb[0], rgb[1], rgb[2]);
            }
        }

        /* Make the region comb */
        mk_comb(cinfo->wdbp, bu_vls_addr(rname), &wcomb.l, 1,
                NULL, NULL, (const unsigned char *)rgb,
                cinfo->reg_id, 0, 0, 0, 0, 0, 0);

    } else {
        /* Substitute a default color */
        rgb[0] = 255; rgb[1] = 255;  rgb[2] = 0;
        creo_log(cinfo, MSG_PLAIN, "  COLOR: Surface properties unavailable for \"%s\", default rgb = %u/%u/%u\n",
                 pname, rgb[0], rgb[1], rgb[2]);
        mk_comb(cinfo->wdbp, bu_vls_addr(rname), &wcomb.l, 1,
                NULL, NULL, (const unsigned char *)rgb,
                cinfo->reg_id, 0, 0, 0, 0, 0, 0);
    }

    /* Set the attributes for the solid/primitive */
    ptc_name = get_brlcad_name(cinfo, wname, NULL, N_CREO); 
    sdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(sname), LOOKUP_QUIET);
    db5_get_attributes(cinfo->wdbp->dbip, &cinfo->avs, sdp);
    bu_avs_add(&cinfo->avs, "importer", "creo-g");
    bu_avs_add(&cinfo->avs, "ptc_name", bu_vls_addr(ptc_name));
    if (have_rpp) {
        bu_vls_sprintf(&vstr, "%g", cinfo->bbox_area);
        bu_avs_add(&cinfo->avs, "rpp_area",  bu_vls_addr(&vstr));
        bu_vls_sprintf(&vstr, "%g", cinfo->bbox_diag);
        bu_avs_add(&cinfo->avs, "rpp_diag",  bu_vls_addr(&vstr));
        bu_vls_sprintf(&vstr, "%g", cinfo->bbox_vol);
        bu_avs_add(&cinfo->avs, "rpp_vol",   bu_vls_addr(&vstr));
        bu_avs_add(&cinfo->avs, "tess_fail", bu_vls_addr(ptc_name));
        bu_vls_free(&vstr);
    } else {
        bu_vls_sprintf(&vstr, "%d", cinfo->tess_count);
        bu_avs_add(&cinfo->avs, "tess_count" , bu_vls_addr(&vstr));
        bu_vls_sprintf(&vstr, "%g", cinfo->tess_chord);
        bu_avs_add(&cinfo->avs, "tess_chord" , bu_vls_addr(&vstr));
        bu_vls_sprintf(&vstr, "%g", cinfo->tess_angle);
        bu_avs_add(&cinfo->avs, "tess_angle" , bu_vls_addr(&vstr));
        bu_vls_free(&vstr);
    }

    db5_standardize_avs(&cinfo->avs);
    db5_update_attributes(sdp, &cinfo->avs, cinfo->wdbp->dbip);

    /* Set the Creo attributes for the region */
    rdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(rname), LOOKUP_QUIET);
    db5_get_attributes(cinfo->wdbp->dbip, &cinfo->avs, rdp);

    bu_avs_add(&cinfo->avs, "ptc_name", bu_vls_addr(ptc_name));

    if (ProMdlVerstampGet(model, &cstamp) == PRO_TK_NO_ERROR) {
        if (ProVerstampStringGet(cstamp, &verstr) == PRO_TK_NO_ERROR)
            bu_avs_add(&cinfo->avs, "ptc_version_stamp", verstr);
        ProVerstampStringFree(&verstr);
    }

    /* If the part has a material, extract the name, otherwise use a default */
    if (ProMaterialCurrentGet(ProMdlToSolid(model), &material) == PRO_TK_NO_ERROR) {
        ProWstringToString(mname, material.matl_name);
        for (int n = 0; mname[n]; n++)
            mname[n] = tolower(mname[n]);
    } else
        bu_strlcpy(mname, "undefined", MAX_MATL_NAME);

    bu_strlcpy(cinfo->mtl_key, mname, MAX_MATL_NAME);
    creo_log(cinfo, MSG_PLAIN, "   MATL: Part \"%s\" has ptc_material_name = \"%s\"\n", 
             pname, cinfo->mtl_key);

    /* Enforce material name attribute */
    bu_avs_add(&cinfo->avs, "ptc_material_name", mname);

    /* If a translation table exists, attempt to find the material */
    if (cinfo->mtl_rec > 0) {
        if (find_matl(cinfo)) {
            int p = cinfo->mtl_ptr;
            creo_log(cinfo, MSG_PLAIN, "   MATL: ========== \"%s\" ==========\n", cinfo->mtl_str[p]);
            creo_log(cinfo, MSG_PLAIN, "   MATL:           item = %d\n"         , p + 1);
            creo_log(cinfo, MSG_PLAIN, "   MATL:    material_id = %d\n"         , cinfo->mtl_id[p]);
            creo_log(cinfo, MSG_PLAIN, "   MATL:            los = %d%s\n"       , cinfo->mtl_los[p], "%");
            creo_log(cinfo, MSG_PLAIN, "   MATL: ---------- \"%s\" ----------\n", cinfo->mtl_str[p]);
            /* Add material attributes when material_id is positive */
            if (cinfo->mtl_id[p] >= 0) {
                bu_vls_sprintf(&vstr, "%d", cinfo->mtl_id[p]);
                bu_avs_add(&cinfo->avs, "material_id" , bu_vls_addr(&vstr));
                bu_vls_sprintf(&vstr, "%d", cinfo->mtl_los[p]);
                bu_avs_add(&cinfo->avs, "los"         , bu_vls_addr(&vstr));
                bu_vls_free(&vstr);
            } else {
                creo_log(cinfo, MSG_PLAIN, "   MATL: Attributes for \"%s\" are suppressed\n",
                         mname);
            }
        } else {
            creo_log(cinfo, MSG_PLAIN, "   MATL: Unable to find \"%s\" in the material table\n",
                     cinfo->mtl_key);
        }
    }

    /* When luminance increases, add the "ptc_rgb" attribute */
    if (rgb_mode > 0) {
        bu_vls_sprintf(&vstr, "%u/%u/%u", creo_rgb[0], creo_rgb[1], creo_rgb[2]);
        bu_avs_add(&cinfo->avs, "ptc_rgb" , bu_vls_addr(&vstr));
        bu_vls_free(&vstr);
    }

    /* Extract mass properties */
    if (ProSolidMassPropertyGet(ProMdlToSolid(model), NULL, &massprops) == PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_PLAIN,     "  SOLID: ========== \"%s\" ==========\n", pname);
        if (massprops.volume > 0.0)
            creo_log(cinfo, MSG_PLAIN, "  SOLID:        volume = %g\n", 
                     massprops.volume);
        else
            creo_log(cinfo, MSG_PLAIN, "  SOLID:        volume = \"%g\" %s\n", 
                     massprops.volume, "(invalid, using 0.0)");
        if (massprops.surface_area > 0.0)
            creo_log(cinfo, MSG_PLAIN, "  SOLID:  surface area = %g\n", 
                     massprops.surface_area);
        else
            creo_log(cinfo, MSG_PLAIN, "  SOLID:  surface area = \"%g\" %s\n", 
                     massprops.surface_area, "(invalid, using 0.0)");
        if (NEAR_EQUAL(massprops.density, 1.0, SMALL_FASTF)) {
            creo_log(cinfo, MSG_PLAIN, "  SOLID:       density = \"%g\" %s\n",
                     massprops.density,"(unassigned, using 1.0)");
            prtdens = 1.0;
        } else if (massprops.density < 0.0 ) {
            creo_log(cinfo, MSG_PLAIN, "  SOLID:       density = \"%g\" %s\n", 
                     massprops.density,"(invalid, using 1.0)");
            prtdens = 1.0;
        } else {
            creo_log(cinfo, MSG_PLAIN, "  SOLID:       density = %g\n", 
                     massprops.density);
            prtdens = massprops.density;
        }
        if (massprops.mass > 0.0)
            creo_log(cinfo, MSG_PLAIN, "  SOLID:          mass = %g\n", 
                     massprops.mass);
        else
            creo_log(cinfo, MSG_PLAIN, "  SOLID:          mass = %g %s\n", 
                     massprops.mass,  "(invalid, using 0.0)");
        creo_log(cinfo, MSG_PLAIN,     "  SOLID: ---------- \"%s\" ----------\n", pname);

        /* Create volume attribute */
        bu_vls_sprintf(&vstr, "%g", std::max<fastf_t>(massprops.volume,0.0));
        bu_avs_add(&cinfo->avs, "volume"      , bu_vls_addr(&vstr));

        /* Create surface area attribute */
        bu_vls_sprintf(&vstr, "%g", std::max<fastf_t>(massprops.surface_area,0.0));
        bu_avs_add(&cinfo->avs, "surface_area", bu_vls_addr(&vstr));

        /* Create mass attribute */
        bu_vls_sprintf(&vstr, "%g", std::max<fastf_t>(massprops.mass,0.0));
        bu_avs_add(&cinfo->avs, "mass"        , bu_vls_addr(&vstr));
        bu_vls_free(&vstr);

    } else if (ProPartDensityGet(ProMdlToPart(model), &prtdens) == PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_PLAIN, "  SOLID: \"%s\" has no mass property values\n", pname);
        /* If the part has a density, extract the value, otherwise use a default  */
        if (NEAR_EQUAL(prtdens, 1.0, SMALL_FASTF)) {
            creo_log(cinfo, MSG_PLAIN, "   MASS: Part \"%s\" has no material assigned, density = %g\n", 
                     pname, prtdens);
            prtdens = 1.0;
        } else if (prtdens < 0.0 ) {
            creo_log(cinfo, MSG_PLAIN, "   MASS: Part \"%s\" has a negative density, value \"%g\" ignored\n", 
                     pname, prtdens);
            prtdens = 1.0;
        } else
            creo_log(cinfo, MSG_PLAIN, "   MASS: Part \"%s\" has density = %g\n", 
                     pname, prtdens);
    } else
        prtdens = 1.0;

    if (NEAR_EQUAL(prtdens, 1.0, SMALL_FASTF))
        creo_log(cinfo, MSG_PLAIN, "   MASS: Part \"%s\" has been assigned a default value, density = %g\n", 
                 pname, prtdens);

    /* Create density attribute */
    bu_vls_sprintf(&vstr, "%g", prtdens);
    bu_avs_add(&cinfo->avs, "density", bu_vls_addr(&vstr));  /* Add density attribute */
    bu_vls_free(&vstr);

    /* Export user-supplied list of parameters */
    param_export(cinfo, model, pname);

    /* Update attributes stored on disk */
    db5_standardize_avs(&cinfo->avs);
    db5_update_attributes(rdp, &cinfo->avs, cinfo->wdbp->dbip);

    /* Increment the region id - this is a concern if we move to multithreaded generation... */
    cinfo->reg_id++;

cleanup:

    /* Unsuppress anything we suppressed */
    if (cinfo->elim_small && !pinfo->suppressed_features->empty()) {
        creo_log(cinfo, MSG_PLAIN, "   FEAT: Unsuppressing %zu features\n", pinfo->suppressed_features->size());
        supp_err = ProFeatureWithoptionsResume(ProMdlToSolid(model), &pinfo->suppressed_features->at(0),
                                               resume_opts, PRO_REGEN_NO_FLAGS);
        if (supp_err != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_PLAIN, "   FEAT: Failed to unsuppress feature(s) for \"%s\"\n", pname);
            cinfo->warn_feature_unsuppress = 1;
        } else
            creo_log(cinfo, MSG_PLAIN, "   FEAT: Successfully unsuppressed feature(s) for \"%s\"\n", pname);
    }

    delete pinfo->suppressed_features;
    delete pinfo->subtractions;
    BU_GET(pinfo, struct part_conv_info);

    /* Identify when bounding box replaces failed tessellation */
    cinfo->tess_bbox = (have_bbox & have_rpp);

    /* Log bounding box as a part failure */
    if (cinfo->tess_bbox)
        creo_log(cinfo, MSG_FAIL, "Part \"%s\" replaced with bounding box\n", pname);

    return tess_err;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
