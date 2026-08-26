/**
 *                    M A I N . C P P
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
 * @file main.cpp
 */

/*                                                                              */
/* Synopsis of Main Routines:                                                   */
/*                                                                              */
/* activate_export_stl       Activates export to STL setting                    */
/* activate_small_feats      Activates small feature settings                   */
/* creo_brl                  Driver routine for converting Creo to BRL-CAD      */
/* creo_brl_access           Enable Creo menu acsess                            */
/* creo_conv_info_free       Clear the conversion information container         */
/* creo_conv_info_init       Initialize the conversion information container    */
/* do_quit                   Exit the converter dialog                          */
/* doit                      Collect user-specified conversion settings         */
/* objects_gather            Build up the sets of assemblies and parts          */
/* output_assems             Output all the assemblies                          */
/* output_parts              Output all the parts                               */
/* output_settings           Output converter settings to the current (.g) file */
/* output_top_level_object   Output the top-level object                        */
/* creo_brl_core_initialize  Runtime-loaded Creo core initialization            */
/* creo_brl_core_terminate   Runtime-loaded Creo core termination               */
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

    BU_GET(cinfo->stl_fname, struct bu_vls);          /* STL file name        */
    bu_vls_init(cinfo->stl_fname);

    BU_GET(cinfo->param_rename, struct bu_vls);       /* renaming parameters  */
    bu_vls_init(cinfo->param_rename);

    BU_GET(cinfo->param_save, struct bu_vls);         /* preserved parameters */
    bu_vls_init(cinfo->param_save);

    BU_GET(cinfo->curr_name, struct bu_vls);          /* current part name    */
    bu_vls_init(cinfo->curr_name);

    BU_GET(cinfo->comb_name, struct bu_vls);          /* combination name     */
    bu_vls_init(cinfo->comb_name);

    BU_GET(cinfo->main_name, struct bu_vls);          /* top-level model name */
    bu_vls_init(cinfo->main_name);

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
    memset(cinfo->mtl_ids, '\0', sizeof(cinfo->mtl_ids));
    memset(cinfo->mtl_los, '\0', sizeof(cinfo->mtl_los));
    cinfo->mtl_ptr = -1;
    cinfo->mtl_rec = -1;

    cinfo->fpstl = (FILE *)NULL;                      /* STL file settings */

    cinfo->xform_mode = XFORM_NONE;                   /* xform mode */
    cinfo->chord_mode = RELATIVE_CHORD;               /* chord mode */

    cinfo->dbip = NULL;
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
    cinfo->min_edge     = 0.000254;
    cinfo->max_facets   = 100000;
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
    cinfo->rej_count    = 0;

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

    bu_vls_free(cinfo->stl_fname);
    BU_PUT(cinfo->stl_fname, struct bu_vls);

    bu_vls_free(cinfo->param_rename);
    BU_PUT(cinfo->param_rename, struct bu_vls);

    bu_vls_free(cinfo->param_save);
    BU_PUT(cinfo->param_save, struct bu_vls);

    bu_vls_free(cinfo->curr_name);
    BU_PUT(cinfo->curr_name, struct bu_vls);

    bu_vls_free(cinfo->comb_name);
    BU_PUT(cinfo->comb_name, struct bu_vls);

    bu_vls_free(cinfo->main_name);
    BU_PUT(cinfo->main_name, struct bu_vls);

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
    memset(cinfo->mtl_ids  , '\0', sizeof(cinfo->mtl_ids  ));
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
    if (cinfo->fpstl)
        fclose(cinfo->fpstl);
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
         *  b) has the same Creo version stamp as the part in the current Creo file
         *  c) then we don't need to re-export it to the .g file
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
                if (rt_db_get_internal(&in, rdp, cinfo->wdbp->dbip, NULL) >= 0) {
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
                db_update_nref(cinfo->wdbp->dbip);
            }
        }

        cinfo->final_part = (prt_count == cinfo->parts->size() - 1) ? 1 : 0;

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
         *  b) has the same Creo version stamp as the part in the current Creo file
         *  c) then we don't need to re-export it to the .g file
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
                db_update_nref(cinfo->wdbp->dbip);
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
        creo_log(cinfo, MSG_FILE, "Failed to get file type for \"%s\"\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* Get component model handle */
    err = ProAsmcompMdlGet(feat, &model);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_MODEL, "Failed to get handle for \"%s\"\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* If this is a skeleton, we're done */
    ProMdlIsSkeleton(model, &is_skel);
    if (is_skel) {
        creo_log(cinfo, MSG_MODEL, "\"%s\" is a \"skeleton\", skipping...\n", name);
        return PRO_TK_NO_ERROR;
    }

    /* Get model type (only a part or assembly should make it here) */
    err = ProMdlTypeGet(model, &mtype);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_MODEL, "Failed to get type for \"%s\"\n", name);
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
    }

    return PRO_TK_NO_ERROR;
}


/* Output converter settings to the current (.g) file */
extern "C" void
output_settings(struct creo_conv_info *cinfo)
{
    struct directory *gdp;
    struct bu_vls username       = BU_VLS_INIT_ZERO;
    struct bu_vls check_solidity = BU_VLS_INIT_ZERO;
    struct bu_vls chord_mode     = BU_VLS_INIT_ZERO;
    struct bu_vls create_boxes   = BU_VLS_INIT_ZERO;
    struct bu_vls elim_small     = BU_VLS_INIT_ZERO;
    struct bu_vls export_stl     = BU_VLS_INIT_ZERO;
    struct bu_vls facets_only    = BU_VLS_INIT_ZERO;
    struct bu_vls importer       = BU_VLS_INIT_ZERO;
    struct bu_vls length_units   = BU_VLS_INIT_ZERO;
    struct bu_vls max_chord      = BU_VLS_INIT_ZERO;
    struct bu_vls min_angle      = BU_VLS_INIT_ZERO;
    struct bu_vls min_chamfer    = BU_VLS_INIT_ZERO;
    struct bu_vls min_hole       = BU_VLS_INIT_ZERO;
    struct bu_vls min_luminance  = BU_VLS_INIT_ZERO;
    struct bu_vls min_round      = BU_VLS_INIT_ZERO;
    struct bu_vls region_counter = BU_VLS_INIT_ZERO;
    struct bu_vls write_normals  = BU_VLS_INIT_ZERO;
    struct bu_vls xform_mode     = BU_VLS_INIT_ZERO;

    /* Assumes global exists */
    gdp = db_lookup(cinfo->wdbp->dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET);
    db5_get_attributes(cinfo->wdbp->dbip, &cinfo->avs, gdp);

    /* Prepare the chord mode & max chord */
    if (cinfo->chord_mode) {
        bu_vls_sprintf(&chord_mode, "%s", "percent");
        bu_vls_sprintf(&max_chord,  "%g %s", cinfo->max_chord, "%");
    } else {
        bu_vls_sprintf(&chord_mode, "%s", "millimeter");
        bu_vls_sprintf(&max_chord,  "%g %s", cinfo->max_chord, "mm");
    }

    /* Prepare the transform mode */
    if (cinfo->xform_mode == XFORM_Y_TO_Z)
        bu_vls_sprintf(&xform_mode, "%s", "y_to_z");
    else if (cinfo->xform_mode == XFORM_X_TO_Z)
        bu_vls_sprintf(&xform_mode, "%s", "x_to_z");
    else
        bu_vls_sprintf(&xform_mode, "%s", "none");

    bu_vls_sprintf(&username,       "%s", get_username());
    bu_vls_sprintf(&check_solidity, "%s", cinfo->check_solidity ? "on" : "off");
    bu_vls_sprintf(&create_boxes,   "%s", cinfo->create_boxes   ? "on" : "off");
    bu_vls_sprintf(&elim_small,     "%s", cinfo->elim_small     ? "on" : "off");
    bu_vls_sprintf(&export_stl,     "%s", "disabled");
/*  bu_vls_sprintf(&export_stl,     "%s", cinfo->export_stl     ? "on" : "off"); */
    bu_vls_sprintf(&facets_only,    "%s", cinfo->facets_only    ? "on" : "off");
    bu_vls_sprintf(&importer,       "%s", "creo-g");
    bu_vls_sprintf(&length_units,   "%s", "millimeters");
    bu_vls_sprintf(&min_angle,      "%g", cinfo->min_angle);
    bu_vls_sprintf(&min_chamfer,    "%g", cinfo->min_chamfer);
    bu_vls_sprintf(&min_hole,       "%g", cinfo->min_hole);
    bu_vls_sprintf(&min_luminance,  "%d", cinfo->min_luminance);
    bu_vls_sprintf(&min_round,      "%g", cinfo->min_round);
    bu_vls_sprintf(&region_counter, "%d", cinfo->region_counter);
    bu_vls_sprintf(&write_normals,  "%s", "disabled");
/*  bu_vls_sprintf(&write_normals,  "%s", cinfo->write_normals  ? "on" : "off"); */

    /* Create the attributes (LIFO) */
    bu_avs_add(&cinfo->avs, "write_surface_normals",     bu_vls_addr(&write_normals));
    bu_avs_add(&cinfo->avs, "box_replaces_failed_part",  bu_vls_addr(&create_boxes));
    bu_avs_add(&cinfo->avs, "reject_failed_bots",        bu_vls_addr(&check_solidity));
    bu_avs_add(&cinfo->avs, "export_facets_to_stl",      bu_vls_addr(&export_stl));
    bu_avs_add(&cinfo->avs, "facetize_everything",       bu_vls_addr(&facets_only));
    bu_avs_add(&cinfo->avs, "minimum_blend_radius",      bu_vls_addr(&min_round));
    bu_avs_add(&cinfo->avs, "minimum_chamfer_dimension", bu_vls_addr(&min_chamfer));
    bu_avs_add(&cinfo->avs, "minimum_hole_diameter",     bu_vls_addr(&min_hole));
    bu_avs_add(&cinfo->avs, "eliminate_small_features",  bu_vls_addr(&elim_small));
    bu_avs_add(&cinfo->avs, "minimum_angle_control",     bu_vls_addr(&min_angle));
    bu_avs_add(&cinfo->avs, "maximum_chord_height",      bu_vls_addr(&max_chord));
    bu_avs_add(&cinfo->avs, "chord_mode",                bu_vls_addr(&chord_mode));
    bu_avs_add(&cinfo->avs, "minimum_luminance",         bu_vls_addr(&min_luminance));
    bu_avs_add(&cinfo->avs, "initial_region_counter",    bu_vls_addr(&region_counter));
    bu_avs_add(&cinfo->avs, "coordinate_transformation", bu_vls_addr(&xform_mode));
    bu_avs_add(&cinfo->avs, "preserved_attributes",      bu_vls_addr(cinfo->param_save));
    bu_avs_add(&cinfo->avs, "create_object_names",       bu_vls_addr(cinfo->param_rename));
    if (cinfo->export_stl)
        bu_avs_add(&cinfo->avs, "stl_file_name",         bu_vls_addr(cinfo->stl_fname));
    bu_avs_add(&cinfo->avs, "material_file_name",        bu_vls_addr(cinfo->mtl_fname));
    bu_avs_add(&cinfo->avs, "process_log_file_name",     bu_vls_addr(cinfo->log_fname));
    bu_avs_add(&cinfo->avs, "process_log_criteria",      bu_vls_addr(cinfo->log_mode));
    bu_avs_add(&cinfo->avs, "output_file_name",          bu_vls_addr(cinfo->out_fname));
    bu_avs_add(&cinfo->avs, "length_units",              bu_vls_addr(&length_units));
    bu_avs_add(&cinfo->avs, "importer",                  bu_vls_addr(&importer));
    bu_avs_add(&cinfo->avs, "username",                  bu_vls_addr(&username));
    bu_avs_add(&cinfo->avs, "title",                     bu_vls_addr(cinfo->comb_name));

    /* Update attributes stored on disk */
    db5_standardize_avs(&cinfo->avs);
    db5_update_attributes(gdp, &cinfo->avs, cinfo->wdbp->dbip);

    /* Free the strings */
    bu_vls_free(&username);
    bu_vls_free(&check_solidity);
    bu_vls_free(&chord_mode);
    bu_vls_free(&create_boxes);
    bu_vls_free(&elim_small);
    bu_vls_free(&export_stl);
    bu_vls_free(&facets_only);
    bu_vls_free(&importer);
    bu_vls_free(&length_units);
    bu_vls_free(&max_chord);
    bu_vls_free(&min_angle);
    bu_vls_free(&min_chamfer);
    bu_vls_free(&min_hole);
    bu_vls_free(&min_luminance);
    bu_vls_free(&min_round);
    bu_vls_free(&region_counter);
    bu_vls_free(&write_normals);
    bu_vls_free(&xform_mode);
}


/**
 * Routine to output the top-level object that is currently displayed in Creo.
 * This is the real beginning of the processing code - doit collects user
 * settings and calls this function.
 */
extern "C" void
output_top_level_object(struct creo_conv_info *cinfo, ProMdl model, ProMdlType mtype)
{
    ProError unit_err = PRO_TK_GENERAL_ERROR;

    wchar_t  wname[CREO_NAME_MAX];
    char      name[CREO_NAME_MAX];
    wchar_t *wname_saved;

    struct directory *tdp = RT_DIR_NULL;

    /* Get object name */
    if (ProMdlMdlnameGet(model, wname) != PRO_TK_NO_ERROR)
        return;

    /* Save name */
    wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "ptc_name");
    wcsncpy(wname_saved, wname, wcslen(wname)+1);

    ProWstringToString(name, wname);
    lower_case(name);

    /* Establish top-level model name */
    bu_vls_sprintf(cinfo->main_name, "%s", name);

    /* Establish top-level model as current model */
    cinfo->curr_model = model;

    /* Extract top-level unit conversion to mm */
    if (creo_conv_to_mm(&(cinfo->main_to_mm), model) != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_MODEL, "Top-level model fails unit conversion\n");
        cinfo->main_to_mm = 1.0;
    }

    /* Adjust tolerance for top-level units using minimum edge distance */
    cinfo->local_tol    = cinfo->min_edge / cinfo->main_to_mm;
    cinfo->local_tol_sq = cinfo->local_tol * cinfo->local_tol;

    if (mtype == PRO_MDL_ASSEMBLY) {
        /* Report top-level model units */
        unit_err = creo_model_units(cinfo);
        if (unit_err == PRO_TK_NO_ERROR) {
            creo_log(cinfo, MSG_UNITS, "================================\n");
            creo_log(cinfo, MSG_UNITS, "      Top-Level Model Units     \n");
            creo_log(cinfo, MSG_UNITS, "--------------------------------\n");
            creo_log(cinfo, MSG_UNITS, "         Name = %s\n", name);
            creo_log(cinfo, MSG_UNITS, "       System = %s\n", bu_vls_addr(cinfo->unitsys));
            creo_log(cinfo, MSG_UNITS, "        Angle = %s\n", bu_vls_addr(cinfo->aunits));
            creo_log(cinfo, MSG_UNITS, "        Force = %s\n", bu_vls_addr(cinfo->funits));
            creo_log(cinfo, MSG_UNITS, "         Mass = %s\n", bu_vls_addr(cinfo->munits));
            creo_log(cinfo, MSG_UNITS, "       Length = %s\n", bu_vls_addr(cinfo->lunits));
            creo_log(cinfo, MSG_UNITS, "         Time = %s\n", bu_vls_addr(cinfo->tunits));
            creo_log(cinfo, MSG_UNITS, "--------------------------------\n");
        } else
            creo_log(cinfo, MSG_UNITS, "\"%s\" failed unit inquiry\n", name);
    }

    /*
     * There are two possibilities - either we have a hierarchy,
     * in which case we need to walk it and collect the objects
     * to process, or we have a single part which we can process
     * directly.
     */

    switch (mtype) {
        case PRO_MDL_PART:
            /* One part only */
            creo_log(cinfo, MSG_MODEL, "Top-level model \"%s\" is a PART\n", name);
            cinfo->parts->insert(wname_saved);
            output_parts(cinfo);
            break;
        case PRO_MDL_ASSEMBLY:
            creo_log(cinfo, MSG_MODEL, "Top-level model \"%s\" is an ASSEMBLY\n", name);
            /* Walk the hierarchy and process all necessary assemblies and parts */
            cinfo->assems->insert(wname_saved);
            ProSolidFeatVisit(ProMdlToPart(model), objects_gather, (ProFeatureFilterAction)component_filter, (ProAppData)cinfo);
            output_parts(cinfo);
            find_empty_assemblies(cinfo);
            (void)ProWindowRefresh(PRO_VALUE_UNUSED);
            output_assems(cinfo);
            break;
        default:
            creo_log(cinfo, MSG_MODEL, "Top-level model \"%s\" is not a PART or an ASSEMBLY, skipping...", name);
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
                                           bu_vls_addr(cinfo->out_fname));
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
extern "C" __declspec(dllexport) void
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

        err = ProUIRadiogroupSelectednamesGet(CREO_UI_NAME, "log_file_type", &n_logger_names, &logger_names);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to get radio button choice: \"log file type\"");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
            delete cinfo;
            return;
        }

        sprintf(log_mode,"%s", logger_names[0]);
        ProStringarrayFree(logger_names, n_logger_names);

        if (BU_STR_EQUAL("failure", log_mode))
            cinfo->curr_log_type = LOGGER_TYPE_FAILURE;
        else if (BU_STR_EQUAL("success", log_mode))
            cinfo->curr_log_type = LOGGER_TYPE_SUCCESS;
        else if (BU_STR_EQUAL("failure/success", log_mode))
            cinfo->curr_log_type = LOGGER_TYPE_FAILURE_OR_SUCCESS;
        else if (BU_STR_EQUAL("all/(debug)", log_mode))
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
        err = ProUIInputpanelValueGet(CREO_UI_NAME, "log_fname", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL, MSG_STATUS, "FAILURE: Unable to get value: \"Process log file name\"");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
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
                ProUIDialogDestroy(CREO_UI_NAME);
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
    creo_log(cinfo, MSG_PLAIN, " Creo to BRL-CAD Converter v20240328");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Set up the output file */
    {
        char out_fname[MAXPATHLEN];

        /* Get the name of the output file */
        err = ProUIInputpanelValueGet(CREO_UI_NAME, "out_fname", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Output file name\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Output file name\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
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
                ProUIDialogDestroy(CREO_UI_NAME);
                delete cinfo;
                return;
            }
        } else {
            /* Create new file */
            if ((cinfo->dbip = db_create(out_fname, BRLCAD_DB_FORMAT_LATEST)) != DBI_NULL)
                cinfo->wdbp = wdb_dbopen(cinfo->dbip, RT_WDB_TYPE_DB_DISK);
            else {
                creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to open file \"%s\"",
                                                      out_fname);
                creo_log(cinfo, MSG_FAIL,            "Unable to open file \"%s\"\n",
                                                      out_fname);
                creo_conv_info_free(cinfo);
                ProUIDialogDestroy(CREO_UI_NAME);
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
        err = ProUIInputpanelValueGet(CREO_UI_NAME, "mtl_fname", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Material file name\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Material file name\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
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
                    ProUIDialogDestroy(CREO_UI_NAME);
                    delete cinfo;
                    return;
                } else {
                    cinfo->mtl_rec = get_mtl_input(cinfo->fpmtl, &(cinfo->mtl_str[0][0]),
                                                                 &(cinfo->mtl_ids[0]),
                                                                 &(cinfo->mtl_los[0]));
                }
            } else
                cinfo->fpmtl = (FILE *)NULL;
        }

        /* Retain the material filename */
        bu_vls_sprintf(cinfo->mtl_fname, "%s", mtl_fname);
    }

    /* Begin echoing the input summary */
    creo_log(cinfo, MSG_PLAIN, "#                  Output file name: \"%s\"\n", bu_vls_addr(cinfo->out_fname));
    creo_log(cinfo, MSG_PLAIN, "#              Process log criteria: \"%s\"\n", bu_vls_addr(cinfo->log_mode));
    creo_log(cinfo, MSG_PLAIN, "#             Process log file name: \"%s\"\n", bu_vls_addr(cinfo->log_fname));
    creo_log(cinfo, MSG_PLAIN, "#                Material file name: \"%s\"\n", bu_vls_addr(cinfo->mtl_fname));

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
        err = ProUIInputpanelValueGet(CREO_UI_NAME, "param_rename", &w_param_rename);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value(s): \"Create object names\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value(s): \"Create object names\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
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
        err = ProUIInputpanelValueGet(CREO_UI_NAME, "param_save", &w_param_save);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value(s): \"Preserved attributes\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value(s): \"Preserved attributes\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
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

        err = ProUIRadiogroupSelectednamesGet(CREO_UI_NAME, "transform", &n_xform_names, &xform_names);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get radio button choice: \"coordinate transformation\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get radio button choice: \"coordinate transformation\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
            delete cinfo;
            return;
        }

        sprintf(xform_str,"%s", xform_names[0]);
        ProStringarrayFree(xform_names, n_xform_names);

        if (BU_STR_EQUAL("y_to_z", xform_str))
            cinfo->xform_mode = XFORM_Y_TO_Z;
        else if (BU_STR_EQUAL("x_to_z", xform_str))
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
    err = ProUIInputpanelValueGet(CREO_UI_NAME, "region_counter", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Initial region counter\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Initial region counter\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        return;
    } else {
        cinfo->region_counter = abs((int)wstr_to_long(cinfo, tmp_str));
        creo_log(cinfo, MSG_PLAIN, "#            Initial region counter: %d\n", cinfo->region_counter);
        (void)ProWstringFree(tmp_str);
    }

    /* Initialize the current region identifier */
    cinfo->curr_reg_id = cinfo->region_counter;

    /* Get minimum luminance threshold */
    err = ProUIInputpanelValueGet(CREO_UI_NAME, "min_luminance", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Min luminance\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Min luminance\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        return;
    } else {
        int lmin = (int)wstr_to_long(cinfo, tmp_str);
        creo_log(cinfo, MSG_PLAIN, "#                  Min luminance, %s: %d\n", "%", lmin);
        cinfo->min_luminance = (lmin < 0) ? 0 : (lmin > 100 ? 100 : lmin);
        (void)ProWstringFree(tmp_str);
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "        Tessellation Controls       ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Set-up chord mode */
    {
        char chord_str[32];
        int  n_chord_modes;
        char **chord_names;

        err = ProUIRadiogroupSelectednamesGet(CREO_UI_NAME, "chord_mode", &n_chord_modes, &chord_names);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get radio button choice: \"chord mode\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get radio button choice: \"chord mode\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
            delete cinfo;
            return;
        }

        sprintf(chord_str,"%s", chord_names[0]);
        ProStringarrayFree(chord_names, n_chord_modes);

        if (BU_STR_EQUAL("percent", chord_str))
            cinfo->chord_mode = RELATIVE_CHORD;
        else
            cinfo->chord_mode = ABSOLUTE_CHORD;

        creo_log(cinfo, MSG_PLAIN, "#                        Chord mode: \"%s\"\n", chord_str);
    }

    /* Get max chord height */
    err = ProUIInputpanelValueGet(CREO_UI_NAME, "max_chord", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Max chord\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Max chord\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        return;
    } else {
        cinfo->max_chord = wstr_to_double(cinfo, tmp_str);
        if (cinfo->chord_mode)
            creo_log(cinfo, MSG_PLAIN, "#                  Max chord height: %8.6f %s\n", cinfo->max_chord, "%");
        else
            creo_log(cinfo, MSG_PLAIN, "#                  Max chord height: %8.6f %s\n", cinfo->max_chord, "mm");
        (void)ProWstringFree(tmp_str);
    }

    /* Get the min angle control */
    err = ProUIInputpanelValueGet(CREO_UI_NAME, "min_angle", &tmp_str);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Min angle\"");
        creo_log(cinfo, MSG_PLAIN,  "FAILURE: Unable to get value: \"Min angle\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        return;
    } else {
        double min_angle = wstr_to_double(cinfo, tmp_str);
        cinfo->min_angle = (min_angle < 0.0) ? 0.0 : (min_angle > 1.0 ? 1.0 : min_angle);
        creo_log(cinfo, MSG_PLAIN, "#                 Min angle control: %8.6f %s\n", cinfo->min_angle, "rad");
        (void)ProWstringFree(tmp_str);
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "       Small Feature Controls       ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Check if user wants to eliminate small features */
    err = ProUICheckbuttonGetState(CREO_UI_NAME, "elim_small", &cinfo->elim_small);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"ignore minimum sizes\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"ignore minimum sizes\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        delete cinfo;
        return;
    } else
        creo_log(cinfo, MSG_PLAIN, "#              Ignore minimum sizes: %s\n", cinfo->elim_small ? "on" : "off");

    if (cinfo->elim_small) {
        /* Get the minimum hole diameter */
        err = ProUIInputpanelValueGet(CREO_UI_NAME, "min_hole", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Hole diameter\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Hole diameter\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
            delete cinfo;
            return;
        } else {
            cinfo->min_hole = abs(wstr_to_double(cinfo, tmp_str));
            creo_log(cinfo, MSG_PLAIN, "#                 Hole diameter, mm: %8.6f\n", cinfo->min_hole);
            (void)ProWstringFree(tmp_str);
        }

        /* Get the minimum chamfer dimension */
        err = ProUIInputpanelValueGet(CREO_UI_NAME, "min_chamfer", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Chamfer dimension\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Chamfer dimension\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
            delete cinfo;
            return;
        } else {
            cinfo->min_chamfer = abs(wstr_to_double(cinfo, tmp_str));
            creo_log(cinfo, MSG_PLAIN, "#             Chamfer dimension, mm: %8.6f\n", cinfo->min_chamfer);;
            (void)ProWstringFree(tmp_str);
        }

        /* Get the minimum blend/round radius */
        err = ProUIInputpanelValueGet(CREO_UI_NAME, "min_round", &tmp_str);
        if (err != PRO_TK_NO_ERROR) {
            creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get value: \"Blend radius\"");
            creo_log(cinfo, MSG_FAIL,            "Unable to get value: \"Blend radius\"\n");
            creo_conv_info_free(cinfo);
            ProUIDialogDestroy(CREO_UI_NAME);
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
    err = ProUICheckbuttonGetState(CREO_UI_NAME, "facets_only", &cinfo->facets_only);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"facets only\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"facets only\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        delete cinfo;
        return;
    } else
        creo_log(cinfo, MSG_PLAIN, "#     Facetize everything, (no CSG): %s\n", cinfo->facets_only ? "on" : "off");

    /* Check if user wants to export facets to STL */
    err = ProUICheckbuttonGetState(CREO_UI_NAME, "export_stl", &cinfo->export_stl);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"Export to STL\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"Export to STL\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        delete cinfo;
        return;
    } else {

        /* Disable export STL */
        cinfo->export_stl = PRO_B_FALSE;
        creo_log(cinfo, MSG_PLAIN, "#         Export facets to STL file: %s\n", "disabled");

        /* Enable export STL
        *   if (!cinfo->facets_only)
        *       cinfo->export_stl = cinfo->facets_only;
        *   creo_log(cinfo, MSG_PLAIN,     "#         Export facets to STL file: %s\n", cinfo->export_stl ? "on" : "off");
        */
    }

    /* Set up the STL file */
    {
        char stl_fname[MAXPATHLEN];

        /* Extract current base output file name */
        struct bu_vls outfn = BU_VLS_INIT_ZERO;
        if (bu_vls_strlen(cinfo->out_fname) > 0) {
            if (bu_path_component(&outfn, bu_vls_cstr(cinfo->out_fname), BU_PATH_BASENAME_EXTLESS))
                bu_vls_printf(&outfn, ".stl");
            else
                bu_vls_printf(&outfn, "%s", "unknown");
        } else
            bu_vls_printf(&outfn, "%s", "unknown");

        /* Initialize the STL filename */
        sprintf(stl_fname,"%s", bu_vls_cstr(&outfn));
        bu_vls_free(&outfn);

        /* Retain the STL filename */
        bu_vls_sprintf(cinfo->stl_fname, "%s", stl_fname);

        if (cinfo->export_stl)
            

        /* Open STL file */
        if (cinfo->export_stl) {
            if ((cinfo->fpstl=fopen(stl_fname, "wb+")) == NULL) {
                creo_log(NULL, MSG_STATUS, "FAILURE: Unable to open STL file \"%s\"", stl_fname);
                creo_conv_info_free(cinfo);
                ProUIDialogDestroy(CREO_UI_NAME);
                delete cinfo;
                return;
            }
            creo_log(cinfo, MSG_PLAIN, "#                     STL file name: \"%s\"\n", bu_vls_addr(cinfo->stl_fname));
        } else
            cinfo->fpstl = (FILE *)NULL;
    }

    /* Check if user wants to test solidity */
    err = ProUICheckbuttonGetState(CREO_UI_NAME, "check_solidity", &cinfo->check_solidity);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"reject failed BoTs\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"reject failed BoTs\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        delete cinfo;
        return;
    } else
        creo_log(cinfo, MSG_PLAIN, "#    Reject BoTs that fail solidity: %s\n", cinfo->check_solidity ? "on" : "off");

    /* Check if user wants to use bounding boxes */
    err = ProUICheckbuttonGetState(CREO_UI_NAME, "create_boxes", &cinfo->create_boxes);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"bounding box for failed part\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"bounding box for failed part\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        delete cinfo;
        return;
    } else
        creo_log(cinfo, MSG_PLAIN, "# Bounding box replaces failed part: %s\n", cinfo->create_boxes ? "on" : "off");

    /* Check if user wants surface normals in the BOT's */
    err = ProUICheckbuttonGetState(CREO_UI_NAME, "write_normals", &cinfo->write_normals);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to get check button state: \"write surface normals\"");
        creo_log(cinfo, MSG_FAIL,            "Unable to get check button state: \"write surface normals\"\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        delete cinfo;
        return;
    } else {
        /* Disable write normals */
        cinfo->write_normals = PRO_B_FALSE;
        creo_log(cinfo, MSG_PLAIN, "#             Write surface normals: %s\n", "disabled");
    }

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    /* Report status of material file */
    if (bu_vls_strlen(cinfo->mtl_fname) > 0) {
        if (cinfo->fpmtl == NULL)
            creo_log(cinfo, MSG_MATL,  "Unknown material translation file \"%s\"\n",
                                        bu_vls_addr(cinfo->mtl_fname));
        else if (cinfo->mtl_rec > 0) {
            creo_log(cinfo, MSG_MATL,  "Found material translation file \"%s\"\n",
                                        bu_vls_addr(cinfo->mtl_fname));
            creo_log(cinfo, MSG_MATL,  "Found %d valid material translation entries\n",
                                        cinfo->mtl_rec);
            creo_log(cinfo, MSG_MATL,  "==========================================================\n");
            creo_log(cinfo, MSG_MATL,  "   n          ptc_material_name         material_id  los\n");
            creo_log(cinfo, MSG_MATL,  "----------------------------------------------------------\n");
            for (int n = 0; n < cinfo->mtl_rec; n++)
                creo_log(cinfo, MSG_MATL, " %3d  %-32s      %3d      %3d\n",
                                          n+1, cinfo->mtl_str[n], cinfo->mtl_ids[n], cinfo->mtl_los[n]);
            creo_log(cinfo, MSG_MATL,  " ----------------------------------------------------------\n");
        } else
            creo_log(cinfo, MSG_FAIL, "File \"%s\" has no valid material translation entries\n",
                                       bu_vls_addr(cinfo->mtl_fname));
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
        ProUIDialogDestroy(CREO_UI_NAME);
        delete cinfo;
        return;
        }

    /* Get top-level model type */
    err = ProMdlTypeGet(model, &mtype);
    if (err == PRO_TK_BAD_INPUTS) {
        creo_log(NULL,  MSG_STATUS, "Unable to get \"type\" for currently displayed Creo model  ");
        creo_log(cinfo, MSG_FAIL,   "Unable to get \"type\" for currently displayed Creo model\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        delete cinfo;
        return;
    }

    /* Retain top-level model and type */
    cinfo->main_model = model;
    cinfo->main_type  = mtype;

    /* Limit scope to parts and assemblies, no drawings, etc. */
    if (mtype != PRO_MDL_ASSEMBLY && mtype != PRO_MDL_PART) {
        creo_log(NULL,  MSG_STATUS, "Current model is not a PART or an ASSEMBLY  ");
        creo_log(cinfo, MSG_FAIL,   "Current model is not a PART or an ASSEMBLY\n");
        creo_conv_info_free(cinfo);
        ProUIDialogDestroy(CREO_UI_NAME);
        delete cinfo;
        return;
    }

    /* Establish start time */
    start = bu_gettime();

    /*
     * Output the top-level object
     * this will recurse through the entire model
     */
    output_top_level_object(cinfo, model, mtype);

    /* Let user know we are done... */
    ProStringToWstring(tmp_line,  "Status: Conversion complete, (Convert/Quit)?");
    (void)ProUILabelTextSet(CREO_UI_NAME, "conv_status", tmp_line);
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
        creo_log(cinfo, MSG_WARN , "Restoration Failure   ");
        creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
        creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
        creo_log(cinfo, MSG_PLAIN,    "%s", bu_vls_addr(&errmsg));
        PopupMsg("WARNING:  Restoration Failure", bu_vls_addr(&errmsg));
        bu_vls_free(&errmsg);
    }

    /* Finish the log file */

    int net_count = (cinfo->prt_count > 0) ? (cinfo->prt_count - cinfo->rej_count) : 0;

    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);
    creo_log(cinfo, MSG_PLAIN,    "#%s",  padstr);
    creo_log(cinfo, MSG_PLAIN, "         Conversion Summary         ");
    creo_log(cinfo, MSG_PLAIN,  "%s#\n",  padstr);
    creo_log(cinfo, MSG_PLAIN, "#%s#\n", linestr);

    creo_log(cinfo, MSG_PLAIN,     "#                       Parts found: %d\n",
                                    cinfo->prt_total);
    creo_log(cinfo, MSG_PLAIN,     "#                   Parts converted: %d\n",
                                    cinfo->prt_count);

    /* Only report these when checking solidity */
    if (cinfo->check_solidity)
        creo_log(cinfo, MSG_PLAIN, "#                    Parts rejected: %d %s\n",
                                    cinfo->rej_count,
                                   "(failed solids)");

    creo_log(cinfo, MSG_PLAIN,     "#                     Part failures: %d\n",
                                    cinfo->prt_total - cinfo->prt_count);

    if (cinfo->prt_total > net_count)
        creo_log(cinfo, MSG_PLAIN, "#                  Conversion ratio: %.1f%s\n",
                                    double(net_count)/double(cinfo->prt_total)*100.0,
                                    "%");
    else
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

    /* Only report these when checking solidity */
    if (cinfo->check_solidity)
        creo_log(cinfo, MSG_STATUS, "Parts rejected = %d %s",
                                     cinfo->rej_count,
                                    "(failed solids)");
    if (cinfo->prt_total > cinfo->prt_count)
        creo_log(cinfo, MSG_STATUS, "Part failures = %d",
                                     cinfo->prt_total - cinfo->prt_count);
    else
        creo_log(cinfo, MSG_STATUS, "No Part failures found");
    if (cinfo->asm_total > cinfo->asm_count)
        creo_log(cinfo, MSG_STATUS, "Assembly failures = %d",
                                     cinfo->asm_total - cinfo->asm_count);
    else
        creo_log(cinfo, MSG_STATUS, "No Assembly failures found");

    creo_conv_info_free(cinfo);
    delete cinfo;
    return;
}


#if defined(CREO_EXEC_PLUGIN)
extern "C" int creo_brl_frontend_command(uiCmdCmdId, uiCmdValue *, void *);
extern "C" uiCmdAccessState creo_brl_frontend_access(uiCmdAccessMode);
extern "C" void doit(char *, char *, ProAppData);

extern "C" void
creo_brl_show_status(const char *message)
{
    creo_log(NULL, MSG_STATUS, "%s", message);
}

extern "C" void
creo_brl_core_load_profile_shim(void)
{
    load_profile();
}

extern "C" void
creo_brl_core_doit_shim(char *dialog, char *component, ProAppData appdata)
{
    doit(dialog, component, appdata);
}

extern "C" int
user_initialize()
{
    ProError err = PRO_TK_GENERAL_ERROR;
    ProFileName msgfil = {'\0'};
    int expected_wchar_size = 0;
    uiCmdCmdId cmd_id = 0;

    err = ProUITranslationFilesEnable();
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL, MSG_STATUS, "ProUITranslationFilesEnable failed (%d)", err);
        return -1;
    }

    err = ProWcharSizeVerify((int)sizeof(wchar_t), &expected_wchar_size);
    if (err != PRO_TK_NO_ERROR || expected_wchar_size != (int)sizeof(wchar_t)) {
        creo_log(NULL, MSG_STATUS, "wchar_t size verification failed (%d)", err);
        return -1;
    }

    err = ProCmdActionAdd(
        "CREO-BRL",
        creo_brl_frontend_command,
        uiProe2ndImmediate,
        creo_brl_frontend_access,
        PRO_B_FALSE,
        PRO_B_FALSE,
        &cmd_id);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL, MSG_STATUS, "Failed to add creo-brl action (%d)", err);
        return -1;
    }

    ProStringToWstring(msgfil, CREO_BRL_MSG_FNAME);
    err = ProMenubarmenuPushbuttonAdd(
        "Tools",
        "CREO-BRL",
        "CREO-BRL",
        "CREO-BRL-HELP",
        NULL,
        PRO_B_TRUE,
        cmd_id,
        msgfil);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(NULL, MSG_STATUS, "Failed to add creo-brl menu bar push button (%d)", err);
        return -1;
    }

    return 0;
}


extern "C" void
user_terminate()
{
    ProMessageClear();
}
#else
/*
 * The real Creo-facing lifecycle lives in the facade.  The runtime-loaded
 * backend only exposes the dialog actions and conversion routines that the
 * facade invokes once Creo has established the Toolkit state.
 */
extern "C" __declspec(dllexport) int
creo_brl_core_initialize()
{
    ProError err = PRO_TK_GENERAL_ERROR;
    int i = 0;

    /*
     * Keep initialization limited to a simple direct Toolkit smoke test.
     * The facade owns command/menu registration because those APIs rely on
     * the registered Toolkit DLL wrapper state set up by Creo.
     */
    err = ProWcharSizeVerify(sizeof(wchar_t), &i);
    if (err != PRO_TK_NO_ERROR || (i != sizeof(wchar_t))) {
        creo_log(NULL, MSG_STATUS, "\"wchar_t\" is the incorrect size (%d), size should be %d", (int)sizeof(wchar_t), i);
        return -1;
    }

    return 0;
}


extern "C" __declspec(dllexport) void
creo_brl_core_terminate()
{
}

 /*
  * IMPORTANT - the names of the next two functions - user_initialize
  * and user_terminate - are dictated by the Creo API. Both are
  * *required* to be present, but we just use them as stubs for
  * the runtime facade. The "real" logic is runtime-loaded from
  * loader.c.
  */
extern "C" int user_initialize()
{
    return 0;
}


extern "C" void user_terminate()
{
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
