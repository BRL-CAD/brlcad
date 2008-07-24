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

#include <string>
#include <iostream>
#include <boost/spirit/include/classic.hpp>

#include "pcPCSet.h"
#include "pcGenerator.h"

#include "bu.h"
#include "raytrace.h"
#include "pc.h"


typedef PCSet<double> PCSetd;

/**
 * Grammar for the expression used for representing the constraint
 *
 */
class Variable_grammar : public boost::spirit::classic::grammar<Variable_grammar>
{
private:
    PCSetd &pcset;
public:
    Variable_grammar(PCSet<double> &pcset);
    virtual ~Variable_grammar();
    template<typename ScannerT>
    struct definition
    {
        boost::spirit::classic::rule<ScannerT> variable;
	/*boost::spirit::rule<ScannerT> expression;*/
	definition(Variable_grammar const &self) {
	    /*expression
	        =    boost::spirit::real_p[varvalue(self.pcparser)]
		;*/
	    variable
	        =    *(boost::spirit::classic::alnum_p)[Generators::varname(self.pcset)]
		     >> '='
	             >>boost::spirit::classic::real_p[Generators::varvalue(self.pcset)]
		     /* expression*/
		;
	}
	boost::spirit::classic::rule<ScannerT> const& start() { return variable;}
    };
};

class Constraint_grammar : public boost::spirit::classic::grammar<Constraint_grammar>
{
private:
    PCSetd &pcset;
public:
    Constraint_grammar(PCSet<double> &pcset);
    virtual ~Constraint_grammar();
    template<typename ScannerT>
    struct definition
    {
        boost::spirit::classic::rule<ScannerT> variable;
	boost::spirit::classic::rule<ScannerT> term;
	boost::spirit::classic::rule<ScannerT> operat;
	boost::spirit::classic::rule<ScannerT> expression;
	boost::spirit::classic::rule<ScannerT> eq;
	boost::spirit::classic::rule<ScannerT> constraint;
	definition(Constraint_grammar const &self) {
	    variable
	        =    boost::spirit::classic::alnum_p
		;
	    term
	        =    boost::spirit::classic::real_p
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
	boost::spirit::classic::rule<ScannerT> const& start() { return constraint;}
    };
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
    //void pushChar(char c) { name.push_back(c); }
    //void setValue(double v) { value = v; } 
    void display() { std::cout<< "Result of Parsing:" << name << " = " <<  value << std::endl; }
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
