/*                   T C L C A D _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
/** @file tclcad_private.h
 *
 * Private header for libtclcad.
 *
 */

#ifndef LIBTCLCAD_TCLCAD_PRIVATE_H
#define LIBTCLCAD_TCLCAD_PRIVATE_H

#include <tcl.h>

__BEGIN_DECLS

/**
 * function returns truthfully whether the library has been
 * initialized.  calling this routine with setit true considers the
 * library henceforth initialized.  there is presently no way to unset
 * or reset initialization.
 */
extern int library_initialized(int setit);


/**
 * Evaluates a TCL command, escaping the argument (if non-null).
 */
extern int tclcad_eval(Tcl_Interp *interp, const char *command, const char *arg);


/**
 * Like tclcad_eval(), but preserves the TCL result object.
 */
extern int tclcad_eval_quiet(Tcl_Interp *interp, const char *command, const char *arg);


/**
 * Evaluates a TCL command, escaping the list of arguments and optionally
 * preserving the TCL result object.
 */
extern int tclcad_eval_var(Tcl_Interp *interp, int preserve_result,
        const char *command, ...);


__END_DECLS

#endif /* LIBTCLCAD_TCLCAD_PRIVATE_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
