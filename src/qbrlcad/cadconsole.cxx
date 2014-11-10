// -*- c++ -*-
/*=========================================================================

  CADConsole.cxx is based on ParaView code:

  Copyright (c)1993-2013 Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
met:

 * Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.

 * Neither name of Ken Martin, Will Schroeder, or Bill Lorensen nor the
 names of any contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notice for more information.

 =========================================================================*/
/*-------------------------------------------------------------------------
  Copyright 2009 Sandia Corporation.
  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
  the U.S. Government retains certain rights in this software.
  -------------------------------------------------------------------------*/

#include "console.h"
#include "cadconsole.h"
#include <QVBoxLayout>
#include <QApplication>
#include "cadapp.h"

//=============================================================================
CADConsole::CADConsole(QWidget *Parent) :
    QWidget(Parent)
{
    this->console = new Console(this);
    QVBoxLayout *const l = new QVBoxLayout(this);
    l->setMargin(0);
    l->addWidget(this->console);

    QString newPrompt = "cad> ";
    this->console->prompt(newPrompt);

    QObject::connect(this->console, SIGNAL(executeCommand(const QString &, ConsoleLog *)),
	    this, SLOT(executeCADCommand(const QString &, ConsoleLog *)));

}

CADConsole::~CADConsole()
{
    delete this->console;
}


void CADConsole::initialize()
{
    this->initialize();

    // TODO - will probably need a lot of setup here...

    this->promptForInput();
}

//-----------------------------------------------------------------------------
void CADConsole::executeCADCommand(const QString &command, ConsoleLog *results_log)
{
    QString cmd = command;
    QString result;
    result.clear();
    QString subcmd = cmd.left(6);
    const char *substr = subcmd.toLocal8Bit().data();

    if (!substr) exit(0);

    if (command == "quit" || command == "q" || command == "exit") {
	((CADApp *)qApp)->closedb();
	qApp->quit();
	/*
    } else if (command.length() > 7 && !command.left(6).compare("opendb")) {
	QString filestr = command.right(command.length() - 7).trimmed();
	QString resultnum;
	int odbrt = current_obj.open_db(filestr);
	if (odbrt) {
	    result.append("Error opening ");
	    result.append(filestr);
	    result.append(": ");
	    result.append(resultnum.setNum(odbrt));
	}
	*/
    } else {
	((CADApp *)qApp)->exec_command(&cmd, &result);
    }
    results_log->append_results(result);

    //this->promptForInput();
}
#if 0
//-----------------------------------------------------------------------------
void CADConsole::printStderr(const QString &text)
{
    this->console->append_results(text);
}

void CADConsole::printStdout(const QString &text)
{
    this->console->append_results(text);
}

void CADConsole::printMessage(const QString &text)
{
    this->console->append_results(text);
}
#endif

//-----------------------------------------------------------------------------
void CADConsole::promptForInput()
{
    QString newPrompt = "cad> ";
    this->console->prompt(newPrompt);
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

