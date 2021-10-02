/*                      N O N G E O M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
