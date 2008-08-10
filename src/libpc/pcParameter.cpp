/*                       P C P A R A M E T E R . C P P
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
/** @file pcParameter.cpp
 *
 * Parameter Class Methods
 *
 * @author Dawn Thomas
 */
#include "pcParameter.h"
#include "pc.h"

Parameter::Parameter(VCSet & vcs, std::string n)
    : vcset(vcs), name(n)
{}

std::string Parameter::getName()
{
    return name;
}

Vector::Vector(VCSet & vcs,std::string n)
    : Parameter(vcs, n)
{
    Parameter::setType(PC_DB_VECTOR_T);
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
