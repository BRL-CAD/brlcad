/*            G E C O D E _ L I B R T _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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

#include "common.h"

#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <gecode/search.hh>

#include "bu/app.h"

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

void
add_constraint_pnt_on_line_segment(GeometrySolve *s, int tol, int p, int s1, int s2)
{
    Gecode::IntVar PX(s->l[0 + 3*p]),  PY(s->l[1 + 3*p]),  PZ(s->l[2 + 3*p]);
    Gecode::IntVar S1X(s->l[0 + 3*s1]), S1Y(s->l[1 + 3*s1]), S1Z(s->l[2 + 3*s1]);
    Gecode::IntVar S2X(s->l[0 + 3*s2]), S2Y(s->l[1 + 3*s2]), S2Z(s->l[2 + 3*s2]);

    /* P, S1 and S2 must be colinear: (S2 - S1) X (P - S1) == 0 */
    Gecode::rel(*s, (S2Y - S1Y) * (PZ - S1Z) - (S2Z - S1Z) * (PY - S1Y) > -1 * tol);
    Gecode::rel(*s, (S2Z - S1Z) * (PX - S1X) - (S2X - S1X) * (PZ - S1Z) > -1 * tol);
    Gecode::rel(*s, (S2X - S1X) * (PY - S1Y) - (S2Y - S1Y) * (PX - S1X) > -1 * tol);
    Gecode::rel(*s, (S2Y - S1Y) * (PZ - S1Z) - (S2Z - S1Z) * (PY - S1Y) < tol);
    Gecode::rel(*s, (S2Z - S1Z) * (PX - S1X) - (S2X - S1X) * (PZ - S1Z) < tol);
    Gecode::rel(*s, (S2X - S1X) * (PY - S1Y) - (S2Y - S1Y) * (PX - S1X) < tol);

    /* In addition to point colinearity, to check that P is between S1 and S2
     * check the dot products: */
    /* (S2 - S1) . (P - S1) >= 0 */
    /* (S2 - S1) . (P - S1) <= (S2 - S1) . (S2 - S1) */
    Gecode::rel(*s, (S2X - S1X) * (PX - S1X) +  (S2Y - S1Y) * (PY - S1Y) + (S2Z - S1Z) * (PZ - S1Z) >= 0);
    Gecode::rel(*s, (S2X - S1X) * (PX - S1X) +  (S2Y - S1Y) * (PY - S1Y) + (S2Z - S1Z) * (PZ - S1Z) <= (S2X - S1X) * (S2X - S1X) +  (S2Y - S1Y) * (S2Y - S1Y) + (S2Z - S1Z) * (S2Z - S1Z) );

}

int main(int argc, const char *argv[]) {

    bu_setprogname(argv[0]);

    /* Perpendicular constraint test */
    {
	int cnt = 0;
	GeometrySolve* s = new GeometrySolve(6, -20, 20);
	Gecode::IntVar AX(s->l[0]), AY(s->l[1]), AZ(s->l[2]), BX(s->l[3]), BY(s->l[4]), BZ(s->l[5]);

	/* Add a canned constraint */
	add_constraint_perpendicular_vect(s, 1, 0, 1);

	/* Define some additional constraints to limit the solution set */
	Gecode::rel(*s, AX > 10);
	Gecode::rel(*s, AY == AX);
	Gecode::rel(*s, AX*AX + AY*AY + AZ*AZ == BX*BX + BY*BY + BZ*BZ);
	Gecode::branch(*s, s->l, Gecode::INT_VAR_SIZE_MIN(), Gecode::INT_VAL_MIN());

	Gecode::DFS<GeometrySolve> e(s);
	delete s;
	while (GeometrySolve* sp = e.next()) {
	    //sp->print();
	    cnt++;
/*	    std::cout << "in rcc_" << cnt << "_1.s rcc 0 0 0 " << sp->l[0] << " "<< sp->l[1] << " "<< sp->l[2] << " 1\n";
	    std::cout << "in rcc_" << cnt << "_2.s rcc 0 0 0 " << sp->l[3] << " "<< sp->l[4] << " "<< sp->l[5] << " 1\n";
	    std::cout << "g rcc_" << cnt << ".g rcc_" << cnt<< "_1.s " << "rcc_" << cnt << "_2.s\n";*/
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
	    //sp->print();
	    delete sp;
	}
    }

    /* Point on line segment constraint test */
    {
	GeometrySolve* s = new GeometrySolve(9, 0, 1000);
	int p = 0;
	int s1 = 1;
	int s2 = 2;
	int cnt = 0;
	int maxcnt = 0;
	Gecode::IntVar PX(s->l[0 + 3*p]),  PY(s->l[1 + 3*p]),  PZ(s->l[2 + 3*p]);
	Gecode::IntVar S1X(s->l[0 + 3*s1]), S1Y(s->l[1 + 3*s1]), S1Z(s->l[2 + 3*s1]);
	Gecode::IntVar S2X(s->l[0 + 3*s2]), S2Y(s->l[1 + 3*s2]), S2Z(s->l[2 + 3*s2]);

	/* Add a canned constraint */
	add_constraint_pnt_on_line_segment(s, 1, 0, 1, 2);

	/* Define some additional constraints to pick a specific line segment */
	Gecode::rel(*s, S1X == 0);
	Gecode::rel(*s, S1Y == 0);
	Gecode::rel(*s, S1Z == 0);
	Gecode::rel(*s, S2X == 120);
	Gecode::rel(*s, S2Y == 800);
	Gecode::rel(*s, S2Z == 920);
	Gecode::branch(*s, s->l, Gecode::INT_VAR_SIZE_MIN(), Gecode::INT_VAL_MIN());

	Gecode::DFS<GeometrySolve> e(s);
	Gecode::DFS<GeometrySolve> e2(s);
	delete s;
	while (GeometrySolve* sp = e.next()) {
	    //sp->print();
	    cnt++;
	    std::cout << "in pnt" << cnt << ".s sph " << sp->l[0] << " " << sp->l[1] << " " << sp->l[2] << " 1\n";
	}
	maxcnt = cnt;
	cnt = 0;
	while (GeometrySolve* sp = e2.next()) {
	    cnt++;
	    if (cnt == 1)
		std::cout << "in line.s rcc " << sp->l[0] << " " << sp->l[1] << " " << sp->l[2];
	    if (cnt == maxcnt)
		std::cout << " " << sp->l[0] << " " << sp->l[1] << " " << sp->l[2] << " 5\n";
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
