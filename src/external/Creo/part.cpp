/**
 *                   P A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017-2024 United States Government as represented by
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

/*                                                                             */
/* Synopsis of Part Routines:                                                  */
/*                                                                             */
/* generic_filter       Filter for Creo feature dimensions                     */
/* failed_facet         Identifies facets w/ duplicate vertices or short edges */
/* get_facet_value      Extract facet corner values                            */
/* v_to_uv              Compute unit vector                                    */
/* pts_to_uv            Compute unit vector between two points                 */
/* p_cross_q            Compute vector cross product                           */
/* p_dot_q              Compute vector scalar product                          */
/* facet_winds_ccw      Does facet wind counter-clockwise?                     */
/* feat_adds_material   Does feature add material?                             */
/* feat_status_to_str   Returns a string for the given ProFeatStatus           */
/* feature_filter       Filter employed by the output_part() routine           */
/* add_dependent_feats  Add dependent features to the suppressed features list */
/* get_dimension        Extracts dimension values based on their type          */
/* feature_visit        Collect and filter feature dimensions                  */
/* suppress_features    Suppress all previously-identified model features      */
/* resume_features      Resume all previously-suppressed model features        */
/* edge_filter          Filter employed by the ProContourEdgeVisit             */
/* edge_process         Gathers edge data for the ProContourEdgeVisit          */
/* contour_filter       Filter employed by the ProSurfaceContourVisit          */
/* contour_process      Gathers contour data for ProContourEdgeVisit           */
/* surface_process      Gathers surface data for ProSolidBodySurfaceVisit      */
/* opennurbs_part       Future support for oppennurbs part conversion          */
/* tessellate_part      Creates a tessellated version of a Creo part           */
/* output_part          Output part and related attributes as a BRL-CAD region */
/*                                                                             */

#include "common.h"
#include <algorithm>
#include "creo-brl.h"


/* Filter for Creo feature dimensions */
extern "C" ProError
generic_filter(ProDimension *UNUSED(dim), ProAppData UNUSED(data)) {
    return PRO_TK_NO_ERROR;
}


/* Identifies facets w/ duplicate vertices or short edges */
extern "C" int
failed_facet(double min_edge, size_t v1, size_t v2, size_t v3, struct bg_vert_tree *tree)
{
    double dist;
    double coord;

    /* Check for duplicate vertices */
    if (v1 == v2 || v2 == v3 || v1 == v3)
        return 1;

    /* Check for minimum edge distance from v1 to v2 */
    dist = 0;
    for (size_t i = 0; i < 3; i++) {
        coord = tree->the_array[v1*3+i] - tree->the_array[v2*3+i];
        dist += coord * coord;
    }
    dist = sqrt(dist);
    if (dist < abs(min_edge))
        return 1;

    /* Check for minimum edge distance from v2 to v3 */
    dist = 0;
    for (size_t i = 0; i < 3; i++) {
        coord = tree->the_array[v2*3+i] - tree->the_array[v3*3+i];
        dist += coord * coord;
    }
    dist = sqrt(dist);
    if (dist < abs(min_edge))
        return 1;

    /* Check for minimum edge distance from v1 to v3 */
    dist = 0;
    for (size_t i = 0; i < 3; i++) {
        coord = tree->the_array[v1*3+i] - tree->the_array[v3*3+i];
        dist += coord * coord;
    }
    dist = sqrt(dist);
    if (dist < abs(min_edge))
        return 1;

    return 0;
}


/* Extract facet corner values */
extern "C" void
get_facet_value(size_t p, struct bg_vert_tree *tree, double *value)
{
    for (size_t i = 0; i < 3; i++)
        value[(int)i] = tree->the_array[p*3 + i];
}


/* Compute unit vector */
extern "C" void
v_to_uv(double *v)
{
    if (!v)
        return;
    else {
        double sum = 0.0;
        for (int i = 0; i < 3; i++)
            sum += v[i] * v[i];
        if (!NEAR_ZERO(sum, SMALL_FASTF))
            for (int i = 0; i < 3; i++)
                v[i] = v[i]/sqrt(sum);
        }
}


/* Compute unit vector between two points */
extern "C" void
pts_to_uv(double *p, double *q, double *r)
{
    if (!p || !q)
        r = NULL;
    else {
        for (int i = 0; i < 3; i++)
            r[i] = q[i] - p[i];
        v_to_uv(r);
    }
}


/* Compute vector cross product */
extern "C" void
p_cross_q(double *p, double *q, double *r)
{
    if (!p || !q)
        r = NULL;
    else {
        r[0] = (p[1]*q[2] - p[2]*q[1]);
        r[1] = (p[2]*q[0] - p[0]*q[2]);
        r[2] = (p[0]*q[1] - p[1]*q[0]);
    }
}


/* Compute vector scalar product */
extern "C" double
p_dot_q(double *p, double *q)
{
    if (!p || !q)
        return 0.0;
    else
        return (p[0]*q[0] + p[1]*q[1] + p[2]*q[2]);
}


/* Does facet wind counter-clockwise? */
extern "C" int
facet_winds_ccw(size_t v1, size_t v2, size_t v3, size_t n1, size_t n2, size_t n3, struct bg_vert_tree *vert, struct bg_vert_tree *norm)
{
    double u1[3], u2[3], u3[3];
    double w1[3], w2[3], w3[3];

    double u12[3], u13[3];
    double nrm[3], dir[3];

    /* Extract corner vertices */
    get_facet_value(v1, vert, u1);
    get_facet_value(v2, vert, u2);
    get_facet_value(v3, vert, u3);

    /* Extract corner normals */
    get_facet_value(n1, norm, w1);
    get_facet_value(n2, norm, w2);
    get_facet_value(n3, norm, w3);

    /* Find the facet unit normal */
    pts_to_uv(u1,  u2,  u12);
    pts_to_uv(u1,  u3,  u13);
    p_cross_q(u12, u13, nrm);
    v_to_uv(nrm);

    /* Average the three vertex normals */
    dir[0] = (w1[0] + w2[0] + w3[0])/3.0;
    dir[1] = (w1[1] + w2[1] + w3[1])/3.0;
    dir[2] = (w1[2] + w2[2] + w3[2])/3.0;
    v_to_uv(dir);

    return (p_dot_q(nrm, dir) > 0.0) ? 1 : 0;
}


/*
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


/* Returns a string for the given ProFeatStatus */
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


/*
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


/* Add dependent features to the suppressed features list */
extern "C" void
add_dependent_feats(struct part_conv_info *pinfo)
{
    ProFeature   *pfeat;
    ProFeature    dfeat;
    ProFeatStatus dstat;
    ProMdl        model = pinfo->cinfo->curr_model;

    int pcount = pinfo->suppressed_features->size();
    int dcount = 0;
    int *dfeats;

    /* Begin with most recent parent feature */
    pfeat = pinfo->feat;
    (void)ProFeatureChildrenGet(pfeat, &dfeats, &dcount);

    /* Identify dependent features */
    for (unsigned int d = 0; d < dcount; d++) {
        if (ProFeatureInit(ProMdlToSolid(model), dfeats[d], &dfeat) != PRO_TK_NO_ERROR)
            continue;
        if (ProFeatureStatusGet(&dfeat, &dstat) != PRO_TK_NO_ERROR)
            continue;
        if (dstat == PRO_FEAT_ACTIVE) {
            creo_log(pinfo->cinfo, MSG_FEAT, "Found dependent feature ID #%d\n", dfeats[d]);
            pinfo->suppressed_features->push_back(dfeats[d]);
        }
    }

    /* Did we add active dependent features? */
    dcount = pinfo->suppressed_features->size() - pcount;
    if (dcount > 1)
        creo_log(pinfo->cinfo, MSG_FEAT, "Feature ID #%d included a total of %d dependent features\n",
                                          pfeat->id, dcount);
    /* Clean-up */
    (void)ProArrayFree((ProArray*)&dfeats);

    return;
}


/* Extracts dimension values based on their type */
extern "C" ProError
get_dimension(ProDimension *dim, ProError UNUSED(status), ProAppData data)
{
    ProDimensiontype dim_type;
    ProError err = PRO_TK_GENERAL_ERROR;

    struct part_conv_info *pinfo = (struct part_conv_info *)data;

    /* Initialize while converting input values to part units */
    double min_hole    = pinfo->cinfo->min_hole/pinfo->cinfo->part_to_mm;
    double min_round   = pinfo->cinfo->min_round/pinfo->cinfo->part_to_mm;
    double min_chamfer = pinfo->cinfo->min_chamfer/pinfo->cinfo->part_to_mm;
    double val;

    err = ProDimensionTypeGet(dim, &dim_type);
    if (err != PRO_TK_NO_ERROR)
        return err;

    switch (dim_type) {           /* refer to ProDimension.h */
        case PRODIMTYPE_RADIUS:
            err = ProDimensionValueGet(dim, &pinfo->radius);
            if (err == PRO_TK_NO_ERROR) {
                pinfo->diameter = 2.0 * pinfo->radius;
                pinfo->ok_dia   = 1;
            }
            break;
        case PRODIMTYPE_DIAMETER:
            err = ProDimensionValueGet(dim, &pinfo->diameter);
            if (err == PRO_TK_NO_ERROR) {
                pinfo->radius = pinfo->diameter / 2.0;
                pinfo->ok_dia = 1;
            }
            break;
        case PRODIMTYPE_LINEAR:
            err = ProDimensionValueGet(dim, &val);
            if (err == PRO_TK_NO_ERROR) {
                if (pinfo->ok_dist)
                    pinfo->dist2 = val;
                else {
                    pinfo->dist1   = val;
                    pinfo->ok_dist = 1;
                }
            }
            break;
    }

    return err;
}


/* Collect and filter feature dimensions */
extern "C" ProError
feature_visit(ProFeature *feat, ProError UNUSED(status), ProAppData data)
{
    ProError    err = PRO_TK_GENERAL_ERROR;
    ProFeattype feat_type;

    struct part_conv_info *pinfo = (struct part_conv_info *)data;

    /* Initialize with input values converted to part units */
    double min_hole    = pinfo->cinfo->min_hole/pinfo->cinfo->part_to_mm;
    double min_round   = pinfo->cinfo->min_round/pinfo->cinfo->part_to_mm;
    double min_chamfer = pinfo->cinfo->min_chamfer/pinfo->cinfo->part_to_mm;

    /* Current number of features */
    int feat_count     = pinfo->suppressed_features->size();

    pinfo->feat     = feat;
    pinfo->radius   = 0.0;
    pinfo->diameter = 0.0;
    pinfo->dist1    = 0.0;
    pinfo->dist2    = 0.0;
    pinfo->ok_dia   = 0;
    pinfo->ok_dist  = 0;

    err = ProFeatureDimensionVisit(feat, get_dimension, generic_filter, (ProAppData)pinfo);
    if (err != PRO_TK_NO_ERROR)
        return err;

    feat_type = pinfo->type;

    switch (feat_type) { 
        case PRO_FEAT_HOLE:                           /* Suppress small holes */
            if (pinfo->diameter < min_hole) {
                creo_log(pinfo->cinfo, MSG_FEAT, "Hole diameter < %g, feature ID #%d \n",
                                                  min_hole, feat->id);
                pinfo->suppressed_features->push_back(feat->id);
            } else if (pinfo->cinfo->facets_only) {   /* Skip when tessellating */
                goto skip;
            } else if (subtract_hole(pinfo)) {        /* Suppress before tessellating */
                creo_log(pinfo->cinfo, MSG_FEAT, "Hole diameter < %g, feature ID #%d \n",
                                                  min_hole, feat->id);
                pinfo->suppressed_features->push_back(feat->id);
            } 
            break;
        case PRO_FEAT_ROUND:                          /* Suppress small rounds */
            if (pinfo->ok_dia && pinfo->radius < min_round) {
                creo_log(pinfo->cinfo, MSG_FEAT, "Radius < %g, feature ID #%d \n",
                                                  min_round, feat->id);
                pinfo->suppressed_features->push_back(feat->id);
            }
            break;
        case PRO_FEAT_CHAMFER:                        /* Suppress small chamfers */
            if (pinfo->ok_dist && pinfo->dist1 < min_chamfer &&
                pinfo->dist2 < min_chamfer) {
                creo_log(pinfo->cinfo, MSG_FEAT, "Chamfer < %g, feature ID #%d \n",
                                                  min_chamfer, feat->id);
                pinfo->suppressed_features->push_back(feat->id);
            }
            break;
    }

    /* Were there any additional features identified? */
    if (pinfo->suppressed_features->size() > feat_count)
        add_dependent_feats(pinfo);

skip: return PRO_TK_NO_ERROR;
}


/* Suppress all previously-identified model features */
extern "C" ProError
suppress_features(struct part_conv_info *pinfo)
{
    ProError err   = PRO_TK_GENERAL_ERROR;
    ProMdl   model = pinfo->cinfo->curr_model;
    int feat_count = pinfo->suppressed_features->size();
    int err_count  = 0;

    /* Establish delete options array */
    ProFeatureDeleteOptions *delete_opts = NULL;
    (void)ProArrayAlloc(1, sizeof(ProFeatureDeleteOptions),
                        1, (ProArray*)&delete_opts);
    delete_opts[0] = (ProFeatureDeleteOptions) PRO_FEAT_DELETE_CLIP;

    /* Establish feat_id array */
    int *feat_id = NULL;
    (void)ProArrayAlloc(1, sizeof(int), 1, (ProArray*)&feat_id);

    /* Suppress features and report results */
    for (unsigned int i = 0; i < feat_count; i++) {
        feat_id[0] = pinfo->suppressed_features->at(i);
        err= ProFeatureWithoptionsSuppress(ProMdlToSolid(model),
                                           feat_id,
                                           delete_opts,
                                           PRO_REGEN_NO_FLAGS);
        /* Report the outcome */
        switch (err) {
            case PRO_TK_NO_ERROR:
                creo_log(pinfo->cinfo, MSG_FEAT, "Successfully suppressed ID #%d in \"%s\"\n",
                                                  feat_id[0],
                                                  bu_vls_addr(pinfo->cinfo->curr_pname));
                break;
            case PRO_TK_BAD_INPUTS:
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: ProToolkit \"ProFeatureWithoptionsSuppress\"\n");
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: \"invalid arguments\" for ID #%d in \"%s\"\n",
                                                  feat_id[0],
                                                  bu_vls_addr(pinfo->cinfo->curr_pname));
                err_count++;
                break;
            case PRO_TK_BAD_CONTEXT:
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: ProToolkit \"ProFeatureWithoptionsSuppress\"\n");
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: \"invalid regen flags\" for ID #%d in \"%s\"\n",
                                                  feat_id[0],
                                                  bu_vls_addr(pinfo->cinfo->curr_pname));
                err_count++;
                break;
            case PRO_TK_NOT_VALID:
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: ProToolkit \"ProFeatureWithoptionsSuppress\"\n");
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: \"not permitted\" for ID #%d in \"%s\"\n",
                                                  feat_id[0],
                                                  bu_vls_addr(pinfo->cinfo->curr_pname));
                err_count++;
                break;
            default:
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: ProToolkit \"ProFeatureWithoptionsSuppress\"\n");
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: \"general failure\" for ID #%d in \"%s\"\n",
                                                  feat_id[0],
                                                  bu_vls_addr(pinfo->cinfo->curr_pname));
                err_count++;
        }
    }

    if (err_count > 0)
        creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: Failed to suppress %d feature(s) in \"%s\"\n",
                                          err_count,
                                          bu_vls_addr(pinfo->cinfo->curr_pname));
    else
        creo_log(pinfo->cinfo, MSG_FEAT, "Successfully suppressed %d feature(s) in \"%s\"\n",
                                          feat_count,
                                          bu_vls_addr(pinfo->cinfo->curr_pname));

    /* Free the ProFeature options arrays */
    (void)ProArrayFree((ProArray*)&delete_opts);
    (void)ProArrayFree((ProArray*)&feat_id);

    return (err_count > 0) ? PRO_TK_GENERAL_ERROR : PRO_TK_NO_ERROR;
}


/* Resume all previously-suppressed model features */
extern "C" ProError
resume_features(struct part_conv_info *pinfo)
{
    ProError err   = PRO_TK_GENERAL_ERROR;
    ProMdl   model = pinfo->cinfo->curr_model;
    int feat_count = pinfo->suppressed_features->size();
    int err_count  = 0;

    /* Establish resume options array */
    ProFeatureResumeOptions *resume_opts = NULL;
    (void)ProArrayAlloc(1, sizeof(ProFeatureResumeOptions),
                        1, (ProArray*)&resume_opts);
    resume_opts[0] = (ProFeatureResumeOptions) PRO_FEAT_RESUME_INCLUDE_PARENTS;

    /* Establish feat_id array */
    int *feat_id = NULL;
    (void)ProArrayAlloc(1, sizeof(int), 1, (ProArray*)&feat_id);

    /* Resume features and report results */
    for (unsigned int i = 0; i < feat_count; i++) {
        feat_id[0] = pinfo->suppressed_features->at(i);
        err = ProFeatureWithoptionsResume(ProMdlToSolid(model),
                                          feat_id,
                                          resume_opts,
                                          PRO_REGEN_NO_FLAGS);
        /* Report the outcome */
        switch (err) {
            case PRO_TK_NO_ERROR:
                creo_log(pinfo->cinfo, MSG_FEAT, "Successfully resumed ID #%d in \"%s\"\n",
                                                  feat_id[0],
                                                  bu_vls_addr(pinfo->cinfo->curr_pname));
                break;
            case PRO_TK_SUPP_PARENTS:
                creo_log(pinfo->cinfo, MSG_FEAT, "WARNING: ProToolkit \"ProFeatureWithoptionsResume\"\n");
                creo_log(pinfo->cinfo, MSG_FEAT, "WARNING: \"suppressed parents\" for ID #%d in \"%s\"\n",
                                                  feat_id[0],
                                                  bu_vls_addr(pinfo->cinfo->curr_pname));
                err_count++;
                break;
            case PRO_TK_BAD_INPUTS:
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: ProToolkit \"ProFeatureWithoptionsResume\"\n");
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: \"invalid arguments\" for ID #%d in \"%s\"\n",
                                                  feat_id[0],
                                                  bu_vls_addr(pinfo->cinfo->curr_pname));
                err_count++;
                break;
            case PRO_TK_BAD_CONTEXT:
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: ProToolkit \"ProFeatureWithoptionsResume\"\n");
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: \"invalid regeneration flags\" for ID #%d in \"%s\"\n",
                                                  feat_id[0],
                                                  bu_vls_addr(pinfo->cinfo->curr_pname));
                err_count++;
                break;
            default:
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: ProToolkit \"ProFeatureWithoptionsResume\"\n");
                creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: Part \"%s\" feature ID #%d failed to resume\n",
                                                  feat_id[0],
                                                  pinfo->suppressed_features->at(i));
                err_count++;
        }
    }

    if (err_count > 0)
        creo_log(pinfo->cinfo, MSG_FEAT, "ERROR: Failed to resume %d feature(s) in \"%s\"\n",
                                          err_count,
                                          bu_vls_addr(pinfo->cinfo->curr_pname));
    else
        creo_log(pinfo->cinfo, MSG_FEAT, "Successfully resumed %d feature(s) in \"%s\"\n",
                                          feat_count,
                                          bu_vls_addr(pinfo->cinfo->curr_pname));

    /* Free the ProFeature options arrays */
    (void)ProArrayFree((ProArray*)&resume_opts);
    (void)ProArrayFree((ProArray*)&feat_id);

    return (err_count > 0) ? PRO_TK_GENERAL_ERROR : PRO_TK_NO_ERROR;
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


/* Filter for Creo part edges */
extern "C" ProError
edge_filter(ProEdge UNUSED(e), ProAppData UNUSED(app_data)) {
    return PRO_TK_NO_ERROR;
}


/*
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


/* Filter employed by the ProSurfaceContourVisit routine    */
extern "C" ProError
contour_filter(ProContour UNUSED(c), ProAppData UNUSED(app_data)) {
    return PRO_TK_NO_ERROR;
}


/*
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


/* Gathers surface data for ProSolidBodySurfaceVisit routine   */
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


/* Future support for oppennurbs part conversion */
extern "C" ProError
opennurbs_part(struct creo_conv_info *cinfo, struct bu_vls **sname)
{
    ProError        err = PRO_TK_GENERAL_ERROR;
    ProMdl        model = cinfo->curr_model;
    ProSolid       psol = ProMdlToSolid(model);
    ProSolidBody  *pbdy = NULL;

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
    bdata.cs_to_onf     = new std::map<int, int>;
    bdata.ce_to_one     = new std::map<int, int>;
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


/* Creates a tessellated version of a Creo part */
extern "C" ProError
tessellate_part(struct creo_conv_info *cinfo, struct bu_vls **sname)
{
    ProMdl   model = cinfo->curr_model;
    ProError err   = PRO_TK_GENERAL_ERROR;
    ProSurfaceTessellationData *tess = NULL;

    wchar_t wname[CREO_NAME_MAX];

    struct bg_vert_tree *vert_tree = NULL;
    struct bg_vert_tree *norm_tree = NULL;
    std::vector<int> faces;
    std::vector<int> face_normals;

    /* Adopt existing tessellation settings */
    double max_angle   = cinfo->max_angle;
    double min_angle   = cinfo->min_angle;
    double max_chord   = cinfo->max_chord;
    double min_chord   = cinfo->min_chord;
    double min_edge    = cinfo->min_edge / cinfo->part_to_mm;
    double min_edge_sq = min_edge * min_edge;
    int    max_facets  = cinfo->max_facets;
    int    max_steps   = cinfo->max_steps;

    /* Tessellation values */
    double angle_incr;
    double chord_incr;
    double curr_chord;
    double curr_angle;

    /* Tessellation results */
    int facet_count = 0;
    int solid_bot   = 0;
    int surf_count  = 0;

    /* tessellation indices */
    size_t n1, n2, n3;
    size_t v1, v2, v3;
    size_t w1, w2, w3;

    /*
     * Note: The below code works, but we can't use model units - Creo
     * "corrects" object sizes with matricies in the parent hierarchies
     * and correcting it here results in problems with those matricies.
     *
     *     ProError ustatus = creo_conv_to_mm(&conv_scale, model);
     */

    if (max_steps < 1) {
        creo_log(cinfo, MSG_TESS, "Invalid settings for \"%s\" with max_steps = %d\n",
                                   bu_vls_addr(cinfo->curr_pname), max_steps);
        return err;
    }

    /* Apply part-specific max/min chord settings */
    max_chord = (cinfo->bbox_diag)*(cinfo->max_chord)/100.0;
    min_chord = (cinfo->bbox_diag)*(cinfo->max_chord)/1000.0;

    /* Specify max_angle */
    max_angle = ((3.0*min_angle/2.0) > 1.0) ? 1.0 : 3.0*min_angle/2.0;

    /* Specify max steps */
    max_steps = 20;

    if (max_steps > 1) {
        angle_incr = (ZERO(max_angle - min_angle)) ? 0.0 : (max_angle - min_angle) / (double)(max_steps - 1);
        chord_incr = (ZERO(max_chord - min_chord)) ? 0.0 : (max_chord - min_chord) / (double)(max_steps - 1);
    } else {
        angle_incr = (ZERO(max_angle - min_angle)) ? 0.0 : (max_angle - min_angle);
        chord_incr = (ZERO(max_chord - min_chord)) ? 0.0 : (max_chord - min_chord);
    }

    if (ZERO(angle_incr) && ZERO(chord_incr))
        max_steps = 0;

    creo_log(cinfo, MSG_TESS, "================================\n");
    creo_log(cinfo, MSG_TESS, "      Tessellation Settings     \n");
    creo_log(cinfo, MSG_TESS, "--------------------------------\n");
    creo_log(cinfo, MSG_TESS, " Max angle cntrl = %g %s\n", max_angle,
                                bu_vls_addr(cinfo->aunits));
    creo_log(cinfo, MSG_TESS, " Min angle cntrl = %g %s\n", min_angle,
                                bu_vls_addr(cinfo->aunits));
    creo_log(cinfo, MSG_TESS, "    Max chord ht = %g %s\n", max_chord,
                                bu_vls_addr(cinfo->lunits));
    creo_log(cinfo, MSG_TESS, "    Min chord ht = %g %s\n", min_chord,
                                bu_vls_addr(cinfo->lunits));
    creo_log(cinfo, MSG_TESS, "        Min edge = %g %s\n", min_edge,
                                bu_vls_addr(cinfo->lunits));
    creo_log(cinfo, MSG_TESS, "      Max facets = %d\n"   , max_facets);
    creo_log(cinfo, MSG_TESS, "       Max steps = %d\n"   , max_steps);
    creo_log(cinfo, MSG_TESS, "--------------------------------\n");

    /* Tessellate part, going from coarse to fine tessellation */
    for (int i = 0; i < max_steps; i++) {
        curr_chord = max_chord - (i * chord_incr);
        curr_angle = min_angle + (i * angle_incr);

        /* Next attempt? */
        if (i > 0) {
            /* Free the tessellation memory */
            ProPartTessellationFree(&tess);
            tess = NULL;

            /* Destroy trees */
            bg_vert_tree_destroy(vert_tree);
            bg_vert_tree_destroy(norm_tree);

            /* Clear faces & normals */
            faces.clear();
            face_normals.clear();
        }

        vert_tree = bg_vert_tree_create();
        norm_tree = bg_vert_tree_create();

        err = ProPartTessellate(ProMdlToPart(model), curr_chord, curr_angle,
                                PRO_B_TRUE, &tess);

        if (err != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_TESS, "Failed for \"%s\"\n",
                                       bu_vls_addr(cinfo->curr_pname));
            creo_log(cinfo, MSG_TESS, "[%g %s, %g %s -> zero facets]\n",
                                       curr_chord,
                                       bu_vls_addr(cinfo->lunits),
                                       curr_angle,
                                       bu_vls_addr(cinfo->aunits));
            continue;
        }

        /* We got through the initial tests - how many surfaces do we have? */
        err = ProArraySizeGet((ProArray)tess, &surf_count);
        if (err != PRO_TK_NO_ERROR)
            goto tess_cleanup;
        else if (surf_count < 4) {
            err = PRO_TK_NOT_EXIST;
            goto tess_cleanup;
        }

        facet_count = 0;
        for (int surf_id = 0; surf_id < surf_count; surf_id++) {
            for (int k = 0; k < tess[surf_id].n_facets; k++) {
                w1 = (size_t)tess[surf_id].facets[k][0];
                w2 = (size_t)tess[surf_id].facets[k][1];
                w3 = (size_t)tess[surf_id].facets[k][2];
                /* Grab vertex indices */
                v1 = bg_vert_tree_add(vert_tree,
                                      tess[surf_id].vertices[w1][0],
                                      tess[surf_id].vertices[w1][1],
                                      tess[surf_id].vertices[w1][2],
                                      min_edge_sq);
                v2 = bg_vert_tree_add(vert_tree,
                                      tess[surf_id].vertices[w2][0],
                                      tess[surf_id].vertices[w2][1],
                                      tess[surf_id].vertices[w2][2],
                                      min_edge_sq);
                v3 = bg_vert_tree_add(vert_tree,
                                      tess[surf_id].vertices[w3][0],
                                      tess[surf_id].vertices[w3][1],
                                      tess[surf_id].vertices[w3][2],
                                      min_edge_sq);
                /* Grab normal indices */
                n1 = bg_vert_tree_add(norm_tree,
                                      tess[surf_id].normals[w1][0],
                                      tess[surf_id].normals[w1][1],
                                      tess[surf_id].normals[w1][2],
                                      min_edge_sq);
                n2 = bg_vert_tree_add(norm_tree,
                                      tess[surf_id].normals[w2][0],
                                      tess[surf_id].normals[w2][1],
                                      tess[surf_id].normals[w2][2],
                                      min_edge_sq);
                n3 = bg_vert_tree_add(norm_tree,
                                      tess[surf_id].normals[w3][0],
                                      tess[surf_id].normals[w3][1],
                                      tess[surf_id].normals[w3][2],
                                      min_edge_sq);

                /* Skip the failed facets */
                if (failed_facet(min_edge, v1, v2, v3, vert_tree)) {
                    creo_log(cinfo, MSG_TESS, "Ignored facet on surface %d, failed edge tolerance %g %s\n",
                                               surf_id, min_edge, bu_vls_addr(cinfo->lunits));
                    continue;
                }

                if (facet_winds_ccw(v1, v3, v2, n1, n3, n2, vert_tree, norm_tree)) {
                    /* Assumes vertices v1 > v3 > v2 for CCW winding */
                    faces.push_back((int)v1);
                    faces.push_back((int)v3);
                    faces.push_back((int)v2);
                    face_normals.push_back((int)n1);
                    face_normals.push_back((int)n3);
                    face_normals.push_back((int)n2);
                } else {
                    /* Reorder vertices v1 > v2 > v3 to enforce CCW winding */
                    creo_log(cinfo, MSG_TESS, "Swapped vertices on facet %d to enforce CCW winding\n", facet_count+1);
                    faces.push_back((int)v1);
                    faces.push_back((int)v2);
                    faces.push_back((int)v3);
                    face_normals.push_back((int)n1);
                    face_normals.push_back((int)n2);
                    face_normals.push_back((int)n3);
                }

            facet_count++;
            }
        }

        /* Check solidity */
        solid_bot = bg_trimesh_solid((int)vert_tree->curr_vert, (size_t)faces.size() / 3,
                                          vert_tree->the_array, &faces[0], NULL);

        /* Check the facet count */
        if (facet_count > max_facets) {
            creo_log(cinfo, MSG_TESS,     "WARNING: Facet count for \"%s\" exceeds %d\n",
                                           bu_vls_addr(cinfo->curr_pname), max_facets);
            if (!solid_bot)
                creo_log(cinfo, MSG_TESS, "Failed solidity for \"%s\"\n",
                                           bu_vls_addr(cinfo->curr_pname));
            break;
        }

        /* Failed solidity, if checking enforced keep trying, otherwise stop */
        if (!solid_bot) {
            if (cinfo->check_solidity) {
                /* Failed solidity with checking enforced, so keep trying */
                creo_log(cinfo, MSG_TESS, "Failed solidity for \"%s\"\n",
                                           bu_vls_addr(cinfo->curr_pname));
                creo_log(cinfo, MSG_TESS, "[%g %s, %g %s -> %d facets]\n",
                                           curr_chord,
                                           bu_vls_addr(cinfo->lunits),
                                           curr_angle,
                                           bu_vls_addr(cinfo->aunits),
                                           facet_count);
                continue;
            } else {
                /* Overide failed solidity, no checking enforced */
                creo_log(cinfo, MSG_TESS, "Failed solidity for \"%s\", "
                                          "but requirement is disabled\n",
                                           bu_vls_addr(cinfo->curr_pname));
                break;
            }
        } else
            break;

    }

    if (facet_count > 3) {
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
    if (cinfo->write_normals && face_normals.size() == 0) {
        err = PRO_TK_NOT_EXIST;
        goto tess_cleanup;
    }

    creo_log(cinfo, MSG_TESS, "Success with \"%s\"\n",
                               bu_vls_addr(cinfo->curr_pname));
    creo_log(cinfo, MSG_TESS, "[%g %s, %g %s -> %d facets]\n",
                               curr_chord,
                               bu_vls_addr(cinfo->lunits),
                               curr_angle,
                               bu_vls_addr(cinfo->aunits),
                               facet_count);

    /* Apply scale factor to BOT vertices */
    for (unsigned int i = 0; i < vert_tree->curr_vert * 3; i++)
        vert_tree->the_array[i] = vert_tree->the_array[i] * cinfo->main_to_mm;

     /* Extract model name */
    (void)ProMdlMdlnameGet(model, wname);

    /* Output the solid - TODO - what is the correct ordering??? does CCW always work? */
    *sname = get_brlcad_name(cinfo, wname, "s", N_SOLID);
    if (cinfo->write_normals)
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

    /* Destroy trees */
    bg_vert_tree_destroy(vert_tree);
    bg_vert_tree_destroy(norm_tree);

    /* Destroy faces & normals */
    faces.clear();
    faces.shrink_to_fit();
    face_normals.clear();
    face_normals.shrink_to_fit();

    return err;
}


/*
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
output_part(struct creo_conv_info *cinfo)
{
    Pro3dPnt        bboxpnts[2];
    ProError        supp_err = PRO_TK_GENERAL_ERROR;
    ProError        tess_err = PRO_TK_GENERAL_ERROR;
    ProError        unit_err = PRO_TK_GENERAL_ERROR;
    ProFileName     msgfil = {'\0'};
    ProMassProperty massprops;
    ProMaterial     material;
    ProMdl          model = cinfo->curr_model;
    ProMdlType      type;
    ProModelitem    mitm;
    ProWVerstamp    cstamp;

    ProSurfaceAppearanceProps aprops;

    char    mname[MAX_MATL_NAME + 1];
    char    pname[CREO_NAME_MAX];
    wchar_t wname[CREO_NAME_MAX];

    char *verstr;

    double prtdens = 1.0;

    /*  bounding box flags */
    int have_bbox = 0;
    int have_rpp  = 0;

    fastf_t rgbflts[3];

    int  surface_side =  0;  /* 0 = surface normal;  1 = other side */
    int  rgb_mode     = -1;

    unsigned char rgb[3], creo_rgb[3];

    struct directory *rdp;
    struct directory *sdp;
    struct bu_color color;
    struct wmember  wcomb;
    struct bu_vls   vstr  = BU_VLS_INIT_ZERO;
    struct bu_vls  *rname = NULL;
    struct bu_vls  *sname = NULL;
    struct bu_vls  *ptc_name;
    struct part_conv_info *pinfo;

    BU_GET(pinfo, struct part_conv_info);
    pinfo->cinfo = cinfo;
    pinfo->csg_holes_supported = 1;
    pinfo->suppressed_features = new std::vector<int>;
    pinfo->subtractions        = new std::vector<struct directory *>;

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    (void)ProMdlTypeGet(model, &type);
    (void)ProMdlMdlnameGet(model, wname);
    ProWstringToString(pname, wname);
    lower_case(pname);

    /* Retain part name & type */ 
    bu_vls_sprintf(cinfo->curr_pname, "%s", pname);
    cinfo->curr_mdl_type = type;

    creo_log(cinfo, MSG_PART, "Processing \"%s\"\n", pname);

    /*
     * TODO - add some up-front logic to detect parts with no
     * associated geometry.
     */

    /* Extract part unit conversion to mm */
    if (creo_conv_to_mm(&(cinfo->part_to_mm), model) != PRO_TK_NO_ERROR) {
        cinfo->part_to_mm = 1.0;
        creo_log(cinfo, MSG_MODEL, "Part model \"%s\" fails unit conversion\n",
                                    bu_vls_addr(cinfo->curr_pname));
    }

    /* Collect info about things that might be eliminated */
    if (cinfo->elim_small) {
        /*
         * Apply user supplied criteria to see if we have anything
         * to suppress
         */

        ProSolidFeatVisit(ProMdlToSolid(model), feature_visit,
                          feature_filter, (ProAppData)pinfo);

        /* If we've got anything to suppress, go ahead and do it. */
        if (!pinfo->suppressed_features->empty()) {
            supp_err = suppress_features(pinfo);
            /* If something went wrong, need to undo just the suppressions we added */
            if (supp_err != PRO_TK_NO_ERROR) {
                creo_log(cinfo, MSG_FEAT, "\"%s\" failed to suppress features\n", pname);
                (void)resume_features(pinfo);
            }
        }
    }

    /* Report model units */
    unit_err = creo_model_units(cinfo);
    if (unit_err == PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_UNITS, "================================\n");
        creo_log(cinfo, MSG_UNITS, "           Model Units          \n");
        creo_log(cinfo, MSG_UNITS, "--------------------------------\n");
        creo_log(cinfo, MSG_UNITS, "       System = %s\n", bu_vls_addr(cinfo->unitsys));
        creo_log(cinfo, MSG_UNITS, "        Angle = %s\n", bu_vls_addr(cinfo->aunits));
        creo_log(cinfo, MSG_UNITS, "        Force = %s\n", bu_vls_addr(cinfo->funits));
        creo_log(cinfo, MSG_UNITS, "         Mass = %s\n", bu_vls_addr(cinfo->munits));
        creo_log(cinfo, MSG_UNITS, "       Length = %s\n", bu_vls_addr(cinfo->lunits));
        creo_log(cinfo, MSG_UNITS, "         Time = %s\n", bu_vls_addr(cinfo->tunits));
        creo_log(cinfo, MSG_UNITS, "--------------------------------\n");
    } else
        creo_log(cinfo, MSG_UNITS, "\"%s\" failed unit inquiry\n", pname);

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
        creo_log(cinfo, MSG_PART, "================================\n");
        creo_log(cinfo, MSG_PART, "         Bounding Box           \n");
        creo_log(cinfo, MSG_PART, "--------------------------------\n");
        creo_log(cinfo, MSG_PART, "     Diagonal = %g %s\n",  
                                   cinfo->bbox_diag,
                                   bu_vls_addr(cinfo->lunits));
        creo_log(cinfo, MSG_PART, " Surface area = %g %s^2\n",
                                   cinfo->bbox_area,
                                   bu_vls_addr(cinfo->lunits));
        creo_log(cinfo, MSG_PART, "       Volume = %g %s^3\n",
                                   cinfo->bbox_vol,
                                   bu_vls_addr(cinfo->lunits));
        creo_log(cinfo, MSG_PART, "--------------------------------\n");
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
    tess_err = tessellate_part(cinfo, &sname);

    /* Deal with the solid conversion results */
    if (tess_err == PRO_TK_NOT_EXIST) {
        /* Failed!!! */
        creo_log(cinfo, MSG_TESS, "Tessellation for \"%s\" failed\n", pname);
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
            creo_log(cinfo, MSG_PART, "\"%s\" replaced with bounding box\n", pname);
            have_rpp = 1;
            goto have_part;
        } else {
            wchar_t *stable = stable_wchar(cinfo, wname);
            if (!stable)
                creo_log(cinfo, MSG_NAME, "Part \"%s\" no stable version of name found\n", pname);
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

        creo_log(cinfo, MSG_COLOR, "Part \"%s\" has rgb = %u/%u/%u\n",
                                    pname, rgb[0], rgb[1], rgb[2]);

        /* Adjust rgb values for minimum luminance thresold */
        if (cinfo->min_luminance > 0) { 
            rgb_mode = rgb4lmin(rgbflts, cinfo->min_luminance);
            if (rgb_mode < 0)
                creo_log(cinfo, MSG_COLOR, "Part \"%s\" has unknown color error\n", pname);
            else if (rgb_mode == 0)
                creo_log(cinfo, MSG_COLOR, "Part \"%s\" luminance exceeds the %d%s minimum\n",
                         pname, cinfo->min_luminance, "%");
            else {
                bu_color_to_rgb_chars(&color, creo_rgb);       /* Preserve existing Creo rgb values */
                bu_color_from_rgb_floats(&color, rgbflts);     /* Adopt the enhanced rgb values */
                creo_log(cinfo, MSG_COLOR, "Part \"%s\" luminance has increased to %d%s\n",
                         pname, cinfo->min_luminance, "%");
                creo_log(cinfo, MSG_COLOR, "Part \"%s\" color changed to rgb = %u/%u/%u\n",
                         pname, rgb[0], rgb[1], rgb[2]);
            }
        }

        /* Make the region comb */
        mk_comb(cinfo->wdbp, bu_vls_addr(rname), &wcomb.l, 1,
                NULL, NULL, (const unsigned char *)rgb,
                cinfo->curr_reg_id, 0, 0, 0, 0, 0, 0);

    } else {
        /* Substitute a default color */
        rgb[0] = 255; rgb[1] = 255;  rgb[2] = 0;
        creo_log(cinfo, MSG_COLOR, "Part \"%s\" has no color assigned, default rgb = %u/%u/%u\n",
                 pname, rgb[0], rgb[1], rgb[2]);
        mk_comb(cinfo->wdbp, bu_vls_addr(rname), &wcomb.l, 1,
                NULL, NULL, (const unsigned char *)rgb,
                cinfo->curr_reg_id, 0, 0, 0, 0, 0, 0);
    }

    /* Set the attributes for the solid/primitive */
    ptc_name = get_brlcad_name(cinfo, wname, NULL, N_CREO); 
    sdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(sname), LOOKUP_QUIET);
    db5_get_attributes(cinfo->wdbp->dbip, &cinfo->avs, sdp);
    bu_avs_add(&cinfo->avs, "importer", "creo-g");
    bu_avs_add(&cinfo->avs, "ptc_name", bu_vls_addr(ptc_name));
    if (have_rpp) {
        bu_vls_sprintf(&vstr, "%g %s^2",
                       cinfo->bbox_area,
                       bu_vls_addr(cinfo->lunits));
        bu_avs_add(&cinfo->avs, "rpp_area", bu_vls_addr(&vstr));

        bu_vls_sprintf(&vstr, "%g %s",
                       cinfo->bbox_diag,
                       bu_vls_addr(cinfo->lunits));
        bu_avs_add(&cinfo->avs, "rpp_diag", bu_vls_addr(&vstr));

        bu_vls_sprintf(&vstr, "%g %s^3",
                       cinfo->bbox_vol,
                       bu_vls_addr(cinfo->lunits));
        bu_avs_add(&cinfo->avs, "rpp_vol",  bu_vls_addr(&vstr));

        bu_avs_add(&cinfo->avs, "tess_fail", bu_vls_addr(ptc_name));
        bu_vls_free(&vstr);
    } else {
        bu_vls_sprintf(&vstr, "%d", cinfo->tess_count);
        bu_avs_add(&cinfo->avs, "tess_count", bu_vls_addr(&vstr));

        if (!NEAR_ZERO(cinfo->bbox_diag, SMALL_FASTF))
            bu_vls_sprintf(&vstr, "%g %s",
                           cinfo->tess_chord/cinfo->bbox_diag*100.0,
                           "%");
        else
            bu_vls_sprintf(&vstr, "%g %s",
                           cinfo->tess_chord,
                           bu_vls_addr(cinfo->lunits));

        bu_avs_add(&cinfo->avs, "tess_chord", bu_vls_addr(&vstr));

        bu_vls_sprintf(&vstr, "%g %s",
                       cinfo->tess_angle,
                       bu_vls_addr(cinfo->aunits));
        bu_avs_add(&cinfo->avs, "tess_angle", bu_vls_addr(&vstr));
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
    creo_log(cinfo, MSG_MATL, "Part \"%s\" has ptc_material_name = \"%s\"\n", 
             pname, cinfo->mtl_key);

    /* Enforce material name attribute */
    bu_avs_add(&cinfo->avs, "ptc_material_name", mname);

    /* If a translation table exists, attempt to find the material */
    if (cinfo->mtl_rec > 0) {
        if (find_matl(cinfo)) {
            int p = cinfo->mtl_ptr;
            creo_log(cinfo, MSG_MATL, "================================\n");
            creo_log(cinfo, MSG_MATL, "      Material Translation      \n");
            creo_log(cinfo, MSG_MATL, "--------------------------------\n");
            creo_log(cinfo, MSG_MATL, "         item = %d\n"  ,
                                                 p + 1);
            creo_log(cinfo, MSG_MATL, "         name = %s\n"  ,
                                                 cinfo->mtl_str[p]);
            creo_log(cinfo, MSG_MATL, "  material_id = %d\n"  ,
                                                 cinfo->mtl_id[p]);
            creo_log(cinfo, MSG_MATL, "          los = %d%s\n",
                                                 cinfo->mtl_los[p], "%");
            creo_log(cinfo, MSG_MATL, "--------------------------------\n");
            /* Add material attributes when material_id is positive */
            if (cinfo->mtl_id[p] >= 0) {
                bu_vls_sprintf(&vstr, "%d", cinfo->mtl_id[p]);
                bu_avs_add(&cinfo->avs, "material_id" , bu_vls_addr(&vstr));
                bu_vls_sprintf(&vstr, "%d", cinfo->mtl_los[p]);
                bu_avs_add(&cinfo->avs, "los"         , bu_vls_addr(&vstr));
                bu_vls_free(&vstr);
            } else {
                creo_log(cinfo, MSG_MATL, "Attributes for \"%s\" are suppressed\n",
                         mname);
            }
        } else {
            creo_log(cinfo, MSG_MATL, "Unable to find \"%s\" in the material table\n",
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
        creo_log(cinfo, MSG_SOLID,     "================================\n");
        creo_log(cinfo, MSG_SOLID,     "         Mass Properties        \n");
        creo_log(cinfo, MSG_SOLID,     "--------------------------------\n");
        creo_log(cinfo, MSG_SOLID,     "         Name = \"%s\" \n", pname  );
        if (massprops.volume > 0.0) {
            creo_log(cinfo, MSG_SOLID, "       Volume = %g %s^3\n", 
                                        massprops.volume,
                                        bu_vls_addr(cinfo->lunits));
        } else {
            creo_log(cinfo, MSG_SOLID, "       Volume = %g %s^3 %s\n", 
                                        massprops.volume,
                                        bu_vls_addr(cinfo->lunits),
                                        "(< 0)");
        }
        if (massprops.surface_area > 0.0) {
            creo_log(cinfo, MSG_SOLID, " Surface area = %g %s^2\n", 
                                        massprops.surface_area,
                                        bu_vls_addr(cinfo->lunits));
        } else {
            creo_log(cinfo, MSG_SOLID, " Surface area = %g %s^2 %s\n", 
                                        massprops.surface_area,
                                        bu_vls_addr(cinfo->lunits),
                                        "(< 0)");
        }
        if (NEAR_EQUAL(massprops.density, 1.0, SMALL_FASTF)) {
            creo_log(cinfo, MSG_SOLID, "      Density = %g %s/%s^3 %s\n",
                                        massprops.density,
                                        bu_vls_addr(cinfo->munits),
                                        bu_vls_addr(cinfo->lunits),
                                        "(= 0)");
                                        prtdens = 1.0;
        } else if (massprops.density < 0.0 ) {
            creo_log(cinfo, MSG_SOLID, "      Density = %g %s/%s^3 %s\n", 
                                        massprops.density,
                                        bu_vls_addr(cinfo->munits),
                                        bu_vls_addr(cinfo->lunits),
                                        "(< 0)");
                                        prtdens = 1.0;
        } else {
            creo_log(cinfo, MSG_SOLID, "      Density = %g %s/%s^3\n", 
                                        massprops.density,
                                        bu_vls_addr(cinfo->munits),
                                        bu_vls_addr(cinfo->lunits));
                                        prtdens = massprops.density;
        }

        if (massprops.mass > 0.0) {
            creo_log(cinfo, MSG_SOLID, "         Mass = %g %s\n", 
                                        massprops.mass,
                                        bu_vls_addr(cinfo->munits));
        } else {
            creo_log(cinfo, MSG_SOLID, "         Mass = %g %s %s\n", 
                                        massprops.mass,
                                        bu_vls_addr(cinfo->munits),
                                        "(< 0)");
        }
        creo_log(cinfo, MSG_SOLID,     "--------------------------------\n");

        /* Create unit system attribute */
        bu_vls_sprintf(&vstr, "%s", bu_vls_addr(cinfo->unitsys));
        bu_avs_add(&cinfo->avs, "ptc_unit_sys", bu_vls_addr(&vstr));

        /* Create volume attribute */
        bu_vls_sprintf(&vstr, "%g %s^3",
                               massprops.volume,
                               bu_vls_addr(cinfo->lunits));
        bu_avs_add(&cinfo->avs, "volume", bu_vls_addr(&vstr));

        /* Create surface area attribute */
        bu_vls_sprintf(&vstr, "%g %s^2",
                               massprops.surface_area,
                               bu_vls_addr(cinfo->lunits));
        bu_avs_add(&cinfo->avs, "surface_area", bu_vls_addr(&vstr));

        /* Create mass attribute */
        bu_vls_sprintf(&vstr, "%g %s",
                               massprops.mass,
                               bu_vls_addr(cinfo->munits));
        bu_avs_add(&cinfo->avs, "mass", bu_vls_addr(&vstr));

        bu_vls_free(&vstr);

    } else if (ProPartDensityGet(ProMdlToPart(model), &prtdens) == PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_PART, "\"%s\" has no mass property values\n", pname);
        /* If the part has a density, extract the value, otherwise use a default  */
        if (NEAR_EQUAL(prtdens, 1.0, SMALL_FASTF)) {
            creo_log(cinfo, MSG_MASS, "Part \"%s\" has no material assigned, density = %g\n", 
                     pname, prtdens);
            prtdens = 1.0;
        } else if (prtdens < 0.0 ) {
            creo_log(cinfo, MSG_MASS, "Part \"%s\" has a negative density, value \"%g\" ignored\n", 
                     pname, prtdens);
            prtdens = 1.0;
        } else
            creo_log(cinfo, MSG_MASS, "Part \"%s\" has density = %g\n", 
                     pname, prtdens);
    } else
        prtdens = 1.0;

    if (NEAR_EQUAL(prtdens, 1.0, SMALL_FASTF))
        creo_log(cinfo, MSG_MASS, "Part \"%s\" has been assigned a default value, density = %g\n", 
                 pname, prtdens);

    /* Create density attribute */
    bu_vls_sprintf(&vstr, "%g %s/%s^3",
                          prtdens,
                          bu_vls_addr(cinfo->munits),
                          bu_vls_addr(cinfo->lunits));
    bu_avs_add(&cinfo->avs, "density",  bu_vls_addr(&vstr));
    bu_vls_free(&vstr);

    /* Export user-supplied list of parameters */
    param_export(cinfo, pname);

    /* Update attributes stored on disk */
    db5_standardize_avs(&cinfo->avs);
    db5_update_attributes(rdp, &cinfo->avs, cinfo->wdbp->dbip);

    /* Increment the region counter - this is a concern if we move to multithreaded generation... */
    cinfo->curr_reg_id++;

cleanup:

    /* Resume anything we suppressed */
    if (cinfo->elim_small && !pinfo->suppressed_features->empty()) {
        creo_log(cinfo, MSG_FEAT, "Resuming %zu features\n", pinfo->suppressed_features->size());
        supp_err = resume_features(pinfo);
        if (supp_err != PRO_TK_NO_ERROR)
            cinfo->warn_feature_resume = 1;
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
