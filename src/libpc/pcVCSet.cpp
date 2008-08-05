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
    std::list<Constraint *>::iterator j;
    for (j = Constraints.begin(); j != Constraints.end(); j++) {
	delete *j;
    }
}

void PCSet::pushVar()
{
    Variable<double> *v = new Variable<double>(name,value);
    v->addInterval(Interval<double>( -std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), 0.00001));
    Vars.push_back(v);
    /*addVariable<double>(name, value);*/
    name.clear();
}

void PCSet::addConstraint(std::string cid, std::string cexpr, functor f,int count,...)
{
    va_list args;
    va_start(args,count);
    Constraint *c = new Constraint(*this, cid, cexpr, f, count, &args);
    va_end(args);
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


void PCSet::display()
{
    using std::endl;
    std::list<VariableAbstract *>::iterator i;
    std::list<Constraint *>::iterator j;
    std::cout << "----------------------------------------------" << endl;
    std::cout << "-----------Parameter-Constraint Set-----------" << endl;
    if (!Vars.empty()) {
	std::cout<< endl << "Variables:" << endl << endl;
	for (i = Vars.begin(); i != Vars.end(); i++)
	    (**i).display();
    }	    
    if (!Constraints.empty()) {
	std::cout<< endl << "Constraints:" << endl << endl;
	for (j = Constraints.begin(); j != Constraints.end(); j++)
	    (**j).display();
    }
}

bool PCSet::check()
{
    std::list<Constraint *>::iterator i;
    for (i = Constraints.begin(); i != Constraints.end(); ++i) {
	if (! (**i).check()) {
	    return false;
	}
    }
    return true;
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
