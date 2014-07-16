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

class GeometrySolve : public Gecode::Space {
    public:
	Gecode::IntVarArray l;
	GeometrySolve(int pnt_cnt, int min, int max) : l(*this, pnt_cnt, min, max) {}
	GeometrySolve(bool share, GeometrySolve& s) : Gecode::Space(share, s) {
	    l.update(*this, share, s.l);
	}
	virtual Space* copy(bool share) {return new GeometrySolve(share,*this);}
	void print(void) const {std::cout << l << std::endl;}
};

void
add_constraint_coincident_pnt(GeometrySolve *s, int tol, int p1, int p2)
{
    Gecode::IntVar AX(s->l[0 + 3*p1]), AY(s->l[1 + 3*p1]), AZ(s->l[2 + 3*p1]);
    Gecode::IntVar BX(s->l[0 + 3*p2]), BY(s->l[1 + 3*p2]), BZ(s->l[2 + 3*p2]);
    Gecode::rel(*s, AX - BX > -1 * tol);
    Gecode::rel(*s, AX - BX < tol);
    Gecode::rel(*s, AY - BY > -1 * tol);
    Gecode::rel(*s, AY - BY < tol);
    Gecode::rel(*s, AZ - BZ > -1 * tol);
    Gecode::rel(*s, AZ - BZ < tol);
}

void
add_constraint_perpendicular_vect(GeometrySolve *s, int tol, int p1, int p2)
{
    Gecode::IntVar AX(s->l[0 + 3*p1]), AY(s->l[1 + 3*p1]), AZ(s->l[2 + 3*p1]);
    Gecode::IntVar BX(s->l[0 + 3*p2]), BY(s->l[1 + 3*p2]), BZ(s->l[2 + 3*p2]);
    Gecode::rel(*s, AX * BX + AY * BY + AZ * BZ > -1 * tol);
    Gecode::rel(*s, AX * BX + AY * BY + AZ * BZ < tol);
}

int main() {

    /* Perpendicular constraint test */
    {
	GeometrySolve* s = new GeometrySolve(6, -100, 100);
	Gecode::IntVar AX(s->l[0]), AY(s->l[1]), AZ(s->l[2]), BX(s->l[3]), BY(s->l[4]), BZ(s->l[5]);

	/* Add a canned constraint */
	add_constraint_perpendicular_vect(s, 1, 0, 1);

	/* Define some additional constraints to limit the solution set */
	Gecode::rel(*s, AX == BY);
	Gecode::rel(*s, AX < 50);
	Gecode::rel(*s, AY > 2);
	Gecode::rel(*s, AY < 40);
	Gecode::rel(*s, AZ == 10);
	Gecode::rel(*s, AX > 0);
	Gecode::rel(*s, AX*AX + AY*AY + AZ*AZ == BX*BX + BY*BY + BZ*BZ);
	Gecode::branch(*s, s->l, Gecode::INT_VAR_SIZE_MIN(), Gecode::INT_VAL_MIN());

	Gecode::DFS<GeometrySolve> e(s);
	delete s;
	while (GeometrySolve* sp = e.next()) {
	    sp->print();
	    delete sp;
	}

    }

    /* Coincident point test */
    {
	GeometrySolve* s = new GeometrySolve(6, -10, 10);
	Gecode::IntVar AX(s->l[0]), AY(s->l[1]), AZ(s->l[2]), BX(s->l[3]), BY(s->l[4]), BZ(s->l[5]);

	/* Add a canned constraint */
	add_constraint_coincident_pnt(s, 2, 0, 1);

	/* Define some constraints to limit the solution set */
	Gecode::rel(*s, AX == BY);
	Gecode::rel(*s, AY == 2);
	Gecode::rel(*s, AX > 0);
	Gecode::rel(*s, BX > 0);
	Gecode::rel(*s, BZ > 0);
	Gecode::rel(*s, AX < 3);
	Gecode::rel(*s, BX < 5);
	Gecode::rel(*s, BZ < 6);
	Gecode::branch(*s, s->l, Gecode::INT_VAR_SIZE_MIN(), Gecode::INT_VAL_MIN());

	Gecode::DFS<GeometrySolve> e(s);
	delete s;
	while (GeometrySolve* sp = e.next()) {
	    sp->print();
	    delete sp;
	}
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
