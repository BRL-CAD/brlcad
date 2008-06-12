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

/*
    To be replaced by boost based classes
*/

using namespace std;

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
