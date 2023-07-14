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

/*                                                                              */
/* Synopsis of Main Routines:                                                   */
/*                                                                              */
/* creo_conv_info_init       Initialize the conversion information container    */
/* creo_conv_info_free       Clear the conversion information container         */
/* output_parts              Output all the parts                               */
/* output_assems             Output all the assemblies                          */
/* objects_gather            Build up the sets of assemblies and parts          */
/* output_settings           Output converter settings to the current (.g) file */
/* output_top_level_object   Output the top-level object                        */
/* doit                      Collect user-specified conversion settings         */
/* elim_small_activate       Activates small feature settings                   */
/* do_quit                   Exit the converter dialog                          */
/* creo_brl                  Driver routine for converting Creo to BRL-CAD      */
/* creo_brl_access           Enable Creo menu acsess                            */
/* user_initialize           Required by Creo API                               */
/* user_terminate            Required by Creo API                               */
/*                                                                              */

#include "common.h"
#include <algorithm>
#include <sstream>
#include "creo-brl.h"


/* Initialize the conversion information container */
extern "C" void
creo_conv_info_init(struct creo_conv_info *cinfo)
{

    BU_GET(cinfo->out_fname, struct bu_vls);          /* output file name     */
    bu_vls_init(cinfo->out_fname);

    BU_GET(cinfo->log_mode, struct bu_vls);           /* log file mode        */
    bu_vls_init(cinfo->log_mode);

    BU_GET(cinfo->log_fname, struct bu_vls);          /* log file name        */
    bu_vls_init(cinfo->log_fname);

    BU_GET(cinfo->mtl_fname, struct bu_vls);          /* material file name   */
    bu_vls_init(cinfo->mtl_fname);

    BU_GET(cinfo->param_rename, struct bu_vls);       /* renaming parameters  */
    bu_vls_init(cinfo->param_rename);

    BU_GET(cinfo->param_save, struct bu_vls);         /* preserved parameters */
    bu_vls_init(cinfo->param_save);

    BU_GET(cinfo->curr_pname, struct bu_vls);         /* current part name    */
    bu_vls_init(cinfo->curr_pname);

    BU_GET(cinfo->comb_name, struct bu_vls);         /* combination name    */
    bu_vls_init(cinfo->comb_name);

    BU_GET(cinfo->unitsys, struct bu_vls);            /* unit system  */
    bu_vls_init(cinfo->unitsys);

    BU_GET(cinfo->aunits, struct bu_vls);             /* angle units  */
    bu_vls_init(cinfo->aunits);

    BU_GET(cinfo->funits, struct bu_vls);             /* force units  */
    bu_vls_init(cinfo->funits);

    BU_GET(cinfo->munits, struct bu_vls);             /* mass units   */
    bu_vls_init(cinfo->munits);

    BU_GET(cinfo->lunits, struct bu_vls);             /* length units */
    bu_vls_init(cinfo->lunits);

    BU_GET(cinfo->tunits, struct bu_vls);             /* time units   */
    bu_vls_init(cinfo->tunits);

    cinfo->fplog = (FILE *)NULL;                      /* log file settings */
    cinfo->curr_log_type = LOGGER_TYPE_ALL;
    cinfo->curr_msg_type = MSG_DEBUG;

    cinfo->fpmtl = (FILE *)NULL;                      /* material file data */
    memset(cinfo->mtl_key, '\0', sizeof(cinfo->mtl_key));
    memset(cinfo->mtl_str, '\0', sizeof(cinfo->mtl_str));
    memset(cinfo->mtl_id , '\0', sizeof(cinfo->mtl_id ));
    memset(cinfo->mtl_los, '\0', sizeof(cinfo->mtl_los));
    cinfo->mtl_ptr = -1;
    cinfo->mtl_rec = -1;

    cinfo->xform_mode = XFORM_NONE;                   /* xform mode */

    cinfo->wdbp = NULL;

    /* Initial unit assumptions */
    cinfo->main_to_mm   = 25.4;    /* inches to mm  */
    cinfo->part_to_mm   = 25.4;    /* inches to mm  */
    cinfo->local_tol    = 0.0100;  /* units of mm   */
    cinfo->local_tol_sq = 0.0001;  /* units of mm^2 */

    /* Tessellation settings */
    cinfo->max_angle    = 1.00;
    cinfo->min_angle    = 0.50;
    cinfo->max_chord    = 0.2000;
    cinfo->min_chord    = 0.0200;
    cinfo->min_edge     = 0.0100;
    cinfo->max_facets   = 8000;
    cinfo->max_steps    = 20;

    /* CSG settings */
    cinfo->min_hole     = 0.0;
    cinfo->min_chamfer  = 0.0;
    cinfo->min_round    = 0.0;

    /* Tessellation results */
    cinfo->tess_bbox    = 0;

    /* Conversion Process results */
    cinfo->asm_count    = 0;
    cinfo->asm_total    = 0;
    cinfo->prt_count    = 0;
    cinfo->prt_total    = 0;

    cinfo->parts  = new std::set<wchar_t *, WStrCmp>;
    cinfo->assems = new std::set<wchar_t *, WStrCmp>;
    cinfo->empty  = new std::set<wchar_t *, WStrCmp>;

    cinfo->region_name_map = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->assem_name_map  = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->solid_name_map  = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->creo_name_map   = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;

    cinfo->brlcad_names = new std::set<struct bu_vls *, StrCmp>;
    cinfo->creo_names   = new std::set<struct bu_vls *, StrCmp>;

    cinfo->obj_name_params = new std::vector<char *>;
    cinfo->obj_attr_params = new std::vector<char *>;

    cinfo->warn_feature_resume = 0;

}


/* Clear the conversion information container */
extern "C" void
creo_conv_info_free(struct creo_conv_info *cinfo)
{
    bu_vls_free(cinfo->out_fname);
    BU_PUT(cinfo->out_fname, struct bu_vls);

    bu_vls_free(cinfo->log_mode);
    BU_PUT(cinfo->log_mode, struct bu_vls);

    bu_vls_free(cinfo->log_fname);
    BU_PUT(cinfo->log_fname, struct bu_vls);

    bu_vls_free(cinfo->mtl_fname);
    BU_PUT(cinfo->mtl_fname, struct bu_vls);

    bu_vls_free(cinfo->param_rename);
    BU_PUT(cinfo->param_rename, struct bu_vls);

    bu_vls_free(cinfo->param_save);
    BU_PUT(cinfo->param_save, struct bu_vls);

    bu_vls_free(cinfo->curr_pname);
    BU_PUT(cinfo->curr_pname, struct bu_vls);

    bu_vls_free(cinfo->comb_name);
    BU_PUT(cinfo->comb_name, struct bu_vls);

    bu_vls_free(cinfo->unitsys);
    BU_PUT(cinfo->unitsys, struct bu_vls);

    bu_vls_free(cinfo->aunits);
    BU_PUT(cinfo->aunits, struct bu_vls);

    bu_vls_free(cinfo->funits);
    BU_PUT(cinfo->funits, struct bu_vls);

    bu_vls_free(cinfo->munits);
    BU_PUT(cinfo->munits, struct bu_vls);

    bu_vls_free(cinfo->lunits);
    BU_PUT(cinfo->lunits, struct bu_vls);

    bu_vls_free(cinfo->tunits);
    BU_PUT(cinfo->tunits, struct bu_vls);

    memset(cinfo->mtl_key  , '\0', sizeof(cinfo->mtl_key  ));
    memset(cinfo->mtl_str  , '\0', sizeof(cinfo->mtl_str  ));
    memset(cinfo->mtl_id   , '\0', sizeof(cinfo->mtl_id   ));
    memset(cinfo->mtl_los  , '\0', sizeof(cinfo->mtl_los  ));

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

    for (unsigned int i = 0; i < cinfo->obj_name_params->size(); i++) {
        char *str = cinfo->obj_name_params->at(i);
        bu_free(str, "free obj name params string");
    }

    for (unsigned int i = 0; i < cinfo->obj_attr_params->size(); i++) {
        char *str = cinfo->obj_attr_params->at(i);
        bu_free(str, "free obj attr params string");
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


/* Output all the parts */
extern "C" void
output_parts(struct creo_conv_info *cinfo)
{
    std::set<wchar_t *, WStrCmp>::iterator d_it;

    int prt_count = 0;
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

        /* Retain current model */
        cinfo->curr_model = model;

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
            db5_get_attributes(cinfo->wdbp->dbip, &cinfo->avs, rdp);
            vs = bu_avs_get(&cinfo->avs, "ptc_version_stamp");
            if (vs && ProStringVerstampGet((char *)vs, &gstamp) == PRO_TK_NO_ERROR
                   && ProVerstampEqual(cstamp, gstamp)          == PRO_B_TRUE) {
                /*
                 * Skip the .g object if it was created from the same
                 * version of the object that exists currently in the
                 * Creo file
                 */
                creo_log(cinfo, MSG_SUCCESS, "Region \"%s\" exists and is current, skipping...\n",
                                              bu_vls_addr(rname));
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
        if (output_part(cinfo) == PRO_TK_NOT_EXIST) {
            creo_log(cinfo, MSG_STATUS, "Part \"%s\" failed to convert", bu_vls_addr(rname));
            if (!cinfo->tess_bbox)
                cinfo->empty->insert(*d_it);
        } else
            creo_log(cinfo, MSG_STATUS, "Part %d of %zu converted", ++prt_count, cinfo->parts->size());

    }

    /* Retain conversion process results */
    cinfo->prt_count = prt_count;
    cinfo->prt_total = cinfo->parts->size();
}


/* Output all the assemblies */
extern "C" void
output_assems(struct creo_conv_info *cinfo)
{
    std::set<wchar_t *, WStrCmp>::iterator d_it;

    int asm_count = 0;
    for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
        wchar_t wname[CREO_NAME_MAX];
        struct bu_vls *aname;
        struct directory *adp;
        ProMdl parent;
        ProWVerstamp cstamp, gstamp;

        ProMdlnameInit(*d_it, PRO_MDLFILE_ASSEMBLY, &parent);
        if (ProMdlMdlnameGet(parent, wname) != PRO_TK_NO_ERROR)
            continue;

        /* Retain current parent */
        cinfo->curr_parent = parent;

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
            db5_get_attributes(cinfo->wdbp->dbip, &cinfo->avs, adp);
            vs = bu_avs_get(&cinfo->avs, "ptc_version_stamp");
            if (vs && ProStringVerstampGet((char *)vs, &gstamp) == PRO_TK_NO_ERROR
                   && ProVerstampEqual(cstamp, gstamp)          == PRO_B_TRUE) {
                /*
                 * Skip the .g object if it was created from the same
                 * version of the object that exists currently in the
                 * Creo file
                 */
                creo_log(cinfo, MSG_SUCCESS, "Assembly \"%s\" exists and is current, skipping...\n",
                         bu_vls_addr(aname));
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
        if (output_assembly(cinfo) != PRO_TK_NO_ERROR)
            creo_log(cinfo, MSG_STATUS, "Assembly \"%s\" failed to convert", bu_vls_addr(aname));
        else
            creo_log(cinfo, MSG_STATUS, "Assembly %d of %zu succeeded", ++asm_count, cinfo->assems->size());

    }

    /* Retain assembly process results */
    cinfo->asm_count = asm_count;
    cinfo->asm_total = cinfo->assems->size();

}


/**
 * Build up the sets of assemblies and parts.  Doing a feature
 * visit for all top-level objects will result in a recursive walk
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
        creo_log(cinfo, MSG_FILE, "Failed to get type for \"%s\"\n", name);
        (void)ProWstringFree(wname);
        return PRO_TK_NO_ERROR;
    }

    /* Get assembly component model */
    err = ProAsmcompMdlGet(feat, &model);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_MODEL, "Failed to get handle for \"%s\"\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* Get model type (only a part or assembly should make it here) */
    err = ProMdlTypeGet(model, &mtype);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_MODEL, "Failed to get type for \"%s\"\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* If this is a skeleton, we're done */
    ProMdlIsSkeleton(model, &is_skel);
    if (is_skel) {
        creo_log(cinfo, MSG_MODEL, "\"%s\" is a \"skeleton\", skipping...\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* Log this member */
    switch (mtype) {
        case PRO_MDL_ASSEMBLY:
            if (cinfo->assems->find(wname) == cinfo->assems->end()) {
                wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "ptc_asm_name");
                wcsncpy(wname_saved, wname, wcslen(wname)+1);
                cinfo->assems->insert(wname_saved);
                creo_log(cinfo, MSG_ASSEM, "Walking into \"%s\"\n", name);
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
            creo_log(cinfo, MSG_MODEL, "\"%s\" is not a PART or an ASSEMBLY, skipping...\n", name);
            return PRO_TK_NO_ERROR;
    }

    return PRO_TK_NO_ERROR;
}


/* Output converter settings to the current (.g) file */
extern "C" void
output_settings(struct creo_conv_info *cinfo)
{
    struct directory *gdp;
    struct bu_vls global          = BU_VLS_INIT_ZERO;
    struct bu_vls importer        = BU_VLS_INIT_ZERO;
    struct bu_vls units           = BU_VLS_INIT_ZERO;
    struct bu_vls xform_mode      = BU_VLS_INIT_ZERO;
    struct bu_vls region_counter  = BU_VLS_INIT_ZERO;
    struct bu_vls min_luminance   = BU_VLS_INIT_ZERO;
    struct bu_vls max_chord       = BU_VLS_INIT_ZERO;
    struct bu_vls min_angle       = BU_VLS_INIT_ZERO;
    struct bu_vls elim_small      = BU_VLS_INIT_ZERO;
    struct bu_vls min_hole        = BU_VLS_INIT_ZERO;
    struct bu_vls min_chamfer     = BU_VLS_INIT_ZERO;
    struct bu_vls min_round       = BU_VLS_INIT_ZERO;
    struct bu_vls facets_only     = BU_VLS_INIT_ZERO;
    struct bu_vls check_solidity  = BU_VLS_INIT_ZERO;
    struct bu_vls create_boxes    = BU_VLS_INIT_ZERO;
    struct bu_vls write_normals   = BU_VLS_INIT_ZERO;

    /* Assumes global exists */
    bu_vls_sprintf(&global, "%s", "_GLOBAL");
    gdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(&global), LOOKUP_QUIET);
    db5_get_attributes(cinfo->wdbp->dbip, &cinfo->avs, gdp);

    /* Prepare the string conversions */
    if (cinfo->xform_mode == XFORM_Y_TO_Z)
        bu_vls_sprintf(&xform_mode, "%s", "y -> z");
    else if (cinfo->xform_mode == XFORM_X_TO_Z)
        bu_vls_sprintf(&xform_mode, "%s", "x -> z");
    else
        bu_vls_sprintf(&xform_mode, "%s", "none");

    bu_vls_sprintf(&region_counter, "%d", cinfo->region_counter);
    bu_vls_sprintf(&min_luminance,  "%d", cinfo->min_luminance);
    bu_vls_sprintf(&max_chord,      "%g", cinfo->max_chord);
    bu_vls_sprintf(&min_angle,      "%g", cinfo->min_angle);
    bu_vls_sprintf(&elim_small,     "%s", cinfo->elim_small     ? "ON" : "OFF");
    bu_vls_sprintf(&min_hole,       "%g", cinfo->min_hole);
    bu_vls_sprintf(&min_chamfer,    "%g", cinfo->min_chamfer);
    bu_vls_sprintf(&min_round,      "%g", cinfo->min_round);
    bu_vls_sprintf(&facets_only,    "%s", cinfo->facets_only    ? "ON" : "OFF");
/*  bu_vls_sprintf(&check_solidity, "%s", cinfo->check_solidity ? "ON" : "OFF"); */
    bu_vls_sprintf(&check_solidity, "%s", "Disabled");
    bu_vls_sprintf(&create_boxes,   "%s", cinfo->create_boxes   ? "ON" : "OFF");
/*  bu_vls_sprintf(&write_normals,  "%s", cinfo->write_normals  ? "ON" : "OFF"); */
    bu_vls_sprintf(&write_normals,  "%s", "Disabled");
    bu_vls_sprintf(&units,          "%s", "millimeters");
    bu_vls_sprintf(&importer,       "%s", "creo-g");

    /* Create the attributes (LIFO) */
    bu_avs_add(&cinfo->avs, "write_surface_normals",     bu_vls_addr(&write_normals));
    bu_avs_add(&cinfo->avs, "box_replaces_failed_part",  bu_vls_addr(&create_boxes));
    bu_avs_add(&cinfo->avs, "reject_failed_bots",        bu_vls_addr(&check_solidity));
    bu_avs_add(&cinfo->avs, "facetize_everything",       bu_vls_addr(&facets_only));
    bu_avs_add(&cinfo->avs, "minimum_blend_radius",      bu_vls_addr(&min_round));
    bu_avs_add(&cinfo->avs, "minimum_chamfer_dimension", bu_vls_addr(&min_chamfer));
    bu_avs_add(&cinfo->avs, "minimum_hole_diameter",     bu_vls_addr(&min_hole));
    bu_avs_add(&cinfo->avs, "eliminate_small_features",  bu_vls_addr(&elim_small));
    bu_avs_add(&cinfo->avs, "minimum_angle_control",     bu_vls_addr(&min_angle));
    bu_avs_add(&cinfo->avs, "maximum_chord_height",      bu_vls_addr(&max_chord));
    bu_avs_add(&cinfo->avs, "minimum_luminance",         bu_vls_addr(&min_luminance));
    bu_avs_add(&cinfo->avs, "initial_region_counter",    bu_vls_addr(&region_counter));
    bu_avs_add(&cinfo->avs, "coordinate_transformation", bu_vls_addr(&xform_mode));
    bu_avs_add(&cinfo->avs, "preserved_attributes",      bu_vls_addr(cinfo->param_save));
    bu_avs_add(&cinfo->avs, "create_object_names",       bu_vls_addr(cinfo->param_rename));
    bu_avs_add(&cinfo->avs, "material_file_name",        bu_vls_addr(cinfo->mtl_fname));
    bu_avs_add(&cinfo->avs, "process_log_file_name",     bu_vls_addr(cinfo->log_fname));
    bu_avs_add(&cinfo->avs, "process_log_criteria",      bu_vls_addr(cinfo->log_mode));
    bu_avs_add(&cinfo->avs, "output_file_name",          bu_vls_addr(cinfo->out_fname));
    bu_avs_add(&cinfo->avs, "units",                     bu_vls_addr(&units));
    bu_avs_add(&cinfo->avs, "importer",                  bu_vls_addr(&importer));
    bu_avs_add(&cinfo->avs, "title",                     bu_vls_addr(cinfo->comb_name));

    /* Update attributes stored on disk */
    db5_standardize_avs(&cinfo->avs);
    db5_update_attributes(gdp, &cinfo->avs, cinfo->wdbp->dbip);

    /* Free the strings */
    bu_vls_free(&global);
    bu_vls_free(&xform_mode);
    bu_vls_free(&region_counter);
    bu_vls_free(&min_luminance);
    bu_vls_free(&max_chord);
    bu_vls_free(&min_angle);
    bu_vls_free(&elim_small);
    bu_vls_free(&min_hole);
    bu_vls_free(&min_chamfer);
    bu_vls_free(&min_round);
    bu_vls_free(&facets_only);
    bu_vls_free(&check_solidity);
    bu_vls_free(&create_boxes);
    bu_vls_free(&write_normals);
}


/**
 * Routine to output the top-level object that is currently displayed in Creo.
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
            creo_log(cinfo, MSG_MODEL, "Top-level object \"%s\" is not a PART or an ASSEMBLY, skipping...", name);
            return;
    }

    /* Make a final top-level comb based on the file name to hold the orientation matrix */
    struct bu_vls *comb_name;
    struct bu_vls top_name = BU_VLS_INIT_ZERO;
    struct wmember wcomb;
    struct directory *dp;
    BU_LIST_INIT(&wcomb.l);

    mat_t xform;

    if (cinfo->xform_mode == XFORM_Y_TO_Z)
        bn_decode_mat(xform, "0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1");   /* y to z */
    else if (cinfo->xform_mode == XFORM_X_TO_Z)
        bn_decode_mat(xform, "0 1 0 0 0 0 1 0 1 0 0 0 0 0 0 1");   /* x to z */
    else
        bn_decode_mat(xform, "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1");   /*  none  */

    comb_name = get_brlcad_name(cinfo, wname, NULL, N_ASSEM);
    dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(comb_name), LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
        comb_name = get_brlcad_name(cinfo, wname, NULL, N_REGION);

    /* Retain combination name */
    bu_vls_sprintf(cinfo->comb_name, "%s", bu_vls_addr(comb_name));

    (void)mk_addmember(bu_vls_addr(comb_name), &(wcomb.l), xform, WMOP_UNION);

    /* Guarantee we have a non-colliding top-level name */
    bu_vls_sprintf(&top_name, "all");
    tdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(&top_name), LOOKUP_QUIET);
    if (tdp != RT_DIR_NULL) {
        bu_vls_sprintf(&top_name, "all-1.g");
        long count = 0;
        while ((tdp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(&top_name), LOOKUP_QUIET)) != RT_DIR_NULL) {
            (void)bu_vls_incr(&top_name, NULL, "0:0:0:0:-", NULL, NULL);
            count++;
            if (count >= MAX_UNIQUE_NAMES) {
                creo_log(cinfo, MSG_NAME, "Failure for \"%s\" at top-level generation\n",
                                           bu_vls_cstr(cinfo->out_fname));
                break;
            }
        }
    }

    if (tdp == RT_DIR_NULL) {
        mk_lcomb(cinfo->wdbp, bu_vls_addr(&top_name), &wcomb, 0, NULL, NULL, NULL, 0);
        output_settings(cinfo);
    }

    bu_vls_free(&top_name);
}


/* Collect user-specified conversion settings */
extern "C" void
doit(char *UNUSED(dialog), char *UNUSED(compnent), ProAppData UNUSED(appdata))
{
    ProError   err = PRO_TK_GENERAL_ERROR;
    ProMdl     model;
    ProMdlType mtype;
    ProLine    tmp_line = {'\0'};

    wchar_t *tmp_str;

    int user_log_type;

    char linestr[71];
    memset(linestr, '-', 70);
    linestr[70] = '\0';

    char padstr[18];
    memset(padstr, ' ', 17);
    padstr[17] = '\0';

    int64_t start;
    int     time_hr, time_min;
    double  elapsed, time_sec;

    /* This replaces the global variables used in the original Creo converter */
    struct creo_conv_info *cinfo = new creo_conv_info;
    creo_conv_info_init(cinfo);

    /* Set-up log file mode */
    {
        char log_mode[32];
        int  n_logger_names;
        char **logger_names;

        err = ProUIRadiogroupSelectednamesGet("creo_brl", "log_file_type_rg", &n_logger_names, &logger_names);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to get radio button choice: \"log file type\"");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        sprintf(log_mode,"%s", logger_names[0]);
        ProStringarrayFree(logger_names, n_logger_names);

        if (BU_STR_EQUAL("Failure", log_mode))
            cinfo->curr_log_type = LOGGER_TYPE_FAILURE;
        else if (BU_STR_EQUAL("Success", log_mode))
            cinfo->curr_log_type = LOGGER_TYPE_SUCCESS;
        else if (BU_STR_EQUAL("Failure/Success", log_mode))
            cinfo->curr_log_type = LOGGER_TYPE_FAILURE_OR_SUCCESS;
        else if (BU_STR_EQUAL("All/(Debug)", log_mode))
            cinfo->curr_log_type = LOGGER_TYPE_ALL;
        else
            cinfo->curr_log_type = LOGGER_TYPE_NONE;

        /* Retain the log mode */
        bu_vls_sprintf(cinfo->log_mode, "%s", log_mode);
    }

    /* Save user-specified logger type */
    user_log_type = cinfo->curr_log_type;
    cinfo->curr_log_type = LOGGER_TYPE_ALL;

    /* Set up log file */
    {
        char log_fname[MAXPATHLEN];

        /* Get the name of the log file */
        err = ProUIInputpanelValueGet("creo_brl", "log_fname", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to get value: \"Process log file name\"");
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
                creo_log(NULL, MSG_STATUS, "FAILURE: Unable to open log file \"%s\"", log_fname);
                creo_conv_info_free(cinfo);
                ProUIDialogDestroy("creo_brl");
                delete cinfo;
                return;
            }
        } else
            cinfo->fplog = (FILE *)NULL;

        /* Retain the log filename */
        bu_vls_sprintf(cinfo->log_fname, "%s", log_fname);
    }

    /* Start the log file */
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, " Creo to BRL-CAD Converter v20230614");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Set up the output file */
    {
        char out_fname[MAXPATHLEN];

        /* Get the name of the output file */
        err = ProUIInputpanelValueGet("creo_brl", "out_fname", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Output file name\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Output file name\"\n");
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
                creo_log(NULL,  MSG_STATUS, "WARNING: Updating a pre-existing file \"%s\"",
                                                      out_fname);
                creo_log(cinfo, MSG_WARN,            "Updating a pre-existing file \"%s\"\n",
                                                      out_fname);
            } else {
                creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to open output file \"%s\"",
                                                      out_fname);
                creo_log(cinfo, MSG_FAIL,            "Unable to open output file \"%s\"\n",
                                                      out_fname);
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
                creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to open file \"%s\"",
                                                      out_fname);
                creo_log(cinfo, MSG_FAIL,            "Unable to open file \"%s\"\n",
                                                      out_fname);
                creo_conv_info_free(cinfo);
                ProUIDialogDestroy("creo_brl");
                delete cinfo;
                return;
            }
        }

        /* Retain the output filename */
        bu_vls_sprintf(cinfo->out_fname, "%s", out_fname);
    }

    /* Set up the material file */
    {
        char mtl_fname[MAXPATHLEN];

        /* Get the name of the material file */
        err = ProUIInputpanelValueGet("creo_brl", "mtl_fname", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Material file name\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Material file name\"\n");
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
                    creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to open material file \"%s\"",
                                                          mtl_fname);
                    creo_log(cinfo, MSG_FAIL,            "Unable to open material file \"%s\"\n",
                                                          mtl_fname);
                    creo_conv_info_free(cinfo);
                    ProUIDialogDestroy("creo_brl");
                    delete cinfo;
                    return;
                } else 
                    cinfo->mtl_rec = get_mtl_input(cinfo->fpmtl, &(cinfo->mtl_str[0][0]) , &(cinfo->mtl_id[0]), &(cinfo->mtl_los[0]));
            } else
                cinfo->fpmtl = (FILE *)NULL;
        }

        /* Retain the material filename */
        bu_vls_sprintf(cinfo->mtl_fname, "%s", mtl_fname);
    }

    /* Begin echoing the input summary */
    creo_log(cinfo, MSG_PLAIN, "#                  Output file name: \"%s\"\n", bu_vls_cstr(cinfo->out_fname));
    creo_log(cinfo, MSG_PLAIN, "#              Process log criteria: \"%s\"\n", bu_vls_cstr(cinfo->log_mode));
    creo_log(cinfo, MSG_PLAIN, "#             Process log file name: \"%s\"\n", bu_vls_cstr(cinfo->log_fname));
    creo_log(cinfo, MSG_PLAIN, "#                Material file name: \"%s\"\n", bu_vls_cstr(cinfo->mtl_fname));

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "           Creo Parameters          ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Read user-supplied list of model parameters for object names */
    {
        /* Get string from dialog */
        char param_rename[MAXPATHLEN];
        wchar_t *w_param_rename;
        err = ProUIInputpanelValueGet("creo_brl", "param_rename", &w_param_rename);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value(s): \"Create object names\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value(s): \"Create object names\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        ProWstringToString(param_rename, w_param_rename);
        (void)ProWstringFree(w_param_rename);

        if (strlen(param_rename) > 0) {
            parse_param_list(cinfo, param_rename, NAME_PARAMS);
            creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
        } else
            creo_log(cinfo, MSG_PLAIN, "#              Creates object names: \"%s\"\n", param_rename);

        /* Retain the list of renaming parameters */
        bu_vls_sprintf(cinfo->param_rename, "%s", param_rename);
    }

    /* Read user-supplied list of model attributes for conversion */
    {
        /* Get string from dialog */
        char param_save[MAXPATHLEN];
        wchar_t *w_param_save;
        err = ProUIInputpanelValueGet("creo_brl", "param_save", &w_param_save);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value(s): \"Preserved attributes\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value(s): \"Preserved attributes\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        ProWstringToString(param_save, w_param_save);
        (void)ProWstringFree(w_param_save);

        if (strlen(param_save) > 0)
            parse_param_list(cinfo, param_save, ATTR_PARAMS);
        else
            creo_log(cinfo, MSG_PLAIN, "#               Preserved attribute: \"%s\"\n", param_save);

        /* Retain the list of preserved parameters */
        bu_vls_sprintf(cinfo->param_save, "%s", param_save);
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
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get radio button choice: \"coordinate transformation\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get radio button choice: \"coordinate transformation\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        }

        sprintf(xform_str,"%s", xform_names[0]);
        ProStringarrayFree(xform_names, n_xform_names);

        if (BU_STR_EQUAL("y -> z", xform_str))
            cinfo->xform_mode = XFORM_Y_TO_Z;
        else if (BU_STR_EQUAL("x -> z", xform_str))
            cinfo->xform_mode = XFORM_X_TO_Z;
        else
            cinfo->xform_mode = XFORM_NONE;

        creo_log(cinfo, MSG_PLAIN, "#         Coordinate transformation: \"%s\"\n", xform_str);
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "          Process Controls          ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Get initial region counter */
    err = ProUIInputpanelValueGet("creo_brl", "region_counter", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Initial region counter\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Initial region counter\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->region_counter = abs((int)wstr_to_long(cinfo, tmp_str));
        creo_log(cinfo, MSG_PLAIN, "#            Initial region counter: %d\n", cinfo->region_counter);
        (void)ProWstringFree(tmp_str);
    }

    /* Initialize the current region identifier */
    cinfo->curr_reg_id = cinfo->region_counter;

    /* Get minimum luminance threshold */
    err = ProUIInputpanelValueGet("creo_brl", "min_luminance", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Min luminance\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Min luminance\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        int lmin = (int)wstr_to_long(cinfo, tmp_str);
        creo_log(cinfo, MSG_PLAIN, "#                  Min luminance, %s: %d\n", "%", lmin);
        cinfo->min_luminance = std::min<int>(std::max<int>(lmin,0),100);
        (void)ProWstringFree(tmp_str);
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "        Tessellation Controls       ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Get max chord height */
    err = ProUIInputpanelValueGet("creo_brl", "max_chord", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Max chord\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Max chord\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        cinfo->max_chord = wstr_to_double(cinfo, tmp_str);
        creo_log(cinfo, MSG_PLAIN, "#               Max chord height, %s: %8.6f\n", "%", cinfo->max_chord);
        (void)ProWstringFree(tmp_str);
    }

    /* Get the min angle control */
    err = ProUIInputpanelValueGet("creo_brl", "min_angle", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Min angle\"");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Min angle\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        return;
    } else {
        double min_angle = wstr_to_double(cinfo, tmp_str);
        if (min_angle < 0.001)
            cinfo->min_angle = 0.001;
        else
            cinfo->min_angle = min_angle;
        creo_log(cinfo, MSG_PLAIN, "#            Min angle control, rad: %8.6f\n", cinfo->min_angle);
        (void)ProWstringFree(tmp_str);
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "       Small Feature Controls       ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Check if user wants to eliminate small features */
    err = ProUICheckbuttonGetState("creo_brl", "elim_small", &cinfo->elim_small);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"ignore minimum sizes\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"ignore minimum sizes\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    } else
        creo_log(cinfo, MSG_PLAIN, "#              Ignore minimum sizes: %s\n", cinfo->elim_small ? "ON" : "OFF");

    if (cinfo->elim_small) {
        /* Get the minimum hole diameter */
        err = ProUIInputpanelValueGet("creo_brl", "min_hole", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Hole diameter\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Hole diameter\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        } else {
            cinfo->min_hole = abs(wstr_to_double(cinfo, tmp_str));
            creo_log(cinfo, MSG_PLAIN, "#                 Hole diameter, mm: %8.6f\n", cinfo->min_hole);
            (void)ProWstringFree(tmp_str);
        }

        /* Get the minimum chamfer dimension */
        err = ProUIInputpanelValueGet("creo_brl", "min_chamfer", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Chamfer dimension\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Chamfer dimension\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        } else {
            cinfo->min_chamfer = abs(wstr_to_double(cinfo, tmp_str));
            creo_log(cinfo, MSG_PLAIN, "#             Chamfer dimension, mm: %8.6f\n", cinfo->min_chamfer);;
            (void)ProWstringFree(tmp_str);
        }

        /* Get the minimum blend/round radius */
        err = ProUIInputpanelValueGet("creo_brl", "min_round", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Blend radius\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Blend radius\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy("creo_brl");
            delete cinfo;
            return;
        } else {
            cinfo->min_round = abs(wstr_to_double(cinfo, tmp_str));
            creo_log(cinfo, MSG_PLAIN, "#                  Blend radius, mm: %8.6f\n", cinfo->min_round);
            (void)ProWstringFree(tmp_str);
        }

    } else {
        cinfo->min_hole    = 0.0;
        cinfo->min_round   = 0.0;
        cinfo->min_chamfer = 0.0;
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "          Surface Controls          ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Check if user wants to do any CSG */
    err = ProUICheckbuttonGetState("creo_brl", "facets_only", &cinfo->facets_only);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"facets only\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"facets only\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    } else 
        creo_log(cinfo, MSG_PLAIN, "#     Facetize everything, (no CSG): %s\n", cinfo->facets_only ? "ON" : "OFF");

    /* Check if user wants to test solidity */
    err = ProUICheckbuttonGetState("creo_brl", "check_solidity", &cinfo->check_solidity);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"reject failed BoTs\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"reject failed BoTs\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    } else {
         /* Disable solidity check */
        cinfo->check_solidity = PRO_B_FALSE;
        creo_log(cinfo, MSG_PLAIN, "#    Reject BoTs that fail solidity: %s\n", "Disabled");
    }

    /* Check if user wants to use bounding boxes */
    err = ProUICheckbuttonGetState("creo_brl", "create_boxes", &cinfo->create_boxes);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"bounding box for failed part\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"bounding box for failed part\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    } else
        creo_log(cinfo, MSG_PLAIN, "# Bounding box replaces failed part: %s\n", cinfo->create_boxes ? "ON" : "OFF");

    /* Check if user wants surface normals in the BOT's */
    err = ProUICheckbuttonGetState("creo_brl", "write_normals", &cinfo->write_normals);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"write surface normals\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"write surface normals\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy("creo_brl");
        delete cinfo;
        return;
    } else {
        /* Disable write normals */
        cinfo->write_normals = PRO_B_FALSE;
        creo_log(cinfo, MSG_PLAIN, "#             Write surface normals: %s\n", "Disabled");
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Report status of material file */
    if (bu_vls_strlen(cinfo->mtl_fname) > 0) {
        if (cinfo->fpmtl == NULL)
            creo_log(cinfo, MSG_MATL,  "Unknown material translation file \"%s\"\n",
                                        bu_vls_cstr(cinfo->mtl_fname));
        else if (cinfo->mtl_rec > 0) {
            creo_log(cinfo, MSG_MATL,  "Found material translation file \"%s\"\n",
                                        bu_vls_cstr(cinfo->mtl_fname));
            creo_log(cinfo, MSG_MATL,  "Found %d valid material translation entries\n",
                                        cinfo->mtl_rec);
            creo_log(cinfo, MSG_MATL,  "==========================================================\n");
            creo_log(cinfo, MSG_MATL,  "   n          ptc_material_name         material_id  los\n");
            creo_log(cinfo, MSG_MATL,  "----------------------------------------------------------\n");
            for (int n = 0; n < cinfo->mtl_rec; n++)
                creo_log(cinfo, MSG_MATL, " %3d  %-32s      %3d      %3d\n",
                                           n+1, cinfo->mtl_str[n], cinfo->mtl_id[n], cinfo->mtl_los[n]);
            creo_log(cinfo, MSG_MATL,  " ----------------------------------------------------------\n");
        } else
            creo_log(cinfo, MSG_FAIL, "File \"%s\" has no valid material translation entries\n",
                     bu_vls_cstr(cinfo->mtl_fname));
    } else {
        creo_log(cinfo, MSG_MATL,     "No translation file was specified\n");
        cinfo->mtl_rec = -1;
    }

    /* Input summary now complete, restore user-specified logger type */
    cinfo->curr_log_type = user_log_type;

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

    /* Extract top-level unit conversion to mm */
    if (creo_conv_to_mm(&(cinfo->main_to_mm), model) != PRO_TK_NO_ERROR) {
        cinfo->main_to_mm = 1.0;
        creo_log(cinfo, MSG_MODEL, "Top-level model fails unit conversion\n");
    }

    /* Adjust tolerance for top-level units using minimum edge distance */
    cinfo->local_tol    = cinfo->min_edge / cinfo->main_to_mm;
    cinfo->local_tol_sq = cinfo->local_tol * cinfo->local_tol;

    start = bu_gettime();

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
    cinfo->curr_log_type = LOGGER_TYPE_ALL;

    if (cinfo->warn_feature_resume) {
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

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Bin the elapsed run time for display as:  hh:mm:ss.sss */
    elapsed  = (double)(bu_gettime() - start)/1000000.0;
    time_hr  = (int)(elapsed)/3600;
    time_min = ((int)(elapsed)-3600*time_hr)/60;
    time_sec = elapsed - 3600*time_hr - 60*time_min;

    creo_log(cinfo, MSG_PLAIN,     "#                      Elapsed time: %02d:%02d:%06.3f\n",
                                                           time_hr, time_min, time_sec);

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

    creo_conv_info_free(cinfo);
    delete cinfo;
    return;
}


/* Activates small feature settings */
extern "C" void
elim_small_activate(char *dialog_name, char *button_name, ProAppData UNUSED(data))
{
    ProBoolean state;

    if (ProUICheckbuttonGetState(dialog_name, button_name, &state) != PRO_TK_NO_ERROR) {
        creo_log(NULL, MSG_STATUS, "FAILURE: Unable to activate check buttons: \"Eliminate Small Features\"");
        return;
    }

    if (state) {
        if (ProUIInputpanelEditable(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to activate: \"Hole diameter\"");
            return;
        }
        if (ProUIInputpanelEditable(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to activate: \"Chamfer dimension\"");
            return;
        }
        if (ProUIInputpanelEditable(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to activate: \"Blend radius\"" );
            return;
        }
    } else {
        if (ProUIInputpanelReadOnly(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to de-activate: \"Hole diameter\"");
            return;
        }
        if (ProUIInputpanelReadOnly(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to de-activate: \"Chamfer dimension\"");
            return;
        }
        if (ProUIInputpanelReadOnly(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to de-activate: \"Blend radius\"");
            return;
        }
    }
}


/* Exit the converter dialog */
extern "C" void
do_quit(char *UNUSED(dialog), char *UNUSED(compnent), ProAppData UNUSED(appdata))
{
    ProUIDialogDestroy("creo_brl");
}


/* Driver routine for converting Creo to BRL-CAD */
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
    ProUIInputpanelMaxlenSet("creo_brl", "out_fname",    MAXPATHLEN - 1);
    ProUIInputpanelMaxlenSet("creo_brl", "log_fname",    MAXPATHLEN - 1);
    ProUIInputpanelMaxlenSet("creo_brl", "mtl_fname",    MAXPATHLEN - 1);

    /* Parameter lists may not get longer than the default... */
    ProUIInputpanelMaxlenSet("creo_brl", "param_rename", MAXPATHLEN - 1);
    ProUIInputpanelMaxlenSet("creo_brl", "param_save",   MAXPATHLEN - 1);

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

    if (ProUIDialogActivate("creo_brl", &err) != PRO_TK_NO_ERROR)
        bu_vls_printf(&vls, "FAILURE: Unexpected error in creo-brl dialog returned %d\n", err);

    print_msg:
    if (bu_vls_strlen(&vls) > 0) {
        creo_log(NULL, MSG_PLAIN, bu_vls_addr(&vls));
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
