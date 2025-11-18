#include "common.h"

#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include "bu/app.h"
#include "bu/opt.h"
#include "bu/vls.h"
}

#include "ComGeomConverter.h"

/* Custom arg processor to copy a string argument into a std::string */
static int
opt_set_string(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "string");
    if (!set_var) return -1;
    std::string *sp = static_cast<std::string *>(set_var);
    *sp = argv[0]; /* copy (bu_opt_str would just set a char *) */
    return 1;      /* consumed one argument */
}

struct CliOptions {
    ConverterOptions conv;
    int show_help = 0;              /* use int for bu_opt compatibility */
    std::vector<std::string> positionals;
};

/* Produce usage/help text */
static void usage(const char *argv0, const struct bu_opt_desc *opt_defs)
{
    std::fprintf(stderr,
        "Usage: %s [options] input_file output_file\n\n", argv0);

    /* Build help description string from bu_opt system */
    char *help = bu_opt_describe(opt_defs, NULL);
    if (help) {
        std::fprintf(stderr, "%s\n", help);
        bu_free(help, "help string");
    }
    std::fprintf(stderr,
        "\nExamples:\n"
        "  %s model.cg model.g              # convert using defaults (version 5)\n"
        "  %s -v 4 -s _cg4 in.cg out.g      # convert v4, append suffix to names\n"
        "  %s -d 2 input.cg out.g           # verbose debug level 2\n",
        argv0, argv0, argv0);
}

int main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);

    CliOptions co;

    /* Prepare option descriptor array.
     * If arg_process is NULL, bu_opt will set set_var (int) to 1 when flag is present.
     */
    struct bu_opt_desc opt_defs[6];
    BU_OPT(opt_defs[0], "h", "help",    "",  NULL,             &co.show_help,
           "Show help and exit.");
    BU_OPT(opt_defs[1], "v", "version", "N", &bu_opt_int,      &co.conv.version,
           "Set COMGEOM input version (1,4,5; default 5).");
    BU_OPT(opt_defs[2], "d", "debug",   "N", &bu_opt_int,      &co.conv.verbose,
           "Set verbosity/debug level (integer).");
    BU_OPT(opt_defs[3], "s", "suffix",  "STR", opt_set_string, &co.conv.suffix,
           "Append name suffix to all generated objects.");
    /* Allow multiple -V occurrences to increment verbosity as an alternative */
    BU_OPT(opt_defs[4], "V", "verbose", "",  &bu_opt_incr_long, &co.conv.verbose,
           "Increment verbosity (may be repeated).");
    BU_OPT_NULL(opt_defs[5]);

    /* We need a mutable argv copy (const correctness per bu_opt_parse signature) */
    std::vector<const char *> argvec;
    argvec.reserve(static_cast<size_t>(argc - 1));
    for (int i = 1; i < argc; ++i) {
        argvec.push_back(argv[i]);
    }

    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    int ret = bu_opt_parse(&msgs, argvec.size(), argvec.data(), opt_defs);

    if (ret < 0) {
        /* Parsing failed; print messages and usage */
        if (BU_VLS_IS_INITIALIZED(&msgs) && bu_vls_strlen(&msgs) > 0)
            std::fprintf(stderr, "%s", bu_vls_addr(&msgs));
        usage(argv[0], opt_defs);
        bu_vls_free(&msgs);
        return 1;
    }

    /* ret = number of unused (positional) args moved to front of array */
    size_t unused = static_cast<size_t>(ret);
    for (size_t i = 0; i < unused; ++i) {
        co.positionals.emplace_back(argvec[i]);
    }

    if (co.show_help) {
        usage(argv[0], opt_defs);
        bu_vls_free(&msgs);
        return 0;
    }

    /* Validate positional count */
    if (co.positionals.size() != 2) {
        std::fprintf(stderr,
                     "Error: expected 2 positional arguments (input_file output_file), got %zu.\n",
                     co.positionals.size());
        usage(argv[0], opt_defs);
        bu_vls_free(&msgs);
        return 1;
    }

    /* Validate version */
    if (co.conv.version != 1 && co.conv.version != 4 && co.conv.version != 5) {
        std::fprintf(stderr, "Error: unsupported COMGEOM version %d (valid: 1, 4, 5).\n",
                     co.conv.version);
        usage(argv[0], opt_defs);
        bu_vls_free(&msgs);
        return 1;
    }

    /* Optionally echo debug messages collected */
    if (BU_VLS_IS_INITIALIZED(&msgs) && bu_vls_strlen(&msgs) > 0 && co.conv.verbose) {
        std::fprintf(stderr, "%s", bu_vls_addr(&msgs));
    }
    bu_vls_free(&msgs);

    const std::string inputPath  = co.positionals[0];
    const std::string outputPath = co.positionals[1];

    ComGeomConverter converter(inputPath, outputPath, co.conv);
    return converter.run();
}