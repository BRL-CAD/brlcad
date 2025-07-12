/*                 P I P E . H
 * BRL-CAD
 *
 * Copyright (c) 2024-2025 United States Government as represented by
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
 /** @file sphere.h
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate sphere implementation
  */

#ifndef SPHERE_INCLUDED
#define SPHERE_INCLUDED

#include "common.h"
#include "wdb.h"



class Sphere {
public:
    Sphere(void);

    void                     setName(const char* value);
    void                     addSphere(const char* sphereName,
				       const point_t& center,
				       const float& radious);

    std::vector<std::string> write(rt_wdb* wdbp);

private:
    std::string                      m_name;
    std::map<std::string,rt_part_internal>  m_sphere;
};


#endif // !SPHERE_INCLUDED


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
