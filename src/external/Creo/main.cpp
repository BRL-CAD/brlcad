/**
 *                    M A I N . C P P
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
/**
 * @file main.cpp
 */

#include "common.h"
#include <algorithm>
#include <sstream>
#include "creo-brl.h"


extern "C" void
creo_conv_info_init(struct creo_conv_info *cinfo)
{

    BU_GET(cinfo->out_fname, struct bu_vls);          /* output file name   */
    bu_vls_init(cinfo->out_fname);

    BU_GET(cinfo->logger_str, struct bu_vls);         /* logger string      */
    bu_vls_init(cinfo->logger_str);

    BU_GET(cinfo->log_fname, struct bu_vls);          /* log file name      */
    bu_vls_init(cinfo->log_fname);

    cinfo->fplog = (FILE *)NULL;                      /* log file settings  */
    cinfo->logger_type = LOGGER_TYPE_ALL;
    cinfo->curr_msg_type = MSG_DEBUG;

    cinfo->fpmtl = (FILE *)NULL;                      /* material file data */
    memset(cinfo->mtl_fname, '\0', sizeof(cinfo->mtl_fname));
    memset(cinfo->mtl_key  , '\0', sizeof(cinfo->mtl_key  ));
    memset(cinfo->mtl_str  , '\0', sizeof(cinfo->mtl_str  ));
    memset(cinfo->mtl_id   , '\0', sizeof(cinfo->mtl_id   ));
    memset(cinfo->mtl_los  , '\0', sizeof(cinfo->mtl_los  ));
    cinfo->mtl_ptr = -1;
    cinfo->mtl_rec = -1;

    cinfo->xform_mode = XFORM_NONE;                   /* xform mode         */
    cinfo->reg_id     = 1000;                         /* region counter     */
    cinfo->lmin       = 30;                           /* minimum luminance  */

    cinfo->wdbp = NULL;

    /* Units - model */
    cinfo->creo_to_brl_conv  = 25.4; /* inches to mm */
    cinfo->local_tol         = 0.0;
    cinfo->local_tol_sq      = 0.0;

    /* Facetization settings */
    cinfo->max_error         = 1.5;
    cinfo->min_error         = 1.5;
    cinfo->tol_dist          = 0.0005;
    cinfo->max_angle_cntrl   = 0.5;
    cinfo->min_angle_cntrl   = 0.5;
    cinfo->max_to_min_steps  = 1;
    cinfo->error_increment   = 0.0;
    cinfo->angle_increment   = 0.0;

    /* CSG settings */
    cinfo->min_hole_diameter = 0.0;
    cinfo->min_chamfer_dim   = 0.0;
    cinfo->min_round_radius  = 0.0;

    /* Tessellation results */
    cinfo->tess_chord        = -1.0;
    cinfo->tess_angle        = -1.0;

    /* Conversion Process results */
    cinfo->asm_count         = 0;
    cinfo->asm_total         = 0;
    cinfo->prt_count         = 0;
    cinfo->prt_total         = 0;

    cinfo->parts  = new std::set<wchar_t *, WStrCmp>;
    cinfo->assems = new std::set<wchar_t *, WStrCmp>;
    cinfo->empty  = new std::set<wchar_t *, WStrCmp>;

    cinfo->region_name_map = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->assem_name_map  = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->solid_name_map  = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->creo_name_map   = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;

    cinfo->brlcad_names = new std::set<struct bu_vls *, StrCmp>;
    cinfo->creo_names   = new std::set<struct bu_vls *, StrCmp>;

    cinfo->model_parameters = new std::vector<char *>;
    cinfo->attrs            = new std::vector<char *>;

    cinfo->warn_feature_unsuppress = 0;

}


extern "C" void
creo_conv_info_free(struct creo_conv_info *cinfo)
{
    bu_vls_free(cinfo->out_fname);
    BU_PUT(cinfo->out_fname, struct bu_vls);

    bu_vls_free(cinfo->log_fname);
    BU_PUT(cinfo->log_fname, struct bu_vls);

    bu_vls_free(cinfo->logger_str);
    BU_PUT(cinfo->logger_str, struct bu_vls);

    memset(cinfo->mtl_fname, '\0', sizeof(cinfo->mtl_fname));
    memset(cinfo->mtl_key  , '\0', sizeof(cinfo->mtl_key ));
    memset(cinfo->mtl_str  , '\0', sizeof(cinfo->mtl_str ));
    memset(cinfo->mtl_id   , '\0', sizeof(cinfo->mtl_id  ));
    memset(cinfo->mtl_los  , '\0', sizeof(cinfo->mtl_los ));

    std::set<wchar_t *, WStrCmp>::iterator d_it;
    for (d_it = cinfo->parts->begin(); d_it != cinfo->parts->end(); d_it++)
        bu_free(*d_it, "free wchar str copy");

    for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++)
        bu_free(*d_it, "free wchar str copy");

    std::set<struct bu_vls *, StrCmp>::iterator s_it;
    for (s_it = cinfo->brlcad_names->begin(); s_it != cinfo->brlcad_names->end(); s_it++) {
        struct bu_vls *v = *s_it;
        bu_vls_free(v);
        BU_PUT(v, struct bu_vls);
    }

    for (s_it = cinfo->creo_names->begin(); s_it != cinfo->creo_names->end(); s_it++) {
        struct bu_vls *v = *s_it;
        bu_vls_free(v);
        BU_PUT(v, struct bu_vls);
    }

    for (unsigned int i = 0; i < cinfo->model_parameters->size(); i++) {
        char *str = cinfo->model_parameters->at(i);
        bu_free(str, "free model param string");
    }

    for (unsigned int i = 0; i < cinfo->attrs->size(); i++) {
        char *str = cinfo->attrs->at(i);
        bu_free(str, "free attrs string");
    }

    delete cinfo->parts;
    delete cinfo->assems;
    delete cinfo->empty;           /* Entries in empty were freed in parts and assems */
    delete cinfo->brlcad_names;
    delete cinfo->region_name_map; /* Entries in name_map were freed in brlcad_names */
    delete cinfo->assem_name_map;  /* Entries in name_map were freed in brlcad_names */
    delete cinfo->solid_name_map;  /* Entries in name_map were freed in brlcad_names */
    delete cinfo->creo_name_map;
    delete cinfo->creo_names;

    if (cinfo->fplog)
        fclose(cinfo->fplog);
    if (cinfo->fpmtl)
        fclose(cinfo->fpmtl);
    if (cinfo->dbip)
        db_close(cinfo->dbip);

    /* Finally, clear the container (TBD)
     *
     * BU_PUT(cinfo, struct creo_conv_info);
     *
     */
}


extern "C" void
output_parts(struct creo_conv_info *cinfo)
{
    std::set<wchar_t *, WStrCmp>::iterator d_it;

    unsigned int prt_count = 1;
    for (d_it = cinfo->parts->begin(); d_it != cinfo->parts->end(); d_it++) {
        wchar_t wname[CREO_NAME_MAX];
        struct bu_vls *rname;
        struct directory *rdp;
        ProMdl model;
        ProWVerstamp cstamp, gstamp;
        if (ProMdlnameInit(*d_it, PRO_MDLFILE_PART, &model) != PRO_TK_NO_ERROR)
            return;
        if (ProMdlMdlnameGet(model, wname) != PRO_TK_NO_ERROR)
            return;

        /*
         * If the part:
         *  a) exists in the .g file already and...
         *  b) has the same Creo version stamp as the part in the current
         *     Creo file
         * then we don't need to re-export it to the .g file
         */

        rname = get_brlcad_name(cinfo, wname, "r", N_REGION);
        rdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(rname), LOOKUP_QUIET);
        if (rdp != RT_DIR_NULL && ProMdlVerstampGet(model, &cstamp) == PRO_TK_NO_ERROR) {
            const char *vs = NULL;
            struct bu_attribute_value_set r_avs;
            db5_get_attributes(cinfo->wdbp->dbip, &r_avs, rdp);
            vs = bu_avs_get(&r_avs, "ptc_version_stamp");
            if (vs && ProStringVerstampGet((char *)vs, &gstamp) == PRO_TK_NO_ERROR
                   && ProVerstampEqual(cstamp, gstamp)          == PRO_B_TRUE) {
                /*
                 * Skip the .g object if it was created from the same
                 * version of the object that exists currently in the
                 * Creo file
                 */
                creo_log(cinfo, MSG_SUCCESS, "Region name \"%s\" exists and is current, skipping...\n", bu_vls_addr(rname));
                continue;
            } else {
                /*
                 * Kill the existing object (region and child solid)
                 * it's out of sync with Creo
                 */
                struct directory **children = NULL;
                struct rt_db_internal in;
                if (rt_db_get_internal(&in, rdp, cinfo->wdbp->dbip, NULL, &rt_uniresource) >= 0) {
                    struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
                    int ccnt = db_comb_children(cinfo->wdbp->dbip, comb, &children, NULL, NULL);
                    if (ccnt > 0) {
                        for (int i = 0; i < ccnt; i++) {
                            db_delete(cinfo->wdbp->dbip, children[i]);
                            db_dirdelete(cinfo->wdbp->dbip, children[i]);
                        }
                    }
                }
                rt_db_free_internal(&in);
                bu_free(children, "free child list");
                db_delete(cinfo->wdbp->dbip, rdp);
                db_dirdelete(cinfo->wdbp->dbip, rdp);
                db_update_nref(cinfo->wdbp->dbip, &rt_uniresource);
            }
        }

        /* All set - process the part */
        if (output_part(cinfo, model) == PRO_TK_NOT_EXIST) {
            creo_log(cinfo, MSG_STATUS, "Part \"%s\" failed to convert", bu_vls_addr(rname));
            cinfo->empty->insert(*d_it);
        } else {
            creo_log(cinfo, MSG_STATUS, "Part %d of %zu converted", prt_count, cinfo->parts->size());
            if (prt_count < cinfo->parts->size())
                prt_count++;
        }

    }

    /* Retain part process results */
    cinfo->prt_count = prt_count;
    cinfo->prt_total = cinfo->parts->size();
}


extern "C" void
output_assems(struct creo_conv_info *cinfo)
{
    std::set<wchar_t *, WStrCmp>::iterator d_it;

    unsigned int asm_count = 1;
    for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
        wchar_t wname[CREO_NAME_MAX];
        struct bu_vls *aname;
        struct directory *adp;
        ProMdl parent;
        ProWVerstamp cstamp, gstamp;

        ProMdlnameInit(*d_it, PRO_MDLFILE_ASSEMBLY, &parent);
        if (ProMdlMdlnameGet(parent, wname) != PRO_TK_NO_ERROR)
            continue;
        /*
         * If the part:
         *  a) exists in the .g file already and...
         *  b) has the same Creo version stamp as the part in the current
         *     Creo file
         * then we don't need to re-export it to the .g file
         */
        aname = get_brlcad_name(cinfo, wname, NULL, N_ASSEM);
        adp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(aname), LOOKUP_QUIET);
        if (adp != RT_DIR_NULL && ProMdlVerstampGet(parent, &cstamp) == PRO_TK_NO_ERROR) {
            const char *vs = NULL;
            struct bu_attribute_value_set a_avs;
            db5_get_attributes(cinfo->wdbp->dbip, &a_avs, adp);
            vs = bu_avs_get(&a_avs, "ptc_version_stamp");
            if (vs && ProStringVerstampGet((char *)vs, &gstamp) == PRO_TK_NO_ERROR
                   && ProVerstampEqual(cstamp, gstamp)          == PRO_B_TRUE) {
                /*
                 * Skip the .g object if it was created from the same
                 * version of the object that exists currently in the
                 * Creo file
                 */
                creo_log(cinfo, MSG_SUCCESS, "Assembly name \"%s\" exists and is current, skipping...\n", bu_vls_addr(aname));
                continue;
            } else {
                /* Kill the existing object - it's out of sync with Creo */
                db_delete(cinfo->wdbp->dbip, adp);
                db_dirdelete(cinfo->wdbp->dbip, adp);
                db_update_nref(cinfo->wdbp->dbip, &rt_uniresource);
            }
        }

        /* Skip if we've determined this one is empty */
        if (cinfo->empty->find(wname) != cinfo->empty->end())
            continue;

        /* All set - process the assembly */
        if (output_assembly(cinfo, parent) != PRO_TK_NO_ERROR)
            creo_log(cinfo, MSG_STATUS, "Assembly \"%s\" failed to convert", bu_vls_addr(aname));
        else {
            creo_log(cinfo, MSG_STATUS, "Assembly %d of %zu succeeded", asm_count, cinfo->assems->size());
            if (asm_count < cinfo->assems->size())
                asm_count++;
        }
    }

    /* Retain assembly process results */
    cinfo->asm_count = asm_count;
    cinfo->asm_total = cinfo->assems->size();

}


/**
 * Build up the sets of assemblies and parts.  Doing a feature
 * visit for all top level objects will result in a recursive walk
 * of the hierarchy that adds all active objects into one of the
 * converter lists.
 *
 * The "app_data" pointer holds the creo_conv_info container.
 *
 */
extern "C" ProError
objects_gather(ProFeature *feat, ProError UNUSED(status), ProAppData app_data)
{
    ProError       err = PRO_TK_GENERAL_ERROR;
    ProMdl         model;
    ProMdlType     mtype;
    ProMdlfileType ftype;
    ProBoolean     is_skel = PRO_B_FALSE;

    char      name[CREO_NAME_MAX];
    wchar_t  wname[CREO_NAME_MAX];
    wchar_t *wname_saved = NULL;

    struct creo_conv_info *cinfo = (struct creo_conv_info *)app_data;

    /* Get assembly component name and filetype */
    err = ProAsmcompMdlMdlnameGet(feat, &ftype, wname);
    ProWstringToString(name, wname);
    lower_case(name);

    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_DEBUG, "Failed to get the file type for \"%s\"\n", name);
        (void)ProWstringFree(wname);
        return PRO_TK_NO_ERROR;
    }

    /* Get assembly component model */
    err = ProAsmcompMdlGet(feat, &model);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_DEBUG, "Failed to get model handle for \"%s\"\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* Get model type (only a part or assembly should make it here) */
    err = ProMdlTypeGet(model, &mtype);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_DEBUG, "Failed to get model type for \"%s\"\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* If this is a skeleton, we're done */
    ProMdlIsSkeleton(model, &is_skel);
    if (is_skel) {
        creo_log(cinfo, MSG_DEBUG, "\"%s\" is a \"skeleton\" model, skipping...\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* Log this member */
    switch (mtype) {
        case PRO_MDL_ASSEMBLY:
            if (cinfo->assems->find(wname) == cinfo->assems->end()) {
                wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "ptc_asm_name");
                wcsncpy(wname_saved, wname, wcslen(wname)+1);
                cinfo->assems->insert(wname_saved);
                creo_log(cinfo, MSG_DEBUG, "Walking into \"%s\"\n", name);
                ProSolidFeatVisit(ProMdlToPart(model), objects_gather, (ProFeatureFilterAction)component_filter, app_data);
            }
            break;
        case PRO_MDL_PART:
            if (cinfo->parts->find(wname) == cinfo->parts->end()) {
                wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "ptc_prt_name");
                wcsncpy(wname_saved, wname, wcslen(wname)+1);
                cinfo->parts->insert(wname_saved);
            }
            break;
        default:
            creo_log(cinfo, MSG_DEBUG, "\"%s\" is not a PART or an ASSEMBLY, skipping...\n", name);
            return PRO_TK_NO_ERROR;
    }

    return PRO_TK_NO_ERROR;
}


/**
 * Routine to output the top level object that is currently displayed in Creo.
 * This is the real beginning of the processing code - doit collects user
 * settings and calls this function.
 */
extern "C" void
output_top_level_object(struct creo_conv_info *cinfo, ProMdl model, ProMdlType mtype)
{

    wchar_t  wname[CREO_NAME_MAX];
    char      name[CREO_NAME_MAX];
    wchar_t *wname_saved;

    struct directory *tdp = RT_DIR_NULL;

    /* Get object name */
    if (ProMdlMdlnameGet(model, wname) != PRO_TK_NO_ERROR)
        return;

    ProWstringToString(name, wname);

    /* Save name */
    wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "ptc_name");
    wcsncpy(wname_saved, wname, wcslen(wname)+1);

    /*
     * There are two possibilities - either we have a hierarchy,
     * in which case we need to walk it and collect the objects
     * to process, or we have a single part which we can process
     * directly.
     */

    switch (mtype) {
        case PRO_MDL_PART:
            /* One part only */
            cinfo->parts->insert(wname_saved);
            output_parts(cinfo);
            break;
        case PRO_MDL_ASSEMBLY:
            /* Walk the hierarchy and process all necessary assemblies and parts */
            cinfo->assems->insert(wname_saved);
            ProSolidFeatVisit(ProMdlToPart(model), objects_gather, (ProFeatureFilterAction)component_filter, (ProAppData)cinfo);
            output_parts(cinfo);
            find_empty_assemblies(cinfo);
            (void)ProWindowRefresh(PRO_VALUE_UNUSED);
            output_assems(cinfo);
            break;
        default:
            creo_log(cinfo, MSG_DEBUG, "Top-level object \"%s\" is not a PART or an ASSEMBLY, skipping...", name);
            return;
    }

    /* Make a final top-level comb based on the file name to hold the orientation matrix */
    struct bu_vls *comb_name;
    struct bu_vls top_name = BU_VLS_INIT_ZERO;
    struct wmember wcomb;
    struct directory *dp;
    BU_LIST_INIT(&wcomb.l);

    mat_t xform;

    if (cinfo->xform_mode == XFORM_X_TO_Z)
        bn_decode_mat(xform, "0 1 0 0 0 0 1 0 1 0 0 0 0 0 0 1");   /* x to z */
    else if (cinfo->xform_mode == XFORM_Y_TO_Z)
        bn_decode_mat(xform, "0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1");   /* y to z */
    else
        bn_decode_mat(xform, "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1");   /*  none  */

    comb_name = get_brlcad_name(cinfo, wname, NULL, N_ASSEM);
    dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(comb_name), LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
        comb_name = get_brlcad_name(cinfo, wname, NULL, N_REGION);

    (void)mk_addmember(bu_vls_addr(comb_name), &(wcomb.l), xform, WMOP_UNION);

    /* Guarantee we have a non-colliding top level name */
    bu_vls_sprintf(&top_name, "all.g");
    tdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(&top_name), LOOKUP_QUIET);
    if (tdp != RT_DIR_NULL) {
        bu_vls_sprintf(&top_name, "all-1.g");
        long count = 0;
        while ((tdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(&top_name), LOOKUP_QUIET)) != RT_DIR_NULL) {
            (void)bu_vls_incr(&top_name, NULL, "0:0:0:0:-", NULL, NULL);
            count++;
            if (count >= MAX_UNIQUE_NAMES) {
                creo_log(cinfo, MSG_DEBUG, "\"%s\" failed top-level name generation\n", bu_vls_cstr(cinfo->out_fname));
                break;
            }
        }
    }

    if (tdp == RT_DIR_NULL)
        mk_lcomb(cinfo->wdbp, bu_vls_addr(&top_name), &wcomb, 0, NULL, NULL, NULL, 0);

    bu_vls_free(&top_name);
}


extern "C" void
doit(char *UNUSED(dialog), char *UNUSED(compnent), ProAppData UNUSED(appdata))
{
    ProError   err = PRO_TK_GENERAL_ERROR;
    ProMdl     model;
    ProMdlType mtype;
    ProLine    tmp_line = {'\0'};

    wchar_t *tmp_str;

    int user_logger_type;

    char linestr[71];
    memset(linestr, '-', 70);
    linestr[70] = '\0';

    char padstr[18];
    memset(padstr, ' ', 17);
    padstr[17] = '\0';

    time_t start;
    time_t finish;
    time_t elapsed;

    /* This replaces the global variables used in the original Creo converter */
    struct creo_conv_info *cinfo = new creo_conv_info;
    creo_conv_info_init(cinfo);

    /* Set-up logger type */
    {
        char logger_str[32];
        int  n_logger_names;
        char **logger_names;

        err = ProUIRadiogroupSelectednamesGet("creo_brl", "log_file_type_rg", &n_logger_names, &logger_names);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to get radio button choice: \"log file type\"  ");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        sprintf(logger_str,"%s", logger_names[0]);
        ProStringarrayFree(logger_names, n_logger_names);

        if (BU_STR_EQUAL("Failure", logger_str))
            cinfo->logger_type = LOGGER_TYPE_FAILURE;
        else if (BU_STR_EQUAL("Success", logger_str))
            cinfo->logger_type = LOGGER_TYPE_SUCCESS;
        else if (BU_STR_EQUAL("Failure/Success", logger_str))
            cinfo->logger_type = LOGGER_TYPE_FAILURE_OR_SUCCESS;
        else if (BU_STR_EQUAL("All/(Debug)", logger_str))
            cinfo->logger_type = LOGGER_TYPE_ALL;
        else
            cinfo->logger_type = LOGGER_TYPE_NONE;

        /* Store the logger string for later use */
        bu_vls_sprintf(cinfo->logger_str, "%s", logger_str);
    }

    /* Save user-specified logger type for later use */
    user_logger_type   = cinfo->logger_type;
    cinfo->logger_type = LOGGER_TYPE_ALL;

    /* Set up log file */
    {
        char log_fname[MAXPATHLEN];

        /* Get the name of the log file */
        err = ProUIInputpanelValueGet("creo_brl", "log_fname", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to get value: \"Process log file name\"  ");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        ProWstringToString(log_fname, tmp_str);
        (void)ProWstringFree(tmp_str);

        /* Open log file, if a name was provided */
        if (strlen(log_fname) > 0) {
            if ((cinfo->fplog=fopen(log_fname, "wb")) == NULL) {
                creo_log(NULL, MSG_STATUS, "FAILURE: Unable to open log file \"%s\"  ", log_fname);
                creo_conv_info_free(cinfo);
                ProUIDialogDestroy("creo_brl");
                delete cinfo;
                return;
            }
        } else
            cinfo->fplog = (FILE *)NULL;

        /* Store the log filename for later use */
        bu_vls_sprintf(cinfo->log_fname, "%s", log_fname);

    }

    /* Start the log file */
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, " Creo to BRL-CAD Converter (7.0.9.0)");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Set up the output file */
    {
        char out_fname[MAXPATHLEN];

        /* Get the name of the output file */
        err = ProUIInputpanelValueGet("creo_brl", "out_fname", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Output file name\"  ");
            creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Output file name\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        ProWstringToString(out_fname, tmp_str);
        (void)ProWstringFree(tmp_str);

        /*
         * If there is a pre-existing file, open it - it may be we
         * only have to update some items in it.
         */
        if (bu_file_exists(out_fname, NULL)) {
            /* Open existing file */
            if ((cinfo->dbip = db_open(out_fname, DB_OPEN_READWRITE)) != DBI_NULL) {
                cinfo->wdbp = wdb_dbopen(cinfo->dbip, RT_WDB_TYPE_DB_DISK);
                creo_log(NULL,  MSG_STATUS, "WARNING: Updating a pre-existing output file \"%s\"  ", out_fname);
                creo_log(cinfo, MSG_PLAIN,  "WARNING: Updating a pre-existing output file \"%s\"\n", out_fname);
            } else {
                creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to open output file \"%s\"  ", out_fname);
                creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to open output file \"%s\"\n", out_fname);
                creo_conv_info_free(cinfo);
                ProUIDialogDestroy("creo_brl");
                delete cinfo;
                return;
            }
        } else {
            /* Create new file */
            if ((cinfo->dbip = db_create(out_fname, 5)) != DBI_NULL)
                cinfo->wdbp = wdb_dbopen(cinfo->dbip, RT_WDB_TYPE_DB_DISK);
            else {
                creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to open output file \"%s\"  ", out_fname);
                creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to open output file \"%s\"\n", out_fname);
                creo_conv_info_free(cinfo);
                ProUIDialogDestroy("creo_brl");
                delete cinfo;
                return;
            }
        }

        /* Store the output filename for later use */
        bu_vls_sprintf(cinfo->out_fname, "%s", out_fname);
    }

    /* Set up the material file */
    {
        char mtl_fname[MAXPATHLEN];

        /* Get the name of the material file */
        err = ProUIInputpanelValueGet("creo_brl", "mtl_fname", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Material file name\"  ");
            creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Material file name\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        ProWstringToString(mtl_fname, tmp_str);
        (void)ProWstringFree(tmp_str);

        /* Open material file when name is provided */
	if (strlen(mtl_fname) > 0) {
	    if (bu_file_exists(mtl_fname, NULL)) {
		if ((cinfo->fpmtl=fopen(mtl_fname, "r")) == NULL) {
		    creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to open material file \"%s\"  ", mtl_fname);
		    creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to open material file \"%s\"\n", mtl_fname);
		    creo_conv_info_free(cinfo);
		    ProUIDialogDestroy("creo_brl");
		    delete cinfo;
		    return;
		} else 
		    cinfo->mtl_rec = get_mtl_input(cinfo->fpmtl, &(cinfo->mtl_str[0][0]) , &(cinfo->mtl_id[0]), &(cinfo->mtl_los[0]));
	    } else {
		cinfo->fpmtl = (FILE *)NULL;
	    }
	}

        /* Store the material filename for later use */
        sprintf(cinfo->mtl_fname, "%s", mtl_fname);
    }

    creo_log(cinfo, MSG_PLAIN, "#                  Output file name: \"%s\"\n", bu_vls_cstr(cinfo->out_fname));
    creo_log(cinfo, MSG_PLAIN, "#              Process log criteria: \"%s\"\n", bu_vls_cstr(cinfo->logger_str));
    creo_log(cinfo, MSG_PLAIN, "#             Process log file name: \"%s\"\n", bu_vls_cstr(cinfo->log_fname));
    creo_log(cinfo, MSG_PLAIN, "#                Material file name: \"%s\"\n", cinfo->mtl_fname);

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "           Creo Parameters          ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Read user-supplied list of model parameters for object names */
    {
        /* Get string from dialog */
        char attr_rename[MAXPATHLEN];
        wchar_t *w_attr_rename;
        err = ProUIInputpanelValueGet("creo_brl", "attr_rename", &w_attr_rename);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value(s): \"Create object names\"  ");
            creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value(s): \"Create object names\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        ProWstringToString(attr_rename, w_attr_rename);
        (void)ProWstringFree(w_attr_rename);

        if (strlen(attr_rename) > 0) {
            std::string pfilestr(attr_rename);
            std::istringstream ss(pfilestr);
            std::string line;
            while (std::getline(ss, line)) {
                std::string pkey;
                std::istringstream ls(line);
                while (std::getline(ls, pkey, ',')) {
                    /* Scrub leading and trailing whitespace */
                    size_t startpos = pkey.find_first_not_of(" \t\n\v\f\r");
                    if (std::string::npos != startpos)
                        pkey = pkey.substr(startpos);
                    size_t endpos = pkey.find_last_not_of(" \t\n\v\f\r");
                    if (std::string::npos != endpos)
                        pkey = pkey.substr(0 ,endpos+1);
                    if (pkey.length() > 0) {
                        creo_log(cinfo, MSG_PLAIN, "#              Creates object names: \"%s\"\n", pkey.c_str());
                        cinfo->model_parameters->push_back(bu_strdup(pkey.c_str()));
                    }
                }
            }
            creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
        } else
            creo_log(cinfo, MSG_PLAIN, "#               Create object names: \"%s\"\n", attr_rename);
    }

    /* Read user-supplied list of model attributes for conversion */
    {
        /* Get string from dialog */
        char attr_save[MAXPATHLEN];
        wchar_t *w_attr_save;
        err = ProUIInputpanelValueGet("creo_brl", "attr_save", &w_attr_save);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value(s): \"Preserve as attributes\"  ");
            creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value(s): \"Preserve as attributes\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        ProWstringToString(attr_save, w_attr_save);
        (void)ProWstringFree(w_attr_save);

        if (strlen(attr_save) > 0) {
            /* Parse attribute list into separate parameter keys */
            std::string afilestr(attr_save);
            std::istringstream ss(afilestr);
            std::string line;
            while (std::getline(ss, line)) {
                std::string pkey;
                std::istringstream ls(line);
                while (std::getline(ls, pkey, ',')) {
                    /* Scrub leading and trailing whitespace */
                    size_t startpos = pkey.find_first_not_of(" \t\n\v\f\r");
                    if (std::string::npos != startpos)
                        pkey = pkey.substr(startpos);
                    size_t endpos = pkey.find_last_not_of(" \t\n\v\f\r");
                    if (std::string::npos != endpos)
                        pkey = pkey.substr(0 ,endpos+1);
                    if (pkey.length() > 0) {
                        creo_log(cinfo, MSG_PLAIN, "#            Preserved as attribute: \"%s\"\n", pkey.c_str());
                        cinfo->attrs->push_back(bu_strdup(pkey.c_str()));
                    }
                }
            }
        } else
              creo_log(cinfo, MSG_PLAIN, "#            Preserve as attributes: \"%s\"\n", attr_save);
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "      Coordinate Transformation     ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Set-up coordinate transformation */
    {
        char xform_str[32];
        int  n_xform_names;
        char **xform_names;

        err = ProUIRadiogroupSelectednamesGet("creo_brl", "transform_rg", &n_xform_names, &xform_names);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get radio button choice: \"coordinate transformation\"  ");
            creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get radio button choice: \"coordinate transformation\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        sprintf(xform_str,"%s", xform_names[0]);
        ProStringarrayFree(xform_names, n_xform_names);

        if (BU_STR_EQUAL("x_to_z", xform_str))
            cinfo->xform_mode = XFORM_X_TO_Z;
        else if (BU_STR_EQUAL("y_to_z", xform_str))
            cinfo->xform_mode = XFORM_Y_TO_Z;
        else
            cinfo->xform_mode = XFORM_NONE;

        creo_log(cinfo, MSG_PLAIN, "#         Coordinate transformation: \"%s\"\n", xform_str);
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "          Process Controls          ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Get starting identifier */
    err = ProUIInputpanelValueGet("creo_brl", "region_counter", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Initial region counter\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Initial region counter\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->reg_id = wstr_to_long(cinfo, tmp_str);
        creo_log(cinfo, MSG_PLAIN, "#            Initial region counter: %d\n", cinfo->reg_id);
        V_MAX(cinfo->reg_id, 1);
        (void)ProWstringFree(tmp_str);
    }

    /* Get minimum luminance threshold */
    err = ProUIInputpanelValueGet("creo_brl", "min_luminance", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Min luminance threshold\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Min luminance threshold\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->lmin = wstr_to_long(cinfo, tmp_str);
        creo_log(cinfo, MSG_PLAIN, "#        Min luminance threshold, %s: %d\n", "%", cinfo->lmin);
        cinfo->lmin = std::min(std::max(cinfo->lmin,0),100);
        (void)ProWstringFree(tmp_str);
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "        Tessellation Controls       ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Get max tessellation error */
    err = ProUIInputpanelValueGet("creo_brl", "max_error", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Max chord error\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Max chord error\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->max_error = wstr_to_double(cinfo, tmp_str);
        creo_log(cinfo, MSG_PLAIN, "#               Max chord error, mm: %8.6f\n", cinfo->max_error);
        (void)ProWstringFree(tmp_str);
    }

    /* Get min tessellation error */
    err = ProUIInputpanelValueGet("creo_brl", "min_error", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Min chord error\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Min chord error\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->min_error = wstr_to_double(cinfo, tmp_str);
        creo_log(cinfo, MSG_PLAIN, "#               Min chord error, mm: %8.6f\n", cinfo->min_error);
        V_MAX(cinfo->max_error, cinfo->min_error);
        (void)ProWstringFree(tmp_str);
    }

    /* Get the max angle control */
    err = ProUIInputpanelValueGet("creo_brl", "max_angle_ctrl", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Max angle control\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Max angle control\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->max_angle_cntrl = wstr_to_double(cinfo, tmp_str);
        creo_log(cinfo, MSG_PLAIN, "#            Max angle control, deg: %8.6f\n", cinfo->max_angle_cntrl);
        (void)ProWstringFree(tmp_str);
    }

    /* Get the min angle control */
    err = ProUIInputpanelValueGet("creo_brl", "min_angle_ctrl", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Min angle control\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Min angle control\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->min_angle_cntrl = wstr_to_double(cinfo, tmp_str);
        creo_log(cinfo, MSG_PLAIN, "#            Min angle control, deg: %8.6f\n", cinfo->min_angle_cntrl);
        V_MAX(cinfo->max_angle_cntrl, cinfo->min_angle_cntrl);
        (void)ProWstringFree(tmp_str);
    }

    /* Get the max to min steps */
    err = ProUIInputpanelValueGet("creo_brl", "isteps", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Max to Min control steps\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Max to Min control steps\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->max_to_min_steps = wstr_to_long(cinfo, tmp_str);
        creo_log(cinfo, MSG_PLAIN, "#          Max to Min control steps: %d\n", cinfo->max_to_min_steps);
        (void)ProWstringFree(tmp_str);
        if (cinfo->max_to_min_steps <= 0)
            cinfo->max_to_min_steps = 0;
        else {
            cinfo->error_increment = (ZERO((cinfo->max_error - cinfo->min_error))) ? 0 : (cinfo->max_error - cinfo->min_error) / (double)cinfo->max_to_min_steps;
            cinfo->angle_increment = (ZERO((cinfo->max_angle_cntrl - cinfo->min_angle_cntrl))) ? 0 : (cinfo->max_angle_cntrl - cinfo->min_angle_cntrl) / (double)cinfo->max_to_min_steps;
            if (ZERO(cinfo->error_increment) && ZERO(cinfo->angle_increment))
                cinfo->max_to_min_steps = 0;
        }
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "       Small Feature Controls       ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Check if user wants to eliminate small features */
    err = ProUICheckbuttonGetState("creo_brl", "elim_small", &cinfo->do_elims);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"ignore minimum sizes\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get check button state: \"ignore minimum sizes\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    } else
        creo_log(cinfo, MSG_PLAIN, "#              Ignore minimum sizes: %s\n", cinfo->do_elims ? "ON" : "OFF");

    if (cinfo->do_elims) {
        /* Get the minimum hole diameter */
        err = ProUIInputpanelValueGet("creo_brl", "min_hole", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Hole diameter\"  ");
            creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Hole diameter\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        } else {
            cinfo->min_hole_diameter = wstr_to_double(cinfo, tmp_str);
            creo_log(cinfo, MSG_PLAIN, "#                 Hole diameter, mm: %8.6f\n", cinfo->min_hole_diameter);
            V_MAX(cinfo->min_hole_diameter, 0.0);
            (void)ProWstringFree(tmp_str);
        }

        /* Get the minimum chamfer dimension */
        err = ProUIInputpanelValueGet("creo_brl", "min_chamfer", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Chamfer dimension\"  ");
            creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Chamfer dimension\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        } else {
            cinfo->min_chamfer_dim = wstr_to_double(cinfo, tmp_str);
            creo_log(cinfo, MSG_PLAIN, "#             Chamfer dimension, mm: %8.6f\n", cinfo->min_chamfer_dim);
            V_MAX(cinfo->min_chamfer_dim, 0.0);
            (void)ProWstringFree(tmp_str);
        }

        /* Get the minimum blend/round radius */
        err = ProUIInputpanelValueGet("creo_brl", "min_round", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Blend radius\"  ");
            creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Blend radius\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        } else {
            cinfo->min_round_radius = wstr_to_double(cinfo, tmp_str);
            creo_log(cinfo, MSG_PLAIN, "#                  Blend radius, mm: %8.6f\n", cinfo->min_round_radius);
            V_MAX(cinfo->min_round_radius, 0.0);
            (void)ProWstringFree(tmp_str);
        }

    } else {
        cinfo->min_hole_diameter = 0.0;
        cinfo->min_round_radius  = 0.0;
        cinfo->min_chamfer_dim   = 0.0;
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "          Surface Controls          ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Check if user wants to do any CSG */
    err = ProUICheckbuttonGetState("creo_brl", "facets_only", &cinfo->do_facets_only);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"facets only\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get check button state: \"facets only\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    } else 
        creo_log(cinfo, MSG_PLAIN, "#     Facetize everything, (no CSG): %s\n", cinfo->do_facets_only ? "ON" : "OFF");

    /* Check if user wants surface normals in the BOT's */
    err = ProUICheckbuttonGetState("creo_brl", "get_normals", &cinfo->get_normals);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"write surface normals\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get check button state: \"write surface normals\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    } else
        creo_log(cinfo, MSG_PLAIN, "#             Write surface normals: %s\n", cinfo->get_normals ? "ON" : "OFF");

    /* Check if user wants to test solidity */
    err = ProUICheckbuttonGetState("creo_brl", "check_solidity", &cinfo->check_solidity);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"reject failed BoTs\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get check button state: \"reject failed BoTs\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    } else
        creo_log(cinfo, MSG_PLAIN, "#    Reject BoTs that fail solidity: %s\n", cinfo->check_solidity ? "ON" : "OFF");

    /* Check if user wants to use bounding boxes */
    err = ProUICheckbuttonGetState("creo_brl", "debug_bboxes", &cinfo->debug_bboxes);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"bounding box for failed part\"  ");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get check button state: \"bounding box for failed part\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    } else
        creo_log(cinfo, MSG_PLAIN, "# Bounding box replaces failed part: %s\n", cinfo->debug_bboxes ? "ON" : "OFF");

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Input summary now complete, restore user-specified logger type */
    cinfo->logger_type = user_logger_type;

    /* Report status of material file */
    if (int(strlen(cinfo->mtl_fname)) > 0) {
        if (cinfo->fpmtl == NULL)
            creo_log(cinfo, MSG_DEBUG, "Unknown material translation file \"%s\"\n",
                     cinfo->mtl_fname);
        else if (cinfo->mtl_rec > 0) {
            creo_log(cinfo, MSG_DEBUG, "Found material translation file \"%s\"\n",
                     cinfo->mtl_fname);
            creo_log(cinfo, MSG_DEBUG, "Found %d valid material translation entries\n",
                     cinfo->mtl_rec);
        } else
            creo_log(cinfo, MSG_FAIL, "File \"%s\" has no valid material translation entries\n",
                     cinfo->mtl_fname);
    } else {
        creo_log(cinfo, MSG_DEBUG, "No material translation file was specified\n");
        cinfo->mtl_rec = -1;
    }

    /* Get currently displayed model in Creo */
    err = ProMdlCurrentGet(&model);
    if (err == PRO_TK_BAD_CONTEXT) {
        creo_log(cinfo, MSG_STATUS, "Unable to get currently displayed Creo model\n");
        creo_log(cinfo, MSG_FAIL,   "Unable to get currently displayed Creo model\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
        }

    /* Get model type */
    err = ProMdlTypeGet(model, &mtype);
    if (err == PRO_TK_BAD_INPUTS) {
        creo_log(NULL,  MSG_STATUS, "Unable to get \"type\" for currently displayed Creo model  ");
        creo_log(cinfo, MSG_FAIL,   "Unable to get \"type\" for currently displayed Creo model\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    }

    /* Limit scope to parts and assemblies, no drawings, etc. */
    if (mtype != PRO_MDL_ASSEMBLY && mtype != PRO_MDL_PART) {
        creo_log(NULL,  MSG_STATUS, "Current model is not a PART or an ASSEMBLY  ");
        creo_log(cinfo, MSG_FAIL,   "Current model is not a PART or an ASSEMBLY\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    }

    /* TODO - verify this is working correctly */
    creo_model_units(&(cinfo->creo_to_brl_conv), model);

    /* Adjust tolerance for Creo units */
    cinfo->local_tol    = cinfo->tol_dist / cinfo->creo_to_brl_conv;
    cinfo->local_tol_sq = cinfo->local_tol * cinfo->local_tol;

    (void)time(&start);

    /*
     * Output the top-level object
     * this will recurse through the entire model
     */
    output_top_level_object(cinfo, model, mtype);

    /* Let user know we are done... */
    ProStringToWstring(tmp_line,  "Status: Conversion complete, (Convert/Quit)?");
    (void)ProUILabelTextSet("creo_brl", "conv_status", tmp_line);
    ProMessageClear();

    /* Enforce full logging */
    cinfo->logger_type = LOGGER_TYPE_ALL;

    if (cinfo->warn_feature_unsuppress) {
        struct bu_vls errmsg = BU_VLS_INIT_ZERO;
        bu_vls_sprintf(&errmsg, "#  During the conversion, one or more parts contained features that  \n"
                                "#  were suppressed.  After the conversion was complete, attempts were\n"
                                "#  made to resume these same features.  One or more of these attempts\n"
                                "#  has failed to resume them, so some features remain suppressed.    \n"
                                "#\n"
                                "#  To avoid permanent changes to the current Creo model, please exit \n"
                                "#  without saving any changes.                                       \n");
        creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
        creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
        creo_log(cinfo, MSG_PLAIN, "    WARNING:  Restoration Failure   ");
        creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
        creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
        creo_log(cinfo, MSG_PLAIN,    "%s", bu_vls_addr(&errmsg));
        PopupMsg("WARNING:  Restoration Failure", bu_vls_addr(&errmsg));
        bu_vls_free(&errmsg);
    }

    /* Finish the log file */

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "         Conversion Summary         ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    creo_log(cinfo, MSG_PLAIN,     "#                       Parts found: %d\n",
                                    cinfo->prt_total);
    creo_log(cinfo, MSG_PLAIN,     "#                   Parts converted: %d\n",
                                    cinfo->prt_count);

    if (cinfo->prt_total > cinfo->prt_count) {
        creo_log(cinfo, MSG_PLAIN, "#                     Part failures: %d\n",
                                    cinfo->prt_total - cinfo->prt_count);
        creo_log(cinfo, MSG_PLAIN, "#                  Conversion ratio: %.1f%s\n",
                                    double(cinfo->prt_count)/double(cinfo->prt_total)*100.0,
                                    "%");
    } else
        creo_log(cinfo, MSG_PLAIN, "#                  Conversion ratio: 100%s\n", "%");

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,     "#                  Assemblies found: %d\n",
                                    cinfo->asm_total);
    creo_log(cinfo, MSG_PLAIN,     "#              Assemblies converted: %d\n",
                                    cinfo->asm_count);

    if (cinfo->asm_total > cinfo->asm_count) {
        creo_log(cinfo, MSG_PLAIN, "#                 Assembly failures: %d\n",
                                    cinfo->asm_total - cinfo->asm_count);
        creo_log(cinfo, MSG_PLAIN, "#                  Conversion ratio: %.1f%s\n",
                                    double(cinfo->asm_count)/double(cinfo->asm_total)*100.0,
                                    "%");
    } else
        creo_log(cinfo, MSG_PLAIN, "#                  Conversion ratio: 100%s\n", "%");

    (void)time(&finish);
    elapsed = finish - start;

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,     "#                      Elapsed time: %02d:%02d:%02d\n",
                                    (int)elapsed/3600,
                                    (int)elapsed%3600/60,
                                    (int)elapsed%60);

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "      End of BRL-CAD Conversion     ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Finish the console log*/
    creo_log(cinfo, MSG_STATUS, "------- Conversion Summary ------");
    if (cinfo->prt_total > cinfo->prt_count)
        creo_log(cinfo, MSG_STATUS, "Part conversion failures = %d",
                                     cinfo->prt_total - cinfo->prt_count);
    else
        creo_log(cinfo, MSG_STATUS, "No Part failures found");
    if (cinfo->asm_total > cinfo->asm_count)
        creo_log(cinfo, MSG_STATUS, "Assembly conversion failures = %d",
                                     cinfo->asm_total - cinfo->asm_count);
    else
        creo_log(cinfo, MSG_STATUS, "No Assembly failures found");
    creo_log(cinfo, MSG_STATUS, "---------------------------------");

    creo_conv_info_free(cinfo);
    delete cinfo;
    return;
}


extern "C" void
elim_small_activate(char *dialog_name, char *button_name, ProAppData UNUSED(data))
{
    ProBoolean state;

    if (ProUICheckbuttonGetState(dialog_name, button_name, &state) != PRO_TK_NO_ERROR) {
        creo_log(NULL, MSG_STATUS, "FAILURE: Unable to activate check buttons: \"Eliminate Small Features\"  ");
        return;
    }

    if (state) {
        if (ProUIInputpanelEditable(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to activate: \"Hole diameter\"  ");
            return;
        }
        if (ProUIInputpanelEditable(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to activate: \"Chamfer dimension\"  ");
            return;
        }
        if (ProUIInputpanelEditable(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to activate: \"Blend radius\"  ");
            return;
        }
    } else {
        if (ProUIInputpanelReadOnly(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to de-activate: \"Hole diameter\"vv");
            return;
        }
        if (ProUIInputpanelReadOnly(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to de-activate: \"Chamfer dimension\"  ");
            return;
        }
        if (ProUIInputpanelReadOnly(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to de-activate: \"Blend radius\"  ");
            return;
        }
    }
}


extern "C" void
do_quit(char *UNUSED(dialog), char *UNUSED(compnent), ProAppData UNUSED(appdata))
{
    ProUIDialogDestroy("creo_brl");
}


/** Driver routine for converting Creo to BRL-CAD */
extern "C" int
creo_brl(uiCmdCmdId UNUSED(command), uiCmdValue *UNUSED(p_value), void *UNUSED(p_push_cmd_data))
{
    int destroy_dialog = 0;
    int err;

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    creo_log(NULL, MSG_STATUS, "Launching Creo to BRL-CAD converter...");

    /* Use UI dialog */
    if (ProUIDialogCreate("creo_brl", "creo_brl") != PRO_TK_NO_ERROR) {
        bu_vls_printf(&vls, "FAILURE: Unable to create dialog box for creo-brl\n");
        goto print_msg;
    }

    if (ProUICheckbuttonActivateActionSet("creo_brl", "elim_small", elim_small_activate, NULL) != PRO_TK_NO_ERROR) {
        bu_vls_printf(&vls, "FAILURE: Unable to set action for \"Eliminate small features\" checkbutton\n");
        goto print_msg;
    }

    if (ProUIPushbuttonActivateActionSet("creo_brl", "doit", doit, NULL) != PRO_TK_NO_ERROR) {
        bu_vls_printf(&vls, "FAILURE: Unable to set action for \"Convert\" button\n");
        destroy_dialog = 1;
        goto print_msg;
    }

    if (ProUIPushbuttonActivateActionSet("creo_brl", "quit", do_quit, NULL) != PRO_TK_NO_ERROR) {
        bu_vls_printf(&vls, "FAILURE: Unable to set action for \"Quit\" button\n");
        destroy_dialog = 1;
        goto print_msg;
    }

    /* File names may not get longer than the default... */
    ProUIInputpanelMaxlenSet("creo_brl", "out_fname", MAXPATHLEN - 1);
    ProUIInputpanelMaxlenSet("creo_brl", "log_fname", MAXPATHLEN - 1);
    ProUIInputpanelMaxlenSet("creo_brl", "mtl_fname", MAXPATHLEN - 1);

    ProMdl model;
    if (ProMdlCurrentGet(&model) != PRO_TK_BAD_CONTEXT) {
        wchar_t wname[CREO_NAME_MAX];
        if (ProMdlMdlnameGet(model, wname) == PRO_TK_NO_ERROR) {
            struct bu_vls groot = BU_VLS_INIT_ZERO;
            struct bu_vls lroot = BU_VLS_INIT_ZERO;
            char name[CREO_NAME_MAX];
            ProWstringToString(name, wname);
            lower_case(name);
            /* Supply a sensible (lowercase) default for the .g file... */
            if (bu_path_component(&groot, name, BU_PATH_BASENAME_EXTLESS)) {
                wchar_t wgout[CREO_NAME_MAX];
                bu_vls_printf(&groot, ".g");
                ProStringToWstring(wgout, bu_vls_addr(&groot));
                ProUIInputpanelValueSet("creo_brl", "out_fname", wgout);
                bu_vls_free(&groot);
           }
           /* Suggest a default log file... */
           if (bu_path_component(&lroot, name, BU_PATH_BASENAME_EXTLESS)) {
               /* Supply a sensible default for the .g file... */
               wchar_t wgout[CREO_NAME_MAX];
               bu_vls_printf(&lroot, "_log.txt");
               ProStringToWstring(wgout, bu_vls_addr(&lroot));
               ProUIInputpanelValueSet("creo_brl", "log_fname", wgout);
               bu_vls_free(&lroot);
           }
       }
    }

    /*
     * Rather than files, (or in addition to?) should probably allow 
     * users to input lists directly... to do so, need to increase char
     * limit
     */
    ProUIInputpanelMaxlenSet("creo_brl", "attr_rename", MAXPATHLEN - 1);
    ProUIInputpanelMaxlenSet("creo_brl", "attr_save",   MAXPATHLEN - 1);

    if (ProUIDialogActivate("creo_brl", &err) != PRO_TK_NO_ERROR)
        bu_vls_printf(&vls, "FAILURE: Unexpected error in creo-brl dialog returned %d\n", err);

    print_msg:
    if (bu_vls_strlen(&vls) > 0) {
        creo_log(NULL, MSG_DEBUG, bu_vls_addr(&vls));
        bu_vls_free(&vls);
    }
    if (destroy_dialog)
        ProUIDialogDestroy("creo_brl");
    return 0;
}


/**
 * This routine determines whether the "creo-brl" menu item in Creo
 * should be displayed as available or greyed out
 */
extern "C" uiCmdAccessState
creo_brl_access(uiCmdAccessMode UNUSED(access_mode))
{
    /* Doing the correct checks appears to be unreliable */
    return ACCESS_AVAILABLE;
}


/*
 * IMPORTANT - the names of the next two functions - user_initialize
 * and user_terminate - are dictated by the Creo API.  These are the
 * hooks that tie the rest of the code into the Creo system.  Both
 * are *required* to be present, even if the user_terminate function
 * doesn't do any actual work.
 */

extern "C" int user_initialize()
{
    ProError err = PRO_TK_GENERAL_ERROR;

    int i;
    uiCmdCmdId cmd_id;

    /* Creo says always check the size of w_char */
    err = ProWcharSizeVerify (sizeof (wchar_t), &i);
    if (err != PRO_TK_NO_ERROR || (i != sizeof (wchar_t))) {
        creo_log(NULL, MSG_STATUS, "\"wchar_t\" is the incorrect size (%d), size should be %d", (int)sizeof(wchar_t), i);
        return -1;
    }

    /* Add a command that calls our creo-brl routine */
    err = ProCmdActionAdd("CREO-BRL", (uiCmdCmdActFn)creo_brl, uiProe2ndImmediate, creo_brl_access, PRO_B_FALSE, PRO_B_FALSE, &cmd_id);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL, MSG_STATUS, "Failed to add creo-brl action");
        return -1;
    }

    /* Add a menu item that runs the new command */
    ProFileName msgfil = {'\0'};
    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);
    err = ProMenubarmenuPushbuttonAdd("Tools", "CREO-BRL", "CREO-BRL", "CREO-BRL-HELP", NULL, PRO_B_TRUE, cmd_id, msgfil);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL, MSG_STATUS, "Failed to add creo-brl menu bar push button");
        return -1;
    }

    /*
     * Let user know we are here?
     *
     *  PopupMsg("Plugin Successfully Loaded", "The Creo to BRL-CAD converter plugin Version 0.2 was successfully loaded.");
     *
     */

    return 0;
}


extern "C" void user_terminate()
{
    ProMessageClear();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
