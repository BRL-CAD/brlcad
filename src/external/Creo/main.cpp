/**
 *                    M A I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017-2021 United States Government as represented by
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
#include <sstream>
#include "creo-brl.h"


extern "C" void
creo_conv_info_init(struct creo_conv_info *cinfo)
{
    /* Output file */
    BU_GET(cinfo->output_file, struct bu_vls);
    bu_vls_init(cinfo->output_file);

    /* Region ID */
    cinfo->reg_id = 1000;

    /* File settings */
    cinfo->logger = (FILE *)NULL;
    cinfo->logger_type=LOGGER_TYPE_NONE;
    cinfo->curr_msg_type = MSG_DEBUG;

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

    cinfo->parts = new std::set<wchar_t *, WStrCmp>;
    cinfo->assems = new std::set<wchar_t *, WStrCmp>;
    cinfo->empty = new std::set<wchar_t *, WStrCmp>;
    cinfo->region_name_map = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->assem_name_map = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->solid_name_map = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->creo_name_map = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->brlcad_names = new std::set<struct bu_vls *, StrCmp>;
    cinfo->creo_names = new std::set<struct bu_vls *, StrCmp>;
    cinfo->model_parameters = new std::vector<char *>;
    cinfo->attrs = new std::vector<char *>;
    cinfo->warn_feature_unsuppress = 0;

}


extern "C" void
creo_conv_info_free(struct creo_conv_info *cinfo)
{

    bu_vls_free(cinfo->output_file);
    BU_PUT(cinfo->output_file, struct bu_vls);

    std::set<wchar_t *, WStrCmp>::iterator d_it;
    for (d_it = cinfo->parts->begin(); d_it != cinfo->parts->end(); d_it++) {
        bu_free(*d_it, "free wchar str copy");
    }
    for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
        bu_free(*d_it, "free wchar str copy");
    }

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
    delete cinfo->empty; /* Entries in empty were freed in parts and assems */
    delete cinfo->brlcad_names;
    delete cinfo->region_name_map; /* Entries in name_map were freed in brlcad_names */
    delete cinfo->assem_name_map; /* Entries in name_map were freed in brlcad_names */
    delete cinfo->solid_name_map; /* Entries in name_map were freed in brlcad_names */
    delete cinfo->creo_name_map;
    delete cinfo->creo_names;

    if (cinfo->logger) fclose(cinfo->logger);
    wdb_close(cinfo->wdbp);

    /* Finally, clear the container */
    //BU_PUT(cinfo, struct creo_conv_info);
}


extern "C" void
output_parts(struct creo_conv_info *cinfo)
{
    std::set<wchar_t *, WStrCmp>::iterator d_it;
    int cnt = 1;
    for (d_it = cinfo->parts->begin(); d_it != cinfo->parts->end(); d_it++) {
        wchar_t wname[CREO_NAME_MAX];
        struct bu_vls *rname;
        struct directory *rdp;
        ProMdl m;
        ProWVerstamp cstamp, gstamp;
        if (ProMdlnameInit(*d_it, PRO_MDLFILE_PART, &m) != PRO_TK_NO_ERROR) return;
        if (ProMdlNameGet(m, wname) != PRO_TK_NO_ERROR) return;

        /*
         * If the part:
         *  a) exists in the .g file already and...
         *  b) has the same CREO version stamp as the part in the current
         *     CREO file
         * then we don't need to re-export it to the .g file
         */

        rname = get_brlcad_name(cinfo, wname, "r", N_REGION);
        rdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(rname), LOOKUP_QUIET);
        if (rdp != RT_DIR_NULL && ProMdlVerstampGet(m, &cstamp) == PRO_TK_NO_ERROR) {
            const char *vs = NULL;
            struct bu_attribute_value_set r_avs;
            db5_get_attributes(cinfo->wdbp->dbip, &r_avs, rdp);
            vs = bu_avs_get(&r_avs, "CREO_VERSION_STAMP");
            if (vs && ProStringVerstampGet((char *)vs, &gstamp) == PRO_TK_NO_ERROR && ProVerstampEqual(cstamp, gstamp) == PRO_B_TRUE) {
                /*
                 * Skip the .g object if it was created from the same
                 * version of the object that exists currently in the
                 * CREO file
                 */
                creo_log(cinfo, MSG_OK, "%s exists and is current - skipping\n", bu_vls_addr(rname));
                continue;
            } else {
                /*
                 * Kill the existing object (region and child solid)
                 * it's out of sync with CREO
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
    creo_log(cinfo, MSG_STATUS, "Processing part %d of %d", cnt++, cinfo->parts->size());
    if (output_part(cinfo, m) == PRO_TK_NOT_EXIST) cinfo->empty->insert(*d_it);
    }
}


extern "C" void
output_assems(struct creo_conv_info *cinfo)
{
    std::set<wchar_t *, WStrCmp>::iterator d_it;
    int cnt = 1;
    for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
        wchar_t wname[CREO_NAME_MAX];
        struct bu_vls *aname;
        struct directory *adp;
        ProMdl parent;
        ProWVerstamp cstamp, gstamp;
        ProMdlnameInit(*d_it, PRO_MDLFILE_ASSEMBLY, &parent);
        if (ProMdlNameGet(parent, wname) != PRO_TK_NO_ERROR) continue;

        /*
         * If the part:
         *  a) exists in the .g file already and...
         *  b) has the same CREO version stamp as the part in the current
         *     CREO file
         * then we don't need to re-export it to the .g file
         */
        aname = get_brlcad_name(cinfo, wname, NULL, N_ASSEM);
        adp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(aname), LOOKUP_QUIET);
        if (adp != RT_DIR_NULL && ProMdlVerstampGet(parent, &cstamp) == PRO_TK_NO_ERROR) {
            const char *vs = NULL;
            struct bu_attribute_value_set a_avs;
            db5_get_attributes(cinfo->wdbp->dbip, &a_avs, adp);
            vs = bu_avs_get(&a_avs, "CREO_VERSION_STAMP");
            if (vs && ProStringVerstampGet((char *)vs, &gstamp) == PRO_TK_NO_ERROR && ProVerstampEqual(cstamp, gstamp) == PRO_B_TRUE) {
            /*
             * Skip the .g object if it was created from the same
             * version of the object that exists currently in the
             * CREO file
             */
             creo_log(cinfo, MSG_OK, "%s exists and is current - skipping\n", bu_vls_addr(aname));
             continue;
        } else {
            /* Kill the existing object - it's out of sync with CREO */
            db_delete(cinfo->wdbp->dbip, adp);
            db_dirdelete(cinfo->wdbp->dbip, adp);
            db_update_nref(cinfo->wdbp->dbip, &rt_uniresource);
        }
    }

    /* Skip if we've determined this one is empty */
    if (cinfo->empty->find(wname) != cinfo->empty->end()) continue;

    /* All set - process the assembly */
    creo_log(cinfo, MSG_STATUS, "Processing assembly %d of %d", cnt++, cinfo->assems->size());
    output_assembly(cinfo, parent);
    }
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
    ProError loc_status;
    ProMdl model;
    ProMdlType type;
    ProBoolean is_skel = PRO_B_FALSE;
    wchar_t wname[CREO_NAME_MAX];
    char name[CREO_NAME_MAX];
    wchar_t *wname_saved;
    struct creo_conv_info *cinfo = (struct creo_conv_info *)app_data;

    /* Get feature name */
    if ((loc_status = ProAsmcompMdlNameGet(feat, &type, wname)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "%s: failure getting name of child\n", name);
        return PRO_TK_NO_ERROR;
    }

    (void)ProWstringToString(name, wname);

    /* Get the model for this member */
    if ((loc_status = ProAsmcompMdlGet(feat, &model)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "%s: failure getting model\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* Get its type (only a part or assembly should make it here) */
    if ((loc_status = ProMdlTypeGet(model, &type)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "%s: failure getting type\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* If this is a skeleton, we're done */
    ProMdlIsSkeleton(model, &is_skel);
    if (is_skel) {
        creo_log(cinfo, MSG_FAIL, "%s: is skeleton, skipping\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* Log this member */
    switch (type) {
        case PRO_MDL_ASSEMBLY:
            if (cinfo->assems->find(wname) == cinfo->assems->end()) {
                wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "CREO name");
                wcsncpy(wname_saved, wname, wcslen(wname)+1);
                cinfo->assems->insert(wname_saved);
                creo_log(cinfo, MSG_DEBUG, "walking into %s\n", name);
                ProSolidFeatVisit(ProMdlToPart(model), objects_gather, (ProFeatureFilterAction)component_filter, app_data);
            }
            break;
        case PRO_MDL_PART:
            if (cinfo->parts->find(wname) == cinfo->parts->end()) {
                wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "CREO name");
                wcsncpy(wname_saved, wname, wcslen(wname)+1);
                cinfo->parts->insert(wname_saved);
            }
            break;
        default:
            creo_log(cinfo, MSG_DEBUG, "%s: is neither an assembly or a part, skipping\n", name);
            return PRO_TK_NO_ERROR;
            break;
    }

    return PRO_TK_NO_ERROR;
}


/**
 * Routine to output the top level object that is currently displayed in CREO.
 * This is the real beginning of the processing code - doit collects user
 * settings and calls this function.
 */
extern "C" void
output_top_level_object(struct creo_conv_info *cinfo, ProMdl model, ProMdlType type)
{
    wchar_t wname[CREO_NAME_MAX];
    char name[CREO_NAME_MAX];
    wchar_t *wname_saved;
    struct directory *tdp = RT_DIR_NULL;

    /* Get object name */
    if (ProMdlNameGet(model, wname) != PRO_TK_NO_ERROR) return;
    (void)ProWstringToString(name, wname);

    /* Save name */
    wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "CREO name");
    wcsncpy(wname_saved, wname, wcslen(wname)+1);

    /*
     * There are two possibilities - either we have a hierarchy,
     * in which case we need to walk it and collect the objects
     * to process, or we have a single part which we can process
     * directly.
     */
    if (type == PRO_MDL_PART) {
        /* One part only */
        cinfo->parts->insert(wname_saved);
        output_parts(cinfo);
    } else if (type == PRO_MDL_ASSEMBLY) {
        /* Walk the hierarchy and process all necessary assemblies and parts */
        cinfo->assems->insert(wname_saved);
        ProSolidFeatVisit(ProMdlToPart(model), objects_gather, (ProFeatureFilterAction)component_filter, (ProAppData)cinfo);
        output_parts(cinfo);
        find_empty_assemblies(cinfo);
        (void)ProWindowRefresh(PRO_VALUE_UNUSED);
        output_assems(cinfo);
    } else {
        creo_log(cinfo, MSG_OK, "Top level object %s is neither PART nor ASSEMBLY, skipping", name);
        return;
    }

    /* Make a final toplevel comb based on the file name to hold the orientation matrix */
    struct bu_vls *comb_name;
    struct bu_vls top_name = BU_VLS_INIT_ZERO;
    struct wmember wcomb;
    struct directory *dp;
    BU_LIST_INIT(&wcomb.l);
    mat_t m;
    bn_decode_mat(m, "0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1");
    comb_name = get_brlcad_name(cinfo, wname, NULL, N_ASSEM);
    if ((dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(comb_name), LOOKUP_QUIET)) == RT_DIR_NULL) {
        comb_name = get_brlcad_name(cinfo, wname, NULL, N_REGION);
    }
    (void)mk_addmember(bu_vls_addr(comb_name), &(wcomb.l), m, WMOP_UNION);

    /* Guarantee we have a non-colliding top level name */
    bu_vls_sprintf(&top_name, "all.g");
    tdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(&top_name), LOOKUP_QUIET);
    if (tdp != RT_DIR_NULL) {
        bu_vls_sprintf(&top_name, "all-1.g");
        long count = 1;
        tdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(&top_name), LOOKUP_QUIET);
        while (tdp != RT_DIR_NULL) {
            (void)bu_vls_incr(&top_name, NULL, "0:0:0:0:-", NULL, NULL);
            if (count == LONG_MAX) {
                creo_log(cinfo, MSG_FAIL, "%s: top level name gen failed\n", bu_vls_cstr(cinfo->output_file));
                break;
            }
        }
    }
    if (tdp == RT_DIR_NULL) mk_lcomb(cinfo->wdbp, bu_vls_addr(&top_name), &wcomb, 0, NULL, NULL, NULL, 0);
    bu_vls_free(&top_name);

}


extern "C" void
doit(char *UNUSED(dialog), char *UNUSED(compnent), ProAppData UNUSED(appdata))
{
    ProError status;
    ProMdl model;
    ProMdlType type;
    ProLine tmp_line = {'\0'};
    wchar_t *tmp_str;
    int n_selected_names;
    char **selected_names;

    /* This replaces the global variables used in the original Creo converter */
    struct creo_conv_info *cinfo = new creo_conv_info;
    creo_conv_info_init(cinfo);

    ProStringToWstring(tmp_line, "Not processing");
    status = ProUILabelTextSet("creo_brl", "curr_proc", tmp_line);

    /* Set up logging */
    {
        char log_file[MAXPATHLEN];
        char logger_type_str[MAXPATHLEN];

        /* Get the name of the log file */
        status = ProUIInputpanelValueGet("creo_brl", "log_file", &tmp_str);
        if (status != PRO_TK_NO_ERROR) {
            fprintf(stderr, "Failed to get log file name\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            return;
        }
        ProWstringToString(log_file, tmp_str);
        ProWstringFree(tmp_str);

        /* Set logger type */
        status = ProUIRadiogroupSelectednamesGet("creo_brl", "log_file_type_rg", &n_selected_names, &selected_names);
        if (status != PRO_TK_NO_ERROR) {
            fprintf(stderr, "Failed to get log file type\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            return;
        }
        sprintf(logger_type_str,"%s", selected_names[0]);
        ProStringarrayFree(selected_names, n_selected_names);
        if (BU_STR_EQUAL("Failure", logger_type_str))
            cinfo->logger_type = LOGGER_TYPE_FAILURE;
        else if (BU_STR_EQUAL("Success", logger_type_str))
            cinfo->logger_type = LOGGER_TYPE_SUCCESS;
        else if (BU_STR_EQUAL("Failure/Success", logger_type_str))
            cinfo->logger_type = LOGGER_TYPE_FAILURE_OR_SUCCESS;
        else
            cinfo->logger_type = LOGGER_TYPE_ALL;

        /* Open log file, if a name was provided */
        if (strlen(log_file) > 0) {
            if ((cinfo->logger=fopen(log_file, "wb")) == NULL) {
                creo_log(cinfo, MSG_FAIL, "Cannot open log file");
                creo_conv_info_free(cinfo);
                ProUIDialogDestroy("creo_brl");
                return;
            }
        } else {
            cinfo->logger = (FILE *)NULL;
        }
    }

    /* Set up the output file */
    {
        /* Get string from dialog */
        char output_file[128];
        wchar_t *w_output_file;
        status = ProUIInputpanelValueGet("creo_brl", "output_file", &w_output_file);
        if (status != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_FAIL, "Failed to get output file name\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            return;
        }
        ProWstringToString(output_file, w_output_file);
        ProWstringFree(w_output_file);

        /*
         * If there is a pre-existing file, open it - it may be we
         * only have to update some items in it.
         */
        if (bu_file_exists(output_file, NULL)) {
            if ((cinfo->dbip = db_open(output_file, DB_OPEN_READWRITE)) != DBI_NULL) {
                cinfo->wdbp = wdb_dbopen(cinfo->dbip, RT_WDB_TYPE_DB_DISK);
                creo_log(cinfo, MSG_OK, "Note: %s exists - opening to update.\n", output_file);
            } else {
                creo_log(cinfo, MSG_FAIL, "Cannot open existing file: \n", output_file);
                return;
            }
        } else {
            /* Open output file */
            if ((cinfo->dbip = db_create(output_file, 5)) == DBI_NULL) {
                creo_log(cinfo, MSG_FAIL, "Cannot open output file: %s.\n", output_file);
                creo_conv_info_free(cinfo);
                ProUIDialogDestroy("creo_brl");
                return;
            }
            cinfo->wdbp = wdb_dbopen(cinfo->dbip, RT_WDB_TYPE_DB_DISK);
        }

        /* Store the filename for later use */
        bu_vls_sprintf(cinfo->output_file, "%s", output_file);
    }

    /* Read user supplied list of model parameters for naming info. */
    {
        /* Get string from dialog */
        char attr_rename[MAXPATHLEN];
        wchar_t *w_attr_rename;
        status = ProUIInputpanelValueGet("creo_brl", "attr_rename", &w_attr_rename);
        if (status != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_FAIL, "Failed to get name of model parameter specification file.\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            return;
        }
        ProWstringToString(attr_rename, w_attr_rename);
        ProWstringFree(w_attr_rename);

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
                    if (std::string::npos != startpos) {
                        pkey = pkey.substr(startpos);
                    }
                    size_t endpos = pkey.find_last_not_of(" \t\n\v\f\r");
                    if (std::string::npos != endpos) {
                        pkey = pkey.substr(0 ,endpos+1);
                    }
                    if (pkey.length() > 0) {
                        creo_log(cinfo, MSG_DEBUG, "Found model parameter naming key: %s.\n", pkey.c_str());
                        cinfo->model_parameters->push_back(bu_strdup(pkey.c_str()));
                    }
                }
            }
        }
    }

    /* Read user-supplied list of model attributes for conversion */
    {
        /* Get string from dialog */
        char attr_save[MAXPATHLEN];
        wchar_t *w_attr_save;
        status = ProUIInputpanelValueGet("creo_brl", "attr_save", &w_attr_save);
        if (status != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_FAIL, "Failed to get name of attribute list file.\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            return;
        }
        ProWstringToString(attr_save, w_attr_save);
        ProWstringFree(w_attr_save);

        if (strlen(attr_save) > 0) {
            /* Parse file contents into a list of parameter keys */
            std::string afilestr(attr_save);
            std::istringstream ss(afilestr);
            std::string line;
            while (std::getline(ss, line)) {
                std::string pkey;
                std::istringstream ls(line);
                while (std::getline(ls, pkey, ',')) {
                    /* Scrub leading and trailing whitespace */
                    size_t startpos = pkey.find_first_not_of(" \t\n\v\f\r");
                    if (std::string::npos != startpos) {
                        pkey = pkey.substr(startpos);
                    }
                    size_t endpos = pkey.find_last_not_of(" \t\n\v\f\r");
                    if (std::string::npos != endpos) {
                        pkey = pkey.substr(0 ,endpos+1);
                    }
                    if (pkey.length() > 0) {
                        creo_log(cinfo, MSG_DEBUG, "Found attribute key: %s.\n", pkey.c_str());
                        cinfo->attrs->push_back(bu_strdup(pkey.c_str()));
                    }
                }
            }
        }
    }

    /* Get starting ident */
    if ((status = ProUIInputpanelValueGet("creo_brl", "starting_ident", &tmp_str)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get starting ident.\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->reg_id = wstr_to_long(cinfo, tmp_str);
        V_MAX(cinfo->reg_id, 1);
        ProWstringFree(tmp_str);
    }

    /* Get max error */
    if ((status = ProUIInputpanelValueGet("creo_brl", "max_error", &tmp_str)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get max tessellation error.\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->max_error = wstr_to_double(cinfo, tmp_str);
        ProWstringFree(tmp_str);
    }

    /* Get min error */
    if ((status = ProUIInputpanelValueGet("creo_brl", "min_error", &tmp_str)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get min tessellation error.\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->min_error = wstr_to_double(cinfo, tmp_str);
        ProWstringFree(tmp_str);
        V_MAX(cinfo->max_error, cinfo->min_error);
    }

    /* Get the max angle control */
    if ((status = ProUIInputpanelValueGet("creo_brl", "max_angle_ctrl", &tmp_str)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get max angle control.\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->max_angle_cntrl = wstr_to_double(cinfo, tmp_str);
        ProWstringFree(tmp_str);
    }

    /* Get the min angle control */
    if ((status = ProUIInputpanelValueGet("creo_brl", "min_angle_ctrl", &tmp_str)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get min angle control.\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->min_angle_cntrl = wstr_to_double(cinfo, tmp_str);
        ProWstringFree(tmp_str);
        V_MAX(cinfo->max_angle_cntrl, cinfo->min_angle_cntrl);
    }

    /* Get the max to min steps */
    if ((status = ProUIInputpanelValueGet("creo_brl", "isteps", &tmp_str)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get max to min steps.\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->max_to_min_steps = wstr_to_long(cinfo, tmp_str);
        ProWstringFree(tmp_str);
        if (cinfo->max_to_min_steps <= 0) {
            cinfo->max_to_min_steps = 0;
        } else {
            cinfo->error_increment = (ZERO((cinfo->max_error - cinfo->min_error))) ? 0 : (cinfo->max_error - cinfo->min_error) / (double)cinfo->max_to_min_steps;
            cinfo->angle_increment = (ZERO((cinfo->max_angle_cntrl - cinfo->min_angle_cntrl))) ? 0 : (cinfo->max_angle_cntrl - cinfo->min_angle_cntrl) / (double)cinfo->max_to_min_steps;
            if (ZERO(cinfo->error_increment) && ZERO(cinfo->angle_increment)) cinfo->max_to_min_steps = 0;
        }
    }

    /* Check if user wants to do any CSG */
    if ((status = ProUICheckbuttonGetState("creo_brl", "facets_only", &cinfo->do_facets_only)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get checkbutton setting (facetize only).\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    }

    /* Check if user wants to eliminate small features */
    if ((status = ProUICheckbuttonGetState("creo_brl", "elim_small", &cinfo->do_elims)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get checkbutton setting (eliminate small features).\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    }

    /* Check if user wants to test solidity */
    if ((status = ProUICheckbuttonGetState("creo_brl", "check_solidity", &cinfo->check_solidity)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get checkbutton setting (check solidity).\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    }

    /* Check if user wants surface normals in the BOT's */
    if ((status = ProUICheckbuttonGetState("creo_brl", "get_normals", &cinfo->get_normals)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get checkbutton setting (extract surface normals).\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    }

    /* Check for user selected log file type */
    if ((status = ProUICheckbuttonGetState("creo_brl", "debug_bboxes", &cinfo->debug_bboxes)) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_FAIL, "Failed to get checkbutton setting (debugging bboxes).\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    }

    if (cinfo->do_elims) {
        /* Get the minimum hole diameter */
        if ((status = ProUIInputpanelValueGet("creo_brl", "min_hole", &tmp_str)) != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_FAIL, "Failed to get minimum hole diameter.\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            return;
        } else {
            cinfo->min_hole_diameter = wstr_to_double(cinfo, tmp_str);
            V_MAX(cinfo->min_hole_diameter, 0.0);
            ProWstringFree(tmp_str);
        }

        /* Get the minimum chamfer dimension */
        if ((status = ProUIInputpanelValueGet("creo_brl", "min_chamfer", &tmp_str)) != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_FAIL, "Failed to get minimum chamfer diameter.\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            return;
        } else {
            cinfo->min_chamfer_dim = wstr_to_double(cinfo, tmp_str);
            V_MAX(cinfo->min_chamfer_dim, 0.0);
            ProWstringFree(tmp_str);
        }

        /* Get the minimum round radius */
        if ((status = ProUIInputpanelValueGet("creo_brl", "min_round", &tmp_str)) != PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_FAIL, "Failed to get minimum round radius.\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            return;
        } else {
            cinfo->min_round_radius = wstr_to_double(cinfo, tmp_str);
            V_MAX(cinfo->min_round_radius, 0.0);
            ProWstringFree(tmp_str);
        }

    } else {
        cinfo->min_hole_diameter = 0.0;
        cinfo->min_round_radius  = 0.0;
        cinfo->min_chamfer_dim   = 0.0;
    }

    /* Get currently displayed model in Creo */
    if ((status = ProMdlCurrentGet(&model)) == PRO_TK_BAD_CONTEXT) {
        creo_log(cinfo, MSG_FAIL, "No model is displayed!!\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
        }

    /* Get model type */
    if ((status = ProMdlTypeGet(model, &type)) == PRO_TK_BAD_INPUTS) {
        creo_log(cinfo, MSG_FAIL, "Cannot get type of current model!!\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    }

    /* Limit scope to parts and assemblies, no drawings, etc. */
    if (type != PRO_MDL_ASSEMBLY && type != PRO_MDL_PART) {
        creo_log(cinfo, MSG_FAIL, "Current model is not a solid object.\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    }

    /* TODO - verify this is working correctly */
    creo_model_units(&(cinfo->creo_to_brl_conv), model);

    /* Adjust tolerance for Creo units */
    cinfo->local_tol = cinfo->tol_dist / cinfo->creo_to_brl_conv;
    cinfo->local_tol_sq = cinfo->local_tol * cinfo->local_tol;

    /*
     * Output the top-level object
     * this will recurse through the entire model
     */
    output_top_level_object(cinfo, model, type);

    /* Let user know we are done */
    ProStringToWstring(tmp_line, "Conversion complete");
    ProUILabelTextSet("creo_brl", "curr_proc", tmp_line);
    ProMessageClear();

    if (cinfo->warn_feature_unsuppress) {
        struct bu_vls errmsg = BU_VLS_INIT_ZERO;
        bu_vls_sprintf(&errmsg, "During the conversion, one or more parts had features suppressed.\nAfter the conversion was complete, an attempt was made to unsuppress\nthese same features.  One or more unsuppression operations failed, so\nsome features are still  suppressed.  Please exit CREO without saving\nany changes so that this problem will not persist.");
        PopupMsg("Warning - Restoration Failure", bu_vls_addr(&errmsg));
        bu_vls_free(&errmsg);
    }
    creo_conv_info_free(cinfo);
    return;
}


extern "C" void
elim_small_activate(char *dialog_name, char *button_name, ProAppData UNUSED(data))
{
    ProBoolean state;

    if (ProUICheckbuttonGetState(dialog_name, button_name, &state) != PRO_TK_NO_ERROR) {
        fprintf(stderr, "checkbutton activate routine: failed to get state\n");
        return;
    }

    if (state) {
        if (ProUIInputpanelEditable(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "Failed to activate \"minimum hole diameter\"");
            return;
        }
        if (ProUIInputpanelEditable(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "Failed to activate \"minimum chamfer dimension\"");
            return;
        }
        if (ProUIInputpanelEditable(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "Failed to activate \"minimum round radius\"");
            return;
        }
    } else {
        if (ProUIInputpanelReadOnly(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "Failed to de-activate \"minimum hole diameter\"");
            return;
        }
        if (ProUIInputpanelReadOnly(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "Failed to de-activate \"minimum chamfer dimension\"");
            return;
        }
        if (ProUIInputpanelReadOnly(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "Failed to de-activate \"minimum round radius\"");
            return;
        }
    }
}


extern "C" void
do_quit(char *UNUSED(dialog), char *UNUSED(compnent), ProAppData UNUSED(appdata))
{
    ProUIDialogDestroy("creo_brl");
}


/** Driver routine for converting CREO to BRL-CAD */
extern "C" int
creo_brl(uiCmdCmdId UNUSED(command), uiCmdValue *UNUSED(p_value), void *UNUSED(p_push_cmd_data))
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int destroy_dialog = 0;
    int ret_status;

    creo_log(NULL, MSG_STATUS, "Launching creo_brl...");

    /* Use UI dialog */
    if (ProUIDialogCreate("creo_brl", "creo_brl") != PRO_TK_NO_ERROR) {
        bu_vls_printf(&vls, "Failed to create dialog box for creo-brl\n");
        goto print_msg;
    }

    if (ProUICheckbuttonActivateActionSet("creo_brl", "elim_small", elim_small_activate, NULL) != PRO_TK_NO_ERROR) {
        bu_vls_printf(&vls, "Failed to set action for \"eliminate small features\" checkbutton\n");
        goto print_msg;
    }

    if (ProUIPushbuttonActivateActionSet("creo_brl", "doit", doit, NULL) != PRO_TK_NO_ERROR) {
        bu_vls_printf(&vls, "Failed to set action for 'Go' button\n");
        destroy_dialog = 1;
        goto print_msg;
    }

    if (ProUIPushbuttonActivateActionSet("creo_brl", "quit", do_quit, NULL) != PRO_TK_NO_ERROR) {
        bu_vls_printf(&vls, "Failed to set action for 'Quit' button\n");
        destroy_dialog = 1;
        goto print_msg;
    }

    /* File names may get longer than the default... */
    ProUIInputpanelMaxlenSet("creo_brl", "output_file", MAXPATHLEN - 1);
    ProUIInputpanelMaxlenSet("creo_brl", "log_file", MAXPATHLEN - 1);

    ProMdl model;
    if (ProMdlCurrentGet(&model) != PRO_TK_BAD_CONTEXT) {
        wchar_t wname[CREO_NAME_MAX];
        if (ProMdlNameGet(model, wname) == PRO_TK_NO_ERROR) {
            struct bu_vls groot = BU_VLS_INIT_ZERO;
            struct bu_vls lroot = BU_VLS_INIT_ZERO;
            char name[CREO_NAME_MAX];
            (void)ProWstringToString(name, wname);
            /* Supply a sensible default for the .g file... */
            if (bu_path_component(&groot, name, BU_PATH_BASENAME_EXTLESS)) {
                wchar_t wgout[CREO_NAME_MAX];
                bu_vls_printf(&groot, ".g");
                (void)ProStringToWstring(wgout, bu_vls_addr(&groot));
                ProUIInputpanelValueSet("creo_brl", "output_file", wgout);
                bu_vls_free(&groot);
           }
           /* Suggest a default log file... */
           if (bu_path_component(&lroot, name, BU_PATH_BASENAME_EXTLESS)) {
               /* Supply a sensible default for the .g file... */
               wchar_t wgout[CREO_NAME_MAX];
               bu_vls_printf(&lroot, "-log.txt");
               (void)ProStringToWstring(wgout, bu_vls_addr(&lroot));
               ProUIInputpanelValueSet("creo_brl", "log_file", wgout);
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
    ProUIInputpanelMaxlenSet("creo_brl", "attr_save", MAXPATHLEN - 1);

    if (ProUIDialogActivate("creo_brl", &ret_status) != PRO_TK_NO_ERROR) {
        bu_vls_printf(&vls, "Error in creo-brl Dialog: dialog returned %d\n", ret_status);
    }

    print_msg:
    if (bu_vls_strlen(&vls) > 0) creo_log(NULL, MSG_FAIL, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    if (destroy_dialog) ProUIDialogDestroy("creo_brl");
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
 * and user_terminate - are dictated by the CREO API.  These are the
 * hooks that tie the rest of the code into the CREO system.  Both
 * are *required* to be present, even if the user_terminate function
 * doesn't do any actual work.
 */

extern "C" int user_initialize()
{
    ProError status;
    int i;
    uiCmdCmdId cmd_id;

    /* Creo says always check the size of w_char */
    status = ProWcharSizeVerify (sizeof (wchar_t), &i);
    if (status != PRO_TK_NO_ERROR || (i != sizeof (wchar_t))) {
        creo_log(NULL, MSG_FAIL, "ERROR wchar_t Incorrect size (%d). Should be: %d", (int)sizeof(wchar_t), i);
        return -1;
    }

    /* Add a command that calls our creo-brl routine */
    status = ProCmdActionAdd("CREO-BRL", (uiCmdCmdActFn)creo_brl, uiProe2ndImmediate, creo_brl_access, PRO_B_FALSE, PRO_B_FALSE, &cmd_id);
    if (status != PRO_TK_NO_ERROR) {
        creo_log(NULL, MSG_FAIL, "Failed to add creo-brl action");
        return -1;
    }

    /* Add a menu item that runs the new command */
    ProFileName msgfil = {'\0'};
    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);
    status = ProMenubarmenuPushbuttonAdd("File", "CREO-BRL", "CREO-BRL", "CREO-BRL-HELP", "File.psh_exit", PRO_B_FALSE, cmd_id, msgfil);
    if (status != PRO_TK_NO_ERROR) {
        creo_log(NULL, MSG_FAIL, "Failed to add creo-brl menu button");
        return -1;
    }


    /* Let user know we are here */
    //PopupMsg("Plugin Successfully Loaded", "The CREO to BRL-CAD converter plugin Version 0.2 was successfully loaded.");

    return 0;
}


extern "C" void user_terminate()
{
    ProMessageClear();
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
