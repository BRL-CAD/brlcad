/*                       P C P A R A M E T E R . C P P
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
/** @file pcParameter.cpp
 *
 * Methods of Parameter and derived classes
 *
 * @author Dawn Thomas
 */
#include "pcParameter.h"
#include "pcVCSet.h"
#include "pc.h"
#if 0
#define PC_PARAM_ADDVAR(_vcs,_name,_value) \
	    Parameter::_vcs.addVariable<double>(_name,_value,\
		    -std::numeric_limits<double>::max(), \
		    std::numeric_limits<double>::max(), .00001)
#endif
#define PC_PARAM_ADDVAR(_vcs,_name,_value) \
	    Parameter::_vcs.addVariable<double>(_name,_value,\
		    -10, 10, .01)

/**
 *			Parameter Methods
 *
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

Parameter::iterator Parameter::erase(iterator begin, iterator end)
{
    return makeIterator(Variables.erase(begin.base(), end.base()));
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
 
/**
 *			Vector Methods
 *
 */

Vector::Vector(VCSet & vcs,std::string n, void * ptr)
    : Parameter(vcs, n)
{
    Parameter::setType(PC_DB_VECTOR_T);
    vectp_t p = vectp_t (ptr);
    if (ptr) {
	std::string t = Parameter::name; 
	t+="[x]";
	Variables.push_back(PC_PARAM_ADDVAR(vcset,t,*p));
	//PC_PARAM_ADDVAR(vcset,t,*p);
	//Variables.push_back(vcs.getVariablebyID(t));
	t = Parameter::name; 
	t+="[y]";
	Variables.push_back(PC_PARAM_ADDVAR(vcset,t,*(p+1)));
	//PC_PARAM_ADDVAR(vcset,t,*(p+1));
	//Variables.push_back(vcs.getVariablebyID(t));
	t = Parameter::name; 
	t+="[z]";
	//PC_PARAM_ADDVAR(vcset,t,*(p+2));
	//Variables.push_back(vcs.getVariablebyID(t));
	Variables.push_back(PC_PARAM_ADDVAR(vcset,t,*(p+2)));
    }
}

/**
 *			Point Methods
 *
 */

Point::Point(VCSet & vcs,std::string n, void * ptr)
    : Parameter(vcs, n)
{
    Parameter::setType(PC_DB_POINT_T);
    pointp_t p = pointp_t (ptr);
    if (ptr) {
	std::string t = Parameter::name; 
	t+="[x]";
	Variables.push_back(PC_PARAM_ADDVAR(vcset,t,*p));
	t = Parameter::name; 
	t+="[y]";
	Variables.push_back(PC_PARAM_ADDVAR(vcset,t,*(p+1)));
	t = Parameter::name; 
	t+="[z]";
	Variables.push_back(PC_PARAM_ADDVAR(vcset,t,*(p+2)));
    }
}

/**
 *			FastF Methods
 *
 */

FastF::FastF(VCSet & vcs,std::string n, void * ptr)
    : Parameter(vcs, n)
{
    Parameter::setType(PC_DB_FASTF_T);
    fastf_t *p = (fastf_t *) ptr;
    if (ptr) {
	std::string t = Parameter::name; 
	Variables.push_back(PC_PARAM_ADDVAR(vcset,t,*p));
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
