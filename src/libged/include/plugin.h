/*                        P L U G I N . H
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file plugin.h
 *
 * Brief description
 *
 */

#include "../ged_private.h"

extern void *ged_cmds;

/* Default command behaviors when it comes to impacts on calling applications.
 * Need callback hooks in gedp so the application can tell the command what it
 * needs in these scenarios.  For some it might be possible to have default
 * libdm based callbacks if none are supplied... */

/* Flags are set and checked with bitwise operations:
 * (see, for example, https://www.learncpp.com/cpp-tutorial/bit-manipulation-with-bitwise-operators-and-bit-masks/)
 *
 * int flags = 0;
 *
 * // Enable one flag:
 * flags |= flag1
 * // Enable multiple flags at once:
 * flags |= ( flag2 | flag3 );
 * // Disable one flag:
 * flags &= ~flag1
 * // Disable multiple flags at once:
 * flags &= &( flag2 | flag3 );
 */

#define GED_CMD_DEFAULT       0
#define GED_CMD_INTERACTIVE   1 << 0
#define GED_CMD_UPDATE_SCENE  1 << 1
#define GED_CMD_UPDATE_VIEW   1 << 2
#define GED_CMD_AUTOVIEW      1 << 3
#define GED_CMD_ALL_VIEWS     1 << 4
#define GED_CMD_VIEW_CALLBACK 1 << 5

struct ged_cmd_impl {
    const char *cname;
    ged_func_ptr cmd;
    int64_t opts;
};


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
