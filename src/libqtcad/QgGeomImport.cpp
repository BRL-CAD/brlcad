/*                 Q G G E O M I M P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2023 United States Government as represented by
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
/** @file QgGeomImport.cpp
 *
 */

#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include "qtcad/QgAppExecDialog.h"
#include "qtcad/QgGeomImport.h"

ASCImportDialog::ASCImportDialog(QString filename, QString g_path, QString l_path)
{
    input_file = filename;

    db_path = new QLineEdit;
    db_path->insert(g_path);
    log_path = new QLineEdit;
    log_path->insert(l_path);

    formGroupBox = new QGroupBox("Import Options");
    QFormLayout *flayout = new QFormLayout;
    flayout->addRow(new QLabel("Output File"), db_path);
    flayout->addRow(new QLabel("Log File"), log_path);
    formGroupBox->setLayout(flayout);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &ASCImportDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ASCImportDialog::reject);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(formGroupBox);
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);
    resize(600,300);
    setWindowTitle(tr("BRL-CAD ASCII Importer"));
}

QString
ASCImportDialog::command()
{
    QString prog_name(bu_dir(NULL, 0, BU_DIR_BIN, "gcv", BU_DIR_EXT, NULL));
    return prog_name;
}


QStringList
ASCImportDialog::options()
{
    QStringList process_args;

    process_args.append(input_file);
    process_args.append(db_path->text());

    return process_args;
}

RhinoImportDialog::RhinoImportDialog(QString filename, QString g_path, QString l_path)
{
    input_file = filename;

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

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

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

    process_args.append("-o");
    process_args.append(db_path->text());

    process_args.append(input_file);

    return process_args;
}

STEPImportDialog::STEPImportDialog(QString filename, QString g_path, QString l_path)
{
    input_file = filename;

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

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

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

    process_args.append("-o");
    process_args.append(db_path->text());

    process_args.append(input_file);

    return process_args;
}

QgGeomImport::QgGeomImport(QWidget *pparent) : QObject(pparent)
{
}

QgGeomImport::~QgGeomImport()
{
    bu_vls_free(&conv_msg);
}

int
QgGeomImport::exec_console_app_in_window(QString command, QStringList options, QString lfile)
{
    if (command.length() > 0) {

	QgAppExecDialog *out_win = new QgAppExecDialog(0, command, options, lfile);
	QString win_title("Running ");
	win_title.append(command);
	out_win->setWindowTitle(win_title);
	out_win->proc = new QProcess(out_win);
	out_win->console->setMinimumHeight(800);
	out_win->console->setMinimumWidth(800);
	out_win->console->printString(command);
	out_win->console->printString(QString(" "));
	out_win->console->printString(options.join(" "));
	out_win->console->printString(QString("\n"));
	out_win->proc->setProgram(command);
	out_win->proc->setArguments(options);
	connect(out_win->proc, &QProcess::readyReadStandardOutput, out_win, &QgAppExecDialog::read_stdout);
	connect(out_win->proc, &QProcess::readyReadStandardError, out_win, &QgAppExecDialog::read_stderr);
	connect(out_win->proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), out_win, &QgAppExecDialog::process_done);
	out_win->proc->start();
	out_win->exec();
    }
    return 0;
}

static int
uniq_test(struct bu_vls *p, void *UNUSED(data))
{
    if (!p)
	return 1;
    if (bu_file_exists(bu_vls_cstr(p), NULL))
	return 0;
    return 1;
}

QString
QgGeomImport::gfile(const char *tfile)
{
    if (tfile) {
	fileName = QString(tfile);
    } else {
	const char *file_filters =
	    "BRL-CAD (*.g);;"
	    "BRL-CAD ASCII (*.asc);;"
	    "Rhino (*.3dm);;"
	    "STEP (*.stp *.step);;"
	    "All Files (*)";
	fileName = QFileDialog::getOpenFileName((QWidget *)this->parent(),
              "Open Geometry File",
              QCoreApplication::applicationDirPath(),
              file_filters,
              NULL,
              QFileDialog::DontUseNativeDialog);
    }

    if (fileName.isEmpty())
          return QString();

    QFileInfo fileinfo(fileName);

    // TODO - at least do a basic validity check... just checking for a .g
    // suffix is pretty weak...
    if (!fileinfo.suffix().compare("g", Qt::CaseSensitive))
       	return fileName;

    // If it's not a .g, check if the calling program wants us to
    // go any further first.
    if (!enable_conversion) {
	bu_vls_sprintf(&conv_msg, "%s does not appear to be a .g file\n", fileName.toLocal8Bit().data());
	return QString();
    }

    /* If we don't already have a filename that claims to be a .g file, see
     * about a conversion. No matter what we're doing, we'll want a target
     * .g filename. */
    QString g_path(fileinfo.path());
    g_path.append("/");
    g_path.append(fileinfo.baseName());

    /* Try to find a target name that doesn't already exist */
    struct bu_vls u_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&u_name, "%s.g", g_path.toLocal8Bit().data());
    if (bu_file_exists(bu_vls_cstr(&u_name), NULL)) {
	bu_vls_sprintf(&u_name, "%s0.g", g_path.toLocal8Bit().data());
	if (bu_vls_incr(&u_name, NULL, "4:0:0:0:_", &uniq_test, NULL) < 0) {
	    bu_vls_free(&u_name);
	    bu_vls_sprintf(&conv_msg, "unable to generate a conversion output name for %s\n", fileName.toLocal8Bit().data());
	    return QString();
	}
    }
    g_path = QString(bu_vls_cstr(&u_name));

    /* Also specify a logging filename */
    QString l_path(bu_vls_cstr(&u_name));
    l_path.append(".log");


    /* Now, we need to handle the format specific aspects of conversion options. */

    // ASC - BRL-CAD ASCII data
    if (!fileinfo.suffix().compare("asc", Qt::CaseSensitive)) {
	ASCImportDialog dialog(fileName, g_path, l_path);
	dialog.exec();
	g_path = dialog.db_path->text();
	l_path = dialog.log_path->text();
	exec_console_app_in_window(dialog.command(),dialog.options(), l_path);
	bu_vls_sprintf(&u_name, "%s", g_path.toLocal8Bit().data());
	if (!bu_file_exists(bu_vls_cstr(&u_name), NULL)) {
	    bu_vls_free(&u_name);
	    bu_vls_sprintf(&conv_msg, "ASC import failed for %s\n", fileName.toLocal8Bit().data());
	    return QString();
	}
    }

    // Rhino / 3DM
    if (!fileinfo.suffix().compare("3dm", Qt::CaseInsensitive)) {
	RhinoImportDialog dialog(fileName, g_path, l_path);
	dialog.exec();
	g_path = dialog.db_path->text();
	l_path = dialog.log_path->text();
	exec_console_app_in_window(dialog.command(), dialog.options(), l_path);
	bu_vls_sprintf(&u_name, "%s", g_path.toLocal8Bit().data());
	if (!bu_file_exists(bu_vls_cstr(&u_name), NULL)) {
	    bu_vls_free(&u_name);
	    bu_vls_sprintf(&conv_msg, "3DM import failed for %s\n", fileName.toLocal8Bit().data());
	    return QString();
	}
    }

    // STEP
    if (!fileinfo.suffix().compare("stp", Qt::CaseInsensitive) || !fileinfo.suffix().compare("step", Qt::CaseInsensitive)) {
	STEPImportDialog dialog(fileName, g_path, l_path);
	dialog.exec();
	g_path = dialog.db_path->text();
	l_path = dialog.log_path->text();
	exec_console_app_in_window(dialog.command(),dialog.options(), l_path);
	bu_vls_sprintf(&u_name, "%s", g_path.toLocal8Bit().data());
	if (!bu_file_exists(bu_vls_cstr(&u_name), NULL)) {
	    bu_vls_free(&u_name);
	    bu_vls_sprintf(&conv_msg, "STEP import failed for %s\n", fileName.toLocal8Bit().data());
	    return QString();
	}
    }

    bu_vls_free(&u_name);
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

