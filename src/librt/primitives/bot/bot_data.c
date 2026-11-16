/*                      B O T _ D A T A . C
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

#include "raytrace.h"
#include "rt/object_io.h"
#include "rt/primitives/bot.h"

#define BOT_DATA_VERSION 1u

static int
bot_data_set(struct rt_data_node *object, const char *name,
	struct rt_data_node *value)
{
    if (value && rt_data_object_set(object, name, value) == BRLCAD_OK)
	return BRLCAD_OK;
    rt_data_free(value);
    return BRLCAD_ERROR;
}

static struct rt_data_node *
bot_data_doubles(const fastf_t *values, size_t count)
{
    struct rt_data_node *array = rt_data_array();
    size_t i;

    if (!array || (!values && count))
	goto fail;
    for (i = 0; i < count; i++) {
	struct rt_data_node *value = rt_data_double(values[i]);
	if (!value || rt_data_array_append(array, value) != BRLCAD_OK) {
	    rt_data_free(value);
	    goto fail;
	}
    }
    return array;

fail:
    rt_data_free(array);
    return NULL;
}

static struct rt_data_node *
bot_data_ints(const int *values, size_t count)
{
    struct rt_data_node *array = rt_data_array();
    size_t i;

    if (!array || (!values && count))
	goto fail;
    for (i = 0; i < count; i++) {
	struct rt_data_node *value = rt_data_int(values[i]);
	if (!value || rt_data_array_append(array, value) != BRLCAD_OK) {
	    rt_data_free(value);
	    goto fail;
	}
    }
    return array;

fail:
    rt_data_free(array);
    return NULL;
}

static struct rt_data_node *
bot_data_uints(const uint32_t *values, size_t count)
{
    struct rt_data_node *array = rt_data_array();
    size_t i;

    if (!array || (!values && count))
	goto fail;
    for (i = 0; i < count; i++) {
	struct rt_data_node *value = rt_data_uint(values[i]);
	if (!value || rt_data_array_append(array, value) != BRLCAD_OK) {
	    rt_data_free(value);
	    goto fail;
	}
    }
    return array;

fail:
    rt_data_free(array);
    return NULL;
}

static struct rt_data_node *
bot_data_face_modes(const struct rt_bot_internal *bot)
{
    struct rt_data_node *array = rt_data_array();
    size_t face;

    if (!array || !bot->face_mode)
	goto fail;
    for (face = 0; face < bot->num_faces; face++) {
	struct rt_data_node *value = rt_data_bool(BU_BITTEST(bot->face_mode, face));
	if (!value || rt_data_array_append(array, value) != BRLCAD_OK) {
	    rt_data_free(value);
	    goto fail;
	}
    }
    return array;

fail:
    rt_data_free(array);
    return NULL;
}

static struct rt_data_node *
bot_data_skin(const struct rt_bot_internal *bot)
{
    const struct rt_bot_skin *skin = bot->skin;
    struct rt_data_node *object = NULL;
    struct rt_data_node *links = NULL;
    size_t joint;

    if (!skin)
	return rt_data_null();
    if (rt_bot_skin_validate(bot, NULL) != BRLCAD_OK)
	return NULL;
    object = rt_data_object();
    links = rt_data_array();
    if (!object || !links)
	goto fail;
    for (joint = 0; joint < skin->joint_count; joint++) {
	struct rt_data_node *link = rt_data_string(
	    bu_vls_cstr(&skin->joint_links[joint]));
	if (!link || rt_data_array_append(links, link) != BRLCAD_OK) {
	    rt_data_free(link);
	    goto fail;
	}
    }
    {
	struct rt_data_node *owned_links = links;
	links = NULL;
	if (bot_data_set(object, "joint_links", owned_links) != BRLCAD_OK)
	    goto fail;
    }
    if (bot_data_set(object, "inverse_bind_matrices",
	    bot_data_doubles(&skin->inverse_bind_matrices[0][0],
		skin->joint_count * ELEMENTS_PER_MAT)) != BRLCAD_OK ||
	bot_data_set(object, "vertex_offsets",
	    bot_data_uints(skin->vertex_offsets, bot->num_vertices + 1)) != BRLCAD_OK ||
	bot_data_set(object, "joint_indices",
	    bot_data_uints(skin->joint_indices, skin->influence_count)) != BRLCAD_OK ||
	bot_data_set(object, "weights",
	    bot_data_doubles(skin->weights, skin->influence_count)) != BRLCAD_OK)
	goto fail;
    return object;

fail:
    rt_data_free(links);
    rt_data_free(object);
    return NULL;
}

int
rt_bot_data_export(struct rt_data_node **result,
	const struct rt_db_internal *intern,
	const struct rt_data_context *UNUSED(context))
{
    const struct rt_bot_internal *bot;
    struct rt_data_node *data;
    int plate;

    if (!result || !intern)
	return BRLCAD_ERROR;
    *result = NULL;
    RT_CK_DB_INTERNAL(intern);
    bot = (const struct rt_bot_internal *)intern->idb_ptr;
    RT_BOT_CK_MAGIC(bot);
    if (bot->num_vertices > SIZE_MAX / (ELEMENTS_PER_VECT * sizeof(fastf_t)) ||
	bot->num_faces > SIZE_MAX / (ELEMENTS_PER_VECT * sizeof(int)) ||
	bot->num_normals > SIZE_MAX / (ELEMENTS_PER_VECT * sizeof(fastf_t)) ||
	bot->num_face_normals > SIZE_MAX / (ELEMENTS_PER_VECT * sizeof(int)) ||
	bot->num_uvs > SIZE_MAX / (ELEMENTS_PER_VECT * sizeof(fastf_t)) ||
	bot->num_face_uvs > SIZE_MAX / (ELEMENTS_PER_VECT * sizeof(int)))
	return BRLCAD_ERROR;
    plate = bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS;
    data = rt_data_object();
    if (!data ||
	bot_data_set(data, "version", rt_data_uint(BOT_DATA_VERSION)) != BRLCAD_OK ||
	bot_data_set(data, "mode", rt_data_uint(bot->mode)) != BRLCAD_OK ||
	bot_data_set(data, "orientation", rt_data_uint(bot->orientation)) != BRLCAD_OK ||
	bot_data_set(data, "flags", rt_data_uint(bot->bot_flags)) != BRLCAD_OK ||
	bot_data_set(data, "vertices", bot_data_doubles(bot->vertices,
		bot->num_vertices * ELEMENTS_PER_VECT)) != BRLCAD_OK ||
	bot_data_set(data, "faces", bot_data_ints(bot->faces,
		bot->num_faces * ELEMENTS_PER_VECT)) != BRLCAD_OK ||
	bot_data_set(data, "thickness", plate ?
	    bot_data_doubles(bot->thickness, bot->num_faces) : rt_data_null()) != BRLCAD_OK ||
	bot_data_set(data, "face_mode", plate ?
	    bot_data_face_modes(bot) : rt_data_null()) != BRLCAD_OK ||
	bot_data_set(data, "normals", bot->normals ?
	    bot_data_doubles(bot->normals, bot->num_normals * ELEMENTS_PER_VECT) :
	    rt_data_null()) != BRLCAD_OK ||
	bot_data_set(data, "face_normals", bot->face_normals ?
	    bot_data_ints(bot->face_normals,
		bot->num_face_normals * ELEMENTS_PER_VECT) : rt_data_null()) != BRLCAD_OK ||
	bot_data_set(data, "uvs", bot->uvs ?
	    bot_data_doubles(bot->uvs, bot->num_uvs * ELEMENTS_PER_VECT) :
	    rt_data_null()) != BRLCAD_OK ||
	bot_data_set(data, "face_uvs", bot->face_uvs ?
	    bot_data_ints(bot->face_uvs,
		bot->num_face_uvs * ELEMENTS_PER_VECT) : rt_data_null()) != BRLCAD_OK ||
	bot_data_set(data, "skin", bot_data_skin(bot)) != BRLCAD_OK) {
	rt_data_free(data);
	return BRLCAD_ERROR;
    }
    *result = data;
    return BRLCAD_OK;
}

static int
bot_data_array_count(const struct rt_data_node *array, size_t tuple_size,
	size_t *count)
{
    size_t elements;

    if (!array || rt_data_node_type(array) != RT_DATA_ARRAY || !tuple_size)
	return BRLCAD_ERROR;
    elements = rt_data_array_size(array);
    if (!elements || elements % tuple_size)
	return BRLCAD_ERROR;
    *count = elements / tuple_size;
    return BRLCAD_OK;
}

static int
bot_data_read_doubles(fastf_t *values, size_t count,
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

static int
bot_data_read_ints(int *values, size_t count, const struct rt_data_node *array)
{
    size_t i;

    if (!values || !array || rt_data_node_type(array) != RT_DATA_ARRAY ||
	rt_data_array_size(array) != count)
	return BRLCAD_ERROR;
    for (i = 0; i < count; i++) {
	int64_t value;
	const struct rt_data_node *node = rt_data_array_get(array, i);
	if (rt_data_get_int(node, &value) != BRLCAD_OK) {
	    uint64_t unsigned_value;
	    if (rt_data_get_uint(node, &unsigned_value) != BRLCAD_OK ||
		unsigned_value > INT_MAX)
		return BRLCAD_ERROR;
	    value = (int64_t)unsigned_value;
	}
	if (value < INT_MIN || value > INT_MAX)
	    return BRLCAD_ERROR;
	values[i] = (int)value;
    }
    return BRLCAD_OK;
}

static int
bot_data_read_uints(uint32_t *values, size_t count,
	const struct rt_data_node *array)
{
    size_t i;

    if (!values || !array || rt_data_node_type(array) != RT_DATA_ARRAY ||
	rt_data_array_size(array) != count)
	return BRLCAD_ERROR;
    for (i = 0; i < count; i++) {
	uint64_t value;
	if (rt_data_get_uint(rt_data_array_get(array, i), &value) != BRLCAD_OK ||
	    value > UINT32_MAX)
	    return BRLCAD_ERROR;
	values[i] = (uint32_t)value;
    }
    return BRLCAD_OK;
}

static int
bot_data_optional_doubles(fastf_t **values, size_t *tuple_count,
	const struct rt_data_node *node)
{
    size_t count;

    if (!node || rt_data_node_type(node) == RT_DATA_NULL) {
	*values = NULL;
	*tuple_count = 0;
	return BRLCAD_OK;
    }
    if (bot_data_array_count(node, ELEMENTS_PER_VECT, &count) != BRLCAD_OK ||
	count > SIZE_MAX / ELEMENTS_PER_VECT)
	return BRLCAD_ERROR;
    *values = (fastf_t *)bu_malloc(count * ELEMENTS_PER_VECT * sizeof(fastf_t),
	"BOT neutral double tuples");
    if (bot_data_read_doubles(*values, count * ELEMENTS_PER_VECT, node) != BRLCAD_OK) {
	bu_free(*values, "BOT neutral double tuples");
	*values = NULL;
	return BRLCAD_ERROR;
    }
    *tuple_count = count;
    return BRLCAD_OK;
}

static int
bot_data_optional_ints(int **values, size_t *tuple_count,
	const struct rt_data_node *node)
{
    size_t count;

    if (!node || rt_data_node_type(node) == RT_DATA_NULL) {
	*values = NULL;
	*tuple_count = 0;
	return BRLCAD_OK;
    }
    if (bot_data_array_count(node, ELEMENTS_PER_VECT, &count) != BRLCAD_OK ||
	count > SIZE_MAX / ELEMENTS_PER_VECT)
	return BRLCAD_ERROR;
    *values = (int *)bu_malloc(count * ELEMENTS_PER_VECT * sizeof(int),
	"BOT neutral integer tuples");
    if (bot_data_read_ints(*values, count * ELEMENTS_PER_VECT, node) != BRLCAD_OK) {
	bu_free(*values, "BOT neutral integer tuples");
	*values = NULL;
	return BRLCAD_ERROR;
    }
    *tuple_count = count;
    return BRLCAD_OK;
}

static int
bot_data_read_skin(struct rt_bot_internal *bot, const struct rt_data_node *node)
{
    struct rt_bot_skin *skin;
    const struct rt_data_node *links;
    const struct rt_data_node *matrices;
    const struct rt_data_node *offsets;
    const struct rt_data_node *indices;
    const struct rt_data_node *weights;
    size_t joint;

    if (!node || rt_data_node_type(node) == RT_DATA_NULL)
	return (bot->bot_flags & RT_BOT_HAS_SKIN) ? BRLCAD_ERROR : BRLCAD_OK;
    if (rt_data_node_type(node) != RT_DATA_OBJECT)
	return BRLCAD_ERROR;
    links = rt_data_object_get(node, "joint_links");
    matrices = rt_data_object_get(node, "inverse_bind_matrices");
    offsets = rt_data_object_get(node, "vertex_offsets");
    indices = rt_data_object_get(node, "joint_indices");
    weights = rt_data_object_get(node, "weights");
    if (!links || rt_data_node_type(links) != RT_DATA_ARRAY ||
	!rt_data_array_size(links) || rt_data_array_size(links) > UINT32_MAX ||
	!indices || rt_data_node_type(indices) != RT_DATA_ARRAY ||
	!rt_data_array_size(indices) || rt_data_array_size(indices) > UINT32_MAX)
	return BRLCAD_ERROR;
    BU_ALLOC(skin, struct rt_bot_skin);
    bot->skin = skin;
    skin->joint_count = rt_data_array_size(links);
    skin->influence_count = rt_data_array_size(indices);
    skin->joint_links = (struct bu_vls *)bu_calloc(skin->joint_count,
	sizeof(struct bu_vls), "BOT neutral skin links");
    skin->inverse_bind_matrices = (mat_t *)bu_malloc(skin->joint_count *
	sizeof(mat_t), "BOT neutral inverse binds");
    skin->vertex_offsets = (uint32_t *)bu_malloc((bot->num_vertices + 1) *
	sizeof(uint32_t), "BOT neutral skin offsets");
    skin->joint_indices = (uint32_t *)bu_malloc(skin->influence_count *
	sizeof(uint32_t), "BOT neutral skin indices");
    skin->weights = (fastf_t *)bu_malloc(skin->influence_count *
	sizeof(fastf_t), "BOT neutral skin weights");
    for (joint = 0; joint < skin->joint_count; joint++) {
	const char *link = rt_data_get_string(rt_data_array_get(links, joint));
	bu_vls_init(&skin->joint_links[joint]);
	if (!link || !link[0])
	    return BRLCAD_ERROR;
	bu_vls_strcpy(&skin->joint_links[joint], link);
    }
    if (bot_data_read_doubles(&skin->inverse_bind_matrices[0][0],
	    skin->joint_count * ELEMENTS_PER_MAT, matrices) != BRLCAD_OK ||
	bot_data_read_uints(skin->vertex_offsets, bot->num_vertices + 1,
	    offsets) != BRLCAD_OK ||
	bot_data_read_uints(skin->joint_indices, skin->influence_count,
	    indices) != BRLCAD_OK ||
	bot_data_read_doubles(skin->weights, skin->influence_count,
	    weights) != BRLCAD_OK ||
	rt_bot_skin_validate(bot, NULL) != BRLCAD_OK)
	return BRLCAD_ERROR;
    return BRLCAD_OK;
}

int
rt_bot_data_import(struct rt_db_internal *intern,
	const struct rt_data_node *data,
	const struct rt_data_context *UNUSED(context))
{
    struct rt_bot_internal *bot;
    struct rt_bot_internal *candidate;
    const struct rt_data_node *vertices;
    const struct rt_data_node *faces;
    const struct rt_data_node *thickness;
    const struct rt_data_node *face_modes;
    uint64_t version;
    uint64_t mode;
    uint64_t orientation;
    uint64_t flags;
    size_t face;
    int plate;

    if (!intern || !data || rt_data_node_type(data) != RT_DATA_OBJECT ||
	rt_data_get_uint(rt_data_object_get(data, "version"), &version) != BRLCAD_OK ||
	version != BOT_DATA_VERSION ||
	rt_data_get_uint(rt_data_object_get(data, "mode"), &mode) != BRLCAD_OK ||
	mode < RT_BOT_SURFACE || mode > RT_BOT_PLATE_NOCOS ||
	rt_data_get_uint(rt_data_object_get(data, "orientation"),
	    &orientation) != BRLCAD_OK ||
	orientation < RT_BOT_UNORIENTED || orientation > RT_BOT_CW ||
	rt_data_get_uint(rt_data_object_get(data, "flags"), &flags) != BRLCAD_OK ||
	flags > UINT8_MAX)
	return BRLCAD_ERROR;
    bot = (struct rt_bot_internal *)intern->idb_ptr;
    RT_BOT_CK_MAGIC(bot);
    vertices = rt_data_object_get(data, "vertices");
    faces = rt_data_object_get(data, "faces");
    BU_ALLOC(candidate, struct rt_bot_internal);
    candidate->magic = RT_BOT_INTERNAL_MAGIC;
    candidate->mode = (unsigned char)mode;
    candidate->orientation = (unsigned char)orientation;
    candidate->bot_flags = (unsigned char)flags;
    if (bot_data_array_count(vertices, ELEMENTS_PER_VECT,
	    &candidate->num_vertices) != BRLCAD_OK ||
	bot_data_array_count(faces, ELEMENTS_PER_VECT,
	    &candidate->num_faces) != BRLCAD_OK ||
	candidate->num_vertices > SIZE_MAX /
	    (ELEMENTS_PER_VECT * sizeof(fastf_t)) ||
	candidate->num_faces > SIZE_MAX /
	    (ELEMENTS_PER_VECT * sizeof(int)))
	goto fail;
    candidate->vertices = (fastf_t *)bu_malloc(candidate->num_vertices *
	ELEMENTS_PER_VECT * sizeof(fastf_t), "BOT neutral vertices");
    candidate->faces = (int *)bu_malloc(candidate->num_faces *
	ELEMENTS_PER_VECT * sizeof(int), "BOT neutral faces");
    if (bot_data_read_doubles(candidate->vertices, candidate->num_vertices *
	    ELEMENTS_PER_VECT, vertices) != BRLCAD_OK ||
	bot_data_read_ints(candidate->faces, candidate->num_faces *
	    ELEMENTS_PER_VECT, faces) != BRLCAD_OK)
	goto fail;
    for (face = 0; face < candidate->num_faces * ELEMENTS_PER_VECT; face++) {
	if (candidate->faces[face] < 0 ||
	    (size_t)candidate->faces[face] >= candidate->num_vertices)
	    goto fail;
    }
    plate = candidate->mode == RT_BOT_PLATE ||
	candidate->mode == RT_BOT_PLATE_NOCOS;
    thickness = rt_data_object_get(data, "thickness");
    face_modes = rt_data_object_get(data, "face_mode");
    if (plate) {
	if (!thickness || rt_data_node_type(thickness) != RT_DATA_ARRAY ||
	    rt_data_array_size(thickness) != candidate->num_faces ||
	    !face_modes || rt_data_node_type(face_modes) != RT_DATA_ARRAY ||
	    rt_data_array_size(face_modes) != candidate->num_faces)
	    goto fail;
	candidate->thickness = (fastf_t *)bu_malloc(candidate->num_faces *
	    sizeof(fastf_t), "BOT neutral thickness");
	candidate->face_mode = bu_bitv_new(candidate->num_faces);
	if (bot_data_read_doubles(candidate->thickness, candidate->num_faces,
		thickness) != BRLCAD_OK)
	    goto fail;
	for (face = 0; face < candidate->num_faces; face++) {
	    int appended;
	    if (rt_data_get_bool(rt_data_array_get(face_modes, face),
		    &appended) != BRLCAD_OK)
		goto fail;
	    if (appended)
		BU_BITSET(candidate->face_mode, face);
	}
    } else if ((thickness && rt_data_node_type(thickness) != RT_DATA_NULL) ||
	(face_modes && rt_data_node_type(face_modes) != RT_DATA_NULL)) {
	goto fail;
    }
    if (bot_data_optional_doubles(&candidate->normals,
	    &candidate->num_normals, rt_data_object_get(data, "normals")) != BRLCAD_OK ||
	bot_data_optional_ints(&candidate->face_normals,
	    &candidate->num_face_normals,
	    rt_data_object_get(data, "face_normals")) != BRLCAD_OK ||
	bot_data_optional_doubles(&candidate->uvs,
	    &candidate->num_uvs, rt_data_object_get(data, "uvs")) != BRLCAD_OK ||
	bot_data_optional_ints(&candidate->face_uvs,
	    &candidate->num_face_uvs,
	    rt_data_object_get(data, "face_uvs")) != BRLCAD_OK ||
	bot_data_read_skin(candidate, rt_data_object_get(data, "skin")) != BRLCAD_OK)
	goto fail;
    if (((candidate->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) != 0) !=
	(candidate->normals && candidate->face_normals) ||
	((candidate->bot_flags & RT_BOT_HAS_TEXTURE_UVS) != 0) !=
	(candidate->uvs && candidate->face_uvs) ||
	(candidate->face_normals && candidate->num_face_normals != candidate->num_faces) ||
	(candidate->face_uvs && candidate->num_face_uvs != candidate->num_faces))
	goto fail;
    for (face = 0; face < candidate->num_face_normals * ELEMENTS_PER_VECT;
	face++) {
	if (candidate->face_normals[face] < 0 ||
	    (size_t)candidate->face_normals[face] >= candidate->num_normals)
	    goto fail;
    }
    for (face = 0; face < candidate->num_face_uvs * ELEMENTS_PER_VECT;
	face++) {
	if (candidate->face_uvs[face] < 0 ||
	    (size_t)candidate->face_uvs[face] >= candidate->num_uvs)
	    goto fail;
    }
    rt_bot_internal_free(bot);
    *bot = *candidate;
    bu_free(candidate, "BOT neutral candidate");
    return BRLCAD_OK;

fail:
    rt_bot_internal_free(candidate);
    bu_free(candidate, "BOT neutral candidate");
    return BRLCAD_ERROR;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-file-style: "stroustrup"
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
