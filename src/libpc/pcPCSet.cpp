/*                    P C P C S E T . C P P
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
 * PCSet Class Methods
 * 
 * @author Dawn Thomas
 */
#include "pcPCSet.h"

PCSet::~PCSet()
{
    std::list<VariableAbstract *>::iterator i;
    for (i = Vars.begin(); i != Vars.end(); i++) {
	switch ((**i).getType()) {
	    case VAR_INT:
		delete (Variable<int> *) *i;
		break;
	    case VAR_DBL:
		delete (Variable<double> *) *i;
		break;
	    case VAR_ABS:
	    default:
		delete *i;
	}
    }
}

void PCSet::display()
{
    std::list<VariableAbstract *>::iterator i;
    std::cout<< "Variables:" << std::endl;
    for (i = Vars.begin(); i != Vars.end(); i++)
        //((Variable<double> *) i)->display();
        (**i).display();
}

void PCSet::pushVar()
{
    Variable<double> *V = new Variable<double>(name,value);
    Vars.push_back(V);
}

void PCSet::addVariable(VariableAbstract * v)
{
    Vars.push_back(v);
}

void PCSet::addConstraint(Constraint * c)
{
    Constraints.push_back(c);
}

VariableAbstract * PCSet::getVariablebyID(std::string vid)
{
    std::list<VariableAbstract *>::iterator i;
    for (i = Vars.begin(); i != Vars.end(); i++) {
	if (vid.compare((**i).getID()) == 0)
	    return *i;
    }
    return NULL;
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
