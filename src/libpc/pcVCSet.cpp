/*                    P C P C S E T . C P P
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
/** @file pcVCSet.cpp
 *
 * VCSet Class Methods
 * 
 */
#include "pcVCSet.h"
#include "pc.h"

VCSet::~VCSet()
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
    std::list<Parameter *>::iterator k = ParTable.begin();
    std::list<Parameter *>::iterator p_end = ParTable.end();
    for (; k != p_end; k++) {
	switch ((**k).getType()) {
	    case PC_DB_FASTF_T:
		delete (FastF *) *k;
		break;
	    case PC_DB_POINT_T:
		delete (Point *) *k;
		break;
	    case PC_DB_VECTOR_T:
		delete (Vector *) *k;
		break;
	    default:;
	}
    }
}


void VCSet::addConstraint(std::string cid, std::string cexpr, functor f, int count, ...)
{
    va_list args;
    va_start(args, count);
    Constraint *c = new Constraint(*this, cid, cexpr, f, count, &args);
    va_end(args);
    Constraints.push_back(c);
}


void VCSet::addConstraint(std::string cid, functor f, std::list<std::string> Vid)
{
    Constraint *c = new Constraint(*this, cid, "", f, Vid);
    Constraints.push_back(c);
}


void VCSet::addConstraint(pc_constrnt *con)
{
    Constraint *c = new Constraint(*this, con);
    Constraints.push_back(c);
}


void VCSet::addParameter(std::string pname, int type, void *ptr)
{
    switch (type) {
	case PC_DB_VECTOR_T: {
	    Vector *v = new Vector(*this, pname, ptr); 
	    ParTable.push_back(v);
	    //std::cout << "!-- Pushed " << pname << std::endl;
	    break;
	}
	case PC_DB_FASTF_T: {
	    FastF *f = new FastF(*this, pname, ptr); 
	    ParTable.push_back(f);
	    break;
	}
	case PC_DB_POINT_T: {
	    Point *p = new Point(*this, pname, ptr); 
	    ParTable.push_back(p);
	    break;
	}
	default:
	    std::cerr << " Invalid parameter type detected"
		      << pname << std::endl;
    }
}


VariableAbstract *VCSet::getVariablebyID(std::string vid)
{
    std::list<VariableAbstract *>::iterator i;
    for (i = Vars.begin(); i != Vars.end(); i++) {
	if (vid.compare((**i).getID()) == 0)
	    return *i;
    }
    return NULL;
}


void VCSet::store()
{
    std::list<VariableAbstract *>::iterator i = Vars.begin();
    std::list<VariableAbstract *>::iterator end = Vars.end();
    
    for (; i != end; i++) {
	(**i).store();
    }
}


void VCSet::restore()
{
    std::list<VariableAbstract *>::iterator i = Vars.begin();
    std::list<VariableAbstract *>::iterator end = Vars.end();
    
    for (; i != end; i++) {
	(**i).restore();
    }
}


Parameter *VCSet::getParameter(std::string pname)
{
    std::list<Parameter *>::iterator i;
    for (i = ParTable.begin(); i != ParTable.end(); i++) {
	if ((**i).getName().compare(pname) == 0)
	    return *i;
    }
    return NULL;
}


/** @todo remove std::list passing */
std::list<std::string> VCSet::getParamVariables(const char *pname)
{
    std::list<std::string> V;
    Parameter *p = getParameter(pname);
    if (!p) {
	return V;
    }

    Parameter::iterator i = p->begin();
    Parameter::iterator end = p->end();
    for (; i !=end; i++) {
	V.push_back(i->getID());
    }
    return V;
}


void VCSet::display()
{
    using std::endl;
    std::list<Parameter *>::iterator h;
    std::list<VariableAbstract *>::iterator i;
    std::list<Constraint *>::iterator j;
    std::cout << "----------------------------------------------" << endl;
    std::cout << "-----------Variable - Constraint Set----------" << endl;
    if (!Vars.empty()) {
	std::cout << endl << "Parameters" << endl << endl;
	for (h = ParTable.begin(); h != ParTable.end(); h++)
	    (**h).display();
    }
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


bool VCSet::check()
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
