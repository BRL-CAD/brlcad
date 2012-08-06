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
#include <vector>

#define BRLCAD_PARALLEL PARALLEL
#undef PARALLEL
#define POSITION_COORDINATE 10.0

/* Adaptagrams Header */
#include "libavoid/libavoid.h"
using namespace Avoid;

/* Public Header */
#include "ged.h"


/**
 * This structure has fields that correspond to three lists for primitive, combination and non-geometry type objects of a database.
 */
struct output {
    std::vector<std::string> primitives;
    std::vector<std::string> combinations;
    std::vector<std::string> non_geometries;
};


/**
 * This structure has fields related to the representation of a graph with the help of the Adaptagrams library.
 */
struct _ged_dag_data {
    Avoid::Router *router;
    struct bu_hash_tbl *ids;
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
    bu_log("\tNew path: ");
    for (size_t i = 0; i < route.ps.size(); ++i) {
        bu_log("%s(%f, %f)", (i > 0) ? "-" : "",
                route.ps[i].x, route.ps[i].y);
    }
    bu_log("\n");
}


/**
 * Add one object to the router of the 'dag' structure.
 */
static Avoid::ShapeRef*
add_object(struct _ged_dag_data *dag, unsigned int id)
{
    Avoid::Point srcPt(dag->object_nr * POSITION_COORDINATE + 2.0, dag->object_nr * POSITION_COORDINATE + 2.0);
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
dag_comb(struct db_i *dbip, struct directory *dp, genptr_t out, struct _ged_dag_data *dag, struct bu_hash_tbl *objects)
{
    size_t i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    struct output *o = (struct output *)out;
    struct bu_hash_entry *prev = NULL;
    struct bu_hash_entry *hsh_entry_comb = NULL;
    unsigned long idx;
    unsigned int comb_id, subnode_id;

    Avoid::ShapeRef *shapeRef1 = NULL;
    Avoid::ShapeRef *shapeRef2 = NULL;
    const unsigned int CENTRE = 1;

    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
    bu_log("ERROR: Database read error, skipping %s\n", dp->d_namep);
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    /* Look for the ID of the current combination */
    if ((hsh_entry_comb = bu_find_hash_entry(objects, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx))) {
        comb_id = atoi((const char*)hsh_entry_comb->value);
    }

    /* Add the combination name to the vector */
    o->combinations.reserve(o->combinations.size() + 1);
    std::string comb_name(dp->d_namep);
    o->combinations.push_back(comb_name);

    /* Check if a shape was already created for this subnode. */
    bool shape_exists = false;
        ObstacleList::const_iterator finish = dag->router->m_obstacles.end();
        for (ObstacleList::const_iterator it = dag->router->m_obstacles.begin(); it != finish; ++it) {
            if ((*it)->id() == comb_id) {
                /* Don't create another shape because it already exists a corresponding one.
                 * Get a reference to the shape that corresponds to the current node of the subtree.
                 */
                shapeRef1 = dynamic_cast<ShapeRef *>(*it);
                shape_exists = true;
                break;
            }
        }
    if (!shape_exists) {
        /* Create a shape for the current node of the subtree */
        dag->object_nr++;
        shapeRef1 = add_object(dag, comb_id);
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

    bu_log("%d subnode(s)\n", actual_count);

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
        struct bu_hash_entry *hsh_entry;
        hsh_entry = bu_find_hash_entry(objects, (unsigned char *)rt_tree_array[i].tl_tree->tr_l.tl_name, strlen(rt_tree_array[i].tl_tree->tr_l.tl_name) + 1, &prev, &idx);

        if (hsh_entry) {
            subnode_id = atoi((const char*)hsh_entry->value);

            /* Check if a shape was already created for this subnode. */
            shape_exists = false;
                finish = dag->router->m_obstacles.end();
                for (ObstacleList::const_iterator it = dag->router->m_obstacles.begin(); it != finish; ++it) {
                    if ((*it)->id() == subnode_id) {
                        /* Don't create another shape because it already exists a corresponding one.
                         * Get a reference to the shape that corresponds to the current node of the subtree.
                         */
                        shapeRef2 = dynamic_cast<ShapeRef *>(*it);
                        shape_exists = true;
                        break;
                    }
                }
            if (!shape_exists) {
                /* Create a shape for the current node of the subtree */
                dag->object_nr++;
                shapeRef2 = add_object(dag, subnode_id);
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
        }

        db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
    }
    bu_vls_free(&vls);

    if (rt_tree_array)
        bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }

    rt_db_free_internal(&intern);
}


/**
 * Method that figures out the type of an object, constructs its corresponding shape in a graph
 * and adds it to the corresponding list of names.
 */
void
put_me_in_a_bucket(struct directory *dp, struct directory *ndp, struct db_i *dbip, struct bu_hash_tbl *objects, struct output *o, struct _ged_dag_data *dag)
{
    struct bu_hash_entry *prev = NULL;
    unsigned long idx;
    unsigned int object_id;
    struct bu_hash_entry *hsh_entry;
    struct bu_vls dp_name_vls = BU_VLS_INIT_ZERO;

    bu_vls_sprintf(&dp_name_vls, "%s%s", "", dp->d_namep);

    if(dp->d_flags & RT_DIR_SOLID) {
        bu_log("Adding PRIMITIVE object [%s]\n", bu_vls_addr(&dp_name_vls));

        /* Check if this solid is in the objects list. */
        prev = NULL;
        hsh_entry = bu_find_hash_entry(objects, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx);

        if (hsh_entry) {
            object_id = atoi((const char*)hsh_entry->value);

            o->primitives.reserve(o->primitives.size() + 1);
            std::string name((const char *)dp->d_namep);
            o->primitives.push_back(name);

            /* Check if a shape was already created for this subnode. */
            bool shape_exists = false;
            ObstacleList::const_iterator finish = dag->router->m_obstacles.end();
            for (ObstacleList::const_iterator it = dag->router->m_obstacles.begin(); it != finish; ++it) {
                if ((*it)->id() == object_id) {
                    /* Don't create another shape because it already exists a corresponding one. */
                    shape_exists = true;
                    break;
                }
            }
            if (!shape_exists) {
                /* Create a shape for the current node of the subtree */
                dag->object_nr++;
                add_object(dag, object_id);
                dag->router->processTransaction();
            }
        }
    } else if (dp->d_flags & RT_DIR_COMB) {
        bu_log("Adding COMB object [%s]\n", bu_vls_addr(&dp_name_vls));
        dag_comb(dbip, ndp, o, dag, objects);
    } else {
        bu_log("Something else: [%s]\n", bu_vls_addr(&dp_name_vls));
        prev = NULL;
        hsh_entry = bu_find_hash_entry(objects, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx);

        if (hsh_entry) {
            o->non_geometries.reserve(o->non_geometries.size() + 1);
            std::string name((const char *)dp->d_namep);
            o->non_geometries.push_back(name);
        }
    }

    bu_vls_free(&dp_name_vls);
}


/**
 * Add the list of objects in the master hash table "objects".
 */
int
add_objects(struct ged *gedp, struct _ged_dag_data *dag)
{
    struct directory *dp = NULL, *ndp = NULL;
    struct bu_vls dp_name_vls = BU_VLS_INIT_ZERO;
    int i, object_nr = 0;
    struct output o;
    struct bu_hash_tbl *objects;
    struct bu_hash_entry *hsh_entry1;
    struct bu_hash_entry *prev = NULL;
    unsigned long idx;
    struct db_i *dbip = gedp->ged_wdbp->dbip;

    /* Create the master "objects" hash table. It will have at most 64 entries. */
    objects = bu_create_hash_tbl(1);
    dag->ids = bu_create_hash_tbl(1);

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

    /* Traverse the database 'gedp' and for each object add its name and an ID into the "objects" hash table. */
    for (i = 0; i < RT_DBNHASH; i++) {
        for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
            bu_vls_sprintf(&dp_name_vls, "%s%s", "", dp->d_namep);
            ndp = db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&dp_name_vls), 1);
            if (ndp) {
                /* Check if this object is already in the hash table. If not, add it to the objects hash table. */
                int new_entry;

                hsh_entry1 = bu_find_hash_entry(objects, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx);

                if (!hsh_entry1) {
                    bu_log("Adding object [%s]\n", bu_vls_addr(&dp_name_vls));

                    /* This object hasn't been registered yet. Add it into the solids hash table and create a shape for it. */
                    object_nr++;

                    struct bu_hash_entry *hsh_entry = bu_hash_add_entry(objects, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &new_entry);
                    char *id = (char *)bu_malloc((size_t)6, "hash entry value");
                    sprintf(id, "%d", object_nr);

                    /* Set the id value for this object */
                    bu_set_hash_value(hsh_entry, (unsigned char *)id);

                    /* Add the ID of this object as a key and its name as a value. */
                    hsh_entry = bu_hash_add_entry(dag->ids, (unsigned char *)id, strlen(id) + 1, &new_entry);
                    bu_set_hash_value(hsh_entry, (unsigned char *)dp->d_namep);
                }
            } else {
                bu_log("ERROR: Unable to locate [%s] within input database, skipping.\n",  bu_vls_addr(&dp_name_vls));
            }
        }
    }

    /* Traverse the database 'gedp' and for each object check its type (primitive, combination or non-geometry)
     * and add its name into the corresponding list.
     */
    for (i = 0; i < RT_DBNHASH; i++) {
        for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
            bu_vls_sprintf(&dp_name_vls, "%s%s", "", dp->d_namep);
            ndp = db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&dp_name_vls), 1);
            if (ndp) {
                put_me_in_a_bucket(dp, ndp, dbip, objects, &o, dag);
            } else {
                bu_log("ERROR: Unable to locate [%s] within input database, skipping.\n",  bu_vls_addr(&dp_name_vls));
            }
        }
    }

    bu_log("Added %d objects.\n", dag->object_nr);

    /* Free memory. */
    bu_vls_free(&dp_name_vls);
    free_hash_values(objects);
    bu_hash_tbl_free(objects);

    return GED_OK;
}


/**
 * This routine returns a vector of vectors of points.
 * A vector of points defines a polygon shape that corresponds to an object in the database.
 */
int
ged_graph_objects_positions(struct ged *gedp, int argc, const char *argv[])
{
    struct _ged_dag_data *dag;
    const char *cmd = argv[0];

    /* The bounding positions of the rectangle corresponding to one object. */
    double minX;
    double minY;
    double maxX;
    double maxY;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 1 || argc == 0) {
            bu_vls_printf(gedp->ged_result_str, "Usage: %s", cmd);
            return GED_ERROR;
    }

    if(argc == 1) {
        dag = (struct _ged_dag_data *) bu_malloc(sizeof(_ged_dag_data), "DAG structure");
        dag->router = new Avoid::Router(Avoid::PolyLineRouting);
        add_objects(gedp, dag);

        ObstacleList::const_iterator finish = dag->router->m_obstacles.end();
        for (ObstacleList::const_iterator it = dag->router->m_obstacles.begin(); it != finish; ++it) {
            char *id = (char *)bu_malloc((size_t)6, "hash entry value");
            sprintf(id, "%d", (*it)->id());

            struct bu_hash_entry *prev = NULL;
            unsigned long idx;
            struct bu_hash_entry *hsh_entry= bu_find_hash_entry(dag->ids, (unsigned char *)id, strlen(id) + 1, &prev, &idx);
            if(hsh_entry) {
                (*it)->polygon().getBoundingRect(&minX, &minY, &maxX, &maxY);
                bu_vls_printf(gedp->ged_result_str, "%s %f %f %f %f\n", hsh_entry->value, minX, minY, maxX, maxY);
            }
        }

        bu_free(dag, "free DAG");
    }

    return GED_OK;
}


/**
 * This routine calls the add_objects() method for a given database if the "view"
 * subcommand is used.
 */
int
ged_graph_structure(struct ged *gedp, int argc, const char *argv[])
{
    struct _ged_dag_data *dag;
    size_t len;
    const char *cmd = argv[0];
    const char *sub;
    static const char *usage = "view\n";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2) {
            bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
            return GED_ERROR;
    }
    if(argc >= 2) {
        /* determine subcommand */
        sub = argv[1];
        len = strlen(sub);

        if(bu_strncmp(sub, "view", len) == 0) {
            dag = (struct _ged_dag_data *) bu_malloc(sizeof(_ged_dag_data), "DAG structure");
            dag->router = new Avoid::Router(Avoid::PolyLineRouting);
            add_objects(gedp, dag);

            dag->router->outputInstanceToSVG("dag2");
            dag->router->outputDiagramSVG("dag3");

            bu_free(dag, "free DAG");
        } else {
            bu_vls_printf(gedp->ged_result_str, "%s: %s is not a known subcommand!", cmd, sub);
            return GED_ERROR;
        }
    }
    return GED_OK;
}


#define PARALLEL BRLCAD_PARALLEL
#undef BRLCAD_PARALLEL


#else

#include "ged_private.h"

/**
 * Dummy graph functions in case no Adaptagrams library found
 */
int
ged_graph_structure(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* Initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_vls_printf(gedp->ged_result_str, "%s : ERROR This command is disabled due to the absence of Adaptagrams library",
          argv[0]);
    return GED_ERROR;
}


int
ged_graph_objects_positions(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* Initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_vls_printf(gedp->ged_result_str, "%s : ERROR This command is disabled due to the absence of Adaptagrams library",
          argv[0]);
    return GED_ERROR;
}


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
