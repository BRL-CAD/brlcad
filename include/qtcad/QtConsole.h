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

#include "common.h"

#include <QCompleter>
#include <QWidget>
#include <QPlainTextEdit>
#include <QFont>
#include <map>

#include "ged.h"
#include "qtcad/defines.h"
#include "qtcad/QtConsoleListener.h"

class QTCAD_EXPORT QtConsoleWidgetCompleter : public QCompleter
{
public:
  /**
   * Update the completion model given a string.  The given string
   * is the current console text between the cursor and the start of
   * the line.
   */
  virtual void updateCompletionModel(const QString& str) = 0;
};

/**
  Qt widget that provides an interactive console - you can send text to the
  console by calling printString() and receive user input by connecting to the
  executeCommand() slot.

  Note:  Qt's signals and slots mechanism helps to make a robust widget for
  multithreaded inputs, since input strings to the console are coming in via
  signals and written by slots.  Users should not attempt to manipulate the
  underlying QPlainTextEdit widget directly.
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

  /// Set a completer for this console widget
  void setCompleter(QtConsoleWidgetCompleter* completer);

  QPoint getCursorPosition();

  // combine multiple history lines and replace them with the combination
  bool consolidateHistory(size_t start, size_t end);
  size_t historyCount();
  std::string historyAt(size_t ind);

  void listen(int fd, struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t c, void *d);
  std::map<std::pair<struct ged_subprocess *, int>, QConsoleListener *> listeners;

  // When inserting a completion, need a setting to control
  // how we handle slashes
  int split_slash = 0;

signals:
  /// Signal emitted whenever the user enters a command
  void executeCommand(const QString& cmd);

  /// Signal that there is queued, unprinted buffer output
  void queued_log(const QString& Text);

public slots:
  /// Writes the supplied text to the console
  void printString(const QString& Text);

  /// Writes the supplied text to the console
  void printStringBeforePrompt(const QString& Text);

  /// Clears a listener (called by the listener's finished() signal)
  void detach(struct ged_subprocess *p, int t);

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

  /// Inserts the given completion string at the cursor.  This will replace
  /// the current word that the cursor is touching with the given text.
  /// Determines the word using QTextCursor::StartOfWord, EndOfWord.
  void insertCompletion(const QString& text);

private:
  QtConsole(const QtConsole&);
  QtConsole& operator=(const QtConsole&);

  // Used by printStringBeforePrompt to queue up a timed repetition of printing
  // to catch anything recently added but not subsequently printed successfully
  // by an event based trigger.  Avoids output getting "stuck" in the buffer
  // waiting for an event.  Noticed when rt instances are lingering but not
  // closed and typing heavily on the command prompt - the last line or two of
  // rt output sometimes weren't printed under those conditions until the
  // window was closed.
  void emit_queued() {
     QString estring("");
     Q_EMIT queued_log(estring);
  }

  QString prompt_str;
  int prompt_start = 0;
  int64_t log_timestamp = 0;
  QString logbuf;

  void internalExecuteCommand(const QString& Command);

  class pqImplementation;
  pqImplementation* const Implementation;
  friend class pqImplementation;
};


#endif // !QTCAD_CONSOLE_H

