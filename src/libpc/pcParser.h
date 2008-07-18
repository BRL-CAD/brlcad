/*                    P C P A R S E R . H
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
/** @addtogroup pcparser */
/** @{ */
/** @file pcParser.h
 *
 * EBNF Parser generation using boost:Spirit
 *
 * @author Dawn Thomas
 */
#ifndef __PCPARSER_H__
#define __PCPARSER_H__

#include "common.h"

#include "pcPCSet.h"
#include <boost/spirit.hpp>

#include "bu.h"
#include "raytrace.h"
#include "tcl.h"
#include "pc.h"

class Parser;

/* Functors associated with the generation of Variables */
struct varname
{
public:
    varname(Parser &pars) : pcparser(pars) {}
    void operator () (char c) const;
private:
    Parser &pcparser;
};

struct varvalue
{
public:
    varvalue(Parser &pars) : pcparser(pars) {}
    void operator () (double v) const;
private:
    Parser &pcparser;
};

/**
 * Grammar for the expression used for representing the constraint
 *
 */
class Variable_grammar : public boost::spirit::grammar<Variable_grammar>
{
private:
    Parser &pcparser;
public:
    Variable_grammar(Parser &parser);
    virtual ~Variable_grammar();
    template<typename ScannerT>
    struct definition
    {
        boost::spirit::rule<ScannerT> variable;
	/*boost::spirit::rule<ScannerT> expression;*/
	definition(Variable_grammar const &self) {
	    /*expression
	        =    boost::spirit::real_p[varvalue(self.pcparser)]
		;*/
	    variable
	        =    *(boost::spirit::alnum_p)[varname(self.pcparser)]
		     >> '='
	             >>boost::spirit::real_p[varvalue(self.pcparser)]
		     /* expression*/
		;
	}
	boost::spirit::rule<ScannerT> const& start() { return variable;}
    };
};

class Constraint_grammar : public boost::spirit::grammar<Constraint_grammar>
{
private:
    Parser &pcparser;
public:
    Constraint_grammar(Parser &parser);
    virtual ~Constraint_grammar();
    template<typename ScannerT>
    struct definition
    {
        boost::spirit::rule<ScannerT> variable;
	boost::spirit::rule<ScannerT> term;
	boost::spirit::rule<ScannerT> operat;
	boost::spirit::rule<ScannerT> expression;
	boost::spirit::rule<ScannerT> eq;
	boost::spirit::rule<ScannerT> constraint;
	definition(Constraint_grammar const &self) {
	    variable
	        =    boost::spirit::alnum_p
		;
	    term
	        =    boost::spirit::real_p
		|    variable
		;
	    eq
	        =    '<'
		|    '='
		|    '>'
		;
            expression
	        =    term
		     >> *(operat >> term )
		;
	    constraint
	        =    '('
		     >> expression
		     >> eq
		     >> expression
		     >> ')'
		;
	}
	boost::spirit::rule<ScannerT> const& start() { return constraint;}
    };
};

/**
 *  Various precompiled functors which are called during parsing depending
 *  on the constraint represented in the expression
 */

struct is_equal
{
    template<typename IteratorT>
    void operator() (IteratorT first, IteratorT last) const;
};

/**
 *
 * The Parser Object which takes the pc_pc_set as the input and performs
 * the following functions
 * 1. Generate Variables corresponding to pc_params
 * 2. Generate Constraint objects corresponding to pc_constrnt by parsing
 *    according to the pcexpression_grammar
 * 3. Return the set of Variables and Constraints/ Call the Generator
 */
class Parser {
private:
    typedef PCSet<double> PC_Set;/* TODO: parametrize the hardcoded double */
    std::string name;
    double value;
    PC_Set &pcset;
    Variable_grammar *var_gram;
    Constraint_grammar *con_gram;
public:
    Parser(PC_Set &pcs);
    virtual ~Parser();
    void parse(struct pc_pc_set * pcs);
    void pushChar(char c) { name.push_back(c); }
    void setValue(double v) { value = v; } 
    void display() { std::cout<< "Result of Parsing:" << name << " = " <<  value << std::endl; }
};

void varname::operator () (char c) const {
    pcparser.pushChar(c);
}

void varvalue::operator () (double v) const {
    pcparser.setValue(v);
}
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
