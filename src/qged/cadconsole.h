// -*- c++ -*-
/*=========================================================================

  CADConsole.h is based on ParaView code:

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

#ifndef QCADCONSOLE_H
#define QCADCONSOLE_H

#include <QWidget>
#include "console.h"

/**
   Qt widget that provides an interactive "shell" interface to a PV Blot
   interpreter.
*/

class CADConsole : public QWidget
{
  Q_OBJECT

public:
  CADConsole(QWidget* Parent);
  ~CADConsole();
  Console *console;

signals:
  /// Emitted whenever this widget starts or stops executing something.  The
  /// single argument is true when execution starts, false when it stops.
  void executing(bool);

public slots:
  virtual void initialize();
  virtual void executeCADCommand(const QString &command, ConsoleLog *results_log);

#if 0
  virtual void printStderr(const QString &text);
  virtual void printStdout(const QString &text);
  virtual void printMessage(const QString &text);
#endif

protected:

  QString FileName;

  virtual void promptForInput();

private:
  CADConsole(const CADConsole &);     // Not implemented
  void operator=(const CADConsole &);  // Not implemented
};

#endif //QCADCONSOLE_H
