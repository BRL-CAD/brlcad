/*                       P C M A T H V M . C P P
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
/** @file pcMathVM.cpp
 *
 * Math VM Method implementation
 *
 */

#include "pcMathVM.h"

#include <boost/next_prior.hpp>
#include <cassert>
#include <map>

#include "vmath.h"


void copyStack(Stack::container_t & lhs, Stack::container_t const & rhs)
{
    lhs.clear();
    Stack::container_t::const_iterator i = rhs.begin();
    Stack::container_t::const_iterator const rhsend = rhs.end();
    for (; i != rhsend; ++i)
	lhs.push_back((*i)->clone());
}


/**
 * Stack Object Methods
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
    Stack::container_t::const_iterator const aend = a.end();
    for (; i != aend; ++i)
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
void Stack::push_back(Node *n)
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


/** Stack operator = overloading */
Stack & Stack::operator=(Stack const & other)
{
    if (&other != this)
	copyStack(data, other.data);
    return *this;
}


/** Stack operator += overloading */
Stack & Stack::operator+=(Stack const & other)
{
    container_t otherdata;
    copyStack(otherdata, other.data);
    data.insert(data.end(), otherdata.begin(), otherdata.end());
    return *this;
}


/**
 * Stack Iterator Methods
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


Stack::iterator Stack::erase(iterator begin_range, iterator end_range)
{
    return makeIterator(data.erase(begin_range.base(), end_range.base()));
}


Stack::iterator Stack::insert(iterator location, Node *n)
{
    assert(n);
    return makeIterator(data.insert(location.base(), boost::shared_ptr<Node>(n)));
}


/**
 * Math Virtual Machine methods
 */

void MathVM::display()
{

}


/** 
 * MathFunction methods
 */

MathFunction::MathFunction(std::string const & n) :
    name(n)
{}

/** getName() returns the name of the function */
std::string const & MathFunction::getName() const
{
    return name;
}


UserFunction *MathFunction::asUserFunction()
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
 * updateStack Functions
 * updates the stack members according to the variable table provided.
 * uses std::map internally for creating a map between source and destination
 * variable addresses.
 *
 */

void updateStack(Stack & s, std::map<double *, double *> const & pmap)
{
    typedef std::map<double *, double *> mapd;

    Stack::iterator i = s.begin();
    Stack::iterator const end = s.end();
    for (; i != end; ++i) {
	VariableNode *vn = dynamic_cast<VariableNode *>(&*i);
	if (vn) {
	    mapd::const_iterator j = pmap.find(vn->pd);
	    if (j == pmap.end()) {
		std::cerr << " Warning: Pointer " << vn->pd << ", "
			  << *vn->pd << " not found!" <<std::endl;
		continue;
	    } else {
		vn->pd = j->second;
	    }
	}

	std::size_t const nbranches = i->branchSize();
	if (nbranches == 0)
	    continue;
	
	for (std::size_t k =0; k !=nbranches; ++k) {
	    Stack *stk = i->branch(k);
	    if (!stk)
		continue;
	    updateStack(*stk, pmap);
	}
    }
}


void updateStack(Stack & s,
		 UserFunction::symboltable const & slocalvariables,
		 UserFunction::symboltable const & dlocalvariables,
		 std::vector<std::string> argn)
{
    using boost::spirit::find;
    if (s.empty())
	return;
    /** create a map between data adresses in destination Variable table
     * and source Variable table
     */
    typedef std::map<double *, double *> mapd;
    mapd pmap;
    for (std::size_t i = 0; i != argn.size(); ++i) {
	double *p1 = find(dlocalvariables, argn[i].c_str());
	double *p2 = find(slocalvariables, argn[i].c_str());
	pmap[p1] = p2;
    }
    updateStack(s, pmap);
}


/** 
 * UserFunction methods
 */

/** Default constructor */
UserFunction::UserFunction()
    : arity_(0)
{}

UserFunction::UserFunction(std::string const & function_name, std::size_t const farity)
    : MathFunction(function_name),
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


UserFunction & UserFunction::operator=(UserFuncExpression const & ufe)
{
    BOOST_ASSERT(ufe.argnames.size() == arity_ && ufe.localvars.get());
    argnames = ufe.argnames;
    localvariables_ = *ufe.localvars;
    stack = ufe.stack;
    updateStack(stack, localvariables_, *ufe.localvars, argnames);
    return *this;
}


/** Arity return method */
std::size_t UserFunction::arity() const
{
    return arity_;
}


UserFunction *UserFunction::asUserFunction()
{
    return this;
}


boost::spirit::symbols<double> const & UserFunction::localvariables() const
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
    for (std::size_t i =0; i != size ; ++i) {
	double *const p = boost::spirit::find(temp, argnames[i].c_str());
	assert(p);
	*p = args[i];
    }
    
    Stack stackcopy = stack;
    updateStack(stackcopy, temp, localvariables_, argnames);

    return evaluate(stackcopy);
}


/**
 * Node Methods
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


VariableNode::VariableNode(double *p)
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


sysFunctionNode::sysFunctionNode(boost::shared_ptr<MathFunction> const & funcp)
{
    fp = funcp;
}


boost::shared_ptr<Node> sysFunctionNode::clone() const
{
    return boost::shared_ptr<Node>(new sysFunctionNode(*this));
}


MathFunction const & sysFunctionNode::func() const
{
    return *fp;
}


/** OrNode methods */

OrNode::OrNode(Stack const & stack)
    : func_(stack)
{}

boost::shared_ptr<Node> OrNode::clone() const
{
    return boost::shared_ptr<Node>(new OrNode(*this));
}


MathFunction const & OrNode::func() const
{
    return func_;
}


std::size_t OrNode::nbranches() const
{
    return 1;
}


Stack *OrNode::branch(std::size_t i)
{
    return i == 0 ? &func_.rhs_stack_ : 0;
}


OrNode::OrFunc::OrFunc(Stack const & stack)
    : MathFunction("or"), rhs_stack_(stack)
{}

std::size_t OrNode::OrFunc::arity() const
{
    return 1;
}


double OrNode::OrFunc::evalp(std::vector<double> const & params) const
{
    return (!ZERO(params[0])) ? 1.0 : evaluate(rhs_stack_);
}


/** FuncDefNode methods */

FuncDefNode::FuncDefNode(boost::shared_ptr<MathFunction> const & funcptr, UserFuncExpression const & value)
    : funcptr_(funcptr), value_(value)
{}

boost::shared_ptr<Node> FuncDefNode::clone() const
{
    return boost::shared_ptr<Node>(new FuncDefNode(*this));
}


void FuncDefNode::assign() const
{
    BOOST_ASSERT(funcptr_.get());
    UserFunction *func = funcptr_->asUserFunction();
    BOOST_ASSERT(func);
    *func = value_;
}


/** AssignNode Methods */

boost::shared_ptr<Node> AssignNode::clone() const
{
    return boost::shared_ptr<Node>(new AssignNode(*this));
}


void AssignNode::assign(double & var, double val) const
{
    var = val;
}


/** BranchNode Methods */

BranchNode::BranchNode(Stack const & stack1, Stack const & stack2)
    : func_(stack1, stack2)
{}

boost::shared_ptr<Node> BranchNode::clone() const
{
    return boost::shared_ptr<Node>(new BranchNode(*this));
}


MathFunction const & BranchNode::func() const
{
    return func_;
}


std::size_t BranchNode::nbranches() const
{
    return 2;
}


Stack *BranchNode::branch(std::size_t i)
{
    if (i > 1)
        return 0;
    return i == 0 ? &func_.stack1_ : &func_.stack2_;
}


BranchNode::BranchFunc::BranchFunc(Stack const & stack1, Stack const & stack2)
    : MathFunction("branch"),
      stack1_(stack1), stack2_(stack2)
{}

std::size_t BranchNode::BranchFunc::arity() const
{
    return 1;
}


double BranchNode::BranchFunc::evalp(std::vector<double> const & params) const
{
    return evaluate(!ZERO(params[0]) ? stack1_ : stack2_);
}


/** Functions assisting evaluation */

NumberNode *getNumberNode(Stack::iterator i)
{
    NumberNode *n = dynamic_cast<NumberNode *>(&*i);
    assert(n);
    return n;
}


VariableNode *getVariableNode(Stack::iterator i)
{
    VariableNode *n = dynamic_cast<VariableNode *>(&*i);
    assert(n);
    return n;
}


std::vector<double> const makeArgList(Stack::iterator const & begin, Stack::iterator const & end)
{
    std::vector<double> v;
    v.reserve(std::distance(begin, end));

    Stack::iterator i = begin;
    for (; i != end; ++i)
	v.push_back(getNumberNode(i)->getValue());
    return v;
}


/**
 * Stack Evaluation Method
 */


double evaluate(Stack s)
{
    if (s.empty())
	return 0.0;

    Stack::iterator i = s.begin();
    while (i != s.end()) {
	if (FunctionNode const *f = dynamic_cast<FunctionNode const *>(&*i)) {
	    MathFunction const & funct = f->func();

	    /** arity must be a signed type (boost::prior) */
	    Stack::difference_type const arity = funct.arity();
	    Stack::iterator j = boost::prior(i, arity);
	    double const result = funct.eval(makeArgList(j, i));

	    i = s.erase(j, boost::next(i));
	    s.insert(i, new ConstantNode(result));
	    continue;
	}

	if (AssignNode const *a = dynamic_cast<AssignNode const *>(&*i)) {
	    Stack::iterator vali = boost::prior(i);
	    Stack::iterator vari = boost::prior(vali);
	    a->assign(getVariableNode(vari)->getVar(), getNumberNode(vali)->getValue());
	    i = s.erase(vari, boost::next(i));
	    continue;
	}

	if (FuncDefNode const *f = dynamic_cast<FuncDefNode const *>(&*i)) {
	    f->assign();
	    i = s.erase(i);
	    continue;
	}

	++i;
    }

    if (s.empty())
	return 0.0;
    
    /* If stack size = 1 */
    BOOST_ASSERT(s.size() == 1) ;
    return getNumberNode(s.begin())->getValue();
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
