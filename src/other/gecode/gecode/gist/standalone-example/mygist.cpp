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

#include <gecode/int.hh>
#include <gecode/gist/qtgist.hh>
#include <gecode/gist/mainwindow.hh>

using namespace Gecode;

/// Space for testing
class TestSpace : public Space {
public:
  IntVarArray x;
  TestSpace(void) : x(*this,2,0,2) {
    branch(*this, x, INT_VAR_NONE, INT_VAL_MIN);
  }
  TestSpace(bool share, TestSpace& t) : Space(share, t) {
    x.update(*this, share, t.x);
  }
  virtual Space* copy(bool share) { 
    return new TestSpace(share,*this); 
  }
  virtual void print(std::ostream& os) const {
    os << x << std::endl;
  }
};

int main(int argc, char** argv) {
  QApplication app(argc,argv);
  Gist::Print<TestSpace> p("My Gist");
  Gist::Options o;
  o.inspect.click(&p);

  Gist::GistMainWindow gist_mw(new TestSpace(), false, o);
  gist_mw.show();

  QMainWindow mw;
  Gist::Gist gist_widget(new TestSpace(), false, &mw, o);
  mw.setCentralWidget(&gist_widget);
  mw.resize(500,300);
  mw.show();

  return app.exec();
}
