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


void
bu_badmagic_tcl(Tcl_Interp *interp,
		const uint32_t *ptr,
		uint32_t magic,
		const char *str,
		const char *file,
		int line)
{
    char buf[SMALLBUFSIZ];

    if (UNLIKELY(!(ptr))) {
	snprintf(buf, SMALLBUFSIZ, "ERROR: NULL %s pointer in TCL interface, file %s, line %d\n",
		 str, file, line);
	Tcl_AppendResult(interp, buf, NULL);
	return;
    }
    if (UNLIKELY(*((uint32_t *)(ptr)) != (magic))) {
	snprintf(buf, SMALLBUFSIZ, "ERROR: bad pointer in TCL interface %p: s/b %s(x%lx), was %s(x%lx), file %s, line %d\n",
		 (void *)ptr,
		 str, (unsigned long)magic,
		 bu_identify_magic(*(ptr)), (unsigned long)*(ptr),
		 file, line);
	Tcl_AppendResult(interp, buf, NULL);
	return;
    }
    Tcl_AppendResult(interp, "bu_badmagic_tcl() mysterious error condition, ", str, " pointer, ", file, "\n", NULL);
}


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
void
bu_tcl_structparse_get_terse_form(Tcl_Interp *interp,
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


int
bu_tcl_structparse_argv(Tcl_Interp *interp,
			int argc,
			const char **argv,
			const struct bu_structparse *desc,
			char *base)
{
    struct bu_vls vlog;
    int ret;

    bu_vls_init(&vlog);
    ret = bu_structparse_argv(&vlog, argc, argv, desc, base);
    Tcl_AppendResult(interp, bu_vls_addr(&vlog), (char *)NULL);
    bu_vls_free(&vlog);

    /* Convert to a Tcl return code */
    if (ret != BRLCAD_OK)
	return TCL_ERROR;

    return TCL_OK;
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
int
bu_tcl_mem_barriercheck(ClientData UNUSED(clientData),
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
 * A tcl wrapper for bu_ck_malloc_ptr.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_ck_malloc_ptr(ClientData UNUSED(clientData),
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv)
{
    void *voidp;

    if (argc != 3) {
	Tcl_AppendResult(interp, "Usage: bu_ck_malloc_ptr ascii-ptr description\n");
	return TCL_ERROR;
    }

    if (sscanf(argv[1], "%p", &voidp) != 1) {
	Tcl_AppendResult(interp, "bu_ck_malloc_ptr: failed to convert %s to a pointer\n", argv[1]);
	return TCL_ERROR;
    }

    bu_ck_malloc_ptr(voidp, argv[2]);
    return TCL_OK;
}


/**
 * A tcl wrapper for bu_malloc_len_roundup.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_malloc_len_roundup(ClientData UNUSED(clientData),
			  Tcl_Interp *interp,
			  int argc,
			  const char **argv)
{
    int val;

    if (argc != 2) {
	Tcl_AppendResult(interp, "Usage: bu_malloc_len_roundup nbytes\n", NULL);
	return TCL_ERROR;
    }
    val = bu_malloc_len_roundup(atoi(argv[1]));
    Tcl_SetObjResult(interp, Tcl_NewIntObj(val));
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
int
bu_tcl_prmem(ClientData UNUSED(clientData),
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
 * A tcl wrapper for bu_vls_printb.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_printb(ClientData UNUSED(clientData),
	      Tcl_Interp *interp,
	      int argc,
	      const char **argv)
{
    struct bu_vls str;

    if (argc != 4) {
	Tcl_AppendResult(interp, "Usage: bu_printb title integer-to-format bit-format-string\n", NULL);
	return TCL_ERROR;
    }
    bu_vls_init(&str);
    bu_vls_printb(&str, argv[1], (unsigned)atoi(argv[2]), argv[3]);
    Tcl_SetResult(interp, bu_vls_addr(&str), TCL_VOLATILE);
    bu_vls_free(&str);
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
int
bu_tcl_get_value_by_keyword(ClientData UNUSED(clientData),
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
 * Given arguments of alternating keywords and values, establish local
 * variables named after the keywords, with the indicated
 * values. Returns in interp a list of the variable names that were
 * assigned to. This lets you detect at runtime what assignments were
 * actually performed.
 *
 * example: bu_get_all_keyword_values az 35 elev 25 temp 9.6
 *
 * This is much faster than writing this in raw Tcl 8 as:
 *
 * foreach {keyword value} $list {
 * 	set $keyword $value
 * 	lappend retval $keyword
 * }
 *
 * If only one argument is given it is interpreted as a list in the
 * same format.
 *
 * example:  bu_get_all_keyword_values {az 35 elev 25 temp 9.6}
 *
 * For security reasons, the name of the local variable assigned to is
 * that of the input keyword with "key_" prepended.  This prevents a
 * playful user from overriding variables inside the function,
 * e.g. loop iterator "i", etc.  This could be even worse when called
 * in global context.
 *
 * Processing order is left-to-right, rightmost value for a repeated
 * keyword will be the one used.
 *
 * Sample use:
 * bu_get_all_keyword_values [concat type [.inmem get box.s]]
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_get_all_keyword_values(ClientData UNUSED(clientData),
			      Tcl_Interp *interp,
			      int argc,
			      const char **argv)
{
    struct bu_vls variable;
    int listc;
    const char **listv;
    const char **tofree = (const char **)NULL;
    int i;

    if (argc < 2) {
	char buf[TINYBUFSIZ];
	snprintf(buf, TINYBUFSIZ, "%d", argc);
	Tcl_AppendResult(interp,
			 "bu_get_all_keyword_values: wrong # of args (", buf, ").\n",
			 "Usage: bu_get_all_keyword_values {list}\n",
			 "Usage: bu_get_all_keyword_values key1 val1 key2 val2 ... keyN valN\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (argc == 2) {
	if (Tcl_SplitList(interp, argv[1], &listc, (const char ***)&listv) != TCL_OK) {
	    Tcl_AppendResult(interp,
			     "bu_get_all_keyword_values: unable to split '",
			     argv[1], "'\n", (char *)NULL);
	    return TCL_ERROR;
	}
	tofree = listv;
    } else {
	/* Take search list from remaining arguments */
	listc = argc - 1;
	listv = argv + 1;
    }

    if ((listc & 1) != 0) {
	char buf[TINYBUFSIZ];
	snprintf(buf, TINYBUFSIZ, "%d", listc);
	Tcl_AppendResult(interp,
			 "bu_get_all_keyword_values: odd # of items in list (",
			 buf, "), aborting.\n",
			 (char *)NULL);
	if (tofree) free((char *)tofree);	/* not bu_free() */
	return TCL_ERROR;
    }


    /* Process all the pairs */
    bu_vls_init(&variable);
    for (i=0; i < listc; i += 2) {
	bu_vls_strcpy(&variable, "key_");
	bu_vls_strcat(&variable, listv[i]);
	/* If value is a list, don't nest it in another list */
	if (listv[i+1][0] == '{') {
	    struct bu_vls str;
	    bu_vls_init(&str);
	    /* Skip leading { */
	    bu_vls_strcat(&str, &listv[i+1][1]);
	    /* Trim trailing } */
	    bu_vls_trunc(&str, -1);
	    Tcl_SetVar(interp, bu_vls_addr(&variable),
		       bu_vls_addr(&str), 0);
	    bu_vls_free(&str);
	} else {
	    Tcl_SetVar(interp, bu_vls_addr(&variable),
		       listv[i+1], 0);
	}
	Tcl_AppendResult(interp, bu_vls_addr(&variable),
			 " ", (char *)NULL);
	bu_vls_trunc(&variable, 0);
    }

    /* All done */
    bu_vls_free(&variable);
    if (tofree) free((char *)tofree);	/* not bu_free() */
    return TCL_OK;
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
int
bu_tcl_rgb_to_hsv(ClientData UNUSED(clientData),
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
int
bu_tcl_hsv_to_rgb(ClientData UNUSED(clientData),
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
 * Converts key=val to "key val" pairs.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_key_eq_to_key_val(ClientData UNUSED(clientData),
			 Tcl_Interp *interp,
			 int argc,
			 const char **argv)
{
    struct bu_vls vls;
    char *next;
    int i=0;

    bu_vls_init(&vls);

    while (++i < argc) {
	if (bu_key_eq_to_key_val(argv[i], (const char **)&next, &vls)) {
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	if (i < argc - 1)
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), " ", NULL);
	else
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), NULL);

	bu_vls_trunc(&vls, 0);
    }

    bu_vls_free(&vls);
    return TCL_OK;

}


/**
 * Converts a shader string to a tcl list.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_shader_to_key_val(ClientData UNUSED(clientData),
			 Tcl_Interp *interp,
			 int argc,
			 const char **argv)
{
    struct bu_vls vls;

    if (argc < 2) {
	Tcl_AppendResult(interp, "Usage: ", argv[0], " shader [key1=value ...]\n", (char *)NULL);
	return TCL_ERROR;
    }

    bu_vls_init(&vls);

    if (bu_shader_to_tcl_list(argv[1], &vls)) {
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    Tcl_AppendResult(interp, bu_vls_addr(&vls), NULL);

    bu_vls_free(&vls);

    return TCL_OK;

}


/**
 * Converts "key value" pairs to key=value.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_key_val_to_key_eq(ClientData UNUSED(clientData),
			 Tcl_Interp *interp,
			 int argc,
			 const char **argv)
{
    int i=0;

    for (i=1; i<argc; i += 2) {
	if (i+1 < argc-1)
	    Tcl_AppendResult(interp, argv[i], "=", argv[i+1], " ", NULL);
	else
	    Tcl_AppendResult(interp, argv[i], "=", argv[i+1], NULL);

    }
    return TCL_OK;

}


/**
 * Converts a shader tcl list into a shader string.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_shader_to_key_eq(ClientData UNUSED(clientData),
			Tcl_Interp *interp,
			int argc,
			const char **argv)
{
    struct bu_vls vls;

    if (argc < 2) {
	Tcl_AppendResult(interp, "Usage: ", argv[0], " shader { tcl list }\n", (char *)NULL);
	return TCL_ERROR;
    }

    bu_vls_init(&vls);

    if (bu_shader_to_key_eq(argv[1], &vls)) {
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    Tcl_AppendResult(interp, bu_vls_addr(&vls), NULL);

    bu_vls_free(&vls);

    return TCL_OK;
}


/**
 * bu_tcl_brlcad_root
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
int
bu_tcl_brlcad_root(ClientData UNUSED(clientData),
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
 * bu_tcl_brlcad_data
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
int
bu_tcl_brlcad_data(ClientData UNUSED(clientData),
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
 * bu_tcl_units_conversion
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
int
bu_tcl_units_conversion(ClientData UNUSED(clientData),
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


void
bu_tcl_setup(Tcl_Interp *interp)
{
    static struct bu_cmdtab cmds[] = {
	{"bu_units_conversion",		bu_tcl_units_conversion},
	{"bu_brlcad_data",		bu_tcl_brlcad_data},
	{"bu_brlcad_root",		bu_tcl_brlcad_root},
	{"bu_mem_barriercheck",		bu_tcl_mem_barriercheck},
	{"bu_ck_malloc_ptr",		bu_tcl_ck_malloc_ptr},
	{"bu_malloc_len_roundup",	bu_tcl_malloc_len_roundup},
	{"bu_prmem",			bu_tcl_prmem},
	{"bu_printb",			bu_tcl_printb},
	{"bu_get_all_keyword_values",	bu_tcl_get_all_keyword_values},
	{"bu_get_value_by_keyword",	bu_tcl_get_value_by_keyword},
	{"bu_rgb_to_hsv",		bu_tcl_rgb_to_hsv},
	{"bu_hsv_to_rgb",		bu_tcl_hsv_to_rgb},
	{"bu_key_eq_to_key_val",	bu_tcl_key_eq_to_key_val},
	{"bu_shader_to_tcl_list",	bu_tcl_shader_to_key_val},
	{"bu_key_val_to_key_eq",	bu_tcl_key_val_to_key_eq},
	{"bu_shader_to_key_eq",		bu_tcl_shader_to_key_eq},
	{(char *)NULL,			NULL }
    };

    /*XXX Use of brlcad_interp is temporary */
    brlcad_interp = interp;
    bu_register_cmds(interp, cmds);

    Tcl_SetVar(interp, "BU_DEBUG_FORMAT", BU_DEBUG_FORMAT, TCL_GLOBAL_ONLY);
    Tcl_LinkVar(interp, "bu_debug", (char *)&bu_debug, TCL_LINK_INT);

    /* initialize command history objects */
    Cho_Init(interp);
}


int
Bu_Init(Tcl_Interp *interp)
{
    bu_tcl_setup(interp);
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
