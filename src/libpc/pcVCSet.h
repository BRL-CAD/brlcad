/*                    P C V C S E T . H
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
/** @addtogroup libpc */
/** @{ */
/** @file pcVCSet.h
 *
 * VCSet Class definition : A Set of Variables and Constraints
 *
 */
#ifndef __PCVCSET_H__
#define __PCVCSET_H__

#include "common.h"

#include <string>
#include <stdarg.h>
#include <iostream>
#include <list>
#include "pcVariable.h"
#include "pcConstraint.h"
#include "pcParameter.h"

#define PC_VCSET_GETVAR(_vcset, _type, _name) \
	_type _name = ((Variable<_type>*) _pcset.getVariablebyID("_name"))->getValue();
/**
 * A wrapper class for a set of Variables and Constraints
 * After a hypergraph representation, this might get deprecated.
 * Parser Class outputs this as a result of parse() and Generator class uses
 * it for generation of the Constraint Network (presently Binary Network)
 *
 */
class Constraint;
struct pc_constrnt;

class VCSet
{

    typedef boost::function2< bool, VCSet &, std::list<std::string> > functor;
public:
    std::list<VariableAbstract *> Vars;
    std::list<Constraint *> Constraints;

    virtual ~VCSet();
    
    /** Private Data modification methods */
    void pushChar(char c) { name.push_back(c); }
    void setValue(double v) { value = v; } 
    
    /** Element addition methods */
    void pushVar();
    template<typename T>
    VariableAbstract *addVariable(std::string vid, T vvalue);
    template<typename T>
    VariableAbstract *addVariable(std::string vid, T vvalue, T vlow, T vhigh, T vstep);
    void addConstraint(std::string cid, std::string cexpr, functor f, int count, ...);
    void addConstraint(std::string cid, functor f, std::list<std::string> Vid);
    void addConstraint(pc_constrnt *c);
    void addParameter(std::string pname, int type, void *ptr);
    
    /** Variable access method */
    VariableAbstract *getVariablebyID(std::string vid);
    void store();
    void restore();
    
    /** Parameter table data access */
    Parameter *getParameter(std::string pid);
    std::list<std::string> getParamVariables(const char *);

    /** Constraint status check method */
    bool check();

    /** Display methods */
    void display();
private:
    std::list<Parameter *> ParTable;
    std::string name;
    double value;
};


template<typename T>
VariableAbstract *VCSet::addVariable(std::string vid, T vvalue)
{
    Variable<T> *v = new Variable<T>(vid, vvalue);
    Vars.push_back(v);
    return v;
}


template<typename T>
VariableAbstract *VCSet::addVariable(std::string vid, T vvalue, T vlow, T vhigh, T vstep)
{
    Variable<T> *v = new Variable<T>(vid, vvalue);
    v->intersectInterval(Interval<T>(vlow, vhigh, vstep));
    Vars.push_back(v);
    return v;
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
