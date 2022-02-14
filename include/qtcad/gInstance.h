/*                        G I N S T A N C E . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file gInstance.h
 *
 * A gInstance corresponds
 * to one unique parent+bool_op+matrix+child instance in a .g combination tree,
 *
 * gInstances are created to reflect the .g database state, and they are what
 * is updated as a direct consequence of callbacks registered with the low
 * level database I/O routines.
 */

#ifndef GINSTANCE_H
#define GINSTANCE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "xxhash.h"

#include "qtcad/defines.h"

#ifndef Q_MOC_RUN
#include "raytrace.h"
#include "ged.h"
#endif

class QTCAD_EXPORT gInstance
{
    public:
	explicit gInstance(struct directory *idp, struct db_i *idbip);
	~gInstance();

	// Debugging function for printing out data in container
	std::string print();

	// gInstance content based hash for quick lookup/comparison of two
	// gInstances.  By default incorporate everything into the hash -
	// other modes are allowed for in case we want to do fuzzy matching.
	unsigned long long hash(int mode = 3);

	// dp of parent comb (NULL for tops objects)
	struct directory *parent = NULL;
	// dp of comb instance (NULL for instances with invalid object names)
	struct directory *dp = NULL;
	// dbip associated with this instance
	struct db_i *dbip = NULL;
	// instance name as string
	std::string dp_name;
	// instance boolean operation incorporating it into the comb tree (u/-/+)
	db_op_t op = DB_OP_NULL;
	// Matrix above comb instance in comb tree (default is IDN)
	mat_t c_m;

	// Return hashes of child instances, if any
	std::vector<unsigned long long> children(std::unordered_map<unsigned long long, gInstance *> *instances);

	// This is a flag that may be set or unset by parent applications.
	// Used primarily to assist in visual identification of components
	// in hierarchical structures.
	int active_flag = 0;

    private:
	XXH64_state_t *h_state;
};

QTCAD_EXPORT extern unsigned long long
ginstance_hash(XXH64_state_t *h_state, int mode, struct directory *parent, std::string &dp_name, db_op_t op, mat_t c_m);

// Given a dp, generate instances and add them to the map
// Certain .g objects (comb, extrude, etc.) will define one or more
// implicit instances.  We need to create those instances both on
// initialization of an existing .g file and on database editing.
QTCAD_EXPORT extern void
add_instances(std::unordered_map<unsigned long long, gInstance *> *instances, struct directory *dp, struct db_i *dbip);

// A modification in the .g file to an object may necessitate a variety
// of updates to the gInstance containers
QTCAD_EXPORT extern void
update_child_instances(std::unordered_map<unsigned long long, gInstance *> *instances, struct directory *dp, struct db_i *dbip);

QTCAD_EXPORT extern void
update_tops_instances(
	std::unordered_map<unsigned long long, gInstance *> *tops_instances,
	std::unordered_map<unsigned long long, gInstance *> *instances,
	struct db_i *dbip);


QTCAD_EXPORT extern void
initialize_instances(
	std::unordered_map<unsigned long long, gInstance *> *tops_instances,
	std::unordered_map<unsigned long long, gInstance *> *instances,
	struct db_i *dbip);


#endif //GINSTANCE_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

