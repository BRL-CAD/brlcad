/*                          R T . H
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
/** @addtogroup ged_raytrace
 *
 * Geometry EDiting Library Database Raytracing Related Functions.
 *
 */
/** @{ */
/** @file ged/rt.h */

#ifndef GED_RT_H
#define GED_RT_H

#include "common.h"
#include "ged/defines.h"

__BEGIN_DECLS

/**
 * Invoke any raytracing application with the current view.
 */
GED_EXPORT extern int ged_rrt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Run the raytracing application.
 */
GED_EXPORT extern int ged_rt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Abort the current raytracing processes.
 */
GED_EXPORT extern int ged_rtabort(struct ged *gedp, int argc, const char *argv[]);

/**
 * Run the rtwizard application.
 */
GED_EXPORT extern int ged_rtwizard(struct ged *gedp, int argc, const char *argv[]);

/**
 * Preview a new style RT animation script.
 */
GED_EXPORT extern int ged_preview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Save keyframe in file
 */
GED_EXPORT extern int ged_savekey(struct ged *gedp, int argc, const char *argv[]);


__END_DECLS

#endif /* GED_RT_H */

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
