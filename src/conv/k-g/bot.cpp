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


Bot::Bot(void) : m_name(), m_thickness(0.)
{
    memset(&m_botInternal, 0, sizeof(m_botInternal));

    m_botInternal.magic       = RT_BOT_INTERNAL_MAGIC;
    m_botInternal.mode        = RT_BOT_SOLID;
    m_botInternal.orientation = RT_BOT_UNORIENTED;
}


Bot::~Bot(void)
{
    if (m_botInternal.vertices != nullptr)
	bu_free(m_botInternal.vertices, "Bot::~Bot: vertices");

    if (m_botInternal.faces != nullptr)
	bu_free(m_botInternal.faces, "Bot::~Bot: faces");

    if (m_botInternal.normals != nullptr)
	bu_free(m_botInternal.normals, "Bot::~Bot: normals");

    if (m_botInternal.face_normals != nullptr)
	bu_free(m_botInternal.face_normals, "Bot::~Bot: face_normals");
}


void Bot::setName
(
    const char* value
) {
    if (value != nullptr)
	m_name = value;
    else
	m_name = "";
}


void Bot::setThickness
(
    double value
) {
    if (value > SMALL_FASTF) {
	m_thickness        = value;
	m_botInternal.mode = RT_BOT_PLATE;
    }
    else {
	m_thickness        = 0.;
	m_botInternal.mode = RT_BOT_SOLID;
    }
}


void Bot::addTriangle
(
    const point_t& point1,
    const point_t& point2,
    const point_t& point3
) {
    m_botInternal.faces = static_cast<int*>(bu_realloc(m_botInternal.faces, (m_botInternal.num_faces + 1) * 3 * sizeof(int), "Bot:addTriangle: faces"));

    m_botInternal.faces[m_botInternal.num_faces * 3]     = add_vertex(point1, m_botInternal);
    m_botInternal.faces[m_botInternal.num_faces * 3 + 1] = add_vertex(point2, m_botInternal);
    m_botInternal.faces[m_botInternal.num_faces * 3 + 2] = add_vertex(point3, m_botInternal);

    ++m_botInternal.num_faces;

    if (m_botInternal.face_normals == 0)
	m_botInternal.face_normals = static_cast<int*>(bu_calloc(3 * m_botInternal.num_faces, sizeof(int), "Bot:addTriangle: face_normals"));
    else if (m_botInternal.num_faces > m_botInternal.num_face_normals) {
	m_botInternal.face_normals = static_cast<int*>(bu_realloc(m_botInternal.face_normals, 3 * m_botInternal.num_faces * sizeof(int), "Bot:addTriangle: face_normals"));

	for (size_t i = m_botInternal.num_face_normals; i < m_botInternal.num_faces; ++i) {
	    fastf_t defaultNormal[3] = {0};
	    int     newIndex         = add_normal(defaultNormal, m_botInternal);

	    m_botInternal.face_normals[3 * i]     = newIndex;
	    m_botInternal.face_normals[3 * i + 1] = newIndex;
	    m_botInternal.face_normals[3 * i + 2] = newIndex;
	}
    }

    m_botInternal.num_face_normals = m_botInternal.num_faces;
}


std::vector<std::string> Bot::write
(
    rt_wdb* wdbp
) {
    std::vector<std::string> ret;
    std::string              botName = m_name;
    botName += ".bot";

    rt_bot_internal*         bot_wdb;

    BU_GET(bot_wdb, rt_bot_internal);
    bot_wdb->magic = RT_BOT_INTERNAL_MAGIC;

    *bot_wdb = m_botInternal;

    if (m_botInternal.faces != 0) {
	bot_wdb->faces =  static_cast<int*>(bu_malloc(3 * m_botInternal.num_faces * sizeof(int), "Bot:write: faces"));

	memcpy(bot_wdb->faces, m_botInternal.faces, 3 * m_botInternal.num_faces * sizeof(int));
    }

    if (m_botInternal.vertices != 0) {
	bot_wdb->vertices = static_cast<fastf_t*>(bu_malloc(3 * m_botInternal.num_vertices * sizeof(fastf_t), "Bot:write: vertices"));

	memcpy(bot_wdb->vertices, m_botInternal.vertices, 3 * m_botInternal.num_vertices * sizeof(fastf_t));
    }

    if (m_botInternal.face_mode != 0)
	bot_wdb->face_mode = bu_bitv_dup(m_botInternal.face_mode);

    if (m_botInternal.normals != 0) {
	bot_wdb->normals = static_cast<fastf_t*>(bu_malloc(3 * m_botInternal.num_normals * sizeof(fastf_t), "Bot:write: normals"));

	memcpy(bot_wdb->normals, m_botInternal.normals, 3 * m_botInternal.num_normals * sizeof(fastf_t));
    }

    if (m_botInternal.face_normals != 0) {
	bot_wdb->face_normals = static_cast<int*>(bu_malloc(3 * m_botInternal.num_face_normals * sizeof(int), "Bot:write: face_normals"));

	memcpy(bot_wdb->face_normals, m_botInternal.face_normals, 3 * m_botInternal.num_face_normals * sizeof(int));
    }

    bot_wdb->face_mode = bu_bitv_new(bot_wdb->num_faces);

    if (m_thickness > SMALL_FASTF) {
	bot_wdb->thickness = static_cast<fastf_t*>(bu_calloc(bot_wdb->num_faces, sizeof(fastf_t), "Bot:write: thickness"));

	for (size_t i = 0; i < bot_wdb->num_faces; ++i)
	    bot_wdb->thickness[i] = m_thickness;
    }

    if (m_botInternal.face_mode != 0 || m_botInternal.normals != 0 || m_botInternal.face_normals || 0) {
	ret.push_back(botName);

	wdb_export(wdbp, botName.c_str(), bot_wdb, ID_BOT, 1.);
    }

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
