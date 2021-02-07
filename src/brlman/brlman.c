/*                       B R L M A N  . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2021 United States Government as represented by
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
/** @file brlman.c
 *
 *  Man page viewer for BRL-CAD man pages.
 *
 */

#include "common.h"

#include <string.h>
#include <ctype.h> /* for isdigit */
#include "bresource.h"

#include "bnetwork.h"
#include "bio.h"

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bu.h"
#include "tclcad.h"

/* Confine the constraining build conditions here - ultimately, we care
 * about graphical and non-graphical, whatever the reasons. */
#ifdef HAVE_TK
#  define MAN_GUI 1
#endif
#ifndef HAVE_WINDOWS_H
#  define MAN_CMDLINE 1
#endif

/* Supported man sections */
const char sections[] = {'1', '3', '5', 'n', '\0'};

/*
 * Checks that a string matches the two lower case letter form of ISO 639-1
 * language codes.  List pulled from:
 *
 * http://www.loc.gov/standards/iso639-2/php/English_list.php
 */
const char *iso639_1[] = {"ab", "aa", "af", "ak", "sq", "am", "ar", "an",
			  "hy", "as", "av", "ae", "ay", "az", "bm", "ba", "eu", "be", "bn", "bh",
			  "bi", "nb", "bs", "br", "bg", "my", "es", "ca", "km", "ch", "ce", "ny",
			  "ny", "zh", "za", "cu", "cu", "cv", "kw", "co", "cr", "hr", "cs", "da",
			  "dv", "dv", "nl", "dz", "en", "eo", "et", "ee", "fo", "fj", "fi", "nl",
			  "fr", "ff", "gd", "gl", "lg", "ka", "de", "ki", "el", "kl", "gn", "gu",
			  "ht", "ht", "ha", "he", "hz", "hi", "ho", "hu", "is", "io", "ig", "id",
			  "ia", "ie", "iu", "ik", "ga", "it", "ja", "jv", "kl", "kn", "kr", "ks",
			  "kk", "ki", "rw", "ky", "kv", "kg", "ko", "kj", "ku", "kj", "ky", "lo",
			  "la", "lv", "lb", "li", "li", "li", "ln", "lt", "lu", "lb", "mk", "mg",
			  "ms", "ml", "dv", "mt", "gv", "mi", "mr", "mh", "ro", "ro", "mn", "na",
			  "nv", "nv", "nd", "nr", "ng", "ne", "nd", "se", "no", "nb", "nn", "ii",
			  "ny", "nn", "ie", "oc", "oj", "cu", "cu", "cu", "or", "om", "os", "os",
			  "pi", "pa", "ps", "fa", "pl", "pt", "pa", "ps", "qu", "ro", "rm", "rn",
			  "ru", "sm", "sg", "sa", "sc", "gd", "sr", "sn", "ii", "sd", "si", "si",
			  "sk", "sl", "so", "st", "nr", "es", "su", "sw", "ss", "sv", "tl", "ty",
			  "tg", "ta", "tt", "te", "th", "bo", "ti", "to", "ts", "tn", "tr", "tk",
			  "tw", "ug", "uk", "ur", "ug", "uz", "ca", "ve", "vi", "vo", "wa", "cy",
			  "fy", "wo", "xh", "yi", "yo", "za", "zu", NULL};

HIDDEN int
opt_lang(struct bu_vls *msg, size_t argc, const char **argv, void *l)
{
    size_t i = 0;
    struct bu_vls *lang = (struct bu_vls *)l;
    if (lang) {
	int ret = bu_opt_vls(msg, argc, argv, (void *)l);
	if (ret == -1)
	    return -1;
	if (bu_vls_strlen(lang) != 2)
	    return -1;
	/* Only return valid if we've got one of the ISO639-1 lang codes */
	while (iso639_1[i]) {
	    if (BU_STR_EQUAL(bu_vls_addr(lang), iso639_1[i]))
		return ret;
	    i++;
	}
	return -1;
    } else {
	return -1;
    }
}


HIDDEN int
opt_section(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    size_t i = 0;
    char *s_set = (char *)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_str");

    /* One char only */
    if (strlen(argv[0]) != 1)
	return -1;

    while(sections[i]) {
	if (sections[i] == argv[0][0]) {
	    if (s_set)
		(*s_set) = argv[0][0];
	    return 1;
	}
	i++;
    }

    return -1;
}


HIDDEN char *
find_man_file(const char *man_name, const char *lang, char section, int gui)
{
    const char *ddir;
    char *ret = NULL;
    struct bu_vls data_dir = BU_VLS_INIT_ZERO;
    struct bu_vls file_ext = BU_VLS_INIT_ZERO;
    struct bu_vls mfile = BU_VLS_INIT_ZERO;
    if (gui) {
	bu_vls_sprintf(&file_ext, "html");
	ddir = bu_dir(NULL, 0, BU_DIR_DOC, "html", NULL);
	bu_vls_sprintf(&data_dir, "%s", ddir);
    } else {
	if (section != 'n') {
	    bu_vls_sprintf(&file_ext, "%c", section);
	} else {
	    bu_vls_sprintf(&file_ext, "nged");
	}
	ddir = bu_dir(NULL, 0, BU_DIR_MAN, NULL);
	bu_vls_sprintf(&data_dir, "%s", ddir);
    }
    bu_vls_sprintf(&mfile, "%s/%s/man%c/%s.%s", bu_vls_addr(&data_dir), lang, section, man_name, bu_vls_addr(&file_ext));
    if (bu_file_exists(bu_vls_addr(&mfile), NULL)) {
	ret = bu_strdup(bu_vls_addr(&mfile));
    } else {
	bu_vls_sprintf(&mfile, "%s/man%c/%s.%s", bu_vls_addr(&data_dir), section, man_name, bu_vls_addr(&file_ext));
	if (bu_file_exists(bu_vls_addr(&mfile), NULL)) {
	    ret = bu_strdup(bu_vls_addr(&mfile));
	}
    }
    bu_vls_free(&data_dir);
    bu_vls_free(&file_ext);
    bu_vls_free(&mfile);
    return ret;
}


#ifndef HAVE_WINDOWS_H
#  define APIENTRY
#  define BRLMAN_MAIN main
#else
#  define BRLMAN_MAIN WinMain
#endif

/* main() */
int APIENTRY
BRLMAN_MAIN(
#ifdef HAVE_WINDOWS_H
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine,
    int nCmdShow
#else
    int argc,
    const char **argv
#endif
    )
{

#if !defined(MAN_CMDLINE) && !defined(MAN_GUI)
    bu_exit(EXIT_FAILURE, "Error: man page display is not supported.");
#else
    int i = 0;
    int status = BRLCAD_ERROR;
    int uac = 0;
#ifndef MAN_CMDLINE
    int enable_gui = 1;
#else
    int enable_gui = 0;
#endif
    int disable_gui = 0;
    int print_help = 0;
    const char *man_cmd = NULL;
    const char *man_name = NULL;
    const char *man_file = NULL;
    char man_section = '\0';
    struct bu_vls lang = BU_VLS_INIT_ZERO;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[6];

#ifdef HAVE_WINDOWS_H
    const char **argv;
    int argc;

    /* Get our args from the c-runtime. Ignore lpszCmdLine. */
    argc = __argc;
    argv = __argv;
#endif

    /* initialize progname for run-time resource finding */
    bu_setprogname(argv[0]);

    /* Handle options in C */
    BU_OPT(d[0], "h", "help",        "",         NULL, &print_help,  "Print help and exit");
    BU_OPT(d[1], "g", "gui",         "",         NULL, &enable_gui,  "Enable GUI");
    BU_OPT(d[2], "",  "no-gui",      "",         NULL, &disable_gui, "Disable GUI");
    BU_OPT(d[3], "L", "language",  "lg",    &opt_lang, &lang,        "Set language");
    BU_OPT(d[4], "S", "section",    "#", &opt_section, &man_section, "Set section");
    BU_OPT_NULL(d[5]);

    /* Skip first arg */
    argv++; argc--;
    uac = bu_opt_parse(&optparse_msg, argc, argv, d);
    if (uac == -1) {
	bu_exit(EXIT_FAILURE, "%s", bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    /* If we want help, print help */
    if (print_help) {
	char *option_help = bu_opt_describe(d, NULL);
	bu_log("Usage: brlman [options] [man_page]\n");
	if (option_help) {
	    bu_log("Options:\n%s\n", option_help);
	    bu_free(option_help, "help str");
	}
	bu_exit(EXIT_SUCCESS, NULL);
    }

    /* If we only have one non-option arg, assume it's the man name */
    if (uac == 1) {
	man_name = argv[0];
    } else {
	/* If we've got more, check for a section number. */
	for (i = 0; i < uac; i++) {
	    if (strlen(argv[i]) == 1 && (isdigit(argv[i][0]) || argv[i][0] == 'n')) {

		/* Record the section */
		man_section = argv[i][0];

		/* If we have a section identifier and it's not already the last
		 * element in argv, adjust the array */
		if (i < uac - 1) {
		    const char *tmp = argv[uac-1];
		    argv[uac-1] = argv[i];
		    argv[i] = tmp;
		}

		/* Found a number and processed - one less argv entry. */
		uac--;

		/* First number wins. */
		break;
	    }
	}

	/* For now, only support specifying one man page at a time */
	if (uac > 1) {
	    bu_exit(EXIT_FAILURE, "Error - need a single man page name");
	}

	/* If we have one, use it.  Zero is allowed if we're in graphical
	 * mode, so uac == at this point isn't necessarily a failure */
	if (uac == 1) {
	    man_name = argv[0];
	}
    }

    if (enable_gui && disable_gui) {
#if defined(MAN_GUI) && defined(MAN_CMDLINE)
	bu_log("Warning - gui explicitly enabled *and* disabled? - platform supports both, enabling GUI.\n");
	disable_gui = 0;
#endif
#if !defined(MAN_GUI) && defined(MAN_CMDLINE)
	bu_log("Warning - gui explicitly enabled *and* disabled? - platform only supports command line, disabling GUI.\n");
#endif
#if defined(MAN_GUI) && !defined(MAN_CMDLINE)
	bu_log("Warning - gui explicitly enabled *and* disabled? - platform only supports GUI viewer, enabling GUI.\n");
#endif
    }

    /* If we're not graphical and we don't have a name at this point, we're done. */
    if (!enable_gui && !man_name) {
	bu_exit(EXIT_FAILURE, "Error: Please specify man page");
    }

    /* If we're not graphical, make sure we have a man command to use */
    if (!enable_gui) {
#ifndef MAN_CMDLINE
	bu_exit(EXIT_FAILURE, "Error: Non-graphical man display is not supported.");
#else
	man_cmd = bu_which("man");
	if (!man_cmd) {
	    if (disable_gui) {
		bu_exit(EXIT_FAILURE, "Error: Graphical man display is disabled, but no man command is available.");
	    } else {
		/* Don't have any specific instructions - fall back on gui */
		bu_log("Warning: Graphical mode was not specified, but no man command is available - enabling GUI.");
		enable_gui = 1;
	    }
	}
#endif
    }

    /* If we don't have a language, check environment */
    if (!bu_vls_strlen(&lang)) {
	char *env_lang = getenv("LANG");
	if (env_lang && strlen(env_lang) >= 2) {
	    /* Grab the first two characters */
	    bu_vls_strncat(&lang, env_lang, 2);
	}
    }

    /* If we *still* don't have a language, use en */
    if (!bu_vls_strlen(&lang)) {
	bu_vls_sprintf(&lang, "en");
    }

    /* Now we have a lang and name - find the file */

    if (man_section != '\0') {
	man_file = find_man_file(man_name, bu_vls_addr(&lang), man_section, enable_gui);
    } else {
	i = 0;
	while(sections[i] != '\0') {
	    man_file = find_man_file(man_name, bu_vls_addr(&lang), sections[i], enable_gui);
	    if (man_file) {
		man_section = sections[i];
		break;
	    }
	    i++;
	}
    }

    if (!enable_gui) {
#ifdef MAN_CMDLINE
	if (!man_file) {
	    if (man_section != '\0') {
		bu_exit(EXIT_FAILURE, "No man page found for %s in section %c\n", man_name, man_section);
	    } else {
		bu_exit(EXIT_FAILURE, "No man page found for %s\n", man_name);
	    }
	}
	/* Note - for reasons that are unclear, passing in man_cmd as the first
	 * argument to execlp will *not* work... */
	status = execlp("man", man_cmd, man_file, (char *)NULL);
#else
	/* Shouldn't get here - should be caught by above tests */
	bu_exit(EXIT_FAILURE, "Error: Non-graphical man display is not supported.");
#endif
    } else {
#ifdef MAN_GUI
	const char *result;
	const char *fullname;
	const char *tcl_man_file;
	const char *brlman_tcl = NULL;
	struct bu_vls tlog = BU_VLS_INIT_ZERO;
	Tcl_DString temp;
	Tcl_Interp *interp = Tcl_CreateInterp();
	struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;

#ifdef HAVE_WINDOWS_H
	Tk_InitConsoleChannels(interp);
#endif

	status = tclcad_init(interp, enable_gui, &tlog);

	if (status == TCL_ERROR) {
	    bu_log("brlman tclcad init failure:\n%s\n", bu_vls_addr(&tlog));
	    bu_vls_free(&tlog);
	    bu_exit(1, "tcl init error");
	}
	bu_vls_free(&tlog);

	/* Have gui flag and specified man page but no file found - graphical failure notice */
	if (enable_gui && man_name && !man_file) {
	    if (man_section != '\0') {
		bu_vls_sprintf(&tcl_cmd, "tk_messageBox -message \"Error: man page not found for %s in section %c\" -type ok", man_name, man_section);
	    } else {
		bu_vls_sprintf(&tcl_cmd, "tk_messageBox -message \"Error: man page not found for %s\" -type ok", man_name);
	    }
	    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	    bu_exit(EXIT_FAILURE, NULL);
	}

	/* Pass the key variables into the interp */
	if (man_section != '\0') {
	    bu_vls_sprintf(&tcl_cmd, "set ::section_number %c", man_section);
	    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	}

	if (man_file) {
	    Tcl_Obj *pathP, *norm;

	    bu_vls_sprintf(&tcl_cmd, "set ::man_name %s", man_name);
	    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

	    pathP = Tcl_NewStringObj(man_file, -1);
	    norm = Tcl_FSGetTranslatedPath(interp, pathP);
	    tcl_man_file = Tcl_GetString(norm);
	    bu_vls_sprintf(&tcl_cmd, "set ::man_file %s", tcl_man_file);
	    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

	} else {
	    bu_vls_sprintf(&tcl_cmd, "set ::data_dir %s/html", bu_dir(NULL, 0, BU_DIR_DOC, NULL));
	    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	}

	brlman_tcl = bu_dir(NULL, 0, BU_DIR_DATA, "tclscripts", "brlman", "brlman.tcl", NULL);
	Tcl_DStringInit(&temp);
	fullname = Tcl_TranslateFileName(interp, brlman_tcl, &temp);
	status = Tcl_EvalFile(interp, fullname);
	Tcl_DStringFree(&temp);

	result = Tcl_GetStringResult(interp);
	if (strlen(result) > 0 && status == TCL_ERROR) {
	    bu_log("%s\n", result);
	}
	Tcl_DeleteInterp(interp);
#else
	/* Shouldn't get here - should be caught by above tests */
	bu_exit(EXIT_FAILURE, "Error: graphical man display is not supported.");
#endif
    }
    return status;
#endif /* !defined(MAN_CMDLINE) && !defined(MAN_GUI) */
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
