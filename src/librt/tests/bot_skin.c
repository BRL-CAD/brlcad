/*                     B O T _ S K I N . C
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

#include "bu/app.h"
#include "raytrace.h"
#include "rt/object_io.h"
#include "rt/primitives/bot.h"

static int
close_enough(fastf_t first, fastf_t second)
{
    return NEAR_EQUAL(first, second, 1.0e-9);
}

static int
make_bot(struct rt_db_internal *intern)
{
    static const fastf_t vertices[] = {
	0.0, 0.0, 0.0,
	1.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	2.0, 2.0, 0.0
    };
    static const int faces[] = {0, 1, 2};
    static const uint32_t offsets[] = {0, 1, 3, 4, 5};
    static const uint32_t indices[] = {0, 0, 1, 1, 0};
    static const fastf_t weights[] = {1.0, 0.25, 0.75, 1.0, 1.0};
    struct rt_bot_internal *bot;
    struct rt_bot_skin *skin;

    RT_DB_INTERNAL_INIT(intern);
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SURFACE;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = RT_BOT_HAS_SKIN;
    bot->num_vertices = 4;
    bot->vertices = (fastf_t *)bu_malloc(sizeof(vertices), "test BOT vertices");
    memcpy(bot->vertices, vertices, sizeof(vertices));
    bot->num_faces = 1;
    bot->faces = (int *)bu_malloc(sizeof(faces), "test BOT faces");
    memcpy(bot->faces, faces, sizeof(faces));

    BU_ALLOC(skin, struct rt_bot_skin);
    skin->joint_count = 2;
    skin->joint_links = (struct bu_vls *)bu_calloc(2, sizeof(struct bu_vls),
	"test BOT skin joints");
    bu_vls_init(&skin->joint_links[0]);
    bu_vls_init(&skin->joint_links[1]);
    bu_vls_strcpy(&skin->joint_links[0], "root");
    bu_vls_strcpy(&skin->joint_links[1], "child");
    skin->inverse_bind_matrices = (mat_t *)bu_malloc(2 * sizeof(mat_t),
	"test BOT inverse binds");
    MAT_IDN(skin->inverse_bind_matrices[0]);
    MAT_IDN(skin->inverse_bind_matrices[1]);
    skin->inverse_bind_matrices[1][3] = -1.0;
    skin->influence_count = 5;
    skin->vertex_offsets = (uint32_t *)bu_malloc(sizeof(offsets),
	"test BOT skin offsets");
    skin->joint_indices = (uint32_t *)bu_malloc(sizeof(indices),
	"test BOT skin indices");
    skin->weights = (fastf_t *)bu_malloc(sizeof(weights),
	"test BOT skin weights");
    memcpy(skin->vertex_offsets, offsets, sizeof(offsets));
    memcpy(skin->joint_indices, indices, sizeof(indices));
    memcpy(skin->weights, weights, sizeof(weights));
    bot->skin = skin;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_BOT;
    intern->idb_meth = &OBJ[ID_BOT];
    intern->idb_ptr = bot;
    return BRLCAD_OK;
}

static int
check_bot(const struct rt_db_internal *intern)
{
    const struct rt_bot_internal *bot =
	(const struct rt_bot_internal *)intern->idb_ptr;

    if (!bot || rt_bot_skin_validate(bot, NULL) != BRLCAD_OK ||
	bot->skin->joint_count != 2 || bot->skin->influence_count != 5 ||
	!BU_STR_EQUAL(bu_vls_cstr(&bot->skin->joint_links[1]), "child") ||
	bot->skin->vertex_offsets[2] != 3 || bot->skin->joint_indices[2] != 1 ||
	!close_enough(bot->skin->weights[1], 0.25) ||
	!close_enough(bot->skin->inverse_bind_matrices[1][3], -1.0))
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

    make_bot(&source);
    RT_DB_INTERNAL_INIT(&restored);
    if (OBJ[ID_BOT].ft_export5(&external, &source, 1.0, NULL) != BRLCAD_OK ||
	OBJ[ID_BOT].ft_import5(&restored, &external, bn_mat_identity, NULL) != BRLCAD_OK)
	goto cleanup;
    result = check_bot(&restored);

cleanup:
    bu_free_external(&external);
    rt_db_free_internal(&source);
    rt_db_free_internal(&restored);
    return result;
}

static int
test_neutral_round_trip(void)
{
    static const fastf_t normals[] = {
	0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0
    };
    static const int face_normals[] = {0, 1, 2};
    static const fastf_t uvs[] = {
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0
    };
    static const int face_uvs[] = {0, 1, 2};
    struct rt_db_internal source;
    struct rt_db_internal restored;
    struct rt_data_context context = RT_DATA_CONTEXT_INIT;
    struct rt_data_node *data = NULL;
    int result = BRLCAD_ERROR;

    make_bot(&source);
    {
	struct rt_bot_internal *bot = (struct rt_bot_internal *)source.idb_ptr;
	bot->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_HAS_TEXTURE_UVS;
	bot->num_normals = 3;
	bot->normals = (fastf_t *)bu_malloc(sizeof(normals), "neutral BOT normals");
	memcpy(bot->normals, normals, sizeof(normals));
	bot->num_face_normals = 1;
	bot->face_normals = (int *)bu_malloc(sizeof(face_normals),
	    "neutral BOT face normals");
	memcpy(bot->face_normals, face_normals, sizeof(face_normals));
	bot->num_uvs = 3;
	bot->uvs = (fastf_t *)bu_malloc(sizeof(uvs), "neutral BOT UVs");
	memcpy(bot->uvs, uvs, sizeof(uvs));
	bot->num_face_uvs = 1;
	bot->face_uvs = (int *)bu_malloc(sizeof(face_uvs),
	    "neutral BOT face UVs");
	memcpy(bot->face_uvs, face_uvs, sizeof(face_uvs));
    }
    RT_DB_INTERNAL_INIT(&restored);
    context.media_type = "model/vnd.brlcad+json";
    if (rt_obj_data_export(&data, &source, &context) != BRLCAD_OK ||
	rt_obj_data_import(&restored, data, &context) != BRLCAD_OK)
	goto cleanup;
    if (check_bot(&restored) == BRLCAD_OK) {
	const struct rt_bot_internal *bot =
	    (const struct rt_bot_internal *)restored.idb_ptr;
	if (bot->num_normals == 3 && bot->num_face_normals == 1 &&
	    bot->face_normals[2] == 2 && bot->num_uvs == 3 &&
	    bot->num_face_uvs == 1 && bot->face_uvs[1] == 1)
	    result = BRLCAD_OK;
    }

cleanup:
    rt_data_free(data);
    rt_db_free_internal(&source);
    rt_db_free_internal(&restored);
    return result;
}

static int
test_copy_and_transform(void)
{
    struct rt_db_internal source;
    struct rt_db_internal transformed;
    const struct rt_bot_skin *source_skin;
    mat_t matrix = MAT_INIT_IDN;
    int result = BRLCAD_ERROR;

    make_bot(&source);
    source_skin = ((struct rt_bot_internal *)source.idb_ptr)->skin;
    RT_DB_INTERNAL_INIT(&transformed);
    MAT_DELTAS(matrix, 4.0, 5.0, 6.0);
    if (OBJ[ID_BOT].ft_xform(&transformed, matrix, &source, 1, NULL) != BRLCAD_OK)
	goto cleanup;
    {
	const struct rt_bot_internal *bot =
	    (const struct rt_bot_internal *)transformed.idb_ptr;
	mat_t joint_matrices[2];
	struct rt_bot_internal *posed = NULL;
	MAT_IDN(joint_matrices[0]);
	MAT_IDN(joint_matrices[1]);
	MAT_DELTAS(joint_matrices[0], 4.0, 5.0, 6.0);
	MAT_DELTAS(joint_matrices[1], 5.0, 5.0, 6.0);
	if (source.idb_ptr || bot->skin == source_skin ||
	    !close_enough(bot->vertices[0], 4.0) ||
	    !close_enough(bot->skin->inverse_bind_matrices[0][3], -4.0) ||
	    !close_enough(bot->skin->inverse_bind_matrices[1][3], -5.0) ||
	    rt_bot_skin_pose(&posed, bot, joint_matrices, 2, NULL) != BRLCAD_OK ||
	    !posed || !close_enough(posed->vertices[0], 4.0) ||
	    !close_enough(posed->vertices[3], 5.0)) {
	    bu_log("BOT skin transform failed: source=%p shared=%d vertex=%.17g binds=(%.17g %.17g) posed=(%.17g %.17g)\n",
		(void *)source.idb_ptr, bot->skin == source_skin, bot->vertices[0],
		bot->skin->inverse_bind_matrices[0][3],
		bot->skin->inverse_bind_matrices[1][3],
		posed ? posed->vertices[0] : -MAX_FASTF,
		posed ? posed->vertices[3] : -MAX_FASTF);
	    rt_bot_internal_free(posed);
	    goto cleanup;
	}
	rt_bot_internal_free(posed);
    }
    result = BRLCAD_OK;

cleanup:
    rt_db_free_internal(&source);
    rt_db_free_internal(&transformed);
    return result;
}

static int
test_subset_and_condense(void)
{
    struct rt_db_internal source;
    struct rt_bot_internal *subset;
    int face = 0;
    int result = BRLCAD_ERROR;

    make_bot(&source);
    subset = rt_bot_subset((struct rt_bot_internal *)source.idb_ptr, &face, 1);
    if (!subset || subset->num_vertices != 3 ||
	rt_bot_skin_validate(subset, NULL) != BRLCAD_OK ||
	subset->skin->influence_count != 4)
	goto cleanup;
    if (rt_bot_condense((struct rt_bot_internal *)source.idb_ptr) != 1 ||
	((struct rt_bot_internal *)source.idb_ptr)->num_vertices != 3 ||
	rt_bot_skin_validate((struct rt_bot_internal *)source.idb_ptr, NULL) != BRLCAD_OK ||
	((struct rt_bot_internal *)source.idb_ptr)->skin->influence_count != 4)
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    if (subset) {
	rt_bot_internal_free(subset);
	BU_PUT(subset, struct rt_bot_internal);
    }
    rt_db_free_internal(&source);
    return result;
}

static int
test_invalid_index(void)
{
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    int result;

    make_bot(&intern);
    bot = (struct rt_bot_internal *)intern.idb_ptr;
    bot->skin->joint_indices[0] = 2;
    result = rt_bot_skin_validate(bot, NULL) == BRLCAD_ERROR ?
	BRLCAD_OK : BRLCAD_ERROR;
    rt_db_free_internal(&intern);
    return result;
}

static int
test_skin_pose(void)
{
    struct rt_db_internal source;
    struct rt_bot_internal *posed = NULL;
    mat_t joint_matrices[2] = {MAT_INIT_IDN, MAT_INIT_IDN};
    int result = BRLCAD_ERROR;

    make_bot(&source);
    MAT_DELTAS(joint_matrices[1], 3.0, 0.0, 0.0);
    if (rt_bot_skin_pose(&posed,
	    (const struct rt_bot_internal *)source.idb_ptr,
	    joint_matrices, 2, NULL) != BRLCAD_OK || !posed)
	goto cleanup;
    if (posed->skin || (posed->bot_flags & RT_BOT_HAS_SKIN) ||
	!close_enough(posed->vertices[0], 0.0) ||
	!close_enough(posed->vertices[3], 2.5) ||
	!close_enough(posed->vertices[6], 2.0) ||
	!close_enough(posed->vertices[7], 1.0) ||
	!close_enough(posed->vertices[9], 2.0) ||
	!close_enough(((struct rt_bot_internal *)source.idb_ptr)->vertices[3], 1.0) ||
	rt_bot_skin_validate((struct rt_bot_internal *)source.idb_ptr, NULL) != BRLCAD_OK)
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    if (posed) {
	rt_bot_internal_free(posed);
	BU_PUT(posed, struct rt_bot_internal);
    }
    rt_db_free_internal(&source);
    return result;
}

static int
test_duplicate_and_merge(void)
{
    struct rt_db_internal intern;
    struct rt_bot_internal *duplicate = NULL;
    struct rt_bot_internal *merged = NULL;
    const struct rt_bot_internal *inputs[2];
    int result = BRLCAD_ERROR;

    make_bot(&intern);
    inputs[0] = (const struct rt_bot_internal *)intern.idb_ptr;
    inputs[1] = inputs[0];
    duplicate = rt_bot_dup(inputs[0]);
    merged = rt_bot_merge(2, inputs);
    if (!duplicate || !merged || duplicate->skin == inputs[0]->skin ||
	rt_bot_skin_validate(duplicate, NULL) != BRLCAD_OK ||
	merged->num_vertices != 8 || merged->skin->influence_count != 10 ||
	rt_bot_skin_validate(merged, NULL) != BRLCAD_OK)
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    if (duplicate) {
	rt_bot_internal_free(duplicate);
	BU_PUT(duplicate, struct rt_bot_internal);
    }
    if (merged) {
	rt_bot_internal_free(merged);
	BU_PUT(merged, struct rt_bot_internal);
    }
    rt_db_free_internal(&intern);
    return result;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    (void)argc;
    if (test_db5_round_trip() != BRLCAD_OK ||
	test_neutral_round_trip() != BRLCAD_OK ||
	test_copy_and_transform() != BRLCAD_OK ||
	test_subset_and_condense() != BRLCAD_OK ||
	test_duplicate_and_merge() != BRLCAD_OK ||
	test_skin_pose() != BRLCAD_OK ||
	test_invalid_index() != BRLCAD_OK)
	return 1;
    return 0;
}
