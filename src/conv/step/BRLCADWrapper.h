/*                 BRLCADWrapper.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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

#ifndef BRLCADWRAPPER_H_
#define BRLCADWRAPPER_H_

#include "common.h"

#include <string>

extern "C" {
/* brlcad headers */
#include <bu.h>
#include <wdb.h>
}


class ON_Brep;

class BRLCADWrapper {
 private:
    std::string filename;
    struct rt_wdb *outfp;
    static int sol_reg_cnt;

 public:
    BRLCADWrapper();
    virtual ~BRLCADWrapper();
    bool OpenFile(const char *filename);
    bool WriteHeader();
    bool WriteSphere(double *center, double radius);
    bool WriteBrep(std::string name, ON_Brep *brep);
    bool Close();
};


#endif /* BRLCADWRAPPER_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
