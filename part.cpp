/**
 *                   P A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017-2022 United States Government as represented by
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
/**
 * @file part.cpp
 */

#include "common.h"
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
bad_triangle(struct creo_conv_info *cinfo,  int v1, int v2, int v3, struct bg_vert_tree *tree)
{
    double dist;
    double coord;

    if (v1 == v2 || v2 == v3 || v1 == v3) return 1;

    dist = 0;
    for (int i = 0; i < 3; i++) {
        coord = tree->the_array[v1*3+i] - tree->the_array[v2*3+i];
        dist += coord * coord;
    }
    dist = sqrt(dist);
    if (dist < cinfo->local_tol) return 1;

    dist = 0;
    for (int i = 0; i < 3; i++) {
        coord = tree->the_array[v2*3+i] - tree->the_array[v3*3+i];
        dist += coord * coord;
    }
    dist = sqrt(dist);
    if (dist < cinfo->local_tol) return 1;

    dist = 0;
    for (int i = 0; i < 3; i++) {
        coord = tree->the_array[v1*3+i] - tree->the_array[v3*3+i];
        dist += coord * coord;
    }
    dist = sqrt(dist);
    if (dist < cinfo->local_tol) return 1;

    return 0;
}


/**
 * If we have a Creo modeling feature that adds material, it can
 * impact the decision on which conversion methods are practical
 */
extern "C" int
feat_adds_material(ProFeattype feat_type)
{

    switch (feat_type) {
        case PRO_FEAT_DOME:         break;  /* solid types  */
        case PRO_FEAT_DOME2:        break;  /* add material */
        case PRO_FEAT_DRAFT:        break;  /*  (return 1)  */
        case PRO_FEAT_EAR:          break;
        case PRO_FEAT_FLANGE:       break;
        case PRO_FEAT_IMPORT:       break;
        case PRO_FEAT_LOC_PUSH:     break;
        case PRO_FEAT_MERGE:        break;
        case PRO_FEAT_MOLD:         break;
        case PRO_FEAT_NECK:         break;  /*     TODO     */
        case PRO_FEAT_OFFSET:       break;  /* review these */
        case PRO_FEAT_PIPE:         break;  /*   add more?  */
        case PRO_FEAT_PROTRUSION:   break;
        case PRO_FEAT_REPLACE_SURF: break;
        case PRO_FEAT_RIB:          break;
        case PRO_FEAT_SHAFT:        break;
        case PRO_FEAT_SHELL:        break;
        case PRO_FEAT_UDF:          break;
        case PRO_FEAT_UDF_THREAD:   break;
        default:                            /* other types  */
            return 0;                       /*  (return 0)  */
    }
    return 1;                               /* solid types  */
}


extern "C" const char *feat_status_to_str(ProFeatStatus feat_stat)
{
    static const char *feat_status[] = {
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


/**
 * This routine filters out which features should be visited by the
 * feature visit initiated in the output_part() routine.
 */
extern "C" ProError
feature_filter(ProFeature *feat, ProAppData data)
{
    ProError err;
    struct part_conv_info *pinfo = (struct part_conv_info *)data;

    err = ProFeatureTypeGet(feat, &pinfo->type);
    if (err != PRO_TK_NO_ERROR)
        return err;

    /* Handle holes, chamfers, and rounds only */
    if (pinfo->type == PRO_FEAT_HOLE || pinfo->type == PRO_FEAT_CHAMFER || pinfo->type == PRO_FEAT_ROUND)
        return PRO_TK_NO_ERROR;

    /*
     * If we encounter a protrusion (or any feature that adds material)
     * after a hole, we cannot convert previous holes to CSG
     */
    if (feat_adds_material(pinfo->type)) pinfo->csg_holes_supported = 0;

    /* Skip everything else */
    return PRO_TK_CONTINUE;
}


extern "C" ProError
check_dimension(ProDimension *dim, ProError UNUSED(status), ProAppData data)
{
    ProDimensiontype dim_type;
    ProError err;
    double tmp;
    struct part_conv_info *pinfo = (struct part_conv_info *)data;

    err = ProDimensionTypeGet(dim, &dim_type);
    if (err != PRO_TK_NO_ERROR)
        return err;

    switch (dim_type) {
        case PRODIMTYPE_RADIUS:
            err = ProDimensionValueGet(dim, &pinfo->radius);
            if (err != PRO_TK_NO_ERROR)
                return err;
            pinfo->diameter = 2.0 * pinfo->radius;
            pinfo->got_diameter = 1;
            break;
        case PRODIMTYPE_DIAMETER:
            err = ProDimensionValueGet(dim, &pinfo->diameter);
            if (err != PRO_TK_NO_ERROR)
                return err;
            pinfo->radius = pinfo->diameter / 2.0;
            pinfo->got_diameter = 1;
            break;
        case PRODIMTYPE_LINEAR:
            err = ProDimensionValueGet(dim, &tmp);
            if (err != PRO_TK_NO_ERROR)
                return err;
            if (pinfo->got_distance1) {
                pinfo->distance2 = tmp;
            } else {
                pinfo->got_distance1 = 1;
                pinfo->distance1 = tmp;
            }
            break;
    }
    return PRO_TK_NO_ERROR;
}


extern "C" ProError
do_feature_visit(ProFeature *feat, ProError UNUSED(status), ProAppData data)
{
    ProError    err;
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
            else if (pinfo->cinfo->do_facets_only)
                break;                                                  /* Skip hole when Facetizing */
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

    return PRO_TK_NO_ERROR;
}


extern "C" void
unsuppress_features(struct part_conv_info *pinfo)
{
    char    pname[CREO_NAME_MAX];
    wchar_t wname[CREO_NAME_MAX];

    ProError      err;
    ProFeature    feat;
    ProFeatStatus feat_stat;

    ProFeatureResumeOptions *resume_opts;
    resume_opts = (ProFeatureResumeOptions *) PRO_FEAT_RESUME_INCLUDE_PARENTS;

    (void)ProMdlMdlnameGet(pinfo->model, wname);
    ProWstringToString(pname, wname);

    for (unsigned int i = 0; i < pinfo->suppressed_features->size(); i++) {

        if (ProFeatureInit(ProMdlToSolid(pinfo->model), pinfo->suppressed_features->at(i), &feat) != PRO_TK_NO_ERROR)
            continue;
        if (ProFeatureStatusGet(&feat, &feat_stat) != PRO_TK_NO_ERROR)
            continue;

        if (feat_stat == PRO_FEAT_SUPPRESSED) {             /* Resume suppressed feature */
            creo_log(pinfo->cinfo, MSG_DEBUG, "\"%s\" unsuppressing feature %d", pname,
                     pinfo->suppressed_features->at(i));
            err = ProFeatureWithoptionsResume(ProMdlToSolid(pinfo->model), &(pinfo->suppressed_features->at(i)),
                                              resume_opts,PRO_REGEN_NO_FLAGS);
            switch (err) {                                  /* Report the outcome */
                case PRO_TK_NO_ERROR:
                    creo_log(pinfo->cinfo, MSG_DEBUG, "\"%s\" feature %d unsuppressed",
                             pname, pinfo->suppressed_features->at(i));
                    break;
                case PRO_TK_SUPP_PARENTS:
                    creo_log(pinfo->cinfo, MSG_DEBUG, "\"%s\" suppressed parents for feature %d not found\n",
                             pname, pinfo->suppressed_features->at(i));
                    break;
                default:
                    creo_log(pinfo->cinfo, MSG_DEBUG, "\"%s\" feature id %d unsuppression failed\n",
                             pname, pinfo->suppressed_features->at(i));
            }
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
    int creo_id = -1;
    int f_id = -1;
    ProSurface s = bdata->s;
    ProSurfaceIdGet(s, &creo_id);
    if (bdata->cs_to_onf->find(creo_id) != bdata->cs_to_onf->end()) f_id = bdata->cs_to_onf->find(creo_id)->second;
    if (f_id == -1) return PRO_TK_GENERAL_ERROR;
    ON_BrepLoop &nl = bdata->brep->NewLoop(ON_BrepLoop::outer, bdata->brep->m_F[f_id]);
    bdata->curr_loop_id = nl.m_loop_index;
    /* Does ProContourTraversal indicate inner vs outer loop? */
    ProContourTraversal tv;
    ProContourTraversalGet(c, &tv);
    if (tv == PRO_CONTOUR_TRAV_INTERNAL) nl.m_type = ON_BrepLoop::inner;

    ProContourEdgeVisit(s, c, edge_process, edge_filter, app_data);

    return PRO_TK_NO_ERROR;
}


extern "C" ProError
surface_process(ProSurface s, ProError UNUSED(status), ProAppData app_data) {
    int c_id;
    ProSurfacedata *ndata;
    ProSrftype s_type;
    ProUvParam uvmin = {DBL_MAX, DBL_MAX};
    ProUvParam uvmax = {DBL_MIN, DBL_MIN};
    ProSurfaceOrient s_orient;
    ProSurfaceshapedata s_shape;
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
            if (ProBsplinesrfdataGet(&s_shape, degree, &up_array, &vp_array, &w_array, &cntl_pnts, &ucnt, &vcnt, &ccnt) == PRO_TK_NO_ERROR) {
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
                    double x = (double)j / (double)(n - p);
                    int knot_index = j + p - 1;
                    ns->SetKnot(1, knot_index, x);
                }
                for (int i = m - p; i < m; i++)
                    ns->SetKnot(1, i, 1.0);

                if (ns->m_is_rat) {
                    for (int i = 0; i <= ucvmax; i++) {
                        for (int j = 0; j <= vcvmax; j++) {
                            ON_4dPoint cv;
                            cv[0] = cntl_pnts[i*(vcvmax+1)+j][0];
                            cv[1] = cntl_pnts[i*(vcvmax+1)+j][1];
                            cv[2] = cntl_pnts[i*(vcvmax+1)+j][2];
                            cv[3] = (w_array) ? w_array[i] : 0;
                            ns->SetCV(i, j, cv);
                        }
                    }
                } else {
                    for (int i = 0; i <= ucvmax; i++) {
                        for (int j = 0; j <= vcvmax; j++) {
                            ON_3dPoint cv;
                            cv[0] = cntl_pnts[i*(vcvmax+1)+j][0];
                            cv[1] = cntl_pnts[i*(vcvmax+1)+j][1];
                            cv[2] = cntl_pnts[i*(vcvmax+1)+j][2];
                            ns->SetCV(i, j, cv);
                        }
                    }
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
    ProError       err = PRO_TK_NO_ERROR;
    ProSolid      psol = ProMdlToSolid(model);
    ProSolidBody *pbdy = NULL;

    ON_Brep *nbrep = ON_Brep::New();
    struct brep_data bdata;
    bdata.brep = nbrep;
    bdata.cs_to_onf = new std::map<int, int>;
    bdata.ce_to_one = new std::map<int, int>;
    bdata.cc3d_to_on3dc = new std::map<int, int>;
    bdata.cc2d_to_on2dc = new std::map<int, int>;

    wchar_t wname[CREO_NAME_MAX];

    err = ProMdlMdlnameGet(model, wname);
    if (err != PRO_TK_NO_ERROR)
        return err;

    err = ProSolidBodiesCollect(psol, &pbdy);
    if (err != PRO_TK_NO_ERROR)
        return err;

    err = ProSolidBodySurfaceVisit(pbdy, surface_process, (ProAppData)&bdata);
    if (err != PRO_TK_NO_ERROR)
        return err;

    /* Output the solid */
    *sname = get_brlcad_name(cinfo, wname, "brep", N_SOLID);
    mk_brep(cinfo->wdbp, bu_vls_addr(*sname), nbrep);

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
    ProError err = PRO_TK_NO_ERROR;
    int success = 0;

    wchar_t wname[CREO_NAME_MAX];
    char    pname[CREO_NAME_MAX];

    struct bg_vert_tree *vert_tree = NULL;
    struct bg_vert_tree *norm_tree = NULL;
    std::vector<int> faces;
    std::vector<int> face_normals;

    int v1, v2, v3;
    int n1, n2, n3;
    int vert_no;
    int surface_count;

    double curr_error;
    double curr_angle;
    double factor = cinfo->creo_to_brl_conv;

   /*
    * Note: The below code works, but we can't use model units - Creo
    * "corrects" object sizes with matricies in the parent hierarchies
    * and correcting it here results in problems with those matricies.
    *
    *     ProError ustatus = creo_model_units(&factor, model);
    */

    (void)ProMdlMdlnameGet(model, wname);
    ProWstringToString(pname, wname);

    creo_log(cinfo, MSG_DEBUG, "Tessellating part \"%s\"...\n", pname);

    /* Tessellate part, going from coarse to fine tessellation */
    for (int i = 0; i <= cinfo->max_to_min_steps; ++i) {
        curr_error = cinfo->max_error - (i * cinfo->error_increment);
        curr_angle = cinfo->min_angle_cntrl + (i * cinfo->angle_increment);

        vert_tree = bg_vert_tree_create();
        norm_tree = bg_vert_tree_create();

        /*
         * Used for diagnostic output:
         *
         * creo_log(cinfo, MSG_SUCCESS, "Tessellating \"%s\" using:  error - %g, angle - %g\n", pname, curr_error, curr_angle);
         *
         */

        err = ProPartTessellate(ProMdlToPart(model), curr_error/cinfo->creo_to_brl_conv,
                                curr_angle, PRO_B_TRUE, &tess);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_DEBUG, "Failed to tessellate \"%s\" using:  error - %g, angle - %g\n",
                     pname, curr_error, curr_angle);
            /*
             * Used for diagnostic output:
             *
             * creo_log(cinfo, MSG_DEBUG, "\tmax_error = %g, min_error - %g, error_increment - %g\n", cinfo->max_error, cinfo->min_error, cinfo->error_increment);
             * creo_log(cinfo, MSG_DEBUG, "\tmax_angle_cntrl = %g, min_angle_cntrl - %g, angle_increment - %g\n", cinfo->max_angle_cntrl, cinfo->min_angle_cntrl, cinfo->angle_increment);
             * creo_log(cinfo, MSG_DEBUG, "\tcurr_error = %g, curr_angle - %g\n", curr_error, curr_angle);
             *
             */
            continue;
        }


        /* We got through the initial tests - how many surfaces do we have? */
        err = ProArraySizeGet((ProArray)tess, &surface_count);
        if (err != PRO_TK_NO_ERROR || surface_count < 1) {
            /* Free the tessellation memory */
            ProPartTessellationFree(&tess);
            tess = NULL;

            /* Free trees */
            bg_vert_tree_destroy(vert_tree);
            bg_vert_tree_destroy(norm_tree);

            err = PRO_TK_NOT_EXIST;
            goto tess_cleanup;
        }


        for (int surfno = 0; surfno < surface_count; surfno++) {
            for (int j = 0; j < tess[surfno].n_facets; j++) {
                /* Grab the triangle */
                vert_no = tess[surfno].facets[j][0];
                v1 = (int) bg_vert_tree_add(vert_tree,
                                            tess[surfno].vertices[vert_no][0],
                                            tess[surfno].vertices[vert_no][1],
                                            tess[surfno].vertices[vert_no][2],
                                            cinfo->local_tol_sq);
                vert_no = tess[surfno].facets[j][1];
                v2 = (int) bg_vert_tree_add(vert_tree,
                                            tess[surfno].vertices[vert_no][0],
                                            tess[surfno].vertices[vert_no][1],
                                            tess[surfno].vertices[vert_no][2],
                                            cinfo->local_tol_sq);
                vert_no = tess[surfno].facets[j][2];
                v3 = (int) bg_vert_tree_add(vert_tree,
                                            tess[surfno].vertices[vert_no][0],
                                            tess[surfno].vertices[vert_no][1],
                                            tess[surfno].vertices[vert_no][2],
                                            cinfo->local_tol_sq);

                if (bad_triangle(cinfo, v1, v2, v3, vert_tree))
                    continue;

                faces.push_back(v1);
                faces.push_back(v3);
                faces.push_back(v2);

                /* Grab the surface normals */
                if (cinfo->get_normals) {
                    vert_no = tess[surfno].facets[j][0];
                    VUNITIZE(tess[surfno].normals[vert_no]);
                    n1 = (int) bg_vert_tree_add(norm_tree,
                                                tess[surfno].normals[vert_no][0],
                                                tess[surfno].normals[vert_no][1],
                                                tess[surfno].normals[vert_no][2],
                                                cinfo->local_tol_sq);
                    vert_no = tess[surfno].facets[j][1];
                    VUNITIZE(tess[surfno].normals[vert_no]);
                    n2 = (int) bg_vert_tree_add(norm_tree,
                                                tess[surfno].normals[vert_no][0],
                                                tess[surfno].normals[vert_no][1],
                                                tess[surfno].normals[vert_no][2],
                                                cinfo->local_tol_sq);
                    vert_no = tess[surfno].facets[j][2];
                    VUNITIZE(tess[surfno].normals[vert_no]);
                    n3 = (int) bg_vert_tree_add(norm_tree,
                                                tess[surfno].normals[vert_no][0],
                                                tess[surfno].normals[vert_no][1],
                                                tess[surfno].normals[vert_no][2],
                                                cinfo->local_tol_sq);
                    face_normals.push_back(n1);
                    face_normals.push_back(n3);
                    face_normals.push_back(n2);
                }
            }
        }

        /* Check solidity */
        int bot_is_solid = !bg_trimesh_solid((int)vert_tree->curr_vert, (int)faces.size() / 3,
                                             vert_tree->the_array, &faces[0], NULL);

        /* If it's not solid and we're testing solidity, keep trying... */
        if (!bot_is_solid) {
            if (cinfo->check_solidity) {
                /* Free the tessellation memory */
                ProPartTessellationFree(&tess);
                tess = NULL;

                /* Free trees */
                bg_vert_tree_destroy(vert_tree);
                bg_vert_tree_destroy(norm_tree);

                creo_log(cinfo, MSG_DEBUG, "\"%s\" tessellation using error - %g, "
                                           "angle - %g failed solidity test, "
                                           "trying next level...\n",
                                           pname, curr_error, curr_angle);

            } else {
                creo_log(cinfo, MSG_DEBUG, "\"%s\" tessellation failed solidity test, "
                                           "but solidity requirement is disabled, "
                                           "continue...\n",
                                           pname);
                success = 1;
                break;
            }
        } else {
            success = 1;
            break;
        }
    }

    if (!success) {
        err = PRO_TK_NOT_EXIST;
        goto tess_cleanup;
    } else {
        err = PRO_TK_NO_ERROR;
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

    creo_log(cinfo, MSG_SUCCESS, "\"%s\" successfully tessellated using error: "
                                 "%g and angle: %g \n", pname, curr_error, curr_angle);

    for (unsigned int i = 0; i < vert_tree->curr_vert * 3; i++) {
        vert_tree->the_array[i] = vert_tree->the_array[i] * factor;
    }

    /* Output the solid - TODO - what is the correct ordering??? does CCW always work? */
    *sname = get_brlcad_name(cinfo, wname, "s", N_SOLID);
    if (cinfo->get_normals) {
        mk_bot_w_normals(cinfo->wdbp, bu_vls_addr(*sname), RT_BOT_SOLID, RT_BOT_CCW, 0,
                         vert_tree->curr_vert, (size_t)(faces.size()/3),
                         vert_tree->the_array, &faces[0], NULL,
                         NULL, (size_t)(face_normals.size()/3),
                         norm_tree->the_array, &face_normals[0]);
    } else {
        mk_bot(cinfo->wdbp, bu_vls_addr(*sname), RT_BOT_SOLID, RT_BOT_CCW, 0,
               vert_tree->curr_vert, (size_t)(faces.size()/3),
               vert_tree->the_array, &faces[0], NULL, NULL);
    }

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
 * The region will have the name from Pro/E with a .r suffix.
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
    char    mname[CREO_NAME_MAX];
    char    pname[CREO_NAME_MAX];
    wchar_t wname[CREO_NAME_MAX];
    
    double matldens;

    ProError         err = PRO_TK_NO_ERROR;
    ProFileName      msgfil = {0};
    ProMassProperty  massprops;
    ProMaterial      material;
    ProMdlType       type;

    ProFeatureResumeOptions *resume_opts;
    resume_opts = (ProFeatureResumeOptions *) PRO_FEAT_RESUME_INCLUDE_PARENTS;
    
    ProFeatureDeleteOptions *delete_opts;
    delete_opts = (ProFeatureDeleteOptions *) PRO_FEAT_DELETE_NO_OPTS;

    struct directory *rdp;
    struct directory *sdp;
    struct bu_attribute_value_set r_avs;
    struct bu_attribute_value_set s_avs;

    struct bu_vls vstr = BU_VLS_INIT_ZERO;

    struct bu_vls shader_args = BU_VLS_INIT_ZERO;
    struct bu_color color;
    fastf_t rgbflts[3];
    unsigned char rgb[3];
    ProModelitem mitm;
    ProSurfaceAppearanceProps aprops;

    struct wmember wcomb;
    struct bu_vls *rname = NULL;
    struct bu_vls *sname = NULL;
    struct bu_vls *creo_id = NULL;

    ProWVerstamp cstamp;
    char *verstr;

    struct bu_vls mdstr = BU_VLS_INIT_ZERO;

    Pro3dPnt bboxpnts[2];
    int have_bbox = 1;
    int failed_solid = 0;

    int surface_side = 0;  /* 0 = surface normal;  1 = other side */

    struct part_conv_info *pinfo;
    BU_GET(pinfo, struct part_conv_info);
    pinfo->cinfo = cinfo;
    pinfo->csg_holes_supported = 1;
    pinfo->model = model;
    pinfo->suppressed_features = new std::vector<int>;
    pinfo->subtractions = new std::vector<struct directory *>;

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    (void)ProMdlMdlnameGet(model, wname);
    ProMdlTypeGet(model, &type);

    ProWstringToString(pname, wname);
    creo_log(cinfo, MSG_SUCCESS, "Processing \"%s\"\n", pname);

    /*
     * TODO - add some up-front logic to detect parts with no
     * associated geometry.
     */

    /* Collect info about things that might be eliminated */
    if (cinfo->do_elims) {
        /*
         * Apply user supplied criteria to see if we have anything
         * to suppress
         */
        ProSolidFeatVisit(ProMdlToSolid(model), do_feature_visit, feature_filter, (ProAppData)pinfo);

        /* If we've got anything to suppress, go ahead and do it. */
        if (!pinfo->suppressed_features->empty()) {
            err = ProFeatureWithoptionsSuppress(ProMdlToSolid(model), &(pinfo->suppressed_features->at(0)),
                                                delete_opts, PRO_REGEN_NO_FLAGS);
            /* If something went wrong, need to undo just the suppressions we added */
            if (err != PRO_TK_NO_ERROR) {
                creo_log(cinfo, MSG_DEBUG, "\"%s\" failed to suppress features\n", pname);
                unsuppress_features(pinfo);
            } else {
                creo_log(cinfo, MSG_DEBUG, "\"%s\" features suppressed, continuing with conversion...\n", pname);
            }
        }
    }

    /*
     * Get bounding box of a solid using "ProSolidOutlineGet"
     * TODO - may want to use this to implement relative facetization tolerance...
     */
    if (ProSolidOutlineGet(ProMdlToSolid(model), bboxpnts) != PRO_TK_NO_ERROR) have_bbox = 0;

    /*
     * TODO - support an option to convert to ON_Brep
     *
     *    err = opennurbs_part(cinfo, model, &sname);
     */

    /* Tessellate */
    err = tessellate_part(cinfo, model, &sname);

    /* Deal with the solid conversion results */
    if (err == PRO_TK_NOT_EXIST) {
        /* Failed!!! */
        creo_log(cinfo, MSG_DEBUG, "\"%s\" tessellation failed\n", pname);
        if (cinfo->debug_bboxes && have_bbox) {
            /*
             * A failed solid conversion with a bounding box indicates a
             * problem.  Rather than ignore it, put the bbox in the .g file
             * as a placeholder.
             */
            point_t rmin, rmax;
            failed_solid = 1;
            sname = get_brlcad_name(cinfo, wname, "rpp", N_SOLID);
            rmin[0] = bboxpnts[0][0];
            rmin[1] = bboxpnts[0][1];
            rmin[2] = bboxpnts[0][2];
            rmax[0] = bboxpnts[1][0];
            rmax[1] = bboxpnts[1][1];
            rmax[2] = bboxpnts[1][2];
            mk_rpp(cinfo->wdbp, bu_vls_addr(sname), rmin, rmax);
            creo_log(cinfo, MSG_FAIL, "\"%s\" replaced with bounding box\n", pname);
            goto have_part;
        } else {
            wchar_t *stable = stable_wchar(cinfo, wname);
            if (!stable) {
                creo_log(cinfo, MSG_DEBUG, "\"%s\" no stable version of name found\n", pname);
            } else {
                cinfo->empty->insert(stable);
            }
            creo_log(cinfo, MSG_FAIL, "\"%s\" not converted\n", pname);
            goto cleanup;
        }
    }

have_part:
    /*
     * We've got a part - the output for a part is a parent region and
     * the solid underneath it.
     */

    BU_LIST_INIT(&wcomb.l);
    rname = get_brlcad_name(cinfo, wname, "r", N_REGION);

    /* Add the solid to the region comb */
    (void)mk_addmember(bu_vls_addr(sname), &wcomb.l, NULL, WMOP_UNION);

    /* Add any subtraction solids created by the hole handling */
    for (unsigned int i = 0; i < pinfo->subtractions->size(); i++) {
        (void)mk_addmember(pinfo->subtractions->at(i)->d_namep, &wcomb.l, NULL, WMOP_SUBTRACT);
    }

    /*
     * Get the surface properties from the part,
     * output the region comb
     */
    ProMdlToModelitem(model, &mitm);
    if (ProSurfaceSideAppearancepropsGet(&mitm, surface_side, &aprops) == PRO_TK_NO_ERROR) {
        /* Use the colors, ... that were set in Creo */
        rgbflts[0] = aprops.color_rgb[0];
        rgbflts[1] = aprops.color_rgb[1];
        rgbflts[2] = aprops.color_rgb[2];
        bu_color_from_rgb_floats(&color, rgbflts);
        bu_color_to_rgb_chars(&color, rgb);

        /* Shader args */
        /*
         * TODO Make exporting material optional
         */
        bu_vls_sprintf(&shader_args, "{");
        if (!NEAR_ZERO(aprops.transparency, SMALL_FASTF)) bu_vls_printf(&shader_args, " tr %g", aprops.transparency);
        if (!NEAR_EQUAL(aprops.shininess, 1.0, SMALL_FASTF)) bu_vls_printf(&shader_args, " sh %d", (int)(aprops.shininess * 18 + 2.0));
        if (!NEAR_EQUAL(aprops.diffuse, 0.3, SMALL_FASTF)) bu_vls_printf(&shader_args, " di %g", aprops.diffuse);
        if (!NEAR_EQUAL(aprops.highlite, 0.7, SMALL_FASTF)) bu_vls_printf(&shader_args, " sp %g", aprops.highlite);
        bu_vls_printf(&shader_args, "}");

        /* Make the region comb */
        mk_comb(cinfo->wdbp, bu_vls_addr(rname), &wcomb.l, 1,
                NULL, NULL, (const unsigned char *)rgb,
                cinfo->reg_id, 0, 0, 0, 0, 0, 0);
    } else {
        /* Something is wrong, but just ignore the missing properties */
        creo_log(cinfo, MSG_DEBUG, "ERROR: Unable to get surface properties for \"%s\"\n", pname);
        mk_comb(cinfo->wdbp, bu_vls_addr(rname), &wcomb.l, 1, NULL, NULL, NULL, cinfo->reg_id, 0, 0, 0, 0, 0, 0);
    }

    /* Set the CREO_NAME attribute for the solid/primitive */
    creo_id = get_brlcad_name(cinfo, wname, NULL, N_CREO);
    sdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(sname), LOOKUP_QUIET);
    db5_get_attributes(cinfo->wdbp->dbip, &s_avs, sdp);
    bu_avs_add(&s_avs, "CREO_NAME", bu_vls_addr(creo_id));
    if (failed_solid) {
        bu_avs_add(&s_avs, "SOLID_STATUS", "FAILED");
    }
    db5_standardize_avs(&s_avs);
    db5_update_attributes(sdp, &s_avs, cinfo->wdbp->dbip);

    /* Set the CREO attributes for the region */
    rdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(rname), LOOKUP_QUIET);
    db5_get_attributes(cinfo->wdbp->dbip, &r_avs, rdp);
    bu_avs_add(&r_avs, "CREO_NAME", bu_vls_addr(creo_id));

    if (ProMdlVerstampGet(model, &cstamp) == PRO_TK_NO_ERROR) {
        if (ProVerstampStringGet(cstamp, &verstr) == PRO_TK_NO_ERROR) {
            bu_avs_add(&r_avs, "CREO_VERSION_STAMP", verstr);
        }
        ProVerstampStringFree(&verstr);
    }

    /* If the part has a material, extract the name, otherwise use a default */
    if (ProMaterialCurrentGet(ProMdlToSolid(model), &material) == PRO_TK_NO_ERROR)
        ProWstringToString(mname, material.matl_name);
    else
        strcpy(mname,"UNDEFINED");

    bu_avs_add(&r_avs, "material_name", mname);  /* Add material name attribute */

    /* If the part has a density, extract the value, otherwise use a default  */
    if (ProPartDensityGet(ProMdlToPart(model), &matldens) == PRO_TK_NO_ERROR)
        bu_vls_sprintf(&mdstr, "%g", matldens);
    else
        bu_vls_sprintf(&mdstr, "%g", -1.0);

    bu_avs_add(&r_avs, "density", bu_vls_addr(&mdstr));  /* Add density attribute */
    bu_vls_free(&mdstr);

    /* Calculate mass properties */
    if (ProSolidMassPropertyGet(ProMdlToSolid(model), NULL, &massprops) == PRO_TK_NO_ERROR) {
        if (massprops.density > 0.0) {
            bu_vls_sprintf(&vstr, "%g", massprops.density);
            bu_avs_add(&r_avs, "density", bu_vls_addr(&vstr));
        }
        if (massprops.mass > 0.0) {
            bu_vls_sprintf(&vstr, "%g", massprops.mass);
            bu_avs_add(&r_avs, "mass", bu_vls_addr(&vstr));
        }
        if (massprops.volume > 0.0) {
            bu_vls_sprintf(&vstr, "%g", massprops.volume);
            bu_avs_add(&r_avs, "volume", bu_vls_addr(&vstr));
        }
        bu_vls_free(&vstr);
    }

    /* If we have a user supplied list of attributes to save, do it */
    if (cinfo->attrs->size() > 0) {
        for (unsigned int i = 0; i < cinfo->attrs->size(); i++) {
            char *attr_val = NULL;
            const char *arg = cinfo->attrs->at(i);
            creo_attribute_val(&attr_val, arg, model);
            if (attr_val) {
                bu_avs_add(&r_avs, arg, attr_val);
                bu_free(attr_val, "value string");
            }
        }
    }

    /* Update attributes stored on disk */
    db5_standardize_avs(&r_avs);
    db5_update_attributes(rdp, &r_avs, cinfo->wdbp->dbip);

    /* Increment the region id - this is a concern if we move to multithreaded generation... */
    cinfo->reg_id++;

cleanup:

    /* Unsuppress anything we suppressed */
    if (cinfo->do_elims && !pinfo->suppressed_features->empty()) {
        creo_log(cinfo, MSG_SUCCESS, "Unsuppressing %zu features\n", pinfo->suppressed_features->size());
        err = ProFeatureWithoptionsResume(ProMdlToSolid(model), &pinfo->suppressed_features->at(0),
                                          resume_opts,PRO_REGEN_NO_FLAGS);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_DEBUG, "\"%s\" failed to unsuppress features\n", pname);
            cinfo->warn_feature_unsuppress = 1;
            return err;
        }
        creo_log(cinfo, MSG_DEBUG, "Successfully unsuppressed features for \"%s\"\n", pname);
    }

    delete pinfo->suppressed_features;
    delete pinfo->subtractions;
    BU_GET(pinfo, struct part_conv_info);

    return err;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8