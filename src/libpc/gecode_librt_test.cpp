/*            G E C O D E _ L I B R T _ T E S T . C P P
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
/** @file gecode_librt_test.cpp
 *
 * @brief Gecode version of BRL-CAD Geometry Constraints
 *
 */

#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <gecode/search.hh>

class PerpendicularSolve : public Gecode::Space {
    public:
	Gecode::IntVarArray l;
	PerpendicularSolve(int max, int tol) : l(*this, 6, -max, max) {
	    Gecode::IntVar AX(l[0]), AY(l[1]), AZ(l[2]), BX(l[3]), BY(l[4]), BZ(l[5]);
	    rel(*this, AX * BX + AY * BY + AZ * BZ > -1 * tol);
	    rel(*this, AX * BX + AY * BY + AZ * BZ < tol);
	}
	PerpendicularSolve(bool share, PerpendicularSolve& s) : Gecode::Space(share, s) {
	    l.update(*this, share, s.l);
	}
	virtual Space* copy(bool share) {return new PerpendicularSolve(share,*this);}
	void print(void) const {std::cout << l << std::endl;}
};

int main() {

  PerpendicularSolve* m = new PerpendicularSolve(100, 1);

  Gecode::IntVar AX(m->l[0]), AY(m->l[1]), AZ(m->l[2]), BX(m->l[3]), BY(m->l[4]), BZ(m->l[5]);

  /* Define some constraints to limit the solution set */
  Gecode::rel(*m, AX == BY);
  Gecode::rel(*m, AX < 50);
  Gecode::rel(*m, AY > 2);
  Gecode::rel(*m, AY < 40);
  Gecode::rel(*m, AZ == 10);
  Gecode::rel(*m, AX > 0);
  Gecode::rel(*m, AX*AX + AY*AY + AZ*AZ == BX*BX + BY*BY + BZ*BZ);
  Gecode::branch(*m, m->l, Gecode::INT_VAR_SIZE_MIN(), Gecode::INT_VAL_MIN());

  Gecode::DFS<PerpendicularSolve> e(m);
  delete m;
  while (PerpendicularSolve* s = e.next()) {
    s->print();
    delete s;
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
