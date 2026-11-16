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

#include "common.h"

#include <math.h>
#include <string.h>

#include "bu/app.h"
#include "raytrace.h"
#include "rt/object_io.h"
#include "rt/primitives/animation.h"

static int
close_enough(fastf_t first, fastf_t second)
{
    return NEAR_EQUAL(first, second, 1.0e-9);
}

static void
set_channel(struct rt_animation_channel *channel, const char *link,
	uint32_t target, uint32_t interpolation, const fastf_t *times,
	size_t keyframe_count, const fastf_t *values)
{
    size_t value_count;

    bu_vls_init(&channel->target_link);
    bu_vls_strcpy(&channel->target_link, link);
    channel->target = target;
    channel->interpolation = interpolation;
    channel->keyframe_count = keyframe_count;
    channel->times = (fastf_t *)bu_malloc(keyframe_count * sizeof(fastf_t),
	"test animation times");
    memcpy(channel->times, times, keyframe_count * sizeof(fastf_t));
    value_count = rt_animation_channel_value_count(channel);
    channel->values = (fastf_t *)bu_malloc(value_count * sizeof(fastf_t),
	"test animation values");
    memcpy(channel->values, values, value_count * sizeof(fastf_t));
}

static void
make_animation(struct rt_db_internal *intern)
{
    static const fastf_t times[] = {0.0, 2.0};
    static const fastf_t translations[] = {
	0.0, 0.0, 0.0, 10.0, 0.0, 0.0
    };
    static const fastf_t rotations[] = {
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0
    };
    static const fastf_t scales[] = {
	0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0,
	0.0, 0.0, 0.0, 3.0, 3.0, 3.0, 0.0, 0.0, 0.0
    };
    struct rt_animation_internal *animation;
    point_t origin = VINIT_ZERO;

    RT_DB_INTERNAL_INIT(intern);
    OBJ[ID_ANIMATION].ft_make(&OBJ[ID_ANIMATION], intern, NULL, origin, 1.0);
    animation = (struct rt_animation_internal *)intern->idb_ptr;
    animation->channel_count = 3;
    animation->channels = (struct rt_animation_channel *)bu_calloc(3,
	sizeof(struct rt_animation_channel), "test animation channels");
    set_channel(&animation->channels[0], "tool", RT_ANIMATION_TRANSLATION,
	RT_ANIMATION_LINEAR, times, 2, translations);
    set_channel(&animation->channels[1], "tool", RT_ANIMATION_ROTATION,
	RT_ANIMATION_LINEAR, times, 2, rotations);
    set_channel(&animation->channels[2], "tool", RT_ANIMATION_SCALE,
	RT_ANIMATION_CUBICSPLINE, times, 2, scales);
}

static int
check_animation(const struct rt_db_internal *intern)
{
    const struct rt_animation_internal *animation =
	(const struct rt_animation_internal *)intern->idb_ptr;
    const fastf_t half_sqrt_two = sqrt(0.5);
    fastf_t value[4];

    if (!animation || rt_animation_validate(animation, NULL) ||
	animation->channel_count != 3 ||
	!BU_STR_EQUAL(bu_vls_cstr(&animation->channels[0].target_link), "tool") ||
	!close_enough(animation->channels[0].values[3], 10.0) ||
	rt_animation_channel_sample(value, &animation->channels[0], 1.0) != BRLCAD_OK ||
	!close_enough(value[0], 5.0) ||
	rt_animation_channel_sample(value, &animation->channels[1], 1.0) != BRLCAD_OK ||
	!close_enough(value[2], half_sqrt_two) ||
	!close_enough(value[3], half_sqrt_two) ||
	rt_animation_channel_sample(value, &animation->channels[2], 1.0) != BRLCAD_OK ||
	!close_enough(value[0], 2.0) || !close_enough(value[1], 2.0) ||
	!close_enough(value[2], 2.0))
	return BRLCAD_ERROR;
    return BRLCAD_OK;
}

static int
test_db5_round_trip(void)
{
    struct rt_db_internal source;
    struct rt_db_internal restored;
    struct bu_external external = BU_EXTERNAL_INIT_ZERO;
    int result = BRLCAD_ERROR;

    make_animation(&source);
    RT_DB_INTERNAL_INIT(&restored);
    if (OBJ[ID_ANIMATION].ft_export5(&external, &source, 1.0, NULL) != BRLCAD_OK ||
	OBJ[ID_ANIMATION].ft_import5(&restored, &external, bn_mat_identity,
	    NULL) != BRLCAD_OK)
	goto cleanup;
    result = check_animation(&restored);

cleanup:
    bu_free_external(&external);
    rt_db_free_internal(&source);
    rt_db_free_internal(&restored);
    return result;
}

static int
test_data_round_trip(void)
{
    struct rt_db_internal source;
    struct rt_db_internal restored;
    struct rt_data_context context = RT_DATA_CONTEXT_INIT;
    struct rt_data_node *data = NULL;
    int result = BRLCAD_ERROR;

    make_animation(&source);
    RT_DB_INTERNAL_INIT(&restored);
    if (rt_obj_data_export(&data, &source, &context) != BRLCAD_OK ||
	rt_obj_data_import(&restored, data, &context) != BRLCAD_OK)
	goto cleanup;
    result = check_animation(&restored);

cleanup:
    rt_data_free(data);
    rt_db_free_internal(&source);
    rt_db_free_internal(&restored);
    return result;
}

static int
test_duplicate_target_rejected(void)
{
    struct rt_db_internal intern;
    struct rt_animation_internal *animation;
    int result;

    make_animation(&intern);
    animation = (struct rt_animation_internal *)intern.idb_ptr;
    animation->channels[2].target = RT_ANIMATION_TRANSLATION;
    result = rt_animation_validate(animation, NULL) ? BRLCAD_OK : BRLCAD_ERROR;
    rt_db_free_internal(&intern);
    return result;
}

static int
test_evaluation(void)
{
    struct rt_db_internal intern;
    const struct rt_animation_internal *animation;
    struct rt_animation_eval_options options = RT_ANIMATION_EVAL_OPTIONS_INIT;
    struct rt_animation_eval *evaluation = NULL;
    fastf_t start;
    fastf_t end;
    fastf_t value[4];
    int result = BRLCAD_ERROR;

    make_animation(&intern);
    animation = (const struct rt_animation_internal *)intern.idb_ptr;
    if (rt_animation_time_range(&start, &end, animation) != BRLCAD_OK ||
	!close_enough(start, 0.0) || !close_enough(end, 2.0) ||
	rt_animation_eval_create(&evaluation, animation, 1.0, &options) != BRLCAD_OK ||
	rt_animation_eval_count(evaluation) != 1 ||
	!BU_STR_EQUAL(rt_animation_eval_link(evaluation, 0), "tool") ||
	rt_animation_eval_value(value, evaluation, 0,
	    RT_ANIMATION_TRANSLATION) != BRLCAD_OK ||
	!close_enough(value[X], 5.0))
	goto cleanup;
    rt_animation_eval_destroy(evaluation);
    evaluation = NULL;

    options.extrapolation = RT_ANIMATION_EXTRAPOLATE_REPEAT;
    if (rt_animation_eval_create(&evaluation, animation, 3.0, &options) != BRLCAD_OK ||
	rt_animation_eval_value(value, evaluation, 0,
	    RT_ANIMATION_TRANSLATION) != BRLCAD_OK ||
	!close_enough(value[X], 5.0))
	goto cleanup;
    rt_animation_eval_destroy(evaluation);
    evaluation = NULL;

    options.extrapolation = RT_ANIMATION_EXTRAPOLATE_MIRROR;
    if (rt_animation_eval_create(&evaluation, animation, -1.0, &options) != BRLCAD_OK ||
	rt_animation_eval_value(value, evaluation, 0,
	    RT_ANIMATION_TRANSLATION) != BRLCAD_OK ||
	!close_enough(value[X], 5.0))
	goto cleanup;
    rt_animation_eval_destroy(evaluation);
    evaluation = NULL;

    options.extrapolation = RT_ANIMATION_EXTRAPOLATE_ERROR;
    if (rt_animation_eval_create(&evaluation, animation, 3.0, &options) != BRLCAD_ERROR)
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    rt_animation_eval_destroy(evaluation);
    rt_db_free_internal(&intern);
    return result;
}

static int
test_negative_time(void)
{
    struct rt_db_internal intern;
    struct rt_animation_internal *animation;
    fastf_t start;
    fastf_t end;
    int result = BRLCAD_ERROR;

    make_animation(&intern);
    animation = (struct rt_animation_internal *)intern.idb_ptr;
    for (size_t channel = 0; channel < animation->channel_count; channel++) {
	animation->channels[channel].times[0] -= 2.0;
	animation->channels[channel].times[1] -= 2.0;
    }
    if (!rt_animation_validate(animation, NULL) &&
	rt_animation_time_range(&start, &end, animation) == BRLCAD_OK &&
	close_enough(start, -2.0) && close_enough(end, 0.0))
	result = BRLCAD_OK;
    rt_db_free_internal(&intern);
    return result;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1 || test_db5_round_trip() != BRLCAD_OK ||
	test_data_round_trip() != BRLCAD_OK ||
	test_duplicate_target_rejected() != BRLCAD_OK ||
	test_evaluation() != BRLCAD_OK ||
	test_negative_time() != BRLCAD_OK)
	return 1;
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
