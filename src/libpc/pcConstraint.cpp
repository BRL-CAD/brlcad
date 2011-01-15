/*                       P C C O N S T R A I N T . C P P
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
/** @file pcConstraint.cpp
 *
 * Constraint Class Methods
 *
 */
#include "pcConstraint.h"
#include "pcVCSet.h"
#include "pc.h"

ConstraintInterface::ConstraintInterface(pc_constrnt *c)
{
    if (c) {
	nargs_ = c->data.cf.nargs;
        dimension_ = c->data.cf.dimension;
        fp_ = c->data.cf.fp;
        a = new double*[nargs_];
	
	for (int i =0; i< nargs_; i++) {
	    a[i] = new double[dimension_];
	}
    }
}


ConstraintInterface::~ConstraintInterface()
{
    if (a) {
	for (int i = 0 ; i < nargs_; i++) {
	    delete[] a[i];
	}
	delete[] a;
    }
}


bool ConstraintInterface::operator() (VCSet & vcset, std::list<std::string> Vid) const
{
    typedef Variable<double> *Vi;

    for (int i =0; i < nargs_; i++) {
        for (int j = 0; j < dimension_; j++) {
	    a[i][j] = ((Vi) vcset.getVariablebyID(Vid.front()))->getValue();
	    Vid.pop_front();
	}
    }

    if (!fp_) {
	std::cout << "!!! Constraint evaluation pointer NULL\n";
	return false;
    }

    if (fp_(a) == 0) {
	return true;
    }

    return false;
}


Constraint::Constraint(VCSet &vcs) :
    status(0),
    cif (NULL),
    vcset(vcs)
{
}


Constraint::Constraint(VCSet &vcs, std::string Cid, std::string Cexpression, functor pf) :
    status(0),
    cif (NULL),
    vcset(vcs),
    id(Cid),
    expression(Cexpression),
    eval(pf)
{
}


Constraint::Constraint(VCSet &vcs, std::string Cid, std::string Cexpression, functor pf, std::list<std::string> Vid) :
    status(0),
    cif (NULL),
    vcset(vcs),
    id(Cid),
    expression(Cexpression),
    Variables(Vid),
    eval(pf)
{ 
    std::list<std::string>::iterator i = Variables.begin();
    std::list<std::string>::iterator end = Variables.end();
    for (; i != end; ++i)
	vcset.getVariablebyID(*i)->setConstrained(1);
}


Constraint::Constraint(VCSet &vcs, std::string Cid, std::string Cexpression, functor pf, int count, va_list *args) :
    status(0),
    cif (NULL),
    vcset(vcs),
    id(Cid),
    expression(Cexpression),
    eval(pf)
{
    for (int i=0; i<count; i++) {
	std::string tmp = va_arg(*args, char *);
	Variables.push_back(tmp);
	vcset.getVariablebyID(tmp)->setConstrained(1);
    }
}


Constraint::Constraint(VCSet &vcs, pc_constrnt *c) :
    status(0),
    cif (c),
    vcset(vcs),
    id(bu_vls_addr(&(c->name))),
    expression("")
{
    eval = boost::ref(cif);
    std::list<std::string> t;
    for (int i = 0; i < c->data.cf.nargs; i++) {
	t = vcset.getParamVariables(c->args[i]);
	Variables.merge(t);
    }
    std::list<std::string>::iterator i = Variables.begin();
    std::list<std::string>::iterator end = Variables.end();
    for (; i != end; ++i)
	vcset.getVariablebyID(*i)->setConstrained(1);
}


bool Constraint::solved()
{
    if (status == 0)
        return false;
    else
        return true;
}


bool Constraint::check()
{
    if (eval(vcset, Variables)) {
	status =1;
	return true;
    } else {
	status = 0;
	return false;
    }
}


void Constraint::display()
{
    std::list<std::string>::iterator i;
    std::list<std::string>::iterator end = Variables.end();
    std::cout<< "Constraint id = " << id << "\t Expression = " << expression \
	     << "\tSolved status = " << status << std::endl;
    for (i = Variables.begin(); i != end; i++)
	std::cout << "--| " << *i << std::endl;
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
