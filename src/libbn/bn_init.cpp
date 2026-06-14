/*                     B N _ I N I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file bn_init.cpp
 *
 * NOTE: as this init is global to ALL applications before main(), care must be
 * taken to not write to STDOUT or STDERR or app output may be corrupted,
 * signals can be raised, or worse.
 */

#include "common.h"

#include <mutex>

#include "bn/noise.h"

static void
libbn_init(void)
{
    bn_noise_init();
}

static std::once_flag g_init_once;

extern "C" void
bn_ensure_initialized(void)
{
    std::call_once(g_init_once, [](){
	libbn_init();
    });
}

struct libbn_initializer {
    /* constructor */
    libbn_initializer() {
	bn_ensure_initialized();
    }
    /* destructor */
    ~libbn_initializer() {
    }
};

static libbn_initializer LIBBN;


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
