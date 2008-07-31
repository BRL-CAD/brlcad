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
/** @addtogroup libpc */
/** @{ */
/** @file pcPCSet.h
 *
 * PCSet Class definition : A Set of Variables and Constraints
 *
 * @author Dawn Thomas
 */
#ifndef __PCPCSET_H__
#define __PCPCSET_H__

#include "common.h"

#include <string>
#include <iostream>
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
class Constraint;

class PCSet
{
private:
    std::string name;
    double value;
public:
    std::list<VariableAbstract *> Vars;
    std::list<Constraint *> Constraints;
    virtual ~PCSet();
    
    void pushChar(char c) { name.push_back(c); }
    void setValue(double v) { value = v; } 
    
    /** Element addition methods */
    void pushVar();
    template<typename T>
    void addVariable(std::string vid, T vvalue);
    template<typename T>
    void addVariable(std::string vid, T vvalue, T vlow, T vhigh, T vstep);
    void addConstraint(Constraint * c);

    VariableAbstract * getVariablebyID(std::string vid);
    void display();
};

template<typename T>
void PCSet::addVariable(std::string vid, T vvalue)
{
    Variable<T> *v = new Variable<T>(vid,vvalue);
    Vars.push_back(v);
}

template<typename T>
void PCSet::addVariable(std::string vid, T vvalue, T vlow, T vhigh, T vstep)
{
    Variable<T> *v = new Variable<T>(vid,vvalue);
    v->intersectInterval(Interval<T>(vlow,vhigh,vstep));
    Vars.push_back(v);
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
