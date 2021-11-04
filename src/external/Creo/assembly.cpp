/*
 *                  A S S E M B L Y . C P P
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
 *
/**
 * @file assembly.cpp
 */

#include "common.h"
#include "creo-brl.h"
#include <string.h>

/** Assembly processing container */
struct assem_conv_info {
    struct creo_conv_info *cinfo; /** global state */
    ProMdl curr_parent;
    struct wmember *wcmb;
};


/** Callback function used only for find_empty_assemblies */
extern "C" ProError
assembly_check_empty(ProFeature *feat, ProError UNUSED(status), ProAppData app_data)
{
    ProError lstatus;
    ProMdlType type;
    wchar_t wname[CREO_NAME_MAX];
    struct adata *ada = (struct adata *)app_data;
    struct creo_conv_info *cinfo = ada->cinfo;
    int *has_shape = (int *)ada->data;
    if ((lstatus = ProAsmcompMdlNameGet(feat, &type, wname)) != PRO_TK_NO_ERROR) return lstatus;
    if (cinfo->empty->find(wname) == cinfo->empty->end()) (*has_shape) = 1;
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
    if (cinfo->empty->size() == 0) return;
    while (!steady_state) {
        std::set<wchar_t *, WStrCmp>::iterator d_it;
        steady_state = 1;
        struct adata *ada;
        BU_GET(ada, struct adata);
        ada->cinfo = cinfo;
        for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
            /**
             * For each assem, verify at least one child is non-empty.
             * If all children are empty, add to empty set and unset
             * steady_state.
             */
            int has_shape = 0;
            ada->data = (void *)&has_shape;
            ProMdl model;
            if (ProMdlnameInit(*d_it, PRO_MDLFILE_ASSEMBLY, &model) == PRO_TK_NO_ERROR) {
                if (cinfo->empty->find(*d_it) == cinfo->empty->end()) {
                    ProSolidFeatVisit(ProMdlToPart(model), assembly_check_empty, (ProFeatureFilterAction)component_filter, (ProAppData)ada);
                    if (!has_shape) {
                        char ename[MAXPATHLEN];
                        wchar_t *stable = stable_wchar(cinfo, *d_it);
                        ProWstringToString(ename, *d_it);
                        if (!stable) {
                            creo_log(cinfo, MSG_DEBUG, "%s - is empty assembly, but no stable version of name found???.\n", ename);
                        } else {
                            creo_log(cinfo, MSG_DEBUG, "all contents of assembly %s are empty, skipping\n", ename);
                            cinfo->empty->insert(stable);
                            steady_state = 0;
                        }
                        creo_log(cinfo, MSG_FAIL, "%s not converted.\n", ename);
                    }
                }
            }
        }
    }
}


/** Routine to check if xform is an identity */
extern "C" int
is_non_identity(ProMatrix xform)
{
    int i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            if (i == j) {
                if (!NEAR_EQUAL(xform[i][j], 1.0, SMALL_FASTF))
                    return 1;
            } else {
                if (!NEAR_EQUAL(xform[i][j], 0.0, SMALL_FASTF))
                    return 1;
            }
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
    if(!feat || !mat) return PRO_TK_GENERAL_ERROR;

    /** Get strings in case we need to log */
    ProError status;
    ProMdlType type;
    wchar_t wpname[CREO_NAME_MAX];
    char pname[CREO_NAME_MAX];
    wchar_t wcname[CREO_NAME_MAX];
    char cname[CREO_NAME_MAX];
    ProMdlNameGet(parent, wpname);
    (void)ProWstringToString(pname, wpname);
    ProAsmcompMdlNameGet(feat, &type, wcname);
    (void)ProWstringToString(cname, wcname);

    /** Find the CREO matrix */
    ProAsmcomppath comp_path;
    ProMatrix xform;
    ProIdTable id_table;
    id_table[0] = feat->id;
    /** Create the path */
    status = ProAsmcomppathInit((ProSolid)parent, id_table, 1, &comp_path);
    if (status != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_DEBUG, "%s: failed to get path from %s to %s, aborting\n", pname, pname, cname);
        return status;
    }
    /** Accumulate the xform matrix along the path created above */
    status = ProAsmcomppathTrfGet(&comp_path, PRO_B_TRUE, xform);
    if (status != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_DEBUG, "%s: failed to get transformation matrix %s/%s, aborting\n", pname, pname, cname);
        return status;
    }

    /**
     * Write the matrix to BRL-CAD form. Note: apparently, one
     * of the functions of these matricies in Creo is to correct
     * the size of subcomponents using different units.
     */
    if (is_non_identity(xform)) {
        struct bu_vls mstr = BU_VLS_INIT_ZERO;
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                if (k == 3 && j < 3) {
                    bu_vls_printf(&mstr, " %.12e", xform[k][j] * cinfo->creo_to_brl_conv);
                } else {
                    bu_vls_printf(&mstr, " %.12e", xform[k][j]);
                }
            }
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
    ProError lstatus;
    ProMdlType mtype;
    ProMdl model;
    ProBoolean is_skel = PRO_B_FALSE;
    struct bu_vls *entry_name;
    wchar_t wname[CREO_NAME_MAX];
    struct assem_conv_info *ainfo = (struct assem_conv_info *)app_data;
    mat_t xform;
    MAT_IDN(xform);

    /** Get name of current member */
    if ((lstatus = ProAsmcompMdlNameGet(feat, &mtype, wname)) != PRO_TK_NO_ERROR) return PRO_TK_NO_ERROR;

    /** Skip this member if the object it refers to is empty */
    if (ainfo->cinfo->empty->find(wname) != ainfo->cinfo->empty->end()) return PRO_TK_NO_ERROR;

    /** If this is a skeleton, skip */
    if ((ProAsmcompMdlGet(feat, &model)) == PRO_TK_NO_ERROR) {
        ProMdlIsSkeleton(model, &is_skel);
        if (is_skel) return PRO_TK_NO_ERROR;
    }

    /** Get BRL-CAD name */
    switch (mtype) {
        case PRO_MDL_PART:
            entry_name = get_brlcad_name(ainfo->cinfo, wname, "r", N_REGION);
            break;
        case PRO_MDL_ASSEMBLY:
            entry_name = get_brlcad_name(ainfo->cinfo, wname, NULL, N_ASSEM);
            break;
        default:
            return PRO_TK_NO_ERROR;
            break;
    }

    /**
     * In case the name routine failed for whatever reason, don't call
     * mk_addmember - all we'll get is a crash.  Just keep going.
     */
    if (!entry_name) return PRO_TK_NO_ERROR;

    /**
     * Get matrix relative to current parent (if any) and create the
     * comb entry
     */
    if ((lstatus = assembly_entry_matrix(ainfo->cinfo, ainfo->curr_parent, feat, &xform)) == PRO_TK_NO_ERROR) {
        (void)mk_addmember(bu_vls_addr(entry_name), &(ainfo->wcmb->l), xform, WMOP_UNION);
    } else {
        (void)mk_addmember(bu_vls_addr(entry_name), &(ainfo->wcmb->l), NULL, WMOP_UNION);
    }

    return PRO_TK_NO_ERROR;

}


/** Only run this *after* find_empty_assemblies has been run */
extern "C" ProError
output_assembly(struct creo_conv_info *cinfo, ProMdl model)
{
    struct bu_vls *comb_name;
    struct bu_vls *obj_name;
    wchar_t wname[CREO_NAME_MAX];
    struct assem_conv_info *ainfo;
    BU_GET(ainfo, struct assem_conv_info);
    ainfo->cinfo = cinfo;

    /**
     * Check for exploded assembly.
     *
     * TODO: This is causing a crash, can't enable???
     *
     */
    //ProBoolean is_exploded = PRO_B_FALSE;
    //ProAssemblyIsExploded(*(ProAssembly *)model, &is_exploded);
    //if (is_exploded) ProAssemblyUnexplode(*(ProAssembly *)model);

    /** We'll need to assemble the list of children */
    struct wmember wcomb;
    BU_LIST_INIT(&wcomb.l);

    /** Initial comb setup */
    char cname[CREO_NAME_MAX];
    ProMdlMdlnameGet(model, wname);
    ProWstringToString(cname, wname);
    ainfo->curr_parent = model;
    ainfo->wcmb = &wcomb;

    /** Add children */
    ProSolidFeatVisit(ProMdlToPart(model), assembly_write_entry, (ProFeatureFilterAction)component_filter, (ProAppData)ainfo);
    creo_log(cinfo, MSG_DEBUG, "%s: All children of assembly visited.\n", cname);

    /** Get BRL-CAD name */
    comb_name = get_brlcad_name(cinfo, wname, NULL, N_ASSEM);

    /** Data sufficient - write the comb */
    mk_lcomb(cinfo->wdbp, bu_vls_addr(comb_name), &wcomb, 0, NULL, NULL, NULL, 0);

    /**
     * Set attributes, if the CREO object has any of the ones
     * on the user-supplied list.
     */
    struct directory *dp = db_lookup(cinfo->wdbp->dbip, bu_vls_addr(comb_name), LOOKUP_QUIET);
    struct bu_attribute_value_set avs;
    db5_get_attributes(cinfo->wdbp->dbip, &avs, dp);

    /** Write the object ID as an attribute */
    obj_name = get_brlcad_name(cinfo, wname, NULL, N_CREO);
    bu_avs_add(&avs, "CREO_NAME", bu_vls_addr(obj_name));

    ProWVerstamp cstamp;
    if (ProMdlVerstampGet(model, &cstamp) == PRO_TK_NO_ERROR) {
        char *vstr;
        if (ProVerstampStringGet(cstamp, &vstr) == PRO_TK_NO_ERROR) {
            bu_avs_add(&avs, "CREO_VERSION_STAMP", vstr);
        }
        ProVerstampStringFree(&vstr);
    }


    /** If we have a user-supplied list of attributes to save, do it */
    if (cinfo->attrs->size() > 0) {
        for (unsigned int i = 0; i < cinfo->attrs->size(); i++) {
            char *attr_val = NULL;
            const char *arg = cinfo->attrs->at(i);
            creo_attribute_val(&attr_val, arg, model);
            if (attr_val) {
                bu_avs_add(&avs, arg, attr_val);
                bu_free(attr_val, "value string");
            }
        }
    }
    /**
     * Solid mass properties are handled separately in CREO,
     * so deal with those as well...
     */
    ProMassProperty mass_prop;
    struct bu_vls mpval = BU_VLS_INIT_ZERO;
    if (ProSolidMassPropertyGet(ProMdlToSolid(model), NULL, &mass_prop) == PRO_TK_NO_ERROR) {
        if (mass_prop.density > 0.0) {
            bu_vls_sprintf(&mpval, "%g", mass_prop.density);
            bu_avs_add(&avs, "density", bu_vls_addr(&mpval));
        }
        if (mass_prop.mass > 0.0) {
            bu_vls_sprintf(&mpval, "%g", mass_prop.mass);
            bu_avs_add(&avs, "mass", bu_vls_addr(&mpval));
        }

        if (mass_prop.volume > 0.0) {
            bu_vls_sprintf(&mpval, "%g", mass_prop.volume);
            bu_avs_add(&avs, "volume", bu_vls_addr(&mpval));
        }
    }

    /** Standardize and write */
    db5_standardize_avs(&avs);
    db5_update_attributes(dp, &avs, cinfo->wdbp->dbip);

    creo_log(cinfo, MSG_DEBUG, "%s: assembly conversion done.\n", cname);
    /** Free local container */
    BU_PUT(ainfo, struct assem_conv_info);
    return PRO_TK_NO_ERROR;
}

/**
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
