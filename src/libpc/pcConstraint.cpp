/*                       P C C O N S T R A I N T . C P P
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
/** @file pcConstraint.cpp
 *
 * Constraint Class Methods
 *
 * @author Dawn Thomas
 */
#include "pcConstraint.h"
#include "pcVCSet.h"

Constraint::Constraint(VCSet &vcs) :
    vcset(vcs),
    status(0)
{
}

Constraint::Constraint(VCSet &vcs, std::string Cid, std::string Cexpression, functor pf) :
    vcset(vcs),
    status(0),
    id(Cid),
    expression(Cexpression),
    eval(pf)
{
}

Constraint::Constraint(VCSet &vcs, std::string Cid, std::string Cexpression, functor pf, std::list<std::string> Vid) :
    vcset(vcs),
    status(0),
    id(Cid),
    expression(Cexpression),
    eval(pf),
    Variables(Vid)
{ 
}

Constraint::Constraint(VCSet &vcs, std::string Cid, std::string Cexpression, functor pf, int count, va_list * args) :
    vcset(vcs),
    status(0),
    id(Cid),
    expression(Cexpression),
    eval(pf)
{
    for (int i=0; i<count; i++) {
	Variables.push_back(va_arg(*args,char *));
    }
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
    for ( i = Variables.begin(); i != end; i++)
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
