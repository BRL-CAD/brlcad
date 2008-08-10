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
 * Parameter Class Methods
 *
 * @author Dawn Thomas
 */
#include "pcParameter.h"
#include "pcVCSet.h"
#include "pc.h"

#define PC_PARAM_ADDVAR(_vcs,_name,_value) \
	    Parameter::_vcs.addVariable<double>(_name,_value,\
		    -std::numeric_limits<double>::max(), \
		    std::numeric_limits<double>::max(), .00001);

Parameter::Parameter(VCSet & vcs, std::string n)
    : vcset(vcs), name(n)
{}

std::string Parameter::getName()
{
    return name;
}

Vector::Vector(VCSet & vcs,std::string n, void * ptr)
    : Parameter(vcs, n)
{
    Parameter::setType(PC_DB_VECTOR_T);
    vectp_t p = vectp_t (ptr);
    if (ptr) {
	std::string t = Parameter::name; 
	t+="[x]";
	PC_PARAM_ADDVAR(vcset,t,*p);
	t = Parameter::name; 
	t+="[y]";
	PC_PARAM_ADDVAR(vcset,t,*(p+1));
	t = Parameter::name; 
	t+="[z]";
	PC_PARAM_ADDVAR(vcset,t,*(p+2));
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
