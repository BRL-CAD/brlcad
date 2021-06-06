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

 * This widget is based off of ParaView's QtConsole
 */

#include "common.h"

#include "qtcad/QtConsole.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMimeData>
#include <QPointer>
#include <QTextCursor>
#include <QTimer>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QScrollBar>

#include "bu.h"

/////////////////////////////////////////////////////////////////////////
// QtConsole::pqImplementation

class QtConsole::pqImplementation :
  public QPlainTextEdit
{
public:
  pqImplementation(QtConsole& p) :
    QPlainTextEdit(&p),
    Parent(p),
    InteractivePosition(documentEnd())
  {
    this->setTabChangesFocus(false);
    this->setAcceptDrops(false);
    this->setUndoRedoEnabled(false);
    this->setMinimumHeight(200);
    this->setMaximumBlockCount(10000);

    QFont f("Courier");
    f.setStyleHint(QFont::TypeWriter);
    f.setFixedPitch(true);
    this->setFont(f);

    this->CommandHistory.append("");
    this->CommandPosition = 0;
  }

  void setFont(const QFont& i_font)
  {
	  QPlainTextEdit::setFont(i_font);
  }

  QFont getFont()
  {
	  return this->font();
  }

  // Try to keep the scrollbar slider from getting too small to be usable
  void resizeEvent(QResizeEvent *e)
  {
     this->setMaximumBlockCount(2*this->height());
     QPlainTextEdit::resizeEvent(e);
  }

  void insertFromMimeData(const QMimeData * s)
  {
    QTextCursor text_cursor = this->textCursor();

    // Set to true if the cursor overlaps the history area
    const bool history_area =
      text_cursor.anchor() < this->InteractivePosition
      || text_cursor.position() < this->InteractivePosition;

    // Avoid pasting into history
    if (history_area) {
       return;
    }

    QPlainTextEdit::insertFromMimeData(s);

    // The text changed - make sure the command buffer knows
    this->updateCommandBuffer();
  }

  void keyPressEvent(QKeyEvent* e)
  {
    QTextCursor text_cursor = this->textCursor();

    // Set to true if there's a current selection
    const bool selection = text_cursor.anchor() != text_cursor.position();
    // Set to true if the cursor overlaps the history area
    const bool history_area =
      text_cursor.anchor() < this->InteractivePosition
      || text_cursor.position() < this->InteractivePosition;

    // Allow copying anywhere in the console ...
    if(e->key() == Qt::Key_C && e->modifiers() == Qt::ControlModifier)
      {
      if(selection)
        {
        this->copy();
        }

      e->accept();
      return;
      }

    // Allow cut only if the selection is limited to the interactive area ...
    if(e->key() == Qt::Key_X && e->modifiers() == Qt::ControlModifier)
      {
      if(selection && !history_area)
        {
        this->cut();
        }

      e->accept();
      return;
      }

    // Allow paste only if the selection is in the interactive area ...
    if(e->key() == Qt::Key_V && e->modifiers() == Qt::ControlModifier)
      {
      if(!history_area)
        {
        const QMimeData* const clipboard = QApplication::clipboard()->mimeData();
        const QString text = clipboard->text();
        if(!text.isNull())
          {
          text_cursor.insertText(text);
          this->updateCommandBuffer();
          }
        }

      e->accept();
      return;
      }

    // Force the cursor back to the interactive area
    if(history_area && e->key() != Qt::Key_Control)
      {
      text_cursor.setPosition(this->documentEnd());
      this->setTextCursor(text_cursor);
      }

    switch(e->key())
      {
      case Qt::Key_Up:
        e->accept();
        if (this->CommandPosition > 0)
          {
          this->replaceCommandBuffer(this->CommandHistory[--this->CommandPosition]);
          }
        break;

      case Qt::Key_Down:
        e->accept();
        if (this->CommandPosition < this->CommandHistory.size() - 2)
          {
          this->replaceCommandBuffer(this->CommandHistory[++this->CommandPosition]);
          }
        else
          {
          this->CommandPosition = this->CommandHistory.size()-1;
          this->replaceCommandBuffer("");
          }
        break;

      case Qt::Key_Left:
        if (text_cursor.position() > this->InteractivePosition)
          {
          QPlainTextEdit::keyPressEvent(e);
          }
        else
          {
          e->accept();
          }
        break;


      case Qt::Key_Delete:
        e->accept();
        QPlainTextEdit::keyPressEvent(e);
        this->updateCommandBuffer();
        break;

      case Qt::Key_Backspace:
        e->accept();
        if(text_cursor.position() > this->InteractivePosition)
          {
          QPlainTextEdit::keyPressEvent(e);
          this->updateCommandBuffer();
          //this->updateCompleterIfVisible();
          }
        break;

      case Qt::Key_Tab:
        e->accept();
        //this->updateCompleter();
        //this->selectCompletion();
        break;


      case Qt::Key_Home:
        e->accept();
        text_cursor.setPosition(this->InteractivePosition);
        this->setTextCursor(text_cursor);
        break;

      case Qt::Key_Return:
      case Qt::Key_Enter:
        e->accept();

        text_cursor.setPosition(this->documentEnd());
        this->setTextCursor(text_cursor);

        this->internalExecuteCommand();
        break;

      default:
        e->accept();
        QPlainTextEdit::keyPressEvent(e);
        this->updateCommandBuffer();
        //this->updateCompleterIfVisible();
        break;
      }
  }

  /// Returns the end of the document
  int documentEnd()
  {
    QTextCursor c(this->document());
    c.movePosition(QTextCursor::End);
    return c.position();
  }

  void focusOutEvent(QFocusEvent *e)
  {
    QPlainTextEdit::focusOutEvent(e);

    // For some reason the QCompleter tries to set the focus policy to
    // NoFocus, set let's make sure we set it back to the default WheelFocus.
    this->setFocusPolicy(Qt::WheelFocus);
  }

  /// Update the contents of the command buffer from the contents of the widget
  void updateCommandBuffer()
  {
    this->commandBuffer() = this->toPlainText().mid(this->InteractivePosition);
  }

  /// Replace the contents of the command buffer, updating the display
  void replaceCommandBuffer(const QString& Text)
  {
    this->commandBuffer() = Text;

    QTextCursor c(this->document());
    c.setPosition(this->InteractivePosition);
    c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    c.insertText(Text);
  }

  /// References the buffer where the current un-executed command is stored
  QString& commandBuffer()
  {
    return this->CommandHistory.back();
  }

  /// Implements command-execution
  void internalExecuteCommand()
    {
    // First update the history cache. It's essential to update the
    // this->CommandPosition before calling internalExecuteCommand() since that
    // can result in a clearing of the current command (BUG #8765).
    QString command = this->commandBuffer();
    if (!command.isEmpty()) // Don't store empty commands in the history
      {
      this->CommandHistory.push_back("");
      this->CommandPosition = this->CommandHistory.size() - 1;
      }
    QTextCursor c(this->document());
    c.movePosition(QTextCursor::End);
    c.insertText("\n");

    this->InteractivePosition = this->documentEnd();
    this->Parent.internalExecuteCommand(command);
    }

  /// Stores a back-reference to our owner
  QtConsole& Parent;

  /** Stores the beginning of the area of interactive input, outside which
  changes can't be made to the text edit contents */
  int InteractivePosition;
  /// Stores command-history, plus the current command buffer
  QStringList CommandHistory;
  /// Stores the current position in the command-history
  int CommandPosition;
};

/////////////////////////////////////////////////////////////////////////
// QtConsole

QtConsole::QtConsole(QWidget* Parent) :
  QWidget(Parent),
  Implementation(new pqImplementation(*this))
{
  QVBoxLayout* const l = new QVBoxLayout(this);
  l->setMargin(0);
  l->addWidget(this->Implementation);
  QObject::connect(this, &QtConsole::queued_log, this, &QtConsole::printStringBeforePrompt);
}

//-----------------------------------------------------------------------------
QtConsole::~QtConsole()
{
  delete this->Implementation;
}

//-----------------------------------------------------------------------------
QFont QtConsole::getFont()
{
  return this->Implementation->getFont();
}

//-----------------------------------------------------------------------------
void QtConsole::setFont(const QFont& i_font)
{
  this->Implementation->setFont(i_font);
}

//-----------------------------------------------------------------------------
QPoint QtConsole::getCursorPosition()
{
  QTextCursor tc = this->Implementation->textCursor();

  return this->Implementation->cursorRect(tc).topLeft();
}

//-----------------------------------------------------------------------------
void QtConsole::listen(int fd, struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t c, void *d)
{
  QConsoleListener *l = new QConsoleListener(fd, p, t, c, d);
  bu_log("Start listening: %d\n", (int)t);
  QObject::connect(l, &QConsoleListener::newLine, this, &QtConsole::printStringBeforePrompt);
  QObject::connect(l, &QConsoleListener::is_finished, this, &QtConsole::detach);
  listeners[std::make_pair(p, t)] = l;
}
void QtConsole::detach(struct ged_subprocess *p, int t)
{
  std::map<std::pair<struct ged_subprocess *, int>, QConsoleListener *>::iterator l_it, si_it, so_it, e_it;
  l_it = listeners.find(std::make_pair(p,t));

  struct ged_subprocess *process = NULL;
  ged_io_func_t callback = NULL;
  void *gdata = NULL;

  if (l_it != listeners.end()) {
     bu_log("Stop listening: %d\n", (int)t);
     QConsoleListener *l = l_it->second;
     process = l->process;
     callback = l->callback;
     gdata = l->data;
     listeners.erase(l_it);
     delete l;
  }

  if (process) {
     si_it = listeners.find(std::make_pair(p,(int)BU_PROCESS_STDIN));
     so_it = listeners.find(std::make_pair(p,(int)BU_PROCESS_STDOUT));
      e_it = listeners.find(std::make_pair(p,(int)BU_PROCESS_STDERR));

     // We don't want to destroy the process until all the listeners are removed.
     // If they all have been, do a final callback call with -1 key to instruct
     // the callback to finalize process and memory removal.
     if (si_it == listeners.end() && so_it == listeners.end() && e_it == listeners.end() && callback) {
        (*callback)(gdata, -1);
	// This is also the point at which we know any output from the subprocess
	// is at an end.  Finalize the before-prompt printing.
	log_timestamp = 0;
	QString estr("");
	printStringBeforePrompt(estr);
     }
  }
}

//-----------------------------------------------------------------------------
void QtConsole::printString(const QString& Text)
{
  QTextCursor text_cursor = this->Implementation->textCursor();
  text_cursor.setPosition(this->Implementation->documentEnd());
  this->Implementation->setTextCursor(text_cursor);
  text_cursor.insertText(Text);

  this->Implementation->InteractivePosition = this->Implementation->documentEnd();
  this->Implementation->ensureCursorVisible();
}

//-----------------------------------------------------------------------------
// This style of insertion is is too slow to just attempt it every time the
// subprocesses send output (Goliath rtcheck is an example.)  Instead, we
// keep track of the last time we updated and if not enough time has passed
// we just queue up the text for later insertion.
//
// This approach also depends on the listener clean-up finalizing the string -
// otherwise there would be a chance of losing some printing due to timing
// issues if the last bits of the output come in right after an insertion.
//
// TODO: It would be better if the input prompt and command were one
// QPlainTextEdit and the output another, so they could operate independently -
// this approach will still introduce brief periods where the input prompt
// disappears and reappears during updating.  However, that would
// require restructuring QtConsole's widget design - for now this functions,
// and if this becomes the production solution we can/should revisit it later.
void QtConsole::printStringBeforePrompt(const QString& Text)
{
  logbuf.append(Text);
  int64_t ctime = bu_gettime();
  double elapsed = ((double)ctime - (double)log_timestamp)/1000000.0;
  if (elapsed > 0.1 && logbuf.length()) {
     // Make a local printing copy and clear the buffer
     QString llogbuf = logbuf;
     logbuf.clear();

     log_timestamp = bu_gettime();

     // While we're manipulating the console contents, we don't want
     // the user modifying anything.
     this->Implementation->setReadOnly(true);

     // Make a copy of the text cursor on which to operate.  Note that this
     // is not the actual cursor being used for editing and we need to use
     // setTextCursor to impact that cursor.  For most of this operation we
     // don't need to, but it's important for restoring the editing state
     // to the user at the end.
     QTextCursor tc = this->Implementation->textCursor();

     // Store the current editing point relative to the prompt (it may not
     // be the end of the command, if the user is editing mid-command.)
     int curr_pos_offset = tc.position() - (prompt_start + prompt_str.length());

     // Before appending new content to the log, clear the old prompt and
     // command string.  We restore them after the new log content is added.
     tc.setPosition(prompt_start);
     tc.setPosition(this->Implementation->documentEnd(), QTextCursor::KeepAnchor);
     tc.removeSelectedText();

     // Print the accumulated logged output to the command window.
     this->Implementation->insertPlainText(llogbuf);

     // Before we re-add the prompt and command buffer, store the new prompt
     // starting point for use in the next write cycle.
     prompt_start = tc.position();

     // Restore the prompt and the next command (if any)
     this->Implementation->insertPlainText(prompt_str);
     this->Implementation->textCursor().insertText(this->Implementation->commandBuffer());

     // Denote the interactive portion of the new state of the buffer as starting after
     // the new prompt.
     this->Implementation->InteractivePosition = prompt_start + prompt_str.length();

     // Make sure the scrolling is set to view the new prompt
     this->Implementation->ensureCursorVisible();

     // The final touch - in case the cursor was not at the end of the command buffer,
     // restore it to its prior offset from the prompt.  Since we are altering the
     // user-interactive editing cursor this time, and not just manipulating the text,
     // we must use setTextCursor to apply the change.
     tc.setPosition(prompt_start + prompt_str.length() + curr_pos_offset);
     this->Implementation->setTextCursor(tc);

     // All done - unlock
     this->Implementation->setReadOnly(false);
  }

  // If there is anything queued up, we need to make sure we print it soon(ish)
  if (logbuf.length()) {
      QTimer::singleShot(1000, this, &QtConsole::emit_queued);
  }
}

//-----------------------------------------------------------------------------
void QtConsole::printCommand(const QString& cmd)
{
  this->Implementation->textCursor().insertText(cmd);
  this->Implementation->updateCommandBuffer();
}

//-----------------------------------------------------------------------------
void QtConsole::prompt(const QString& text)
{
  QTextCursor text_cursor = this->Implementation->textCursor();

  // if the cursor is currently on a clean line, do nothing, otherwise we move
  // the cursor to a new line before showing the prompt.
  text_cursor.movePosition(QTextCursor::StartOfLine);
  int startpos = text_cursor.position();
  text_cursor.movePosition(QTextCursor::EndOfLine);
  int endpos = text_cursor.position();
  if (endpos != startpos)
    {
    this->Implementation->textCursor().insertText("\n");
    }

  prompt_start = text_cursor.position();
  prompt_str = text;

  this->Implementation->textCursor().insertText(text);
  this->Implementation->InteractivePosition = this->Implementation->documentEnd();
  this->Implementation->ensureCursorVisible();
}

//-----------------------------------------------------------------------------
void QtConsole::clear()
{
  this->Implementation->clear();

  // For some reason the QCompleter tries to set the focus policy to
  // NoFocus, set let's make sure we set it back to the default WheelFocus.
  this->Implementation->setFocusPolicy(Qt::WheelFocus);
}

//-----------------------------------------------------------------------------
void QtConsole::internalExecuteCommand(const QString& Command)
{
  emit this->executeCommand(Command);
}

//-----------------------------------------------------------------------------
