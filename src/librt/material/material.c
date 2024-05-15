/*                      M A T E R I A L . C
 * BRL-CAD
 *
 * Copyright (c) 2021-2024 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup librt */
/** @{ */
/** @file librt/material.c
 *
 * Various functions associated with material object database I/O
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bn.h"
#include "rt/db4.h"
#include "pc.h"
#include "raytrace.h"


const char *
get_all_avs_keys(const struct bu_attribute_value_set *avsp, const char *title)
{
    BU_CK_AVS(avsp);

    struct bu_attribute_value_pair *avpp;
    size_t i;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (title) {
        bu_vls_strcat(&str, title);
        bu_vls_strcat(&str, "=\"");
    }

    avpp = avsp->avp;
    for (i = 0; i < avsp->count; i++, avpp++) {
        bu_vls_strcat(&str, "        ");
        bu_vls_strcat(&str, avpp->name ? avpp->name : "NULL");
        bu_vls_strcat(&str, ":");
        bu_vls_strcat(&str, avpp->value ? avpp->value : "NULL");
        bu_vls_strcat(&str, "\n");
    }

    if (title) {
        bu_vls_strcat(&str, "\"");
    }

    const char *attributes = bu_vls_strgrab(&str);

    return attributes;
}


/**
 * Free the storage associated with the rt_db_internal version of
 * material object.
 */
void rt_material_ifree(struct rt_db_internal *ip)
{
    register struct rt_material_internal *material;

    RT_CK_DB_INTERNAL(ip);
    material = (struct rt_material_internal *)ip->idb_ptr;

    if (material) {
        material->magic = 0; /* sanity */
        bu_vls_free(&material->name);
        bu_vls_free(&material->parent);
        bu_vls_free(&material->source);

        bu_avs_free(&material->physicalProperties);
        bu_avs_free(&material->mechanicalProperties);
        bu_avs_free(&material->opticalProperties);
        bu_avs_free(&material->thermalProperties);
        bu_free((void *)material, "material ifree");
    }
    ip->idb_ptr = ((void *)0); /* sanity */
}


/**
 * Import a material from the database format to the internal format.
 */
int rt_material_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *UNUSED(mat), const struct db_i *UNUSED(dbip))
{
    struct rt_material_internal *material_ip;

    BU_CK_EXTERNAL(ep);
    RT_CK_DB_INTERNAL(ip);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_MATERIAL;
    ip->idb_meth = &OBJ[ID_MATERIAL];
    BU_ALLOC(ip->idb_ptr, struct rt_material_internal);

    material_ip = (struct rt_material_internal *)ip->idb_ptr;
    material_ip->magic = RT_MATERIAL_MAGIC;
    BU_VLS_INIT(&material_ip->name);
    BU_VLS_INIT(&material_ip->parent);
    BU_VLS_INIT(&material_ip->source);

    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls parent = BU_VLS_INIT_ZERO;
    struct bu_vls source = BU_VLS_INIT_ZERO;

    struct bu_external physical_ep = BU_EXTERNAL_INIT_ZERO;
    struct bu_external mechanical_ep = BU_EXTERNAL_INIT_ZERO;
    struct bu_external optical_ep = BU_EXTERNAL_INIT_ZERO;
    struct bu_external thermal_ep = BU_EXTERNAL_INIT_ZERO;
    unsigned char *cp = ep->ext_buf;

    // copy name to internal format
    bu_vls_strcat(&name, (char *)cp);
    bu_vls_vlscat(&material_ip->name, &name);
    cp += strlen((const char *)cp) + 1;
    bu_vls_free(&name);

    // copy parent to internal format
    bu_vls_strcat(&parent, (char *)cp);
    bu_vls_vlscat(&material_ip->parent, &parent);
    cp += strlen((const char *)cp) + 1;
    bu_vls_free(&parent);

    // copy source to internal format
    bu_vls_strcat(&source, (char *)cp);
    bu_vls_vlscat(&material_ip->source, &source);
    cp += strlen((const char *)cp) + 1;
    bu_vls_free(&source);

    // copy physical properties
    uint32_t size = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;

    physical_ep.ext_nbytes = size;
    physical_ep.ext_buf = cp;
    if (size > 0) {
        db5_import_attributes(&material_ip->physicalProperties, &physical_ep);
    } else {
        bu_avs_init_empty(&material_ip->physicalProperties);
    }
    cp += size;

    // copy mechanical properties
    size = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;

    mechanical_ep.ext_nbytes = size;
    mechanical_ep.ext_buf = cp;
    if (size > 0) {
        db5_import_attributes(&material_ip->mechanicalProperties, &mechanical_ep);
    } else {
        bu_avs_init_empty(&material_ip->mechanicalProperties);
    }
    cp += size;

    // copy optical properties
    size = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;

    optical_ep.ext_nbytes = size;
    optical_ep.ext_buf = cp;
    if (size > 0) {
        db5_import_attributes(&material_ip->opticalProperties, &optical_ep);
    } else {
        bu_avs_init_empty(&material_ip->opticalProperties);
    }
    cp += size;

    // copy thermal properties
    size = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;

    thermal_ep.ext_nbytes = size;
    thermal_ep.ext_buf = cp;
    if (size > 0) {
        db5_import_attributes(&material_ip->thermalProperties, &thermal_ep);
    } else {
        bu_avs_init_empty(&material_ip->thermalProperties);
    }

    return 0; /* OK */
}


/**
 * Export a material from the internal format to the database format.
 */
int rt_material_export5(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *UNUSED(dbip))
{
    struct rt_material_internal *material_ip;
    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_type != ID_MATERIAL)
        bu_bomb("rt_material_export() type not ID_MATERIAL");
    material_ip = (struct rt_material_internal *)ip->idb_ptr;

    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls parent = BU_VLS_INIT_ZERO;
    struct bu_vls source = BU_VLS_INIT_ZERO;

    struct bu_external physical_ep = BU_EXTERNAL_INIT_ZERO;
    struct bu_external mechanical_ep = BU_EXTERNAL_INIT_ZERO;
    struct bu_external optical_ep = BU_EXTERNAL_INIT_ZERO;
    struct bu_external thermal_ep = BU_EXTERNAL_INIT_ZERO;

    BU_EXTERNAL_INIT(ep);

    db5_export_attributes(&physical_ep, &material_ip->physicalProperties);
    db5_export_attributes(&mechanical_ep, &material_ip->mechanicalProperties);
    db5_export_attributes(&optical_ep, &material_ip->opticalProperties);
    db5_export_attributes(&thermal_ep, &material_ip->thermalProperties);

    // initialize entire buffer
    ep->ext_nbytes = bu_vls_strlen(&material_ip->name) + 1 + bu_vls_strlen(&material_ip->parent) + 1 + bu_vls_strlen(&material_ip->source) + 1 + (4 * SIZEOF_NETWORK_LONG) + physical_ep.ext_nbytes + mechanical_ep.ext_nbytes + optical_ep.ext_nbytes + thermal_ep.ext_nbytes;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "material external");
    unsigned char *cp = ep->ext_buf;

    // copy over the name to buffer
    bu_vls_vlscat(&name, &material_ip->name);
    bu_strlcpy((char *)cp, bu_vls_cstr(&name), bu_vls_strlen(&name) + 1);
    cp += bu_vls_strlen(&name) + 1;
    bu_vls_free(&name);

    // copy over the parent to buffer
    bu_vls_vlscat(&parent, &material_ip->parent);
    bu_strlcpy((char *)cp, bu_vls_cstr(&parent), bu_vls_strlen(&parent) + 1);
    cp += bu_vls_strlen(&parent) + 1;
    bu_vls_free(&parent);

    // copy over the source to buffer
    bu_vls_vlscat(&source, &material_ip->source);
    bu_strlcpy((char *)cp, bu_vls_cstr(&source), bu_vls_strlen(&source) + 1);
    cp += bu_vls_strlen(&source) + 1;
    bu_vls_free(&source);

    // copy physical properties
    *(uint32_t *)cp = htonl(physical_ep.ext_nbytes);
    cp += SIZEOF_NETWORK_LONG;
    memcpy(cp, physical_ep.ext_buf, physical_ep.ext_nbytes);
    cp += physical_ep.ext_nbytes;
    bu_free_external(&physical_ep);

    // copy mechanical properties
    *(uint32_t *)cp = htonl(mechanical_ep.ext_nbytes);
    cp += SIZEOF_NETWORK_LONG;
    memcpy(cp, mechanical_ep.ext_buf, mechanical_ep.ext_nbytes);
    cp += mechanical_ep.ext_nbytes;
    bu_free_external(&mechanical_ep);

    // copy optical properties
    *(uint32_t *)cp = htonl(optical_ep.ext_nbytes);
    cp += SIZEOF_NETWORK_LONG;
    memcpy(cp, optical_ep.ext_buf, optical_ep.ext_nbytes);
    cp += optical_ep.ext_nbytes;
    bu_free_external(&optical_ep);

    // copy thermal properties
    *(uint32_t *)cp = htonl(thermal_ep.ext_nbytes);
    cp += SIZEOF_NETWORK_LONG;
    memcpy(cp, thermal_ep.ext_buf, thermal_ep.ext_nbytes);
    bu_free_external(&thermal_ep);

    return 0; /* OK */
}

void
rt_material_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    struct rt_material_internal* ip;

    intern->idb_type = ID_MATERIAL;
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;

    BU_ASSERT(&OBJ[intern->idb_type] == ftp);
    intern->idb_meth = ftp;

    BU_ALLOC(ip, struct rt_material_internal);
    intern->idb_ptr = (void *)ip;

    ip->magic = RT_MATERIAL_MAGIC;
    BU_VLS_INIT(&ip->name);
    BU_VLS_INIT(&ip->parent);
    BU_VLS_INIT(&ip->source);
    BU_AVS_INIT(&ip->physicalProperties);
    BU_AVS_INIT(&ip->mechanicalProperties);
    BU_AVS_INIT(&ip->opticalProperties);
    BU_AVS_INIT(&ip->thermalProperties);
}

int
rt_material_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_material_internal *material = (struct rt_material_internal *)intern->idb_ptr;

    while (argv && argc >= 2) {
        const char* prop = *argv++;
        argc--;

        if (BU_STR_EQUAL(prop, "name")) {
            BU_VLS_INIT(&material->name);
            bu_vls_strcpy(&material->name, *argv++);
            argc--;
        } else if (BU_STR_EQUAL(prop, "parent")) {
            BU_VLS_INIT(&material->parent);
            bu_vls_strcpy(&material->parent, *argv++);
            argc--;
        } else if (BU_STR_EQUAL(prop, "source")) {
            BU_VLS_INIT(&material->source);
            bu_vls_strcpy(&material->source, *argv++);
            argc--;
        } else if (BU_STR_EQUAL(prop, "physical")) {
            if (argc < 2) {
                bu_vls_printf(logstr, "Error: (%s) requres sub_property / value pair", prop);
                return BRLCAD_ERROR;
            }
            const char* attr = *argv++;
            const char* val = *argv++;
            bu_avs_remove(&material->physicalProperties, attr);
            bu_avs_add(&material->physicalProperties, attr, val);
            argc -= 2;
        }  else if (BU_STR_EQUAL(prop, "mechanical")) {
            if (argc < 2) {
                bu_vls_printf(logstr, "Error: (%s) requres sub_property / value pair", prop);
                return BRLCAD_ERROR;
            }
            const char* attr = *argv++;
            const char* val = *argv++;
            bu_avs_remove(&material->mechanicalProperties, attr);
            bu_avs_add(&material->mechanicalProperties, attr, val);
            argc -= 2;
        } else if (BU_STR_EQUAL(prop, "optical")) {
            if (argc < 2) {
                bu_vls_printf(logstr, "Error: (%s) requres sub_property / value pair", prop);
                return BRLCAD_ERROR;
            }
            const char* attr = *argv++;
            const char* val = *argv++;
            bu_avs_remove(&material->opticalProperties, attr);
            bu_avs_add(&material->opticalProperties, attr, val);
            argc -= 2;
        } else if (BU_STR_EQUAL(prop, "thermal")) {
            if (argc < 2) {
                bu_vls_printf(logstr, "Error: (%s) requres sub_property / value pair", prop);
                return BRLCAD_ERROR;
            }
            const char* attr = *argv++;
            const char* val = *argv++;
            bu_avs_remove(&material->thermalProperties, attr);
            bu_avs_add(&material->thermalProperties, attr, val);
            argc -= 2;
        } else {
            bu_vls_printf(logstr, "an error occurred finding the material property group: (%s)", prop);
            return BRLCAD_ERROR;
        }
    }

    return BRLCAD_OK;
}


/**
 * Make human-readable formatted presentation of this object.  First
 * line describes type of object.  Additional lines are indented one
 * tab, and give parameter values.
 */
int rt_material_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double UNUSED(mm2local))
{
    register struct rt_material_internal *material_ip = (struct rt_material_internal *)ip->idb_ptr;

    RT_CHECK_MATERIAL(material_ip);

    struct bu_vls buf = BU_VLS_INIT_ZERO;
    bu_vls_strcat(str, "material (MATERIAL)\n");

    bu_vls_printf(&buf, "    name: %s\n", material_ip->name.vls_str);
    bu_vls_printf(&buf, "    parent: %s\n", material_ip->parent.vls_str);
    bu_vls_printf(&buf, "    source: %s\n", material_ip->source.vls_str);

    if (!verbose) {
        bu_vls_vlscat(str, &buf);
        bu_vls_free(&buf);
        return 0;
    }

    const char *physicalProperties = get_all_avs_keys(&material_ip->physicalProperties, NULL);
    const char *mechanicalProperties = get_all_avs_keys(&material_ip->mechanicalProperties, NULL);
    const char *opticalProperties = get_all_avs_keys(&material_ip->opticalProperties, NULL);
    const char *thermalProperties = get_all_avs_keys(&material_ip->thermalProperties, NULL);

    bu_vls_printf(&buf, "    physical properties\n%s", physicalProperties);
    bu_vls_printf(&buf, "    mechanical properties\n%s", mechanicalProperties);
    bu_vls_printf(&buf, "    optical properties\n%s", opticalProperties);
    bu_vls_printf(&buf, "    thermal properties\n%s", thermalProperties);

    bu_vls_vlscat(str, &buf);
    bu_vls_free(&buf);

    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
