/*                   I F C W R A P P E R . H
 * BRL-CAD
 *
 * Copyright (c) 1994-2021 United States Government as represented by
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
/** @file step/ifc-g/IFCWrapper.h
 *
 * Class definition for C++ wrapper to NIST STEP parser/database
 * functions.
 *
 */

#ifndef CONV_STEP_IFCWRAPPER_H
#define CONV_STEP_IFCWRAPPER_H

#include "common.h"

#ifdef DEBUG
#define REPORT_ERROR(arg) std::cerr << __FILE__ << ":" << __LINE__ << ":" << arg << std::endl
#else
#define REPORT_ERROR(arg)
#endif

/* system headers */
#include <list>
#include <map>
#include <vector>

/* interface headers */
#include <sdai.h>

#include <STEPattribute.h>
#include <STEPcomplex.h>
#include <STEPfile.h>

#include "SdaiIFC4.h"

#define IFC_LOADED 1
#define IFC_LOAD_ERROR 2

class IFCWrapper
{
private:
    std::string ifcfile;
    InstMgr *instance_list;
    Registry  *registry;
    STEPfile  *sfile;
    bool verbose;

    void printEntity(SDAI_Application_instance *se, int level);
    void printEntityAggregate(STEPaggregate *sa, int level);
    const char *getBaseType(int type);

public:
    IFCWrapper();
    virtual ~IFCWrapper();

    std::map<int,int> entity_status;
    char *summary_log_file;
    int dry_run;

    bool load(std::string &step_file);
    void printLoadStatistics();

    bool Verbose() const
    {
	return verbose;
    }

    void Verbose(bool value)
    {
	this->verbose = value;
    }
};

#endif /* CONV_STEP_IFCWRAPPER_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
