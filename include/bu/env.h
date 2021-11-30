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


/* Specific types of machine memory information that may be requested */
#define BU_MEM_ALL 0
#define BU_MEM_AVAIL 1
#define BU_MEM_PAGE_SIZE 2

/* Report information about system memory.  Returns value in bytes (>= 0) if
 * requested information is available, otherwise error. */
BU_EXPORT extern long int bu_mem(int type);

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
