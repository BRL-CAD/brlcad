/*                       P C C O N S T R A I N T . H
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
/** @file pcConstraint.h
 *
 * Constraint Class for Parametrics and Constraints Library
 *
 * @author Dawn Thomas
 */
#ifndef __PCCONSTRAINT_H__
#define __PCCONSTRAINT_H__

#include <stdarg.h>
#include "pcVariable.h"

template<class T>
class Constraint {
private:
    int status;
    std::string id;
    std::string expression;
    bool (*funct) (std::vector<T>);
public:

    typename std::list<std::string> Variables;

    Constraint() { status = 0; } 
    Constraint(std::string Cid, std::string Cexpression, bool (*pf) (std::vector<T>));
    Constraint(std::string Cid, std::string Cexpression, bool (*pf) (std::vector<T>),int count,...);
    void function(bool (*pf) (std::vector<T>)) { funct = pf; };
    bool solved() { 
	if (status == 0) return false;
	else return true;
	}
    /* TODO: Take a functor as an argument? */
    bool check(std::vector<T> V);
    std::string getID() const { return id; }
    std::string getExp() const { return expression; }
    void setStatus(int st) { status = st; }

};

template <class T>
Constraint<T>::Constraint(std::string Cid, std::string Cexpression, bool (*pf) (std::vector<T>)) { 
    status = 0;
    id = Cid;
    expression = Cexpression;
    funct = pf;
}

template <class T>
Constraint<T>::Constraint(std::string Cid, std::string Cexpression, bool (*pf) (std::vector<T>),int count,...) { 
    status = 0;
    id = Cid;
    expression = Cexpression;
    funct = pf;

    va_list args;
    va_start(args,count);
    for( int i=0; i<count; i++) {
	Variables.push_back(va_arg(args,char *));
    }
    va_end(args);
}

template <class T>
bool Constraint<T>::check(std::vector<T> V) {
	typename std::vector<T>::iterator i;
	/*std::cout<<"##Checking for Values";
	for( i = V.begin(); i!= V.end(); i++) std::cout<<" "<< *i;
	std::cout<< " for the constraint " << getExp() <<std::endl;*/
	if (funct(V)) {
	    status =1;
	    return true;
	}
	else {
	    status = 0;
	    return false;
	}
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
