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

enum rt_animation_extrapolation {
    RT_ANIMATION_EXTRAPOLATE_CLAMP = 0,
    RT_ANIMATION_EXTRAPOLATE_REPEAT,
    RT_ANIMATION_EXTRAPOLATE_MIRROR,
    RT_ANIMATION_EXTRAPOLATE_ERROR
};

struct rt_animation_eval;

#define RT_ANIMATION_EVAL_OPTIONS_VERSION 1u
#define RT_ANIMATION_EVAL_OPTIONS_RESERVED 4u

struct rt_animation_eval_options {
    size_t size;
    uint32_t version;
    uint64_t flags;
    uint32_t extrapolation;
    struct bu_vls *messages;
    void *reserved[RT_ANIMATION_EVAL_OPTIONS_RESERVED];
};

#define RT_ANIMATION_EVAL_OPTIONS_INIT { \
    sizeof(struct rt_animation_eval_options), \
    RT_ANIMATION_EVAL_OPTIONS_VERSION, 0, \
    RT_ANIMATION_EXTRAPOLATE_CLAMP, NULL, {NULL, NULL, NULL, NULL} \
}

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

/** Return the inclusive range spanning every channel's key times. */
RT_EXPORT extern int rt_animation_time_range(fastf_t *start, fastf_t *end,
	const struct rt_animation_internal *animation);

/**
 * Evaluate a clip into immutable per-link pose overrides.  Evaluation never
 * changes the animation or its target joints.  The caller owns the result.
 */
RT_EXPORT extern int rt_animation_eval_create(struct rt_animation_eval **result,
	const struct rt_animation_internal *animation, fastf_t time,
	const struct rt_animation_eval_options *options);
RT_EXPORT extern void rt_animation_eval_destroy(struct rt_animation_eval *evaluation);
RT_EXPORT extern size_t rt_animation_eval_count(
	const struct rt_animation_eval *evaluation);
RT_EXPORT extern const char *rt_animation_eval_link(
	const struct rt_animation_eval *evaluation, size_t index);
RT_EXPORT extern int rt_animation_eval_value(fastf_t *result,
	const struct rt_animation_eval *evaluation, size_t index, uint32_t target);

/** Sample one channel at time in seconds. */
RT_EXPORT extern int rt_animation_channel_sample(fastf_t *result,
	const struct rt_animation_channel *channel, fastf_t time);

__END_DECLS

#endif /* RT_PRIMITIVES_ANIMATION_H */
