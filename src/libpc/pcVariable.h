/*                    P C V A R I A B L E . H
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
/** @file pcVariable.h
 *
 * Class definition of Variable & Domain
 *
 * @author Dawn Thomas
 */
#ifndef __PCVARIABLE_H__
#define __PCVARIABLE_H__

#include "common.h"

#include <iostream>
#include <cstdlib>
#include <limits>
#include <string>
#include <list>

#include "pcBasic.h"
#include "pcInterval.h"


template<class T>
class Domain
{
private:
    std::list<Interval<T> > Interv;
    int mergeIntervals (typename std::list<Interval<T> >::iterator);
    void packIntervals ();
public:
    Domain();
    ~Domain();
    Domain(T low,T high,T step);
    int size() { return Interv.size(); }
    Interval<T> getInterval(T) throw(pcException);
    T getFirst() { return Interv.front().getLow();}
    T getLast() { return Interv.back().getHigh(); }
    T getNextLow (T);
    void addInterval(const Interval<T>);
    void display();
};

class VariableAbstract {
};

template<class T>
class Variable : public VariableAbstract {
private:
    std::string  id;
    T value;
    Domain<T> D;
public:
/* Implement functionality to search which domain the variable belongs to
   and increment according to the stepvalue of that domain */
    int constrained;
    Variable(std::string id = "" , T value = 0);
    ~Variable();
    void addInterval(const Interval<T>);
    void setValue(T t) { value = t; }
    std::string getID() const { return id; }
    T getValue() { return value; }
    T getFirst() { return D.getFirst(); }
    T getLast() { return D.getLast(); }
    void display();

    Variable& operator++();
    /*Variable operator++(T) { value++; return this; }*/
};

template<class T>
class VarDomain {
public:
    Variable<T> V;
    Domain<T> D;
    VarDomain() {};
    ~VarDomain() {};
    VarDomain(Variable<T> V,Domain<T> D);
};

template<class T>
class Solution {
private:
public:
    std::list<VarDomain<T> > VarDom;
    void display();
    void clear();
};

template<class T>
class SearchStructure {

};

/* Domain Class Functions */
template<class T>
Domain<T>::Domain()
{

}

template<class T>
Domain<T>::~Domain()
{

}

template<class T>
Domain<T>::Domain(T low,T high,T step) {
    addInterval(Interval<T>(low, high, step) );
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
    this->Interv.insert(i,t);
    packIntervals();
}

template<class T>
Interval<T> Domain<T>::getInterval(T t) throw(pcException)
{
    typename std::list<Interval<T> >::iterator i;
    for (i = this->Interv.begin(); i != this->Interv.end(); i++) {
	if (i->inInterval(t)) return *i;
    }
    throw new pcException("Variable not in domain");
}

template<class T>
T Domain<T>::getNextLow (T value)
{
    typename std::list<Interval<T> >::iterator i = this->Interv.begin();
    while (i!=Interv.end() && i->getLow() < value) i++;
    if(  i == Interv.end() )
	return Interv.begin()->getLow();
    else
	return i->getLow();
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
	if (j->getStep() == i->getStep()) {
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
void  Domain<T>::packIntervals ()
{
    if (Interv.size()>1) {
	typename std::list<Interval<T> >::iterator i,j,temp;
	i=j=Interv.begin();j++;

	do {

/*	    display();std::cout << "++++" << i->getHigh() << " " << j->getLow() << std::endl;*/
	    if (i->getHigh() > j->getLow() ) {
		if (mergeIntervals(i) !=0) {
		    std::cout << "Error: Incompatible stepsizes" << std::endl;
		    std::exit(-1);
		} else {
		    packIntervals();
		    break;
		}
	    } else {
		i++,j++;
		continue;
	    }
	} while (j != Interv.end());
	/*std::cout << "!!!!!" << j->getHigh() << " " << j->getLow() << std::endl;
	  std::cout << "-----------------------------------" << std::endl;*/
	return;
    }
}

/**
 * Variable Class methods
 */

template<class T>
Variable<T>::Variable(std::string vid, T vvalue) :
    constrained(0),
    value(vvalue),
    id(vid)
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
	value = D.getNextLow(value);
	return *this;
    }
};

template<class T>
void Variable<T>::addInterval(const Interval<T> t)
{
    D.addInterval(t);
};

template<class T>
void Variable<T>::display()
{
    std::cout << "!-- " << getID() << " = " << getValue() << std::endl;
    D.display();
}
/* Solution Class Functions */
template <class T>
VarDomain<T>::VarDomain(Variable<T> Variable,Domain<T> Domain) {
    V = Variable;
    D = Domain;
}

template<class T>
void Solution<T>::display()
{
    typename std::list<VarDomain<T> >::iterator i;
    for (i = VarDom.begin(); i != VarDom.end(); i++) {
	i->V.display();
	//i->D.display();
    }
}

template<class T>
void Solution<T>::clear()
{
    VarDom.clear();
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
