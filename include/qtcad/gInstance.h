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

class QTCAD_EXPORT GInstance
{
    public:
        explicit GInstance(struct directory *idp, struct db_i *idbip);
        ~GInstance();

	// Debugging function for printing out data in container
	std::string print();

	// Report if the instance has children
	bool hasChildren();

        // Return hashes of child instances, if any
        std::vector<unsigned long long> children(std::unordered_map<unsigned long long, GInstance *> *instances);

        // Given a dbip, construct or find the instances associated with it and add them
        // to the containers.  The instances maps may contain previous GInstances created
        // by earlier passes, and if they are still valid they will be reused.  Any GInstances
        // that are no longer valid will be removed from the maps and deleted.
        static void sync_instances(std::unordered_map<unsigned long long, GInstance *> *tops_instances, std::unordered_map<unsigned long long, GInstance *> *instances, struct db_i *dbip);

        inline int getActiveFlag() { return activeFlag; }
        inline void setActiveFlag(int flag) { activeFlag = flag; }

        inline unsigned long long getHash() { return hash; }

        inline directory *getParent() { return parent; }

        inline directory *getDP() { return dp; };
        inline void setDP(directory *newDP) { dp = newDP; }

        inline db_i *getDbip() { return dbip; }

        inline std::string getDpName() { return dpName; }

        inline const char *getDpNameStr() { return dpName.c_str();  }

        inline db_op_t getOp() { return op; }

        inline fastf_t *getCombMatrix() { return cM; }

    private:
        static void add_g_instance(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, int tree_op, void *pdp, void *inst_map, void *vchash, void *val_inst, void *c_set);
        static void dp_instances(std::unordered_map<unsigned long long, GInstance *> *valid_instances, std::unordered_map<unsigned long long, GInstance *> *instances, struct directory *dp, struct db_i *dbip);

	// This is a flag that may be set or unset by parent applications.
	// Used primarily to assist in visual identification of components
	// in hierarchical structures.  Not part of any hash calculations.
	int activeFlag = 0;

	// GInstance content based hash for quick lookup/comparison of two
	// GInstance.  mode == 3 - i.e., hash of full GInstance data.
	unsigned long long hash = 0;

	// dp of parent comb (NULL for tops objects)
	struct directory *parent = NULL;
	// dp of comb instance (NULL for instances with invalid object names)
	struct directory *dp = NULL;
	// dbip associated with this instance
	struct db_i *dbip = NULL;
	// instance name as string
	std::string dpName;
	// instance boolean operation incorporating it into the comb tree (u/-/+)
	db_op_t op = DB_OP_NULL;
	// Matrix above comb instance in comb tree (default is IDN)
	mat_t cM;
};

/* Given GInstance data, construct its hash.  This function is public to allow
 * for hashing of different "modes" in client codes wanting to do fuzzy matching
 * of instances. */
QTCAD_EXPORT extern unsigned long long
ginstance_hash(XXH64_state_t *h_state, int mode, struct directory *parent, std::string &dp_name, db_op_t op, mat_t c_m, int cnt);

#endif //GINSTANCE_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

