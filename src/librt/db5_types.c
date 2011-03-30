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
/** @file db5_types.c
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

/**
 * Define standard attribute types in BRL-CAD geometry. (See the
 * gattributes manual page) these should be a collective enumeration
 * starting from 0 and increasing without any gaps in the numbers so
 * db5_standard_attribute() can be used as an index-based iterator.
 */

enum {
    ATTR_REGION = 0,
    ATTR_REGION_ID,
    ATTR_MATERIAL_ID,
    ATTR_AIR,
    ATTR_LOS,
    ATTR_COLOR,
    ATTR_SHADER,
    ATTR_INHERIT,
    ATTR_NULL
};


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
	    return "air";
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

    for (i=0; (attr_have = db5_standard_attribute(i)) != NULL; i++) {
	if (BU_STR_EQUAL(attr_want, attr_have)) return 1;
    }

    return 0;
}


/**
 * D B 5 _ S T A N D A R D I Z E _ A T T R I B U T E
 *
 * Function for recognizing various versions of the DB5 standard
 * attribute names that have been used - returns the attribute type
 * of the supplied attribute name, or -1 if it is not a recognized
 * variation of the standard attributes.
 *
 */
size_t
db5_standardize_attribute(const char *attrname)
{
    size_t i;
    char *region_flag_names[2];
    char *region_id_names[4];
    char *material_id_names[5];
    char *air_names[3];
    char *los_names[2];
    char *color_names[4];
    char *shader_names[2];
    char *inherit_names[2];
    region_flag_names[0] = "region";
    region_flag_names[1] = "REGION";
    region_id_names[0] = "region_id";
    region_id_names[1] = "REGION_ID";
    region_id_names[2] = "id";
    region_id_names[3] = "ID";
    material_id_names[0] = "material_id";
    material_id_names[1] = "MATERIAL_ID";
    material_id_names[2] = "GIFTmater";
    material_id_names[3] = "GIFT_MATERIAL";
    material_id_names[4] = "mat";
    los_names[0] = "los";
    los_names[1] = "LOS";
    air_names[0] = "air";
    air_names[1] = "AIR";
    air_names[2] = "AIRCODE";
    color_names[0] = "color";
    color_names[1] = "rgb";
    color_names[2] = "RGB";
    color_names[3] = "COLOR";
    shader_names[0] = "oshader";
    shader_names[1] = "SHADER";
    inherit_names[0] = "inherit";
    inherit_names[1] = "INHERIT";

    for (i = 0; i < sizeof(region_flag_names)/sizeof(char *); i++) {
	if (BU_STR_EQUAL(attrname, region_flag_names[i])) return ATTR_REGION;
    }

    for (i = 0; i < sizeof(region_id_names)/sizeof(char *); i++) {
	if (BU_STR_EQUAL(attrname, region_id_names[i])) return ATTR_REGION_ID;
    }

    for (i = 0; i < sizeof(material_id_names)/sizeof(char *); i++) {
	if (BU_STR_EQUAL(attrname, material_id_names[i])) return ATTR_MATERIAL_ID;
    }

    for (i = 0; i < sizeof(air_names)/sizeof(char *); i++) {
	if (BU_STR_EQUAL(attrname, air_names[i])) return ATTR_AIR;
    }

    for (i = 0; i < sizeof(los_names)/sizeof(char *); i++) {
	if (BU_STR_EQUAL(attrname, los_names[i])) return ATTR_LOS;
    }

    for (i = 0; i < sizeof(color_names)/sizeof(char *); i++) {
	if (BU_STR_EQUAL(attrname, color_names[i])) return ATTR_COLOR;
    }

    for (i = 0; i < sizeof(shader_names)/sizeof(char *); i++) {
	if (BU_STR_EQUAL(attrname, shader_names[i])) return ATTR_SHADER;
    }

    for (i = 0; i < sizeof(inherit_names)/sizeof(char *); i++) {
	if (BU_STR_EQUAL(attrname, inherit_names[i])) return ATTR_INHERIT;
    }

    return -1;

}


/**
 * D B 5 _ S T A N D A R D I Z E _ A V S
 *
 * Ensures that an attribute set containing one or more standard
 * attributes, for every attribute type present one of the AV
 * pairs conforms to modern naming conventions.  It will not remove
 * other attributes of the same type, but will warn if they are found.
 *
 * @file: db5_types.c
 */
void
db5_standardize_avs(struct bu_attribute_value_set *avs)
{
    size_t i, attr_type;
    struct bu_attribute_value_set avstmp;
    struct bu_attribute_value_pair *avpp;
    int has_standard[8];
    bu_avs_init_empty(&avstmp);
    avpp = avs->avp;
    for (i=0; i < 8; i++) {
	has_standard[i] = 0;
    }
    for (i=0; i < avs->count; i++, avpp++) {
	if (BU_STR_EQUAL(avpp->name, "region")) has_standard[ATTR_REGION] = 1;
	if (BU_STR_EQUAL(avpp->name, "region_id")) has_standard[ATTR_REGION_ID] = 1;
	if (BU_STR_EQUAL(avpp->name, "material_id")) has_standard[ATTR_MATERIAL_ID] = 1;
	if (BU_STR_EQUAL(avpp->name, "air")) has_standard[ATTR_AIR] = 1;
	if (BU_STR_EQUAL(avpp->name, "los")) has_standard[ATTR_LOS] = 1;
	if (BU_STR_EQUAL(avpp->name, "color")) has_standard[ATTR_COLOR] = 1;
	if (BU_STR_EQUAL(avpp->name, "oshader")) has_standard[ATTR_SHADER] = 1;
	if (BU_STR_EQUAL(avpp->name, "inherit")) has_standard[ATTR_INHERIT] = 1;
    }

    avpp = avs->avp;
    for (i=0; i < avs->count; i++, avpp++) {
	attr_type = db5_standardize_attribute(avpp->name);
	switch (attr_type) {
	    case ATTR_REGION:
		/* In the case of regions, values like Yes and 1 are causing trouble
		 * somewhere in the code.  Do "R" for all affirmative cases and
		 * strip any non-affirmative cases out of the avs */
		if (bu_str_true(avpp->value)) {
		    (void)bu_avs_add(&avstmp, "region", "R");
		    has_standard[ATTR_REGION] = 1;
		}
		break;
	    case ATTR_REGION_ID:
		if (has_standard[ATTR_REGION_ID] != 1) {
		    (void)bu_avs_add(&avstmp, "region_id", bu_strdup(avpp->value));
		    has_standard[ATTR_REGION_ID] = 1;
		} else {
		    (void)bu_avs_add(&avstmp, bu_strdup(avpp->name), bu_strdup(avpp->value));
		}
		break;
	    case ATTR_MATERIAL_ID:
		if (has_standard[ATTR_MATERIAL_ID] != 1) {
		    (void)bu_avs_add(&avstmp, "material_id", bu_strdup(avpp->value));
		    has_standard[ATTR_MATERIAL_ID] = 1;
		} else {
		    (void)bu_avs_add(&avstmp, bu_strdup(avpp->name), bu_strdup(avpp->value));
		}
		break;
	    case ATTR_AIR:
		if (has_standard[ATTR_AIR] != 1) {
		    (void)bu_avs_add(&avstmp, "air", bu_strdup(avpp->value));
		    has_standard[ATTR_AIR] = 1;
		} else {
		    (void)bu_avs_add(&avstmp, bu_strdup(avpp->name), bu_strdup(avpp->value));
		}
		break;
	    case ATTR_LOS:
		if (has_standard[ATTR_LOS] != 1) {
		    (void)bu_avs_add(&avstmp, "los", bu_strdup(avpp->value));
		    has_standard[ATTR_LOS] = 1;
		} else {
		    (void)bu_avs_add(&avstmp, bu_strdup(avpp->name), bu_strdup(avpp->value));
		}
		break;
	    case ATTR_COLOR:
		if (has_standard[ATTR_COLOR] != 1) {
		    (void)bu_avs_add(&avstmp, "color", bu_strdup(avpp->value));
		    has_standard[ATTR_COLOR] = 1;
		} else {
		    (void)bu_avs_add(&avstmp, bu_strdup(avpp->name), bu_strdup(avpp->value));
		}
		break;
	    case ATTR_SHADER:
		if (has_standard[ATTR_SHADER] != 1) {
		    (void)bu_avs_add(&avstmp, "oshader", bu_strdup(avpp->value));
		    has_standard[ATTR_SHADER] = 1;
		} else {
		    (void)bu_avs_add(&avstmp, bu_strdup(avpp->name), bu_strdup(avpp->value));
		}
		break;
	    case ATTR_INHERIT:
		if (has_standard[ATTR_INHERIT] != 1) {
		    (void)bu_avs_add(&avstmp, "inherit", bu_strdup(avpp->value));
		    has_standard[ATTR_INHERIT] = 1;
		} else {
		    (void)bu_avs_add(&avstmp, bu_strdup(avpp->name), bu_strdup(avpp->value));
		}
		break;
	    default:
		/* not a standard attribute, just copy it*/
		(void)bu_avs_add(&avstmp, bu_strdup(avpp->name), bu_strdup(avpp->value));
		break;
	}
    }
    bu_avs_free(avs);
    bu_avs_merge(avs, &avstmp);
    bu_avs_free(&avstmp);
}


void
db5_apply_std_attributes(struct db_i *dbip, struct directory *dp, struct rt_comb_internal *comb)
{
    size_t i;
    long attr_num_val;
    int color[3];
    struct bu_attribute_value_set avs;
    struct bu_vls newval;
    bu_vls_init(&newval);

    RT_CK_COMB(comb);
    bu_avs_init_empty(&avs);

    if (!db5_get_attributes(dbip, &avs, dp)) {

	db5_standardize_avs(&avs);

	/* region flag */
	bu_vls_sprintf(&newval, "%s", bu_avs_get(&avs, "region"));
	if (bu_str_true(bu_vls_addr(&newval))) {
	    comb->region_flag = 1;
	    dp->d_flags |= RT_DIR_REGION;
	} else {
	    comb->region_flag = 0;
	    dp->d_flags &= ~RT_DIR_REGION;
	}

	/* region_id */
	bu_vls_sprintf(&newval, "%s", bu_avs_get(&avs, "region_id"));
	attr_num_val = atoi(bu_vls_addr(&newval));
	if (attr_num_val >= 0 || attr_num_val == -1) {
	    comb->region_id = attr_num_val;
	} else {
	    bu_log("Warning - invalid region_id value on comb %s - comb->region_id remains at %d\n", dp->d_namep, comb->region_id);
	}

	/* material_id */
	bu_vls_sprintf(&newval, "%s", bu_avs_get(&avs, "material_id"));
	attr_num_val = atoi(bu_vls_addr(&newval));
	if (attr_num_val >= 0) {
	    comb->GIFTmater = attr_num_val;
	} else {
	    bu_log("Warning - invalid material_id value on comb %s - comb->GIFTmater remains at %d\n", dp->d_namep, comb->GIFTmater);
	}

	/* air */
	bu_vls_sprintf(&newval, "%s", bu_avs_get(&avs, "air"));
	attr_num_val = atoi(bu_vls_addr(&newval));
	if (attr_num_val == 0 || attr_num_val == 1) {
	    comb->aircode = attr_num_val;
	} else {
	    bu_log("Warning - invalid Air Code value on comb %s - comb->aircode remains at %d\n", dp->d_namep, comb->aircode);
	}

	/* los */
	bu_vls_sprintf(&newval, "%s", bu_avs_get(&avs, "los"));
	attr_num_val = atoi(bu_vls_addr(&newval)); /* Is LOS really limited to integer values?? - also, need some sanity checking */
	comb->los = attr_num_val;

	/* color */
	bu_vls_sprintf(&newval, "%s", bu_avs_get(&avs, "color"));
	if (bu_avs_get(&avs, "color")) {
	    if (sscanf(bu_vls_addr(&newval), "%i/%i/%i", color+0, color+1, color+2) == 3) {
		for (i = 0; i < 3; i++) {
		    if (color[i] > 255) color[i] = 255;
		    if (color[i] < 0) color[i] = 0;
		}
		comb->rgb[0] = color[0];
		comb->rgb[1] = color[1];
		comb->rgb[2] = color[2];
	    } else {
		bu_log("Warning - color string on comb %s does not match the R/G/B pattern - color remains at %d/%d/%d\n", dp->d_namep, comb->rgb[0], comb->rgb[1], comb->rgb[2]);
	    }
	}

	/* oshader */
	bu_vls_strcpy(&comb->shader, bu_avs_get(&avs, "oshader"));

	/* inherit */
	bu_vls_sprintf(&newval, "%s", bu_avs_get(&avs, "inherit"));
	if (bu_str_true(bu_vls_addr(&newval))) {
	    comb->inherit = 1;
	} else {
	    comb->inherit = 0;
	}

	db5_update_attributes(dp, &avs, dbip);
    }
    bu_vls_free(&newval);
}


void
db5_update_std_attributes(struct db_i *dbip, struct directory *dp, const struct rt_comb_internal *comb)
{
    struct bu_attribute_value_set avs;
    struct bu_vls newval;

    RT_CK_COMB(comb);
    bu_avs_init_empty(&avs);
    bu_vls_init(&newval);

    if (!db5_get_attributes(dbip, &avs, dp)) {
	db5_standardize_avs(&avs);
	if (comb->region_flag) {
	    (void)bu_avs_add(&avs, "region", "R");
	} else {
	    bu_avs_remove(&avs, "region");
	}
	if (comb->region_flag && (comb->region_id >=0 || comb->region_id == -1)) {
	    bu_vls_sprintf(&newval, "%d", comb->region_id);
	    (void)bu_avs_add_vls(&avs, "region_id", &newval);
	} else {
	    bu_avs_remove(&avs, "region_id");
	}
	if (comb->GIFTmater >= 0) {
	    bu_vls_sprintf(&newval, "%d", comb->GIFTmater);
	    (void)bu_avs_add_vls(&avs, "material_id", &newval);
	} else {
	    bu_avs_remove(&avs, "material_id");
	}
	if (comb->aircode) {
	    bu_vls_sprintf(&newval, "%d", comb->aircode);
	    (void)bu_avs_add_vls(&avs, "air", &newval);
	} else {
	    bu_avs_remove(&avs, "air");
	}
	if (comb->los) {
	    bu_vls_sprintf(&newval, "%d", comb->los);
	    (void)bu_avs_add_vls(&avs, "los", &newval);
	} else {
	    bu_avs_remove(&avs, "los");
	}
	if (bu_avs_get(&avs, "color") || !(comb->rgb[0] == 0 && comb->rgb[1] == 0 && comb->rgb[2] == 0)) {
	    bu_vls_sprintf(&newval, "%d/%d/%d", comb->rgb[0], comb->rgb[1], comb->rgb[2]);
	    (void)bu_avs_add_vls(&avs, "color", &newval);
	}
	if (!BU_STR_EQUAL(bu_vls_addr(&comb->shader), "")) {
	    bu_vls_sprintf(&newval, "%s", bu_vls_addr(&comb->shader));
	    (void)bu_avs_add_vls(&avs, "oshader", &newval);
	} else {
	    bu_avs_remove(&avs, "oshader");
	}
	if (comb->inherit) {
	    bu_vls_sprintf(&newval, "%d", comb->inherit);
	    (void)bu_avs_add_vls(&avs, "inherit", &newval);
	} else {
	    bu_avs_remove(&avs, "inherit");
	}
	db5_update_attributes(dp, &avs, dbip);
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
