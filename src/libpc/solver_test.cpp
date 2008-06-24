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
    
    using namespace boost;
    {

    // declare a graph object
    binaryNetwork<int > N;

    // Convenient naming for the vertices and Corrsponding Variables
    Variable<int> A,B,C,D;

    Interval<int> I;
    I.assign(0,3,1);
    A.addInterval(I);
    B.addInterval(I);
    C.addInterval(I);
    D.addInterval(I);

    enum { T, U, V, W};

    // Establish the set of Edges
    Edge edge_array[] = 
    { Edge(T,U), Edge(T,V), Edge(T,W), Edge(V,W), Edge(V,U), Edge(U,W)};

    // writing out the edges in the graph
    const int num_edges = sizeof(edge_array)/sizeof(edge_array[0]);

    // add the edges to the graph object
    for (int i = 0; i < num_edges; ++i)
	N.add_edge(edge_array[i].first, edge_array[i].second);
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
