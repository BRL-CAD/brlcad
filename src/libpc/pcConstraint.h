/*                       P C C O N S T R A I N T . H
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
/** @file pcConstraint.h
 *
 * Constraint Class for Parametrics and Constraints Library
 *
 */
#ifndef __PCCONSTRAINT_H__
#define __PCCONSTRAINT_H__

#include "common.h"

#include <string>
#include <stdarg.h>
#include <vector>
#include <list>
#include <boost/function.hpp>
#include <boost/ref.hpp>
#include "pcVariable.h"

struct pc_constrnt;
class VCSet;

struct ConstraintInterface
{
public:
    ConstraintInterface(struct pc_constrnt *c);
    ~ConstraintInterface();
    bool operator() (VCSet & vcset, std::list<std::string> Vid) const;
    double **a;
private:
    int (*fp_) (double **);
    int nargs_;
    int dimension_;
};


class Constraint {
    typedef boost::function2< bool, VCSet &, std::list<std::string> > functor;
public:

    /** constructors & Destructors */
    Constraint(VCSet &vcs);
    Constraint(VCSet &vcs, std::string Cid, std::string Cexpr, functor);
    Constraint(VCSet &vcs, std::string Cid, std::string Cexpr, functor, \
	       std::list<std::string> Vid);
    Constraint(VCSet &vcs, std::string Cid, std::string Cexpr, functor, \
	       int count, va_list *args);
    Constraint(VCSet &vcs, pc_constrnt *);
    
    bool solved();
    bool check();
    void evalfunction(functor pf) { eval = pf; };
    
    /** Data access/modification methods */
    std::string getID() const { return id; }
    std::string getExp() const { return expression; }
    std::list<std::string> getVariableList() { return Variables; }
    void setStatus(int st) { status = st; }

    /** Display methods */
    void display();
private:
    int status;
    ConstraintInterface cif;
    VCSet &vcset;
    std::string id;
    std::string expression;
    std::list<std::string> Variables;
    functor eval; 
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
