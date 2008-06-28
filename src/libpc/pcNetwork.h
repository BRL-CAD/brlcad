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
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

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
class Vertexwriter {
    typedef typename boost::adjacency_list<vecS, vecS, undirectedS, 
	Variable<T>, Constraint<T> > Graph;
    typedef graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::vertex_descriptor Vertex;
public:
    Vertexwriter(const Graph& g):g(g) {};
    void operator() (std::ostream& out,const Vertex& v) const {
    out <<"[label=\""<<g[v].getID()<<"\"]";
    }
private:
    const Graph& g;
};

using namespace boost;
template<class T>
class Edgewriter {
    typedef typename boost::adjacency_list<vecS, vecS, undirectedS, 
	Variable<T>, Constraint<T> > Graph;
    typedef graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::edge_descriptor Edge;
public:
    Edgewriter(const Graph& g):g(g) {};
    void operator() (std::ostream& out,const Edge& e) const {
    out <<"[label=\""<<g[e].getExp()<<"\"]";
    }
private:
    const Graph& g;
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

    typename GraphTraits::vertex_iterator v_i, v_end;
    typename GraphTraits::out_edge_iterator e_i, e_end; 

public:
    Graph G;
    BinaryNetwork();
    BinaryNetwork(std::vector<Variable<T> >, std::vector<Constraint<T> >);
    void add_vertex(Variable<T> V) {
	boost::add_vertex(V,G);
    }
    void add_edge(Constraint<T> C) {
	Vertex v1,v2;
	getVertexbyID(C.Variables.front(), v1);
	getVertexbyID(C.Variables.back(), v2);
	//G[v1].display();
	//G[v2].display();
	boost::add_edge(v1,v2,C,G);
    }

    void setVariable(Vertex v, Variable<T>& var) {
	G[v] = var;
    }

    Solution<T> solve();
    void getVertexbyID(std::string,Vertex&);

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
    Solution<T> S;

/* Initialize values for Variables 
    for(tie(v_i,v_end) = vertices(G); v_i != v_end; ++v_i) {
	v = *v_i;
    }
 */

    for(tie(v_i,v_end) = vertices(G); v_i != v_end; ++v_i) {
	v = *v_i;
	for(tie(e_i,e_end) = out_edges(v,G); e_i !=e_end; ++e_i) {
	    e=*e_i;
	    if(! G[e].solved()) {
		std::vector<T> Vars; 
		Vertex v1 = target(e,G);
solve_start:
		Vars.push_back(G[v].getValue());
		Vars.push_back(G[v1].getValue());
		
		/* Check if constraint is solved for a particular set of values */
		while(G[v1].constrained == 0 && ! G[e].check(Vars) && G[v1].getValue() != G[v1].getLast() ) {
		    Vars.pop_back();		    
		    ++G[v1];
		    Vars.push_back(G[v1].getValue());
		}
		if( G[e].solved()) { 
		    G[v].constrained =1; G[v1].constrained = 1;
		    std::cout<<"+--------------------------------------"<<std::endl;
		    break;
		}

		if(G[v1].getValue() == G[v1].getLast() ) {
		    G[v1].setValue(G[v1].getFirst());
		    ++G[v];
		    goto solve_start;
		}
		if(G[v].getValue() == G[v].getLast() ) {
		    G[v1].setValue(G[v1].getFirst());
		    ++G[v];
		    goto solve_start;
		}
		std::cout<<"++--------------------------------------"<<std::endl;
	    }
	}
    }
    /* Store the values in the Solution */
    for(tie(v_i,v_end) = vertices(G); v_i != v_end; ++v_i) {
	v = *v_i;
	if(G[v].constrained ==1) S.VarDom.push_back(VarDomain<int>(G[v],Domain<int>()));
    }
    return S;
}

template<class T>
void BinaryNetwork<T>::display()
{
  std::map<std::string,std::string> graph_attr, vertex_attr, edge_attr;
  graph_attr["size"] = "3,3";
  graph_attr["rankdir"] = "LR";
  graph_attr["ratio"] = "fill";
  vertex_attr["shape"] = "circle";
  
  Vertexwriter<T> vw(G);
  Edgewriter<T> ew(G);
  boost::default_writer dw;
  boost::default_writer gw;  
  boost::write_graphviz(std::cout, G,vw,ew,make_graph_attributes_writer(graph_attr, vertex_attr, 
                                                     edge_attr));

}


template<class T>
/*boost::graph_traits<boost::adjacency_list<vecS, vecS, undirectedS, Variable<T>, Constraint<T> > > ::vertex_descriptor*/
void BinaryNetwork<T>::getVertexbyID(std::string Vid, Vertex& v)
{
    for(tie(v_i,v_end) = vertices(G); v_i != v_end; ++v_i) {
	if ( G[*v_i].getID() == Vid ) {
	    v = *v_i; 
	    //G[v].display();
	    //std::cout<< "<<<" << G[v].getID() << ">>>"<<std::endl;
	    return ;
	    break;
	}
    }
    return ;
    
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
