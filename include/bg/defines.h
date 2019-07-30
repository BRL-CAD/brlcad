/*                        D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2019 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup bg_defines
 * @brief
 * Common definitions for the headers used in bg.h (i.e. the headers found in include/bg)
 */
/** @{ */
/** @file bg/defines.h */

#ifndef BG_DEFINES_H
#define BG_DEFINES_H

#include "common.h"

#ifndef BG_EXPORT
#  if defined(BG_DLL_EXPORTS) && defined(BG_DLL_IMPORTS)
#    error "Only BG_DLL_EXPORTS or BG_DLL_IMPORTS can be defined, not both."
#  elif defined(BG_DLL_EXPORTS)
#    define BG_EXPORT __declspec(dllexport)
#  elif defined(BG_DLL_IMPORTS)
#    define BG_EXPORT __declspec(dllimport)
#  else
#    define BG_EXPORT
#  endif
#endif

/* Definitions for clockwise and counter-clockwise loop directions */
#define BG_CW 1
#define BG_CCW -1

/**
 * Tessellation (geometric) tolerances, different beasts than the
 * calculation tolerance in bn_tol.
 */
struct bg_tess_tol {
    uint32_t magic;
    double              abs;                    /**< @brief absolute dist tol */
    double              rel;                    /**< @brief rel dist tol */
    double              norm;                   /**< @brief normal tol */
};
#define BG_CK_TESS_TOL(_p) BU_CKMAG(_p, BG_TESS_TOL_MAGIC, "bg_tess_tol")
#define BG_TESS_TOL_INIT_ZERO {BG_TESS_TOL_MAGIC, 0.0, 0.0, 0.0}

#endif  /* BG_DEFINES_H */
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
