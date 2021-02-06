/*                         V E R S I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#ifndef BU_VERSION_H
#define BU_VERSION_H

#include "common.h"
#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup bu_version
 * @brief
 * Version reporting for LIBBU
 */
/** @{ */
/** @file bu/version.h */

/**
 * returns the compile-time version of libbu
 */
BU_EXPORT extern const char *bu_version(void);

/** @} */

__END_DECLS

#endif  /* BU_VERSION_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
