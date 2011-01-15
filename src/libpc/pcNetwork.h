/*                    P C N E T W O R K . H
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
/** @addtogroup pcsolver */
/** @{ */
/** @file pcNetwork.h
 *
 * Class definition of Constraint Network
 *
 */
#ifndef __PCNETWORK_H__
#define __PCNETWORK_H__

#include "common.h"

#include <iostream>
#include <string>
#include <vector>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

#include "pcBasic.h"
#include "pcVariable.h"
#include "pcConstraint.h"
#include "pcVCSet.h"

template<typename T>
class GTSolver;
template<typename T>
class BTSolver;

template<class T>
class Vertexwriter {
    typedef typename boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, Variable<T> *, Constraint *> Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::vertex_descriptor Vertex;
public:
    Vertexwriter(const Graph& graph) : g(graph) {};
    void operator() (std::ostream& output, const Vertex& v) const {
	output << "[label=\"" << g[v]->getID() << "\"]";
    }
private:
    const Graph& g;
};


template<class T>
class Edgewriter {
    typedef typename boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, Variable<T> *, Constraint *> Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::edge_descriptor Edge;
public:
    Edgewriter(const Graph& graph) : g(graph) {};
    void operator() (std::ostream& output, const Edge& e) const {
	output << "[label=\"" << g[e]->getExp() << "\"]";
    }
private:
    const Graph& g;
};


template<class T>
class BinaryNetwork
{
    typedef typename boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, Variable<T> *, Constraint *> Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::vertex_descriptor Vertex;
    typedef typename GraphTraits::edge_descriptor Edge;

    typename GraphTraits::vertex_iterator v_i, v_end;
    typename GraphTraits::out_edge_iterator e_i, e_end;
public:
    Graph G;
    /** Constructors and Destructors */
    BinaryNetwork();
    BinaryNetwork(std::vector<Variable<T> *>, std::vector<Constraint *>);
    BinaryNetwork(VCSet & vcset);
    
    /** Data access methods */
    void getVertexbyID(std::string, Vertex&);

    /** Data addition/modification methods */
    void add_vertex(Variable<T> *V);
    void add_edge(Constraint *C);
    void setVariable(Vertex v, Variable<T> *var);

    /** Solution support functions */
    bool check();

    /** Data display methods */
    void display();
private:
    std::vector<Edge> precedents;
    Solution<T> S;
    Vertex v;
    Edge e;

    friend class GTSolver<T>;
    friend class BTSolver<T>;
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
