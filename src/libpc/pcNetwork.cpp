/*                    P C N E T W O R K . C P P
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
/** @file pcNetwork.cpp
 *
 * Method implementations of Constraint Network
 *
 */

#include "pcNetwork.h"


/** Default Constructor */
template<class T>
BinaryNetwork<T>::BinaryNetwork()
{
}


template<class T>
BinaryNetwork<T>::BinaryNetwork(std::vector<Variable<T> *> V, std::vector<Constraint *> C) {
    typename std::vector<Variable<T> *>::iterator i = V.begin();
    typename std::vector<Constraint *>::iterator j = C.begin();

    while (i!=V.end()) {
	add_vertex(*i, G);
	++i;
    }

    while (j!=C.end()) {
	add_edge(*j, G);
	++i;
    }
}


/** BinaryNetwork construction from VCSet */
template<class T>
BinaryNetwork<T>::BinaryNetwork(VCSet & vcset)
{
    std::list<VariableAbstract *>::iterator i;
    std::list<Constraint *>::iterator j;

    for (i = vcset.Vars.begin(); i != vcset.Vars.end(); ++i) {
	typedef Variable<T> *Vp;
	add_vertex(Vp (*i));
    }
    for (j = vcset.Constraints.begin(); j != vcset.Constraints.end(); ++j) {
	add_edge(*j);
    }
}


template<class T>
void BinaryNetwork<T>::add_vertex(Variable<T> *V)
{
    boost::add_vertex(V, G);
}


template<class T>
void BinaryNetwork<T>::add_edge(Constraint *C)
{
    Vertex v1, v2;
    std::list<std::string> vl = C->getVariableList();
    getVertexbyID(vl.front(), v1);
    getVertexbyID(vl.back(), v2);
    boost::add_edge(v1, v2, C, G);
}


template<class T>
void BinaryNetwork<T>::setVariable(Vertex vertex, Variable<T> *var)
{
    G[v] = var;
}


template<class T>
bool BinaryNetwork<T>::check()
{
    typename GraphTraits::edge_iterator edge_i, edge_end;
    for (tie(edge_i, edge_end) = edges(G); edge_i != edge_end; ++edge_i) {
	e = *edge_i;
	if (! G[e]->check()) {
	    return false;
	}
    }
    return true;
}


template<class T>
void BinaryNetwork<T>::display()
{
    std::map<std::string, std::string> graph_attr, vertex_attr, edge_attr;
    graph_attr["size"] = "3, 3";
    graph_attr["rankdir"] = "LR";
    graph_attr["ratio"] = "fill";
    vertex_attr["shape"] = "circle";

    Vertexwriter<T> vw(G);
    Edgewriter<T> ew(G);
    boost::write_graphviz(std::cout, G, vw, ew,
			  boost::make_graph_attributes_writer(graph_attr, vertex_attr, edge_attr));

}


template<class T>
void BinaryNetwork<T>::getVertexbyID(std::string Vid, Vertex& vertex)
{
    for (tie(v_i, v_end) = vertices(G); v_i != v_end; ++v_i) {
	if (G[*v_i]->getID() == Vid) {
	    vertex = *v_i;
	    return ;
	}
    }
    return ;

}


/* Temporary Hack for separation of code between hpp and cpp
 * would be unnecessary after detemplating
 */
template BinaryNetwork<int>::BinaryNetwork(VCSet&);
template void BinaryNetwork<int>::display();
template bool BinaryNetwork<int>::check();

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
