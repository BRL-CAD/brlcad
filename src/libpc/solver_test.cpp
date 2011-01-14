/*                 S O L V E R _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file solver_test.cpp
 *
 * @brief Simple Test cases for Constraint Solver
 *
 */

#include "common.h"

#include <iostream>
#include <utility>                   // for std::pair
#include <algorithm>                 // for std::for_each

#include "pcVariable.h"
#include "pcVCSet.h"
#include "pcNetwork.h"
#include "pcSolver.h"
#include "pcParser.h"
#include "pcMathVM.h"
#include "pc.h"

typedef Variable<int> *Vip;
/* Constraint functions */
struct f1 {
public:
    bool operator() (VCSet & vcset, std::list<std::string> UNUSED(Vid)) const {
	typedef Variable<int> *Vi ;
	int A = ((Variable<int>*) vcset.getVariablebyID("A"))->getValue();
	int B = ((Vi) vcset.getVariablebyID("B"))->getValue();
	return (A * B == 12);
    }
} f1;
struct f2 {
public:
    bool operator() (VCSet & vcset, std::list<std::string> UNUSED(Vid)) const {
	typedef Variable<int> *Vi ;
	int B = ((Vi) vcset.getVariablebyID("B"))->getValue();
	int C = ((Vi) vcset.getVariablebyID("C"))->getValue();
	return (B + C < 5);
    }
} f2;

struct f3 {
public:
    bool operator() (VCSet & vcset, std::list<std::string> UNUSED(Vid)) const {
	typedef Variable<int> *Vi ;
	int A = ((Vi) vcset.getVariablebyID("A"))->getValue();
	int D = ((Vi) vcset.getVariablebyID("D"))->getValue();
	return (A - D == 2);
    }
} f3;

struct f4 {
public:
    bool operator() (VCSet & vcset, std::list<std::string> UNUSED(Vid)) const {
	typedef Variable<int> *Vi ;
	int C = ((Vi) vcset.getVariablebyID("C"))->getValue();
	int A = ((Vi) vcset.getVariablebyID("A"))->getValue();
	return (A * C == 4);
    }
} f4;

int main()
{
    BackTrackSolver<double> GBTS; /* Generic Backtracker */
    PCSolver<double> GPCS; /* Generic Solver */
    Solution<double> S1;
    struct pc_pc_set pcs;
    VCSet vc_set;

    pc_init_pcset(&pcs);
    fastf_t D = 8.04;
    point_t E = {8.04, 3.2, 0.0};
    vect_t F = {5.4, 3.6, 4.4};
    pc_pushparam_struct(&pcs, "D", PC_DB_FASTF_T, &D);
    pc_pushparam_struct(&pcs, "E", PC_DB_POINT_T, &E);
    pc_pushparam_struct(&pcs, "G", PC_DB_VECTOR_T, &F);
    const char *args[] = { "F", "G" };
    pc_pushconstraint_expr(&pcs, "Constraint-test", "A + B < 0");
    pc_pushparam_struct(&pcs, "F", PC_DB_VECTOR_T, &F);
    pc_constrnt *con;
    pc_mk_isperpendicular(&con, "G _|_ F", args);
    pc_pushconstraint_struct(&pcs, "Struct-test", 2, 3, &pc_isperpendicular, args);
    pc_free_constraint(con);

    Parser myparser(vc_set); /* generate vc_set from pcs using parser */
    myparser.parse(&pcs);
   
    /* modify/access parameter property in vc_set using getParameter */
    vc_set.getParameter("G")->setConst(true);

    /* Two solution methods*/
    vc_set.display();
    GBTS.solve(vc_set, S1);
    std::cout << "\nSolution using Generic BackTracking Solver "
              << GBTS.numChecks() << "\t" << GBTS.numSolutions() << std::endl;
    S1.display();
    S1.clear();

    vc_set.display();
    GPCS.solve(vc_set, S1);
    std::cout << "\nSolution using Generic Solver "
              << GPCS.numChecks() << "\t" << GPCS.numSolutions() << std::endl;
    S1.cdisplay();

    pc_free_pcset(&pcs);
#if 0    
    /** Testing PCSolver Methods */

    VCSet myvcset;
    Solution<int> S;

    /* add Variables to VCSet */
    myvcset.addVariable<int>("A", 1, 0, 5, 1);
    myvcset.addVariable<int>("B", 3, 0, 5, 1);
    myvcset.addVariable<int>("C", 2, 0, 5, 1);
    myvcset.addVariable<int>("D", 0, 0, 5, 1);
    
    /* add Constraints to VCSet */
    myvcset.addConstraint("0", "A * B = 12", f1, 2, "A", "B");
    myvcset.addConstraint("1", "B + C < 5", f2, 2, "B", "C");
    myvcset.addConstraint("2", "A - D = 2", f3, 2, "A", "D");
    myvcset.addConstraint("3", "A * C = 4", f4, 2, "A", "C");
    myvcset.display();
    
    BinaryNetwork<int > N(myvcset);
    N.display();

    GTSolver<int> GTS;
    BackTrackSolver<int> BTS;
    PCSolver<int> PCS;

    std::cout << "-----------------------------" << std::endl;
    GTS.solve(N, S);
    std::cout << "Solution using Generate-Test" << std::endl;
    S.display();
    S.clear();
    std::cout << "-----------------------------" << std::endl;
    BTS.solve(myvcset, S);
    std::cout << "Solution using BackTracking" << std::endl;
    S.display();
    S.clear();
    std::cout << "-----------------------------" << std::endl;
    PCS.solve(myvcset, S);
    std::cout << "Solution using Generic GT Solver" << std::endl;
    S.display();    
    std::cout << "-----------------------------" << std::endl;
    std::cout << "Number of Constraint checks performed" << std::endl;
    std::cout << "Generate-Test:" << GTS.numChecks() << std::endl;
    std::cout << "BackTracking based:" << BTS.numChecks() << std::endl;
    std::cout << "Generic Generate-Test:" << PCS.numChecks() << std::endl;
#endif
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
