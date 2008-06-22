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
/** @addtogroup soln */
/** @{ */
/** @file pcVariable.h
 *
 * Class definition of Variable & Domain
 *
 * @author Dawn Thomas
 */
#ifndef __PCVARIABLE_H__
#define __PCVARIABLE_H__

#include <iostream>
#include <cstdlib>
#include <string>
#include <list>
#include <stack>
#include "pcBasic.h"
#include "pcInterval.h"

template<class T>
class Domain

{
private:
    std::list<Interval<T> > Interv; /* TODO: Change to Inheritance rather than membership */
    int mergeIntervals (typename std::list<Interval<T> >::iterator);
    void packIntervals ();
public:
    /*class iterator
      : public std::iterator<std::bidirectional_iterator_tag,T> {
      public:
      iterator();
      ~iterator();
      iterator begin() { return this->begin(); }
      iterator end()  { return this->end(); }
      bool operator==(const iterator&);
      bool operator!=(const iterator&);
      };
      class const_iterator
      : public std::iterator<std::bidirectional_iterator_tag,T> {
      public:
      const_iterator();
      ~const_iterator();
      };
    */
    Domain();
    ~Domain();
    int size() { return Interv.size(); }
    Interval<T> getInterval(T) throw(pcException);
    T getNextLow (T);
    void addInterval(const Interval<T>);
    void display();
};

template<class T>
class Variable {
private:
    T value;
public:
/* Implement functionality to search which domain the variable belongs to
   and increment according to the stepvalue of that domain */
    Domain<T> D;
    Variable();
    ~Variable();
    void setValue(T t) { value = t; }
    T getValue() { return value; }
    Variable& operator++();
    /*Variable operator++(T) { value++; return this; }*/
};

template<class T>
class Solution {

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
    return i->getLow();
}

template<class T>
void Domain<T>::display()
{
    typename std::list<Interval<T> >::iterator i;
    for (i = this->Interv.begin(); i != this->Interv.end(); i++) {
	std::cout<<"!-- "<<i->getLow()<<" "<<i->getHigh()<<" "<<i->getWidth()<<" "<<i->getStep()<<std::endl;
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

/*	    display();std::cout<<"++++"<<i->getHigh() <<" "<< j->getLow()<<std::endl;*/
	    if (i->getHigh() > j->getLow() ) {
		if (mergeIntervals(i) !=0) {
		    std::cout<<"Error: Incompatible stepsizes"<<std::endl;
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
	/*std::cout<<"!!!!!"<<j->getHigh()<<" "<<j->getLow()<<std::endl;
	  std::cout<<"-----------------------------------"<<std::endl;*/
	return;
    }
}

/* Variable Class Functions */
template<class T>
Variable<T>::Variable()
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
