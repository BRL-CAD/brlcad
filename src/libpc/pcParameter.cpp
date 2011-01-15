/*                       P C P A R A M E T E R . C P P
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
/** @file pcParameter.cpp
 *
 * Methods of Parameter and derived classes
 *
 */

#include "pcParameter.h"
#include "pcVCSet.h"
#include "pc.h"

/*
 * Parameter Methods
 */

Parameter::Parameter(VCSet & vcs, std::string n)
    : vcset(vcs), name(n)
{}

Parameter::iterator makeIterator(Parameter::Varlist::iterator i)
{
    return boost::make_indirect_iterator(i);
}


Parameter::const_iterator makeIterator(Parameter::Varlist::const_iterator i)
{
    return boost::make_indirect_iterator(i);
}


Parameter::iterator Parameter::begin()
{
    return makeIterator(Variables.begin());
}


Parameter::iterator Parameter::end()
{
    return makeIterator(Variables.end());
}


Parameter::const_iterator Parameter::begin() const
{
    return makeIterator(Variables.begin());
}


Parameter::const_iterator Parameter::end() const
{
    return makeIterator(Variables.end());
}


Parameter::iterator Parameter::erase(iterator location)
{
    return makeIterator(Variables.erase(location.base()));
}


Parameter::iterator Parameter::erase(iterator beginning, iterator ending)
{
    return makeIterator(Variables.erase(beginning.base(), ending.base()));
}


std::string Parameter::getName() const
{
    return name;
}


int Parameter::getType() const
{
    return type;
}


void Parameter::display() const
{
    std::cout << "!-- " << name << "  Type = " << type <<std::endl;
}
/** Convenience wrappers around setConst() methods of Variables */

void Parameter::setConst(bool t)
{
    iterator i;
    for (i = begin(); i != end(); ++i) {
	if (t)
	    (*i).setConst();
	else
	    (*i).setVar();
    }
}


/**
 * Vector Methods
 *
 */
Vector::Vector(VCSet & vcs, std::string n, void *ptr)
    : Parameter(vcs, n)
{
    Parameter::setType(PC_DB_VECTOR_T);
    vectp_t p = vectp_t (ptr);
    if (ptr) {
	std::string t;
	VariableAbstract *var;

	t = Parameter::name; 
	t += "[x]";
	var = vcset.addVariable<double>(t, *(p+0), -10.0, 10.0, 0.1);
	Variables.push_back(var);

	t = Parameter::name; 
	t += "[y]";
	var = vcset.addVariable<double>(t, *(p+1), -10.0, 10.0, 0.1);
	Variables.push_back(var);

	t = Parameter::name; 
	t += "[z]";
	var = vcset.addVariable<double>(t, *(p+2), -10.0, 10.0, 0.1);
	Variables.push_back(var);
    }
}


/**
 * Point Methods
 *
 */
Point::Point(VCSet & vcs, std::string n, void *ptr)
    : Parameter(vcs, n)
{
    Parameter::setType(PC_DB_POINT_T);
    pointp_t p = pointp_t (ptr);
    if (ptr) {
	std::string t;
	VariableAbstract *var;

	t = Parameter::name; 
	t += "[x]";
	var = vcset.addVariable<double>(t, *(p+0), -10.0, 10.0, 0.1);
	Variables.push_back(var);

	t = Parameter::name; 
	t += "[y]";
	var = vcset.addVariable<double>(t, *(p+1), -10.0, 10.0, 0.1);
	Variables.push_back(var);

	t = Parameter::name; 
	t += "[z]";
	var = vcset.addVariable<double>(t, *(p+2), -10.0, 10.0, 0.1);
	Variables.push_back(var);
    }
}


/**
 * FastF Methods
 *
 */

FastF::FastF(VCSet & vcs, std::string n, void *ptr)
    : Parameter(vcs, n)
{
    Parameter::setType(PC_DB_FASTF_T);
    fastf_t *p = (fastf_t *) ptr;
    if (ptr) {
	std::string t = Parameter::name; 
	VariableAbstract *var = vcset.addVariable<double>(t, *(p+0), -10.0, 10.0, 0.1);
	Variables.push_back(var);
    }
}


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
