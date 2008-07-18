/*                    P C P C S E T . H
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
/** @addtogroup pcparser */
/** @{ */
/** @file pcPCSet.h
 *
 * PCSet Class definition : A Set of Variables and Constraints
 *
 * @author Dawn Thomas
 */
#ifndef __PCPCSET_H__
#define __PCPCSET_H__

#include <list>
#include "pcVariable.h"
#include "pcConstraint.h"

/**
 * A wrapper class for a set of Variables and Constraints
 * After a hypergraph representation, this might get deprecated.
 * Parser Class outputs this as a result of parse() and Generator class uses
 * it for generation of the Constraint Network (presently Binary Network)
 *
 */
template <typename T>
class PCSet
{
public:
    std::list<Variable<T> > Vars;
    std::list<Constraint<T> > Constraints;
    void display();
};
    
template <typename T>
void PCSet<T>::display() {
    typename std::list<Variable<T> >::iterator i;
    std::cout<< "Variables:" << std::endl;
    for (i = Vars.begin(); i != Vars.end(); i++)
        i->display();
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
