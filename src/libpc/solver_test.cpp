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
#include "pcVCSet.h"
#include "pcNetwork.h"
#include "pcSolver.h"
#include "pcParser.h"
#include "pcMathVM.h"
#include "pc.h"

typedef Variable<int> * Vip;
/* Constraint functions */
struct f1 {
public:
    bool operator() (VCSet & vcset, std::list<std::string> Vid) const {
    typedef Variable<int> * Vi ;
    int A = ((Variable<int>*) vcset.getVariablebyID("A"))->getValue();
    int B = ((Vi) vcset.getVariablebyID("B"))->getValue();
    //std::cout << "!== A  " << A << std::endl;
    //std::cout << "!== B  " << B << std::endl;
    //PC_PCSET_GETVAR(pcset, int, A);
    //PC_PCSET_GETVAR(pcset, int, B);
    return (A * B == 12);
    }
} f1;
struct f2 {
public:
    bool operator() (VCSet & vcset, std::list<std::string> Vid) const {
    typedef Variable<int> * Vi ;
    int B = ((Vi) vcset.getVariablebyID("B"))->getValue();
    int C = ((Vi) vcset.getVariablebyID("C"))->getValue();
    //std::cout << "!== B  " << B << std::endl;
    //std::cout << "!== C  " << C << std::endl;
    //PC_PCSET_GETVAR(pcset, int, B);
    //PC_PCSET_GETVAR(pcset, int, C);
    return (B + C < 5);
    }
} f2;

struct f3 {
public:
    bool operator() (VCSet & vcset, std::list<std::string> Vid) const {
    typedef Variable<int> * Vi ;
    int A = ((Vi) vcset.getVariablebyID("A"))->getValue();
    int D = ((Vi) vcset.getVariablebyID("D"))->getValue();
    //PC_PCSET_GETVAR(pcset, int, A);
    //PC_PCSET_GETVAR(pcset, int, D);
    return (A - D == 2);
    }
} f3;

struct f4 {
public:
    bool operator() (VCSet & vcset, std::list<std::string> Vid) const {
    typedef Variable<int> * Vi ;
    int C = ((Vi) vcset.getVariablebyID("C"))->getValue();
    int A = ((Vi) vcset.getVariablebyID("A"))->getValue();
    //PC_PCSET_GETVAR(pcset, int, A);
    //PC_PCSET_GETVAR(pcset, int, C);
    return (A * C == 4);
    }
} f4;

int main()
{

    struct pc_pc_set pcs;
    VCSet vc_set;
    PC_INIT_PCSET(pcs);
    pc_pushparameter(&pcs,"Testpar123=325.0");
    pc_pushparameter(&pcs,"Testpar234 = 1289.36243");
    pc_pushparameter(&pcs,"Testpar452 =1325.043");
    pc_pushconstraint(&pcs,"Constraint-test");
    
    Parser myparser(vc_set);
    myparser.parse(&pcs);
    
    /* display the set of variables and constraints generated as a
     * result of parsing
     */
    vc_set.display();
    pc_free_pcset(&pcs);

    typedef boost::adjacency_list<boost::vecS, boost::vecS,
		    boost::bidirectionalS, Variable<int>*, Constraint *> Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef GraphTraits::vertex_descriptor Vertex;
    typedef GraphTraits::edge_descriptor Edge;

    /* declare a graph object */
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

    /*N.display();
      S = N.solve();
      S.display();
    */
    GTSolver<int> GTS;
    BTSolver<int> BTS;
    PCSolver<int> PCS;

    std::cout << "-----------------------------" << std::endl;
    GTS.solve(N,S);
    std::cout << "Solution using Generate-Test" << std::endl;
    S.display();
    S.clear();
    std::cout << "-----------------------------" << std::endl;
    BTS.solve(N,S);
    std::cout << "Solution using BackTracking" << std::endl;
    S.display();
    S.clear();
    std::cout << "-----------------------------" << std::endl;
    PCS.solve(myvcset,S);
    std::cout << "Solution using Generic GT Solver" << std::endl;
    S.display();    
    std::cout << "-----------------------------" << std::endl;
    std::cout << "Number of Constraint checks performed" << std::endl;
    std::cout << "Generate-Test Solution:" << GTS.numChecks() << std::endl;
    std::cout << "BackTracking based Solution:" << BTS.numChecks() << std::endl;
    std::cout << "Generic Generate-Test Solution:" << PCS.numChecks() << std::endl;
    
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
