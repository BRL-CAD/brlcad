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

#include <gecode/gist.hh>
#include <gecode/gist/textoutput.hh>
#include <gecode/gist/mainwindow.hh>
#include <gecode/gist/stopbrancher.hh>

namespace Gecode { namespace Gist {

  std::string
  Inspector::name(void) { return "Inspector"; }
  
  void
  Inspector::finalize(void) {}
  
  Inspector::~Inspector(void) {}

  std::string
  Comparator::name(void) { return "Comparator"; }
  
  void
  Comparator::finalize(void) {}
  
  Comparator::~Comparator(void) {}
    
  TextOutput::TextOutput(const std::string& name)
    : t(NULL), n(name) {}
  
  void
  TextOutput::finalize(void) {
    delete t;
    t = NULL;
  }
  
  TextOutput::~TextOutput(void) {
    delete t;
  }

  std::string
  TextOutput::name(void) { return n; }
  
  void
  TextOutput::init(void) {
    if (t == NULL) {
      t = new TextOutputI(n);
    }
    t->setVisible(true);
  }

  std::ostream&
  TextOutput::getStream(void) {
    return t->getStream();
  }
  
  void
  TextOutput::flush(void) {
    t->flush();
  }
  
  void
  TextOutput::addHtml(const char* s) {
    t->insertHtml(s);
  }

  const Options Options::def;
  
  int
  explore(Space* root, bool bab, const Options& opt) {
    char argv0='\0'; char* argv1=&argv0;
    int argc=0;
    QApplication app(argc, &argv1);
    GistMainWindow mw(root, bab, opt);
    return app.exec();
  }

  void
  stopBranch(Space& home) {
    StopBrancher::post(home);
  }

}}

// STATISTICS: gist-any
