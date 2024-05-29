/*                    G E D _ L I N T . H
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
/** @file ged_lint.h
 *
 * Private header for libged lint cmd.
 *
 */

#ifndef LIBGED_LINT_GED_PRIVATE_H
#define LIBGED_LINT_GED_PRIVATE_H

#include "common.h"

#include <set>
#include <string>
#include <map>
#include <time.h>

extern "C" {
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/primitives/bot.h"
}
#include "../ged_private.h"

struct _ged_lint_opts {
    int verbosity;
    int cyclic_check;
    int missing_check;
    int invalid_shape_check;
    struct bu_vls filter;
};

struct _ged_cyclic_data {
    struct ged *gedp;
    struct bu_ptbl *paths;
};


struct _ged_missing_data {
    struct ged *gedp;
    std::set<std::string> missing;
};

struct invalid_obj {
    std::string name;
    std::string type;
    std::string error;
};

struct _ged_invalid_data {
    struct ged *gedp;
    struct _ged_lint_opts *o;
    std::set<struct directory *> invalid_dps;
    std::map<struct directory *, struct invalid_obj> invalid_msgs;
};

extern struct _ged_lint_opts *_ged_lint_opts_create();
extern void _ged_lint_opts_destroy(struct _ged_lint_opts *o);

extern int _ged_lint_bot_thin_check(struct bu_ptbl *ofaces, struct rt_bot_internal *bot, struct rt_i *rtip, double ttol, int verbose);

extern int _ged_lint_bot_close_check(struct bu_ptbl *ofaces, struct rt_bot_internal *bot, struct rt_i *rtip, double ttol, int verbose);

extern int _ged_lint_bot_miss_check(struct bu_ptbl *ofaces, struct rt_bot_internal *bot, struct rt_i *rtip, double ttol, int verbose);

#endif /* LIBGED_LINT_GED_PRIVATE_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
