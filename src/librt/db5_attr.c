/*                     D B 5 _ A T T R . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2013 United States Government as represented by
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
/** @file librt/db5_attr.c
 *
 * Properties and functions related to standard DB5 object attributes
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

/* this is the master source of standard and registered attribute information */
const struct db5_attr_ctype db5_attr_std[] = {
    { ATTR_REGION, 0, ATTR_STANDARD,
	"region",
	"boolean",
	"Yes, R, 1, 0", /* example */
	"", /* aliases, if any */
	"Region Flag", /* property, if any */
	/* long_description, if any: */
	"The Region Flag identifies a particular geometric combination as being a solid material; in other words, any geometry below this combination in the tree can overlap without the overlap being regarded as a non-physical description, since it is the combination of all descriptions in the region object that defines the physical volume in space."

    },
    { ATTR_REGION_ID, 0, ATTR_STANDARD,
	"region_id",
	"an integer",
	"0, -1, and positive integers", /* examples */
	"id",  /* aliases, if any */
	"Region Identifier Number",  /* property, if any */
	/* long_description, if any: */
	"The Region Identifier Number identifies a particular region with a unique number. This allows multiple region objects to be regarded as being the same type of region, without requiring that they be included in the same combination object."
    },
    { ATTR_MATERIAL_ID, 0, ATTR_STANDARD,
	"material_id",
	"zero or positive integer (user-defined)",
	"", /* examples */
	"giftmater,mat",  /* aliases, if any */
	"Material Identifier Number",  /* property, if any */
	/* long_description, if any: */
	"The Material ID Number corresponds to an entry in a DENSITIES table, usually contained in a text file.  This table associates numbers with material names and density information used by analytical programs such as 'rtweight'."
    },
    { ATTR_AIR, 0, ATTR_STANDARD,
	"aircode",
	"an integer (application defined)",
	"'0', '1', or '-2'", /* examples */
	"air",  /* aliases, if any */
	"Air Code",  /* property, if any */
	/* long_description, if any: */
	"Any non-zero Air Code alerts the raytracer that the region in question is modeling air which is handled by specialized rules in LIBRT."
    },
    { ATTR_LOS, 0, ATTR_STANDARD,
	"los",
	"an integer in the inclusive range: 0 to 100",
	"'24' or '100'", /* examples */
	"",  /* aliases, if any */
	"Line of Sight Thickness Equivalence",  /* property, if any */
	/* long_description, if any: */
	""
    },
    { ATTR_COLOR, 0, ATTR_STANDARD,
	"color",
	"a 3-tuple of RGB values",
	"\"0 255 255\"", /* examples */
	"rgb",  /* aliases, if any */
	"Color",  /* property, if any */
	""  /* long_description, if any */
    },
    { ATTR_SHADER, 0, ATTR_STANDARD,
	"shader",
	"a string of shader characteristics in a standard format",
	"", /* examples */
	"oshader",  /* aliases, if any */
	"Shader Name",  /* property, if any */
	/* long_description, if any: */
	"LIBRT can use a variety of shaders when rendering.  This attribute holds a text string which corresponds to the name and other details of the shader to be used."
    },
    { ATTR_INHERIT, 0, ATTR_STANDARD,
	"inherit",
	"boolean",
	"Yes, 1, 0", /* examples */
	"",  /* aliases, if any */
	"Inherit Properties",  /* property, if any */
	/* long_description, if any: */
	"The Inherit Properties value, if true, indicates all child objects inherit the attributes of this parent object."
    },
    { ATTR_TIMESTAMP, 1, ATTR_STANDARD, /* first binary attribute */
	"mtime",

	"a binary time stamp for an object's last mod time (the time is displayed in human-readable form with the 'attr' command)",

	"", /* examples */
	"timestamp,time_stamp,modtime,mod_time",  /* aliases, if any */
	"Time Stamp",  /* property, if any */
	/* long_description, if any: */
	""
    },
    { ATTR_NULL, 0, ATTR_UNKNOWN_ORIGIN, "", "", "", "", "", "" }
};

const char *
db5_standard_attribute(int idx)
{
    if (idx >= ATTR_NULL) return NULL;
    return db5_attr_std[idx].name;
}

int
db5_is_standard_attribute(const char *attr_want)
{
    int i = 0;
    const char *attr_have = NULL;

    if (!attr_want) return 0;

    for (i = 0; (attr_have = db5_standard_attribute(i)) != NULL; i++) {
	if (BU_STR_EQUIV(attr_want, attr_have)) return 1;
    }

    return 0;
}


int
db5_standardize_attribute(const char *attr)
{
    int curr_attr = 0;
    struct bu_vls alias;
    const char *curr_pos, *next_pos;
    if (!attr) return ATTR_NULL;
    bu_vls_init(&alias);
    while (db5_attr_std[curr_attr].attr_type != ATTR_NULL) {
	curr_pos = db5_attr_std[curr_attr].aliases;
	while (curr_pos && strlen(curr_pos) > 0) {
	    next_pos = strchr(curr_pos+1, ',');
	    if (next_pos) {
		bu_vls_strncpy(&alias, curr_pos, next_pos - curr_pos);
		bu_log("found %s\n", bu_vls_addr(&alias));
		if (BU_STR_EQUIV(attr, bu_vls_addr(&alias))) {
		    bu_vls_free(&alias);
		    return db5_attr_std[curr_attr].attr_type;
		}
		curr_pos = next_pos + 1;
	    } else {
		bu_vls_strncpy(&alias, curr_pos, strlen(curr_pos));
		bu_log("found %s\n", bu_vls_addr(&alias));
		if (BU_STR_EQUIV(attr, bu_vls_addr(&alias))) {
		    bu_vls_free(&alias);
		    return db5_attr_std[curr_attr].attr_type;
		}
		curr_pos = NULL;
	    }
	}
	curr_attr++;
    }
    bu_vls_free(&alias);
    return ATTR_NULL;
}


HIDDEN
int
boolean_attribute(int attr) {
    if (attr == ATTR_REGION)
	return 1;
    if (attr == ATTR_INHERIT)
	return 1;
    return 0;
}


HIDDEN
void
attr_add(struct bu_attribute_value_set *newavs, int attr_type, const char *stdattr, const char *value)
{
    if (boolean_attribute(attr_type)) {
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
	/* see if this is a standardizable attribute name */
	attr_type = db5_standardize_attribute(avpp->name);

	/* get the standard name for this type */
	stdattr = db5_standard_attribute(attr_type);

	/* name is already in standard form, add it */
	if (attr_type != ATTR_NULL && BU_STR_EQUAL(stdattr, avpp->name))
	    (void)attr_add(&newavs, attr_type, stdattr, avpp->value);
    }

    /* SECOND PASS: check for duplicates and non-standard
     * attributes.
     */
    for (BU_AVS_FOR(avpp, avs)) {
	/* see if this is a standardizable attribute name */
	attr_type = db5_standardize_attribute(avpp->name);

	/* get the standard name for this type */
	stdattr = db5_standard_attribute(attr_type);

	/* see if we already added this attribute */
	added = bu_avs_get(&newavs, stdattr);

	if (attr_type != ATTR_NULL && added == NULL) {

	    /* case 1: name is "standardizable" and not added */

	    (void)attr_add(&newavs, attr_type, stdattr, avpp->value);
	} else if (attr_type != ATTR_NULL && BU_STR_EQUAL(added, avpp->value)) {

	    /* case 2: name is "standardizable", but we already added the same value */

	    /* ignore/skip it (because it's the same value) */
	} else if (attr_type != ATTR_NULL && !BU_STR_EQUAL(added, avpp->value)) {

	    /* case 3: name is "standardizable", but we already added something else */

	    /* preserve the conflict, keep the old value too */
	    (void)attr_add(&newavs, attr_type, avpp->name, avpp->value);
	    conflict++;
	} else {

	    /* everything else: add it */

	    (void)attr_add(&newavs, attr_type, avpp->name, avpp->value);
	}
    }
    bu_avs_free(avs);
    bu_avs_merge(avs, &newavs);
    bu_avs_free(&newavs);

    return conflict;
}


void
db5_sync_attr_to_comb(struct rt_comb_internal *comb, const struct bu_attribute_value_set *avs, struct directory *dp)
{
    int ret;
    size_t i;
    long int attr_num_val;
    /*double attr_float_val;*/
    char *endptr = NULL;
    int color[3] = {-1, -1, -1};
    struct bu_vls newval = BU_VLS_INIT_ZERO;
    char *name = dp->d_namep;

    /* check inputs */
    RT_CK_COMB(comb);

    /* region flag */
    bu_vls_sprintf(&newval, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_REGION)));
    bu_vls_trimspace(&newval);
    if (bu_str_true(bu_vls_addr(&newval))) {
	/* set region bit */
	dp->d_flags |= RT_DIR_REGION;
	comb->region_flag = 1;
    } else {
	/* unset region bit */
	dp->d_flags &= ~RT_DIR_REGION;
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
	    bu_log("WARNING: [%s] has invalid region_id value [%s]\nregion_id remains at %ld\n", name, bu_vls_addr(&newval), comb->region_id);
	}
    } else {
	/* remove region_id */
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
	    bu_log("WARNING: [%s] has invalid material_id value [%s]\nmateriel_id remains at %ld\n", name, bu_vls_addr(&newval), comb->GIFTmater);
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
	    bu_log("WARNING: [%s] has invalid aircode value [%s]\naircode remains at %ld\n", name, bu_vls_addr(&newval), comb->aircode);
	}
    } else {
	/* not air */
	comb->aircode = 0;
    }

    /* los */
    bu_vls_sprintf(&newval, "%s", bu_avs_get(avs, db5_standard_attribute(ATTR_LOS)));
    bu_vls_trimspace(&newval);
    if (bu_vls_strlen(&newval) != 0
	&& !BU_STR_EQUAL(bu_vls_addr(&newval), "(null)")
	&& !BU_STR_EQUAL(bu_vls_addr(&newval), "del")) {
	/* Currently, struct rt_comb_internal lists los as a long.  Probably should allow
	 * floating point, but as it's DEPRECATED anyway I suppose we can wait for that? */
	/* attr_float_val = strtod(bu_vls_addr(&newval), &endptr); */
	attr_num_val = strtol(bu_vls_addr(&newval), &endptr, 0);
	if (endptr == bu_vls_addr(&newval) + strlen(bu_vls_addr(&newval))) {
	    comb->los = attr_num_val;
	} else {
	    bu_log("WARNING: [%s] has invalid los value %s\nlos remains at %ld\n",
		   name, bu_vls_addr(&newval), comb->los);
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
db5_sync_comb_to_attr(struct bu_attribute_value_set *avs, const struct rt_comb_internal *comb)
{
    struct bu_vls newval = BU_VLS_INIT_ZERO;

    /* check inputs */
    RT_CK_COMB(comb);

    /* Region */
    if (comb->region_flag) {
	(void)bu_avs_add(avs, db5_standard_attribute(ATTR_REGION), "R");
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_REGION));
    }

    /* Region ID */
    if (comb->region_flag) {
	bu_vls_sprintf(&newval, "%ld", comb->region_id);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_REGION_ID), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_REGION_ID));
    }

    /* Material ID */
    if (comb->GIFTmater != 0) {
	bu_vls_sprintf(&newval, "%ld", comb->GIFTmater);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_MATERIAL_ID), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_MATERIAL_ID));
    }

    /* Air */
    if (comb->aircode) {
	bu_vls_sprintf(&newval, "%ld", comb->aircode);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_AIR), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_AIR));
    }

    /* LOS */
    if (comb->los) {
	bu_vls_sprintf(&newval, "%ld", comb->los);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_LOS), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_LOS));
    }

    /* Color */
    if (comb->rgb_valid) {
	bu_vls_sprintf(&newval, "%d/%d/%d", comb->rgb[0], comb->rgb[1], comb->rgb[2]);
	(void)bu_avs_add_vls(avs, db5_standard_attribute(ATTR_COLOR), &newval);
    } else {
	bu_avs_remove(avs, db5_standard_attribute(ATTR_COLOR));
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
