/*                     A N I M A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file rt/primitives/animation.h
 *
 * Persistent animation channels targeting stable articulation link names.
 */

#ifndef RT_PRIMITIVES_ANIMATION_H
#define RT_PRIMITIVES_ANIMATION_H

#include "common.h"

#include "bu/vls.h"
#include "rt/defines.h"
#include "rt/geom.h"

__BEGIN_DECLS

enum rt_animation_target {
    RT_ANIMATION_TRANSLATION = 0,
    RT_ANIMATION_ROTATION,
    RT_ANIMATION_SCALE
};

enum rt_animation_interpolation {
    RT_ANIMATION_STEP = 0,
    RT_ANIMATION_LINEAR,
    RT_ANIMATION_CUBICSPLINE
};

RT_EXPORT extern const char *rt_animation_target_name(uint32_t target);
RT_EXPORT extern const char *rt_animation_interpolation_name(uint32_t interpolation);
RT_EXPORT extern int rt_animation_target_from_name(const char *name, uint32_t *target);
RT_EXPORT extern int rt_animation_interpolation_from_name(const char *name,
	uint32_t *interpolation);
RT_EXPORT extern size_t rt_animation_channel_components(
	const struct rt_animation_channel *channel);
RT_EXPORT extern size_t rt_animation_channel_value_count(
	const struct rt_animation_channel *channel);

/** Validate all channels and keyframes.  Returns the number of errors. */
RT_EXPORT extern int rt_animation_validate(
	const struct rt_animation_internal *animation, struct bu_vls *messages);

/** Sample one channel at time in seconds. */
RT_EXPORT extern int rt_animation_channel_sample(fastf_t *result,
	const struct rt_animation_channel *channel, fastf_t time);

__END_DECLS

#endif /* RT_PRIMITIVES_ANIMATION_H */
