/*            G E C O D E _ S O L V E R _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file gecode_solver_test.cpp
 *
 * @brief Gecode version of Simple Test cases for Constraint Solver
 *
 */

/*  Based on send-more-money-de-mystified.cpp
 *
 *  Authors:
 *    Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *    Christian Schulte, 2008-2013
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software, to deal in the software without restriction,
 *  including without limitation the rights to use, copy, modify, merge,
 *  publish, distribute, sublicense, and/or sell copies of the software,
 *  and to permit persons to whom the software is furnished to do so, subject
 *  to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "common.h"

#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <gecode/search.hh>

#include "bu/app.h"

class EqSolve : public Gecode::Space {
    public:
	Gecode::IntVarArray l;
	EqSolve(int v1, int v2, int v3) : l(*this, v1, v2, v3) {}
	EqSolve(bool share, EqSolve& s) : Gecode::Space(share, s) {l.update(*this, share, s.l);}
	virtual Space* copy(bool share) {return new EqSolve(share,*this);}
	void print(void) const {std::cout << l << std::endl;}
};

int main(int argc, const char *argv[]) {

  bu_setprogname(argv[0]);

  EqSolve* m = new EqSolve(4, 0, 10);

  Gecode::IntVar A(m->l[0]), B(m->l[1]), C(m->l[2]), D(m->l[3]);

  /* Define the constraints */

  Gecode::rel(*m, A * B == 12);
  Gecode::rel(*m, B + C < 5);
  Gecode::rel(*m, A - D == 2);
  Gecode::rel(*m, A * C == 4);
  Gecode::branch(*m, m->l, Gecode::INT_VAR_SIZE_MIN(), Gecode::INT_VAL_MIN());

  Gecode::DFS<EqSolve> e(m);
  delete m;
  while (EqSolve* s = e.next()) {
    s->print(); delete s;
  }
  return 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
