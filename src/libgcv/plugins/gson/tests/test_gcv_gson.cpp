/*              T E S T _ G C V _ G S O N . C P P
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

#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/ptbl.h"
#include "gcv/api.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/animation.h"
#include "rt/primitives/bot.h"
#include "rt/primitives/joint.h"
#include "wdb.h"

namespace {

void
require(bool condition, const std::string &message)
{
    if (!condition)
	throw std::runtime_error(message);
}

class context {
public:
    context() { gcv_context_init(&value); }
    ~context() { gcv_context_destroy(&value); }
    context(const context &) = delete;
    context &operator=(const context &) = delete;

    int execute(enum gcv_filter_type type, const char *path)
    {
	const struct bu_ptbl *filters = gcv_list_filters(&value);
	for (size_t index = 0; index < BU_PTBL_LEN(filters); ++index) {
	    const auto *filter = reinterpret_cast<const struct gcv_filter *>(
		BU_PTBL_GET(filters, index));
	    if (filter->filter_type == type &&
		filter->mime_type == BU_MIME_MODEL_VND_BRLCAD_PLUS_JSON)
		return gcv_execute(&value, filter, nullptr, 0, nullptr, path);
	}
	throw std::runtime_error("GSON plugin was not loaded");
    }

    struct gcv_context value;
};

void
add_joint(context &destination)
{
    const char *arguments[] = {
	"type", "general", "parent", "root", "child", "tool",
	"dof0", "translation limited {0 0 1} -2 8 5.1234567890123457"
    };
    struct rt_db_internal internal;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    struct rt_wdb *wdbp = wdb_dbopen(destination.value.dbip,
	RT_WDB_TYPE_DB_INMEM);
    point_t origin = VINIT_ZERO;

    RT_DB_INTERNAL_INIT(&internal);
    int status = OBJ[ID_JOINT].ft_make(&OBJ[ID_JOINT], &internal, nullptr,
	origin, 1.0);
    if (status == BRLCAD_OK)
	status = OBJ[ID_JOINT].ft_adjust(&messages, &internal,
	    static_cast<int>(sizeof(arguments) / sizeof(arguments[0])), arguments);
    if (status == BRLCAD_OK)
	status = wdb_put_internal(wdbp, "tool.j", &internal, 1.0);
    rt_db_free_internal(&internal);
    bu_vls_free(&messages);
    require(status == BRLCAD_OK, "unable to create GSON joint fixture");
}

void
add_animation(context &destination)
{
    static const fastf_t times[] = {0.0, 2.0};
    static const fastf_t values[] = {
	0.0, 0.0, 0.0, 12.345678901234567, 0.0, 0.0
    };
    struct rt_db_internal internal;
    struct rt_wdb *wdbp = wdb_dbopen(destination.value.dbip,
	RT_WDB_TYPE_DB_INMEM);
    point_t origin = VINIT_ZERO;

    RT_DB_INTERNAL_INIT(&internal);
    require(OBJ[ID_ANIMATION].ft_make(&OBJ[ID_ANIMATION], &internal, nullptr,
	origin, 1.0) == BRLCAD_OK, "unable to make GSON animation fixture");
    auto *animation = static_cast<struct rt_animation_internal *>(internal.idb_ptr);
    animation->channel_count = 1;
    animation->channels = static_cast<struct rt_animation_channel *>(bu_calloc(
	1, sizeof(struct rt_animation_channel), "GSON animation channel"));
    struct rt_animation_channel &channel = animation->channels[0];
    bu_vls_init(&channel.target_link);
    bu_vls_strcpy(&channel.target_link, "tool");
    channel.target = RT_ANIMATION_TRANSLATION;
    channel.interpolation = RT_ANIMATION_LINEAR;
    channel.keyframe_count = 2;
    channel.times = static_cast<fastf_t *>(bu_malloc(sizeof(times),
	"GSON animation times"));
    channel.values = static_cast<fastf_t *>(bu_malloc(sizeof(values),
	"GSON animation values"));
    memcpy(channel.times, times, sizeof(times));
    memcpy(channel.values, values, sizeof(values));
    int status = rt_animation_validate(animation, nullptr) == 0 ?
	wdb_put_internal(wdbp, "motion.anim", &internal, 1.0) : BRLCAD_ERROR;
    rt_db_free_internal(&internal);
    require(status == BRLCAD_OK, "unable to create GSON animation fixture");
}

void
add_skinned_bot(context &destination)
{
    static const fastf_t vertices[] = {
	0.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0, 10.0, 0.0
    };
    static const int faces[] = {0, 1, 2};
    static const uint32_t offsets[] = {0, 1, 2, 3};
    static const uint32_t indices[] = {0, 1, 1};
    static const fastf_t weights[] = {1.0, 1.0, 1.0};
    struct rt_db_internal internal;
    struct rt_wdb *wdbp = wdb_dbopen(destination.value.dbip,
	RT_WDB_TYPE_DB_INMEM);
    struct rt_bot_internal *bot;

    RT_DB_INTERNAL_INIT(&internal);
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SURFACE;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = RT_BOT_HAS_SKIN;
    bot->num_vertices = 3;
    bot->vertices = static_cast<fastf_t *>(bu_malloc(sizeof(vertices),
	"GSON BOT vertices"));
    memcpy(bot->vertices, vertices, sizeof(vertices));
    bot->num_faces = 1;
    bot->faces = static_cast<int *>(bu_malloc(sizeof(faces), "GSON BOT faces"));
    memcpy(bot->faces, faces, sizeof(faces));
    BU_ALLOC(bot->skin, struct rt_bot_skin);
    bot->skin->joint_count = 2;
    bot->skin->joint_links = static_cast<struct bu_vls *>(bu_calloc(2,
	sizeof(struct bu_vls), "GSON skin links"));
    bu_vls_init(&bot->skin->joint_links[0]);
    bu_vls_init(&bot->skin->joint_links[1]);
    bu_vls_strcpy(&bot->skin->joint_links[0], "root");
    bu_vls_strcpy(&bot->skin->joint_links[1], "tool");
    bot->skin->inverse_bind_matrices = static_cast<mat_t *>(bu_malloc(
	2 * sizeof(mat_t), "GSON inverse binds"));
    MAT_IDN(bot->skin->inverse_bind_matrices[0]);
    MAT_IDN(bot->skin->inverse_bind_matrices[1]);
    bot->skin->inverse_bind_matrices[1][3] = -10.0;
    bot->skin->influence_count = 3;
    bot->skin->vertex_offsets = static_cast<uint32_t *>(bu_malloc(
	sizeof(offsets), "GSON skin offsets"));
    bot->skin->joint_indices = static_cast<uint32_t *>(bu_malloc(
	sizeof(indices), "GSON skin indices"));
    bot->skin->weights = static_cast<fastf_t *>(bu_malloc(sizeof(weights),
	"GSON skin weights"));
    memcpy(bot->skin->vertex_offsets, offsets, sizeof(offsets));
    memcpy(bot->skin->joint_indices, indices, sizeof(indices));
    memcpy(bot->skin->weights, weights, sizeof(weights));
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_BOT;
    internal.idb_meth = &OBJ[ID_BOT];
    internal.idb_ptr = bot;
    int status = rt_bot_skin_validate(bot, nullptr) == BRLCAD_OK ?
	wdb_put_internal(wdbp, "tool.bot", &internal, 1.0) : BRLCAD_ERROR;
    rt_db_free_internal(&internal);
    require(status == BRLCAD_OK, "unable to create GSON BOT fixture");
}

const struct rt_db_internal
read_object(context &source, const char *name, int type)
{
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    const struct directory *directory = db_lookup(source.value.dbip, name,
	LOOKUP_QUIET);
    require(directory && rt_db_get_internal(&internal, directory,
	source.value.dbip, nullptr) >= 0 && internal.idb_type == type,
	std::string("missing restored object: ") + name);
    return internal;
}

void
check_round_trip(const char *path)
{
    context source;
    add_joint(source);
    add_animation(source);
    add_skinned_bot(source);
    require(source.execute(GCV_FILTER_WRITE, path) == 1,
	"GSON articulation export failed");
    std::ifstream stream(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(stream)),
	std::istreambuf_iterator<char>());
    require(text.find("encoding") != std::string::npos &&
	text.find("native") != std::string::npos &&
	text.find("joint_links") != std::string::npos &&
	text.find("inverse_bind_matrices") != std::string::npos,
	"GSON output omitted native skin data");

    context restored;
    require(restored.execute(GCV_FILTER_READ, path) == 1,
	"GSON articulation import failed");
    struct rt_db_internal joint_internal = read_object(restored, "tool.j", ID_JOINT);
    struct rt_db_internal animation_internal = read_object(restored,
	"motion.anim", ID_ANIMATION);
    struct rt_db_internal bot_internal = read_object(restored, "tool.bot", ID_BOT);
    const auto *joint = static_cast<const struct rt_joint_internal *>(joint_internal.idb_ptr);
    const auto *animation = static_cast<const struct rt_animation_internal *>(
	animation_internal.idb_ptr);
    const auto *bot = static_cast<const struct rt_bot_internal *>(bot_internal.idb_ptr);
    require(joint->dof_count == 1 && EQUAL(joint->dofs[0].current,
	5.1234567890123457), "GSON joint data did not round-trip");
    require(animation->channel_count == 1 && EQUAL(
	animation->channels[0].values[3], 12.345678901234567),
	"GSON animation data did not round-trip");
    require(rt_bot_skin_validate(bot, nullptr) == BRLCAD_OK &&
	bot->num_vertices == 3 && bot->skin->joint_count == 2 &&
	BU_STR_EQUAL(bu_vls_cstr(&bot->skin->joint_links[1]), "tool") &&
	EQUAL(bot->skin->inverse_bind_matrices[1][3], -10.0),
	"GSON skinned BOT data did not round-trip");
    rt_db_free_internal(&bot_internal);
    rt_db_free_internal(&animation_internal);
    rt_db_free_internal(&joint_internal);
}

} // namespace

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 2)
	return 1;
    bu_file_delete(argv[1]);
    try {
	check_round_trip(argv[1]);
	bu_file_delete(argv[1]);
	return 0;
    } catch (const std::exception &error) {
	bu_log("%s\n", error.what());
	bu_file_delete(argv[1]);
	return 1;
    }
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
