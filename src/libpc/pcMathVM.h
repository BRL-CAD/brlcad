/*                       P C M A T H V M . H
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
/** @file pcMathVM.h
 *
 * Math Virtual Machine for evaluating arbitrary math expressions
 *
 * Code is based on concepts from YAC by Angus Leeming
 *
 * @author Dawn Thomas
 */
#ifndef __PCMATHVM_H__
#define __PCMATHVM_H__

#include "common.h"

#include <boost/shared_ptr.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/spirit/include/classic_symbols.hpp>

#include <list>

class Stack;

struct Node {
    /** Destructor */
    virtual ~Node() {}

    /** Branch Methods */
    virtual std::size_t branchsize() const { return 0; }
    virtual Stack * branch(std::size_t) { return 0; }
};

class Stack
{
    typedef std::list<boost::shared_ptr<Node> > container_t;
    typedef boost::indirect_iterator<container_t::iterator> iterator;
public:
    /** Create and Copy Constructors*/
    Stack() {}
    Stack(Stack const &);

    /** Push Method */
    void push_back(Node * n);
    iterator begin();
    iterator end();
private:
    container_t data;
    void copy(Stack::container_t const &);
};

struct function;

struct MathVM
{
    Stack stack;
    boost::spirit::classic::symbols<double> variables;
    boost::spirit::classic::symbols<boost::shared_ptr<function> > functions;
};

double evaluate(Stack s);

/* Node Implementations */

struct number_node : public Node
{
    virtual double value() const = 0;
};

struct function_node : public Node
{
    virtual function const & func() const = 0;
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
