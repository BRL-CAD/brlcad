/*                       A R C H E R  . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2026 United States Government as represented by
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

#include <string.h>

#ifdef HAVE_WINDOWS_H
#  include <direct.h> /* For chdir */
#endif

#include "bresource.h"
#include "bnetwork.h"
#include "bio.h"

#include "tcl.h"

#include "bu.h"
#include "tclcad.h"

// Tclcad pulls in OpenNURBS in C++ compilation mode, which defines None, which
// will conflict with Tk.h's Xlib None if we include tk.h before tclcad.h
#ifdef HAVE_TK
#  include "tk.h"
#endif

struct archer_args {
    int help;
};

static int
archer_help_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    if (arg && !BU_STR_EQUAL(arg, "all")) {
	if (msg)
	    bu_vls_strcat(msg, "--help accepts only the optional value 'all'\n");
	return -1;
    }
    if (storage)
	*((int *)storage) = arg ? 2 : 1;
    return 0;
}

static const struct bu_arg_option archer_options[] = {
    BU_ARG_ATTACHED_OPTIONAL("h", "help", "help", struct archer_args,
	help, archer_help_parse, "all", "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand archer_operands[] = {
    BU_ARG_OPERAND("geometry", BU_ARG_VALUE_FILE, 0, 1,
	"Geometry file to open or import", NULL),
    BU_ARG_OPERAND("argument", BU_ARG_VALUE_RAW, 0, BU_ARG_COUNT_UNLIMITED,
	"Additional argument passed to Tcl/Tk", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema archer_schema = BU_ARG_SCHEMA(
    "archer", "BRL-CAD graphical geometry editor", archer_options,
    archer_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar archer_grammar = {
    &archer_schema, NULL, NULL
};

#ifdef HAVE_WINDOWS_H
int APIENTRY
WinMain(HINSTANCE UNUSED(hInstance),
	HINSTANCE UNUSED(hPrevInstance),
	LPSTR UNUSED(lpszCmdLine),
	int UNUSED(nCmdShow))
{
    int argc = __argc;
    char **argv = __argv;
#else
int
main(int argc, const char **argv)
{
#endif

    int status = 0;

#if !defined(HAVE_TK)
    bu_exit(EXIT_FAILURE, "Error: Archer requires Tk.");
#else

    const char *archer_tcl = NULL;
    const char *fullname = NULL;
    struct archer_args args = {0};
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    struct bu_vls schema_output = BU_VLS_INIT_ZERO;
    struct bu_vls tlog = BU_VLS_INIT_ZERO;
    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;
    const char *result;
    Tcl_DString temp;
    Tcl_Interp *interp = NULL;

    /* initialize progname for runtime resource finding */
    bu_setprogname(argv[0]);

    status = bu_arg_introspect(&schema_output, &archer_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (status != 0) {
	bu_flog(status > 0 ? stdout : stderr, "%s", bu_vls_cstr(&schema_output));
	bu_vls_free(&schema_output);
	return status > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    bu_vls_free(&schema_output);

    /* Archer doesn't handle empty input strings well - scrub the argv */
    const char **av = (const char **)bu_calloc(argc, sizeof(char *), "argv cpy");
    int ac = 1;
    av[0] = argv[0];
    for (int i = 1; i < argc; i++) {
	if (strlen(argv[i]) > 0) {
	    av[ac] = argv[i];
	    ac++;
	}
    }
    status = bu_arg_schema_parse_known(&archer_schema, &args, &parse_msgs,
	ac - 1, av + 1);
    if (status < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&parse_msgs));
	bu_vls_free(&parse_msgs);
	bu_free((void *)av, "argv cpy");
	return EXIT_FAILURE;
    }
    bu_vls_free(&parse_msgs);

    if (args.help) {
	char *help = bu_arg_schema_help_mode(&archer_schema, bu_getprogname(),
	    args.help > 1 ? BU_ARG_HELP_EXTENDED : BU_ARG_HELP_DEFAULT);
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "archer help");
	bu_free((void *)av, "argv cpy");
	return EXIT_SUCCESS;
    }

    interp = Tcl_CreateInterp();

    /* Change the working directory to BU_DIR_HOME if we are invoking
     * without any arguments. */
    if (ac == 1) {
	const char *homed = bu_dir(NULL, 0, BU_DIR_HOME, NULL);
	if (homed && chdir(homed)) {
	    bu_free((void *)av, "av cpy");
	    bu_exit(1, "Failed to change working directory to \"%s\" ", homed);
	}
    }

    /* initialize Tcl args */
    bu_vls_sprintf(&tcl_cmd, "set av0 %s", av[0]);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    bu_vls_sprintf(&tcl_cmd, "set ::no_bwish 1");
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

    /* Pass on ac/av - for now, handling all that in Tcl/Tk land */
    tclcad_set_argv(interp, ac - 1, av + 1);

#ifdef HAVE_WINDOWS_H
    Tk_InitConsoleChannels(interp);
#endif

    status = tclcad_init(interp, 1, &tlog);

    if (status == TCL_ERROR) {
	/* If we had an init failure this may not work, but we may be launching
	 * from an icon so go ahead and try. */
	bu_vls_sprintf(&tcl_cmd, "tk_messageBox -message \"Error - tclcad init failure: %s\" -type ok", bu_vls_addr(&tlog));
	if (Tcl_Eval(interp, bu_vls_addr(&tcl_cmd)) == TCL_ERROR) {
	    /* go for bu_log as a last resort */
	    bu_log("archer tclcad init failure:\n%s\n", bu_vls_addr(&tlog));
	}
	bu_vls_free(&tlog);
	bu_vls_free(&tcl_cmd);
	bu_exit(1, NULL);
    }
    bu_vls_free(&tlog);

    archer_tcl = bu_dir(NULL, 0, BU_DIR_DATA, "tclscripts", "archer", "init", "archer_launch.tcl", NULL);
    Tcl_DStringInit(&temp);
    fullname = Tcl_TranslateFileName(interp, archer_tcl, &temp);
    status = Tcl_EvalFile(interp, fullname);
    Tcl_DStringFree(&temp);

    result = Tcl_GetStringResult(interp);
    if (strlen(result) > 0 && status == TCL_ERROR) {
	bu_log("%s\n", result);
    }
    Tcl_DeleteInterp(interp);


    bu_free((void *)av, "argv cpy");
#endif /* HAVE_TK */

    return status;
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
