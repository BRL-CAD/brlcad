/*                     Q G M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2024 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details->
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

#include "common.h"
#include <iostream>
#include <unordered_map>
#include <vector>
#ifdef USE_QTTEST
#  include <QAbstractItemModelTester>
#endif

#include "bu/app.h"
#include "bu/log.h"
#include "../../libged/alphanum.h"
#include "qtcad/QgModel.h"

void
open_children(QgItem *itm, QgModel *s, int depth, int max_depth)
{
    if (!itm || !itm->ihash)
	return;

    if (max_depth > 0 && depth >= max_depth)
	return;

    itm->open();

    for (size_t j = 0; j < itm->children.size(); j++) {
	QgItem *c = itm->child(j);
	open_children(c, s, depth+1, max_depth);
    }
}

void
open_tops(QgModel *s, int depth)
{
    for (size_t i = 0; i < s->tops_items.size(); i++) {
	QgItem *itm = s->tops_items[i];
	if (!itm->ihash)
	    continue;
	open_children(itm, s, 0, depth);
    }
}

void
close_children(QgItem *itm)
{
    itm->close();
    for (size_t j = 0; j < itm->children.size(); j++) {
	QgItem *c = itm->child(j);
	close_children(c);
    }
}

void
print_children(QgItem *itm, QgModel *s, int depth)
{
    if (!itm || !itm->ihash)
	return;


    for (int i = 0; i < depth; i++) {
	std::cout << "  ";
    }
    if (depth)
	std::cout << "* ";

    struct bu_vls path_str = BU_VLS_INIT_ZERO;
    std::vector<unsigned long long> path_hashes = itm->path_items();
    itm->mdl->gedp->dbi_state->print_hash(&path_str, path_hashes[path_hashes.size()-1]);
    std::cout << bu_vls_cstr(&path_str) << "\n";
    bu_vls_free(&path_str);

    for (size_t j = 0; j < itm->children.size(); j++) {
	QgItem *c = itm->child(j);

	if (!itm->open_itm) {
	    continue;
	}

	print_children(c, s, depth+1);
    }
}

void
print_tops(QgModel *s)
{
    for (size_t i = 0; i < s->tops_items.size(); i++) {
	QgItem *itm = s->tops_items[i];
	if (!itm->ihash)
	    continue;
	print_children(itm, s, 0);
    }
}

int main(int argc, char *argv[])
{

    bu_setprogname(argv[0]);

    argc--; argv++;

    if (argc != 1)
	bu_exit(-1, "need to specify .g file\n");

    QgModel sm(NULL, argv[0]);
    QgModel *s = &sm;

#ifdef USE_QTTEST
    //QAbstractItemModelTester *tester = new QAbstractItemModelTester((QAbstractItemModel *)s, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QAbstractItemModelTester *tester = new QAbstractItemModelTester((QAbstractItemModel *)s, QAbstractItemModelTester::FailureReportingMode::Warning);
#endif

    // 2.  Implement "open" and "close" routines for the items that will exercise
    // the logic to identify, populate, and clear items based on child info.

    // Open everything
    std::cout << "\nAll open:\n";
    open_tops(s, -1);
    print_tops(s);

    // Close top level
    std::cout << "\nTop level closed:\n";
    for (size_t i = 0; i < s->tops_items.size(); i++) {
	QgItem *itm = s->tops_items[i];
	itm->close();
    }
    print_tops(s);


    // Open first level
    std::cout << "\nOpen first level (remember open children):\n";
    open_tops(s, 1);
    print_tops(s);


    // Close everything
    std::cout << "\nEverything closed:\n";
    for (size_t i = 0; i < s->tops_items.size(); i++) {
	QgItem *itm = s->tops_items[i];
	close_children(itm);
    }
    print_tops(s);

    // Open first level
    std::cout << "\nOpen first level (children closed):\n";
    open_tops(s, 1);
    print_tops(s);


    // 3.  Add callback support for syncing the instance sets after a database
    // operation.  This is the most foundational of the pieces needed for
    // read/write support.

    std::cout << "\nInitial state:\n";
    open_tops(s, 2);
    print_tops(s);

    struct ged *g = s->gedp;

    // Perform edit operations to trigger callbacks->  assuming
    // moss->g example
    int ac = 3;
    const char *av[4];
    av[0] = "rm";
    av[1] = "all.g";
    av[2] = "ellipse.r";
    av[3] = NULL;
    ged_exec(g, ac, (const char **)av);
    s->g_update(g->dbip);

    std::cout << "\nRemoved ellipse.r from all.g:\n";
    print_tops(s);

    av[0] = "g";
    av[1] = "all.g";
    av[2] = "ellipse.r";
    av[3] = NULL;
    ged_exec(g, ac, (const char **)av);
    s->g_update(g->dbip);

    std::cout << "\nAdded ellipse.r back to the end of all.g, no call to open:\n";
    print_tops(s);

    std::cout << "\nAfter additional open pass on tree:\n";
    open_tops(s, 2);
    print_tops(s);


    av[0] = "rm";
    av[1] = "tor.r";
    av[2] = "tor";
    av[3] = NULL;
    ged_exec(g, ac, (const char **)av);
    s->g_update(g->dbip);

    std::cout << "\ntops tree after removing tor from tor.r:\n";
    print_tops(s);

    av[0] = "kill";
    av[1] = "-f";
    av[2] = "all.g";
    av[3] = NULL;
    ged_exec(g, ac, (const char **)av);
    s->g_update(g->dbip);
    std::cout << "\ntops tree after deleting all.g:\n";
    print_tops(s);

    std::cout << "\nexpanded tops tree after deleting all.g:\n";
    open_tops(s, -1);
    print_tops(s);

    const char *objs[] = {"box.r", "box.s", "cone.r", "cone.s", "ellipse.r", "ellipse.s", "light.r", "LIGHT", "platform.r", "platform.s", "tor", "tor.r", NULL};
    const char *obj = objs[0];
    int i = 0;
    while (obj) {
	av[0] = "kill";
	av[1] = "-f";
	av[2] = obj;
	av[3] = NULL;
	ged_exec(g, ac, (const char **)av);
	i++;
	obj = objs[i];
    }
    s->g_update(g->dbip);
    std::cout << "\ntops tree after deleting everything:\n";
    print_tops(s);

    std::cout << "\nexpanded tops tree after deleting everything:\n";
    open_tops(s, -1);
    print_tops(s);

#ifdef USE_QTTEST
    delete tester;
#endif

    // TODO - so the rough progression of steps here is:
    //
    // 4. Figure out how to do the Item update pass in response to #3.  In
    // particular, how to preserve the tree's "opened/closed" state through
    // edit operations->  For each child items vector we'll build a new vector
    // based on the gInstances tree, comparing it as we go to the Items array
    // that existed previously.  For each old item, if the new item matches the
    // old (qghash comparison?) reuse the old item, otherwise create a new one.
    // This is where set_difference may be useful (not clear yet - if so it may require
    // some fancy tricks with the comparison function...)  We'll need to do this for
    // all active items, but unless the user has tried to expand all trees in
    // all paths this should be a relatively small subset of the .g file
    // structure to verify.
    //

    return -1;
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
