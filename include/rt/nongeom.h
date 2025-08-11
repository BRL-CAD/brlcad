/*                      N O N G E O M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
/** @file rt/nongeom.h
 *
 * In memory format for non-geometry objects in BRL-CAD databases.
 */

#ifndef RT_NONGEOM_H
#define RT_NONGEOM_H

#include "common.h"
#include "vmath.h"
#include "bu/vls.h"

__BEGIN_DECLS

union tree;  /* forward declaration */

/**
 * In-memory format for database "combination" record (non-leaf node).
 * (Regions and Groups are both a kind of Combination).  Perhaps move
 * to wdb.h or rtgeom.h?
 */
struct rt_comb_internal {
    uint32_t            magic;
    union tree *        tree;           /**< @brief Leading to tree_db_leaf leaves */
    char                region_flag;    /**< @brief !0 ==> this COMB is a REGION */
    char                is_fastgen;     /**< @brief REGION_NON_FASTGEN/_PLATE/_VOLUME */
    /* Begin GIFT compatibility */
    long                region_id;      /* DEPRECATED, use attribute */
    long                aircode;        /* DEPRECATED, use attribute */
    long                GIFTmater;      /* DEPRECATED, use attribute */
    long                los;            /* DEPRECATED, use attribute */
    /* End GIFT compatibility */
    char                rgb_valid;      /**< @brief !0 ==> rgb[] has valid color */
    unsigned char       rgb[3];
    float               temperature;    /**< @brief > 0 ==> region temperature */
    struct bu_vls       shader;
    struct bu_vls       material;
    char                inherit;

    /* This holds the pointer to the parent database from which the comb
     * definition has come.  Combs are unlike most other BRL-CAD geometry
     * definitions in that they are not self-contained - an rt_db_internal
     * representation of a comb does not (by itself) have enough information
     * for operations like plotting or tessellation.  Hence, we store a pointer
     * to the database as internal data of the comb, since other data contained
     * in the database is generally an integral part of making the comb
     * definition meaningful.
     *
     * Note that this has implications for how a comb rt_db_internal must be
     * treated.  Most rt_db_internal objects are independent of the original .g
     * database they were loaded from, and can be used with a different dbip
     * without worry.  That is not true for combs - if a comb is to be copied
     * to a new database, a new comb instance MUST be opened once the comb data
     * is established in the new database context.  Without the old dbip being
     * valid, comb methods such as tess and plot will NOT work.
     *
     * Generally speaking this pointer should only be used internally for
     * functab methods and not by calling codes.  If there is another way to
     * obtain dbip other than referencing this variable, that's what callers
     * should do.  Use this ONLY when you're sure it is the only way to get the
     * job done, and ALWAYS validate it before use with RT_CK_DBI. */
    const struct db_i  *src_dbip;

    /* Name of the original database object that produced the comb.  If the
     * database has changed this name may no longer identify a current object
     * in the database - the caller is responsible for validating it. */
    const char *src_objname;
};
#define RT_CHECK_COMB(_p) BU_CKMAG(_p, RT_COMB_MAGIC, "rt_comb_internal")
#define RT_CK_COMB(_p) RT_CHECK_COMB(_p)

/**
 * initialize an rt_comb_internal to empty.
 */
#define RT_COMB_INTERNAL_INIT(_p) { \
	(_p)->magic = RT_COMB_MAGIC; \
	(_p)->tree = TREE_NULL; \
	(_p)->region_flag = 0; \
	(_p)->is_fastgen = REGION_NON_FASTGEN; \
	(_p)->region_id = 0; \
	(_p)->aircode = 0; \
	(_p)->GIFTmater = 0; \
	(_p)->los = 0; \
	(_p)->rgb_valid = 0; \
	(_p)->rgb[0] = 0; \
	(_p)->rgb[1] = 0; \
	(_p)->rgb[2] = 0; \
	(_p)->temperature = 0.0; \
	BU_VLS_INIT(&(_p)->shader); \
	BU_VLS_INIT(&(_p)->material); \
	(_p)->inherit = 0; \
	(_p)->src_dbip = 0; \
	(_p)->src_objname = 0; \
    }

/**
 * deinitialize an rt_comb_internal.  the tree pointer is not released
 * so callers will need to call BU_PUT() or db_free_tree()
 * directly if a tree is set.  the shader and material strings are
 * released.  the comb itself will need to be released with bu_free()
 * unless it resides on the stack.
 */
#define RT_FREE_COMB_INTERNAL(_p) { \
	bu_vls_free(&(_p)->shader); \
	bu_vls_free(&(_p)->material); \
	(_p)->tree = TREE_NULL; \
	(_p)->magic = 0; \
    }


/**
 * In-memory format for database uniform-array binary object.  Perhaps
 * move to wdb.h or rtgeom.h?
 */
struct rt_binunif_internal {
    uint32_t            magic;
    int                 type;
    size_t              count;
    union {
	float           *flt;
	double          *dbl;
	char            *int8;
	short           *int16;
	int             *int32;
	long            *int64;
	unsigned char   *uint8;
	unsigned short  *uint16;
	unsigned int    *uint32;
	unsigned long   *uint64;
    } u;
};
#define RT_CHECK_BINUNIF(_p) BU_CKMAG(_p, RT_BINUNIF_INTERNAL_MAGIC, "rt_binunif_internal")
#define RT_CK_BINUNIF(_p) RT_CHECK_BINUNIF(_p)

/**
 * In-memory format for database "constraint" record
 */
struct rt_constraint_internal {
    uint32_t magic;
    int id;
    int type;
    struct bu_vls expression;
};

#define RT_CHECK_CONSTRAINT(_p) BU_CKMAG(_p, RT_CONSTRAINT_MAGIC, "rt_constraint_internal")
#define RT_CK_CONSTRAINT(_p) RT_CHECK_CONSTRAINT(_p)

/**
 * In-memory format for database "material" record
 */
struct rt_material_internal {
	uint32_t magic;
	struct bu_vls name;
	struct bu_vls parent;
	struct bu_vls source;

	struct bu_attribute_value_set physicalProperties;
	struct bu_attribute_value_set mechanicalProperties;
	struct bu_attribute_value_set opticalProperties;
	struct bu_attribute_value_set thermalProperties;
};

#define RT_CHECK_MATERIAL(_p) BU_CKMAG(_p, RT_MATERIAL_MAGIC, "rt_material_internal")
#define RT_CK_MATERIAL(_p) RT_CHECK_MATERIAL(_p)

__END_DECLS

#endif /* RT_NONGEOM_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
