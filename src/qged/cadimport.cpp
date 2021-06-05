/*                   C A D I M P O R T . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file cadimport.cxx
 *
 * Logic for launching command line importers.  Eventually libgcv
 * will probably replace the explicit program running, but the option
 * dialogs will still be necessary in some form...
 *
 */

#include <QFileInfo>
#include <QFormLayout>

#include "cadapp.h"
#include "cadimport.h"

#include "bu.h"

#include "brlcad_config.h" // For EXECUTABLE_SUFFIX

RhinoImportDialog::RhinoImportDialog(QString filename)
{
    input_file = filename;
    QFileInfo fileinfo(filename);
    QString g_path(fileinfo.path());
    g_path.append("/");
    g_path.append(fileinfo.baseName());
    QString l_path(g_path);
    g_path.append(".g");
    l_path.append(".log");

    db_path = new QLineEdit;
    db_path->insert(g_path);
    log_path = new QLineEdit;
    log_path->insert(l_path);
    scaling_factor = new QLineEdit;
    tolerance = new QLineEdit;
    verbosity = new QLineEdit;
    debug_printing = new QCheckBox;
    random_colors = new QCheckBox;
    uuid = new QCheckBox;

    formGroupBox = new QGroupBox("Import Options");
    QFormLayout *flayout = new QFormLayout;
    flayout->addRow(new QLabel("Output File"), db_path);
    flayout->addRow(new QLabel("Print Debugging Info"), debug_printing);
    flayout->addRow(new QLabel("Printing Verbosity"), verbosity);
    flayout->addRow(new QLabel("Scaling Factor"), scaling_factor);
    flayout->addRow(new QLabel("Tolerance"), tolerance);
    flayout->addRow(new QLabel("Randomize Colors"), random_colors);
    flayout->addRow(new QLabel("Use Universally Unique Identifiers\n(UUIDs) for Object Names"), uuid);
    flayout->addRow(new QLabel("Log File"), log_path);
    formGroupBox->setLayout(flayout);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
	    | QDialogButtonBox::Cancel);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &RhinoImportDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &RhinoImportDialog::reject);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(formGroupBox);
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);
    resize(600,300);
    setWindowTitle(tr("Rhino 3DM Importer (3dm-g)"));

}

QString
RhinoImportDialog::command()
{
    QString prog_name(bu_dir(NULL, 0, BU_DIR_BIN, "3dm-g", BU_DIR_EXT, NULL));
    return prog_name;
}


QStringList
RhinoImportDialog::options()
{
    QStringList process_args;

    if (debug_printing->isChecked()) process_args.append(" -d");
    if (verbosity->text().length() > 0) {
	process_args.append("-v");
	process_args.append(verbosity->text());
    }
    if (scaling_factor->text().length() > 0) {
	process_args.append("-s");
	process_args.append(scaling_factor->text());
    }
    if (tolerance->text().length() > 0) {
	process_args.append("-t");
	process_args.append(tolerance->text());
    }
    if (random_colors->isChecked()) process_args.append("-r");
    if (uuid->isChecked()) process_args.append("-u");

    // TODO - pop up a message and quit if we don't have an output file
    process_args.append("-o");
    process_args.append(db_path->text());

    process_args.append(input_file);

    return process_args;
}


STEPImportDialog::STEPImportDialog(QString filename)
{
    input_file = filename;
    QFileInfo fileinfo(filename);
    QString g_path(fileinfo.path());
    g_path.append("/");
    g_path.append(fileinfo.baseName());
    QString l_path(g_path);
    g_path.append(".g");
    l_path.append(".log");

    db_path = new QLineEdit;
    db_path->insert(g_path);
    log_path = new QLineEdit;
    log_path->insert(l_path);
    verbosity = new QCheckBox;
    verbosity->setCheckState(Qt::Checked);

    formGroupBox = new QGroupBox("Import Options");
    QFormLayout *flayout = new QFormLayout;
    flayout->addRow(new QLabel("Output File"), db_path);
    flayout->addRow(new QLabel("Printing Verbosity"), verbosity);
    flayout->addRow(new QLabel("Log File"), log_path);
    formGroupBox->setLayout(flayout);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
	    | QDialogButtonBox::Cancel);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &STEPImportDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &STEPImportDialog::reject);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(formGroupBox);
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);
    resize(600,300);
    setWindowTitle(tr("AP203 STEP Importer (step-g)"));

}

QString
STEPImportDialog::command()
{
    QString prog_name(bu_dir(NULL, 0, BU_DIR_BIN, "step-g", BU_DIR_EXT, NULL));
    return prog_name;
}


QStringList
STEPImportDialog::options()
{
    QStringList process_args;

    if (verbosity->isChecked()) process_args.append("-v");

    // TODO - pop up a message and quit if we don't have an output file
    process_args.append("-o");
    process_args.append(db_path->text());

    process_args.append(input_file);

    return process_args;
}



QString
import_db(QString filename) {

    QFileInfo fileinfo(filename);
    QString g_path("");
    QString l_path("");
    QString conversion_command;

    if (!fileinfo.suffix().compare("g", Qt::CaseInsensitive)) return filename;

    /* If we're not a .g, time for the conversion */

    if (!fileinfo.suffix().compare("3dm", Qt::CaseInsensitive)) {
       RhinoImportDialog dialog(filename);
       dialog.exec();
       g_path = dialog.db_path->text();
       l_path = dialog.log_path->text();
       // TODO - integrate logging mechanism into command execution
       ((CADApp *)qApp)->exec_console_app_in_window(dialog.command(),dialog.options(), l_path);
    }

    if (!fileinfo.suffix().compare("stp", Qt::CaseInsensitive) || !fileinfo.suffix().compare("step", Qt::CaseInsensitive)) {
       STEPImportDialog dialog(filename);
       dialog.exec();
       g_path = dialog.db_path->text();
       l_path = dialog.log_path->text();
       // TODO - integrate logging mechanism into command execution
       ((CADApp *)qApp)->exec_console_app_in_window(dialog.command(),dialog.options(), l_path);
    }

    return g_path;
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

