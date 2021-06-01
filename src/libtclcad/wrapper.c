/*                        W R A P P E R . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2021 United States Government as represented by
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

#include "ged.h"
#include "tclcad.h"

/* Private headers */
#include "./tclcad_private.h"
#include "./view/view.h"

/* Wraps calls to commands like "draw" that need to reset the view */
int
to_autoview_func(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr func,
		 const char *UNUSED(usage),
		 int UNUSED(maxargs))
{
    size_t i;
    int ret;
    char *av[2];
    int aflag = 0;
    int rflag = 0;
    struct bview *gdvp;

    av[0] = "who";
    av[1] = (char *)0;
    ret = ged_who(gedp, 1, (const char **)av);

    for (i = 1; i < (size_t)argc; ++i) {
	if (argv[i][0] != '-') {
	    break;
	}

	if (argv[i][1] == 'R' && argv[i][2] == '\0') {
	    rflag = 1;
	    break;
	}
    }

    if (!rflag && ret == GED_OK && strlen(bu_vls_addr(gedp->ged_result_str)) == 0)
	aflag = 1;

    for (i = 0; i < BU_PTBL_LEN(&current_top->to_gedp->ged_views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(&current_top->to_gedp->ged_views, i);
	if (to_is_viewable(gdvp)) {
	    gedp->ged_gvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
	    gedp->ged_gvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
	}
    }

    ret = (*func)(gedp, argc, (const char **)argv);

    if (ret == GED_OK) {
	if (aflag)
	    to_autoview_all_views(current_top);
	else
	    to_refresh_all_views(current_top);
    }

    return ret;
}



/* Wraps calls to commands that interactively request more arguments from the
 * user (such as "in") */
int
to_more_args_func(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr func,
		  const char *UNUSED(usage),
		  int UNUSED(maxargs))
{
    register int i;
    int ac;
    int total_ac;
    size_t total_alloc;
    size_t size_needed;
    int ret;
    char **av;
    struct bu_vls callback_cmd = BU_VLS_INIT_ZERO;
    struct bu_vls temp = BU_VLS_INIT_ZERO;
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;

    ac = argc;
    total_ac = ac + 1;
    total_alloc = total_ac * sizeof(char *);

    /* allocate space for initial args */
    av = (char **)bu_calloc(total_alloc, 1, "to_more_args_func");

    /* copy all args */
    for (i = 0; i < ac; ++i)
	av[i] = bu_strdup((char *)argv[i]);
    av[ac] = (char *)0;

    while ((ret = (*func)(gedp, ac, (const char **)av)) & GED_MORE) {
	int ac_more;
	const char **avmp;
	const char **av_more = NULL;

	if (0 < bu_vls_strlen(&tgd->go_more_args_callback)) {
	    bu_vls_trunc(&callback_cmd, 0);
	    bu_vls_printf(&callback_cmd, "%s [string range {%s} 0 end]",
			  bu_vls_addr(&tgd->go_more_args_callback),
			  bu_vls_addr(gedp->ged_result_str));

	    if (Tcl_Eval(current_top->to_interp, bu_vls_addr(&callback_cmd)) != TCL_OK) {
		bu_vls_trunc(gedp->ged_result_str, 0);
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
		Tcl_ResetResult(current_top->to_interp);
		ret = GED_ERROR;
		goto end;
	    }

	    bu_vls_trunc(&temp, 0);
	    bu_vls_printf(&temp, "%s", Tcl_GetStringResult(current_top->to_interp));
	    Tcl_ResetResult(current_top->to_interp);
	} else {
	    bu_log("\r%s", bu_vls_addr(gedp->ged_result_str));
	    bu_vls_trunc(&temp, 0);
	    if (bu_vls_gets(&temp, stdin) < 0) {
		break;
	    }
	}

	if (Tcl_SplitList(current_top->to_interp, bu_vls_addr(&temp), &ac_more, &av_more) != TCL_OK) {
	    continue;
	}

	if (ac_more < 1) {
	    /* space has still been allocated */
	    Tcl_Free((char *)av_more);

	    continue;
	}

	/* skip first element if empty */
	avmp = av_more;
	if (*avmp[0] == '\0') {
	    --ac_more;
	    ++avmp;
	}

	/* ignore last element if empty */
	if (*avmp[ac_more-1] == '\0')
	    --ac_more;

	/* allocate space for additional args */
	total_ac += ac_more;
	size_needed = total_ac * sizeof(char *);
	if (size_needed > total_alloc) {
	    while (size_needed > total_alloc)
		total_alloc *= 2;
	    av = (char **)bu_realloc((void *)av, total_alloc, "to_more_args_func additional");
	}

	/* copy additional args */
	for (i = 0; i < ac_more; ++i)
	    av[ac++] = bu_strdup(avmp[i]);
	av[ac] = (char *)0;

	Tcl_Free((char *)av_more);
    }

    bu_vls_printf(gedp->ged_result_str, "BUILT_BY_MORE_ARGS");
    for (i = 0; i < ac; ++i) {
	bu_vls_printf(gedp->ged_result_str, "%s ", av[i]);
    }

end:
    for (i = 0; i < ac; ++i) {
	bu_free((void *)av[i], "to_more_args_func");
    }
    bu_vls_free(&callback_cmd);
    bu_vls_free(&temp);
    bu_free((void *)av, "to_more_args_func");

    return ret;
}


int
to_pass_through_func(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr func,
		     const char *UNUSED(usage),
		     int UNUSED(maxargs))
{
    return (*func)(gedp, argc, argv);
}

/* Used for commands that may change the scene but do not need
 * to adjust the camera (such as "kill") - all views will need
 * to be updated */
int
to_pass_through_and_refresh_func(struct ged *gedp,
				 int argc,
				 const char *argv[],
				 ged_func_ptr func,
				 const char *UNUSED(usage),
				 int UNUSED(maxargs))
{
    int ret;

    ret = (*func)(gedp, argc, argv);

    if (ret == GED_OK)
	to_refresh_all_views(current_top);

    return ret;
}

int
to_view_func_common(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs,
		    int cflag,
		    int rflag)
{
    register int i;
    int ret;
    int ac;
    char **av;
    struct bview *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);
    av = (char **)bu_calloc(argc+1, sizeof(char *), "alloc av copy");

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (maxargs != TO_UNLIMITED && maxargs < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    gedp->ged_dmp = (struct dm *)gdvp->dmp;

    /* Copy argv into av while skipping argv[1] (i.e. the view name) */
    gedp->ged_gvp = gdvp;
    gedp->ged_refresh_clientdata = (void *)gdvp;
    av[0] = (char *)argv[0];
    ac = argc-1;
    for (i = 2; i < argc; ++i)
	av[i-1] = (char *)argv[i];
    av[i-1] = (char *)0;
    ret = (*func)(gedp, ac, (const char **)av);

    bu_free(av, "free av copy");

    /* Keep the view's perspective in sync with its corresponding display manager */
    dm_set_perspective((struct dm *)gdvp->dmp, gdvp->gv_perspective);

    if (gdvp->gv_s->adaptive_plot &&
	gdvp->gv_s->redraw_on_zoom)
    {
	char *gr_av[] = {"redraw", NULL};

	ged_redraw(gedp, 1, (const char **)gr_av);

	gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
	gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    }

    if (ret == GED_OK) {
	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	if (cflag && 0 < bu_vls_strlen(&tvd->gdv_callback)) {
	    struct bu_vls save_result = BU_VLS_INIT_ZERO;

	    bu_vls_printf(&save_result, "%s", bu_vls_addr(gedp->ged_result_str));
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&tvd->gdv_callback));
	    bu_vls_trunc(gedp->ged_result_str, 0);
	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&save_result));
	    bu_vls_free(&save_result);
	}

	if (rflag)
	    to_refresh_view(gdvp);
    }

    return ret;
}


/* For commands that involve a single "current" view (such as "rt") */
int
to_view_func(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
    return to_view_func_common(gedp, argc, argv, func, usage, maxargs, 0, 1);
}

int
to_view_func_less(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr func,
		  const char *usage,
		  int maxargs)
{
    return to_view_func_common(gedp, argc, argv, func, usage, maxargs, 1, 0);
}


int
to_view_func_plus(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr func,
		  const char *usage,
		  int maxargs)
{
    return to_view_func_common(gedp, argc, argv, func, usage, maxargs, 1, 1);
}

/* For functions that need the gedp display manager pointer to be that of
 * the current view before they are run. */
int
to_dm_func(struct ged *gedp,
	   int argc,
	   const char *argv[],
	   ged_func_ptr func,
	   const char *usage,
	   int maxargs)
{
    register int i;
    int ret;
    int ac;
    char **av;
    struct bview *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);
    av = (char **)bu_calloc(argc+1, sizeof(char *), "alloc av copy");

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (maxargs != TO_UNLIMITED && maxargs < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* Copy argv into av while skipping argv[1] (i.e. the view name) */
    gedp->ged_gvp = gdvp;
    gedp->ged_dmp = (void *)gdvp->dmp;
    gedp->ged_refresh_clientdata = (void *)gdvp;
    av[0] = (char *)argv[0];
    ac = argc-1;
    for (i = 2; i < argc; ++i)
	av[i-1] = (char *)argv[i];
    av[i-1] = (char *)0;
    ret = (*func)(gedp, ac, (const char **)av);

    bu_free(av, "free av copy");

    /* Keep the view's perspective in sync with its corresponding display manager */
    dm_set_perspective((struct dm *)gdvp->dmp, gdvp->gv_perspective);

    return ret;
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
