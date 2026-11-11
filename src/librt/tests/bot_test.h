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
