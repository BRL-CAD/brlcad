/*                     C A D H E L P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file cadhelp.cpp
 *
 * Brief description
 *
 */

#include <iostream>
#include <QFile>
#include "cadhelp.h"
#include "app.h"

#include "bu.h"

CADManViewer::CADManViewer(QWidget *pparent, QString *file) : QDialog(pparent)
{
    QVBoxLayout *dlayout = new QVBoxLayout;
    buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &CADManViewer::accept);

    browser = new QTextBrowser();
    QString filename(*file);
    QFile manfile(filename);
    if (manfile.open(QFile::ReadOnly | QFile::Text)) {
	browser->setHtml(manfile.readAll());
    }
    dlayout->addWidget(browser);
    dlayout->addWidget(buttons);
    setLayout(dlayout);

    // TODO - find something smarter to do for size
    setMinimumWidth(800);
    setMinimumHeight(600);

    setWindowModality(Qt::NonModal);
}

CADManViewer::~CADManViewer()
{
    delete browser;
}

int cad_man_view(QString *args, CADApp *app)
{
    Q_UNUSED(app);
    // TODO - a language setting in the application would be appropriate here so we can select language appropriate docs...
    QString man_path_root("doc/html/mann/");
    man_path_root.append(args);
    man_path_root.append(".html");
    const char *mroot = man_path_root.toLocal8Bit();
    const char *mdir = bu_dir(NULL, 0, BU_DIR_DATA, mroot, NULL);
    bu_log("mdir: %s\n", mdir);
    QString man_file(mdir);
    CADManViewer *viewer = new CADManViewer(0, &man_file);
    viewer->show();
    return 0;
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

