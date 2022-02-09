/*                        P L U G I N . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
#include "brlcad_version.h"

#define GED_API (2*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)

extern void *ged_cmds;

#define GED_CMD_REGISTER(command, callback, options) \
    extern "C" { \
	struct ged_cmd_impl CPP_GLUE(command, _cmd_impl) = {CPP_STR(command), callback, options}; \
	const struct ged_cmd CPP_GLUE(command, _cmd) = {&CPP_GLUE(command, _cmd_impl)}; \
    }

#define GED_CMD_HELPER1(x, y) x##y
#define GED_CMD(x) \
	int GED_CMD_HELPER1(ged_,x)(struct ged *gedp, int argc, const char *argv[]) \
	{ \
	    const char *fname = #x ; \
	    const char *argv0 = argv[0] ; \
	    int vret = ged_cmd_valid(argv[0], fname); \
	    if (vret) { \
		argv[0] = fname; \
	    }\
	    int ret = ged_exec(gedp, argc, argv); \
	    if (vret) { \
		ret |= BRLCAD_UNKNOWN; \
	    } \
	    argv[0] = argv0; \
	    return ret; \
	} \



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

/* Unsigned long long (which we get from C99) must be at least 64 bits, so we
 * may define up to 64 flags here. (although we probably don't want that many,
 * using that type for future proofing...) */
#define GED_CMD_DEFAULT       0
#define GED_CMD_INTERACTIVE   1ULL << 0
#define GED_CMD_UPDATE_SCENE  1ULL << 1
#define GED_CMD_UPDATE_VIEW   1ULL << 2
#define GED_CMD_AUTOVIEW      1ULL << 3
#define GED_CMD_ALL_VIEWS     1ULL << 4
#define GED_CMD_VIEW_CALLBACK 1ULL << 5

struct ged_cmd_impl {
    const char *cname;
    ged_func_ptr cmd;
    unsigned long long opts;
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
