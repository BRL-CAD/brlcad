#include "common.h"

#include "bio.h"
#include <string>
#include <vector>
#include <cassert>

#include "ged.h"

const size_t MAX_ARGS = 5;
const std::string OUTPUT = "fuzz_ged.g";


static size_t getArgc(uint8_t byte) {
    return (byte % MAX_ARGS) + 1;
}


static const std::string& getCommand(uint8_t byte) {
    static const std::vector<std::string> commands = {
	"help",
	"in",
	"ls",
	"tops"
    };

    return commands[byte % commands.size()];
}


static const std::string& getArg(uint8_t byte) {
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


static void printCommand(std::vector<std::string> & argv) {
    std::cout << "Running";
    for (size_t j = 0; j < argv.size(); j++)
	std::cout << " " << argv[j];
    std::cout << std::endl;
}


extern "C" int LLVMFuzzerTestOneInput(const int8_t *data, size_t size) {

    if (size == 0)
	return 0;

    size_t i = 0;
    size_t argc = 0;

    if (i < size)
	argc = getArgc(data[i++]);

    std::vector<std::string> argv;
    argv.resize(argc+1, "");
    argv[0] = getCommand(data[i++]);

    for (size_t j = 1; j < argc; j++) {
	if (i < size) {
	    argv[j] = getArg(data[i++]);
	} else {
	    /* loop around if we run out of slots */
	    argv[j] = getArg(data[i++ % size]);
	}
    }

    printCommand(argv);

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
