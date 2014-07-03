/*                 BRLCADWrapper.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file step/BRLCADWrapper.h
 *
 * Class definition for C++ wrapper to BRL-CAD database functions.
 *
 */

#ifndef CONV_STEP_BRLCADWRAPPER_H
#define CONV_STEP_BRLCADWRAPPER_H

#include "common.h"

#include <string>
#include <map>

extern "C" {
    /* brlcad headers */
#include <bu.h>
#include <wdb.h>
#include <raytrace.h>
}


class ON_Brep;

typedef std::map<std::string, struct bu_list *> MAP_OF_BU_LIST_HEADS;

class BRLCADWrapper
{
private:
    std::string filename;
    struct rt_wdb *outfp;
    struct db_i *dbip;
    static int sol_reg_cnt;

public:
    BRLCADWrapper();
    virtual ~BRLCADWrapper();
    bool load(std::string &filename);
    bool OpenFile(std::string &filename);
    bool WriteHeader();
    bool WriteSphere(double *center, double radius);
    bool WriteBrep(std::string name, ON_Brep *brep, mat_t &mat);
    bool WriteCombs();
    bool AddMember(const std::string &combname,const std::string &member,mat_t mat);
    std::string GetBRLCADName(std::string &name);
    static void getRandomColor(unsigned char *rgb);
    struct db_i * GetDBIP();
    bool Close();
};


#endif /* CONV_STEP_BRLCADWRAPPER_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
