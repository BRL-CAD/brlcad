/*                       P C M A T H V M . C P P
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
/** @file pcMathVM.cpp
 *
 * Math VM Method implementation
 *
 * @author Dawn Thomas
 */

#include "pcMathVM.h"
#include <cassert>
#include <map>

/**
 * 				Stack Object Methods
 */

/** Copy constructor */
Stack::Stack(Stack const & s)
{
    copy(s.data);
}

/** Private copy function used by the copy constructor */
void Stack::copy(Stack::container_t const & a)
{
    data.clear();
    Stack::container_t::const_iterator i = a.begin();
    Stack::container_t::const_iterator const end = a.end();
    for (; i != end; ++i)
	data.push_back((*i)->clone());
}

/** Size access method */
Stack::size_type Stack::size() const
{
    return data.size();
}

/** Stack empty check method */
bool Stack::empty() const
{
    return data.empty();
}

/** Node addition method */
void Stack::push_back(Node * n)
{
    /** assert that node exists */
    assert(n);
    data.push_back(boost::shared_ptr<Node>(n));
}

/** Stack clear */
void Stack::clear()
{
    data.clear();
}

/**
 * 			     Stack Iterator Methods
 *
 * Indirect iterators from bost are generated using wrapper functions.
 * Indirect iterator adapts an iterator by applying an extra dereference inside
 * of operator*(). For example, this iterator adaptor makes it possible to view
 * a container of pointers (e.g. list<foo*>) as if it were a container of the 
 * pointed-to type (e.g. list<foo>).
 *
 */

Stack::iterator makeIterator(Stack::container_t::iterator i)
{
    return boost::make_indirect_iterator(i);
}

Stack::const_iterator makeIterator(Stack::container_t::const_iterator i)
{
    return boost::make_indirect_iterator(i);
}

Stack::iterator Stack::begin()
{
    return makeIterator(data.begin());
}

Stack::iterator Stack::end()
{
    return makeIterator(data.end());
}

Stack::const_iterator Stack::begin() const
{
    return makeIterator(data.begin());
}

Stack::const_iterator Stack::end() const
{
    return makeIterator(data.end());
}

Stack::iterator Stack::erase(iterator location)
{
    return makeIterator(data.erase(location.base()));
}

Stack::iterator Stack::erase(iterator begin, iterator end)
{
    return makeIterator(data.erase(begin.base(), end.base()));
}

Stack::iterator Stack::insert(iterator location, Node * n)
{
    assert(n);
    return makeIterator(data.insert(location.base(), boost::shared_ptr<Node>(n)));
}

/**
 * 			     Math Virtual Machine methods
 */

void MathVM::display()
{

}
    
/** 
 * 				  MathFunction methods
 */

MathFunction::MathFunction(std::string const & n) :
	name(n)
{}

/** getName() returns the name of the function */
std::string const & MathFunction::getName() const
{
    return name;
}

UserFunction * MathFunction::asUserFunction()
{
    return 0;
}

/** 
 * eval() is effectively a wrapper function over the private evaluation
 * function . It does an arity check to match the number of provided
 * arguments with the expected.
 */
double MathFunction::eval(std::vector<double> const & args) const
{
    assert(args.size() == arity());
    return evalp(args);
}


/** 
 * 				  updateStack Functions
 * updates the stack members according to the variable table provided.
 * uses std::map internally for creating a map between source and destination
 * variable addresses.
 *
 */

void updateStack(Stack & s, std::map<double *,double *> const & pmap)
{
    typedef std::map<double *, double *> mapd;

    Stack::iterator i = s.begin();
    Stack::iterator const end = s.end();
    for (; i != end; ++i)
    {
	VariableNode * vn = dynamic_cast<VariableNode *>(&*i);
	if (vn) {
	    mapd::const_iterator j = pmap.find(vn->pd);
	    if (j == pmap.end()) {
		std::cerr << " Warning: Pointer " << vn->pd << ","
			  << *vn->pd << " not found!" <<std::endl;
		continue;
	    } else {
		vn->pd = j->second;
	    }
	}

	std::size_t const nbranches = i->branchSize();
	if (nbranches == 0)
	    continue;
	
	for(std::size_t k =0; k !=nbranches; ++k)
	{
	    Stack * stk = i->branch(k);
	    if (!stk)
		continue;
	    updateStack(*stk,pmap);
	}
    }
}

void updateStack(Stack & s,
		 UserFunction::symboltable const & slocalvariables,
		 UserFunction::symboltable const & dlocalvariables,
		 std::vector<std::string> argn)
{
    using boost::spirit::classic::find;
    if (s.empty())
	return;
    /** create a map between data adresses in destination Variable table
     * and source Variable table
     */
    typedef std::map<double *, double *> mapd;
    mapd pmap;
    for (std::size_t i = 0; i != argn.size(); ++i)
    {
	double * p1 = find(dlocalvariables,argn[i].c_str());
	double * p2 = find(slocalvariables,argn[i].c_str());
	pmap[p1] = p2;
    }
    updateStack(s,pmap);
}


/** 
 * 				  UserFunction methods
 */

/** Default constructor */
UserFunction::UserFunction()
    : arity_(0)
{}

UserFunction::UserFunction(std::string const & name, std::size_t const farity)
    : MathFunction(name),
      arity_(farity)
{}

/** Copy constructor */
UserFunction::UserFunction(UserFunction const & other)
    : MathFunction(other),
      arity_(other.arity_),
      argnames(other.argnames),
      localvariables_(other.localvariables_),
      stack(other.stack)
{
    updateStack(stack, localvariables_, other.localvariables_, argnames);
}

/** Arity return method */
std::size_t UserFunction::arity() const
{
    return arity_;
}

UserFunction * UserFunction::asUserFunction()
{
    return this;
}

boost::spirit::classic::symbols<double> const & UserFunction::localvariables() const
{
    return localvariables_;
}

/** Actual (private) evaluation function */
double UserFunction::evalp(std::vector<double> const & args) const
{
    /** assert that the size of arguments is as expected */
    assert(argnames.size() == arity_);
    
    /** create a copy of symbol table*/
    symboltable temp = localvariables_;
    
    /** store data from args into the copy */
    std::size_t const size = argnames.size();
    for(std::size_t i =0; i != size ; ++i) {
	double * const p = boost::spirit::classic::find(temp, argnames[i].c_str());
	assert(p);
	*p = args[i];
    }
    
    Stack stackcopy = stack;
    updateStack(stackcopy, temp, localvariables_, argnames);

    return evaluate(stackcopy);
}

/**
 * 				   Node Methods
 */

ConstantNode::ConstantNode(double v)
    : value(v)
{}

/* Creating a clone of the existing node */
boost::shared_ptr<Node> ConstantNode::clone() const
{
    return boost::shared_ptr<Node>(new ConstantNode(*this));
}

double ConstantNode::getValue() const
{
    return value;
}

VariableNode::VariableNode(double * p)
    : pd(p) 
{
    /** Assert that pointer to double(pd) is not NULL */
    assert(pd);
}

boost::shared_ptr<Node> VariableNode::clone() const
{
    return boost::shared_ptr<Node>(new VariableNode(*this));
}

double VariableNode::getValue() const
{
    return *pd;
}

double & VariableNode::getVar() const
{
    return *pd;
}

/**
 * 				   Stack Evaluation Method
 */

double evaluate(Stack s)
{
    if (s.empty())
	return 0.0;
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
