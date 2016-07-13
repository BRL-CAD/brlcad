/*                           G E D . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @addtogroup libged
 *
 * Functions provided by the LIBGED geometry editing library.  These
 * routines are a procedural basis for the geometric editing
 * capabilities available in BRL-CAD.  The library is tightly coupled
 * to the LIBRT library for geometric representation and analysis.
 *
 */
/** @{ */
/** @file ged.h */

#ifndef GED_H
#define GED_H

#include "common.h"

#include "dm/bview.h"
#include "raytrace.h"
#include "rt/solid.h"
#include "ged/defines.h"
#include "ged/database.h"
#include "ged/objects.h"
#include "ged/framebuffer.h"
#include "ged/view.h"
#include "ged/analyze.h"
#include "ged/debug.h"
#include "ged/rt.h"


__BEGIN_DECLS

/**
 * Delay the specified amount of time
 */
GED_EXPORT extern int ged_delay(struct ged *gedp, int argc, const char *argv[]);

/**
 * Echo the specified arguments.
 */
GED_EXPORT extern int ged_echo(struct ged *gedp, int argc, const char *argv[]);

/**
 * Query or manipulate properties of a graph.
 */
GED_EXPORT extern int ged_graph(struct ged *gedp, int argc, const char *argv[]);



/***************************************
 * Conceptual Documentation for LIBGED *
 ***************************************
 *
 * Below are developer notes for a data structure layout that this
 * library is being migrated towards.  This is not necessarily the
 * current status of the library, but rather a high-level concept for
 * how the data might be organized down the road for the core data
 * structures available for application and extension management.
 *
 * struct ged {
 *   dbip
 *   views * >-----.
 *   result()      |
 * }               |
 *                 |
 * struct view { <-'
 *   geometry * >------.
 *   update()          |
 * }                   |
 *                     |
 * struct geometry { <-'
 *   display lists
 *   directory *
 *   update()
 * }
 *
 */

__END_DECLS

#endif /* GED_H */

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
