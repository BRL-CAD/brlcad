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
/** @addtogroup pcsolver */
/** @{ */
/** @file pcSolver.h
 *
 * various constraint solving methods
 *
 * @author Dawn Thomas
 */
#ifndef __PCSOLVER_H__
#define __PCSOLVER_H__

#include "common.h"

#include <vector>

#include "pcBasic.h"
#include "pcVariable.h"
#include "pcConstraint.h"
#include "pcNetwork.h"

/* Generic solver for VCSet */
template <typename T>
class PCSolver
{
public:
    PCSolver();
    bool solve(VCSet &, Solution<T> & );
    unsigned long numChecks () { return num_checks_; }
    unsigned long numSolutions () { return num_solutions_; }
private:
    unsigned long num_checks_;
    unsigned long num_solutions_;
    bool initiated_;
    bool solved_;
    std::list<VariableAbstract *> vars_;
    bool generator();
    void initiate();
};

template <typename T>
PCSolver<T>::PCSolver()
    : num_checks_(0),
      num_solutions_(0),
      solved_(false),
      initiated_(false)
{}

template <typename T>
void PCSolver<T>::initiate() {
    std::list<VariableAbstract *>::iterator i;
    for (i = vars_.begin(); i != vars_.end(); ++i) {
	typedef Variable<T> * Vp;
	(Vp (*i))->setValue((Vp (*i))->getFirst());
    }
    initiated_ = true;
}

template <typename T>
bool PCSolver<T>::generator() {
    if (!initiated_) {
	initiate();
    } else {
	std::list<VariableAbstract *>::iterator i,j;
	bool atend = true;
	i = vars_.begin();
	j = vars_.end();
	typedef Variable<T> * Vp;
	while (--j != i && (Vp (*j))->atUpperBoundary());

	for (; i != vars_.end(); ++i)
	    if (! (Vp (*i))->atUpperBoundary())
		atend = false;
	if (atend)
	    return false;
	/* Increment one variable , set other variables to the first value */
	++(*(Vp (*j)));
	
	++j;
	while (j != vars_.end()) {
	    (Vp (*j))->setValue((Vp (*j))->getFirst());
	    ++j;
	}
    }
    return true;
}

template <typename T>
bool PCSolver<T>::solve(VCSet & vcset, Solution<T>& S) {
    std::list<VariableAbstract *>::iterator i = vcset.Vars.begin();
    std::list<VariableAbstract *>::iterator end = vcset.Vars.end();
    num_checks_ = 0;
    num_solutions_ = 0;
    solved_ = false;
    
    for (; i != end; ++i)
	if ( (*i)->isConstrained() == 1 && ! (*i)->isConst()) {
	    (*i)->display();
	    vars_.push_back(*i);
	}
    while (generator()) {
	++num_checks_;
	/* std::cout << "Checking ";
	std::list<VariableAbstract *>::iterator i = vcset.Vars.begin();
	typedef Variable<int> * Vi;
	for (; i != vcset.Vars.end(); ++i)
	    std::cout <<  (Vi (*i))->getValue() << "\t";
	std::cout << std::endl;*/

	if (vcset.check()) {
	    S.addSolution(vcset.Vars);
	    solved_ = true;
	    num_solutions_++;
	}
    }
    return solved_;
}

/* Generate Test based Solver Technique */
template <typename T>
class GTSolver
{
    typedef typename boost::adjacency_list<boost::vecS, boost::vecS,\
	    	    boost::bidirectionalS, Variable<T>*, Constraint *> Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::vertex_descriptor Vertex;
    typedef typename GraphTraits::edge_descriptor Edge;
    typename GraphTraits::vertex_iterator v_i, v_end;

public:
    GTSolver() : initiated(false) { }
    bool solve(BinaryNetwork<T>&, Solution<T>& );
    long numChecks () { return num_checks; }
private:
    long num_checks;
    bool initiated;
    BinaryNetwork<T>* N;
    bool generator();
    void initiate();
};


template <typename T>
void GTSolver<T>::initiate() {
    for (tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i)
	N->G[*v_i]->setValue(N->G[*v_i]->getFirst());
    initiated = true;
}

template <typename T>
bool GTSolver<T>::generator() {
    if (!initiated) {
	initiate();
    } else {
	typename GraphTraits::vertex_iterator vertex_v, vertex_u,vertex_end;
	tie(vertex_u,vertex_v) = vertices(N->G);
	vertex_end = vertex_v;
	--vertex_v;
	while (vertex_v != vertex_u) {
	    if (N->G[*vertex_v]->getValue() == N->G[*vertex_v]->getLast())
		--vertex_v;
	    else
		break;
	}

	if (N->G[*vertex_u]->getValue() == N->G[*vertex_v]->getLast())
	    return false;
	/* Increment one variable , set other variables to the first value */
	++(*(N->G[*vertex_v]));
	if (true ||vertex_v != vertex_u) {
	    ++vertex_v;
	    while (vertex_v != vertex_end) {
		N->G[*vertex_v]->setValue(N->G[*vertex_v]->getFirst());
		++vertex_v;
	    }
	}
    }
    return true;
}

template <typename T>
bool GTSolver<T>::solve(BinaryNetwork<T>& BN, Solution<T>& S) {
    N = &BN;
    num_checks = 0;
    while (generator()) {
	++num_checks;
	if (N->check()) {
	    for (tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i) {
		S.insert(N->G[*v_i]);
	    }
	    return true;
	}
    }
    return false;
}

/* BackTracking Solver Technique */
template <typename T>
class BTSolver
{
    typedef typename boost::adjacency_list<boost::vecS, boost::vecS,
		    boost::bidirectionalS, Variable<T>*, Constraint *> Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::vertex_descriptor Vertex;
    typedef typename GraphTraits::edge_descriptor Edge;
    typename GraphTraits::vertex_iterator v_i, v_end;
    typename GraphTraits::edge_iterator e_i, e_end;

public:
    bool solve(class BinaryNetwork<T>& , Solution<T>& );
    long numChecks() { return num_checks; }

private:
    Vertex v,u;
    long num_checks;
    std::vector<bool> labels;
    class BinaryNetwork<T>* N;

    int labelsize();
    bool backtrack();
    bool check();
};

template <typename T>
int BTSolver<T>::labelsize() {
    int i=0;
    for (tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i)
	if (labels[*v_i] == true) i++;
    return i;
}

template <typename T>
bool BTSolver<T>::backtrack()
{
    bool nexttest = false;
    typename GraphTraits::vertex_iterator ver_i, ver_end;

    if (labelsize() == num_vertices(N->G)) {
	return true;
    }
    tie(ver_i,ver_end) = vertices(N->G);
    while (labels[*ver_i] && ver_i!= ver_end)
	++ver_i;
    labels[*ver_i] = true;
    for (N->G[*ver_i]->setValue(N->G[*ver_i]->getFirst()); \
	     N->G[*ver_i]->getValue() != N->G[*ver_i]->getLast(); \
	     ++*(N->G[*ver_i])) {
	if (check()) {
	    nexttest = backtrack();
	    if (nexttest) {
		return true;
	    }
	}
    }
    labels[*ver_i] = false;
    return false;
}

template <typename T>
bool BTSolver<T>::check()
{
    for (tie(e_i,e_end) = edges(N->G); e_i != e_end; ++e_i) {
	v = source(*e_i,N->G);
	u = target(*e_i,N->G);
	if (labels[v] && labels[u] ) {
	    ++num_checks;
	    if (!N->G[*e_i]->check()) {
		return false;
	    }
	}
    }
    return true;
}

template <typename T>
bool BTSolver<T>::solve(class BinaryNetwork<T>& BN,Solution<T>& S) {
    N = &BN;
    num_checks = 0;
    for (tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i) {
	labels.push_back(false);
	N->G[*v_i]->setValue(N->G[*v_i]->getFirst());
    }
    backtrack();
    if (N->check()) {
	for (tie(v_i,v_end) = vertices(N->G); v_i != v_end; ++v_i) {
	    S.insert(N->G[*v_i]);
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
