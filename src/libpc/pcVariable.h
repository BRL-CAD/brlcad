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

#define VAR_ABS 0
#define VAR_INT 1
#define VAR_DBL 2

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
public:
    /** Constructors and Destructor */
    Domain();
    ~Domain();
    Domain(T low,T high,T step);
    
    /** Data access methods */
    int size() { return Interv.size(); }
    Interval<T> getInterval(T) throw(pcException);
    T getFirst() { return Interv.front().getLow();}
    T getLast() { return Interv.back().getHigh(); }
    T getNextLow (T);

    /** Data modification methods */
    void addInterval(const Interval<T>);
    void addInterval(T, T, T);
    void intersectInterval(Interval<T>);
    
    /** Emptiness check */
    bool isEmpty();

    /** Disjoint check */
    bool isDisjoint();

    /** Uniqueness check */
    bool isUnique();

    /** Display method */
    void display();
private:
    std::list<Interval<T> > Interv;
    int mergeIntervals (typename std::list<Interval<T> >::iterator);
    void packIntervals ();
};

class VariableAbstract {
public:
    VariableAbstract(std::string vid ="");
    int getType() const { return type; }
    std::string getID() const { return id; }
    virtual void display();
protected:
    int type;
private:
    std::string  id;
    int constrained;
};

template<class T>
class Variable : public VariableAbstract {
public:
/* TODO: Implement functionality to search which domain the variable belongs to
   and increment according to the stepvalue of that domain */
    
    /* Constructor and Destructor */
    Variable(std::string vid = "" , T vvalue = 0);
    ~Variable();
    
    /* Data Access methods */
    T getValue() { return value; }
    T getFirst() { return D.getFirst(); }
    T getLast() { return D.getLast(); }
    void setValue(T t) { value = t; }

    /* Domain Modification methods */
    void addInterval(const Interval<T>);
    void addInterval(T low, T high, T step);
    void intersectInterval(Interval<T> t) { D.intersectInterval(t); }
    
    /* Variable display method */
    void display();

    Variable& operator++();
    /*Variable operator++(T) { value++; return this; }*/
private:
    T value;
    Domain<T> D;
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
void Domain<T>::addInterval(T low, T high, T step) {
    addInterval(Interval<T>(low,high,step));
}

template<class T>
void Domain<T>::intersectInterval(Interval<T> t)
{
    if (Interv.empty()) {
	addInterval(t);
    } else {
	typename std::list<Interval<T> >::iterator i = this->Interv.begin();
	while (i != Interv.end() && i->getHigh() < t.getLow() )
	    Interv.erase(i++);
	i->setLow(t.getLow());
	i = Interv.end();
	i--;
	while (i != Interv.begin() && i->getLow() > t.getHigh() )
	    Interv.erase(i--);
	i->setHigh(t.getHigh());	
    }
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
bool Domain<T>::isEmpty()
{
    if ( Interv.empty() )
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
    if ( Interv.size() == 1 && Interv.front().isUnique() )
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
    VariableAbstract(vid),
    value(vvalue)
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
void Variable<T>::addInterval(T low, T high, T step)
{
    D.addInterval(low, high, step);
};

template<class T>
void Variable<T>::display()
{
    std::cout << "!-- " << getID() << " = " << getValue() << "\tType value = "<< VariableAbstract::type << std::endl;
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
