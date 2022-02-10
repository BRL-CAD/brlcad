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
#include  <QTreeView>
#include "qtcad/QgModel.h"

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);

    bu_setprogname(argv[0]);

    argc--; argv++;

    if (argc != 1)
	bu_exit(-1, "need to specify .g file\n");

    QgModel sm(NULL, argv[0]);
    QgModel *s = &sm;

    if (!s->IsValid())
	bu_exit(-1, "failed to open .g file at %s\n", argv[0]);

    QTreeView tree;
    tree.setModel(s);
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
