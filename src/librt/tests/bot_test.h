/*              B O T _ T E S T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
#ifndef LIBRT_TESTS_BOT_TEST_H
#define LIBRT_TESTS_BOT_TEST_H

#include "common.h"
#include "raytrace.h"
#include "rt/geom.h"

namespace bot_test {
inline struct rt_surface_derivatives
derivative_marker()
{
    // Nonzero vectors reveal accidental output replacement on failed queries.
    return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}};
}

inline bool
same_derivatives(const struct rt_surface_derivatives &actual,
    const struct rt_surface_derivatives &expected, double tolerance)
{
    return !VINVALID(actual.dpdu) && !VINVALID(actual.dpdv) &&
	!VINVALID(actual.dndu) && !VINVALID(actual.dndv) &&
	VNEAR_EQUAL(actual.dpdu, expected.dpdu, tolerance) &&
	VNEAR_EQUAL(actual.dpdv, expected.dpdv, tolerance) &&
	VNEAR_EQUAL(actual.dndu, expected.dndu, tolerance) &&
	VNEAR_EQUAL(actual.dndv, expected.dndv, tolerance);
}

class Shot {
    struct resource *m_resource;
    struct seg m_head;
    int m_hit_count;
  public:
    Shot(struct application &application, struct soltab *solid, struct xray &ray)
	: m_resource(application.a_resource)
    {
	BU_LIST_INIT(&m_head.l);
	m_hit_count = rt_obj_shot(solid, &ray, &application, &m_head);
    }
    ~Shot()
    {
	struct seg *segment;
	while (BU_LIST_WHILE(segment, seg, &m_head.l)) {
	    BU_LIST_DEQUEUE(&segment->l);
	    RT_FREE_SEG(segment, m_resource);
	}
    }
    struct seg *first()
    {
	return m_hit_count > 0 && !BU_LIST_IS_EMPTY(&m_head.l)
	    ? BU_LIST_FIRST(seg, &m_head.l) : nullptr;
    }
    Shot(const Shot &) = delete;
    Shot &operator=(const Shot &) = delete;
};

class PreparedBot {
  public:
    struct soltab solid = {};
    int status;
    PreparedBot(struct rt_bot_internal &bot, struct rt_i *rtip)
    {
	solid.l.magic = RT_SOLTAB_MAGIC;
	solid.l2.magic = RT_SOLTAB2_MAGIC;
	solid.st_meth = &OBJ[ID_BOT];
	solid.st_id = ID_BOT;
	solid.st_rtip = rtip;
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_BOT;
	internal.idb_meth = &OBJ[ID_BOT];
	internal.idb_ptr = &bot;
	status = OBJ[ID_BOT].ft_prep(&solid, &internal, rtip);
    }
    ~PreparedBot() { if (solid.st_specific) OBJ[ID_BOT].ft_free(&solid); }
    PreparedBot(const PreparedBot &) = delete;
    PreparedBot &operator=(const PreparedBot &) = delete;
};
} // namespace bot_test
#endif
