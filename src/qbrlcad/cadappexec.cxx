/*                  C A D A P P E X E C . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "cadappexec.h"
#include <QFileInfo>
#include <QFile>
#include <QPlainTextEdit>
#include <QTextStream>

QDialog_App::QDialog_App(QWidget *pparent, QString executable, QStringList args, QString lfile) : QDialog(pparent)
{
    QVBoxLayout *dlayout = new QVBoxLayout;
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(process_abort()));
    console = new pqConsoleWidget(this);
    setLayout(dlayout);
    dlayout->addWidget(console);
    dlayout->addWidget(buttonBox);
    log = NULL;
    if (lfile.length() > 0) {
	log = new QFile(lfile);
	if (!log->open(QIODevice::Append | QIODevice::Text)) {
	    log = NULL;
	    return;
	}
	QTextStream log_stream(log);
	log_stream << executable << " " << args.join(" ") << "\n";
    }
}

void QDialog_App::read_stdout()
{
    QString std_output = proc->readAllStandardOutput();
    console->printString(std_output);
    if (log) {
	QTextStream log_stream(log);
	log_stream << std_output;
	log->flush();
    }
}

void QDialog_App::read_stderr()
{
    QString err_output = proc->readAllStandardError();
    console->printString(err_output);
    if (log) {
	QTextStream log_stream(log);
	log_stream << err_output;
	log->flush();
    }
}

void QDialog_App::process_abort()
{
    proc->kill();
    console->printString("\nAborted!\n");
    if (log) {
	QTextStream log_stream(log);
	log_stream << "\nAborted!\n";
    }
    process_done(0, QProcess::NormalExit);
}

void QDialog_App::process_done(int , QProcess::ExitStatus)
{
    if (log) log->close();
    buttonBox->clear();
    buttonBox->addButton(QDialogButtonBox::Ok);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
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

