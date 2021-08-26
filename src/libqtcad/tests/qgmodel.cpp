/*                     Q G M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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
/** @file qgmodel.cpp
 *
 * Brief description
 *
 */

#include <iostream>
#include <unordered_map>
#include <vector>

#include "bu/app.h"
#include "bu/log.h"
#include "../../libged/alphanum.h"
#include "qtcad/QgModel.h"

void
open_children(QgItem *itm, QgModel_ctx *s, int depth, int max_depth)
{
    if (!itm || !itm->ihash)
	return;

    if (max_depth > 0 && depth >= max_depth)
	return;

    itm->open();
    for (int j = 0; j < itm->childCount(); j++) {
	QgItem *c = itm->child(j);
	if (s->instances->find(c->ihash) == s->instances->end())
	    continue;
	open_children(c, s, depth+1, max_depth);
    }
}

void
close_children(QgItem *itm)
{
    itm->close();
    for (int j = 0; j < itm->childCount(); j++) {
	QgItem *c = itm->child(j);
	close_children(c);
    }
}

void
print_children(QgItem *itm, QgModel_ctx *s, int depth)
{
    if (!itm || !itm->ihash)
	return;

    QgInstance *inst;
    if (depth == 0) {
	inst = (*s->tops_instances)[itm->ihash];
    } else {
	inst = (*s->instances)[itm->ihash];
    }

    for (int i = 0; i < depth; i++) {
	std::cout << "  ";
    }
    if (depth)
	std::cout << "* ";

    std::cout << inst->dp_name << "\n";

    for (int j = 0; j < itm->childCount(); j++) {
	QgItem *c = itm->child(j);
	if (s->instances->find(c->ihash) == s->instances->end())
	    continue;

	if (!itm->open_itm) {
	    continue;
	}

	print_children(c, s, depth+1);
    }
}

int main(int argc, char *argv[])
{

    bu_setprogname(argv[0]);

    argc--; argv++;

    if (argc != 1)
	bu_exit(-1, "need to specify .g file\n");

    struct db_i *dbip = db_open(argv[0], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL)
	bu_exit(-1, "db_open failed on geometry database file %s\n", argv[0]);

    RT_CK_DBI(dbip);

    dbip->dbi_wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);

    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
    }
    db_update_nref(dbip, &rt_uniresource);

    struct ged g;
    GED_INIT(&g, dbip->dbi_wdbp);

    QgModel_ctx s(NULL, &g);

    bu_log("Hierarchy instance cnt: %zd\n", s.instances->size());

    // The above logic will create hierarchy instances, but it won't create the
    // top level instances (which, by definition, aren't under anything.)  Make
    // a second pass using db_ls to create those instances (this depends on
    // db_update_nref being up to date.)
    bu_log("Top instance cnt: %zd\n", s.tops_instances->size());

    // 2.  Implement "open" and "close" routines for the items that will exercise
    // the logic to identify, populate, and clear items based on child info.

    // Open everything
    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	if (!itm->ihash)
	    continue;
	open_children(itm, &s, 0, -1);
    }
    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	if (!itm->ihash)
	    continue;
	print_children(itm, &s, 0);
    }

    // Close everything
    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	itm->close();
    }

    // Open first level
    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	if (!itm->ihash)
	    continue;
	open_children(itm, &s, 0, 1);
    }
    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	if (!itm->ihash)
	    continue;
	print_children(itm, &s, 0);
    }

    // Close
    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	close_children(itm);
    }

    // TODO - so the rough progression of steps here is:
    // 3.  Add callback support for syncing the instance sets after a database
    // operation.  This is the most foundational of the pieces needed for
    // read/write support.

    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	if (!itm->ihash)
	    continue;
	open_children(itm, &s, 0, 2);
    }
    std::cout << "Before\n";
    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	if (!itm->ihash)
	    continue;
	print_children(itm, &s, 0);
    }

    // Perform edit operations to trigger callbacks.  assuming
    // moss.g example
    int ac = 3;
    const char *av[4];
    av[0] = "rm";
    av[1] = "all.g";
    av[2] = "ellipse.r";
    av[3] = NULL;
    ged_exec(&g, ac, (const char **)av);

    std::cout << "After 1\n";
    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	if (!itm->ihash)
	    continue;
	print_children(itm, &s, 0);
    }

    av[0] = "g";
    av[1] = "all.g";
    av[2] = "ellipse.r";
    av[3] = NULL;
    ged_exec(&g, ac, (const char **)av);


    std::cout << "After 2\n";
    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	if (!itm->ihash)
	    continue;
	print_children(itm, &s, 0);
    }

    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	if (!itm->ihash)
	    continue;
	open_children(itm, &s, 0, 2);
    }
    std::cout << "After 3\n";
    for (size_t i = 0; i < s.tops_items.size(); i++) {
	QgItem *itm = s.tops_items[i];
	if (!itm->ihash)
	    continue;
	print_children(itm, &s, 0);
    }



    // The callback experiments we've been doing have some
    // of this, but we'll need to carefully consider how to handle it.  My
    // current thought is we'll accumulate items to change based on QgInstance
    // changes made, and then do a single Item update pass at the end for
    // performance reasons (we don't want the view updating a whole bunch if
    // thousands of objects are impacted by a single edit...)  We'll probably want
    // to update the QgInstances from the per-object callbacks, and then
    // rebuild the tops list and walk down the Items associated with those instances
    // to perform any needed updates.
    //
    // 4. Figure out how to do the Item update pass in response to #3.  In
    // particular, how to preserve the tree's "opened/closed" state through
    // edit operations.  For each child items vector we'll build a new vector
    // based on the QgInstances tree, comparing it as we go to the Items array
    // that existed previously.  For each old item, if the new item matches the
    // old (qghash comparison?) reuse the old item, otherwise create a new one.
    // This is where set_difference may be useful (not clear yet - if so it may require
    // some fancy tricks with the comparison function...)  We'll need to do this for
    // all active items, but unless the user has tried to expand all trees in
    // all paths this should be a relatively small subset of the .g file
    // structure to verify.
    //

    return (*s.instances).size();
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
