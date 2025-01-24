/*                         T E D I T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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
/** @file mged/tedit.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bresource.h"

#include "bu/app.h"
#include "bu/env.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"
#include "./primitives/mged_functab.h"


#define V3BASE2LOCAL(_pt)	(_pt)[X]*s->dbip->dbi_base2local, (_pt)[Y]*s->dbip->dbi_base2local, (_pt)[Z]*s->dbip->dbi_base2local

/* editors to test, in order of discovery preference (EDITOR overrides) */
#define WIN_EDITOR "\"c:/Program Files/Windows NT/Accessories/wordpad\""
#define MAC_EDITOR "/Applications/TextEdit.app/Contents/MacOS/TextEdit"
#define EMACS_EDITOR "emacs"
#define VIM_EDITOR "vim"
#define VI_EDITOR "vi"
#define NANO_EDITOR "nano"
#define ED_EDITOR "ed"

/* used to invoke the above editor if X11 is in use */
#define XTERM_COMMAND "xterm"

/* Can the mac terminal be used to launch applications?  Doesn't seem like it
 * in initial trials, but maybe there's some trick? */
#define MAC_BINARY "/Applications/Utilities/Terminal.app/Contents/MacOS/Terminal"

static char tmpfil[MAXPATHLEN] = {0};

int writesolid(struct mged_state *), readsolid(struct mged_state *);

/*
 *
 * No-frills edit - opens an editor on the supplied
 * file name.
 *
 */
int
editit(struct mged_state *s, const char *tempfile) {
    int argc = 5;
    const char *av[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
    struct bu_vls editstring = BU_VLS_INIT_ZERO;

    CHECK_DBI_NULL;

    if (!get_editor_string(s, &editstring))
	return TCL_ERROR;

    av[0] = "editit";
    av[1] = "-e";
    av[2] = bu_vls_addr(&editstring);
    av[3] = "-f";
    av[4] = tempfile;
    av[5] = NULL;

    ged_exec(s->gedp, argc, (const char **)av);

    bu_vls_free(&editstring);
    return TCL_OK;
}

int
f_tedit(ClientData clientData, Tcl_Interp *interp, int argc, const char **UNUSED(argv))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    FILE *fp;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 1 || 1 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help ted");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Only do this if in solid edit state */
    if (not_state(s, ST_S_EDIT, "Primitive Text Edit"))
	return TCL_ERROR;

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (fp == NULL)
	return TCL_ERROR;

    if (writesolid(s)) {
	bu_file_delete(tmpfil);
	fclose(fp);
	return TCL_ERROR;
    }

    (void)fclose(fp);

    if (editit(s, tmpfil) == TCL_OK) {
	if (readsolid(s)) {
	    bu_file_delete(tmpfil);
	    return TCL_ERROR;
	}

	/* Update the display */
	replot_editing_solid(s);
	view_state->vs_flag = 1;
	Tcl_AppendResult(interp, "done\n", (char *)NULL);
    }

    bu_file_delete(tmpfil);

    return TCL_OK;
}


/* Write numerical parameters of a solid into a file */
int
writesolid(struct mged_state *s)
{
    FILE *fp;

    CHECK_DBI_NULL;

    struct rt_db_internal *ip = &s->s_edit->es_int;
    if (!MGED_OBJ[ip->idb_type].ft_write_params) {
	Tcl_AppendResult(s->interp, "Cannot text edit this solid type\n", (char *)NULL);
	return 1;
    }

    struct bu_vls params = BU_VLS_INIT_ZERO;
    (*MGED_OBJ[ip->idb_type].ft_write_params)(&params, ip, &s->tol.tol, s->dbip->dbi_base2local);
    fp = fopen(tmpfil, "w");
    fprintf(fp, "%s", bu_vls_cstr(&params));
    (void)fclose(fp);
    return 0;
}

/* Read numerical parameters of solid from file */
int
readsolid(struct mged_state *s)
{
    CHECK_DBI_NULL;

    struct rt_db_internal *ip = &s->s_edit->es_int;

    if (!MGED_OBJ[ip->idb_type].ft_read_params) {
	Tcl_AppendResult(s->interp, "Cannot text edit this solid type\n", (char *)NULL);
	return 1;
    }

    struct bu_vls solid_in = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *mf = bu_open_mapped_file(tmpfil, (char *)NULL);
    if (!mf) {
	bu_log("cannot read temporary file \"%s\"\n", tmpfil);
	return 1;	/* FAIL */
    }
    bu_vls_strncpy(&solid_in, (char *)mf->buf, mf->buflen);
    bu_close_mapped_file(mf);

    if ((*MGED_OBJ[ip->idb_type].ft_read_params)(ip, bu_vls_cstr(&solid_in), &s->tol.tol, s->dbip->dbi_local2base) == BRLCAD_ERROR) {
	bu_vls_free(&solid_in);
	return 1;   /* FAIL */
    }

    bu_vls_free(&solid_in);
    return 0;
}


int
get_editor_string(struct mged_state *s, struct bu_vls *editstring)
{
    /* There are two possible situations for MGED - in classic mode
     * the assumption is made that the command window is a controlling
     * terminal, and an editor should be launched that will utilize
     * that controlling window.  In GUI mode, the editor will be launched
     * either as a separate GUI application or in a separate terminal. */
    int need_terminal = 0;
    const char *editor = (char *)NULL;
    const char *editor_opt = (char *)NULL;

    if (s->classic_mged) {
	// Console editors only
	editor = bu_editor(&editor_opt, 1, 0, NULL);
    } else {
	// First try GUI editors only
	editor = bu_editor(&editor_opt, 2, 0, NULL);
	if (!editor) {
	    // Falling back to console, will need terminal
	    need_terminal = 1;
	    editor = bu_editor(&editor_opt, 1, 0, NULL);
	}
    }

    if (!editor) {
	// No suitable editor found
	return 0;
    }

    if (!need_terminal) {
	bu_vls_sprintf(editstring, "(null) (null) %s %s", editor, editor_opt?editor_opt:"(null)");
	return 1;
    }

    // If we do need a terminal, try to find one
    const char *terminal = bu_which(XTERM_COMMAND);
    /* look a little harder if we found nothing */
    if (!terminal) {
	terminal = bu_which("/usr/X11R6/bin/" XTERM_COMMAND);
    }
    if (!terminal) {
	terminal = bu_which("/usr/X11/bin/" XTERM_COMMAND);
    }

    const char *terminal_opt = (char *)NULL;
    if (terminal)
	terminal_opt = "-e";

    bu_vls_sprintf(editstring, "%s %s %s %s", terminal?terminal:"(null)", terminal_opt?terminal_opt:"(null)", editor, editor_opt?editor_opt:"(null)");

    return 1;
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
