/*                     Q G V I E W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2022 United States Government as represented by
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
/** @file qgview.cpp
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
#include  <QApplication>
#include "qtcad/QgModel.h"
#include "qtcad/QgSelectionProxyModel.h"
#include "qtcad/QgTreeView.h"

void
open_children(QgItem *itm, QgModel *s, int depth, int max_depth)
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
open_tops(QgModel *s, int depth)
{
    for (size_t i = 0; i < s->tops_items.size(); i++) {
	QgItem *itm = s->tops_items[i];
	if (!itm->ihash)
	    continue;
	open_children(itm, s, 0, depth);
    }
}


int main(int argc, char *argv[])
{

    QApplication app(argc, argv);

    bu_setprogname(argv[0]);

    argc--; argv++;

    if (argc != 1)
	bu_exit(-1, "need to specify .g file\n");

    QgModel sm(NULL, argv[0]);
    QgModel *s = &sm;

    //open_tops(s, -1);

    QgSelectionProxyModel sp;
    sp.setSourceModel(s);

    QgTreeView tree(NULL, &sp);
    tree.setWindowTitle(argv[0]);
    tree.show();


    return app.exec();
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
