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
#include <iostream>
#include <utility>                   // for std::pair
#include <algorithm>                 // for std::for_each
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>

#include "pcVariable.h"
#include "pcNetwork.h"

/* Constraint functions */
bool f1(std::vector<int> V ) {
    return (V[0]+V[1]-3 == 0);
}
bool f2(std::vector<int> V ) {
    return (V[0]+V[1]-4 > 0);
}
bool f3(std::vector<int> V ) {
    return (V[0]- V[1] -5 < 0);
}
bool f4(std::vector<int> V ) {
    return (V[1]-V[0]-2 == 0);
}

int main()
{
    
    /* Stage I : Checking Constrained Classes */
    std::cout<<"______________________________________________________________"<<std::endl<<std::endl;
    std::cout<<"____________________Checking Network Classes__________________"<<std::endl;

    using namespace boost;
    {

    }

    /* Stage II : Checking Network Classes */
    std::cout<<"______________________________________________________________"<<std::endl<<std::endl;
    std::cout<<"____________________Checking Network Classes__________________"<<std::endl;


    typedef Interval<float> Ifl;
    Solution<float> Soln;
    Variable<float> O =Variable<float>("On");
    O.addInterval(Ifl(3.4,8.3,0.6));
    O.addInterval(Ifl(-2.8,2.8,0.6));
    O.addInterval(Ifl(-13.4,-5.3,0.6));
    O.addInterval(Ifl(-14.4,-10.,0.6));
    O.display();
    
    using namespace boost;
    {

    // declare a graph object
    BinaryNetwork<int > N;

    // Convenient naming for the vertices and Corrsponding Variables
    Variable<int> A,B,C,D;

    Interval<int> I;
    I.assign(0,5,1);
    A.addInterval(I);
    B.addInterval(I);
    C.addInterval(I);
    D.addInterval(I);

    Constraint<int> constraint_array[4];
    constraint_array[0].function(f1);
    constraint_array[1].function(f2);
    constraint_array[2].function(f3);
    constraint_array[3].function(f4);

    // Establish the set of Edges
    typedef Edge<int> Ei;
    Ei edge_array[] = 
    { Ei(A,B), Ei(B,C), Ei(A,D), Ei(A,C)};
/*
    Edge edge_array[] = 
    { Edge(U,V), Edge(V,W), Edge(U,X), Edge(U,W)};

    // writing out the edges in the graph
    const int num_edges = sizeof(edge_array)/sizeof(edge_array[0]);

    // add the edges to the graph object
    for (int i = 0; i < num_edges; ++i)
    N.add_edge(edge_array[i].first, edge_array[i].second);
*/
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
