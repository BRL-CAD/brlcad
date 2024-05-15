/**
 *                  A S S E M B L Y . C P P
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
 * @file assembly.cpp
 */

#include "common.h"
#include "creo-brl.h"
#include <string.h>

/* Assembly processing container */
struct assem_conv_info {
    struct creo_conv_info *cinfo; /* Global state */
    ProMdl curr_parent;
    struct wmember *wcmb;
};


/** Callback function used only for find_empty_assemblies */
extern "C" ProError
assembly_check_empty(ProFeature *feat, ProError UNUSED(status), ProAppData app_data)
{

    ProError err = PRO_TK_GENERAL_ERROR;
    ProMdlfileType ftype;

    wchar_t wname[CREO_NAME_MAX];
    struct adata *ada = (struct adata *)app_data;
    struct creo_conv_info *cinfo = ada->cinfo;
    int *has_shape = (int *)ada->data;

    err = ProAsmcompMdlMdlnameGet(feat, &ftype, wname);
    if (err != PRO_TK_NO_ERROR)
        return err;

    if (cinfo->empty->find(wname) == cinfo->empty->end())
        (*has_shape) = 1;

    return PRO_TK_NO_ERROR;
}


/**
 * Run this only *after* output_parts - that is where empty "assembly"
 * objects will be identified.  Without knowing which parts are empty,
 * we can't know if an assembly of parts is empty.
 */
extern "C" void
find_empty_assemblies(struct creo_conv_info *cinfo)
{
    int steady_state = 0;

    if (cinfo->empty->size() == 0)
        return;

    while (!steady_state) {
        std::set<wchar_t *, WStrCmp>::iterator d_it;
        steady_state = 1;
        struct adata *ada;
        BU_GET(ada, struct adata);
        ada->cinfo = cinfo;
        for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
            /*
             * For each assem, verify at least one child is non-empty.
             * If all children are empty, add to empty set and unset
             * steady_state.
             */
            int has_shape = 0;
            ada->data = (void *)&has_shape;
            ProMdl model;
            if (ProMdlnameInit(*d_it, PRO_MDLFILE_ASSEMBLY, &model) == PRO_TK_NO_ERROR)
                if (cinfo->empty->find(*d_it) == cinfo->empty->end()) {
                    ProSolidFeatVisit(ProMdlToPart(model), assembly_check_empty, (ProFeatureFilterAction)component_filter, (ProAppData)ada);
                    if (!has_shape) {
                        char ename[CREO_NAME_MAX];
                        wchar_t *stable = stable_wchar(cinfo, *d_it);
                        ProWstringToString(ename, *d_it);
                        if (!stable)
                            creo_log(cinfo, MSG_PLAIN, "  ASSEM: \"%s\" is empty, but no stable version of name found\n", ename);
                        else {
                            creo_log(cinfo, MSG_PLAIN, "  ASSEM: All contents of \"%s\" are empty, skipping...\n", ename);
                            cinfo->empty->insert(stable);
                            steady_state = 0;
                        }
                        creo_log(cinfo, MSG_FAIL, "Assembly \"%s\" not converted\n", ename);
                    }
                }
        }
    }
}


/**
 * Routine to check if xform is non-identity matrix
 */
extern "C" int
is_non_identity(ProMatrix xform)
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            if (i == j) {
                if (!NEAR_EQUAL(xform[i][j], 1.0, SMALL_FASTF))
                    return 1;
            } else {
                if (!NEAR_EQUAL(xform[i][j], 0.0, SMALL_FASTF))
                    return 1;
            }
        }
    return 0;
}


/**
 * Get the transformation matrix to apply to a member (feat) of a
 * comb this call is creating a path from the assembly to this
 * particular member (assembly/member)
 */
extern "C" ProError
assembly_entry_matrix(struct creo_conv_info *cinfo, ProMdl parent, ProFeature *feat, mat_t *mat)
{
    if(!feat || !mat)
        return PRO_TK_GENERAL_ERROR;

    ProAsmcomppath comp_path;
    ProError       err = PRO_TK_GENERAL_ERROR;
    ProIdTable     id_table;
    ProMdlfileType ftype;
    ProMatrix      xform;

    /* Get strings in case we need to log */
    wchar_t wpname[CREO_NAME_MAX];
    char     pname[CREO_NAME_MAX];
    wchar_t wcname[CREO_NAME_MAX];
    char     cname[CREO_NAME_MAX];

    /* Retrieve the parent (assembly) name */
    (void)ProMdlMdlnameGet(parent, wpname);
    ProWstringToString(pname, wpname);
    lower_case(pname);

    /* Retrieve the assembly component (part) name */
    (void)ProAsmcompMdlMdlnameGet(feat, &ftype, wcname);
    ProWstringToString(cname, wcname);
    lower_case(cname);

    /* Find the Creo matrix, obtain the path */
    id_table[0] = feat->id;
    err = ProAsmcomppathInit(ProMdlToSolid(parent), id_table, 1, &comp_path);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_PLAIN, "FAILURE: Unknown path from \"%s\" to \"%s\", aborting...\n", pname, cname);
        return err;
    }

    /* Accumulate the xform matrix along the path created above */
    err = ProAsmcomppathTrfGet(&comp_path, PRO_B_TRUE, xform);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_PLAIN, "FAILURE: Transformation matrix for \"%s\" failed: \"%s/%s\", aborting...\n", pname, pname, cname);
        return err;
    }

    /*
     * Notes:
     *
     *   1) Transforms matrix to BRL-CAD form:
     *
     *      | a b c d |
     *      | e f g h |  --->  [a e i m  b f j n  c g k o  d h l p]
     *      | i j k l |
     *      | m n o p |
     *
     *   2) Substitutes unit conversion factor for Creo
     *
     *       with:  m = n = o = cinfo->creo_to_brl_conv
     *
     */
    if (is_non_identity(xform)) {
        struct bu_vls mstr = BU_VLS_INIT_ZERO;
        for (int j = 0; j < 4; j++)
            for (int i = 0; i < 4; i++) {
                if (i == 3 && j < 3)
                    bu_vls_printf(&mstr, " %.12e", xform[i][j] * cinfo->creo_to_brl_conv);
                else
                    bu_vls_printf(&mstr, " %.12e", xform[i][j]);
            }
        bn_decode_mat((*mat), bu_vls_addr(&mstr));
        bu_vls_free(&mstr);
        return PRO_TK_NO_ERROR;
    }

    return PRO_TK_GENERAL_ERROR;
}


extern "C" ProError
assembly_write_entry(ProFeature *feat, ProError UNUSED(status), ProAppData app_data)
{
    ProBoolean     is_skel = PRO_B_FALSE;
    ProError       err = PRO_TK_GENERAL_ERROR;
    ProMdl         model;
    ProMdlfileType ftype;

    struct bu_vls *entry_name;
    wchar_t wname[CREO_NAME_MAX];
    struct assem_conv_info *ainfo = (struct assem_conv_info *)app_data;
    mat_t xform;
    MAT_IDN(xform);

    /* Get name of current member, ignoring any errors */
    err = ProAsmcompMdlMdlnameGet(feat, &ftype, wname);
    if (err != PRO_TK_NO_ERROR)
        return PRO_TK_NO_ERROR;

    /* Skip this member if the object it refers to is empty */
    if (ainfo->cinfo->empty->find(wname) != ainfo->cinfo->empty->end())
        return PRO_TK_NO_ERROR;

    /* If this is a skeleton, skip */
    err = ProAsmcompMdlGet(feat, &model);
    if (err == PRO_TK_NO_ERROR) {
        ProMdlIsSkeleton(model, &is_skel);
        if (is_skel)
            return PRO_TK_NO_ERROR;
    }

    /* Get BRL-CAD name */
    switch (ftype) {
        case PRO_MDLFILE_PART:
            entry_name = get_brlcad_name(ainfo->cinfo, wname, "r", N_REGION);
            break;
        case PRO_MDLFILE_ASSEMBLY:
            entry_name = get_brlcad_name(ainfo->cinfo, wname, NULL, N_ASSEM);
            break;
        default:
            return PRO_TK_NO_ERROR;
    }

    /*
     * In case the name routine failed for whatever reason, don't call
     * mk_addmember - all we'll get is a crash.  Just keep going.
     */
    if (!entry_name)
        return PRO_TK_NO_ERROR;

    /*
     * Get matrix relative to current parent (if any) and create the
     * comb entry
     */
    err = assembly_entry_matrix(ainfo->cinfo, ainfo->curr_parent, feat, &xform);
    if (err == PRO_TK_NO_ERROR)
        (void)mk_addmember(bu_vls_addr(entry_name), &(ainfo->wcmb->l), xform, WMOP_UNION);
    else
        (void)mk_addmember(bu_vls_addr(entry_name), &(ainfo->wcmb->l),  NULL, WMOP_UNION);

    return PRO_TK_NO_ERROR;
}


/** Only run this *after* find_empty_assemblies has been run */
extern "C" ProError
output_assembly(struct creo_conv_info *cinfo, ProMdl model)
{

    ProError err = PRO_TK_GENERAL_ERROR;
    ProMassProperty massprops;

    struct bu_vls *comb_name;
    struct bu_vls *obj_name;
    wchar_t wname[CREO_NAME_MAX];
    struct assem_conv_info *ainfo;
    BU_GET(ainfo, struct assem_conv_info);
    ainfo->cinfo = cinfo;

    /*
     * Check for exploded assembly.
     *
     * TODO: This is causing a crash, can't enable???
     *
     *    ProBoolean is_exploded = PRO_B_FALSE;
     *    ProAssemblyIsExploded(*(ProAssembly *)model, &is_exploded);
     *    if (is_exploded) ProAssemblyUnexplode(*(ProAssembly *)model);
     */

    /* We'll need to assemble the list of children */
    struct wmember wcomb;
    BU_LIST_INIT(&wcomb.l);

    /* Initial comb setup */
    char cname[CREO_NAME_MAX];

    (void)ProMdlMdlnameGet(model, wname);
    ProWstringToString(cname, wname);
    lower_case(cname);

    ainfo->curr_parent = model;
    ainfo->wcmb = &wcomb;

    /* Add children */
    ProSolidFeatVisit(ProMdlToPart(model), assembly_write_entry, (ProFeatureFilterAction)component_filter, (ProAppData)ainfo);
    creo_log(cinfo, MSG_PLAIN, "  ASSEM: All children of \"%s\" were visited\n", cname);

    /* Get BRL-CAD name */
    comb_name = get_brlcad_name(cinfo, wname, NULL, N_ASSEM);

    /* Data sufficient - write the comb */
    mk_lcomb(cinfo->wdbp, bu_vls_addr(comb_name), &wcomb, 0, NULL, NULL, NULL, 0);

    /*
     * Set attributes, if the CREO object has any of the ones
     * on the user-supplied list.
     */
    struct directory *dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(comb_name), LOOKUP_QUIET);
    db5_get_attributes(cinfo->wdbp->dbip, &cinfo->avs, dp);

    /* Write the object ID as an attribute */
    obj_name = get_brlcad_name(cinfo, wname, NULL, N_CREO);
    bu_avs_add(&cinfo->avs, "ptc_name", bu_vls_addr(obj_name));

    ProWVerstamp cstamp;
    if (ProMdlVerstampGet(model, &cstamp) == PRO_TK_NO_ERROR) {
        char *verstr;
        if (ProVerstampStringGet(cstamp, &verstr) == PRO_TK_NO_ERROR)
            bu_avs_add(&cinfo->avs, "ptc_version_stamp", verstr);
        ProVerstampStringFree(&verstr);
    }

    /* Export user-supplied list of parameters */
    param_export(cinfo, model, cname);

    /*
     * Solid mass properties are handled separately in Creo,
     * so deal with those as well...
     */
    struct bu_vls vstr = BU_VLS_INIT_ZERO;

    err = ProSolidMassPropertyGet(ProMdlToSolid(model), NULL, &massprops);
    if (err == PRO_TK_NO_ERROR) {
        if (massprops.volume > 0.0) {
            bu_vls_sprintf(&vstr, "%g", massprops.volume);
            bu_avs_add(&cinfo->avs, "volume", bu_vls_addr(&vstr));
        }
        if (massprops.surface_area > 0.0) {
            bu_vls_sprintf(&vstr, "%g", massprops.surface_area);
            bu_avs_add(&cinfo->avs, "surface_area", bu_vls_addr(&vstr));
        }
        if (massprops.density > 0.0) {
            bu_vls_sprintf(&vstr, "%g", massprops.density);
            bu_avs_add(&cinfo->avs, "density", bu_vls_addr(&vstr));
        }
        if (massprops.mass > 0.0) {
            bu_vls_sprintf(&vstr, "%g", massprops.mass);
            bu_avs_add(&cinfo->avs, "mass", bu_vls_addr(&vstr));
        }
    bu_vls_free(&vstr);
    }


    /* Write */
    db5_update_attributes(dp, &cinfo->avs, cinfo->wdbp->dbip);

    creo_log(cinfo, MSG_PLAIN, "  ASSEM: Conversion of assembly \"%s\" complete\n", cname);
    /* Free local container */
    BU_PUT(ainfo, struct assem_conv_info);
    return PRO_TK_NO_ERROR;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
