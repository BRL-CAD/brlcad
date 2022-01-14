/*                  Q T A P P E X E C D I A L O G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file QtAppExecDialog.cpp
 *
 * Support for running external applications and viewing the output
 * stream in a console widget embedded in a dialog window.
 *
 */

#include <QFileInfo>
#include <QFile>
#include <QPlainTextEdit>
#include <QTextStream>
#include "qtcad/QtAppExecDialog.h"

QtAppExecDialog::QtAppExecDialog(QWidget *pparent, QString executable, QStringList args, QString lfile) : QDialog(pparent)
{
    QVBoxLayout *dlayout = new QVBoxLayout;
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QtAppExecDialog::process_abort);
    console = new QtConsole(this);
    console->prompt("");
    setLayout(dlayout);
    dlayout->addWidget(console);
    dlayout->addWidget(buttonBox);
    logfile = NULL;
    if (lfile.length() > 0) {
	logfile = new QFile(lfile);
	if (!logfile->open(QIODevice::Append | QIODevice::Text)) {
	    logfile = NULL;
	    return;
	}
	QTextStream log_stream(logfile);
	log_stream << executable << " " << args.join(" ") << "\n";
    }
}

void QtAppExecDialog::read_stdout()
{
    QString std_output = proc->readAllStandardOutput();
    console->printString(std_output);
    if (logfile) {
	QTextStream log_stream(logfile);
	log_stream << std_output;
	logfile->flush();
    }
}

void QtAppExecDialog::read_stderr()
{
    QString err_output = proc->readAllStandardError();
    console->printString(err_output);
    if (logfile) {
	QTextStream log_stream(logfile);
	log_stream << err_output;
	logfile->flush();
    }
}

void QtAppExecDialog::process_abort()
{
    proc->kill();
    console->printString("\nAborted!\n");
    if (logfile) {
	QTextStream log_stream(logfile);
	log_stream << "\nAborted!\n";
	logfile->flush();
    }
    process_done(0, QProcess::NormalExit);
}

void QtAppExecDialog::process_done(int , QProcess::ExitStatus)
{
    if (logfile) logfile->close();
    buttonBox->clear();
    buttonBox->addButton(QDialogButtonBox::Ok);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QtAppExecDialog::accept);
    setWindowTitle("Process Finished");
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

