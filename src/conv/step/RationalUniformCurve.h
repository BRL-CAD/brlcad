/*                 RationalUniformCurve.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file RationalUniformCurve.h
 *
 * Class definition used to convert STEP "RationalUniformCurve" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef RATIONALUNIFORMCURVE_H_
#define RATIONALUNIFORMCURVE_H_

#include "RationalBSplineCurve.h"
#include "UniformCurve.h"

class RationalUniformCurve : public RationalBSplineCurve, public UniformCurve {
private:
	static string entityname;

protected:

public:
	RationalUniformCurve();
	virtual ~RationalUniformCurve();
	RationalUniformCurve(STEPWrapper *sw,int STEPid);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual bool LoadONBrep(ON_Brep *brep);
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* RATIONALUNIFORMCURVE_H_ */
