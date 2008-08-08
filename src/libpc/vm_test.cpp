/*                        V M _ T E S T . C P P
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
/** @file vm_test.cpp
 *
 * @brief Simple Test cases for Math Virtual Machine
 *
 * @author Dawn Thomas
 *
 */

#include "common.h"

#include "pcMathVM.h"
#include <iostream>
#include <cmath>


typedef boost::shared_ptr<MathFunction> ct;
using boost::spirit::classic::find;

/* Test functions */

double add(double a, double b) { return a+b; }
/* Type definitions for unary and binary function pointers */

typedef double (* function2_ptr) (double, double);
typedef double (* function1_ptr) (double);

/* Binary function maker */
boost::shared_ptr<MathFunction> make_function(char const * name, function2_ptr f2_p)
{
    return boost::shared_ptr<MathFunction>(new MathF2<double>(name, f2_p));
}

/* Unary function maker */
boost::shared_ptr<MathFunction> make_function(char const * name, function1_ptr f1_p)
{
    return boost::shared_ptr<MathFunction>(new MathF1<double>(name, f1_p));
}

/* Arity based function maker 
boost::shared_ptr<MathFunction> make_function(char const * name, int arity)
{
    return boost::shared_ptr<MathFunction>(new UserFunction(name, arity));
}
*/
void findfunction(ct ** ap, const char * s,MathVM & vm) {
   *ap = find<ct>(vm.functions,s);
   if (! *ap) {
	std::cout << "Function not found" << std::endl;
   } else {
	std::cout << "Function found " << (**ap)->arity() << std::endl;
	(**ap)->display();
   }
}

void eval()
{
    MathVM vm;
    ct * a;
    std::vector<double> args;
    std::cout << "MathVM evaluation" << std::endl;
    vm.functions.add("sin#1", make_function("sin",&sin))
		    ("add##2", make_function("add#", &add));
    findfunction(&a,"sin#1",vm);
    if(a) {
	args.push_back(0.7585);
	std::cout << (*a)->eval(args) <<std::endl;
	args.clear();
    }
    findfunction(&a,"add##2",vm);
    if(a) {
	args.push_back(24.3);
	args.push_back(42.3);
	std::cout << (*a)->eval(args) <<std::endl;
	args.clear();
    }
    findfunction(&a,"sub",vm);
}

int main()
{
    eval();
    return 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
