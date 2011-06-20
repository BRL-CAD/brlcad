/*                 DimensionalExponent.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2010 DimensionalExponented States Government as represented by
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
/** @file step/DimensionalExponents.h
 *
 * Class definition used to convert STEP "DimensionalExponents" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef DIMENSIONALEXPONENTS_H_
#define DIMENSIONALEXPONENTS_H_

#include "STEPEntity.h"

class DimensionalExponents : public STEPEntity {
private:
	static string entityname;

protected:
	  double length_exponent;
	  double mass_exponent;
	  double time_exponent;
	  double electric_current_exponent;
	  double thermodynamic_temperature_exponent;
	  double amount_of_substance_exponent;
	  double luminous_intensity_exponent;

public:
	DimensionalExponents();
	virtual ~DimensionalExponents();
	DimensionalExponents(STEPWrapper *sw,int step_id);
	bool Load(STEPWrapper *sw,SCLP23(Application_instance) *sse);
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* DIMENSIONALEXPONENTS_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
