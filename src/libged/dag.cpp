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
#include <stdlib.h>

#define BRLCAD_PARALLEL PARALLEL
#undef PARALLEL
#define POSITION_COORDINATE 10.0

/* Adaptagrams Header */
#include "libavoid/libavoid.h"
using namespace Avoid;

/* Public Header */
#include "ged.h"

/**
 * This structure has fields that correspond to three hash tables for solid, group and region type objects of a database.
 */
struct output {
    struct bu_hash_tbl *groups;
    struct bu_hash_tbl *regions;
    struct bu_hash_tbl *solids;
};


/**
 * This structure has fields related to the representation of a graph with the help of the Adaptagrams library.
 */
struct _ged_dag_data {
    Avoid::Router *router;
    uint16_t object_nr;
    uint16_t last_connref_id;
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
add_object(struct _ged_dag_data *dag, unsigned int id)
{
    Avoid::Point srcPt(dag->object_nr * POSITION_COORDINATE + 1.0, dag->object_nr * POSITION_COORDINATE + 1.0);
    Avoid::Point dstPt(POSITION_COORDINATE + dag->object_nr * POSITION_COORDINATE, POSITION_COORDINATE +
        dag->object_nr * POSITION_COORDINATE);
    Avoid::Rectangle shapeRect(srcPt, dstPt);
    Avoid::ShapeRef *shapeRef = new Avoid::ShapeRef(dag->router, shapeRect, id);

    return shapeRef;
}


/**
 * Method that frees the memory allocated for each value field of a bu_hash_entry structure.
 */
void
free_hash_values(struct bu_hash_tbl *htbl)
{
    struct bu_hash_entry *entry;
    struct bu_hash_record rec;

    entry = bu_hash_tbl_first(htbl, &rec);

    while (entry) {
    bu_free(bu_get_hash_value(entry), "hash entry");
    entry = bu_hash_tbl_next(&rec);
    }
}


/**
 * Function which processes a combination node. I.e., it adds a new entry into the hash tables, if necessary.
 * In this case, it also adds a Avoid::ShapeRef object to the graph. It also traverses its subtree, adds
 * entries into the hash table for solid objects, if neccessary. In this case as well, it also adds a
 * Avoid::ShapeRef object to the graph.
 */
static void
dag_comb(struct db_i *dbip, struct directory *dp, genptr_t out, struct _ged_dag_data *dag)
{
    size_t i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    struct output *o = (struct output *)out;
    struct bu_hash_entry *prev = NULL;
    unsigned long idx;
    int new_entry;
    int was_solid;
    unsigned int value = 0;
    char *id = (char *)bu_malloc((size_t)6, "hash entry value");

    Avoid::ShapeRef *shapeRef1 = NULL;
    Avoid::ShapeRef *shapeRef2 = NULL;
    const unsigned int CENTRE = 1;

    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
    bu_log("ERROR: Database read error, skipping %s\n", dp->d_namep);
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    struct bu_hash_entry *hsh_entry_solid = NULL;
    if ((hsh_entry_solid = bu_find_hash_entry(o->solids, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx))) {
        value = atoi((const char*)hsh_entry_solid->value);
        was_solid = 1;

        /* The comb object has been registered as a solid prior to this. Delete its corresponding entry from the solids hash table. */
        if (hsh_entry_solid == o->solids->lists[idx]) {
            o->solids->lists[idx] = hsh_entry_solid->next;
        } else {
            prev->next = hsh_entry_solid->next;
        }
        free(hsh_entry_solid->value);
        hsh_entry_solid->value = NULL;
        free(hsh_entry_solid->key);
        hsh_entry_solid->key = NULL;
        free(hsh_entry_solid);
        hsh_entry_solid = NULL;

        o->solids->num_entries--;
    } else {
        was_solid = 0;
    }

    if (comb->region_flag) {   
        if (bu_find_hash_entry(o->regions, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx) == NULL) {
            struct bu_hash_entry *hsh_entry = bu_hash_add_entry(o->regions, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &new_entry);
            bu_log("index: %d\n", idx);
            if (was_solid == 1) {
                bu_log("it already is a SOLID\n");
                sprintf(id, "%d", value);
            } else {
                bu_log("it isn't a solid\n");
                dag->object_nr ++;
                sprintf(id, "%d", dag->object_nr);
                value = dag->object_nr;
            }
            /* Set the id value for this shape */
            bu_set_hash_value(hsh_entry, (unsigned char *)id);

            bu_log("\t\"%s\" [ color=blue shape=box3d ];\n", dp->d_namep);
        }
    } else {
        if (bu_find_hash_entry(o->groups, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx) == NULL) {
            struct bu_hash_entry *hsh_entry = bu_hash_add_entry(o->groups, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &new_entry);
            if (was_solid == 1) {
                sprintf(id, "%d", value);
            } else {
                dag->object_nr ++;
                sprintf(id, "%d", dag->object_nr);
                value = dag->object_nr;
            }
            /* Set the id value for this shape */
            bu_set_hash_value(hsh_entry, (unsigned char *)id);

            bu_log("\t\"%s\" [ color=green ];\n", dp->d_namep);
        }
    }

    if(was_solid == 1) {
        /* Don't create another shape because it already exists a corresponding one. */
        ObstacleList::const_iterator finish = dag->router->m_obstacles.end();
        for (ObstacleList::const_iterator it = dag->router->m_obstacles.begin(); it != finish; ++it) {
            if ((*it)->id() == value) {
                shapeRef1 = dynamic_cast<ShapeRef *>(*it);
                break;
            }
        }
    } else {
        /* Create a shape for the current object. */
        shapeRef1 = add_object(dag, value);
    }

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

    bu_log("actual count: %d\n", actual_count);

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

        struct bu_hash_entry *hsh_entry1, *hsh_entry2, *hsh_entry3;

        hsh_entry1 = bu_find_hash_entry(o->regions, (unsigned char *)rt_tree_array[i].tl_tree->tr_l.tl_name, strlen(rt_tree_array[i].tl_tree->tr_l.tl_name) + 1, &prev, &idx);
        hsh_entry2 = bu_find_hash_entry(o->groups, (unsigned char *)rt_tree_array[i].tl_tree->tr_l.tl_name, strlen(rt_tree_array[i].tl_tree->tr_l.tl_name) + 1, &prev, &idx);
        hsh_entry3 = bu_find_hash_entry(o->solids, (unsigned char *)rt_tree_array[i].tl_tree->tr_l.tl_name, strlen(rt_tree_array[i].tl_tree->tr_l.tl_name) + 1, &prev, &idx);

        if ((!hsh_entry1) && (!hsh_entry2) && (!hsh_entry3)) {
            /* This object hasn't been registered yet. Add it into the solids hash table and create a shape for it. */
            struct bu_hash_entry *hsh_entry = bu_hash_add_entry(o->solids, (unsigned char *)rt_tree_array[i].tl_tree->tr_l.tl_name, strlen(rt_tree_array[i].tl_tree->tr_l.tl_name) + 1, &new_entry);
            bu_log("index: %d\n", idx);
            dag->object_nr ++;

            id = (char *)bu_malloc((size_t)6, "hash entry value");
            sprintf(id, "%d", dag->object_nr);

            /* Set the id value for this shape */
            bu_set_hash_value(hsh_entry, (unsigned char *)id);

            value = dag->object_nr;
            bu_log("\t\"%s\" -> \"%s\" [ label=\"%c\" ];\n", dp->d_namep, rt_tree_array[i].tl_tree->tr_l.tl_name, op);
            bu_log("[SOLID]added %d objects.\n", dag->object_nr);

            /* Create a shape for the current node of the subtree */
            shapeRef2 = add_object(dag, value);
        } else {
            /* The shape already exists */
            bu_log("\t\"%s\" -> \"%s\" [ label=\"%c\" ];\n", dp->d_namep, rt_tree_array[i].tl_tree->tr_l.tl_name, op);
            struct bu_hash_entry *hsh_entry = (hsh_entry1) ? hsh_entry1 : ((hsh_entry2) ? hsh_entry2 : hsh_entry3);
            value = atoi((const char *)hsh_entry->value);

            /* Don't create another shape because it already exists a corresponding one. */
            ObstacleList::const_iterator finish = dag->router->m_obstacles.end();
            for (ObstacleList::const_iterator it = dag->router->m_obstacles.begin(); it != finish; ++it) {
                if ((*it)->id() == value) {
                    /* Get a reference to the shape that corresponds to the current node of the subtree. */
                    shapeRef2 = dynamic_cast<ShapeRef *>(*it);
                    break;
                }
            }
        }

        /* Create connection pins on shapes for linking the parent node with the subnode. */
        new Avoid::ShapeConnectionPin(shapeRef1, CENTRE, Avoid::ATTACH_POS_CENTRE, Avoid::ATTACH_POS_CENTRE);
        new Avoid::ShapeConnectionPin(shapeRef2, CENTRE, Avoid::ATTACH_POS_CENTRE, Avoid::ATTACH_POS_CENTRE);

        /* Create connector from each shape shapeRef2 to the input pin on shapeRef1. */
        Avoid::ConnEnd dstEnd(shapeRef1, CENTRE);
        Avoid::ConnEnd srcEnd(shapeRef2, CENTRE);
        dag->last_connref_id++;
        Avoid::ConnRef *connRef = new Avoid::ConnRef(dag->router, srcEnd, dstEnd, dag->last_connref_id);

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
 * Add the list of objects(either solids, comb or region) in the hash tables and to the router.
 */
int
add_objects(struct ged *gedp, struct _ged_dag_data *dag)
{
    struct directory *dp = NULL, *ndp = NULL;
    struct bu_vls dp_name_vls = BU_VLS_INIT_ZERO;
    int i;
    struct output o;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    Avoid::ShapeRef *shapeRef = NULL;

    /* Create the hash tables. Each one will have at most 64 entries. */
    o.regions = bu_create_hash_tbl(1);
    o.groups = bu_create_hash_tbl(1);
    o.solids = bu_create_hash_tbl(1);

    /* Sets a spacing distance for overlapping orthogonal connectors to be nudged apart. */
    dag->router->setOrthogonalNudgeDistance(25);

    /* Number of Avoid::ShapeRef objects in the graph. These corresponds to the ones in the database. */
    dag->object_nr = 0;

    /*
     * This value is used for establishing the starting id for the Avoid::ConnRef objects.
     * It starts with the value <nr_of_objects_in_the_database>.
     * It is incremented every time a new Avoid::ConnRef is added to the graph.
     * It is needed in order to avoid the overlapping with the Avoid::ShapeRef ids when adding a Avoid::ConnRef to the graph.
     */
    dag->last_connref_id = gedp->ged_wdbp->dbip->dbi_nrec;

    /* Traverse the database 'gedp'. */
    for (i = 0; i < RT_DBNHASH; i++) {
        for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
            bu_vls_sprintf(&dp_name_vls, "%s%s", "", dp->d_namep);
            ndp = db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&dp_name_vls), 1);
            if (ndp) {
                if(dp->d_flags & RT_DIR_SOLID) {
                    bu_log("Adding SOLID object [%s]\n", bu_vls_addr(&dp_name_vls));

                    /* Check if this solid is already in one of the hash tables. If not, add it to the solids hash table. */
                    struct bu_hash_entry *hsh_entry1, *hsh_entry2, *hsh_entry3;
                    struct bu_hash_entry *prev = NULL;
                    unsigned long idx;
                    int new_entry;

                    hsh_entry1 = bu_find_hash_entry(o.regions, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx);
                    hsh_entry2 = bu_find_hash_entry(o.groups, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx);
                    hsh_entry3 = bu_find_hash_entry(o.solids, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx);

                    if ((!hsh_entry1) && (!hsh_entry2) && (!hsh_entry3)) {
                        /* This object hasn't been registered yet. Add it into the solids hash table and create a shape for it. */
                        dag->object_nr++;

                        struct bu_hash_entry *hsh_entry = bu_hash_add_entry(o.solids, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &new_entry);
                        char *id = (char *)bu_malloc((size_t)6, "hash entry value");
                        sprintf(id, "%d", dag->object_nr);

                        /* Set the id value for this object */
                        bu_set_hash_value(hsh_entry, (unsigned char *)id);

                        /* Create and add a shape (obstacle) to the router */
                        shapeRef = add_object(dag, dag->object_nr);
                        dag->router->processTransaction();
                        bu_log("[SOLID]added %d objects.\n", dag->object_nr);
                    }

                } else if (dp->d_flags & RT_DIR_COMB) {
                    bu_log("Adding COMB object [%s]\n", bu_vls_addr(&dp_name_vls));
                    dag_comb(dbip, ndp, &o, dag);
                    bu_log("[COMB]added %d objects.\n", dag->object_nr);
                } else {
                    bu_log("Something else: [%s]\n", bu_vls_addr(&dp_name_vls));
                }
            } else {
                bu_log("ERROR: Unable to locate [%s] within input database, skipping.\n",  bu_vls_addr(&dp_name_vls));
            }
        }
    }

    ged_close(gedp);

    /* Free memory. */
    bu_vls_free(&dp_name_vls);
    dag->router->deleteShape(shapeRef);
    free_hash_values(o.regions);
    bu_hash_tbl_free(o.regions);
    free_hash_values(o.groups);
    bu_hash_tbl_free(o.groups);
    free_hash_values(o.solids);
    bu_hash_tbl_free(o.solids);

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
