/*                          A N I M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @file rt/anim.h
 *
 */

#ifndef RT_ANIM_H
#define RT_ANIM_H

#include "common.h"
#include "vmath.h"
#include "bu/vls.h"
#include "rt/db_fullpath.h"

__BEGIN_DECLS

/**
 * Each one of these structures specifies an arc in the tree that is
 * to be operated on for animation purposes.  More than one animation
 * operation may be applied at any given arc.  The directory structure
 * points to a linked list of animate structures (built by
 * rt_anim_add()), and the operations are processed in the order
 * given.
 */
struct anim_mat {
    int         anm_op;                 /**< @brief  ANM_RSTACK, ANM_RARC... */
    mat_t       anm_mat;                /**< @brief  Matrix */
};
#define ANM_RSTACK      1               /**< @brief  Replace stacked matrix */
#define ANM_RARC        2               /**< @brief  Replace arc matrix */
#define ANM_LMUL        3               /**< @brief  Left (root side) mul */
#define ANM_RMUL        4               /**< @brief  Right (leaf side) mul */
#define ANM_RBOTH       5               /**< @brief  Replace stack, arc=Idn */

struct rt_anim_property {
    uint32_t magic;
    int                 anp_op;         /**< @brief  RT_ANP_REPLACE, etc. */
    struct bu_vls       anp_shader;     /**< @brief  Update string */
};
#define RT_ANP_REPLACE  1               /**< @brief  Replace shader string */
#define RT_ANP_APPEND   2               /**< @brief  Append to shader string */
#define RT_CK_ANP(_p) BU_CKMAG((_p), RT_ANP_MAGIC, "rt_anim_property")

struct rt_anim_color {
    int anc_rgb[3];                     /**< @brief  New color */
};


struct animate {
    uint32_t            magic;          /**< @brief  magic number */
    struct animate *    an_forw;        /**< @brief  forward link */
    struct db_full_path an_path;        /**< @brief  (sub)-path pattern */
    int                 an_type;        /**< @brief  AN_MATRIX, AN_COLOR... */
    union animate_specific {
        struct anim_mat         anu_m;
        struct rt_anim_property anu_p;
        struct rt_anim_color    anu_c;
        float                   anu_t;
    } an_u;
};
#define RT_AN_MATRIX      1             /**< @brief  Matrix animation */
#define RT_AN_MATERIAL    2             /**< @brief  Material property anim */
#define RT_AN_COLOR       3             /**< @brief  Material color anim */
#define RT_AN_SOLID       4             /**< @brief  Solid parameter anim */
#define RT_AN_TEMPERATURE 5             /**< @brief  Region temperature */

#define ANIM_NULL       ((struct animate *)0)
#define RT_CK_ANIMATE(_p) BU_CKMAG((_p), ANIMATE_MAGIC, "animate")

__END_DECLS

#endif /* RT_ANIM_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
