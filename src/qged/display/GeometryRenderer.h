/*              G E O M E T R Y R E N D E R E R . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file GeometryRenderer.h */

#ifndef BRLCAD_GEOMETRYRENDERER_H
#define BRLCAD_GEOMETRYRENDERER_H


#include <rt/wdb.h>
#include <rt/tree.h>
#include <rt/db_instance.h>
#include <rt/functab.h>
#include <raytrace.h>
#include <rt/db_io.h>
#include "DisplayManager.h"
#include "Renderable.h"
#include <rt/global.h>

struct rt_i;
struct bu_list;
struct bv_vlist;
struct db_tree_state;
struct db_full_path;
struct rt_db_internal;
union tree;

class GeometryRenderer:public Renderable {
public:
    explicit GeometryRenderer(DisplayManager *displayManager);
    rt_wdb * getDatabase();
    void setDatabase(rt_wdb *database);
    void onDatabaseUpdated();
    void render() override;
private:
    rt_wdb *database = nullptr;
    rt_i * r_database = nullptr;
    DisplayManager *displayManager;
    float defaultWireColor[3] = {1,.1,.4};
    bool immediateMode = false;
    bool databaseUpdated = false;

    static tree *drawSolid(db_tree_state *tsp, const db_full_path *pathp, rt_db_internal *ip, void *clientData);
    void drawDatabase();
    std::vector<int> solids; // contains the display list of each solid
};


#endif //BRLCAD_GEOMETRYRENDERER_H
