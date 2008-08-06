/*                       P C P A R A M E T E R . H
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
/** @file pcParameter.h
 *
 * Parameter Class abstraction over the Variable class
 *
 * @author Dawn Thomas
 */
#ifndef __PCPARAMETER_H__
#define __PCPARAMETER_H__

#include "common.h"

#include "pcVariable.h"

#include <string>
#include <list>

class Parameter

{
public:
    std::string getName();
private:
    std::string name;
    std::list<VariableAbstract *> Variables;
};

class Vector : public Parameter
{

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
