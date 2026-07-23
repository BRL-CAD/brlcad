/*                 M G E D _ I N T E R R U P T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file mged/mged_interrupt.h
 *
 * Pure decision helpers for MGED's cooperative interrupt handling
 * These functions contain no MGED/Tcl/GED state and no side effects,
 * so the interrupt policy can be exercised headlessly.
 *
 * The rest of the wiring (signal handlers, the mged_interrupt Tcl
 * command, the mged_heartbeat reaction) lives in mged.c / cmd.c and
 * merely feeds these predicates and acts on their answers.
 */

#ifndef MGED_INTERRUPT_H
#define MGED_INTERRUPT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Rising-edge reactor decision for the mged_heartbeat timer.
 *
 * Given the previous pending value (@p prev) and the current one
 * (@p pending), returns 1 exactly on a 0->1 transition (react once),
 * and 0 otherwise (including while the flag stays raised).  When a
 * rising edge is detected and @p should_log is non-NULL, *should_log
 * is set to 1 iff there is something visibly running to report
 * ("interrupt requested — stopping..."), i.e. an in-process command
 * (@p cmd_running) or one or more live subprocesses (@p subp_len > 0).
 * On a non-edge, *should_log is set to 0.
 *
 * The reaction itself (ged_subprocesses_terminate) is unconditional on
 * the edge; only the human-facing log line is gated by should_log.
 */
int mged_interrupt_service(int prev, int pending, int cmd_running, int subp_len, int *should_log);

/**
 * Guard predicate for the mged_interrupt Tcl command / <Escape>.
 *
 * Returns 1 iff there is something to interrupt (an in-process command
 * is running, or at least one subprocess is registered), meaning the
 * caller should raise the interrupt flag and swallow the key; returns 0
 * when idle so the key event falls through to its normal binding.
 */
int mged_interrupt_should_act(int cmd_running, int subp_len);

#ifdef __cplusplus
}
#endif

#endif /* MGED_INTERRUPT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
