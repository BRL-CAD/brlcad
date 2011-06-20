/*                     D B 5 _ T Y P E S . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2011 United States Government as represented by
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
/** @addtogroup db5 */
/** @{ */
/** @file librt/db5_types.c
 *
 * Map between Major_Types/Minor_Types and ASCII strings
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "db5.h"
#include "raytrace.h"


struct db5_type {
    int major_code;
    int minor_code;
    int heed_minor;
    char *tag;
    char *description;
};


/**
 * In order to support looking up Major_Types as well as (Major_Type,
 * Minor_Type) pairs, every Major_Type needs an entry with
 * heed_minor==0 and it must occur below any of its entries that have
 * heed_minor==1.
 */
static const struct db5_type type_table[] = {
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_TOR, 1, "tor", "torus" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_TGC, 1, "tgc", "truncated general cone" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_ELL, 1, "ell", "ellipsoid" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_ARB8, 1, "arb8", "arb8" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_ARS, 1, "ars", "waterline" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_HALF, 1, "half", "halfspace" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_REC, 1, "rec", "right elliptical cylinder" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_BSPLINE, 1, "bspline", "B-spline" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_SPH, 1, "sph", "sphere" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_NMG, 1, "nmg", "nmg" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_EBM, 1, "ebm", "extruded bitmap" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_VOL, 1, "vol", "voxels" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_ARBN, 1, "arbn", "arbn" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_PIPE, 1, "pipe", "pipe" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_PARTICLE, 1, "particle", "particle" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_RPC, 1, "rpc", "right parabolic cylinder" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_RHC, 1, "rhc", "right hyperbolic cylinder" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_EPA, 1, "epa", "elliptical paraboloid" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_EHY, 1, "ehy", "elliptical hyperboloid" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_ETO, 1, "eto", "elliptical torus" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_GRIP, 1, "grip", "grip" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_JOINT, 1, "joint", "joint" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_DSP, 1, "dsp", "displacement map (height field)" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_SKETCH, 1, "sketch", "sketch" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_EXTRUDE, 1, "extrude", "extrusion" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_SUBMODEL, 1, "submodel", "submodel" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_CLINE, 1, "cline", "cline" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_BOT, 1, "bot", "bag of triangles" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_COMBINATION, 1, "combination", "combination" },
    { DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_BREP, 1, "brep", "Boundary Representation" },
    { DB5_MAJORTYPE_BRLCAD, 0, 0, "brlcad", "BRL-CAD geometry" },
    { DB5_MAJORTYPE_ATTRIBUTE_ONLY, 0, 0, "attribonly", "attribute only" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_FLOAT, 1, "float", "array of floats" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_FLOAT, 1, "f", "array of floats" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_DOUBLE, 1, "double", "array of doubles" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_DOUBLE, 1, "d", "array of doubles" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_8BITINT_U, 1, "u8", "array of unsigned 8-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_16BITINT_U, 1, "u16", "array of unsigned 16-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT_U, 1, "u32", "array of unsigned 32-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT_U, 1, "uint", "array of unsigned 32-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT_U, 1, "ui", "array of unsigned 32-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_64BITINT_U, 1, "u64", "array of unsigned 64-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_8BITINT, 1, "8", "array of 8-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_16BITINT, 1, "16", "array of 16-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT, 1, "32", "array of 32-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT, 1, "int", "array of 32-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT, 1, "i", "array of 32-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_64BITINT, 1, "64", "array of 64-bit ints" },
    { DB5_MAJORTYPE_BINARY_UNIF, 0, 0, "binunif", "uniform-array binary" },
    /* Following entry must be at end of table */
    { DB5_MAJORTYPE_RESERVED, 0, 0, 0, 0 },
};


int
db5_type_tag_from_major(char **tag, const int major)
{
    register struct db5_type *tp;

    for (tp = (struct db5_type *) type_table;
	 tp->major_code != DB5_MAJORTYPE_RESERVED;
	 ++tp) {
	if ((tp->major_code == major) && !(tp->heed_minor)) {
	    *tag = tp->tag;
	    return 0;
	}
    }
    return 1;
}


int
db5_type_descrip_from_major(char **descrip, const int major)
{
    register struct db5_type *tp;

    for (tp = (struct db5_type *) type_table;
	 tp->major_code != DB5_MAJORTYPE_RESERVED;
	 ++tp) {
	if ((tp->major_code == major) && !(tp->heed_minor)) {
	    *descrip = tp->description;
	    return 0;
	}
    }
    return 1;
}


int
db5_type_tag_from_codes(char **tag, const int major, const int minor)
{
    register struct db5_type *tp;
    register int found_minors = 0;

    for (tp = (struct db5_type *) type_table;
	 tp->major_code != DB5_MAJORTYPE_RESERVED;
	 ++tp) {
	if (tp->major_code == major) {
	    if (tp->heed_minor)
		found_minors = 1;
	    if ((tp->minor_code == minor) || !found_minors) {
		*tag = tp->tag;
		return 0;
	    }
	}
    }
    return 1;
}


int
db5_type_descrip_from_codes(char **descrip, const int major, const int minor)
{
    register struct db5_type *tp;
    register int found_minors = 0;

    for (tp = (struct db5_type *) type_table;
	 tp->major_code != DB5_MAJORTYPE_RESERVED;
	 ++tp) {
	if (tp->major_code == major) {
	    if (tp->heed_minor)
		found_minors = 1;
	    if ((tp->minor_code == minor) || !found_minors) {
		*descrip = tp->description;
		return 0;
	    }
	}
    }
    return 1;
}


int
db5_type_codes_from_tag(int *major, int *minor, const char *tag)
{
    register struct db5_type *tp;

    for (tp = (struct db5_type *) type_table;
	 tp->major_code != DB5_MAJORTYPE_RESERVED;
	 ++tp) {
	if ((*(tp->tag) == *tag) && (BU_STR_EQUAL(tp->tag, tag))) {
	    *major = tp->major_code;
	    *minor = tp->minor_code;
	    return 0;
	}
    }
    return 1;
}


int
db5_type_codes_from_descrip(int *major, int *minor, const char *descrip)
{
    register struct db5_type *tp;

    for (tp = (struct db5_type *) type_table;
	 tp->major_code != DB5_MAJORTYPE_RESERVED;
	 ++tp) {
	if ((*(tp->description) == *descrip)
	    && (BU_STR_EQUAL(tp->description, descrip))) {
	    *major = tp->major_code;
	    *minor = tp->minor_code;
	    return 0;
	}
    }
    return 1;
}


size_t
db5_type_sizeof_h_binu(const int minor)
{
    switch (minor) {
	case DB5_MINORTYPE_BINU_FLOAT:
	    return sizeof(float);
	case DB5_MINORTYPE_BINU_DOUBLE:
	    return sizeof(double);
	case DB5_MINORTYPE_BINU_8BITINT:
	case DB5_MINORTYPE_BINU_8BITINT_U:
	    return (size_t) 1;
	case DB5_MINORTYPE_BINU_16BITINT:
	case DB5_MINORTYPE_BINU_16BITINT_U:
	    return (size_t) 2;
	case DB5_MINORTYPE_BINU_32BITINT:
	case DB5_MINORTYPE_BINU_32BITINT_U:
	    return (size_t) 4;
	case DB5_MINORTYPE_BINU_64BITINT:
	case DB5_MINORTYPE_BINU_64BITINT_U:
	    return (size_t) 8;
    }
    return 0;
}


size_t
db5_type_sizeof_n_binu(const int minor)
{
    switch (minor) {
	case DB5_MINORTYPE_BINU_FLOAT:
	    return (size_t) SIZEOF_NETWORK_FLOAT;
	case DB5_MINORTYPE_BINU_DOUBLE:
	    return (size_t) SIZEOF_NETWORK_DOUBLE;
	case DB5_MINORTYPE_BINU_8BITINT:
	case DB5_MINORTYPE_BINU_8BITINT_U:
	    return (size_t) 1;
	case DB5_MINORTYPE_BINU_16BITINT:
	case DB5_MINORTYPE_BINU_16BITINT_U:
	    return (size_t) 2;
	case DB5_MINORTYPE_BINU_32BITINT:
	case DB5_MINORTYPE_BINU_32BITINT_U:
	    return (size_t) 4;
	case DB5_MINORTYPE_BINU_64BITINT:
	case DB5_MINORTYPE_BINU_64BITINT_U:
	    return (size_t) 8;
    }
    return 0;
}


const char *
db5_standard_attribute(int idx)
{
    switch (idx) {
	case ATTR_REGION:
	    return "region";
	case ATTR_REGION_ID:
	    return "region_id";
	case ATTR_MATERIAL_ID:
	    return "material_id";
	case ATTR_AIR:
	    return "aircode";
	case ATTR_LOS:
	    return "los";
	case ATTR_COLOR:
	    return "color";
	case ATTR_SHADER:
	    return "shader";
	case ATTR_INHERIT:
	    return "inherit";
	case ATTR_NULL:
	    return NULL;
    }
    /* no match */
    return NULL;
}


int
db5_is_standard_attribute(const char *attr_want)
{
    int i = 0;
    const char *attr_have = NULL;

    if (!attr_want)
	return 0;

    for (i=0; (attr_have = db5_standard_attribute(i)) != NULL; i++) {
	if (BU_STR_EQUAL(attr_want, attr_have)) return 1;
    }

    return 0;
}


int
db5_standardize_attribute(const char *attr)
{
    /* FIXME: these should all be converted to case-insensitive
     * comparisions for the standard attribute names.
     */

    if (!attr)
	return ATTR_NULL;

    if (BU_STR_EQUAL(attr, "region"))
	return ATTR_REGION;
    if (BU_STR_EQUAL(attr, "REGION"))
	return ATTR_REGION;

    if (BU_STR_EQUAL(attr, "region_id"))
	return ATTR_REGION_ID;
    if (BU_STR_EQUAL(attr, "REGION_ID"))
	return ATTR_REGION_ID;
    if (BU_STR_EQUAL(attr, "id"))
	return ATTR_REGION_ID;
    if (BU_STR_EQUAL(attr, "ID"))
	return ATTR_REGION_ID;

    if (BU_STR_EQUAL(attr, "material_id"))
	return ATTR_MATERIAL_ID;
    if (BU_STR_EQUAL(attr, "MATERIAL_ID"))
	return ATTR_MATERIAL_ID;
    if (BU_STR_EQUAL(attr, "GIFTmater"))
	return ATTR_MATERIAL_ID;
    if (BU_STR_EQUAL(attr, "GIFT_MATERIAL"))
	return ATTR_MATERIAL_ID;
    if (BU_STR_EQUAL(attr, "mat"))
	return ATTR_MATERIAL_ID;

    if (BU_STR_EQUAL(attr, "air"))
	return ATTR_AIR;
    if (BU_STR_EQUAL(attr, "AIR"))
	return ATTR_AIR;
    if (BU_STR_EQUAL(attr, "AIRCODE"))
	return ATTR_AIR;
    if (BU_STR_EQUAL(attr, "aircode"))
	return ATTR_AIR;


    if (BU_STR_EQUAL(attr, "los"))
	return ATTR_LOS;
    if (BU_STR_EQUAL(attr, "LOS"))
	return ATTR_LOS;

    if (BU_STR_EQUAL(attr, "color"))
	return ATTR_COLOR;
    if (BU_STR_EQUAL(attr, "COLOR"))
	return ATTR_COLOR;
    if (BU_STR_EQUAL(attr, "rgb"))
	return ATTR_COLOR;
    if (BU_STR_EQUAL(attr, "RGB"))
	return ATTR_COLOR;

    if (BU_STR_EQUAL(attr, "shader"))
	return ATTR_SHADER;
    if (BU_STR_EQUAL(attr, "oshader"))
	return ATTR_SHADER;
    if (BU_STR_EQUAL(attr, "SHADER"))
	return ATTR_SHADER;

    if (BU_STR_EQUAL(attr, "inherit"))
	return ATTR_INHERIT;
    if (BU_STR_EQUAL(attr, "INHERIT"))
	return ATTR_INHERIT;

    return ATTR_NULL;
}


int
db5_is_boolean_attribute(int attr) {
    if (attr == ATTR_REGION) 
	return 1;
    if (attr == ATTR_INHERIT)
	return 1;
    return 0;
}


void
db_attr_add(struct bu_attribute_value_set *newavs, int attr_type, const char *stdattr, const char *value)
{   
    if (db5_is_boolean_attribute(attr_type)) {
	if (bu_str_true(value)) {
	    /* Use R for region, otherwise go with 1 */
	    if (attr_type == ATTR_REGION) {
		(void)bu_avs_add(newavs, stdattr, "R\0");
	    } else {
		(void)bu_avs_add(newavs, stdattr, "1\0");
	    }
	} else {
	    (void)bu_avs_remove(newavs, stdattr);
	}
    } else {
	(void)bu_avs_add(newavs, stdattr, value);
    }
}


size_t
db5_standardize_avs(struct bu_attribute_value_set *avs)
{
    size_t conflict = 0;
    int attr_type;

    const char *stdattr;
    const char *added;

    struct bu_attribute_value_set newavs;
    struct bu_attribute_value_pair *avpp;

    /* check inputs */
    BU_CK_AVS(avs);
    if (avs->count <= 0)
	return 0;

    bu_avs_init_empty(&newavs);

    /* FIRST PASS: identify any attributes that are already in
     * standard form since they take priority if there are duplicates
     * with different values.
     */
    for (BU_AVS_FOR(avpp, avs)) {
	/* see if this is a standarizable attribute name */
	attr_type = db5_standardize_attribute(avpp->name);

	/* get the standard name for this type */
	stdattr = db5_standard_attribute(attr_type);

	/* name is already in standard form, add it */
	if (attr_type != ATTR_NULL && BU_STR_EQUAL(stdattr, avpp->name))
	    (void)db_attr_add(&newavs, attr_type, stdattr, avpp->value);
    }

    /* SECOND PASS: check for duplicates and non-standard
     * attributes.
     */
    for (BU_AVS_FOR(avpp, avs)) {
	/* see if this is a standarizable attribute name */
	attr_type = db5_standardize_attribute(avpp->name);

	/* get the standard name for this type */
	stdattr = db5_standard_attribute(attr_type);

	/* see if we already added this attribute */
	added = bu_avs_get(&newavs, stdattr);

	if (attr_type != ATTR_NULL && added == NULL) {

	    /* case 1: name is "standardizable" and not added */

	    (void)db_attr_add(&newavs, attr_type, stdattr, avpp->value);
	} else if (attr_type != ATTR_NULL && BU_STR_EQUAL(added, avpp->value)) {

	    /* case 2: name is "standardizable", but we already added the same value */

	    /* ignore/skip it (because it's the same value) */
	} else if (attr_type != ATTR_NULL && !BU_STR_EQUAL(added, avpp->value)) {

	    /* case 3: name is "standardizable", but we already added something else */

	    /* preserve the conflict, keep the old value too */
	    (void)db_attr_add(&newavs, attr_type, avpp->name, avpp->value);
	    conflict++;
	} else {

	    /* everything else: add it */

	    (void)db_attr_add(&newavs, attr_type, avpp->name, avpp->value);
	}
    }
    bu_avs_free(avs);
    bu_avs_merge(avs, &newavs);
    bu_avs_free(&newavs);

    return conflict;
}


void
db5_sync_attr_to_comb(struct rt_comb_internal *comb, const struct bu_attribute_value_set *avs, const char *name)
{
    int ret;
    size_t i;
    long int attr_num_val;
    /*double attr_float_val;*/
    char *endptr;
    int color[3] = {-1, -1, -1};
    struct bu_vls newval;
    bu_vls_init(&newval);

    /* check inputs */
    RT_CK_COMB(comb);

    /* region flag */
    bu_vls_sprintf(&newval, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_REGION)));
    bu_vls_trimspace(&newval);
    if (bu_str_true(bu_vls_addr(&newval))) {
	comb->region_flag = 1;
    } else {
	comb->region_flag = 0;
    }

    /* region_id */
    bu_vls_sprintf(&newval, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_REGION_ID)));
    bu_vls_trimspace(&newval);
    if (bu_vls_strlen(&newval) != 0 && !BU_STR_EQUAL(bu_vls_addr(&newval), "(null)") && !BU_STR_EQUAL(bu_vls_addr(&newval), "del")) {
	attr_num_val = strtol(bu_vls_addr(&newval), &endptr, 0);
	if (endptr == bu_vls_addr(&newval) + strlen(bu_vls_addr(&newval))) {
	    comb->region_id = attr_num_val;
	} else {
	    bu_log("WARNING: [%s] has invalid region_id value [%s]\nregion_id remains at %d\n", name, bu_vls_addr(&newval), comb->region_id);
	}
    } else {
	/* remove region_id  */
	comb->region_id = 0;
    }

    /* material_id */
    bu_vls_sprintf(&newval, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_MATERIAL_ID)));
    bu_vls_trimspace(&newval);
    if (bu_vls_strlen(&newval) != 0 && !BU_STR_EQUAL(bu_vls_addr(&newval), "(null)") && !BU_STR_EQUAL(bu_vls_addr(&newval), "del")) {
	attr_num_val = strtol(bu_vls_addr(&newval), &endptr, 0);
	if (endptr == bu_vls_addr(&newval) + strlen(bu_vls_addr(&newval))) {
	    comb->GIFTmater = attr_num_val;
	} else {
	    bu_log("WARNING: [%s] has invalid material_id value [%s]\nmateriel_id remains at %d\n", name, bu_vls_addr(&newval), comb->GIFTmater);
	}
    } else {
	/* empty - set to zero */
	comb->GIFTmater = 0;
    }

    /* aircode */
    bu_vls_sprintf(&newval, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_AIR)));
    bu_vls_trimspace(&newval);
    if (bu_vls_strlen(&newval) != 0 && !BU_STR_EQUAL(bu_vls_addr(&newval), "(null)") && !BU_STR_EQUAL(bu_vls_addr(&newval), "del")) {
	attr_num_val = strtol(bu_vls_addr(&newval), &endptr, 0);
	if (endptr == bu_vls_addr(&newval) + strlen(bu_vls_addr(&newval))) {
	    comb->aircode = attr_num_val;
	} else {
	    bu_log("WARNING: [%s] has invalid aircode value [%s]\naircode remains at %d\n", name, bu_vls_addr(&newval), comb->aircode);
	}
    } else {
	/* not air */
	comb->aircode = 0;
    }

    /* los */
    bu_vls_sprintf(&newval, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_LOS)));
    bu_vls_trimspace(&newval);
    if (bu_vls_strlen(&newval) != 0 && !BU_STR_EQUAL(bu_vls_addr(&newval), "(null)") && !BU_STR_EQUAL(bu_vls_addr(&newval), "del")) {
	/* Currently, struct rt_comb_internal lists los as a long.  Probably should allow
	 * floating point, but as it's DEPRECATED anyway I suppose we can wait for that? */ 
	/* attr_float_val = strtod(bu_vls_addr(&newval), &endptr); */
	attr_num_val = strtol(bu_vls_addr(&newval), &endptr, 0);
	if (endptr == bu_vls_addr(&newval) + strlen(bu_vls_addr(&newval))) {
	    comb->los = attr_num_val;
	} else {
	    bu_log("WARNING: [%s] has invalid los value\nlos remains at %d\n", name, bu_vls_addr(&newval), comb->los);
	}
    } else {
	/* no los */
	comb->los = 0;
    }


    /* color */
    bu_vls_sprintf(&newval, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_COLOR)));
    bu_vls_trimspace(&newval);
    if (bu_vls_strlen(&newval) != 0 && !BU_STR_EQUAL(bu_vls_addr(&newval), "(null)") && !BU_STR_EQUAL(bu_vls_addr(&newval), "del")) {
	ret = sscanf(bu_vls_addr(&newval), "%3i%*c%3i%*c%3i", color+0, color+1, color+2);
	if (ret == 3) {
	    if (color[0] < 0 || color[1] < 0 || color[2] < 0) {
		comb->rgb_valid = 0;
	    } else {
		for (i = 0; i < 3; i++) {
		    if (color[i] > 255) color[i] = 255;
		}
		comb->rgb[0] = color[0];
		comb->rgb[1] = color[1];
		comb->rgb[2] = color[2];
		comb->rgb_valid = 1;
	    }
	} else if (ret == 1 && color[0] < 0) {
	    comb->rgb_valid = 0;
	} else {
	    if (comb->rgb_valid) {
		bu_log("WARNING: [%s] color does not match an R/G/B pattern\nColor remains unchanged at %d/%d/%d\n", name, V3ARGS(comb->rgb));
	    }
	}
    } else {
	comb->rgb_valid = 0;
    }

    /* shader */
    bu_vls_sprintf(&newval, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_SHADER)));
    bu_vls_trimspace(&newval);
    if (bu_vls_strlen(&newval) != 0 && !BU_STR_EQUAL(bu_vls_addr(&newval), "(null)") && !BU_STR_EQUAL(bu_vls_addr(&newval), "del")) {
	bu_vls_sprintf(&comb->shader, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_SHADER)));
    } else {
	bu_vls_trunc(&comb->shader, 0);
    }

    /* inherit */
    bu_vls_sprintf(&newval, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_INHERIT)));
    if (bu_str_true(bu_vls_addr(&newval))) {
	comb->inherit = 1;
    } else {
	comb->inherit = 0;
    }

    bu_vls_free(&newval);
}


void
db5_sync_comb_to_attr( struct bu_attribute_value_set *avs, const struct rt_comb_internal *comb)
{
    struct bu_vls newval;

    /* check inputs */
    RT_CK_COMB(comb);
    bu_vls_init(&newval);

    /* Region */
    if (comb->region_flag) {
	(void)bu_avs_add(avs, db5_standard_attribute(ATTR_REGION), "R");
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_REGION));
    }

    /* Region ID */
    if (comb->region_flag) {
	bu_vls_sprintf(&newval, "%d", comb->region_id);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_REGION_ID), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_REGION_ID));
    }

    /* Material ID */
    if (comb->GIFTmater != 0) {
	bu_vls_sprintf(&newval, "%d", comb->GIFTmater);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_MATERIAL_ID), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_MATERIAL_ID));
    }

    /* Air */
    if (comb->aircode) {
	bu_vls_sprintf(&newval, "%d", comb->aircode);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_AIR), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_AIR));
    }

    /* LOS */
    if (comb->los) {
	bu_vls_sprintf(&newval, "%d", comb->los);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_LOS), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_LOS));
    }

    /* Color */
    if (comb->rgb_valid) {
	bu_vls_sprintf(&newval, "%d/%d/%d", comb->rgb[0], comb->rgb[1], comb->rgb[2]);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_COLOR), &newval);
    }

    /* Shader - may be redundant */
    if (bu_vls_strlen(&comb->shader) != 0 && !BU_STR_EQUAL(bu_vls_addr(&newval), "(null)")) {
	bu_vls_sprintf(&newval, "%s", bu_vls_addr(&comb->shader));
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_SHADER), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_SHADER));
    }

    /* Inherit */
    if (comb->inherit) {
	bu_vls_sprintf(&newval, "%d", comb->inherit);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_INHERIT), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_INHERIT));
    }
    bu_vls_free(&newval);
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
