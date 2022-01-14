/*            G E O M E T R Y R E N D E R E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file GeometryRenderer.cpp */

#include "common.h"
#include "GeometryRenderer.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wshadow"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#endif


GeometryRenderer::GeometryRenderer(DisplayManager *displayManager) : displayManager(displayManager) {}

void GeometryRenderer::render() {
    if (database == nullptr) return;
    displayManager->saveState();
    glEnable(GL_LIGHTING);

    if(!immediateMode) {
        // If database has been updated we need to redraw.
        // Also QOpenGLWidget sometimes looses saved display list after UI changes (dock / undock etc).
        // Therefore we need to draw again if the dlists are not available
        if (databaseUpdated || (!solids.empty() && !displayManager->isDListValid(solids[0]))) {
            drawDatabase();
            databaseUpdated = false;
        }
        for (auto i:solids) {
            displayManager->drawDList(i);
        }
    }
    else{
        drawDatabase();
    }
    displayManager->restoreState();
}

void GeometryRenderer::onDatabaseUpdated() {
    r_database = rt_new_rti(database->dbip);
    if (!immediateMode) {
        databaseUpdated = true;
    }
    else {

    }
}

/*
 * Clears existing display lists, iterate through each solid and generates display lists by calling drawSolid on each
 */
void GeometryRenderer::drawDatabase() {
    db_tree_state initState;
    db_init_db_tree_state(&initState, r_database->rti_dbip, database->wdb_resp);
    initState.ts_ttol = &r_database->rti_ttol;
    initState.ts_tol = &r_database->rti_tol;

    for (auto i: solids){
        displayManager->freeDLists(i,1);
    }
    solids.clear();

    struct directory **dbObjects = NULL;
    int path_cnt = db_ls(database->dbip, DB_LS_TOPS, NULL, &dbObjects);
    if (path_cnt) {
        for (int i = 0; i < path_cnt; i++) {
            const char *topObjectName = dbObjects[i]->d_namep;
            db_walk_tree(r_database->rti_dbip, 1, &topObjectName, 1, &initState, 0, 0, drawSolid, this);
        }
    }
}


/*
 * Set the color and line attribute to suit a given solid, creates a display list, plots and draws solid's vlist into the display list.
 * The created display list is added to GeometryRenderer::solids
 */
tree *GeometryRenderer::drawSolid(db_tree_state *tsp, const db_full_path *UNUSED(pathp), rt_db_internal *ip, void *clientData) {
    tree *ret = TREE_NULL;
    auto *geometryRenderer = static_cast<GeometryRenderer *>(clientData);
    auto *displayManager = geometryRenderer->displayManager;

    struct bu_list vhead;
    BU_LIST_INIT(&vhead);

    if (ip->idb_meth->ft_plot != 0) {
        if (ip->idb_meth->ft_plot(&vhead, ip, tsp->ts_ttol, tsp->ts_tol, 0) == 0) {
            BU_GET(ret, tree);
            RT_TREE_INIT(ret);
            ret->tr_op = OP_NOP;
        }
    }

    GLuint dlist;
    if (!geometryRenderer->immediateMode) {
        dlist = displayManager->genDLists(1);
        displayManager->beginDList(dlist);  // begin display list --------------
        geometryRenderer->solids.push_back(dlist);
    }

    if (tsp->ts_mater.ma_color_valid) {
        displayManager->setFGColor(tsp->ts_mater.ma_color[0], tsp->ts_mater.ma_color[1], tsp->ts_mater.ma_color[2], 1);
    }
    else {
        displayManager->setFGColor(geometryRenderer->defaultWireColor[0], geometryRenderer->defaultWireColor[1],
                geometryRenderer->defaultWireColor[2], 1);
    }

    displayManager->setLineStyle(tsp->ts_sofar & (TS_SOFAR_MINUS | TS_SOFAR_INTER));
    displayManager->drawVList(reinterpret_cast<bv_vlist *>(&vhead));

    if (!geometryRenderer->immediateMode) {
        displayManager->endDList();     // end display list --------------
    }

    return ret;
}

rt_wdb *GeometryRenderer::getDatabase() {
    return database;
}

void GeometryRenderer::setDatabase(rt_wdb *database) {
    this->database = database;
    onDatabaseUpdated();
}

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

