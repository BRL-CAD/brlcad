/*                     C A D I M P O R T . H
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
/** @file cadimport.h
 *
 * Import dialogs for specific geometry file format types
 *
 */

#ifndef CADIMPORT_H
#define CADIMPORT_H

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 6) && !defined(__clang__)
#  pragma message "Disabling GCC float equality comparison warnings via pragma due to Qt headers..."
#endif
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#undef Success
#include <QObject>
#undef Success
#include <QString>
#undef Success
#include <QStringList>
#undef Success
#include <QDialog>
#undef Success
#include <QLineEdit>
#undef Success
#include <QCheckBox>
#undef Success
#include <QGroupBox>
#undef Success
#include <QDialogButtonBox>
#undef Success
#include <QLabel>
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

class RhinoImportDialog : public QDialog
{
    Q_OBJECT

    public:
	RhinoImportDialog(QString filename);

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
	STEPImportDialog(QString filename);

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


#endif // CADIMPORT_H

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

