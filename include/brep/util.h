/*                      U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

/** @addtogroup brep_util
 *
 * @brief
 * These are utility routines for libbrep, used throughout the library.
 *
 */
/** @{ */
/** @file brep/util.h */

#ifndef BREP_UTIL_H
#define BREP_UTIL_H

#include "common.h"
#include "brep/defines.h"

#ifdef __cplusplus

__BEGIN_DECLS

extern "C++" {

    BREP_EXPORT bool ON_NearZero(double x, double tolerance = ON_ZERO_TOLERANCE);

} /* extern C++ */

__END_DECLS

#endif  /* __cplusplus */

/** @} */

#endif  /* BREP_UTIL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
