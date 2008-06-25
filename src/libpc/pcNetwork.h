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
#include "pcConstraint.h"

template<class T>
class Edge : public std::pair< std::string, std::string> 
{
private:

public:
    Edge(Variable<T> t, Variable<T> u):std::pair<std::string, std::string>(t.getID(), u.getID()) {};
};

using namespace boost;
template<class T>
class BinaryNetwork 
{
private:
    typedef typename boost::adjacency_list<vecS, vecS, undirectedS, 
	Variable<T>, Constraint<T> > Graph;
    typedef graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::vertex_descriptor Vertex;
    typedef typename GraphTraits::edge_descriptor Edge;

    Graph G;
    typename GraphTraits::vertex_iterator v_i, v_end;
    typename GraphTraits::out_edge_iterator e_i, e_end; 

public:
    BinaryNetwork();
    BinaryNetwork(std::vector<Variable<T> >, std::vector<Constraint<T> >);
    void add_edge(int a,int b) {
	boost::add_edge(a,b, G);
    };

    void setVariable(Vertex v, Variable<T>& var) {
	G[v] = var;
    };

    Solution<T> solve();
    void getVertexbyID(std::string Vid);
    void display();
};

template<class T>
BinaryNetwork<T>::BinaryNetwork()
{
};

template<class T>
BinaryNetwork<T>::BinaryNetwork(std::vector<Variable<T> > V, std::vector<Constraint<T> > C) {

    typename std::vector<Variable<T> >::iterator i = V.begin();
    typename std::vector<Constraint<T> >::iterator j = C.begin();
    
    while(i!=V.end()) {
	add_vertex(*i,G);
	++i;
    }

    while(j!=C.end()) {
	add_edge(*j,G);
	++i;
    }    
    /*for(tie(v_i,v_end) = vertices(G); v_i != v_end; ++v_i, ++i) {
    G[*v_i]=*i;
    }
    for(tie(e_i,e_end) = edges(G); e_i !=e_end; ++e_i,++j) {
	G[*e_i]=*j;
    }*/
};

template<class T>
Solution<T> BinaryNetwork<T>::solve()
{
    Vertex v;
    Edge e;
    for(tie(v_i,v_end) = vertices(G); v_i != v_end; ++v_i) {
	v = *v_i;
	for(tie(e_i,e_end) = out_edges(v,G); e_i !=e_end; ++e_i) {
	    e=*e_i;
	    std::cout<<"Solved status: "<<G[e].solved()<<std::endl;
	    }
	}

}

template<class T>
void BinaryNetwork<T>::display()
{
  std::map<std::string,std::string> graph_attr, vertex_attr, edge_attr;
  graph_attr["size"] = "3,3";
  graph_attr["rankdir"] = "LR";
  graph_attr["ratio"] = "fill";
  vertex_attr["shape"] = "circle";

  /* boost::write_graphviz(std::cout, G, 
                        make_label_writer(),
                        make_label_writer(),
                        make_graph_attributes_writer(graph_attr, vertex_attr, 
                                                     edge_attr));*/
}
template<class T>
void BinaryNetwork<T>::getVertexbyID(std::string Vid)
{
}
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
