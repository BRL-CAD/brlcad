/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2006
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <QtGui>
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#endif

#include <iostream>
#include <gecode/gist/textoutput.hh>
#include <gecode/gist/gecodelogo.hh>

namespace Gecode { namespace Gist {

  /// \brief An outputstream that prints on a QTextEdit
  class GistOutputStream
  : public std::basic_ostream<char, std::char_traits<char> > {
    /// \brief Buffer for printing on a QTextEdit
    class Buf
    : public std::basic_streambuf<char, std::char_traits<char> > {
      QString buffer;
      QTextEdit* editor;
    public:
      void flush(void) {
        QTextBlockFormat bf = editor->textCursor().blockFormat();
        bf.setBottomMargin(0);
        editor->textCursor().setBlockFormat(bf);
        editor->append(buffer);
        buffer.clear();        
      }
      virtual int overflow(int v = std::char_traits<char>::eof()) {
        if (v == '\n') {
          flush();
        } else {
          buffer += (char)v;
        }
        return v;
      }
      Buf(QTextEdit* e) : editor(e) {}
    };

    Buf _buf;
  public:
    GistOutputStream(QTextEdit* editor)
    : std::basic_ostream<char, std::char_traits<char> >(&_buf),
      _buf(editor) {
      clear();
    }
    void flush(void) {
      _buf.flush();
    }
  };

  TextOutputI::TextOutputI(const std::string& name, QWidget *parent)
  : QMainWindow(parent) {
    Logos logos;

    QPixmap myPic;
    myPic.loadFromData(logos.gistLogo, logos.gistLogoSize);
    setWindowIcon(myPic);

    QFont font;
    QString fontFamily("Courier");
    font.setFamily(fontFamily);
    font.setFixedPitch(true);
    font.setPointSize(12);
    

    editor = new QTextEdit;
    editor->setFont(font);
    editor->setReadOnly(true);
    editor->setLineWrapMode(QTextEdit::FixedColumnWidth);
    editor->setLineWrapColumnOrWidth(80);
    os = new GistOutputStream(editor);

    QAction* clearText = new QAction("Clear", this);
    clearText->setShortcut(QKeySequence("Ctrl+K"));
    this->addAction(clearText);
    connect(clearText, SIGNAL(triggered()), editor,
                       SLOT(clear()));

    QAction* closeWindow = new QAction("Close window", this);
    closeWindow->setShortcut(QKeySequence("Ctrl+W"));
    this->addAction(closeWindow);
    connect(closeWindow, SIGNAL(triggered()), this,
                         SLOT(close()));

    QToolBar* t = addToolBar("Tools");
    t->setFloatable(false);
    t->setMovable(false);
    t->addAction(clearText);

    stayOnTop = new QAction("Stay on top", this);
    stayOnTop->setCheckable(true);
    t->addAction(stayOnTop);
    connect(stayOnTop, SIGNAL(changed()), this,
                         SLOT(changeStayOnTop()));

    changeStayOnTop();
    setCentralWidget(editor);
    setWindowTitle(QString((std::string("Gist Console: ") + name).c_str()));

    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(600,300);
  }

  TextOutputI::~TextOutputI(void) {
    delete os;
  }

  std::ostream&
  TextOutputI::getStream(void) {
    return *os;
  }

  void
  TextOutputI::flush(void) {
    static_cast<GistOutputStream*>(os)->flush();
  }

  void
  TextOutputI::insertHtml(const QString& s) {
    QTextBlockFormat bf = editor->textCursor().blockFormat();
    bf.setBottomMargin(0);
    editor->textCursor().setBlockFormat(bf);
    editor->insertHtml(s);
    editor->ensureCursorVisible();
  }

  void TextOutputI::changeStayOnTop(void) {
    QPoint p = pos();
    if (stayOnTop->isChecked()) {
      setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    } else {
      setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);      
    }
    move(p);
    show();
  }

}}

// STATISTICS: gist-any
