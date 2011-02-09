/*                    P C I N T E R V A L . H
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
/** @file pcInterval.h
 *
 * Class definition of Interval for Constraint Solution
 *
 */
#ifndef __PCINTERVAL_H__
#define __PCINTERVAL_H__

#include "common.h"

#include "pcBasic.h"

template<class T>
class Interval {
public:
    /** Constructors */
    Interval();
    Interval(const T l, const T h, const T s);
    
    /** Data access & modification methods */
    void assign(const T l, const T h, const T s);
    void setLow(const T l);
    void setHigh(const T h);
    void setStep(const T s);
    T getLow();
    T getHigh();
    T getStep();
    T getWidth();

    /** Interval related checking functions */
    bool inInterval(T);
    bool isUnique();
    
    /** Operator overloading for Interval comparison */
    bool operator<(Interval<T> &U) const;
    bool operator>(Interval<T> &U) const;
private:
    T low;
    T high;
    T step;
};


/* Interval Class Functions */

template<class T>
Interval<T>::Interval()
{

}


template<class T>
Interval<T>::Interval(T l, T h, T s) : 
    low(l),
    high(h),
    step(s)
{
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
bool Interval<T>::isUnique()
{
    if (high == low)
	return true;
    else
	return false;
}


template<class T>
bool Interval<T>::operator<(Interval<T> &U) const
{
    /* Ensure high<low etc. here?
	if (low<U.getLow()) {
	if (high<U.getLow())
	return true;
	else {
	return false;
	}
	} else
	return false;*/
    if (low<U.getLow())
	return true;
    else
	return false;
}


template<class T>
bool Interval<T>::operator>(Interval<T> &U) const
{
    if (low>U.getLow())
	return true;
    else
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
