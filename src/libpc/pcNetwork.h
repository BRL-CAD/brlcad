/*                    P C N E T W O R K . H
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
/** @addtogroup soln */
/** @{ */
/** @file pcNetwork.h
 *
 * Class definition of Constraint Network
 *
 * @author Dawn Thomas
 */
#ifndef __PCNETWORK_H__
#define __PCNETWORK_H__

#include <iostream>
#include <cstdlib>
#include <string>
#include <list>
#include <stack>
#include "pcBasic.h"
#include "pcVariable.h"

class Edge : public std::pair<int, int > 
{
private:

public:
    Edge(int t,int u):std::pair<int, int>(t,u) {};
};

using namespace boost;
template<class T>
class BinaryNetwork 
{
private:

    typedef typename boost::adjacency_list<vecS, vecS, undirectedS, 
	Variable<T>, Constraint > Graph;
    Graph G;
 
public:
    void add_edge(int a,int b) {
	boost::add_edge(a,b, G);
    };

    void setVariable(typename Graph::vertex_descriptor v, Variable<T>& var) {
	G[v] = var;
    };

    Solution<T> solve()
    {
	typedef typename boost::adjacency_list<vecS, vecS, undirectedS, 
	Variable<T>, Constraint > Graph;
	typedef typename graph_traits<Graph>::vertex_descriptor Vertex;
	Vertex u = *vertices(G).first;
    }
};

#endif
/** @} */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
