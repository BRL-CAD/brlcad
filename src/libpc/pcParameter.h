/*                       P C P A R A M E T E R . H
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
/** @file pcParameter.h
 *
 * Parameter Class abstraction over the Variable class
 *
 */
#ifndef __PCPARAMETER_H__
#define __PCPARAMETER_H__

#include "common.h"

#include "pcVariable.h"
#include <boost/iterator/indirect_iterator.hpp>
#include <string>
#include <list>

class VCSet;


class Parameter
{
public:
    typedef std::list<VariableAbstract *> Varlist;
    typedef boost::indirect_iterator<Varlist::iterator> iterator;
    typedef boost::indirect_iterator<Varlist::const_iterator> const_iterator;

    /** Constructor */
    Parameter(VCSet & vcs, std::string n = "default Parameter");

    /** Iterator methods */
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    iterator erase(iterator location);
    iterator erase(iterator begin, iterator end);

    /** Data access methods */
    std::string getName() const;
    int getType() const;
    Varlist::size_type getSize() { return Variables.size(); }
    bool isConst();

    /** Data modification methods */
    void setType(int n) { type = n; }
    void setConst(bool t);

    /** Display method */
    void display() const;

protected:
    int type;
    VCSet & vcset;
    std::string name;
    Varlist Variables;
};


extern Parameter::iterator makeIterator(Parameter::Varlist::iterator i);
extern Parameter::const_iterator makeIterator(Parameter::Varlist::const_iterator i);


class Vector : public Parameter
{
public:
    Vector(VCSet & vcs, std::string n = "default Vector", void *ptr = NULL);
};


class Point : public Parameter
{
public:
    Point(VCSet & vcs, std::string n = "default Point", void *ptr = NULL);
};


class FastF : public Parameter
{
public:
    FastF(VCSet & vcs, std::string n = "default FastF", void *ptr = NULL);
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
