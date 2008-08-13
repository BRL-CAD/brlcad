/*                    P C G E N E R A T O R . C P P
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
/** @file pcGenerator.cpp
 *
 * Generator Class implementation as well as well as implementation of various
 * functors used in the generation of Variables and Constraints
 *
 * @author Dawn Thomas
 */

#include "pcGenerator.h"

void Generators::varname::operator () (char c) const {
    vcset.pushChar(c);
}

void Generators::varvalue::operator () (double v) const {
    vcset.setValue(v);
}
bool Generators::constraint2V::operator() (VCSet & vcset, std::list<std::string> Vid) const {
    typedef Variable<double> * Vi;
    double ** a = new double*[2];
    //a = (double **) malloc(2 *(sizeof(double *)));
    
    for (int i =0; i< 3; i++)
        a[i] = new double[3];
	//a[i] = (double *)malloc(3 *(sizeof(double)));
    for (int i =0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
	    a[i][j] = ((Vi) vcset.getVariablebyID(Vid.front()))->getValue();
	    Vid.pop_front();
	}
    }

    if (fp_) {
        if ( fp_(a) == 0) {
	    for (int i = 0 ; i < 3; i++)
		delete[] a[i];
	    delete[] a;
	    return true;
	} else {
	    for (int i = 0 ; i < 3; i++)
		delete[] a[i];
	    delete[] a;
	    return false;
	}
    } else {
	std::cout << "!!! Constraint evaluation pointer NULL\n";
    }
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
