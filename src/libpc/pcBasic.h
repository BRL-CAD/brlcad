/*                       P C B A S I C . H
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
/** @addtogroup libpc */
/** @{ */
/** @file pcBasic.h
 *
 * Basic classes for Parametrics and Constraints Library
 *
 * @author Dawn Thomas
 */
#ifndef __PCBASIC_H__
#define __PCBASIC_H__

#define PC_MAX_STACK_SIZE 1000

#include "common.h"

#include <iostream>
#include <cstdlib>
#include <string>
#include <list>
#include <stack>


/* Basic Exception Handling classes */

class pcException {
private:
    std::string str;
public:
    pcException() {};
    pcException(const char *temp) {str=temp;};
    ~pcException() {};
    std::string Error() const {
	return str;
    }
};

/* Structures for defining varios derived objects from
 * constrained_value templatein Boost
 */

struct is_even {
    bool operator () (int i) const
	{ return (i % 2) == 0; }
};

/* Classes for Graph Visualization */


/* TO BE REMOVED */
class Relation {
public:
private:
};

template<class T>
class Stack : public std::stack< T,std::list<T> > {
public:
    T pop() {
	T tmp = std::stack<T>::top();
	std::stack<T>::pop();
	return tmp;
    }
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
