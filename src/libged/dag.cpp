/*                         D A G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012 United States Government as represented by
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
/** @file libged/dag.cpp
 *
 * The model for a directed acyclic graph.
 *
 */

#include <stdint.h>

#include "libavoid/libavoid.h"
using namespace Avoid;

#include "common.h"
#include "bu.h"
#include "ged_private.h"
#include "ged.h"
#include "raytrace.h"


struct _ged_dag_data {
    Avoid::Router *router;
    Avoid::ConnRef *connRef;
    uint16_t object_nr;
};


/**
 * Method called when a connector of type Avoid::ConnRef needs to be redrawn.
 */
static void
conn_callback(void *ptr)
{
    Avoid::ConnRef *connRef = (Avoid::ConnRef *) ptr;

    const Avoid::PolyLine& route = connRef->route();
    printf("New path: ");
    for (size_t i = 0; i < route.ps.size(); ++i)
    {
        printf("%s(%f, %f)", (i > 0) ? "-" : "",
                route.ps[i].x, route.ps[i].y);
    }
    printf("\n");
}


/**
 * Add one object to the router of the 'dag' structure.
 */
int
add_object(struct _ged_dag_data *dag)
{
    dag->object_nr++;
    Avoid::Point srcPt(1.2+dag->object_nr, 0.5);
    Avoid::Point dstPt(1.5+dag->object_nr, 0.5);
    Avoid::ConnRef *connRef = new Avoid::ConnRef(dag->router, srcPt, dstPt);
    connRef->setCallback(conn_callback, connRef);
    dag->router->processTransaction();

    return GED_OK;
}


/**
 * Add the list of objects(either solids, comb or region) to the router.
 */
int
add_objects(struct ged *gedp, struct _ged_dag_data *dag)
{
    struct directory *dp = NULL, *ndp = NULL;
    struct bu_vls dp_name_vls = BU_VLS_INIT_ZERO;
    int i;

    /* Traverse the database 'gedp' */
    for(i = 0; i < RT_DBNHASH; i++)
    for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
        bu_vls_sprintf(&dp_name_vls, "%s%s", "", dp->d_namep);
        ndp = db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&dp_name_vls), 1);
        if (ndp) {
            if(dp->d_flags & RT_DIR_SOLID) {
                bu_log("Adding SOLID object [%s]\n", bu_vls_addr(&dp_name_vls));
                add_object(dag);
                bu_log("[SOLID]added %d objects.\n", dag->object_nr);
            } else if (dp->d_flags & RT_DIR_COMB) {
                bu_log("Adding COMB object [%s]\n", bu_vls_addr(&dp_name_vls));
                add_object(dag);
                bu_log("[COMB]added %d objects.\n", dag->object_nr);
            } else if (dp->d_flags & RT_DIR_REGION) {
                bu_log("Adding REGION object [%s]\n", bu_vls_addr(&dp_name_vls));
                add_object(dag);
                bu_log("[REGION]added %d objects.\n", dag->object_nr);
            } else {
                bu_log("Something else: [%s]\n", bu_vls_addr(&dp_name_vls));
            }
        } else {
            bu_log("ERROR: Unable to locate [%s] within input database, skipping.\n",  bu_vls_addr(&dp_name_vls));
        }
    }

    ged_close(gedp);
    bu_vls_free(&dp_name_vls);

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
