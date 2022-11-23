/*                       R E G I O N . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @file rt/region.h
 *
 */

#ifndef RT_REGION_H
#define RT_REGION_H

#include "common.h"
#include "vmath.h"
#include "bu/avs.h"
#include "bu/list.h"
#include "bu/magic.h"
#include "rt/defines.h"
#include "rt/mater.h"

__BEGIN_DECLS

union tree; /* forward declaration */
struct resource; /* forward declaration */
struct db_i; /* forward declaration */

/**
 * The region structure.
 */
struct region {
    struct bu_list      l;              /**< @brief magic # and doubly linked list */
    const char *        reg_name;       /**< @brief Identifying string */
    union tree *        reg_treetop;    /**< @brief Pointer to boolean tree */
    int                 reg_bit;        /**< @brief constant index into Regions[] */
    int                 reg_regionid;   /**< @brief Region ID code.  If <=0, use reg_aircode */
    int                 reg_aircode;    /**< @brief Region ID AIR code */
    int                 reg_gmater;     /**< @brief GIFT Material code */
    int                 reg_los;        /**< @brief approximate line-of-sight thickness equivalence */
    struct mater_info   reg_mater;      /**< @brief Real material information */
    void *              reg_mfuncs;     /**< @brief User appl. funcs for material */
    void *              reg_udata;      /**< @brief User appl. data for material */
    int                 reg_transmit;   /**< @brief flag:  material transmits light */
    long                reg_instnum;    /**< @brief instance number, from d_uses */
    short               reg_all_unions; /**< @brief 1=boolean tree is all unions */
    short               reg_is_fastgen; /**< @brief FASTGEN-compatibility mode? */
#define REGION_NON_FASTGEN      0
#define REGION_FASTGEN_PLATE    1
#define REGION_FASTGEN_VOLUME   2
    struct bu_attribute_value_set attr_values;  /**< @brief Attribute/value set */
};
#define REGION_NULL     ((struct region *)0)
#define RT_CK_REGION(_p) BU_CKMAG(_p, RT_REGION_MAGIC, "struct region")

#ifdef USE_OPENCL
struct cl_bool_region {
    cl_uint btree_offset;           /**< @brief index to the start of the bit tree */
    cl_int reg_aircode;             /**< @brief Region ID AIR code */
    cl_int reg_bit;                 /**< @brief constant index into Regions[] */
    cl_short reg_all_unions;        /**< @brief 1=boolean tree is all unions */
};

/*
 * Values for Shader Function ID.
 */
#define SH_NONE 	0
#define SH_PHONG	1


struct cl_phong_specific {
    cl_double wgt_specular;
    cl_double wgt_diffuse;
    cl_int shine;
};

/**
 * The region structure.
 */
struct cl_region {
    cl_float color[3];		/**< @brief explicit color:  0..1  */
    cl_int mf_id;
    union {
	struct cl_phong_specific phg_spec;
    }udata;
};

#endif

/* Print a region */
RT_EXPORT extern void rt_pr_region(const struct region *rp);

/**
 * Given the name of a region, return the matrix which maps model
 * coordinates into "region" coordinates.
 *
 * Returns:
 * 0 OK
 * <0 Failure
 */
RT_EXPORT extern int db_region_mat(mat_t                m,              /* result */
				   struct db_i  *dbip,
				   const char   *name,
				   struct resource *resp);


__END_DECLS

#endif /* RT_REGION_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
