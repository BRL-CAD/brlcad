/*            Q G M O D E L _ D B _ U P D A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

/** @file qgmodel_db_update.cpp */

#include "common.h"

#include <QCoreApplication>
#include <QTemporaryDir>

#include "bu/app.h"
#include "qtcad/QgModel.h"


static bool
check_command(QgModel &model, int argc, const char **argv, int expected_rows)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    const int status = model.run_cmd(&result, argc, argv);
    const int rows = model.rowCount();
    if (status != BRLCAD_OK || rows != expected_rows)
	bu_log("%s returned %d with %d hierarchy rows, expected %d: %s\n",
	    argv[0], status, rows, expected_rows, bu_vls_cstr(&result));
    bu_vls_free(&result);
    return status == BRLCAD_OK && rows == expected_rows;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) {
	bu_log("Unable to create the model test directory\n");
	return 1;
    }

    const QByteArray first = directory.filePath("first.g").toUtf8();
    const QByteArray second = directory.filePath("second.g").toUtf8();
    QgModel model;

    const char *open_first[] = {"opendb", first.constData()};
    const char *open_second[] = {"opendb", second.constData()};
    const char *make[] = {"make", "sphere.s", "sph"};
    const char *kill[] = {"kill", "sphere.s"};
    const char *close[] = {"closedb"};
    if (!check_command(model, 2, open_first, 0) ||
	!check_command(model, 3, make, 1) ||
	!check_command(model, 2, kill, 0) ||
	!check_command(model, 1, close, 0) ||
	!check_command(model, 2, open_second, 0) ||
	!check_command(model, 3, make, 1))
	return 1;

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
