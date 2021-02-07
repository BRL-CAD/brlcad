/*                            O P . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file op.h
 *
 * Routines for detecting, using, and managing boolean operators.
 *
 */

#ifndef RT_OP_H
#define RT_OP_H

#include "rt/defines.h"

__BEGIN_DECLS

/**
 * Boolean operations between solids.
 */
#define MKOP(x)         (x)

#define OP_SOLID        MKOP(1)         /**< @brief  Leaf:  tr_stp -> solid */
#define OP_UNION        MKOP(2)         /**< @brief  Binary: L union R */
#define OP_INTERSECT    MKOP(3)         /**< @brief  Binary: L intersect R */
#define OP_SUBTRACT     MKOP(4)         /**< @brief  Binary: L subtract R */
#define OP_XOR          MKOP(5)         /**< @brief  Binary: L xor R, not both*/
#define OP_REGION       MKOP(6)         /**< @brief  Leaf: tr_stp -> combined_tree_state */
#define OP_NOP          MKOP(7)         /**< @brief  Leaf with no effect */
/* Internal to library routines */
#define OP_NOT          MKOP(8)         /**< @brief  Unary:  not L */
#define OP_GUARD        MKOP(9)         /**< @brief  Unary:  not L, or else! */
#define OP_XNOP         MKOP(10)        /**< @brief  Unary:  L, mark region */
#define OP_NMG_TESS     MKOP(11)        /**< @brief  Leaf: tr_stp -> nmgregion */
/* LIBWDB import/export interface to combinations */
#define OP_DB_LEAF      MKOP(12)        /**< @brief  Leaf of combination, db fmt */
#define OP_FREE         MKOP(13)        /**< @brief  Unary:  L has free chain */


typedef enum {
    DB_OP_NULL = 0,
    DB_OP_UNION = 'u',
    DB_OP_SUBTRACT = '-',
    DB_OP_INTERSECT = '+'
} db_op_t;


/**
 * Get the next CSG boolean operator found in a given string.
 *
 * Skipping any whitespace, this routine returns the CSG boolean
 * operation in canonical (single-byte enumeration) form.  It will
 * attempt to recognize operators in various unicode formats if the
 * input string contains mixed encodings.
 */
RT_EXPORT db_op_t
db_str2op(const char *str);

__END_DECLS

#endif /* RT_OP_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
