/*                       G L O B A L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
/** @file rt/global.h
 *
 * TODO - rather than completely eliminating the rt_vlfree global, a better
 * approach might be to make it optional - i.e., have our code fall back on it
 * automatically if a user doesn't supply their own vlfree.  That would allow
 * user code to "just work" in all cases, but allow applications to manage
 * their vlfree containers if they so choose (right now, once vlist memory is
 * allocated, it generally gets added to rt_vlfree and memory usage grows until
 * the app is shut down.  This may be fine, but if an app wants to discard
 * their vlfree memory for a particular use case to reduce memory footprint, or
 * use different vlfree lists for multithreading situations, they should be
 * able to manage their own vlfree to do so:
 *
 * struct bu_list usr_vlfree;
 * BU_LIST_INIT(&usr_vlfree);
 *
 *
 * rt_uniresource is a similar case in point - the convenience is very useful,
 * but there are situations where applications want/need to manage their own
 * resources for more demanding scenarios.
 */

#ifndef RT_GLOBAL_H
#define RT_GLOBAL_H

#include "common.h"

#include "vmath.h"
#include "rt/defines.h"

__BEGIN_DECLS

/**
 * Definitions for librt.a which are global to the library regardless
 * of how many different models are being worked on
 */

/**
 * Global container for holding reusable vlist elements.  This is an
 * acceleration mechanism for things like GED drawing, which would otherwise
 * need to (re)allocate massive numbers of individual vlists. */
RT_EXPORT extern struct bu_list rt_vlfree;

/**
 * Default librt-supplied resource structure for uniprocessor cases.  Can be
 * used by applications as a convenient way to supply a resource to functions
 * requiring it without requiring them to create and manage their own
 * resources.  For more demanding scenarios such as multithreaded raytracing,
 * applications will want to manage their own per-CPU resources.
 *
 * Unlike user-declared struct resource instances, rt_uniresource does not need
 * to be initialized with rt_init_resource - that is handled by LIBRT.
 */
struct resource;
RT_EXPORT extern struct resource rt_uniresource;

__END_DECLS

#endif /* RT_GLOBAL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
