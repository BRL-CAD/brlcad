/*                    P C S O L V E R . H
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
/** @file pcSolver.h
 *
 * various constraint solving methods
 *
 */
#ifndef __PCSOLVER_H__
#define __PCSOLVER_H__

#include "common.h"

#include <vector>

#include "pcBasic.h"
#include "pcVariable.h"
#include "pcConstraint.h"
#include "pcNetwork.h"

/** Base Solver Class */
class Solver
{
public:
    typedef std::list<VariableAbstract *> Varlist;
    Solver();
    unsigned long numChecks () { return num_checks_; }
    unsigned long numSolutions () { return num_solutions_; }
protected:
    unsigned long num_checks_;
    unsigned long num_solutions_;
    bool initiated_;
    bool solved_;
};


Solver::Solver()
    : num_checks_(0),
      num_solutions_(0),
      initiated_(false),
      solved_(false)
{}

/* Generic solver for VCSet */
template <typename T>
class PCSolver : public Solver
{
public:
    PCSolver();
    bool solve(VCSet &, Solution<T> &);
private:
    std::list<VariableAbstract *> vars_;
    bool generator();
    void initiate();
};


template <typename T>
PCSolver<T>::PCSolver()
    : Solver()
{}

template <typename T>
void PCSolver<T>::initiate() {
    if (!initiated_) {
	std::list<VariableAbstract *>::iterator i;
        for (i = vars_.begin(); i != vars_.end(); ++i) {
	    typedef Variable<T> *Vp;
	    (Vp (*i))->store();
	    //(Vp (*i))->setValue((Vp (*i))->getFirst());
        }
	initiated_ = true;
    }
}


template <typename T>
bool PCSolver<T>::generator() {
    std::list<VariableAbstract *>::iterator i, j, k;
    //    bool atend = true;
    i = vars_.begin();
    j = vars_.end();
    typedef Variable<T> *Vp;
    j--;
    //while (--j != i && (Vp (*j))->atUpperBoundary());
    while ((Vp (*j))->atCriticalBelow() && j != i) j--;
    
    /*k = j;
      for (; i != ++k; ++i)
      //if (! (Vp (*i))->atUpperBoundary())
      if (! (Vp (*i))->atCriticalBelow())
      atend = false;
      if (atend)*/
    if (i == j && (Vp (*i))->atCriticalBelow())
        return false;
    /* Increment one variable , set other variables to the first value */
    ++(*(Vp (*j)));
    while (++j != vars_.end())
	//(Vp (*j))->setValue((Vp (*j))->getFirst());
	(Vp (*j))->restore();
    return true;
}


template <typename T>
bool PCSolver<T>::solve(VCSet & vcset, Solution<T>& S) {
    std::list<VariableAbstract *>::iterator i = vcset.Vars.begin();
    std::list<VariableAbstract *>::iterator end = vcset.Vars.end();
    num_checks_ = 0;
    num_solutions_ = 0;
    solved_ = false;

    /** BEGIN DEBUG CODE */
    std::string av =""; /* Actual varying variables in the network */
    std::string ov = ""; /* Constant or non constrained variables */

    for (; i != end; ++i)
	if ((*i)->isConstrained() == 1 && ! (*i)->isConst()) {
	    av += (*i)->getID() + " ";
	    vars_.push_back(*i);
	    ((Variable<T> *) *i)->store();
	} else {
	    ov += (*i)->getID() + " ";
	}
    std::cout << std::endl << "Generic Constraint Solution" <<std::endl;
    std::cout << "Actual Variables: " << av << std::endl;
    std::cout << "Other Variables: " << ov << std::endl;
    /** END DEBUG CODE */

    initiate();
    while (generator()) {
	++num_checks_;
	/*std::cout << "Checking ";
	  std::list<VariableAbstract *>::iterator i = vcset.Vars.begin();
	  typedef Variable<T> *Vi;
	  for (; i != vcset.Vars.end(); ++i)
	  std::cout << (Vi (*i))->diff() << "\t";
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
class GTSolver : public Solver
{
    typedef typename boost::adjacency_list<boost::vecS, boost::vecS, \
					   boost::bidirectionalS, Variable<T>*, Constraint *> Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::vertex_descriptor Vertex;
    typedef typename GraphTraits::edge_descriptor Edge;
    typedef typename GraphTraits::vertex_iterator VertexIterator;

public:
    GTSolver();
    bool solve(BinaryNetwork<T>&, Solution<T>&);
private:
    std::list<VariableAbstract *> vars_;
    BinaryNetwork<T> *N;
    bool generator();
    void initiate();
};


template <typename T>
GTSolver<T>::GTSolver()
    : Solver()
{}

template <typename T>
void GTSolver<T>::initiate() {
    if (!initiated_) {
        typename GraphTraits::vertex_iterator v_i, v_end;
	for (tie(v_i, v_end) = vertices(N->G); v_i != v_end; ++v_i)
	    N->G[*v_i]->setValue(N->G[*v_i]->getFirst());
	initiated_ = true;
    }
}


template <typename T>
bool GTSolver<T>::generator() {
    VertexIterator vertex_v, vertex_u, vertex_end;
    tie(vertex_u, vertex_v) = vertices(N->G);
    vertex_end = vertex_v;
    while (--vertex_v != vertex_u && N->G[*vertex_v]->atUpperBoundary());
    
    if (N->G[*vertex_u]->getValue() == N->G[*vertex_v]->getLast())
        return false;
    /* Increment one variable , set other variables to the first value */
    ++(*(N->G[*vertex_v]));
    if (vertex_v != vertex_u) {
        ++vertex_v;
	while (vertex_v != vertex_end) {
	    N->G[*vertex_v]->setValue(N->G[*vertex_v]->getFirst());
	    ++vertex_v;
	}
    }
    return true;
}


template <typename T>
bool GTSolver<T>::solve(BinaryNetwork<T>& BN, Solution<T>& S) {
    typename GraphTraits::vertex_iterator v_i, v_end;
    N = &BN;
    num_checks_ = 0;
    num_solutions_ = 0;
    solved_ = true;

    for (tie(v_i, v_end) = vertices(N->G); v_i != v_end; ++v_i)
	vars_.push_back(N->G[*v_i]);

    initiate();
    while (generator()) {
	++num_checks_;
	if (N->check()) {
	    S.addSolution(vars_);
	    solved_ = true;
	    ++num_solutions_;
	}
    }
    return solved_;
}
/** Generic BackTracking Solver */

template <typename T>
class BackTrackSolver : public Solver
{
public:
    typedef Variable<T> *Vp;
    BackTrackSolver();
    bool solve(VCSet&, Solution<T>&);
private:
    std::map<VariableAbstract *, bool> labels;
    std::list<VariableAbstract *> vars_;
    VCSet *vcs;

    size_t labelsize();
    bool backtrack();
    bool check();
};


template <typename T>
BackTrackSolver<T>::BackTrackSolver()
    : Solver()
{}

template <typename T>
size_t BackTrackSolver<T>::labelsize() {
    int sum = 0;
    Varlist::iterator i = vars_.begin();
    Varlist::iterator end = vars_.end();
    for (; i != end; ++i) {
	if (labels[*i] == true) sum++;
    }
    return sum;
}


template <typename T>
bool BackTrackSolver<T>::check()
{
    bool labelsum = true;
    std::list<Constraint *>::iterator i = vcs->Constraints.begin();
    std::list<Constraint *>::iterator end = vcs->Constraints.end();
    for (; i != end; ++i) {
	typedef std::list<std::string> Vlist;
	Vlist variables = (*i)->getVariableList();
	Vlist::iterator j = variables.begin();	
	for (; j != variables.end(); ++j) {
	    Varlist::iterator k = vars_.begin();
	    bool varisrelevant = false;
	    for (; k != vars_.end(); ++k)
		if ((*j).compare((*k)->getID()) == 0)
		    varisrelevant = true;
	    if (varisrelevant && ! labels[vcs->getVariablebyID(*j)]) {
		labelsum = false;
		break;
	    }
	}
	if (labelsum) {
	    ++num_checks_;
	    if (!(*i)->check()) {
		return false;
	    }
	}
    }
    return true;
}


template <typename T>
bool BackTrackSolver<T>::backtrack()
{
    Varlist::iterator i = vars_.begin();

    if (labelsize() == vars_.size()) {
	return true;
    }
    while (labels[*i] && i != vars_.end())
	++i;
    
    labels[*i] = true;
    Vp vptr = Vp (*i);
    for (vptr->minimize(); ! vptr->atUpperBoundary(); ++(*vptr)) {
	if (check()) {
	    if (backtrack()) {
		return true;
	    }
	}
    }
    labels[*i] = false;
    return false;
}


template <typename T>
bool BackTrackSolver<T>::solve(VCSet & vcset, Solution<T>& S) {
    std::list<VariableAbstract *>::iterator i = vcset.Vars.begin();
    std::list<VariableAbstract *>::iterator end = vcset.Vars.end();
    num_checks_ = 0;
    num_solutions_ = 0;
    solved_ = false;
    vcs  = &vcset;

    std::string av =""; /* Actual varying variables in the network */
    std::string ov = ""; /* Constant or non constrained variables */    
    vcset.store();

    for (; i != end; ++i)
	if ((*i)->isConstrained() == 1 && ! (*i)->isConst()) {
	    av += (*i)->getID() + " ";
	    vars_.push_back(*i);
	    labels[*i] = false;
	    (Vp (*i))->minimize();
	} else {
	    ov += (*i)->getID() + " ";
	}

    std::cout << std::endl << "Gneric Backtracking Constraint Solution" <<std::endl;
    std::cout << "Actual Variables: " << av << std::endl;
    std::cout << "Other Variables: " << ov << std::endl;

    backtrack();
    if (vcset.check()) {
    	S.addSolution(vcset.Vars);
	vcset.restore();
	return true;
    }
    vcset.restore();
    return false;
}


/* BackTracking Solver Technique */
template <typename T>
class BTSolver : public Solver
{
    typedef typename boost::adjacency_list<boost::vecS, boost::vecS,
					   boost::bidirectionalS, Variable<T>*, Constraint *> Graph;
    typedef boost::graph_traits<Graph> GraphTraits;
    typedef typename GraphTraits::vertex_descriptor Vertex;
    typedef typename GraphTraits::edge_descriptor Edge;
    typename GraphTraits::vertex_iterator v_i, v_end;
    typename GraphTraits::edge_iterator e_i, e_end;

public:
    BTSolver();
    bool solve(class BinaryNetwork<T>& , Solution<T>&);
private:
    Vertex v, u;
    std::vector<bool> labels;
    class BinaryNetwork<T> *N;
    std::list<VariableAbstract *> vars_;

    size_t labelsize();
    bool backtrack();
    bool check();
};


template <typename T>
BTSolver<T>::BTSolver()
    : Solver()
{}

template <typename T>
size_t BTSolver<T>::labelsize() {
    int i=0;
    for (tie(v_i, v_end) = vertices(N->G); v_i != v_end; ++v_i)
	if (labels[*v_i] == true) i++;
    return i;
}


template <typename T>
bool BTSolver<T>::backtrack()
{
    typename GraphTraits::vertex_iterator ver_i, ver_end;

    if (labelsize() == num_vertices(N->G)) {
	return true;
    }
    tie(ver_i, ver_end) = vertices(N->G);
    while (labels[*ver_i] && ver_i!= ver_end)
	++ver_i;
    labels[*ver_i] = true;
    for (N->G[*ver_i]->minimize(); ! N->G[*ver_i]->atUpperBoundary(); \
	     ++*(N->G[*ver_i])) {
	if (check()) {
	    if (backtrack()) {
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
    for (tie(e_i, e_end) = edges(N->G); e_i != e_end; ++e_i) {
	v = source(*e_i, N->G);
	u = target(*e_i, N->G);
	if (labels[v] && labels[u]) {
	    ++num_checks_;
	    if (!N->G[*e_i]->check()) {
		return false;
	    }
	}
    }
    return true;
}


template <typename T>
bool BTSolver<T>::solve(class BinaryNetwork<T>& BN, Solution<T>& S) {
    N = &BN;
    num_checks_ = 0;
    for (tie(v_i, v_end) = vertices(N->G); v_i != v_end; ++v_i)
	vars_.push_back(N->G[*v_i]);

    for (tie(v_i, v_end) = vertices(N->G); v_i != v_end; ++v_i) {
	labels.push_back(false);
	N->G[*v_i]->setValue(N->G[*v_i]->getFirst());
    }
    backtrack();
    if (N->check()) {
    	S.addSolution(vars_);
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
