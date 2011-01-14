/*                    P C V A R I A B L E . C P P
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
/** @file pcVariable.cpp
 *
 * Methods for Variable Class
 *
 */

#include "pcVariable.h"

/* Abstract Variable Class methods */
VariableAbstract::VariableAbstract(std::string vid) :
    type(VAR_ABS),
    const_(false),
    id(vid),
    constrained_(0)
{
}


VariableAbstract::~VariableAbstract()
{
}


void VariableAbstract::display()
{
}


void VariableAbstract::store()
{
}


void VariableAbstract::restore()
{
}
/* Constructor specialization for specific variable types */
template<>
Variable<int>::Variable(std::string vid, int vvalue) :
    VariableAbstract(vid),
    value(vvalue)
{
    double min, max;
    VariableAbstract::type = VAR_INT;
#ifdef DBL_MAX
    min = -DBL_MAX;
    max = DBL_MAX;
#else
    min = -std::numeric_limits<double>::max();
    max = std::numeric_limits<double>::max();
#endif
    addInterval(Interval<int>(min, max, 1));
}


template<>
Variable<double>::Variable(std::string vid, double vvalue) :
    VariableAbstract(vid),
    value(vvalue)
{
    double min, max;
    VariableAbstract::type = VAR_DBL;
#ifdef DBL_MAX
    min = -DBL_MAX;
    max = DBL_MAX;
#else
    min = -std::numeric_limits<double>::max();
    max = std::numeric_limits<double>::max();
#endif
    addInterval(Interval<double>(min, max, .00001));
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
