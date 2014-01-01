/*                         D A G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
#define POSITION_COORDINATE 50.0
#define X_WIDTH 30.0
#define Y_HEIGHT 30.0

/* Maximum dimension of a subcommand's name. */
#define NAME_SIZE 10

/* Adaptagrams Header */
#include "libavoid/libavoid.h"
using namespace Avoid;

/* Public Header */
#include "ged.h"


/**
 * Subcommand name information, for a table of available subcommands.
 */
struct graph_subcmd_tab {
    char *name;
};

/**
 * Table of graph subcommand functions.
 */
static const struct graph_subcmd_tab graph_subcmds[] = {
    {(char *)"show"},
    {(char *)"positions"},
    {(char *)NULL}
};

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
    struct bu_hash_tbl *object_types;
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
    connRef->route();
}


/**
 * Routine that returns the root nodes within the graph.
 */
std::vector<unsigned int>
find_roots(_ged_dag_data *dag)
{
    std::vector<unsigned int> roots;
    ObstacleList::const_iterator finish = dag->router->m_obstacles.end();

    for (ObstacleList::const_iterator it = dag->router->m_obstacles.begin(); it != finish; ++it) {
	Avoid::IntList shapes_runningFrom;
	dag->router->attachedShapes(shapes_runningFrom, (*it)->id(), Avoid::runningFrom);

	/* Check if there is no edge that goes in this node and that there exist edges going out of it. */
	if (shapes_runningFrom.size() == 0) {
	    roots.push_back((*it)->id());
	}
    }
    return roots;
}


/**
 * This routine sets the position of the node depending on its level within the tree
 * and on the maximum X coordinate on that level.
 */
void
position_node(_ged_dag_data *dag, bool has_parent, Avoid::ShapeRef *parent, Avoid::ShapeRef *child, unsigned int &level, std::vector<double> &maxX_level)
{
    double new_x, new_y, new_x1, new_y1;

#define LIBAVOID_LATEST_API
#if defined LIBAVOID_LATEST_API
    #define LIBX 0
    #define LIBY 1
    Box bbox = child->polygon().offsetBoundingBox(0);
    new_x  = bbox.min[LIBX];
    new_y  = bbox.min[LIBY];
    new_x1 = bbox.max[LIBX];
    new_y1 = bbox.max[LIBY];
#else
    child->polygon().getBoundingRect(&new_x, &new_y, &new_x1, &new_y1);
#endif

    /* If it has a parent it means that is not the root node.
     * Therefore, set the child's position depending on the parent's position.
     */
    if (has_parent) {
	double parent_x, parent_y, parent_x1, parent_y1;

#if defined LIBAVOID_LATEST_API
	Box parent_bbox = parent->polygon().offsetBoundingBox(0);
	parent_x  = bbox.min[LIBX];
	parent_y  = bbox.min[LIBY];
	parent_x1 = bbox.max[LIBX];
	parent_y1 = bbox.max[LIBY];
#else
	parent->polygon().getBoundingRect(&parent_x, &parent_y, &parent_x1, &parent_y1);
#endif
	/* If the child node is not already on that level. Reset its y coordinate. */
	if (!NEAR_EQUAL(new_y, parent_y + POSITION_COORDINATE, 0.001)) {
	    new_y = parent_y + POSITION_COORDINATE;
	} else {
	    maxX_level[level] = std::max(new_x1, maxX_level[level]);
	    return;
	}
    } else {
	/* This means that it is a root node. */
	new_y = POSITION_COORDINATE - 20.0;
    }
    new_x = maxX_level[level] + POSITION_COORDINATE;
    new_x1 = new_x + X_WIDTH;
    new_y1 = new_y + Y_HEIGHT;

    /* Construct a new polygon. */
    Avoid::Point srcPt(new_x, new_y);
    Avoid::Point dstPt(new_x1, new_y1);
    Avoid::Rectangle shapeRect(srcPt, dstPt);

    /* Move the shape by setting its new polygon. */
    dag->router->moveShape(child, shapeRect, true);
    maxX_level[level] = std::max(new_x1, maxX_level[level]);
}


/**
 * Routine that sets the layout of the graph.
 * It recurses over all nodes of a tree, starting from the root node.
 */
void
set_layout(_ged_dag_data *dag, bool has_parent, unsigned long int parent_id, unsigned long int child_id, unsigned int &level, std::vector<double> &maxX_level)
{
    Avoid::ShapeRef *parent = NULL;
    Avoid::ShapeRef *child = NULL;
    std::list<unsigned int> children;

    /* Find references to the parent and child shapes. */
    ObstacleList::const_iterator finish = dag->router->m_obstacles.end();
    for (ObstacleList::const_iterator it = dag->router->m_obstacles.begin(); it != finish; ++it) {
	if (has_parent && (*it)->id() == parent_id) {
	    parent = dynamic_cast<ShapeRef *>(*it);
	} else if ((*it)->id() == child_id) {
	    child = dynamic_cast<ShapeRef *>(*it);

	    /* Find the child's children. */
	    dag->router->attachedShapes(children, child_id, Avoid::runningTo);
	}
	if ((parent && child) || (child && !has_parent)) {
	    break;
	}
    }

    /* Set the position of the child node. */
    position_node(dag, has_parent, parent, child, level, maxX_level);
    dag->router->processTransaction();

    if (children.size() == 0) {
	return;
    } else {
	has_parent = true;
	std::list<unsigned int>::iterator it;

	level ++;
	if (maxX_level.size() <= level) {
	    maxX_level.reserve(level + 1);
	    maxX_level.push_back(0.0);
	}
	for ( it=children.begin() ; it != children.end(); it++) {
	    set_layout(dag, has_parent, child_id, *it, level, maxX_level);
	}
	level --;
    }
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
 * Method that "decorates" each object depending on its type: primitive / combination / something else.
 * A new entry is added into the objects_types hash table with the name of the object as a key and
 * the type of the object as a value.
 */
void
decorate_object(struct _ged_dag_data *dag, char *object_name, int object_type)
{
    int new_entry;
    struct bu_hash_entry *hsh_entry = bu_hash_tbl_add(dag->object_types, (unsigned char *)object_name, strlen(object_name) + 1, &new_entry);

    char *type = (char *)bu_malloc((size_t)1, "hash entry value");
    sprintf(type, "%d", object_type);
    bu_set_hash_value(hsh_entry, (unsigned char *)type);
}


/**
 * Function which processes a combination node. I.e., it adds a new entry into the hash tables, if necessary.
 * In this case, it also adds a Avoid::ShapeRef object to the graph. It also traverses its subtree, adds
 * entries into the hash table for solid objects, if necessary. In this case as well, it also adds a
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
    if ((hsh_entry_comb = bu_hash_tbl_find(objects, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx))) {
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
	hsh_entry = bu_hash_tbl_find(objects, (unsigned char *)rt_tree_array[i].tl_tree->tr_l.tl_name, strlen(rt_tree_array[i].tl_tree->tr_l.tl_name) + 1, &prev, &idx);

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
	hsh_entry = bu_hash_tbl_find(objects, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx);

	if (hsh_entry) {
	    object_id = atoi((const char*)hsh_entry->value);

	    o->primitives.reserve(o->primitives.size() + 1);
	    std::string name((const char *)dp->d_namep);
	    o->primitives.push_back(name);

	    /* "Decorate" the object. Add its name and type into the hash table. */
	    decorate_object(dag, dp->d_namep, dp->d_flags);

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

	/* "Decorate" the object. Add its name and type into the hash table. */
	decorate_object(dag, dp->d_namep, dp->d_flags);
    } else {
	bu_log("Something else: [%s]\n", bu_vls_addr(&dp_name_vls));
	prev = NULL;
	hsh_entry = bu_hash_tbl_find(objects, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx);

	if (hsh_entry) {
	    o->non_geometries.reserve(o->non_geometries.size() + 1);
	    std::string name((const char *)dp->d_namep);
	    o->non_geometries.push_back(name);

	    /* "Decorate" the object. Add its name and type into the hash table. */
	    decorate_object(dag, dp->d_namep, dp->d_flags);
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
    objects = bu_hash_tbl_create(1);
    dag->ids = bu_hash_tbl_create(1);
    dag->object_types = bu_hash_tbl_create(1);

    /* Sets a spacing distance for overlapping orthogonal connectors to be nudged apart. */
#if defined LIBAVOID_LATEST_API
    dag->router->setRoutingParameter(idealNudgingDistance, 25);
#else
    dag->router->setOrthogonalNudgeDistance(25);
#endif

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

		hsh_entry1 = bu_hash_tbl_find(objects, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &prev, &idx);

		if (!hsh_entry1) {
		    bu_log("Adding object [%s]\n", bu_vls_addr(&dp_name_vls));

		    /* This object hasn't been registered yet. Add it into the solids hash table and create a shape for it. */
		    object_nr++;

		    struct bu_hash_entry *hsh_entry = bu_hash_tbl_add(objects, (unsigned char *)dp->d_namep, strlen(dp->d_namep) + 1, &new_entry);
		    char *id = (char *)bu_malloc((size_t)6, "hash entry value");
		    sprintf(id, "%d", object_nr);

		    /* Set the id value for this object */
		    bu_set_hash_value(hsh_entry, (unsigned char *)id);

		    /* Add the ID of this object as a key and its name as a value. */
		    hsh_entry = bu_hash_tbl_add(dag->ids, (unsigned char *)id, strlen(id) + 1, &new_entry);
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
 * This routine provides the name of the objects in a database along with their
 * type and positions within the graph.
 */
HIDDEN void
graph_positions(struct ged *gedp, struct _ged_dag_data *dag)
{
    /* The bounding positions of the rectangle corresponding to one object. */
    double minX;
    double minY;
    double maxX;
    double maxY;

    add_objects(gedp, dag);

    /* Find the root nodes. */
    std::vector<unsigned int> root_ids = find_roots(dag);
    std::vector<double> maxX_level;
    unsigned int level;
    unsigned int size = root_ids.size();

    /* Traverse the root nodes and for each tree set the positions of all the nodes. */
    for (unsigned int i = 0; i < size; i++) {
	char *root = (char *)bu_malloc((size_t)6, "hash entry value");
	sprintf(root, "%u", root_ids[i]);
	struct bu_hash_entry *prev_root = NULL;
	unsigned long idx_root;
	struct bu_hash_entry *hsh_entry_root = bu_hash_tbl_find(dag->ids, (unsigned char *)root, strlen(root) + 1, &prev_root, &idx_root);

	if (hsh_entry_root) {
	    level = 0;
	    if (i == 0) {
		/* First root node: set the maximum X coordinate to be 0.0. */
		maxX_level.reserve(1);
		maxX_level.push_back(0.0);
	    }

	    /* Set the layout for the tree that starts with this root node. */
	    set_layout(dag, false, root_ids[i], root_ids[i], level, maxX_level);
	}
    }

    /* Traverse each shape within the graph and pass on the name, type and coordinates
     * of the object.
     */
    ObstacleList::const_iterator finish = dag->router->m_obstacles.end();
    for (ObstacleList::const_iterator it = dag->router->m_obstacles.begin(); it != finish; ++it) {
	char *id = (char *)bu_malloc((size_t)6, "hash entry value");
	sprintf(id, "%d", (*it)->id());

	struct bu_hash_entry *prev = NULL;
	unsigned long idx;
	struct bu_hash_entry *hsh_entry = bu_hash_tbl_find(dag->ids, (unsigned char *)id, strlen(id) + 1, &prev, &idx);
	if(hsh_entry) {
#if defined LIBAVOID_LATEST_API
	  Box bbox = (*it)->polygon().offsetBoundingBox(0);
	  minX = bbox.min[LIBX];
	  minY = bbox.min[LIBY];
	  maxX = bbox.max[LIBX];
	  maxY = bbox.max[LIBY];
#else
	    (*it)->polygon().getBoundingRect(&minX, &minY, &maxX, &maxY);
#endif
	    prev = NULL;
	    struct bu_hash_entry *hsh_entry_type = bu_hash_tbl_find(dag->object_types, hsh_entry->value,
								      strlen((char *)hsh_entry->value) + 1, &prev, &idx);
	    if(hsh_entry_type) {
		bu_vls_printf(gedp->ged_result_str, "%s %s %f %f %f %f\n", hsh_entry->value, hsh_entry_type->value, minX,
			      minY, maxX, maxY);
	    }
	}
    }
}


/**
 * Routine that sets a list of names, types, and positions that correspond to the objects in the gedp database.
 * The positions refer to the coordinates that determine the nodes within the graph.
 * This routine is called when the "positions" subcommand is used.
 */
int
_ged_graph_positions(struct ged *gedp)
{
    struct _ged_dag_data *dag;

    BU_ALLOC(dag, struct _ged_dag_data);
    dag->router = new Avoid::Router(Avoid::PolyLineRouting);

    /* Get the name, type and position within the graph for each object. */
    graph_positions(gedp, dag);
    bu_free(dag, "free DAG");

    return GED_OK;
}


/**
 * Routine that sets a list of names, types, and positions that correspond to the objects in the gedp database,
 * along with the positions of the edges that connect the nodes from the graph.
 * The positions refer to the coordinates that determine the nodes within the graph.
 * This routine is called when the "show" subcommand is used.
 */
int
_ged_graph_show(struct ged *gedp)
{
    struct _ged_dag_data *dag;

    BU_ALLOC(dag, struct _ged_dag_data);
    dag->router = new Avoid::Router(Avoid::PolyLineRouting);

    /* Get the name, type and position within the graph for each object. */
    graph_positions(gedp, dag);

    /* Provide the positions of the points via which the connection's route passes. */
    double x, y;
    Avoid::ConnRefList::const_iterator fin = dag->router->connRefs.end();
    for (Avoid::ConnRefList::const_iterator i = dag->router->connRefs.begin(); i != fin; ++i) {
	bu_vls_printf(gedp->ged_result_str, "edge\n");
	Avoid::Polygon polyline = (*i)->displayRoute();
	unsigned int size = polyline.ps.size();

	for (unsigned int j = 0; j < size; j ++) {
	    x = polyline.ps[j].x;
	    y = polyline.ps[j].y;
	    bu_vls_printf(gedp->ged_result_str, "%f %f\n", x, y);
	}
    }
    bu_free(dag, "free DAG");

    return GED_OK;
}


/**
 * The libged graph function.
 * This function constructs the graph structure corresponding to the given database.
 */
int
ged_graph(struct ged *gedp, int argc, const char *argv[])
{
    const char *cmd = argv[0];
    const char *subcommand;
    static const char *usage = "subcommand\n";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Didn't provide any subcommand. Must be wanting help */
    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %sAvailable subcommands: ", cmd, usage);

	/* Output the available subcommands. */
	for (int i = 0; graph_subcmds[i].name; ++i) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", graph_subcmds[i].name);
	}
	return GED_ERROR;
    } else if (argc == 2) {
	/* Determine subcommand. */
	subcommand = argv[1];

	/* Check if it's the "show" subcommand. */
	if (BU_STR_EQUAL(graph_subcmds[0].name, subcommand)) {
	    return _ged_graph_show(gedp);
	} else if (BU_STR_EQUAL(graph_subcmds[1].name, subcommand)) {
	    /* It is the "positions" subcommand. */
	    return _ged_graph_positions(gedp);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a known subcommand!\nAvailable subcommands: ", cmd, subcommand);

	    /* Output the available subcommands. */
	    for (int i = 0; graph_subcmds[i].name; ++i) {
		bu_vls_printf(gedp->ged_result_str, "%s ", graph_subcmds[i].name);
	    }
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
 * Dummy graph function in case no Adaptagrams library is found.
 */
int
ged_graph(struct ged *gedp, int argc, const char *argv[])
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
