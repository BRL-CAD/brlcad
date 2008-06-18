/*              	     P C _ S O L V E R . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2012 United States Government as represented by
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
/** @file pc.h
 *
 *  Structures required for solving constraint networks
 *
 *@author Dawn Thomas
 */
#ifndef __PC_SOLVER_H__
#define __PC_SOLVER_H__

#define PC_MAX_STACK_SIZE 1000

#include <iostream>
#include <string>
#include <list>
#include <stack>

using namespace std;

/* Basic Exception Handling classes */

class pcException {
    private:
	string str;
    public:
	pcException() {};
	pcException(const char *temp) {str=temp;};
	~pcException() {};
	string Error() const {
	    return str;
	}
};

/*
    TODO: To be replaced by boost based classes ??
*/

template<class T>
class Interval {
	private:
		T low;
		T high;
		T step;
	public:
		Interval();
		Interval(const T l, const T h, const T s);
		void assign(const T l, const T h,const T s);
		void setLow(const T l);
		void setHigh(const T h);
		void setStep(const T s);
		T getLow();
		T getHigh();
		T getStep();
		T getWidth();
		
		bool inInterval(T);
		bool operator<(Interval<T> &U) const;
		bool operator>(Interval<T> &U) const;
};

template<class T>
class Domain

{
    private:
	list<Interval<T> > Interv; /* TODO: Change to Inheritance rather than membership */
	int mergeIntervals (typename list<Interval<T> >::iterator);
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

/* Interval Class Functions */

template<class T>
Interval<T>::Interval()
{

}

template<class T>
Interval<T>::Interval(T l, T h, T s)
{
    low = l;
    high = h;
    step = s;
}

template<class T>
void Interval<T>::assign(const T l, const T h, const T s)
{
	low = l;
	high = h;
	step = s;
}

template<class T>
void Interval<T>::setLow(const T l)
{
	low = l;
}

template<class T>
void Interval<T>::setHigh(const T h)
{
	high = h;
}

template<class T>
void Interval<T>::setStep(const T s)
{
	step = s;
}

template<class T>
T Interval<T>::getLow()
{
	return low;
}

template<class T>
T Interval<T>::getHigh()
{
        return high;
}

template<class T>
T Interval<T>::getStep()
{
        return step;
}

template<class T>
T Interval<T>::getWidth()
{
        return high-low;
}

template<class T>
bool Interval<T>::inInterval(T t)
{
    if (t>=low && t<=high)
	return true;
    else
	return false;
}

template<class T>
bool Interval<T>::operator<(Interval<T> &U) const
{
    /*  Ensure high<low etc. here?
	if(low<U.getLow()) {
	if(high<U.getLow())
	    return true;
	else {
	    return false;
	}
    } else 
    return false;*/
	if(low<U.getLow())
	    return true;
	else
	    return false;
}

template<class T>
bool Interval<T>::operator>(Interval<T> &U) const
{
	if(low>U.getLow())
	    return true;
	else
	    return false;
}

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
    typename list<Interval<T> >::iterator i = this->Interv.begin();
    while( i != this->Interv.end() && t > *i ) i++;
/*    if ( i == this->Interv.end()) this->Interv.push_back(t);
            else */this->Interv.insert(i,t);
    packIntervals();
}

template<class T>
Interval<T> Domain<T>::getInterval(T t) throw(pcException)
{
    typename list<Interval<T> >::iterator i;
    for(i = this->Interv.begin(); i != this->Interv.end(); i++)
	{
	    if ( i->inInterval(t)) return *i;
	}
    throw new pcException("Variable not in domain");
}

template<class T>
T Domain<T>::getNextLow (T value)
{
	typename list<Interval<T> >::iterator i = this->Interv.begin();
	while(i!=Interv.end() && i->getLow() < value) i++;
	return i->getLow();
}

template<class T>
void Domain<T>::display()
{
    typename list<Interval<T> >::iterator i;
    for(i = this->Interv.begin(); i != this->Interv.end(); i++)
	{
	    cout<<"!-- "<<i->getLow()<<" "<<i->getHigh()<<" "<<i->getWidth()<<" "<<i->getStep()<<endl;
	}
}


template<class T>
int Domain<T>::mergeIntervals (typename list<Interval<T> >::iterator i)
{
    if( i!=Interv.end() ) {
	typename list<Interval<T> >::iterator j = i;j++;
	if(j->getStep() == i->getStep()) {
	    if(j->getHigh() > i->getHigh()) /* If interval is not inside the present one */
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
    if(Interv.size()>1) {
	typename list<Interval<T> >::iterator i,j,temp;
	i=j=Interv.begin();j++;

	do {

/*	    display();cout<<"++++"<<i->getHigh() <<" "<< j->getLow()<<endl;*/
	    if (i->getHigh() > j->getLow() ) {
		if(mergeIntervals(i) !=0) {
		    cout<<"Error: Incompatible stepsizes"<<endl;
		    std::exit(-1);
		} else {
		    packIntervals();
		    break;
		}
	    } else {
		i++,j++;
		continue;
	    }
	} while(j != Interv.end());
	/*cout<<"!!!!!"<<j->getHigh()<<" "<<j->getLow()<<endl;
	cout<<"-----------------------------------"<<endl;*/
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
    if(I.inInterval(value)) return *this;
    else {
	value = D.getNextLow(value);
	return *this;
	}
}

/* TO BE REMOVED */
class Relation {
	public:
	private:
};

template<class T>
class Stack : public stack< T,list<T> > {
	public:
	    T pop() {
		T tmp = stack<T>::top();
	        stack<T>::pop();
	        return tmp;
	    }
};

class Constraint {
	public:
	
	private:
};


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
