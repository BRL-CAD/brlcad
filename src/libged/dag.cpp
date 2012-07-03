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

#include "common.h"

#ifdef HAVE_ADAPTAGRAMS

/* System Header */
#include <stdint.h>

#define BRLCAD_PARALLEL PARALLEL
#undef PARALLEL
#define POSITION_COORDINATE 10.0

/* Adaptagrams Header */
#include "libavoid/libavoid.h"
using namespace Avoid;

/* Public Header */
#include "ged.h"
#include "raytrace.h"


struct output {
    FILE *outfp;
    struct bu_ptbl groups;
    struct bu_ptbl regions;
    struct bu_ptbl combinations;
    struct bu_ptbl primitives;
};


/**
 * This structure has fields related to the representation of a graph with the help of the Adaptagrams library.
 */
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
    for (size_t i = 0; i < route.ps.size(); ++i) {
        printf("%s(%f, %f)", (i > 0) ? "-" : "",
                route.ps[i].x, route.ps[i].y);
    }
    printf("\n");
}


/**
 * Add one object to the router of the 'dag' structure.
 */
static Avoid::ShapeRef*
add_object(struct _ged_dag_data *dag)
{
    dag->object_nr++;

    Avoid::Point srcPt(dag->object_nr * POSITION_COORDINATE + 1.0, dag->object_nr * POSITION_COORDINATE + 1.0);
    Avoid::Point dstPt(POSITION_COORDINATE + dag->object_nr * POSITION_COORDINATE, POSITION_COORDINATE +
        dag->object_nr * POSITION_COORDINATE);
    Avoid::Rectangle shapeRect(srcPt, dstPt);
    Avoid::ShapeRef *shapeRef = new Avoid::ShapeRef(dag->router, shapeRect);

    return shapeRef;
}

/**
 * Function to process a combination node.
 */
static void
dag_comb(struct db_i *dbip, struct directory *dp, genptr_t out, struct _ged_dag_data *dag)
{
    size_t i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    struct output *o = (struct output *)out;

    Avoid::ShapeRef *shapeRef1 = NULL;
    Avoid::ShapeRef *shapeRef2 = NULL;
    const unsigned int CENTRE = 1;

    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
    bu_log("ERROR: Database read error, skipping %s\n", dp->d_namep);
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb->region_flag) {
        if (bu_ptbl_ins_unique(&(o->regions), (long *)bu_hash((unsigned char *)dp->d_namep, strlen(dp->d_namep))) == -1) {
            bu_log("\t\"%s\" [ color=blue shape=box3d ];\n", dp->d_namep);
        }
    } else {
        if (bu_ptbl_ins_unique(&(o->groups), (long *)bu_hash((unsigned char *)dp->d_namep, strlen(dp->d_namep))) == -1) {
            bu_log("\t\"%s\" [ color=green ];\n", dp->d_namep);
        }
    }

    shapeRef1 = add_object(dag);

    /* FIXME: this is yet-another copy of the commonly-used code that
     * gets a list of comb members.  needs to return tabular data.
     */
    if (comb->tree) {
    size_t node_count = 0;
    size_t actual_count = 0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct rt_tree_array *rt_tree_array = NULL;

    if (db_ck_v4gift_tree(comb->tree) < 0) {
        db_non_union_push(comb->tree, &rt_uniresource);
        if (db_ck_v4gift_tree(comb->tree) < 0) {
            bu_log("INTERNAL_ERROR: Cannot flatten tree of [%s] for listing", dp->d_namep);
            return;
        }
    }

    node_count = db_tree_nleaves(comb->tree);
    if (node_count > 0) {
        rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");
        actual_count = (struct rt_tree_array *)db_flatten_tree(rt_tree_array, comb->tree, OP_UNION, 1, &rt_uniresource) - rt_tree_array;
        BU_ASSERT_SIZE_T(actual_count, ==, node_count);
        comb->tree = TREE_NULL;
    } else {
        actual_count = 0;
        rt_tree_array = NULL;
    }

    bu_log("actual count: %d", actual_count);

    for (i = 0; i < actual_count; i++) {
        char op;

        switch (rt_tree_array[i].tl_op) {
            case OP_UNION:
                op = '+';
                break;
            case OP_INTERSECT:
                op = 'x';
                break;
            case OP_SUBTRACT:
                op = '-';
                break;
            default:
                op = '?';
                break;
        }

        bu_log("\t\"%s\" -> \"%s\" [ label=\"%c\" ];\n", dp->d_namep, rt_tree_array[i].tl_tree->tr_l.tl_name, op);

        /* Link this node with its subnode */
        shapeRef2 = add_object(dag);

        /* Create connection pins on shapes */
        new Avoid::ShapeConnectionPin(shapeRef1, CENTRE, Avoid::ATTACH_POS_CENTRE, Avoid::ATTACH_POS_CENTRE);
        new Avoid::ShapeConnectionPin(shapeRef2, CENTRE, Avoid::ATTACH_POS_CENTRE, Avoid::ATTACH_POS_CENTRE);

        /* Create connector from each shape shapeRef2 to the input pin on shapeRef1 */
        Avoid::ConnEnd dstEnd(shapeRef1, CENTRE);
        Avoid::ConnEnd srcEnd(shapeRef2, CENTRE);
        Avoid::ConnRef *connRef = new Avoid::ConnRef(dag->router, srcEnd, dstEnd);
        connRef->setCallback(conn_callback, connRef);
        dag->router->processTransaction();

        db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
    }
    bu_vls_free(&vls);

    if (rt_tree_array)
        bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }

    rt_db_free_internal(&intern);
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
    struct output o = {NULL, BU_PTBL_INIT_ZERO, BU_PTBL_INIT_ZERO, BU_PTBL_INIT_ZERO, BU_PTBL_INIT_ZERO};
    o.outfp = NULL;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    Avoid::ShapeRef *shapeRef = NULL;

    /* Traverse the database 'gedp' */
    for (i = 0; i < RT_DBNHASH; i++) {
        for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
            bu_vls_sprintf(&dp_name_vls, "%s%s", "", dp->d_namep);
            ndp = db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&dp_name_vls), 1);
            if (ndp) {
                if(dp->d_flags & RT_DIR_SOLID) {
                    bu_log("Adding SOLID object [%s]\n", bu_vls_addr(&dp_name_vls));
                    dag->object_nr++;

                    /* Create and add a shape (obstacle) to the router */
                    Avoid::Rectangle rectangle(Avoid::Point(dag->object_nr * POSITION_COORDINATE + 1.0,
                        dag->object_nr * POSITION_COORDINATE + 1.0), Avoid::Point(POSITION_COORDINATE +
                        dag->object_nr * POSITION_COORDINATE, POSITION_COORDINATE + dag->object_nr *
                        POSITION_COORDINATE));
                    shapeRef = new Avoid::ShapeRef(dag->router, rectangle);
                    dag->router->processTransaction();
                    bu_log("[SOLID]added %d objects.\n", dag->object_nr);
                } else if (dp->d_flags & RT_DIR_COMB) {
                    bu_log("Adding COMB object [%s]\n", bu_vls_addr(&dp_name_vls));
                    dag_comb(dbip, ndp, &o, dag);
                } else {
                    bu_log("Something else: [%s]\n", bu_vls_addr(&dp_name_vls));
                }
            } else {
                bu_log("ERROR: Unable to locate [%s] within input database, skipping.\n",  bu_vls_addr(&dp_name_vls));
            }
        }
    }

    ged_close(gedp);
    bu_vls_free(&dp_name_vls);
    dag->router->deleteShape(shapeRef);

    return GED_OK;
}


#define PARALLEL BRLCAD_PARALLEL
#undef BRLCAD_PARALLEL

#endif


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
