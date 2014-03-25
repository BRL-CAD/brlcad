/*                   A P _ S C H E M A . H
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file step/ap_schema.h
 *
 * Determine which header to include
 *
 */

#ifndef AP_COMMON_H
#define AP_COMMON_H

#include "common.h"

#ifdef AP203
#  include "AP203.h"
#endif

#ifdef AP203e2
#  include "AP203e2.h"
#endif

#ifdef AP214e3
#  include "AP214e3.h"
#endif

#ifdef AP242
#  include "AP242.h"
#endif

#endif /* AP_COMMON_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
