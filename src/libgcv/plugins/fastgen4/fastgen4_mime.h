/* F A S T G E N 4 _ M I M E . H
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

#ifndef GCV_FASTGEN4_MIME_H
#define GCV_FASTGEN4_MIME_H

#include "../mime_probe.h"

/* The card dialect is identifiable, but the deck has no exact edition
 * declaration. Do not turn the implementation's edition into a file fact. */
static const struct bu_mime_trait fastgen4_traits[] = {
    BU_MIME_TRAIT_STRING("fastgen.dialect", "4"),
    BU_MIME_TRAIT_STRING("encoding", "ascii")
};
static const struct bu_mime_traits fastgen4_profile = {
    fastgen4_traits, sizeof(fastgen4_traits) / sizeof(fastgen4_traits[0])
};

#endif
