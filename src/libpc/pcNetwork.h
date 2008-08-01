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
/** @addtogroup pcsolver */
/** @{ */
/** @file pcNetwork.h
 *
 * Class definition of Constraint Network
 *
 * @author Dawn Thomas
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
#include "pcPCSet.h"
#include "pcSolver.h"


template<class T>
class Edge : public std::pair< std::string, std::string>
{
private:

public:
    Edge(Variable<T> t, Variable<T> u):std::pair<std::string, std::string>(t.getID(), u.getID()) {};
};


template<class T>
class Vertexwriter {
    typedef typename boost::adjacency_list<vecS, vecS, bidirectionalS,
					   Variable<T>, Constraint > Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::vertex_descriptor Vertex;
public:
    Vertexwriter(const Graph& g):g(g) {};
    void operator() (std::ostream& output, const Vertex& v) const {
	output << "[label=\"" << g[v].getID() << "\"]";
    }
private:
    const Graph& g;
};


template<class T>
class Edgewriter {
    typedef typename boost::adjacency_list<vecS, vecS, bidirectionalS,
					   Variable<T>, Constraint > Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::edge_descriptor Edge;
public:
    Edgewriter(const Graph& g):g(g) {};
    void operator() (std::ostream& output, const Edge& e) const {
	output << "[label=\"" << g[e].getExp() << "\"]";
    }
private:
    const Graph& g;
};


template<class T>
class BinaryNetwork
{
    typedef typename boost::adjacency_list<vecS, vecS, bidirectionalS,
					   Variable<T>, Constraint > Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::vertex_descriptor Vertex;
    typedef typename GraphTraits::edge_descriptor Edge;

    typename GraphTraits::vertex_iterator v_i, v_end;
    typename GraphTraits::out_edge_iterator e_i, e_end;
public:
    /** Constructors and Destructors */
    BinaryNetwork();
    BinaryNetwork(std::vector<Variable<T> >, std::vector<Constraint>);
    BinaryNetwork(PCSet & pcset);
    void add_vertex(Variable<T> V);
    void add_edge(Constraint C);
    void setVariable(Vertex v, Variable<T>& var);

    Solution<T> solve();
    void getVertexbyID(std::string, Vertex&);
    void display();
    bool check();
    /* TODO: Implement Solver Hierarchy and Friendship Inheritance ? */
    friend class GTSolver<T>;
    friend class BTSolver<T>;
private:
    std::vector<Edge> precedents;
    Solution<T> S;
    Vertex v;
    Edge e;
    int checkEdge(Edge e);

    void preprocess();
    void propagate();
    bool happy();
    void atomic();
    void split();
    Graph G;

};

template<class T>
BinaryNetwork<T>::BinaryNetwork()
{
};

template<class T>
BinaryNetwork<T>::BinaryNetwork(std::vector<Variable<T> > V, std::vector<Constraint> C) {

    typename std::vector<Variable<T> >::iterator i = V.begin();
    typename std::vector<Constraint>::iterator j = C.begin();

    while (i!=V.end()) {
	add_vertex(*i, G);
	++i;
    }

    while (j!=C.end()) {
	add_edge(*j, G);
	++i;
    }
    /*for (tie(v_i, v_end) = vertices(G); v_i != v_end; ++v_i, ++i) {
      G[*v_i]=*i;
      }
      for (tie(e_i, e_end) = edges(G); e_i !=e_end; ++e_i, ++j) {
      G[*e_i]=*j;
      }*/
};

template<class T>
BinaryNetwork<T>::BinaryNetwork(PCSet & pcset)
{
    std::list<VariableAbstract *>::iterator i;
    std::list<Constraint *>::iterator j;
/*
    for ( i = pcset.Vars.begin(); i != pcset.Vars.end(); ++i) {
	//add_vertex(*(Variable<T>*(*i)));
    }
    for ( j = pcset.Constraints.begin(); j != pcset.Constraints.end(); ++j) {
	add_edge(**j);
    }
*/
}

template<class T>
void BinaryNetwork<T>::add_vertex(Variable<T> V)
{
    boost::add_vertex(V, G);
}

template<class T>
void BinaryNetwork<T>::add_edge(Constraint C)
{
    Vertex v1, v2;
    getVertexbyID(C.Variables.front(), v1);
    getVertexbyID(C.Variables.back(), v2);
    boost::add_edge(v1, v2, C, G);
}

template<class T>
void BinaryNetwork<T>::setVariable(Vertex v, Variable<T>& var)
{
    G[v] = var;
}

template<class T>
int BinaryNetwork<T>::checkEdge(Edge e)
{
    precedents.push_back(e);
    if (!G[e].solved()) {
	std::vector<T> Vars;
	Vertex v = source(e, G);
	Vertex v1 = target(e, G);
    checkEdge_start:
	Vars.push_back(G[v].getValue());
	Vars.push_back(G[v1].getValue());

	/* Check if constraint is solved for a particular set of values */
	while (!G[e].check(Vars)) {
	    if (G[v1].getValue() == G[v1].getLast() && G[v].getValue() == G[v].getLast() ) {
		G[v].setValue(G[v].getFirst());
		G[v1].setValue(G[v1].getFirst());
		precedents.pop_back();
		e = precedents.back();
		precedents.pop_back();
		std::cout << "++---------- End Reached calling previous edge" << std::endl;
		checkEdge(e);
		break;
	    } else if (G[v1].getValue() == G[v1].getLast()) {
		typename GraphTraits::out_edge_iterator edge_i, edge_end;
		G[v1].setValue(G[v1].getFirst());
		++G[v];
		precedents.pop_back();
		for (tie(edge_i, edge_end) = out_edges(v, G); edge_i !=edge_end; ++edge_i) {
		    G[*edge_i].setStatus(0);
		}
		std::cout << "++---------- Incrementing source" << std::endl;
		checkEdge(e);
		break;
	    }

	    Vars.pop_back();
	    ++G[v1];
	    Vars.push_back(G[v1].getValue());
	}
    }
    std::cout << "++---------- Found ONE. return" << std::endl;
    return 0;
}

template<class T>
Solution<T> BinaryNetwork<T>::solve()
{
    /* Initialize values for Variables TODO: Define a function / better version*/
    for (tie(v_i, v_end) = vertices(G); v_i != v_end; ++v_i) {
	G[*v_i].setValue(G[*v_i].getFirst());
    }

    /* Loop through all the vertices and all their incident edges */
    for (tie(v_i, v_end) = vertices(G); v_i != v_end; ++v_i) {
	v = *v_i;
	for (tie(e_i, e_end) = out_edges(v, G); e_i !=e_end; ++e_i) {
	    e=*e_i;
	    checkEdge(e);
	}
    }

    /* Push the state of Variables into the solution */
    for (tie(v_i, v_end) = vertices(G); v_i != v_end; ++v_i) {
	v = *v_i;
	S.VarDom.push_back(VarDomain<int>(G[v], Domain<int>(G[v].getValue(), G[v].getValue(), 1)));
    }
    return S;
}

template<class T>
void BinaryNetwork<T>::display()
{
    std::map<std::string, std::string> graph_attr, vertex_attr, edge_attr;
    graph_attr["size"] = "3,3";
    graph_attr["rankdir"] = "LR";
    graph_attr["ratio"] = "fill";
    vertex_attr["shape"] = "circle";

    Vertexwriter<T> vw(G);
    Edgewriter<T> ew(G);
    boost::default_writer dw;
    boost::default_writer gw;
    boost::write_graphviz(std::cout, G, vw, ew,
			  make_graph_attributes_writer(graph_attr, vertex_attr, edge_attr));

}


template<class T>
/*boost::graph_traits<boost::adjacency_list<vecS, vecS, undirectedS, Variable<T>, Constraint> > ::vertex_descriptor*/
void BinaryNetwork<T>::getVertexbyID(std::string Vid, Vertex& v)
{
    for (tie(v_i, v_end) = vertices(G); v_i != v_end; ++v_i) {
	if ( G[*v_i].getID() == Vid ) {
	    v = *v_i;
	    //G[v].display();
	    //std::cout << "<<<" << G[v].getID() << ">>>"<<std::endl;
	    return ;
	    break;
	}
    }
    return ;

}

template<class T>
bool BinaryNetwork<T>::check()
{

    Vertex v, u;
    typename GraphTraits::edge_iterator e_i, e_end;
    std::vector<VariableAbstract *> assignment;
    for (tie(e_i, e_end) = edges(G); e_i != e_end; ++e_i) {
	e = *e_i;
	v = source(e, G);
	u = target(e, G);
	assignment.push_back(&G[v]);
	assignment.push_back(&G[u]);
	if (! G[e].check(assignment)) {
	    assignment.clear();
	    return false;
	}
	assignment.clear();
    }
    return true;
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
