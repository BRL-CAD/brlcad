/*                 S O L V E R _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
 * @author Dawn Thomas
 *
 */

#include "common.h"

#include <iostream>
#include <utility>                   // for std::pair
#include <algorithm>                 // for std::for_each

#include "pcVariable.h"
#include "pcPCSet.h"
#include "pcNetwork.h"
#include "pcSolver.h"
#include "pcParser.h"
#include "pc.h"

typedef Variable<int> * Vip;
/* Constraint functions */
bool f1(std::vector<VariableAbstract *> V ) {
    
    return ((Vip (V[0]))->getValue()*(Vip (V[1]))->getValue()-12 == 0);
}
bool f2(std::vector<VariableAbstract *> V ) {
    return ((Vip (V[0]))->getValue()+(Vip (V[1]))->getValue()-5 < 0);
}
bool f3(std::vector<VariableAbstract *> V ) {
    return ((Vip (V[0]))->getValue()- (Vip (V[1]))->getValue() - 2 == 0);
}
bool f4(std::vector<VariableAbstract *> V ) {
    return ((Vip (V[1]))->getValue()* (Vip (V[0]))->getValue() - 4 == 0);
}

int main()
{

    struct pc_pc_set pcs;
    PCSet<double> pc_set;
    PC_INIT_PCSET(pcs);
    pc_pushparameter(&pcs,"Testpar123=325.0");
    pc_pushparameter(&pcs,"Testpar234 = 1289.36243");
    pc_pushparameter(&pcs,"Testpar452 =1325.043");
    pc_pushconstraint(&pcs,"Constraint-test");
    
    Parser myparser(pc_set);
    myparser.parse(&pcs);
    
    /* display the set of variables and constraints generated as a
     * result of parsing
     */
    pc_set.display();

    pc_free_pcset(&pcs);

    typedef boost::adjacency_list<vecS, vecS, bidirectionalS,
	Variable<int>, Constraint > Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef GraphTraits::vertex_descriptor Vertex;
    typedef GraphTraits::edge_descriptor Edge;

    // declare a graph object
    BinaryNetwork<int > N;
    Solution<int> S;
    
    // Convenient naming for the vertices and Corrsponding Variables
    typedef Variable<int> Vi;
    Vi A=Vi("A", 1);
    Vi B=Vi("B", 2);
    Vi C=Vi("C", 3);
    Vi D=Vi("D", 4);
    Vi E=Vi("E", 6);

    Interval<int> I;
    I.assign(0, 5, 1);
    A.addInterval(I);
    B.addInterval(I);
    C.addInterval(I);
    D.addInterval(I);
    E.addInterval(-16, -10, 1);
    E.addInterval(-6, -4, 1);
    E.addInterval(0, 7, 1);
    E.addInterval(8, 10, 1);
    E.addInterval(16, 18, 1);
    E.addInterval(32, 46, 1);
    E.addInterval(64, 68, 1);
    std::cout<< std::endl << "Testing Interval Intersection" << std::endl;
    E.display();
    E.intersectInterval(Interval<int>(3,40,1));
    E.display();
    typedef Constraint Ci;
    Ci constraint_array[4];
    constraint_array[0] = Ci("0", "A*B=12",f1,2,"A","B");
    constraint_array[1] = Ci("1", "B+C<5",f2,2,"B","C");
    constraint_array[2] = Ci("2", "A-D=2",f3,2,"A","D");
    constraint_array[3] = Ci("3", "A*C=4",f4,2,"A","C");

    //std::cout << constraint_array[0].getExp() << "--------" << std::endl;
    /* Add the vertices */

    N.add_vertex(A);
    N.add_vertex(B);
    N.add_vertex(C);
    N.add_vertex(D);
    /* Add the edges */
    for (int i=0; i<4; i++) {
        N.add_edge(constraint_array[i]);
    }
    //std::cout << "___" << boost::num_vertices(N.G) << std::endl;

    /*N.display();
      S = N.solve();
      S.display();
    */
    GTSolver<int> GTS(N);
    BTSolver<int> BTS;

    std::cout << "-----------------------------" << std::endl;
    GTS.solve(&S);
    std::cout << "Solution using Generate-Test" << std::endl;
    S.display();
    S.clear();
    std::cout << "-----------------------------" << std::endl;
    
    BTS.solve(&N,&S);
    std::cout << "Solution using BackTracking" << std::endl;
    
    S.display();
    std::cout << "Number of Constraint checks performed" << std::endl;
    std::cout << "Generate-Test Solution:" << GTS.numChecks() << std::endl;
    std::cout << "BackTracking based Solution:" << BTS.numChecks() << std::endl;

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
