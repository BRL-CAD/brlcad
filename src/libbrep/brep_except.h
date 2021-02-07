/*                   B R E P _ E X C E P T . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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

#ifndef BREP_EXCEPT_H
#define BREP_EXCEPT_H

#include "common.h"

#include <stdexcept>
#include <string>


class InvalidBooleanOperation : public std::invalid_argument
{
public:
InvalidBooleanOperation(const std::string &msg = "") : std::invalid_argument(msg) {}
};


class InvalidGeometry : public std::invalid_argument
{
public:
InvalidGeometry(const std::string &msg = "") : std::invalid_argument(msg) {}
};


class AlgorithmError : public std::runtime_error
{
public:
AlgorithmError(const std::string &msg = "") : std::runtime_error(msg) {}
};


class GeometryGenerationError : public std::runtime_error
{
public:
GeometryGenerationError(const std::string &msg = "") : std::runtime_error(msg) {}
};


class IntervalGenerationError : public std::runtime_error
{
public:
IntervalGenerationError(const std::string &msg = "") : std::runtime_error(msg) {}
};


class InvalidInterval : public std::runtime_error
{
public:
InvalidInterval(const std::string &msg = "") : std::runtime_error(msg) {}
};


#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
