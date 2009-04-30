/*                 STEPWrapper.h
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
/** @file STEPWrapper.h
 *
 * Class definition for C++ wrapper to NIST STEP parser/database functions.
 *
 */
#ifndef STEPWRAPPER_H_
#define STEPWRAPPER_H_

#include <sdai.h>
#include <STEPfile.h>
#include <SdaiCONFIG_CONTROL_DESIGN.h>

#include <BRLCADWrapper.h>

class STEPWrapper {
private:
	string stepfile;
	InstMgr instance_list;
	Registry  *registry;
	STEPfile  *sfile;
	BRLCADWrapper *dotg;
	void printEntity(SCLP23(Application_instance) *se, int level);
	void printEntityAggregate(STEPaggregate *sa, int level);
public:
	STEPWrapper();
	virtual ~STEPWrapper();
	bool loadSTEP(string &stepfile);
	void printLoadStatistics();
	bool convertSTEP(BRLCADWrapper *dotg);
};

#endif /* STEPWRAPPER_H_ */
