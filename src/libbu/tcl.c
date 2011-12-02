/*                        B U _ T C L . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"

#include "tcl.h"
#include "cmd.h"		/* this includes bu.h */
#include "bu.h"

/*XXX Temporary global interp */
Tcl_Interp *brlcad_interp = (Tcl_Interp *)0;

#define TINYBUFSIZ 32
#define SMALLBUFSIZ 256

/* defined in libbu/cmdhist_obj.c */
extern int Cho_Init(Tcl_Interp *interp);


/**
 * Convert the "form" of a bu_structparse table into a TCL result
 * string, with parameter-name data-type pairs:
 *
 * V {%f %f %f} A {%f %f %f}
 *
 * A different routine should build a more general 'form', along the
 * lines of:
 *
 * {V {%f %f %f} default {help}} {A {%f %f %f} default# {help}}
 *
 * @param interp tcl interpreter
 * @param sp structparse table
 *
 * 	@return
 * 	void
 */
HIDDEN void
tcl_bu_structparse_get_terse_form(Tcl_Interp *interp,
				  const struct bu_structparse *sp)
{
#if 1
    struct bu_vls vlog;

    bu_vls_init(&vlog);
    bu_structparse_get_terse_form(&vlog, sp);
    Tcl_AppendResult(interp, bu_vls_addr(&vlog), (char *)NULL);
    bu_vls_free(&vlog);
#else
    struct bu_vls str;
    int i;

    bu_vls_init(&str);

    while (sp->sp_name != NULL) {
	Tcl_AppendElement(interp, sp->sp_name);
	bu_vls_trunc(&str, 0);
	/* These types are specified by lengths, e.g. %80s */
	if (BU_STR_EQUAL(sp->sp_fmt, "%c") ||
	    BU_STR_EQUAL(sp->sp_fmt, "%s") ||
	    BU_STR_EQUAL(sp->sp_fmt, "%S") || /* XXX - DEPRECATED [7.14] */
	    BU_STR_EQUAL(sp->sp_fmt, "%V")) {
	    if (sp->sp_count > 1) {
		/* Make them all look like %###s */
		bu_vls_printf(&str, "%%%lds", sp->sp_count);
	    } else {
		/* Singletons are specified by their actual character */
		bu_vls_printf(&str, "%%c");
	    }
	} else {
	    /* Vectors are specified by repetition, e.g. {%f %f %f} */
	    bu_vls_printf(&str, "%s", sp->sp_fmt);
	    for (i = 1; i < sp->sp_count; i++)
		bu_vls_printf(&str, " %s", sp->sp_fmt);
	}
	Tcl_AppendElement(interp, bu_vls_addr(&str));
	++sp;
    }
    bu_vls_free(&str);
#endif
}


/**
 * A tcl wrapper for bu_mem_barriercheck.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
HIDDEN int
tcl_bu_mem_barriercheck(ClientData UNUSED(clientData),
			Tcl_Interp *interp,
			int argc,
			const char **argv)
{
    int ret;

    if (argc > 1) {
	Tcl_AppendResult(interp, "Usage: ", argv[0], "\n", (char *)NULL);
	return TCL_ERROR;
    }

    ret = bu_mem_barriercheck();
    if (UNLIKELY(ret < 0)) {
	Tcl_AppendResult(interp, "bu_mem_barriercheck() failed\n", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}


/**
 * A tcl wrapper for bu_prmem. Prints map of
 * memory currently in use, to stderr.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
HIDDEN int
tcl_bu_prmem(ClientData UNUSED(clientData),
	     Tcl_Interp *interp,
	     int argc,
	     const char **argv)
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "Usage: bu_prmem title\n");
	return TCL_ERROR;
    }

    bu_prmem(argv[1]);
    return TCL_OK;
}


/**
 * Given arguments of alternating keywords and values and a specific
 * keyword ("Iwant"), return the value associated with that keyword.
 *
 * example: bu_get_value_by_keyword Iwant az 35 elev 25 temp 9.6
 *
 * If only one argument is given after the search keyword, it is
 * interpreted as a list in the same format.
 *
 * example: bu_get_value_by_keyword Iwant {az 35 elev 25 temp 9.6}
 *
 * Search order is left-to-right, only first match is returned.
 *
 * Sample use:
 * bu_get_value_by_keyword V8 [concat type [.inmem get box.s]]
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
HIDDEN int
tcl_bu_get_value_by_keyword(ClientData UNUSED(clientData),
			    Tcl_Interp *interp,
			    int argc,
			    const char **argv)
{
    int i = 0;
    int listc = 0;
    const char *iwant = (const char *)NULL;
    const char **listv = (const char **)NULL;
    const char **tofree = (const char **)NULL;

    if (argc < 3) {
	char buf[TINYBUFSIZ];
	snprintf(buf, TINYBUFSIZ, "%d", argc);
	Tcl_AppendResult(interp,
			 "bu_get_value_by_keyword: wrong # of args (", buf, ").\n",
			 "Usage: bu_get_value_by_keyword iwant {list}\n",
			 "Usage: bu_get_value_by_keyword iwant key1 val1 key2 val2 ... keyN valN\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    iwant = argv[1];

    if (argc == 3) {
	if (Tcl_SplitList(interp, argv[2], &listc, (const char ***)&listv) != TCL_OK) {
	    Tcl_AppendResult(interp, "bu_get_value_by_keyword: iwant='", iwant, "', unable to split '", argv[2], "'\n", (char *)NULL);
	    return TCL_ERROR;
	}
	tofree = listv;
    } else {
	/* Take search list from remaining arguments */
	listc = argc - 2;
	listv = argv + 2;
    }

    if ((listc & 1) != 0) {
	char buf[TINYBUFSIZ];
	snprintf(buf, TINYBUFSIZ, "%d", listc);
	Tcl_AppendResult(interp, "bu_get_value_by_keyword: odd # of items in list (", buf, ").\n", (char *)NULL);
	if (tofree)
	    Tcl_Free((char *)tofree); /* not bu_free() */
	return TCL_ERROR;
    }

    for (i=0; i < listc; i += 2) {
	if (BU_STR_EQUAL(iwant, listv[i])) {
	    /* If value is a list, don't nest it in another list */
	    if (listv[i+1][0] == '{') {
		struct bu_vls str;
		bu_vls_init(&str);
		/* Skip leading { */
		bu_vls_strcat(&str, &listv[i+1][1]);
		/* Trim trailing } */
		bu_vls_trunc(&str, -1);
		Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
		bu_vls_free(&str);
	    } else {
		Tcl_AppendResult(interp, listv[i+1], (char *)NULL);
	    }
	    if (tofree)
		Tcl_Free((char *)tofree); /* not bu_free() */
	    return TCL_OK;
	}
    }

    /* Not found */
    Tcl_AppendResult(interp, "bu_get_value_by_keyword: keyword '", iwant, "' not found in list\n", (char *)NULL);
    if (tofree)
	Tcl_Free((char *)tofree); /* not bu_free() */
    return TCL_ERROR;
}


/**
 * A tcl wrapper for bu_rgb_to_hsv.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
HIDDEN int
tcl_bu_rgb_to_hsv(ClientData UNUSED(clientData),
		  Tcl_Interp *interp,
		  int argc,
		  const char **argv)
{
    int rgb_int[3];
    unsigned char rgb[3];
    fastf_t hsv[3];
    struct bu_vls result;

    bu_vls_init(&result);
    if (argc != 4) {
	Tcl_AppendResult(interp, "Usage: bu_rgb_to_hsv R G B\n",
			 (char *)NULL);
	return TCL_ERROR;
    }
    if ((Tcl_GetInt(interp, argv[1], &rgb_int[0]) != TCL_OK)
	|| (Tcl_GetInt(interp, argv[2], &rgb_int[1]) != TCL_OK)
	|| (Tcl_GetInt(interp, argv[3], &rgb_int[2]) != TCL_OK)
	|| (rgb_int[0] < 0) || (rgb_int[0] > 255)
	|| (rgb_int[1] < 0) || (rgb_int[1] > 255)
	|| (rgb_int[2] < 0) || (rgb_int[2] > 255)) {
	bu_vls_printf(&result, "bu_rgb_to_hsv: Bad RGB (%s, %s, %s)\n",
		      argv[1], argv[2], argv[3]);
	Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
	bu_vls_free(&result);
	return TCL_ERROR;
    }
    rgb[0] = rgb_int[0];
    rgb[1] = rgb_int[1];
    rgb[2] = rgb_int[2];

    bu_rgb_to_hsv(rgb, hsv);
    bu_vls_printf(&result, "%g %g %g", hsv[0], hsv[1], hsv[2]);
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

}


/**
 * A tcl wrapper for bu_hsv_to_rgb.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
HIDDEN int
tcl_bu_hsv_to_rgb(ClientData UNUSED(clientData),
		  Tcl_Interp *interp,
		  int argc,
		  const char **argv)
{
    fastf_t hsv[3];
    unsigned char rgb[3];
    struct bu_vls result;

    if (argc != 4) {
	Tcl_AppendResult(interp, "Usage: bu_hsv_to_rgb H S V\n",
			 (char *)NULL);
	return TCL_ERROR;
    }
    bu_vls_init(&result);
    if ((Tcl_GetDouble(interp, argv[1], &hsv[0]) != TCL_OK)
	|| (Tcl_GetDouble(interp, argv[2], &hsv[1]) != TCL_OK)
	|| (Tcl_GetDouble(interp, argv[3], &hsv[2]) != TCL_OK)
	|| (bu_hsv_to_rgb(hsv, rgb) == 0)) {
	bu_vls_printf(&result, "bu_hsv_to_rgb: Bad HSV (%s, %s, %s)\n",
		      argv[1], argv[2], argv[3]);
	Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
	bu_vls_free(&result);
	return TCL_ERROR;
    }

    bu_vls_printf(&result, "%d %d %d", rgb[0], rgb[1], rgb[2]);
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

}


/**
 * tcl_bu_brlcad_root
 *
 * A tcl wrapper for bu_brlcad_root.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
HIDDEN int
tcl_bu_brlcad_root(ClientData UNUSED(clientData),
		   Tcl_Interp *interp,
		   int argc,
		   const char **argv)
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "Usage: bu_brlcad_root subdir\n",
			 (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, bu_brlcad_root(argv[1], 1), NULL);
    return TCL_OK;
}


/**
 * tcl_bu_brlcad_data
 *
 * A tcl wrapper for bu_brlcad_data.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
HIDDEN int
tcl_bu_brlcad_data(ClientData UNUSED(clientData),
		   Tcl_Interp *interp,
		   int argc,
		   const char **argv)
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "Usage: bu_brlcad_data subdir\n",
			 (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, bu_brlcad_data(argv[1], 1), NULL);
    return TCL_OK;
}


/**
 * tcl_bu_units_conversion
 *
 * A tcl wrapper for bu_units_conversion.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
HIDDEN int
tcl_bu_units_conversion(ClientData UNUSED(clientData),
			Tcl_Interp *interp,
			int argc,
			const char **argv)
{
    double conv_factor;
    struct bu_vls result;

    if (argc != 2) {
	Tcl_AppendResult(interp, "Usage: bu_units_conversion units_string\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    conv_factor = bu_units_conversion(argv[1]);
    if (conv_factor <= 0.0) {
	Tcl_AppendResult(interp, "ERROR: bu_units_conversion: Unrecognized units string: ",
			 argv[1], "\n", (char *)NULL);
	return TCL_ERROR;
    }

    bu_vls_init(&result);
    bu_vls_printf(&result, "%.12e", conv_factor);
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;
}


int
Bu_Init(Tcl_Interp *interp)
{
    static struct bu_cmdtab cmds[] = {
	{"bu_units_conversion",		tcl_bu_units_conversion},
	{"bu_brlcad_data",		tcl_bu_brlcad_data},
	{"bu_brlcad_root",		tcl_bu_brlcad_root},
	{"bu_mem_barriercheck",		tcl_bu_mem_barriercheck},
	{"bu_prmem",			tcl_bu_prmem},
	{"bu_get_value_by_keyword",	tcl_bu_get_value_by_keyword},
	{"bu_rgb_to_hsv",		tcl_bu_rgb_to_hsv},
	{"bu_hsv_to_rgb",		tcl_bu_hsv_to_rgb},
	{(char *)NULL,			NULL }
    };

    /*XXX Use of brlcad_interp is temporary */
    brlcad_interp = interp;
    bu_register_cmds(interp, cmds);

    Tcl_SetVar(interp, "BU_DEBUG_FORMAT", BU_DEBUG_FORMAT, TCL_GLOBAL_ONLY);
    Tcl_LinkVar(interp, "bu_debug", (char *)&bu_debug, TCL_LINK_INT);

    /* initialize command history objects */
    Cho_Init(interp);

    return TCL_OK;
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
