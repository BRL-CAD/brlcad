/*                  C A D A P P E X E C . C X X
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
/** @file cadappexec.cxx
 *
 * Support for running external applications and viewing the results.
 *
 */

#include <QFileInfo>
#include <QFile>
#include <QPlainTextEdit>
#include <QTextStream>
#include "cadappexec.h"

QDialog_App::QDialog_App(QWidget *pparent, QString executable, QStringList args, QString lfile) : QDialog(pparent)
{
    QVBoxLayout *dlayout = new QVBoxLayout;
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog_App::process_abort);
    console = new pqConsoleWidget(this);
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

void QDialog_App::read_stdout()
{
    QString std_output = proc->readAllStandardOutput();
    console->printString(std_output);
    if (logfile) {
	QTextStream log_stream(logfile);
	log_stream << std_output;
	logfile->flush();
    }
}

void QDialog_App::read_stderr()
{
    QString err_output = proc->readAllStandardError();
    console->printString(err_output);
    if (logfile) {
	QTextStream log_stream(logfile);
	log_stream << err_output;
	logfile->flush();
    }
}

void QDialog_App::process_abort()
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

void QDialog_App::process_done(int , QProcess::ExitStatus)
{
    if (logfile) logfile->close();
    buttonBox->clear();
    buttonBox->addButton(QDialogButtonBox::Ok);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog_App::accept);
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

