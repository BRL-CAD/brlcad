/*                          D E B U G . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @addtogroup ged_debug
 *
 * Geometry EDiting Library Database Debugging Related Functions.
 *
 */
/** @{ */
/** @file ged/debug.h */

#ifndef GED_DEBUG_H
#define GED_DEBUG_H

#include "common.h"
#include "ged/defines.h"

__BEGIN_DECLS

/**
 * Report/Control BRL-CAD library debugging settings
 */
GED_EXPORT extern int ged_debug(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get libbu's debug bit vector
 */
GED_EXPORT extern int ged_debugbu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Dump of the database's directory
 */
GED_EXPORT extern int ged_debugdir(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get librt's debug bit vector
 */
GED_EXPORT extern int ged_debuglib(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get librt's NMG debug bit vector
 */
GED_EXPORT extern int ged_debugnmg(struct ged *gedp, int argc, const char *argv[]);

/**
 * Used to control logging.
 */
GED_EXPORT extern int ged_log(struct ged *gedp, int argc, const char *argv[]);

/* defined in trace.c */

#define _GED_MAX_LEVELS 12
struct _ged_trace_data {
    struct ged *gtd_gedp;
    struct directory *gtd_path[_GED_MAX_LEVELS];
    struct directory *gtd_obj[_GED_MAX_LEVELS];
    mat_t gtd_xform;
    int gtd_objpos;
    int gtd_prflag;
    int gtd_flag;
};


GED_EXPORT extern void ged_trace(struct directory *dp,
				 int pathpos,
				 const mat_t old_xlate,
				 struct _ged_trace_data *gtdp,
				 int verbose);



__END_DECLS

#endif /* GED_DEBUG_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
