/*                       P C M A T H V M . H
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
/** @file pcMathVM.h
 *
 * Math Virtual Machine for evaluating arbitrary math expressions
 *
 * Code is based on concepts from YAC by Angus Leeming
 *
 */
#ifndef __PCMATHVM_H__
#define __PCMATHVM_H__

#include "common.h"

#include <boost/shared_ptr.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/spirit/include/classic.hpp>

#include <iostream>
#include <string>
#include <list>
#include <vector>

class Stack;

/**
 * Node element : holds each individual entity in an expression
 * to be evaluated
 */
struct Node {
    /** Destructor */
    virtual ~Node() {}

    /** Clone method */
    virtual boost::shared_ptr<Node> clone() const = 0;
    
    /** Branch Methods */
    virtual std::size_t branchSize() const { return 0; }
    virtual Stack *branch(std::size_t) { return 0; }
};


/** Stack object: A collection of Node pointers and associated methods */
class Stack
{
public:
    typedef std::list<boost::shared_ptr<Node> > container_t;
    typedef container_t::size_type size_type;
    typedef container_t::difference_type difference_type;
    typedef boost::indirect_iterator<container_t::iterator> iterator;
    typedef boost::indirect_iterator<container_t::const_iterator> const_iterator;

    /** Create and Copy Constructors*/
    Stack() {}
    Stack(Stack const &);

    /** Data access/mnodification methods */
    size_type size() const;
    bool empty() const;
    void push_back(Node *n);
    void clear();

    /** Stack iterator methods */
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    iterator erase(iterator location);
    iterator erase(iterator begin, iterator end);

    iterator insert(iterator location, Node *n);

    /** Operator overloading */
    Stack & operator=(Stack const & other);
    Stack & operator+=(Stack const & other);

private:
    container_t data;
    void copy(Stack::container_t const &);
};


struct MathFunction;

/** The core Math Virtual Machine Object */
struct MathVM
{
    Stack stack;
    boost::spirit::classic::symbols<double> variables;
    boost::spirit::classic::symbols<boost::shared_ptr<MathFunction> > functions;
    void display();
};


/** Stack evaluation function (TODO: Consider shifting to Stack object)*/
double evaluate(Stack s);

struct UserFunction;

struct MathFunction
{
    MathFunction() {}
    MathFunction(std::string const &);
    virtual ~MathFunction() {}
    
    virtual UserFunction *asUserFunction();

    /** Data access methods */
    std::string const & getName() const;
    virtual std::size_t arity() const = 0;

    /** The evaluation method */
    double eval(std::vector<double> const & args) const;
    void display() {
	std::cout << MathFunction::name << std::endl;
    }
private:
    virtual double evalp(std::vector<double> const & args) const = 0;
    std::string name;

};


/** Implemation of various convenience Function types*/

/** Unary function */
template<typename T>
struct MathF1 : public MathFunction
{
    /* function pointer to a function taking a unary argument */
    typedef T (* function_ptr) (T);

    MathF1(std::string const & function_name, function_ptr fp)
	: MathFunction(function_name),
	  funct(fp)
    {}
    
    /* Implementation of the virtual function in MathFunction class
     * to return an arity of 1
     */
    std::size_t arity() const { return 1;}
private:
    double evalp(std::vector<double> const & args) const
    {
	return funct(static_cast<T>(args[0]));
    }
    function_ptr funct;
};


/** Binary function */
template<typename T>
struct MathF2 : public MathFunction
{
    /* function pointer to a function taking two arguments */
    typedef T (* function_ptr) (T, T);
    MathF2(std::string const & function_name, function_ptr fp)
	: MathFunction(function_name),
	  funct(fp)
    {
	std::cout << "Constructed " << function_name << std::endl;
    }
    /** arity return method */
    std::size_t arity() const { return 2; }
private:
    double evalp(std::vector<double> const & args) const
    {
	return funct(static_cast<T>(args[0]), static_cast<T>(args[1]));
    }
    function_ptr funct;
};


/** UserFunction Defintion */
struct UserFuncExpression;
struct UserFunction : public MathFunction
{
    typedef boost::spirit::classic::symbols<double> symboltable;
    typedef boost::shared_ptr<symboltable> stptr;

    /** Create and Copy constructors */
    UserFunction();
    UserFunction(std::string const & name, std::size_t const farity);
    UserFunction(UserFunction const & other);
    UserFunction & operator=(UserFuncExpression const & ufe);

    /** Data access methods */
    UserFunction *asUserFunction();
    boost::spirit::classic::symbols<double> const & localvariables() const;

    /** Arity return method */
    std::size_t arity() const;
private:
    /** XXX standardize underscoring variables */
    double evalp(std::vector<double> const & args) const;
    std::size_t arity_;
    std::vector<std::string> argnames;
    symboltable localvariables_;
    Stack stack;
    UserFunction & operator=(UserFunction const &);
};


/** Node Implementations */

struct NumberNode : public Node
{
    virtual double getValue() const = 0;
};


struct FunctionNode : public Node
{
    virtual MathFunction const & func() const = 0;
};


struct AssignNode : public Node
{
    boost::shared_ptr<Node> clone() const;
    void assign(double & var, double val) const;
};


struct ConstantNode : public NumberNode
{
    ConstantNode(double v);
    boost::shared_ptr<Node> clone() const;
    double getValue() const;
private:
    double value;
};


struct VariableNode : public NumberNode
{
    VariableNode(double *p);
    boost::shared_ptr<Node> clone() const;

    /** Data access methods */
    double getValue() const; /* Get the variable value */
    double & getVar() const; /* Get the variable reference */

    double *pd;
};


struct sysFunctionNode : public FunctionNode
{
    sysFunctionNode(boost::shared_ptr<MathFunction> const & funcp);
    boost::shared_ptr<Node> clone() const;
    MathFunction const & func() const;
private:
    boost::shared_ptr<MathFunction> fp;
};


struct OrNode : public FunctionNode
{
    OrNode(Stack const &);
    boost::shared_ptr<Node> clone () const;

    MathFunction const & func() const;
    std::size_t nbranches() const;
    Stack *branch(std::size_t);
private:
    struct OrFunc : public MathFunction {
    	OrFunc(Stack const &);

	std::size_t arity() const;
	double evalp(std::vector<double> const & params) const;

	Stack rhs_stack_;
    };

    OrFunc func_;
};


struct BranchNode : public FunctionNode
{
    BranchNode(Stack const & stack1, Stack const & stack2);
    boost::shared_ptr<Node> clone() const;
    MathFunction const & func() const;

    std::size_t nbranches() const;
    Stack *branch(std::size_t i);
private:
    struct BranchFunc : public MathFunction {
	BranchFunc(Stack const & stack1, Stack const & stack2);
	
	std::size_t arity() const;
	double evalp(std::vector<double> const & params) const;

	Stack stack1_;
	Stack stack2_;
    };
    BranchFunc func_;
};


struct UserFuncExpression
{
    UserFuncExpression(std::vector<std::string> const & arnam, \
		       boost::shared_ptr<boost::spirit::classic::symbols<double> > const & locvar,
		       Stack const & s)
    	: argnames(arnam), localvars(locvar), stack(s)
    {}
    std::vector<std::string> argnames;
    boost::shared_ptr<boost::spirit::classic::symbols<double> > localvars;
    Stack stack;
};


struct FuncDefNode : public Node
{
    FuncDefNode(boost::shared_ptr<MathFunction> const & funcptr, \
    		UserFuncExpression const & value);
    boost::shared_ptr<Node> clone() const;
    void assign() const;
private:
    boost::shared_ptr<MathFunction> funcptr_;
    UserFuncExpression value_;
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
