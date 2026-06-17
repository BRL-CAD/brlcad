/*                    S C E N E _ O B J E C T . H
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
/** @addtogroup libbsg
 *
 * @brief
 * Semantic scene-object query APIs.
 *
 * Public callers may visit database-derived scene nodes or request scene
 * bounds updates through this header.  Storage-level node allocation,
 * lifecycle callbacks, node lookup tables, and mutable geometry lists are
 * private implementation details.
 */
/** @{ */
/* @file bsg/scene_object.h */

#ifndef BSG_SCENE_OBJECT_H
#define BSG_SCENE_OBJECT_H

#include "common.h"
#include "vmath.h"
#include "bsg/defines.h"

__BEGIN_DECLS

/* Public scene-object identity and bounds operations are exposed through
 * typed refs, feature records, render records, and scene-builder APIs. */

__END_DECLS

#endif /* BSG_SCENE_OBJECT_H */

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
