/*          C H E C K _ M U L T I _ I N T E R P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file check_multi_interp.cpp
 *
 * Verify that libtclcad initialization and its Tcl object registries are
 * scoped to each interpreter.
 */

#include "common.h"

#include <cstdio>
#include <cstring>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/vls.h"
#include "tclcad.h"
#include "../tclcad_private.h"


static bool
eval_ok(Tcl_Interp *interp, const char *script)
{
    if (Tcl_Eval(interp, script) == TCL_OK)
	return true;

    std::fprintf(stderr, "Tcl command failed: %s\n%s\n", script,
	    Tcl_GetStringResult(interp));
    return false;
}


static bool
result_is(Tcl_Interp *interp, const char *expected)
{
    return BU_STR_EQUAL(Tcl_GetStringResult(interp), expected);
}


static bool
has_command(Tcl_Interp *interp, const char *command)
{
    Tcl_CmdInfo command_info;
    return Tcl_GetCommandInfo(interp, command, &command_info) != 0;
}


static bool
init_tclcad(Tcl_Interp *interp, int init_gui)
{
    struct bu_vls log = BU_VLS_INIT_ZERO;
    int status = tclcad_init(interp, init_gui, &log);
    if (status != TCL_OK)
	std::fprintf(stderr, "%s", bu_vls_cstr(&log));
    bu_vls_free(&log);
    return status == TCL_OK;
}


static bool
check_gui_packages(Tcl_Interp *interp)
{
    const char *script =
	"proc tops {args} {return {}};"
	"proc who {} {return {}};"
	"proc graph {args} {return {}};"
	"foreach package {"
	" Archer cadwidgets::Ged RtWizard::Wizard Sdialogs Swidgets"
	"} {package require $package};"
	"foreach class {"
	" ::DataUtils ::sdialogs::Stddlgs ::swidgets::Togglearrow"
	"} {"
	" if {![llength [info commands $class]] && ![auto_load $class]} {"
	"  error \"class $class is not autoloadable\""
	" }"
	"};"
	"expr {"
	" [llength [info commands ::Archer]] == 1 &&"
	" [llength [info commands ::cadwidgets::Ged]] == 1 &&"
	" [llength [info commands ::RtWizard::Wizard]] == 1"
	"}";

    if (!eval_ok(interp, script) || !result_is(interp, "1")) {
	std::fprintf(stderr, "GUI package initialization was incomplete: %s\n",
	    Tcl_GetStringResult(interp));
	return false;
    }

    return true;
}


static bool
check_initialized(Tcl_Interp *interp, const char *value, bool init_gui)
{
    if (!has_command(interp, "bu_dir") ||
	!has_command(interp, "bn_noise_perlin") ||
	!has_command(interp, "dm_open") ||
	!has_command(interp, "go_open") ||
	!has_command(interp, "ch_open")) {
	std::fprintf(stderr, "libtclcad did not register all expected commands\n");
	return false;
    }
    if (init_gui &&
	(!has_command(interp, "frame") ||
	 !Tcl_PkgPresent(interp, "Tk", "8.6", 0) ||
	 !Tcl_PkgPresent(interp, "Itk", TCLCAD_ITK_MIN_VERSION, 0))) {
	std::fprintf(stderr, "libtclcad did not complete GUI initialization\n");
	return false;
    }

    if (init_gui && !check_gui_packages(interp))
	return false;

    const char *script =
	"itcl::class MultiInterpClass {"
	" variable value;"
	" constructor {v} {set value $v};"
	" method get {} {return $value}"
	"};"
	"MultiInterpClass multi_interp_object expected;"
	"multi_interp_object get";
    if (!eval_ok(interp, script))
	return false;
    if (!result_is(interp, "expected")) {
	std::fprintf(stderr, "Itcl object returned an unexpected value: %s\n",
	    Tcl_GetStringResult(interp));
	return false;
    }

    if (!eval_ok(interp, "ch_open shared_history; llength [ch_open]"))
	return false;
    if (!result_is(interp, "1")) {
	std::fprintf(stderr, "command history registry crossed interpreter boundaries\n");
	return false;
    }

    Tcl_SetVar(interp, "multi_interp_value", value, TCL_GLOBAL_ONLY);
    return true;
}


static bool
open_shared_database(Tcl_Interp *first, Tcl_Interp *second)
{
    char database[MAXPATHLEN] = {0};
    bu_dir(database, MAXPATHLEN, BU_DIR_DATA, "db", "m35.g", NULL);
    if (!bu_file_exists(database, NULL)) {
	std::fprintf(stderr, "Unable to find test database: %s\n", database);
	return false;
    }

    if (!Tcl_SetVar(first, "multi_interp_database", database, TCL_GLOBAL_ONLY) ||
	!Tcl_SetVar(second, "multi_interp_database", database, TCL_GLOBAL_ONLY) ||
	!eval_ok(first, "go_open shared_ged db $multi_interp_database") ||
	!eval_ok(second, "go_open shared_ged db $multi_interp_database") ||
	!eval_ok(first, "llength [go_open]") ||
	!result_is(first, "1") ||
	!eval_ok(second, "llength [go_open]") ||
	!result_is(second, "1")) {
	std::fprintf(stderr, "GED object registry crossed interpreter boundaries\n");
	return false;
    }

    return true;
}


static bool
check_framebuffer_registry_growth(Tcl_Interp *interp)
{
    const char *script =
	"for {set i 0} {$i < 10} {incr i} {"
	" fb_open lifecycle_fb_$i /dev/mem -s 4"
	"};"
	"if {[llength [fb_open]] != 10} {"
	" error {framebuffer registry has the wrong initial size}"
	"};"
	"rename lifecycle_fb_4 {};"
	"if {[llength [fb_open]] != 9} {"
	" error {framebuffer registry has the wrong size after deletion}"
	"};"
	"expr {[lifecycle_fb_0 getwidth] == 4 &&"
	" [lifecycle_fb_9 getheight] == 4}";

    return eval_ok(interp, script) &&
	result_is(interp, "1");
}


static bool
open_shared_runtime_objects(Tcl_Interp *first, Tcl_Interp *second, bool init_gui)
{
    const char *open_framebuffer = "fb_open shared_fb /dev/mem -s 16";
    const char *open_display_manager = "dm_open shared_dm X";

    if (!eval_ok(first, open_framebuffer) ||
	!eval_ok(second, open_framebuffer) ||
	!eval_ok(first, "llength [fb_open]") ||
	!result_is(first, "1") ||
	!eval_ok(second, "llength [fb_open]") ||
	!result_is(second, "1")) {
	std::fprintf(stderr, "framebuffer registry crossed interpreter boundaries\n");
	return false;
    }

    if (init_gui &&
	(!eval_ok(first, open_display_manager) ||
	 !eval_ok(second, open_display_manager) ||
	 !eval_ok(first, "llength [dm_open]") ||
	 !result_is(first, "1") ||
	 !eval_ok(second, "llength [dm_open]") ||
	 !result_is(second, "1"))) {
	std::fprintf(stderr, "display manager registry crossed interpreter boundaries\n");
	return false;
    }

    return true;
}


int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    Tcl_FindExecutable(argv[0]);

    bool init_gui = false;
    if (argc == 2 && BU_STR_EQUAL(argv[1], "--gui")) {
	init_gui = true;
    } else if (argc != 1) {
	std::fprintf(stderr, "Usage: %s [--gui]\n", argv[0]);
	return 1;
    }

    struct bu_vls null_log = BU_VLS_INIT_ZERO;
    if (tclcad_init(NULL, 0, &null_log) != TCL_ERROR ||
	std::strstr(bu_vls_cstr(&null_log), "NULL interpreter") == NULL) {
	std::fprintf(stderr, "NULL interpreter was not rejected deliberately\n");
	bu_vls_free(&null_log);
	return 1;
    }
    bu_vls_free(&null_log);

    Tcl_Interp *first = Tcl_CreateInterp();
    Tcl_Interp *second = Tcl_CreateInterp();
    if (!first || !second) {
	std::fprintf(stderr, "Unable to create Tcl interpreters\n");
	if (first)
	    Tcl_DeleteInterp(first);
	if (second)
	    Tcl_DeleteInterp(second);
	return 1;
    }

    /*
     * Initializing GED alone must not suppress full TclCAD initialization.
     * The GUI run also checks the supported core-to-GUI upgrade sequence.
     */
    if (Ged_Init(first) != TCL_OK || !has_command(first, "go_open") ||
	has_command(first, "bu_dir") || !init_tclcad(first, 0) ||
	(init_gui && !init_tclcad(first, 1)) ||
	!init_tclcad(first, init_gui) ||
	!init_tclcad(second, init_gui) ||
	!check_initialized(first, "first", init_gui) ||
	!check_initialized(second, "second", init_gui) ||
	!open_shared_database(first, second) ||
	!open_shared_runtime_objects(first, second, init_gui)) {
	Tcl_DeleteInterp(first);
	Tcl_DeleteInterp(second);
	return 1;
    }

    Tcl_DeleteInterp(first);
    if (!eval_ok(second, "set multi_interp_value") ||
	!result_is(second, "second") ||
	!eval_ok(second, "multi_interp_object get") ||
	!eval_ok(second, "llength [ch_open]") ||
	!result_is(second, "1") ||
	!eval_ok(second, "llength [shared_ged tops]") ||
	result_is(second, "0") ||
	!eval_ok(second, "shared_fb getwidth") ||
	!result_is(second, "16") ||
	(init_gui && !eval_ok(second, "shared_dm get_aspect"))) {
	Tcl_DeleteInterp(second);
	return 1;
    }
    Tcl_DeleteInterp(second);

    Tcl_Interp *replacement = Tcl_CreateInterp();
    if (!replacement || !init_tclcad(replacement, init_gui) ||
	!check_initialized(replacement, "replacement", init_gui) ||
	!check_framebuffer_registry_growth(replacement)) {
	if (replacement)
	    Tcl_DeleteInterp(replacement);
	return 1;
    }
    Tcl_DeleteInterp(replacement);

    std::printf("libtclcad initialized independent and replacement interpreters%s\n",
	init_gui ? " with Tk" : "");
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
