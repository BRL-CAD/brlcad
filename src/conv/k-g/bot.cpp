/*                         B O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
/** @file bot.cpp
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * intermediate bag of triangles data implementation
 */

#include "common.h"

#include "wdb.h"

#include "bot.h"


static int add_vertex
(
    const point_t&   point,
    rt_bot_internal& bot
) {
    int ret = 0; // index of the added vertex

    // vertex already there?
    for (; ret < static_cast<int>(bot.num_vertices); ++ret) {
	fastf_t tmp[3] = {bot.vertices[ret * 3], bot.vertices[ret * 3 + 1], bot.vertices[ret * 3 + 2]};

	if (VNEAR_EQUAL(point, tmp, VUNITIZE_TOL))
	    break;
    }

    if (ret == static_cast<int>(bot.num_vertices)) {
	// add a new vertex
	++bot.num_vertices;
	bot.vertices = static_cast<fastf_t*>(bu_realloc(bot.vertices, bot.num_vertices * 3 * sizeof(fastf_t), "add_vertex: vertex"));
	bot.vertices[ret * 3]     = point[0];
	bot.vertices[ret * 3 + 1] = point[1];
	bot.vertices[ret * 3 + 2] = point[2];
    }

    return ret;
}


static int add_normal
(
    const fastf_t    normal[3],
    rt_bot_internal& bot
) {
    int     ret = 0; // index of the added normal
    fastf_t tmp[3];

    // normal already there?
    for (; ret < static_cast<int>(bot.num_normals); ++ret) {
	tmp[0] = bot.normals[ret];
	tmp[1] = bot.normals[ret + 1];
	tmp[2] = bot.normals[ret + 2];

	if (VNEAR_EQUAL(normal, tmp, VUNITIZE_TOL))
	    break;
    }

    if (ret == static_cast<int>(bot.num_normals)) {
	// add a new normal
	++bot.num_normals;
	bot.normals = static_cast<fastf_t*>(bu_realloc(bot.normals, bot.num_normals * 3 * sizeof(fastf_t), "add_normal: normal"));
	bot.normals[ret]     = normal[0];
	bot.normals[ret + 1] = normal[1];
	bot.normals[ret + 2] = normal[2];
    }

    return ret;
}


Bot::Bot(void) : name(), thickness(0.) {
    memset(&bot_internal, 0, sizeof(bot_internal));

    bot_internal.magic       = RT_BOT_INTERNAL_MAGIC;
    bot_internal.mode        = RT_BOT_SOLID;
    bot_internal.orientation = RT_BOT_UNORIENTED;
}


Bot::~Bot(void) {
    if (bot_internal.vertices != nullptr)
	bu_free(bot_internal.vertices, "Bot::~Bot: vertices");

    if (bot_internal.faces != nullptr)
	bu_free(bot_internal.faces, "Bot::~Bot: faces");

    if (bot_internal.normals != nullptr)
	bu_free(bot_internal.normals, "Bot::~Bot: normals");

    if (bot_internal.face_normals != nullptr)
	bu_free(bot_internal.face_normals, "Bot::~Bot: face_normals");
}


void Bot::setName
(
    const char* value
) {
    if (value != nullptr)
	name = value;
    else
	name = "";
}


void Bot::setThickness
(
    double value
) {
    if (value > SMALL_FASTF) {
	thickness         = value;
	bot_internal.mode = RT_BOT_PLATE;
    }
    else {
	thickness         = 0.;
	bot_internal.mode = RT_BOT_SOLID;
    }
}


void Bot::addTriangle
(
    const point_t& point1,
    const point_t& point2,
    const point_t& point3
) {
    bot_internal.faces = static_cast<int*>(bu_realloc(bot_internal.faces, (bot_internal.num_faces + 1) * 3 * sizeof(int), "Bot:addTriangle: faces"));

    bot_internal.faces[bot_internal.num_faces * 3]     = add_vertex(point1, bot_internal);
    bot_internal.faces[bot_internal.num_faces * 3 + 1] = add_vertex(point2, bot_internal);
    bot_internal.faces[bot_internal.num_faces * 3 + 2] = add_vertex(point3, bot_internal);

    ++bot_internal.num_faces;

    if (bot_internal.face_normals == 0)
	bot_internal.face_normals = static_cast<int*>(bu_calloc(3 * bot_internal.num_faces, sizeof(int), "Bot:addTriangle: face_normals"));
    else if (bot_internal.num_faces > bot_internal.num_face_normals) {
	bot_internal.face_normals = static_cast<int*>(bu_realloc(bot_internal.face_normals, 3 * bot_internal.num_faces * sizeof(int), "Bot:addTriangle: face_normals"));

	for (size_t i = bot_internal.num_face_normals; i < bot_internal.num_faces; ++i) {
	    fastf_t defaultNormal[3] = {0};
	    int     newIndex         = add_normal(defaultNormal, bot_internal);

	    bot_internal.face_normals[3 * i]     = newIndex;
	    bot_internal.face_normals[3 * i + 1] = newIndex;
	    bot_internal.face_normals[3 * i + 2] = newIndex;
	}
    }

    bot_internal.num_face_normals = bot_internal.num_faces;
}

void Bot::setAttributes
(
    std::map<std::string, std::string> attr
) {
    attributes = attr;
}


const char* Bot::getName(void) const {
    return name.c_str();
}

std::map<std::string, std::string> Bot::getAttributes(void) const{
    return attributes;
}


void Bot::write
(
    rt_wdb* wdbp
) {
    rt_bot_internal* bot_wdb;

    BU_GET(bot_wdb, rt_bot_internal);
    bot_wdb->magic = RT_BOT_INTERNAL_MAGIC;

    *bot_wdb = bot_internal;

    if (bot_internal.faces != 0) {
	bot_wdb->faces =  static_cast<int*>(bu_malloc(3 * bot_internal.num_faces * sizeof(int), "Bot:write: faces"));

	memcpy(bot_wdb->faces, bot_internal.faces, 3 * bot_internal.num_faces * sizeof(int));
    }

    if (bot_internal.vertices != 0) {
	bot_wdb->vertices = static_cast<fastf_t*>(bu_malloc(3 * bot_internal.num_vertices * sizeof(fastf_t), "Bot:write: vertices"));

	memcpy(bot_wdb->vertices, bot_internal.vertices, 3 * bot_internal.num_vertices * sizeof(fastf_t));
    }

    if (bot_internal.face_mode != 0)
	bot_wdb->face_mode = bu_bitv_dup(bot_internal.face_mode);

    if (bot_internal.normals != 0) {
	bot_wdb->normals = static_cast<fastf_t*>(bu_malloc(3 * bot_internal.num_normals * sizeof(fastf_t), "Bot:write: normals"));

	memcpy(bot_wdb->normals, bot_internal.normals, 3 * bot_internal.num_normals * sizeof(fastf_t));
    }

    if (bot_internal.face_normals != 0) {
	bot_wdb->face_normals = static_cast<int*>(bu_malloc(3 * bot_internal.num_face_normals * sizeof(int), "Bot:write: face_normals"));

	memcpy(bot_wdb->face_normals, bot_internal.face_normals, 3 * bot_internal.num_face_normals * sizeof(int));
    }

    bot_wdb->face_mode = bu_bitv_new(bot_wdb->num_faces);

    if (thickness > SMALL_FASTF) {
	bot_wdb->thickness = static_cast<fastf_t*>(bu_calloc(bot_wdb->num_faces, sizeof(fastf_t), "Bot:write: thickness"));

	for (size_t i = 0; i < bot_wdb->num_faces; ++i)
	    bot_wdb->thickness[i] = thickness;
    }

    wdb_export(wdbp, name.c_str(), bot_wdb, ID_BOT, 1.);
}

void Bot::writeAttributes
(
    rt_wdb* wdbp,
    const char* name,
    std::map<std::string, std::string> attributes
) {
    struct rt_db_internal     bot_internal;
    struct directory*         dp;
    struct db_i*              dbip;
    bu_attribute_value_set*   avs;

    dbip = wdbp->dbip;
    dp = db_lookup(dbip, name, 0);

    rt_db_get_internal(&bot_internal, dp, dbip, bn_mat_identity, &rt_uniresource);
    avs = &bot_internal.idb_avs;
    for (std::map<std::string,std::string>::iterator it= attributes.begin(); it!= attributes.end() ; it++)
    {
	bu_avs_add(avs, it->first.c_str(), it->second.c_str());
    }

    bu_avs_print(avs, "BOT Attributes:");
    db5_update_attributes(dp, avs, dbip);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
