/*                     D B 5 _ T Y P E S . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2010 United States Government as represented by
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
db5_type_tag_from_major(char **tag, const int major) {
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
db5_type_descrip_from_major(char **descrip, const int major) {
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
db5_type_tag_from_codes(char **tag, const int major, const int minor) {
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
db5_type_descrip_from_codes(char **descrip, const int major,
			    const int minor) {
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
db5_type_codes_from_tag(int *major, int *minor, const char *tag) {
    register struct db5_type *tp;


    for (tp = (struct db5_type *) type_table;
	 tp->major_code != DB5_MAJORTYPE_RESERVED;
	 ++tp) {
	if ((*(tp->tag) == *tag) && (strcmp(tp->tag, tag) == 0)) {
	    *major = tp->major_code;
	    *minor = tp->minor_code;
	    return 0;
	}
    }
    return 1;
}

int
db5_type_codes_from_descrip(int *major, int *minor, const char *descrip) {
    register struct db5_type *tp;


    for (tp = (struct db5_type *) type_table;
	 tp->major_code != DB5_MAJORTYPE_RESERVED;
	 ++tp) {
	if ((*(tp->description) == *descrip)
	    && (strcmp(tp->description, descrip) == 0)) {
	    *major = tp->major_code;
	    *minor = tp->minor_code;
	    return 0;
	}
    }
    return 1;
}

size_t
db5_type_sizeof_h_binu(const int minor) {
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
db5_type_sizeof_n_binu(const int minor) {
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

size_t
db5_is_standard_attribute(char *attrname) {
    int i;
    char *standard_attributes[8];
    standard_attributes[0] = "region";
    standard_attributes[1] = "region_id";
    standard_attributes[2] = "material_id";
    standard_attributes[3] = "los";
    standard_attributes[4] = "air";
    standard_attributes[5] = "color";
    standard_attributes[6] = "oshader";
    standard_attributes[7] = "inherit";
   
    for (i = 0; i < sizeof(standard_attributes)/sizeof(char *); i++) {
	if (strcmp(attrname, standard_attributes[i]) == 0) return 1;
    }
   
    return 0;    
}

size_t
db5_standardize_attribute(char *attrname) {
    int i;
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
	if (strcmp(attrname, region_flag_names[i]) == 0) return ATTR_REGION;
    }
    
    for (i = 0; i < sizeof(region_id_names)/sizeof(char *); i++) {
	if (strcmp(attrname, region_id_names[i]) == 0) return ATTR_REGION_ID;
    }
    
    for (i = 0; i < sizeof(material_id_names)/sizeof(char *); i++) {
	if (strcmp(attrname, material_id_names[i]) == 0) return ATTR_MATERIAL_ID;
    }
    
    for (i = 0; i < sizeof(air_names)/sizeof(char *); i++) {
	if (strcmp(attrname, air_names[i]) == 0) return ATTR_AIR;
    }
    
    for (i = 0; i < sizeof(los_names)/sizeof(char *); i++) {
	if (strcmp(attrname, los_names[i]) == 0) return ATTR_LOS;
    }
    
    for (i = 0; i < sizeof(color_names)/sizeof(char *); i++) {
	if (strcmp(attrname, color_names[i]) == 0) return ATTR_COLOR;
    }
    
    for (i = 0; i < sizeof(shader_names)/sizeof(char *); i++) {
	if (strcmp(attrname, shader_names[i]) == 0) return ATTR_SHADER;
    }
    
    for (i = 0; i < sizeof(inherit_names)/sizeof(char *); i++) {
	if (strcmp(attrname, inherit_names[i]) == 0) return ATTR_INHERIT;
    }

    return -1;    

}

void
db5_standardize_avs(struct bu_attribute_value_set *avs) {
    size_t i, attr_type, attr_val;
    struct bu_attribute_value_pair *avpp;
    int type_count[8], has_standard[8];
    avpp = avs->avp;
    for (i=0; i < 8; i++) {
	type_count[i] = 0;
        has_standard[i] = 0;
    }
    for (i=0; i < avs->count; i++, avpp++) {
        if (!strcmp(avpp->name, "region")) has_standard[ATTR_REGION] = 1;
        if (!strcmp(avpp->name, "region_id")) has_standard[ATTR_REGION_ID] = 1;
        if (!strcmp(avpp->name, "material_id")) has_standard[ATTR_MATERIAL_ID] = 1;
        if (!strcmp(avpp->name, "air")) has_standard[ATTR_AIR] = 1;
        if (!strcmp(avpp->name, "los")) has_standard[ATTR_LOS] = 1;
        if (!strcmp(avpp->name, "color")) has_standard[ATTR_COLOR] = 1;
        if (!strcmp(avpp->name, "oshader")) has_standard[ATTR_SHADER] = 1;
        if (!strcmp(avpp->name, "inherit")) has_standard[ATTR_INHERIT] = 1;
    }

    avpp = avs->avp;
    for (i=0; i < avs->count; i++, avpp++) {
	attr_type = db5_standardize_attribute(avpp->name);
        switch (attr_type) {
		case ATTR_REGION:
		     if (type_count[ATTR_REGION] == 0) {
		         if (has_standard[ATTR_REGION] != 1) {
			    (void)bu_avs_add(avs, "region", avpp->value);
			    if (strcmp(avpp->name, "region")) bu_avs_remove(avs, avpp->name);
		         }
			 type_count[ATTR_REGION] = 1;
		     } else {
			 bu_log("Warning - multiple region attributes detected\n");
		     }
		     break;
		case ATTR_REGION_ID:
		     if (type_count[ATTR_REGION_ID] == 0) {
		         if (has_standard[ATTR_REGION_ID] != 1) {
		             (void)bu_avs_add(avs, "region_id", avpp->value);
		             if (strcmp(avpp->name, "region_id")) bu_avs_remove(avs, avpp->name);
		         }
			 type_count[ATTR_REGION_ID] = 1;
		     } else {
			 bu_log("Warning - multiple region_id attributes detected\n");
		     }
		     break;
		case ATTR_MATERIAL_ID:
		     if (type_count[ATTR_MATERIAL_ID] == 0) {
		         if (has_standard[ATTR_MATERIAL_ID] != 1) {
		             (void)bu_avs_add(avs, "material_id", avpp->value);
		             if (strcmp(avpp->name, "material_id")) bu_avs_remove(avs, avpp->name);
		         }
			 type_count[ATTR_MATERIAL_ID] = 1;
		     } else {
			 bu_log("Warning - multiple material_id attributes detected\n");
		     }
		     break;
		case ATTR_AIR:
		     if (type_count[ATTR_AIR] == 0) {
		         if (has_standard[ATTR_AIR] != 1) {
		             (void)bu_avs_add(avs, "air", avpp->value);
		             if (strcmp(avpp->name, "air")) bu_avs_remove(avs, avpp->name);
		         }
			 type_count[ATTR_AIR] = 1;
		     } else {
			 bu_log("Warning - multiple air attributes detected\n");
		     }
		     break;
		case ATTR_LOS:
		     if (type_count[ATTR_LOS] == 0) {
		         if (has_standard[ATTR_LOS] != 1) {
		             (void)bu_avs_add(avs, "los", avpp->value);
		             if (strcmp(avpp->name, "los")) bu_avs_remove(avs, avpp->name);
		         }
			 type_count[ATTR_LOS] = 1;
		     } else {
			 bu_log("Warning - multiple los attributes detected\n");
		     }
		     break;
		case ATTR_COLOR:
		     if (type_count[ATTR_COLOR] == 0) {
		         if (has_standard[ATTR_COLOR] != 1) {
		             (void)bu_avs_add(avs, "color", avpp->value);
		             if (strcmp(avpp->name, "color")) bu_avs_remove(avs, avpp->name);
		         }
			 type_count[ATTR_COLOR] = 1;
		     } else {
			 bu_log("Warning - multiple color attributes detected\n");
		     }
		     break;
		case ATTR_SHADER:
		     if (type_count[ATTR_SHADER] == 0) {
		         if (has_standard[ATTR_SHADER] != 1) {
		            (void)bu_avs_add(avs, "oshader", avpp->value);
		            if (strcmp(avpp->name, "oshader")) bu_avs_remove(avs, avpp->name);
		         }
			 type_count[ATTR_SHADER] = 1;
		     } else {
			 bu_log("Warning - multiple shader attributes detected\n");
		     }
		     break;
		case ATTR_INHERIT:
		     if (type_count[ATTR_INHERIT] == 0) {
		         if (has_standard[ATTR_INHERIT] != 1) {
		            (void)bu_avs_add(avs, "inherit", avpp->value);
		            if (strcmp(avpp->name, "inherit")) bu_avs_remove(avs, avpp->name);
		         }
			 type_count[ATTR_INHERIT] = 1;
		     } else {
			 bu_log("Warning - multiple inherit attributes detected\n");
		     }
		     break;
		default:
		     /* not a standard attribute, no action */
		     break;
 	 }
    }
}

size_t
db5_apply_std_attributes(struct db_i *dbip, struct directory *dp, struct rt_comb_internal *comb) {

    size_t i, j, attr_type, attr_num_val;
    int color[3];
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;
    struct bu_vls newval;
    bu_vls_init(&newval);
 
    RT_CK_COMB(comb);
    bu_avs_init_empty(&avs);

    if (!db5_get_attributes(dbip, &avs, dp)) {

	db5_standardize_avs(&avs);

	/* region flag */
	bu_vls_sprintf(&newval, "%s", bu_avs_get(&avs, "region"));
        if (!strcmp(bu_vls_addr(&newval), "Yes") || !strcmp(bu_vls_addr(&newval), "R") || !strcmp(bu_vls_addr(&newval), "1") || 
		!strcmp(bu_vls_addr(&newval), "Y") || !strcmp(bu_vls_addr(&newval), "y") ) {
 	   comb->region_flag = 1;
	} else {
           comb->region_flag = 0;
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
	   if ( sscanf(bu_vls_addr(&newval), "%i/%i/%i", color+0, color+1, color+2) == 3 ) {
	      for (j = 0; j < 3; j++) {       
     	        if (comb->rgb[j] > 255) comb->rgb[j] = 255;
     	        if (comb->rgb[j] < 0) comb->rgb[j] = 0;
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
	if (!strcmp(bu_vls_addr(&newval), "Yes") || !strcmp(bu_vls_addr(&newval), "1") || 
		!strcmp(bu_vls_addr(&newval), "Y") || !strcmp(bu_vls_addr(&newval), "y") ) {
	   comb->inherit = 1;
	} else {
	   comb->inherit = 0;
	}
    
        db5_update_attributes(dp, &avs, dbip);
    }
    bu_vls_free(&newval);
}

size_t
db5_update_std_attributes(struct db_i *dbip, struct directory *dp, struct rt_comb_internal *comb) {
    size_t i, attr_type;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;
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
	for (i = 0; i < 3; i++) {
  	   if (comb->rgb[i] > 255) comb->rgb[i] = 255;
           if (comb->rgb[i] < 0) comb->rgb[i] = 0;
        }
        if (bu_avs_get(&avs, "color") || !(comb->rgb[0] == 0 && comb->rgb[1] == 0 && comb->rgb[2] == 0)) {
	   bu_vls_sprintf(&newval, "%d/%d/%d", comb->rgb[0], comb->rgb[1], comb->rgb[2]);
  	   (void)bu_avs_add_vls(&avs, "color", &newval); 
        }
        if (strcmp(bu_vls_addr(&comb->shader), "")) {
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
