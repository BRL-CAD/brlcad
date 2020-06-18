/*                    G E D _ B O T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file ged_bot.h
 *
 * Private header for libged bot cmd.
 *
 */

#ifndef LIBGED_BREP_GED_PRIVATE_H
#define LIBGED_BREP_GED_PRIVATE_H

#include "common.h"

#include <set>
#include <string>
#include <time.h>

#include "bu/opt.h"
#include "bn/plot3.h"
#include "bu/color.h"
#include "rt/db4.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "ged.h"

__BEGIN_DECLS

#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

struct _ged_bot_info {
    struct ged *gedp = NULL;
    struct rt_db_internal *intern = NULL;
    struct directory *dp = NULL;
    struct bn_vlblock *vbp = NULL;
    struct bu_color *color = NULL;
    int verbosity;
    int visualize;
    std::string solid_name;
    const struct bu_cmdtab *cmds = NULL;
    struct bu_opt_desc *gopts = NULL;
};

/* defined in draw.c */
extern void _ged_cvt_vlblock_to_solids(struct ged *gedp,
				       struct bn_vlblock *vbp,
				       const char *name,
				       int copy);

int _bot_obj_setup(struct _ged_bot_info *gb, const char *name);

int _bot_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps);

int _bot_cmd_extrude(void *bs, int argc, const char **argv);

int _bot_cmd_check(void *bs, int argc, const char **argv);

int _bot_cmd_remesh(void *bs, int argc, const char **argv);

__END_DECLS

#endif /* LIBGED_BREP_GED_PRIVATE_H */

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
