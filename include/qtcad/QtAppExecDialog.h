/*                  Q T A P P E X E C D I A L O G . H
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
/** @file cadappexec.h
 *
 *  Support for running console programs and viewing the output stream
 *  in a dialog window.
 *
 */

#ifndef QTAPPEXECDIALOG_H
#define QTAPPEXECDIALOG_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QProcess>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextStream>

#include "qtcad/defines.h"
#include "qtcad/QtConsole.h"

class QTCAD_EXPORT QtAppExecDialog : public QDialog
{
    Q_OBJECT

    public:
	QtAppExecDialog(QWidget *pparent, QString executable, QStringList args, QString lfile = "");
	~QtAppExecDialog() {}

    public slots:
	void read_stdout();
	void read_stderr();
	void process_abort();
	void process_done(int, QProcess::ExitStatus);

    public:
	QFile *logfile;
        QtConsole *console;
        QProcess *proc;
        QDialogButtonBox *buttonBox;
};

#endif // QTAPPEXECDIALOG_H

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

