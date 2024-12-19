/*                     R T _ I N I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2024 United States Government as represented by
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
/** @file rt_init.cpp
 *
 * NOTE: as this init is global to ALL applications before main(),
 * care must be taken to not write to STDOUT or STDERR or app output
 * may be corrupted, signals can be raised, or worse.
 *
 */

#include "common.h"

#include "rt/defines.h"
#include "rt/debug.h"
#include "rt/global.h"
#include "rt/resource.h"
#include "rt/rt_instance.h"

static void
librt_init(void)
{
    BU_LIST_INIT(&rt_vlfree);
    rt_init_resource(&rt_uniresource, 0, NULL);

    // NOTE - rt_new_rti used to do this, checking if the rtg_vlfree list was
    // initialized.  Since we're doing that initialization in this routine,
    // handle reading LIBRT_DEBUG here as well.
    const char *debug_flags = getenv("LIBRT_DEBUG");
    if (debug_flags)
	rt_debug = strtol(debug_flags, NULL, 0x10);
}

static void
librt_clear(void)
{
}

struct librt_initializer {
    /* constructor */
    librt_initializer() {
	librt_init();
    }
    /* destructor */
    ~librt_initializer() {
	librt_clear();
    }
};

static librt_initializer LIBRT;


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

