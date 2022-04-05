/*                    F U Z Z _ G E D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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

#include "common.h"

#include "bio.h"
#include <string>
#include <vector>
#include <cassert>

#include "bu.h"
#include "bv.h"
#include "ged.h"

const size_t MAX_ARGS = 5;
//const std::string OUTPUT = "fuzz_ged.g";


static size_t
getArgc(uint8_t byte)
{
    return (byte % MAX_ARGS) + 1;
}


static const std::string&
getCommand(uint8_t byte)
{
    static const std::vector<std::string> commands = {
	"help",
	"in",
	"ls",
	"tops"
    };

    return commands[byte % commands.size()];
}


static const std::string&
getArg(uint8_t byte)
{
    static const std::vector<std::string> args = {
	"COMMAND",

	"-a",
	"-b",
	"-c",
	"-d",
	"-e",
	"-f",
	"-g",
	"-h",
	"-i",
	"-j",
	"-k",
	"-l",
	"-m",
	"-n",
	"-o",
	"-p",
	"-q",
	"-r",
	"-s",
	"-t",
	"-u",
	"-v",
	"-w",
	"-x",
	"-y",
	"-z",

	"a",
	"b",
	"c",
	"x",
	"y",
	"z",

	/* auto-extracted using:
	 * grep \", \ \" src/librt/primitives/table.cpp | grep FUNC | cut -f3 -d, | perl -p -0777 -e 's/"\n/", \n/g'
	 */

	"NULL",
	"tor",
	"tgc",
	"ell",
	"arb8",
	"ars",
	"half",
	"rec",
	"poly",
	"bspline",
	"sph",
	"nmg",
	"ebm",
	"vol",
	"arbn",
	"pipe",
	"part",
	"rpc",
	"rhc",
	"epa",
	"ehy",
	"eto",
	"grip",
	"joint",
	"hf",
	"dsp",
	"sketch",
	"extrude",
	"submodel",
	"cline",
	"bot",
	"comb",
	"unused1",
	"binunif",
	"unused2",
	"superell",
	"metaball",
	"brep",
	"hyp",
	"constrnt",
	"revolve",
	"pnts",
	"annot",
	"hrt",
	"datum",
	"script",

	"u",
	"-",
	"+",

	"-5",
	"-4",
	"-3",
	"-1",
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",

	"-5.0",
	"-1.0",
	"-0.5",
	"-0.05",
	"0.0",
	"0.05",
	"0.5",
	"1.0",
	"5.0"
    };

    assert(args.size() < UCHAR_MAX);

    size_t idx = byte % args.size();
    if (idx == 0)
	return getCommand(byte);
    return args[idx];
}


static void
printCommand(std::vector<const char *> & argv)
{
    std::cout << "Running";
    for (size_t j = 0; j < argv.size(); j++)
	std::cout << " " << argv[j];
    std::cout << std::endl;
}


extern "C" int
LLVMFuzzerTestOneInput(const int8_t *data, size_t size)
{
    if (size == 0)
	return 0;

    size_t i = 0;
    size_t argc = 0;

    if (i < size)
	argc = getArgc(data[i++]);

    std::vector<const char *> argv;
    argv.resize(argc+1, "");
    argv[0] = getCommand(data[i++]).c_str();

    for (size_t j = 1; j < argc; j++) {
	if (i < size) {
	    argv[j] = getArg(data[i++]).c_str();
	} else {
	    /* loop around if we run out of slots */
	    argv[j] = getArg(data[i++ % size]).c_str();
	}
    }

    printCommand(argv);

    struct ged g;
    struct rt_wdb *wdbp;
    struct db_i *dbip;

    dbip = db_create_inmem();
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);

    GED_INIT(&g, wdbp);

    /* FIXME: To draw, we need to init this LIBRT global */
    BU_LIST_INIT(&RTG.rtg_vlfree);

    /* Need a view for commands that expect a view */
    struct bview *gvp;
    BU_GET(gvp, struct bview);
    bv_init(gvp);
    g.ged_gvp = gvp;

    void *libged = bu_dlopen(NULL, BU_RTLD_LAZY);
    if (!libged) {
	std::cout << "ERROR: unable to dynamically load" << std::endl;
	return 1;
    }

    int (*func)(struct ged *, int, char *[]);
    std::string funcname = std::string("ged_") + argv[0];

    *(void **)(&func) = bu_dlsym(libged, funcname.c_str());

    int ret = 0;
    if (func) {
	ret = func(&g, argc, (char **)argv.data());
    }
    bu_dlclose(libged);

    ged_close(&g);

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
