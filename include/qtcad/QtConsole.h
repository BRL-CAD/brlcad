/*
 * Copyright (c) 2005-2008 Sandia Corporation, Kitware Inc.
   All rights reserved.
 *
 * Sandia National Laboratories, New Mexico PO Box 5800 Albuquerque, NM 87185
 *
 * Kitware Inc.
 * 28 Corporate Drive
 * Clifton Park, NY 12065
 * USA
 *
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive license
 * for use of this work by or on behalf of the U.S. Government.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of Kitware nor the names of any contributors may be used
 *      to endorse or promote products derived from this software without specific
 *      prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 * This widget is based off of ParaView's pqConsoleWidget
 */

#ifndef QTCAD_CONSOLE_H
#define QTCAD_CONSOLE_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QFont>

#include "ged.h"
#include "qtcad/defines.h"
#include "qtcad/QtConsoleListener.h"

/**
  Qt widget that provides an interactive console - you can send text to the
  console by calling printString() and receive user input by connecting to the
  executeCommand() slot.
*/
class QTCAD_EXPORT QtConsole : public QWidget
{
  Q_OBJECT

public:
  QtConsole(QWidget* Parent);
  virtual ~QtConsole();

  /// Returns the current font
  QFont getFont();
  /// Sets current font
  void setFont(const QFont &font);

  QPoint getCursorPosition();

  void listen(int *fd, struct ged_subprocess *p);
  void detach();

signals:
  /// Signal emitted whenever the user enters a command
  void executeCommand(const QString& cmd);

public slots:
  /// Writes the supplied text to the console
  void printString(const QString& Text);

  /// Updates the current command. Unlike printString, this will affect the
  /// current command being typed.
  void printCommand(const QString& cmd);

  /// Clears the contents of the console
  void clear();

  /// Puts out an input accepting prompt.
  /// It is recommended that one uses prompt instead of printString() to print
  /// an input prompt since this call ensures that the prompt is shown on a new
  /// line.
  void prompt(const QString& text);

private:
  QtConsole(const QtConsole&);
  QtConsole& operator=(const QtConsole&);

  QConsoleListener *listener;

  void internalExecuteCommand(const QString& Command);

  class pqImplementation;
  pqImplementation* const Implementation;
  friend class pqImplementation;
};

#endif // !QTCAD_CONSOLE_H

