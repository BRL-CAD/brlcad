/*                    P C V A R I A B L E . H
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
/** @file pcVariable.h
 *
 * Class definition of Variable & Domain
 *
 */
#ifndef __PCVARIABLE_H__
#define __PCVARIABLE_H__

#define VAR_ABS 0
#define VAR_INT 1
#define VAR_DBL 2

#include "common.h"

#include <iostream>
#include <cstdlib>
#include <string>
#include <list>
#include <vector>

#include <float.h>
#ifndef DBL_MAX
#  include <limits>
#endif

#include "vmath.h"
#include "pcBasic.h"
#include "pcInterval.h"

#include <boost/iterator/indirect_iterator.hpp>

template<class T>
class Domain
{
public:
    typedef std::list<Interval<T> > IntervalList;
    typedef typename IntervalList::size_type size_type;
    typedef typename IntervalList::difference_type difference_type;
    typedef typename boost::indirect_iterator
    <typename IntervalList::iterator> iterator;
    typedef typename boost::indirect_iterator
    <typename IntervalList::const_iterator> const_iterator;

    /** Constructors and Destructor */
    Domain() {}
    ~Domain() {}
    Domain(T low, T high, T step);
    
    /** Data access methods */
    int size() { return Interv.size(); }
    Interval<T> & getInterval(T) throw(pcException);
    Interval<T> & getNextInterval(T) throw (pcException);
    Interval<T> & getPreviousInterval(T) throw (pcException);
    T getFirst() { return Interv.front().getLow();}
    T getLast() { return Interv.back().getHigh(); }
    T getNextLow (T);

    /** Data modification methods */
    void addInterval(const Interval<T>);
    void addInterval(T, T, T);
    void intersectInterval(Interval<T>);
    
    /** Iterator methods */
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    iterator erase(iterator location);
    iterator erase(iterator begin, iterator end);

    iterator insert(iterator location, Interval<T> *I);
    
    /** Emptiness check */
    bool isEmpty();

    /** Disjoint check */
    bool isDisjoint();

    /** Uniqueness check */
    bool isUnique();

    /** Comparison */
    bool operator==(Domain<T> & d) {
	if (Interv == d.Interv) return true;
	else return false; } 
    /** Display method */
    void display();
private:
    IntervalList Interv;
    int mergeIntervals (typename std::list<Interval<T> >::iterator);
    void packIntervals ();
};


class VariableAbstract {
public:
    VariableAbstract(std::string vid ="");
    virtual ~VariableAbstract();

    int getType() const { return type; }
    std::string getID() const { return id; }
    virtual void display();
    virtual void store();
    virtual void restore();
    void setConst() { const_ = true; }
    void setVar() { const_ = false; }
    bool isConst() { return const_; }
    int isConstrained() { return constrained_; }
    void setConstrained(int n) { constrained_ = n; }

protected:
    int type;
    bool const_;
private:
    std::string id;
    int constrained_;
};


template<class T>
class Variable : public VariableAbstract {
public:
/* TODO: Implement functionality to search which domain the variable belongs to
   and increment according to the stepvalue of that domain */
    
    /* Constructor and Destructor */
    Variable(std::string vid = "" , T vvalue = 0);
    virtual ~Variable();
    
    /* Data Access methods */
    T getValue() { return value; }
    T getFirst() { return D.getFirst(); }
    T getLast() { return D.getLast(); }
    T getStep() { return D.getInterval(value).getStep(); }
    void setValue(T t) { value = t; }
    void minimize() { value = D.getFirst(); }
    void maximize() { value = D.getLast(); }
    
    /* Value storing and restoring methods to assist in iteration */
    void store() { vcopy_ = value; }
    void restore() { value = vcopy_; }
    T diff() { return vcopy_ - value; }

    /* Domain Modification methods */
    void addInterval(const Interval<T>);
    void addInterval(T low, T high, T step);
    void intersectInterval(Interval<T> t) { D.intersectInterval(t); }

    /* value location in domain check methods */
    bool atUpperBoundary();
    bool atLowerBoundary(); 
    bool atCriticalBelow();
    bool atCriticalAbove();

    /* Variable display method */
    void display();
    
    Variable& operator++();
    /*Variable operator++(T) { value++; return this; }*/
private:
    T value;
    T vcopy_;
    Domain<T> D;
};


template<class T>
class Solution {
public:
    typedef std::list<VariableAbstract *> VarSet;
    typedef std::list<Domain<T> > Domains;
    typedef std::list<Domains> DomSet;

    void insert(VariableAbstract *v);
    void display();
    void cdisplay();
    void clear();
    bool addSolution(VarSet & V);
private:
    VarSet Varset_;
    DomSet Domset_;
};


template<class T>
class SearchStructure {

};


/* Domain Class Functions */
template<class T>
Domain<T>::Domain(T low, T high, T step) {
    addInterval(Interval<T>(low, high, step));
}


template<class T>
void Domain<T>::addInterval(const Interval<T> t)
{
    typename std::list<Interval<T> >::iterator i = this->Interv.begin();
    while (i != this->Interv.end() && t > *i)
	i++;
/*
  if (i == this->Interv.end())
  this->Interv.push_back(t);
  else
*/
    this->Interv.insert(i, t);
    packIntervals();
}


template<typename T>
typename Domain<T>::iterator
makeIterator(typename Domain<T>::IntervalList::iterator i)
{
    return boost::make_indirect_iterator(i);
}


template<typename T>
typename Domain<T>::const_iterator
makeIterator(typename Domain<T>::IntervalList::const_iterator i)
{
    return boost::make_indirect_iterator(i);
}


template<typename T>
typename Domain<T>::iterator
Domain<T>::begin()
{
    return makeIterator(Interv.begin());
}


template<typename T>
typename Domain<T>::iterator
Domain<T>::end()
{
    return makeIterator(Interv.end());
}


template<typename T>
typename Domain<T>::const_iterator
Domain<T>::begin() const
{
    return makeIterator(Interv.begin());
}


template<typename T>
typename Domain<T>::const_iterator
Domain<T>::end() const
{
    return makeIterator(Interv.end());
}


template<typename T>
typename Domain<T>::iterator
Domain<T>::erase(iterator location)
{
    return makeIterator(Interv.erase(location.base()));
}


template<typename T>
typename Domain<T>::iterator
Domain<T>::erase(iterator begini, iterator endi)
{
    return makeIterator(Interv.erase(begini.base(), endi.base()));
}


template<typename T>
typename Domain<T>::iterator
Domain<T>::insert(iterator location, Interval<T> *I)
{
    assert(I);
    return makeIterator(Interv.insert(location.base(), I));
}


template<class T>
void Domain<T>::addInterval(T low, T high, T step) {
    addInterval(Interval<T>(low, high, step));
}


template<class T>
void Domain<T>::intersectInterval(Interval<T> t)
{
    if (Interv.empty()) {
	addInterval(t);
    } else {
	typename std::list<Interval<T> >::iterator i = this->Interv.begin();
	while (i != Interv.end() && i->getHigh() < t.getLow())
	    Interv.erase(i++);
	i->setLow(t.getLow());
	i = Interv.end();
	i--;
	while (i != Interv.begin() && i->getLow() > t.getHigh())
	    Interv.erase(i--);
	i->setHigh(t.getHigh());
	i->setStep(t.getStep());
    }
}
template<class T>
Interval<T> & Domain<T>::getInterval(T t) throw(pcException)
{
    typename std::list<Interval<T> >::iterator i;
    for (i = this->Interv.begin(); i != this->Interv.end(); i++) {
	if (i->inInterval(t)) return *i;
    }
    throw new pcException("Variable not in domain");
}
/*
  template<class T>
  Interval<T> & Domain<T>::getPreviousInterval(T t) throw(pcException)
  {
  typename std::list<Interval<T> >::iterator i;
  for (i = this->Interv.begin(); i != this->Interv.end(); i++) {
  if (i->inInterval(t))
  break;
  }
  if (*i == Interv.front())
  return Interv.back();
  else {
  i--;
  return *i;
  }
  throw new pcException("Variable not in domain");
  }


  template<class T>
  Interval<T> & Domain<T>::getNextInterval(T t) throw(pcException)
  {
  typename std::list<Interval<T> >::iterator i;
  for (i = this->Interv.begin(); i != this->Interv.end(); i++) {
  if (i->inInterval(t))
  break;
  }
  if (*i == Interv.back())
  return Interv.front();
  else {
  i++;
  return *i;
  }
  throw new pcException("Variable not in domain");    
  }
*/
template<class T>
T Domain<T>::getNextLow (T value)
{
    typename std::list<Interval<T> >::iterator i = this->Interv.begin();
    while (i!=Interv.end() && i->getLow() < value) i++;
    if (i == Interv.end())
	return Interv.begin()->getLow();
    else
	return i->getLow();
}


template<class T>
bool Domain<T>::isEmpty()
{
    if (Interv.empty())
	return true;
    else
	return false;
}


template<class T>
bool Domain<T>::isDisjoint()
{
    if (Interv.size() == 1)
	return false;
    else
	return true;
}


template<class T>
bool Domain<T>::isUnique()
{
    if (Interv.size() == 1 && Interv.front().isUnique())
	return true;
    else
	return false;
}


template<class T>
void Domain<T>::display()
{
    typename std::list<Interval<T> >::iterator i;
    for (i = this->Interv.begin(); i != this->Interv.end(); i++) {
	std::cout << "!-- " << i->getLow() << " " << i->getHigh() << " "
		  << i->getWidth() << " " << i->getStep() << std::endl;
    }
}


template<class T>
int Domain<T>::mergeIntervals (typename std::list<Interval<T> >::iterator i)
{
    if (i!=Interv.end()) {
	typename std::list<Interval<T> >::iterator j = i;j++;
	if (ZERO(j->getStep() - i->getStep())) {
	    /* If interval is not inside the present one */
	    if (j->getHigh() > i->getHigh())
		i->setHigh(j->getHigh());
	    Interv.erase(j);
	    return 0;
	} else {
	    return -2;
	}
    } else {
	return 0;
    }
}


template<class T>
void Domain<T>::packIntervals ()
{
    if (Interv.size()>1) {
	typename std::list<Interval<T> >::iterator i, j, temp;
	i=j=Interv.begin();j++;

	do {
	    if (i->getHigh() > j->getLow()) {
		if (mergeIntervals(i) !=0) {
		    std::cout << "Error: Incompatible stepsizes" << std::endl;
		    std::exit(-1);
		} else {
		    packIntervals();
		    break;
		}
	    } else {
		i++, j++;
		continue;
	    }
	} while (j != Interv.end());
    }
}


/**
 * Variable Class methods
 */

template<class T>
Variable<T>::Variable(std::string vid, T vvalue)
    : VariableAbstract(vid),
      value(vvalue),
      vcopy_(vvalue)
{
}


template<class T>
Variable<T>::~Variable()
{
}


template<class T>
Variable<T>& Variable<T>::operator++()
{
    Interval<T> I;
    I = this->D.getInterval(value);
    value +=I.getStep();
    if (I.inInterval(value)) {
	return *this;
    } else {
	/* Temporary? fix for the boundary problem by not assigning
	   Boundary values */
	value = D.getNextLow(value) + I.getStep();
	return *this;
    }
}


template<class T>
void Variable<T>::addInterval(const Interval<T> t)
{
    D.addInterval(t);
}


template<class T>
void Variable<T>::addInterval(T low, T high, T step)
{
    D.addInterval(low, high, step);
}


template<class T>
void Variable<T>::display()
{
    std::cout << "!-- " << getID() << " = " << getValue() 
	      << "\tType = "<< VariableAbstract::type
	      << "\tConstant = "<< VariableAbstract::const_
	      << std::endl;
    D.display();
}


template<class T>
bool Variable<T>::atUpperBoundary()
{
    T st = D.getInterval(getLast()).getStep();
    if (getLast() - value < st)
	return true;
    else
	return false;
}


template<class T>
bool Variable<T>::atLowerBoundary()
{
    T st = D.getInterval(getFirst()).getStep();
    if (value - getFirst() < st)
    	return true;
    else
	return false;
}


template<class T>
bool Variable<T>::atCriticalBelow()
{
    T st = D.getInterval(vcopy_).getStep();
    if (vcopy_ - value < st && vcopy_ -value > 0.0)
	return true;
    else
	return false;
}


template<class T>
bool Variable<T>::atCriticalAbove()
{
    T st = D.getInterval(vcopy_).getStep();
    if (value - vcopy_ < st)
	return true;
    else
	return false;
}


/* Solution Class Functions */

template<typename T>
void Solution<T>::insert(VariableAbstract *v)
{
    Variable<T> *vt = (Variable<T> *) v;
    Domain<T> dom(vt->getValue(), vt->getValue(), vt->getStep());
    Varset_.push_back(v);
    //Domset_.back().push_back(dom);
}


template<class T>
bool Solution<T>::addSolution(VarSet & V)
{
    if (Varset_.empty())
	Varset_ = V;
    else if (Varset_ != V) {
	/** @todo throw exception */
	std::cout << "!-- Incompatible solutions : Variable set mismatch\n";
	return false;
    }
    VarSet::iterator i = Varset_.begin();
    typename DomSet::iterator j = Domset_.begin();

    Domains D;

    for (; i != Varset_.end(); ++i) {
	Variable<T> *vt = (Variable<T> *) *i;
	Domain<T> dom(vt->getValue(), vt->getValue(), vt->getStep());
	D.push_back(dom);
    }

    /** No check performed to see if solution already exists */
    Domset_.push_back(D);

    return true;
}
template<class T>
void Solution<T>::display()
{
    VarSet::iterator i;
    typename DomSet::iterator j;
    typename Domains::iterator k;
    for (i = Varset_.begin(); i != Varset_.end(); i++) {
	if (*i) std::cout << (*i)->getID() << "\t";
    }
    std::cout << std::endl;
    if (!Domset_.empty()) {
	for (j = Domset_.begin(); j != Domset_.end(); ++j) {
	    for (k = j->begin(); k != j->end(); ++k) {
		std::cout << k->getFirst() << "\t";
	    }
	    std::cout << std::endl;
	}
	std::cout << std::endl;
    }
    std::cout << "Number of Solutions: " << Domset_.size() << std::endl;
}


template<class T>
void Solution<T>::cdisplay()
{
    if (Domset_.size() < 2)
	display();
    else {
	size_t l;
	VarSet::iterator i;
	typename DomSet::iterator j;
        typename Domains::iterator k;
	std::vector<double> minmax;
	
        std::cout << "Solution Ranges are shown due to existance of non-unique solutions." << std::endl;
	for (i = Varset_.begin(); i != Varset_.end(); i++) {
	    if (*i) std::cout << (*i)->getID() << "\t";
	}
        std::cout << std::endl;
	
	if (!Domset_.empty()) {
	    j = Domset_.begin();
	    for (k = j->begin(), l=0; k != j->end(); ++k, ++l) {
	        minmax.push_back(k->getFirst());
	        minmax.push_back(k->getFirst());
	    }
	    
	    for (j = Domset_.begin(); j != Domset_.end(); ++j) {
		for (k = j->begin(), l = 0; k != j->end(); ++k, ++l) {
		    T value = k->getFirst();
		    minmax[2*l] = (value < minmax[2*l])?value:minmax[2*l];
		    minmax[2*l+1] = (value > minmax[2*l])?value:minmax[2*l+1];
		}
	    }
	}
	for (l = 0; l < minmax.size()/2; ++l) {
	    std::cout << minmax[2*l] << "\t";
	}
	std::cout << std::endl;

	for (l = 0; l < minmax.size()/2; ++l) {
	    if (!ZERO(minmax[2*l] - minmax[2*l+1])) /* TODO: needs proper tolerancing */
		std::cout << "to" << "\t";
	}
	std::cout << std::endl;
    
	for (l = 0; l < minmax.size()/2; ++l) {
	    if (!ZERO(minmax[2*l] - minmax[2*l+1])) /* TODO: needs proper tolerancing */
		std::cout << minmax[2*l+1] << "\t";
	}
    
	std::cout << std::endl << "Number of Solutions: " << Domset_.size() << std::endl;
    }
}


template<typename T>
void Solution<T>::clear()
{
    Varset_.clear();
    Domset_.clear();
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
