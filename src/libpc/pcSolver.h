/*                    P C S O L V E R . H
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
/** @file pcSolver.h
 *
 * various constraint solving methods
 *
 * @author Dawn Thomas
 */
#ifndef __PCSOLVER_H__
#define __PCSOLVER_H__

#include <iostream>
#include <cstdlib>
#include <string>
#include <list>
#include <stack>

#include "pcBasic.h"
#include "pcVariable.h"
#include "pcConstraint.h"
#include "pcNetwork.h"

template<class T>
class BinaryNetwork;

/* Generate Test based Solver Technique */
using namespace boost;
template<class T>
class GTSolver
{
private:
	typedef typename boost::adjacency_list<vecS, vecS, bidirectionalS, 
	Variable<T>, Constraint<T> > Graph;
	typedef graph_traits<Graph> GraphTraits;
	typedef typename GraphTraits::vertex_descriptor Vertex;
	typedef typename GraphTraits::edge_descriptor Edge;
	typename GraphTraits::vertex_iterator v_i, v_end;

	long num_checks;
	bool initiated;
	class BinaryNetwork<T>* N;
	bool generator();
	void initiate();

public:
	GTSolver() { initiated = false; }
	bool solve(class BinaryNetwork<T>* , Solution<T>* );
	long numChecks () { return num_checks; }
};


template<class T>
void GTSolver<T>::initiate() {
	for(tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i)
	    N->G[*v_i].setValue(N->G[*v_i].getFirst());
}

template<class T>
bool GTSolver<T>::generator() {
	if ( !initiated ) {
	    initiate();
	    initiated = true;
	}
	else {
	    typename GraphTraits::vertex_iterator vertex_v, vertex_u,vertex_end;
	    tie(vertex_u,vertex_v) = vertices(N->G);
	    vertex_end = vertex_v;
	    --vertex_v;
	    while( vertex_v != vertex_u) {
		//std::cout<< N->G[*vertex_v].getValue() <<" ???"<< N->G[*vertex_v].getLast()<<std::endl;
		if( N->G[*vertex_v].getValue() == N->G[*vertex_v].getLast())
			--vertex_v;
		else break;
	    }

	    if(N->G[*vertex_u].getValue() == N->G[*vertex_v].getLast())
		return false;
	    /* Increment one variable and set the other variables to the first value */
	    ++(N->G[*vertex_v]);
	    if(true ||vertex_v != vertex_u) {
		while(vertex_v != vertex_end) {
		    ++vertex_v;
		    N->G[*vertex_v].setValue(N->G[*vertex_v].getFirst());
		}
	    }
	}
	return true;
}

template<class T>
bool GTSolver<T>::solve(class BinaryNetwork<T>* BN,Solution<T>* S) {
	N = BN;
	num_checks = 0;
	while(generator()) {
	 /*for(tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i)
	     std::cout<<N->G[*v_i].getValue();
	     std::cout<<std::endl;*/
	    ++num_checks;
	    if (N->check()) {
		for(tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i) {
		    S->VarDom.push_back(VarDomain<T>(N->G[*v_i],\
				Domain<T>(N->G[*v_i].getValue(),N->G[*v_i].getValue(),1)));
		}
		return true;
	    }
	}
	return false;
}

/* BackTracking Solver Technique */
using namespace boost;
template<class T>
class BTSolver
{
private:
	typedef typename boost::adjacency_list<vecS, vecS, bidirectionalS, 
	Variable<T>, Constraint<T> > Graph;
	typedef graph_traits<Graph> GraphTraits;
	typedef typename GraphTraits::vertex_descriptor Vertex;
	typedef typename GraphTraits::edge_descriptor Edge;
	typename GraphTraits::vertex_iterator v_i, v_end;
	typename GraphTraits::edge_iterator e_i, e_end;
	Vertex v,u;
	long num_checks;
	std::vector<bool> labels;
	class BinaryNetwork<T>* N;
	
	int labelsize() {
	    int i=0;
	    for(tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i)
		if (labels[*v_i] == true) i++;
	    return i;
	}
	bool backtrack();
	bool check();


public:
	bool solve(class BinaryNetwork<T>* , Solution<T>* );
	long numChecks() { return num_checks; }
};

template<class T>
bool BTSolver<T>::backtrack()
{
    //std::cout<<"-------------------------------"<<std::endl;
    bool nexttest = false;
    typename GraphTraits::vertex_iterator ver_i, ver_end;

    if (labelsize() == num_vertices(N->G)) {
	//std::cout<<"+------------------------------"<<std::endl;
	return true;
    }
    tie(ver_i,ver_end) = vertices(N->G);
    while(labels[*ver_i] && ver_i!= ver_end)
	++ver_i;
    labels[*ver_i] = true;
    //std::cout<<"Variable: "<<N->G[*ver_i].getID()<<std::endl;
    for(N->G[*ver_i].setValue(N->G[*ver_i].getFirst()); \
	    N->G[*ver_i].getValue() != N->G[*ver_i].getLast(); \
	    ++N->G[*ver_i]) {
	if (check()) {
	    //std::cout<<"Passed checking"<<std::endl;
	    //std::cout<<"==-----------------------------"<<std::endl;
	    nexttest = backtrack();
	    //std::cout<<"Backtrack returned "<<nexttest<<std::endl;
	    if(nexttest) {
		//std::cout<<"++-----------------------------"<<std::endl;
		return true;
		break;
	    }

	} /*else { 
	    labels[*v_i] = false;
	}*/
	//std::cout<<"Chance to Increment Variable: "<<N->G[*ver_i].getID()<< " from " << N->G[*ver_i].getValue()<<std::endl;
//	if(N->G[*v_i].getValue() == N->G[*v_i].getLast()) break;
    }
//    N->G[*v_i].setValue(N->G[*v_i].getFirst());
    labels[*ver_i] = false;
    //std::cout<<"Backtrack to previous"<<std::endl;
    //std::cout<<"+++----------------------------"<<std::endl;
    return false;
}

template<class T>
bool BTSolver<T>::check()
{
    std::vector<T> assignment;
    for(tie(e_i,e_end) = edges(N->G); e_i != e_end; ++e_i) {
	v = source(*e_i,N->G);
	u = target(*e_i,N->G);
	if(labels[v] && labels[u] ) {
	    //std::cout<<"---"<<N->G[*e_i].getExp()<<"  "<<N->G[v].getValue()<<","<<N->G[u].getValue()<<std::endl;
	    assignment.push_back(N->G[v].getValue());
	    assignment.push_back(N->G[u].getValue());
	    ++num_checks;
	    if (!N->G[*e_i].check(assignment)) {
		assignment.clear();
		//std::cout<<"Constraint unsolved"<<std::endl;
		return false;
	    }
	}
	assignment.clear();
    }
    //std::cout<<"Constraint solved"<<std::endl;
    return true;
}

template<class T>
bool BTSolver<T>::solve(class BinaryNetwork<T>* BN,Solution<T>* S) {
    N = BN;
    num_checks = 0;
    for(tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i) {
	labels.push_back(false);
	N->G[*v_i].setValue(N->G[*v_i].getFirst());
    }
    backtrack();
    if (N->check()) {
	for(tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i) {
	    S->VarDom.push_back(VarDomain<T>(N->G[*v_i],\
		Domain<T>(N->G[*v_i].getValue(),N->G[*v_i].getValue(),1)));
	}
	return true;
    }
    return false;
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
