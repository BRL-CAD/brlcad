/*                          E N V . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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

#ifndef BU_ENV_H
#define BU_ENV_H

#include "common.h"

#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup bu_env
 *
 * @brief
 * Platform-independent methods for interacting with the parent operating
 * system environment.
 *
 */
/** @{ */
/** @file bu/env.h */

BU_EXPORT extern int bu_setenv(const char *name, const char *value, int overwrite);

/* Attempts to report available free RAM (not disk or swap space) on
 * the current machine.  Returns -1 if this information is not
 * available. */
BU_EXPORT extern long int bu_avail_mem();

/** @} */

__END_DECLS

#endif /* BU_ENV_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
