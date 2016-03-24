/*                       B R L M A N  . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2016 United States Government as represented by
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
#include "tk.h"

#include "bu.h"
#include "tclcad.h"

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
opt_lang(struct bu_vls *msg, int argc, const char **argv, void *l)
{
    int i = 0;
    struct bu_vls *lang = (struct bu_vls *)l;
    int ret = bu_opt_vls(msg, argc, argv, (void *)l);
    if (ret == -1) return -1;
    if (bu_vls_strlen(lang) != 2) return -1;
    /* Only return valid if we've got one of the ISO639-1 lang codes */
    while (iso639_1[i]) {
	if (BU_STR_EQUAL(bu_vls_addr(lang), iso639_1[i])) return ret;
	i++;
    }
    return -1;
}

HIDDEN char *
find_man_file(const char *man_name, const char *lang, char section, int gui)
{
    char *ret = NULL;
    struct bu_vls data_dir = BU_VLS_INIT_ZERO;
    struct bu_vls file_ext = BU_VLS_INIT_ZERO;
    struct bu_vls mfile = BU_VLS_INIT_ZERO;
    if (gui) {
	bu_vls_sprintf(&file_ext, "html");
	bu_vls_sprintf(&data_dir, "%s/html", bu_brlcad_root(bu_brlcad_dir("doc", 1), 1));
    } else {
	if (section != 'n') {
	    bu_vls_sprintf(&file_ext, "%c", section);
	} else {
	    bu_vls_sprintf(&file_ext, "nged");
	}
	bu_vls_sprintf(&data_dir, "%s", bu_brlcad_root(bu_brlcad_dir("man", 1), 1));
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

HIDDEN char *
find_man_path(const char *man_name, const char *lang, char section, int gui)
{
    char *ret = NULL;
    char *man_file = find_man_file(man_name, lang, section, gui);
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    struct bu_vls mpath = BU_VLS_INIT_ZERO;

    if (!man_file) goto done;
    if (!bu_path_component(&tmp, man_file, BU_PATH_DIRNAME)) goto done;
    if (!bu_path_component(&mpath, bu_vls_addr(&tmp), BU_PATH_DIRNAME)) goto done;

    ret = bu_strdup(bu_vls_addr(&mpath));

done:
    if (man_file) bu_free(man_file, "free file path");
    bu_vls_free(&tmp);
    bu_vls_free(&mpath);
    return ret;
}

int
main(int argc, const char **argv)
{
    int i = 0;
    const char *result;
    const char *fullname;
    int status;
    const char *brlman_tcl = NULL;
    struct bu_vls tlog = BU_VLS_INIT_ZERO;
    int uac = 0;
#ifdef HAVE_WINDOWS_H
    int enable_gui = 1;
#else
    int enable_gui = 0;
#endif
    int disable_gui = 0;
    const char *man_cmd = NULL;
    const char *man_name = NULL;
    const char *man_file = NULL;
    char man_section = '\0';
    struct bu_vls lang = BU_VLS_INIT_ZERO;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[4];
    const char sections[] = {'1', '3', '5', 'n', '\0'};

    /* Need progname set for bu_brlcad_root/bu_brlcad_data to work */
    bu_setprogname(argv[0]);

    /* Handle options in C */
    BU_OPT(d[0], "g", "gui",         "",       NULL, (void *)&enable_gui,  "Enable GUI");
    BU_OPT(d[1], "",  "no-gui",      "",       NULL, (void *)&disable_gui, "Disable GUI");
    BU_OPT(d[2], "L", "language",  "lg",  &opt_lang, (void *)&lang,        "Set language");
    BU_OPT_NULL(d[3]);

    /* Skip first arg */
    argv++; argc--;
    uac = bu_opt_parse(&optparse_msg, argc, argv, d);
    if (uac == -1) {
	bu_exit(EXIT_FAILURE, bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

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
	bu_log("Warning - gui explicitly enabled *and* disabled? - enabling.\n");
	disable_gui = 0;
    }

    /* If we're not graphical and we don't have a name at this point, we're done. */
    if (!enable_gui && !man_name) {
	bu_exit(EXIT_FAILURE, "Error: Please specify man page");
    }

    /* If we're not graphical, make sure we have a man command to use */
    if (!enable_gui) {
	man_cmd = bu_which("man");
	if (!man_cmd) {
	    if (disable_gui) {
		bu_exit(EXIT_FAILURE, "Error: Graphical man display is disabled, but no man command is available.");
	    } else {
		/* Don't have any specific instructions - fall back on gui */
		enable_gui = 1;
	    }
	}
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
	    if (man_file) break;
	    i++;
	}
    }


    if (!enable_gui) {
	/* Note - for reasons that are unclear, passing in man_cmd as the first
	 * argument to execlp will *not) work... */
	(void)execlp("man", man_cmd, man_file, (char *)NULL);
    } else {
	Tcl_DString temp;
	Tcl_Interp *interp = Tcl_CreateInterp();
	struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;

	status = tclcad_init(interp, enable_gui, &tlog);

	if (status == TCL_ERROR) {
	    bu_log("brlman tclcad init failure:\n%s\n", bu_vls_addr(&tlog));
	    bu_vls_free(&tlog);
	    bu_exit(1, "tcl init error");
	}
	bu_vls_free(&tlog);

	/* Have gui but no man page - graphical failure notice */
	if (enable_gui) {
	    bu_vls_sprintf(&tcl_cmd, "tk_messageBox -message \"Error: Please specify man page\" -type ok");
	    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	    bu_exit(EXIT_FAILURE, "no man page");
	} else {
	    bu_exit(EXIT_FAILURE, "Error: Please specify man page");
	}

	brlman_tcl = bu_brlcad_data("tclscripts/brlman/brlman.tcl", 1);
	Tcl_DStringInit(&temp);
	fullname = Tcl_TranslateFileName(interp, brlman_tcl, &temp);
	status = Tcl_EvalFile(interp, fullname);
	Tcl_DStringFree(&temp);

	result = Tcl_GetStringResult(interp);
	if (strlen(result) > 0) {
	    bu_log("%s\n", result);
	}

    }
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
