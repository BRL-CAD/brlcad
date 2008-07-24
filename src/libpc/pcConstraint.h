/*                       P C C O N S T R A I N T . H
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
/** @file pcConstraint.h
 *
 * Constraint Class for Parametrics and Constraints Library
 *
 * @author Dawn Thomas
 */
#ifndef __PCCONSTRAINT_H__
#define __PCCONSTRAINT_H__

#include "common.h"

#include <string>
#include <stdarg.h>
#include <vector>
#include <list>
#include <boost/function.hpp>
#include "pcVariable.h"

class Constraint {
private:
    int status;
    std::string id;
    std::string expression;
    boost::function1< bool, std::vector<VariableAbstract *> > funct; 
    //bool (*funct) (std::vector<VariableAbstract *>);
public:

    std::list<std::string> Variables;

    Constraint() : status(0) { }
    Constraint(std::string Cid, std::string Cexpression, bool (*pf) (std::vector<VariableAbstract *>));
    Constraint(std::string Cid, std::string Cexpression, bool (*pf) (std::vector<VariableAbstract *>),int count,...);
    void function(bool (*pf) (std::vector<VariableAbstract *>)) { funct = pf; };
    bool solved() {
	if (status == 0)
	    return false;
	else
	    return true;
    }
    /* TODO: Take a functor as an argument? */
    bool check(std::vector<VariableAbstract *> V);
    std::string getID() const { return id; }
    std::string getExp() const { return expression; }
    void setStatus(int st) { status = st; }

};
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
