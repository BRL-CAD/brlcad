/*                    S C E N E _ S E T . H
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
 * A bsg_scene_set manages per-view scene attachments across a set of views
 * that share a GED instance.  It is reserved for future use when a
 * centralised per-GED registry is needed.
 */
/** @{ */
/* @file bsg/scene_set.h */

#ifndef BSG_SCENE_SET_H
#define BSG_SCENE_SET_H

#include "common.h"
#include "bsg/defines.h"

__BEGIN_DECLS

/**
 * Opaque per-GED scene root registry.
 */
struct bsg_scene_set;

/**
 * Allocate an empty bsg_scene_set.
 */
BSG_EXPORT extern struct bsg_scene_set *
bsg_scene_set_create(void);

/**
 * Free a bsg_scene_set (does NOT free the scene roots stored inside it).
 */
BSG_EXPORT extern void
bsg_scene_set_destroy(struct bsg_scene_set *ss);

/**
 * Scene roots are registered through implementation-private storage while the
 * public API is finalized around scene refs.
 */

/**
 * Remove the entry for @p v from @p ss (does NOT free the stored scene).
 */
BSG_EXPORT extern void
bsg_scene_set_remove(struct bsg_scene_set *ss, struct bsg_view *v);

__END_DECLS

#endif /* BSG_SCENE_SET_H */

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
