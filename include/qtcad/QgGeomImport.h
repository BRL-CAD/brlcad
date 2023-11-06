/*                   Q G G E O M I M P O R T . H
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
/** @file cadappexec.h
 *
 * Encapsulates a graphical process for getting a path to a .g file to
 * open, wrapping patterns for graphical configuration and in-dialogue
 * running of converter processes into a reusable package.
 *
 * The final result is a QString with the path to the .g file suitable for
 * opening with libged's "open" command, or an empty string if no such .g file
 * ends up being available.  For 3dm, STEP, etc. files that path will be to the
 * output of the converter, but if the original specified file is already a.g
 * file this will function like a normal file open dialog from Qt.
 */

#ifndef QGGEOMIMPORT_H
#define QGGEOMIMPORT_H

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QString>
#include <QStringList>

#include "qtcad/defines.h"

class QTCAD_EXPORT QgGeomImport : public QObject
{
    Q_OBJECT

    public:
	QgGeomImport(QWidget *pparent = NULL);
	~QgGeomImport();

	QString gfile(const char *tfile = NULL);

	bool enable_conversion = true;

	struct bu_vls conv_msg = BU_VLS_INIT_ZERO;

    private:
	int exec_console_app_in_window(QString command, QStringList options, QString lfile);
	QString fileName;
};

// GUI dialogues for specific format conversion options

class ASCImportDialog : public QDialog
{
    Q_OBJECT

    public:
	ASCImportDialog(QString filename, QString g_path, QString l_path);

	QString command();
	QStringList options();
	QLineEdit *db_path;
	QLineEdit *log_path;

    private:

	QString input_file;
	QGroupBox *formGroupBox;
	QDialogButtonBox *buttonBox;
};

class RhinoImportDialog : public QDialog
{
    Q_OBJECT

    public:
	RhinoImportDialog(QString filename, QString g_path, QString l_path);

	QString command();
	QStringList options();
	QLineEdit *db_path;
	QLineEdit *log_path;

    private:

	QString input_file;

	QLineEdit *scaling_factor;
	QLineEdit *tolerance;
	QLineEdit *verbosity;
	QCheckBox *debug_printing;
	QCheckBox *random_colors;
	QCheckBox *uuid;


	QGroupBox *formGroupBox;
	QDialogButtonBox *buttonBox;
};

class STEPImportDialog : public QDialog
{
    Q_OBJECT

    public:
	STEPImportDialog(QString filename, QString g_path, QString l_path);

	QString command();
	QStringList options();
	QLineEdit *db_path;
	QLineEdit *log_path;

    private:

	QString input_file;

	QCheckBox *verbosity;

	QGroupBox *formGroupBox;
	QDialogButtonBox *buttonBox;
};

#endif // QGGEOMIMPORT_H

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

