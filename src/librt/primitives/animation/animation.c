/*                    A N I M A T I O N . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @addtogroup primitives */
/** @{ */
/** @file primitives/animation/animation.c
 *
 * Persistent, non-geometric animation channels.  Channels target stable
 * articulation link names and remain independent of joint and skin storage.
 */

#include "common.h"

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "bio.h"
#include "bu/cv.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/object_io.h"
#include "rt/primitives/animation.h"

#include "../../librt_private.h"

#define ANIMATION_EXT_MAGIC 0x414e4d31u /* "ANM1" */
#define ANIMATION_EXT_VERSION 2u
#define ANIMATION_EXT_VERSION_LEGACY 1u
#define ANIMATION_EXT_HEADER_LONGS 3u
#define ANIMATION_CHANNEL_HEADER_LONGS 5u
#define ANIMATION_CHANNEL_HEADER_LONGS_LEGACY 4u
#define ANIMATION_QUATERNION_TOLERANCE 1.0e-5
#define ANIMATION_TARGET_COUNT 4u

struct animation_eval_entry {
    struct bu_vls link;
    uint32_t targets;
    size_t components[ANIMATION_TARGET_COUNT];
    fastf_t *values[ANIMATION_TARGET_COUNT];
};

struct rt_animation_eval {
    size_t count;
    struct animation_eval_entry *entries;
};

static const char *animation_targets[] = {
    "translation", "rotation", "scale", "weights"
};

static const char *animation_interpolations[] = {
    "STEP", "LINEAR", "CUBICSPLINE"
};

const char *
rt_animation_target_name(uint32_t target)
{
    if (target >= sizeof(animation_targets) / sizeof(animation_targets[0]))
	return NULL;
    return animation_targets[target];
}

const char *
rt_animation_interpolation_name(uint32_t interpolation)
{
    if (interpolation >= sizeof(animation_interpolations) /
	    sizeof(animation_interpolations[0]))
	return NULL;
    return animation_interpolations[interpolation];
}

int
rt_animation_target_from_name(const char *name, uint32_t *target)
{
    uint32_t i;

    if (!name || !target)
	return BRLCAD_ERROR;
    for (i = 0; i < sizeof(animation_targets) / sizeof(animation_targets[0]); i++) {
	if (BU_STR_EQUAL(name, animation_targets[i])) {
	    *target = i;
	    return BRLCAD_OK;
	}
    }
    return BRLCAD_ERROR;
}

int
rt_animation_interpolation_from_name(const char *name, uint32_t *interpolation)
{
    uint32_t i;

    if (!name || !interpolation)
	return BRLCAD_ERROR;
    for (i = 0; i < sizeof(animation_interpolations) /
	    sizeof(animation_interpolations[0]); i++) {
	if (BU_STR_EQUAL(name, animation_interpolations[i])) {
	    *interpolation = i;
	    return BRLCAD_OK;
	}
    }
    return BRLCAD_ERROR;
}

size_t
rt_animation_channel_components(const struct rt_animation_channel *channel)
{
    size_t expected;

    if (!channel || !rt_animation_target_name(channel->target))
	return 0;
    if (channel->target == RT_ANIMATION_WEIGHTS)
	return channel->component_count;
    expected = channel->target == RT_ANIMATION_ROTATION ? 4 : 3;
    return !channel->component_count || channel->component_count == expected ?
	expected : 0;
}

size_t
rt_animation_channel_value_count(const struct rt_animation_channel *channel)
{
    size_t components;
    size_t tuples;

    if (!channel || !channel->keyframe_count ||
	!rt_animation_interpolation_name(channel->interpolation))
	return 0;
    components = rt_animation_channel_components(channel);
    tuples = channel->interpolation == RT_ANIMATION_CUBICSPLINE ? 3 : 1;
    if (!components || channel->keyframe_count > SIZE_MAX / components ||
	channel->keyframe_count * components > SIZE_MAX / tuples)
	return 0;
    return channel->keyframe_count * components * tuples;
}

static void
animation_channel_init(struct rt_animation_channel *channel)
{
    memset(channel, 0, sizeof(*channel));
    bu_vls_init(&channel->target_link);
    channel->interpolation = RT_ANIMATION_LINEAR;
}

static void
animation_channel_release(struct rt_animation_channel *channel)
{
    if (!channel)
	return;
    if (BU_VLS_IS_INITIALIZED(&channel->target_link))
	bu_vls_free(&channel->target_link);
    bu_free(channel->times, "animation key times");
    bu_free(channel->values, "animation key values");
    memset(channel, 0, sizeof(*channel));
}

static void
animation_init(struct rt_animation_internal *animation)
{
    memset(animation, 0, sizeof(*animation));
    animation->magic = RT_ANIMATION_INTERNAL_MAGIC;
}

static void
animation_release(struct rt_animation_internal *animation)
{
    size_t i;

    if (!animation)
	return;
    for (i = 0; i < animation->channel_count; i++)
	animation_channel_release(&animation->channels[i]);
    bu_free(animation->channels, "animation channels");
    memset(animation, 0, sizeof(*animation));
}

static void
animation_message(struct bu_vls *messages, const char *format, ...)
{
    va_list arguments;

    if (!messages)
	return;
    if (bu_vls_strlen(messages))
	bu_vls_strcat(messages, "; ");
    va_start(arguments, format);
    bu_vls_vprintf(messages, format, arguments);
    va_end(arguments);
}

int
rt_animation_validate(const struct rt_animation_internal *animation,
	struct bu_vls *messages)
{
    size_t channel_index;
    int errors = 0;

    if (!animation) {
	animation_message(messages, "animation is NULL");
	return 1;
    }
    RT_ANIMATION_CK_MAGIC(animation);
    if (!animation->channel_count || !animation->channels) {
	animation_message(messages, "animation has no channels");
	return 1;
    }
    if (animation->channel_count > UINT32_MAX) {
	animation_message(messages, "animation channel count exceeds serialization limits");
	return 1;
    }
    for (channel_index = 0; channel_index < animation->channel_count;
	channel_index++) {
	const struct rt_animation_channel *channel =
	    &animation->channels[channel_index];
	size_t value_count = rt_animation_channel_value_count(channel);
	size_t components = rt_animation_channel_components(channel);
	size_t key;
	size_t other;

	if (!BU_VLS_IS_INITIALIZED(&channel->target_link) ||
	    !bu_vls_strlen(&channel->target_link)) {
	    animation_message(messages, "channel %zu has no target link", channel_index);
	    errors++;
	}
	if (!rt_animation_target_name(channel->target)) {
	    animation_message(messages, "channel %zu has unknown target %u",
		channel_index, channel->target);
	    errors++;
	}
	if (!rt_animation_interpolation_name(channel->interpolation)) {
	    animation_message(messages, "channel %zu has unknown interpolation %u",
		channel_index, channel->interpolation);
	    errors++;
	}
	if (!components || components > UINT32_MAX) {
	    animation_message(messages, "channel %zu has invalid component count",
		channel_index);
	    errors++;
	}
	if (!channel->keyframe_count || channel->keyframe_count > UINT32_MAX ||
	    !channel->times || !channel->values || !value_count) {
	    animation_message(messages, "channel %zu has invalid keyframe storage",
		channel_index);
	    errors++;
	    continue;
	}
	for (other = 0; other < channel_index; other++) {
	    if (channel->target == animation->channels[other].target &&
		BU_STR_EQUAL(bu_vls_cstr(&channel->target_link),
		    bu_vls_cstr(&animation->channels[other].target_link))) {
		animation_message(messages,
		    "channel %zu duplicates target %s on link '%s'", channel_index,
		    rt_animation_target_name(channel->target),
		    bu_vls_cstr(&channel->target_link));
		errors++;
	    }
	}
	for (key = 0; key < channel->keyframe_count; key++) {
	    if (!isfinite(channel->times[key]) ||
		(key && channel->times[key] <= channel->times[key - 1])) {
		animation_message(messages,
		    "channel %zu key times are not finite and increasing",
		    channel_index);
		errors++;
		break;
	    }
	}
	for (key = 0; key < value_count; key++) {
	    if (!isfinite(channel->values[key])) {
		animation_message(messages, "channel %zu values are not finite",
		    channel_index);
		errors++;
		break;
	    }
	}
	if (channel->target == RT_ANIMATION_ROTATION && components == 4) {
	    size_t stride = components *
		(channel->interpolation == RT_ANIMATION_CUBICSPLINE ? 3 : 1);
	    size_t value_offset = channel->interpolation ==
		RT_ANIMATION_CUBICSPLINE ? components : 0;
	    for (key = 0; key < channel->keyframe_count; key++) {
		const fastf_t *quaternion = &channel->values[key * stride + value_offset];
		fastf_t magnitude = sqrt(quaternion[0] * quaternion[0] +
		    quaternion[1] * quaternion[1] + quaternion[2] * quaternion[2] +
		    quaternion[3] * quaternion[3]);
		if (!NEAR_EQUAL(magnitude, 1.0, ANIMATION_QUATERNION_TOLERANCE)) {
		    animation_message(messages,
			"channel %zu rotation key %zu is not a unit quaternion",
			channel_index, key);
		    errors++;
		}
	    }
	}
    }
    return errors;
}

int
rt_animation_time_range(fastf_t *start, fastf_t *end,
	const struct rt_animation_internal *animation)
{
    size_t channel_index;
    fastf_t first;
    fastf_t last;

    if (!start || !end || rt_animation_validate(animation, NULL))
	return BRLCAD_ERROR;
    first = animation->channels[0].times[0];
    last = animation->channels[0].times[
	animation->channels[0].keyframe_count - 1];
    for (channel_index = 1; channel_index < animation->channel_count;
	channel_index++) {
	const struct rt_animation_channel *channel =
	    &animation->channels[channel_index];
	if (channel->times[0] < first)
	    first = channel->times[0];
	if (channel->times[channel->keyframe_count - 1] > last)
	    last = channel->times[channel->keyframe_count - 1];
    }
    *start = first;
    *end = last;
    return BRLCAD_OK;
}

static int
animation_eval_time(fastf_t *sample_time, fastf_t time, fastf_t start,
	fastf_t end, const struct rt_animation_eval_options *options)
{
    fastf_t duration;
    fastf_t offset;

    if (time >= start && time <= end) {
	*sample_time = time;
	return BRLCAD_OK;
    }
    duration = end - start;
    switch (options->extrapolation) {
	case RT_ANIMATION_EXTRAPOLATE_CLAMP:
	    *sample_time = time < start ? start : end;
	    return BRLCAD_OK;
	case RT_ANIMATION_EXTRAPOLATE_REPEAT:
	    if (ZERO(duration)) {
		*sample_time = start;
		return BRLCAD_OK;
	    }
	    offset = fmod(time - start, duration);
	    if (offset < 0.0)
		offset += duration;
	    *sample_time = start + offset;
	    return BRLCAD_OK;
	case RT_ANIMATION_EXTRAPOLATE_MIRROR:
	    if (ZERO(duration)) {
		*sample_time = start;
		return BRLCAD_OK;
	    }
	    offset = fmod(time - start, 2.0 * duration);
	    if (offset < 0.0)
		offset += 2.0 * duration;
	    if (offset > duration)
		offset = 2.0 * duration - offset;
	    *sample_time = start + offset;
	    return BRLCAD_OK;
	case RT_ANIMATION_EXTRAPOLATE_ERROR:
	    animation_message(options->messages,
		"animation time %.17g is outside [%.17g, %.17g]", time,
		start, end);
	    return BRLCAD_ERROR;
	default:
	    animation_message(options->messages,
		"unsupported animation extrapolation %u", options->extrapolation);
	    return BRLCAD_ERROR;
    }
}

int
rt_animation_eval_create(struct rt_animation_eval **result,
	const struct rt_animation_internal *animation, fastf_t time,
	const struct rt_animation_eval_options *options)
{
    const size_t minimum_options_size = offsetof(
	struct rt_animation_eval_options, reserved);
    struct rt_animation_eval *evaluation;
    fastf_t start;
    fastf_t end;
    fastf_t sample_time;
    size_t channel_index;

    if (!result)
	return BRLCAD_ERROR;
    *result = NULL;
    if (!options || options->size < minimum_options_size ||
	options->version != RT_ANIMATION_EVAL_OPTIONS_VERSION ||
	options->flags || !isfinite(time) ||
	options->extrapolation > RT_ANIMATION_EXTRAPOLATE_ERROR) {
	animation_message(options ? options->messages : NULL,
	    "invalid animation evaluation options");
	return BRLCAD_ERROR;
    }
    if (rt_animation_time_range(&start, &end, animation) != BRLCAD_OK ||
	animation_eval_time(&sample_time, time, start, end, options) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_ALLOC(evaluation, struct rt_animation_eval);
    evaluation->count = 0;
    evaluation->entries = (struct animation_eval_entry *)bu_calloc(
	animation->channel_count, sizeof(struct animation_eval_entry),
	"animation evaluation entries");
    for (channel_index = 0; channel_index < animation->channel_count;
	channel_index++) {
	const struct rt_animation_channel *channel =
	    &animation->channels[channel_index];
	struct animation_eval_entry *entry = NULL;
	size_t entry_index;

	for (entry_index = 0; entry_index < evaluation->count; entry_index++) {
	    if (BU_STR_EQUAL(bu_vls_cstr(&evaluation->entries[entry_index].link),
		    bu_vls_cstr(&channel->target_link))) {
		entry = &evaluation->entries[entry_index];
		break;
	    }
	}
	if (!entry) {
	    entry = &evaluation->entries[evaluation->count++];
	    bu_vls_init(&entry->link);
	    bu_vls_vlscat(&entry->link, &channel->target_link);
	}
	entry->components[channel->target] = rt_animation_channel_components(channel);
	entry->values[channel->target] = (fastf_t *)bu_malloc(
	    entry->components[channel->target] * sizeof(fastf_t),
	    "animation evaluation values");
	if (rt_animation_channel_sample(entry->values[channel->target], channel,
		sample_time) != BRLCAD_OK) {
	    rt_animation_eval_destroy(evaluation);
	    return BRLCAD_ERROR;
	}
	entry->targets |= 1u << channel->target;
    }
    *result = evaluation;
    return BRLCAD_OK;
}

void
rt_animation_eval_destroy(struct rt_animation_eval *evaluation)
{
    size_t index;

    if (!evaluation)
	return;
    for (index = 0; index < evaluation->count; index++) {
	size_t target;
	bu_vls_free(&evaluation->entries[index].link);
	for (target = 0; target < ANIMATION_TARGET_COUNT; target++)
	    bu_free(evaluation->entries[index].values[target],
		"animation evaluation values");
    }
    bu_free(evaluation->entries, "animation evaluation entries");
    bu_free(evaluation, "animation evaluation");
}

size_t
rt_animation_eval_count(const struct rt_animation_eval *evaluation)
{
    return evaluation ? evaluation->count : 0;
}

const char *
rt_animation_eval_link(const struct rt_animation_eval *evaluation, size_t index)
{
    if (!evaluation || index >= evaluation->count)
	return NULL;
    return bu_vls_cstr(&evaluation->entries[index].link);
}

int
rt_animation_eval_value(fastf_t *result,
	const struct rt_animation_eval *evaluation, size_t index, uint32_t target)
{
    size_t components = rt_animation_eval_value_components(evaluation, index,
	target);

    if (!result || !components)
	return BRLCAD_ERROR;
    memcpy(result, evaluation->entries[index].values[target],
	components * sizeof(fastf_t));
    return BRLCAD_OK;
}

size_t
rt_animation_eval_value_components(const struct rt_animation_eval *evaluation,
	size_t index, uint32_t target)
{
    if (!evaluation || index >= evaluation->count ||
	target >= ANIMATION_TARGET_COUNT ||
	!(evaluation->entries[index].targets & (1u << target)))
	return 0;
    return evaluation->entries[index].components[target];
}

static const fastf_t *
animation_key_value(const struct rt_animation_channel *channel, size_t key)
{
    size_t components = rt_animation_channel_components(channel);
    size_t stride = components *
	(channel->interpolation == RT_ANIMATION_CUBICSPLINE ? 3 : 1);
    size_t offset = channel->interpolation == RT_ANIMATION_CUBICSPLINE ?
	components : 0;
    return &channel->values[key * stride + offset];
}

static void
animation_normalize_quaternion(fastf_t *quaternion)
{
    fastf_t magnitude = sqrt(quaternion[0] * quaternion[0] +
	quaternion[1] * quaternion[1] + quaternion[2] * quaternion[2] +
	quaternion[3] * quaternion[3]);
    size_t component;

    if (magnitude <= SMALL_FASTF)
	return;
    for (component = 0; component < 4; component++)
	quaternion[component] /= magnitude;
}

static void
animation_slerp(fastf_t *result, const fastf_t *first,
	const fastf_t *second, fastf_t fraction)
{
    fastf_t adjusted[4];
    fastf_t dot = first[0] * second[0] + first[1] * second[1] +
	first[2] * second[2] + first[3] * second[3];
    size_t component;

    for (component = 0; component < 4; component++)
	adjusted[component] = second[component];
    if (dot < 0.0) {
	dot = -dot;
	for (component = 0; component < 4; component++)
	    adjusted[component] = -adjusted[component];
    }
    if (dot > 0.9995) {
	for (component = 0; component < 4; component++)
	    result[component] = first[component] +
		fraction * (adjusted[component] - first[component]);
	animation_normalize_quaternion(result);
	return;
    }
    {
	fastf_t angle;
	CLAMP(dot, -1.0, 1.0);
	angle = acos(dot);
	fastf_t denominator = sin(angle);
	fastf_t first_weight = sin((1.0 - fraction) * angle) / denominator;
	fastf_t second_weight = sin(fraction * angle) / denominator;
	for (component = 0; component < 4; component++)
	    result[component] = first_weight * first[component] +
		second_weight * adjusted[component];
    }
}

int
rt_animation_channel_sample(fastf_t *result,
	const struct rt_animation_channel *channel, fastf_t time)
{
    size_t components;
    size_t upper;
    size_t lower;
    size_t component;
    fastf_t fraction;
    fastf_t duration;

    if (!result || !channel || !isfinite(time) ||
	!channel->keyframe_count || !channel->times || !channel->values)
	return BRLCAD_ERROR;
    components = rt_animation_channel_components(channel);
    if (!components || !rt_animation_channel_value_count(channel))
	return BRLCAD_ERROR;
    if (time <= channel->times[0]) {
	memcpy(result, animation_key_value(channel, 0), components * sizeof(fastf_t));
	return BRLCAD_OK;
    }
    if (time >= channel->times[channel->keyframe_count - 1]) {
	memcpy(result, animation_key_value(channel, channel->keyframe_count - 1),
	    components * sizeof(fastf_t));
	return BRLCAD_OK;
    }
    upper = 1;
    while (upper < channel->keyframe_count && time >= channel->times[upper])
	upper++;
    lower = upper - 1;
    if (channel->interpolation == RT_ANIMATION_STEP) {
	memcpy(result, animation_key_value(channel, lower),
	    components * sizeof(fastf_t));
	return BRLCAD_OK;
    }
    duration = channel->times[upper] - channel->times[lower];
    fraction = (time - channel->times[lower]) / duration;
    if (channel->interpolation == RT_ANIMATION_LINEAR) {
	const fastf_t *first = animation_key_value(channel, lower);
	const fastf_t *second = animation_key_value(channel, upper);
	if (channel->target == RT_ANIMATION_ROTATION)
	    animation_slerp(result, first, second, fraction);
	else
	    for (component = 0; component < components; component++)
		result[component] = first[component] +
		    fraction * (second[component] - first[component]);
	return BRLCAD_OK;
    }
    if (channel->interpolation == RT_ANIMATION_CUBICSPLINE) {
	size_t stride = components * 3;
	const fastf_t *first = animation_key_value(channel, lower);
	const fastf_t *second = animation_key_value(channel, upper);
	const fastf_t *out_tangent = &channel->values[lower * stride + 2 * components];
	const fastf_t *in_tangent = &channel->values[upper * stride];
	fastf_t squared = fraction * fraction;
	fastf_t cubed = squared * fraction;
	fastf_t first_value_weight = 2.0 * cubed - 3.0 * squared + 1.0;
	fastf_t first_tangent_weight = cubed - 2.0 * squared + fraction;
	fastf_t second_value_weight = -2.0 * cubed + 3.0 * squared;
	fastf_t second_tangent_weight = cubed - squared;
	for (component = 0; component < components; component++)
	    result[component] = first_value_weight * first[component] +
		first_tangent_weight * duration * out_tangent[component] +
		second_value_weight * second[component] +
		second_tangent_weight * duration * in_tangent[component];
	if (channel->target == RT_ANIMATION_ROTATION)
	    animation_normalize_quaternion(result);
	return BRLCAD_OK;
    }
    return BRLCAD_ERROR;
}

static uint32_t
animation_get_uint32(const unsigned char *buffer)
{
    uint32_t value;
    memcpy(&value, buffer, sizeof(value));
    return ntohl(value);
}

static void
animation_put_uint32(unsigned char *buffer, uint32_t value)
{
    value = htonl(value);
    memcpy(buffer, &value, sizeof(value));
}

C_DECL int
rt_animation_import5(struct rt_db_internal *intern,
	const struct bu_external *external, const fastf_t *UNUSED(matrix),
	const struct db_i *dbip)
{
    struct rt_animation_internal *animation;
    const unsigned char *cursor;
    size_t available;
    size_t channel_index;
    uint32_t version;

    if (dbip) RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(intern);
    BU_CK_EXTERNAL(external);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_ANIMATION;
    intern->idb_meth = &OBJ[ID_ANIMATION];
    BU_ALLOC(animation, struct rt_animation_internal);
    intern->idb_ptr = animation;
    animation_init(animation);
    cursor = external->ext_buf;
    available = external->ext_nbytes;

#define ANIMATION_IMPORT_REQUIRE(_bytes) do { \
    size_t required = (_bytes); \
    if (required > available) goto import_error; \
    available -= required; \
} while (0)

    ANIMATION_IMPORT_REQUIRE(ANIMATION_EXT_HEADER_LONGS * SIZEOF_NETWORK_LONG);
    if (animation_get_uint32(cursor) != ANIMATION_EXT_MAGIC)
	goto import_error;
    cursor += SIZEOF_NETWORK_LONG;
    version = animation_get_uint32(cursor);
    if (version != ANIMATION_EXT_VERSION &&
	version != ANIMATION_EXT_VERSION_LEGACY)
	goto import_error;
    cursor += SIZEOF_NETWORK_LONG;
    animation->channel_count = animation_get_uint32(cursor);
    cursor += SIZEOF_NETWORK_LONG;
    if (!animation->channel_count ||
	animation->channel_count > SIZE_MAX / sizeof(struct rt_animation_channel))
	goto import_error;
    animation->channels = (struct rt_animation_channel *)bu_calloc(
	animation->channel_count, sizeof(struct rt_animation_channel),
	"animation channels");
    for (channel_index = 0; channel_index < animation->channel_count;
	channel_index++) {
	struct rt_animation_channel *channel = &animation->channels[channel_index];
	uint32_t link_length;
	size_t value_count;
	size_t value;
	double number;
	animation_channel_init(channel);
	ANIMATION_IMPORT_REQUIRE((version == ANIMATION_EXT_VERSION ?
	    ANIMATION_CHANNEL_HEADER_LONGS : ANIMATION_CHANNEL_HEADER_LONGS_LEGACY) *
	    SIZEOF_NETWORK_LONG);
	link_length = animation_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	channel->target = animation_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	channel->interpolation = animation_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	if (version == ANIMATION_EXT_VERSION) {
	    channel->component_count = animation_get_uint32(cursor);
	    cursor += SIZEOF_NETWORK_LONG;
	}
	channel->keyframe_count = animation_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	if (!link_length || !channel->keyframe_count)
	    goto import_error;
	ANIMATION_IMPORT_REQUIRE(link_length);
	bu_vls_strncpy(&channel->target_link, (const char *)cursor, link_length);
	cursor += link_length;
	value_count = rt_animation_channel_value_count(channel);
	if (!value_count || channel->keyframe_count > SIZE_MAX / sizeof(fastf_t) ||
	    value_count > SIZE_MAX / sizeof(fastf_t))
	    goto import_error;
	if (channel->keyframe_count > available / SIZEOF_NETWORK_DOUBLE)
	    goto import_error;
	ANIMATION_IMPORT_REQUIRE(channel->keyframe_count * SIZEOF_NETWORK_DOUBLE);
	channel->times = (fastf_t *)bu_malloc(channel->keyframe_count *
	    sizeof(fastf_t), "animation key times");
	for (value = 0; value < channel->keyframe_count; value++) {
	    bu_cv_ntohd((unsigned char *)&number, cursor, 1);
	    cursor += SIZEOF_NETWORK_DOUBLE;
	    channel->times[value] = number;
	}
	if (value_count > available / SIZEOF_NETWORK_DOUBLE)
	    goto import_error;
	ANIMATION_IMPORT_REQUIRE(value_count * SIZEOF_NETWORK_DOUBLE);
	channel->values = (fastf_t *)bu_malloc(value_count * sizeof(fastf_t),
	    "animation key values");
	for (value = 0; value < value_count; value++) {
	    bu_cv_ntohd((unsigned char *)&number, cursor, 1);
	    cursor += SIZEOF_NETWORK_DOUBLE;
	    channel->values[value] = number;
	}
    }
    if (available || rt_animation_validate(animation, NULL))
	goto import_error;
#undef ANIMATION_IMPORT_REQUIRE
    return BRLCAD_OK;

import_error:
#undef ANIMATION_IMPORT_REQUIRE
    animation_release(animation);
    BU_FREE(animation, struct rt_animation_internal);
    intern->idb_ptr = NULL;
    intern->idb_type = ID_NULL;
    intern->idb_meth = NULL;
    return BRLCAD_ERROR;
}

C_DECL int
rt_animation_export5(struct bu_external *external,
	const struct rt_db_internal *intern, double local2mm,
	const struct db_i *dbip)
{
    const struct rt_animation_internal *animation;
    unsigned char *cursor;
    size_t external_size = ANIMATION_EXT_HEADER_LONGS * SIZEOF_NETWORK_LONG;
    size_t channel_index;

    if (dbip) RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(intern);
    BU_CK_EXTERNAL(external);
    if (intern->idb_type != ID_ANIMATION)
	return BRLCAD_ERROR;
    animation = (const struct rt_animation_internal *)intern->idb_ptr;
    RT_ANIMATION_CK_MAGIC(animation);
    if (rt_animation_validate(animation, NULL))
	return BRLCAD_ERROR;
    for (channel_index = 0; channel_index < animation->channel_count;
	channel_index++) {
	const struct rt_animation_channel *channel = &animation->channels[channel_index];
	size_t link_length = bu_vls_strlen(&channel->target_link);
	size_t number_count = channel->keyframe_count +
	    rt_animation_channel_value_count(channel);
	size_t fixed_size = ANIMATION_CHANNEL_HEADER_LONGS * SIZEOF_NETWORK_LONG;
	if (link_length > UINT32_MAX || number_count >
	    (SIZE_MAX - fixed_size - link_length) / SIZEOF_NETWORK_DOUBLE ||
	    external_size > SIZE_MAX - fixed_size - link_length -
	    number_count * SIZEOF_NETWORK_DOUBLE)
	    return BRLCAD_ERROR;
	external_size += fixed_size + link_length +
	    number_count * SIZEOF_NETWORK_DOUBLE;
    }
    external->ext_nbytes = external_size;
    external->ext_buf = (uint8_t *)bu_malloc(external_size,
	"animation external");
    cursor = external->ext_buf;
    animation_put_uint32(cursor, ANIMATION_EXT_MAGIC); cursor += SIZEOF_NETWORK_LONG;
    animation_put_uint32(cursor, ANIMATION_EXT_VERSION); cursor += SIZEOF_NETWORK_LONG;
    animation_put_uint32(cursor, (uint32_t)animation->channel_count);
    cursor += SIZEOF_NETWORK_LONG;
    for (channel_index = 0; channel_index < animation->channel_count;
	channel_index++) {
	const struct rt_animation_channel *channel = &animation->channels[channel_index];
	size_t link_length = bu_vls_strlen(&channel->target_link);
	size_t value_count = rt_animation_channel_value_count(channel);
	size_t value;
	animation_put_uint32(cursor, (uint32_t)link_length); cursor += SIZEOF_NETWORK_LONG;
	animation_put_uint32(cursor, channel->target); cursor += SIZEOF_NETWORK_LONG;
	animation_put_uint32(cursor, channel->interpolation); cursor += SIZEOF_NETWORK_LONG;
	animation_put_uint32(cursor,
	    (uint32_t)rt_animation_channel_components(channel));
	cursor += SIZEOF_NETWORK_LONG;
	animation_put_uint32(cursor, (uint32_t)channel->keyframe_count);
	cursor += SIZEOF_NETWORK_LONG;
	memcpy(cursor, bu_vls_cstr(&channel->target_link), link_length);
	cursor += link_length;
	for (value = 0; value < channel->keyframe_count; value++) {
	    double number = channel->times[value];
	    bu_cv_htond(cursor, (const unsigned char *)&number, 1);
	    cursor += SIZEOF_NETWORK_DOUBLE;
	}
	for (value = 0; value < value_count; value++) {
	    double number = channel->values[value];
	    if (channel->target == RT_ANIMATION_TRANSLATION)
		number *= local2mm;
	    bu_cv_htond(cursor, (const unsigned char *)&number, 1);
	    cursor += SIZEOF_NETWORK_DOUBLE;
	}
    }
    return BRLCAD_OK;
}

C_DECL void
rt_animation_ifree(struct rt_db_internal *intern)
{
    struct rt_animation_internal *animation;

    RT_CK_DB_INTERNAL(intern);
    animation = (struct rt_animation_internal *)intern->idb_ptr;
    if (!animation)
	return;
    RT_ANIMATION_CK_MAGIC(animation);
    animation_release(animation);
    BU_FREE(animation, struct rt_animation_internal);
    intern->idb_ptr = NULL;
}

C_DECL int
rt_animation_make(const struct rt_functab *ftp,
	struct rt_db_internal *intern, const char *UNUSED(variant),
	const point_t UNUSED(origin), double UNUSED(scale))
{
    struct rt_animation_internal *animation;

    RT_CK_FUNCTAB(ftp);
    RT_DB_INTERNAL_INIT(intern);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_ANIMATION;
    intern->idb_meth = ftp;
    BU_ALLOC(animation, struct rt_animation_internal);
    animation_init(animation);
    intern->idb_ptr = animation;
    return BRLCAD_OK;
}

C_DECL int
rt_animation_describe(struct bu_vls *output,
	const struct rt_db_internal *intern, int verbose, double UNUSED(mm2local))
{
    const struct rt_animation_internal *animation;
    size_t channel;

    BU_CK_VLS(output);
    RT_CK_DB_INTERNAL(intern);
    animation = (const struct rt_animation_internal *)intern->idb_ptr;
    RT_ANIMATION_CK_MAGIC(animation);
    bu_vls_printf(output, "animation with %zu channel%s",
	animation->channel_count, animation->channel_count == 1 ? "" : "s");
    if (!verbose)
	return BRLCAD_OK;
    for (channel = 0; channel < animation->channel_count; channel++) {
	const struct rt_animation_channel *entry = &animation->channels[channel];
	bu_vls_printf(output, "\n  %s %s %s: %zu keys",
	    bu_vls_cstr(&entry->target_link),
	    rt_animation_target_name(entry->target),
	    rt_animation_interpolation_name(entry->interpolation),
	    entry->keyframe_count);
    }
    return BRLCAD_OK;
}

static struct rt_data_node *
animation_data_numbers(const fastf_t *values, size_t count)
{
    struct rt_data_node *array = rt_data_array();
    size_t i;

    if (!array)
	return NULL;
    for (i = 0; i < count; i++) {
	if (rt_data_array_append(array, rt_data_double(values[i])) != BRLCAD_OK) {
	    rt_data_free(array);
	    return NULL;
	}
    }
    return array;
}

static int
animation_data_set(struct rt_data_node *object, const char *name,
	struct rt_data_node *value)
{
    if (!value)
	return BRLCAD_ERROR;
    if (rt_data_object_set(object, name, value) != BRLCAD_OK) {
	rt_data_free(value);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

C_DECL int
rt_animation_data_export(struct rt_data_node **result,
	const struct rt_db_internal *intern,
	const struct rt_data_context *UNUSED(context))
{
    const struct rt_animation_internal *animation;
    struct rt_data_node *data = NULL;
    struct rt_data_node *channels = NULL;
    size_t channel_index;

    if (!result || !intern)
	return BRLCAD_ERROR;
    *result = NULL;
    RT_CK_DB_INTERNAL(intern);
    animation = (const struct rt_animation_internal *)intern->idb_ptr;
    RT_ANIMATION_CK_MAGIC(animation);
    if (rt_animation_validate(animation, NULL))
	return BRLCAD_ERROR;
    data = rt_data_object();
    channels = rt_data_array();
    if (!data || !channels ||
	animation_data_set(data, "version",
	    rt_data_uint(ANIMATION_EXT_VERSION)) != BRLCAD_OK)
	goto export_error;
    for (channel_index = 0; channel_index < animation->channel_count;
	channel_index++) {
	const struct rt_animation_channel *channel = &animation->channels[channel_index];
	struct rt_data_node *description = rt_data_object();
	struct rt_data_node *times = animation_data_numbers(channel->times,
	    channel->keyframe_count);
	struct rt_data_node *values = animation_data_numbers(channel->values,
	    rt_animation_channel_value_count(channel));
	if (!description || !times || !values ||
	    animation_data_set(description, "link",
		rt_data_string(bu_vls_cstr(&channel->target_link))) != BRLCAD_OK ||
	    animation_data_set(description, "target",
		rt_data_string(rt_animation_target_name(channel->target))) != BRLCAD_OK ||
	    animation_data_set(description, "interpolation",
		rt_data_string(rt_animation_interpolation_name(channel->interpolation))) != BRLCAD_OK ||
	    animation_data_set(description, "components",
		rt_data_uint(rt_animation_channel_components(channel))) != BRLCAD_OK)
	    goto channel_error;
	{
	    struct rt_data_node *owned_times = times;
	    times = NULL;
	    if (animation_data_set(description, "times", owned_times) != BRLCAD_OK)
		goto channel_error;
	}
	{
	    struct rt_data_node *owned_values = values;
	    values = NULL;
	    if (animation_data_set(description, "values", owned_values) != BRLCAD_OK)
		goto channel_error;
	}
	if (rt_data_array_append(channels, description) != BRLCAD_OK)
	    goto channel_error;
	description = NULL;
	continue;

channel_error:
	{
	    rt_data_free(description);
	    rt_data_free(times);
	    rt_data_free(values);
	    goto export_error;
	}
    }
    if (animation_data_set(data, "channels", channels) != BRLCAD_OK)
	goto export_error;
    channels = NULL;
    *result = data;
    return BRLCAD_OK;

export_error:
    rt_data_free(channels);
    rt_data_free(data);
    return BRLCAD_ERROR;
}

static int
animation_data_read_numbers(fastf_t *values, size_t count,
	const struct rt_data_node *array)
{
    size_t i;

    if (!values || !array || rt_data_node_type(array) != RT_DATA_ARRAY ||
	rt_data_array_size(array) != count)
	return BRLCAD_ERROR;
    for (i = 0; i < count; i++) {
	double value;
	if (rt_data_get_double(rt_data_array_get(array, i), &value) != BRLCAD_OK ||
	    !isfinite(value))
	    return BRLCAD_ERROR;
	values[i] = value;
    }
    return BRLCAD_OK;
}

C_DECL int
rt_animation_data_import(struct rt_db_internal *intern,
	const struct rt_data_node *data,
	const struct rt_data_context *UNUSED(context))
{
    struct rt_animation_internal candidate;
    struct rt_animation_internal *animation;
    const struct rt_data_node *channels;
    uint64_t version;
    size_t channel_index;

    if (!intern || !data || rt_data_node_type(data) != RT_DATA_OBJECT ||
	rt_data_get_uint(rt_data_object_get(data, "version"), &version) != BRLCAD_OK ||
	(version != ANIMATION_EXT_VERSION &&
	 version != ANIMATION_EXT_VERSION_LEGACY))
	return BRLCAD_ERROR;
    animation = (struct rt_animation_internal *)intern->idb_ptr;
    RT_ANIMATION_CK_MAGIC(animation);
    channels = rt_data_object_get(data, "channels");
    if (!channels || rt_data_node_type(channels) != RT_DATA_ARRAY ||
	!rt_data_array_size(channels))
	return BRLCAD_ERROR;
    animation_init(&candidate);
    candidate.channel_count = rt_data_array_size(channels);
    candidate.channels = (struct rt_animation_channel *)bu_calloc(
	candidate.channel_count, sizeof(struct rt_animation_channel),
	"animation channels");
    for (channel_index = 0; channel_index < candidate.channel_count;
	channel_index++) {
	struct rt_animation_channel *channel = &candidate.channels[channel_index];
	const struct rt_data_node *description = rt_data_array_get(channels, channel_index);
	const struct rt_data_node *times;
	const struct rt_data_node *values;
	const char *link;
	const char *target;
	const char *interpolation;
	uint64_t components = 0;
	size_t value_count;
	animation_channel_init(channel);
	if (!description || rt_data_node_type(description) != RT_DATA_OBJECT)
	    goto import_error;
	link = rt_data_get_string(rt_data_object_get(description, "link"));
	target = rt_data_get_string(rt_data_object_get(description, "target"));
	interpolation = rt_data_get_string(rt_data_object_get(description,
	    "interpolation"));
	times = rt_data_object_get(description, "times");
	values = rt_data_object_get(description, "values");
	if (!link || !target || !interpolation || !times || !values ||
	    rt_animation_target_from_name(target, &channel->target) != BRLCAD_OK ||
	    rt_animation_interpolation_from_name(interpolation,
		&channel->interpolation) != BRLCAD_OK ||
	    rt_data_node_type(times) != RT_DATA_ARRAY)
	    goto import_error;
	if (version == ANIMATION_EXT_VERSION) {
	    if (rt_data_get_uint(rt_data_object_get(description, "components"),
		    &components) != BRLCAD_OK || !components || components > UINT32_MAX)
		goto import_error;
	    channel->component_count = (size_t)components;
	}
	bu_vls_strcpy(&channel->target_link, link);
	channel->keyframe_count = rt_data_array_size(times);
	value_count = rt_animation_channel_value_count(channel);
	if (!channel->keyframe_count || !value_count)
	    goto import_error;
	channel->times = (fastf_t *)bu_malloc(channel->keyframe_count *
	    sizeof(fastf_t), "animation key times");
	channel->values = (fastf_t *)bu_malloc(value_count * sizeof(fastf_t),
	    "animation key values");
	if (animation_data_read_numbers(channel->times, channel->keyframe_count,
		times) != BRLCAD_OK ||
	    animation_data_read_numbers(channel->values, value_count,
		values) != BRLCAD_OK)
	    goto import_error;
    }
    if (rt_animation_validate(&candidate, NULL))
	goto import_error;
    animation_release(animation);
    *animation = candidate;
    return BRLCAD_OK;

import_error:
    animation_release(&candidate);
    return BRLCAD_ERROR;
}

C_DECL int
rt_animation_functab_validate(struct bu_vls *messages,
	const struct rt_db_internal *intern, const struct bn_tol *UNUSED(tolerance))
{
    const struct rt_animation_internal *animation;

    if (!intern)
	return BRLCAD_ERROR;
    RT_CK_DB_INTERNAL(intern);
    animation = (const struct rt_animation_internal *)intern->idb_ptr;
    return rt_animation_validate(animation, messages) ? BRLCAD_ERROR : BRLCAD_OK;
}

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
