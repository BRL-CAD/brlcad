/*                  C O M M A N D _ I O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2000-2026 United States Government as represented by
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
/** @addtogroup libtclcad */
/** @{ */
/** @file libtclcad/command_io.cpp
 *
 * I/O callbacks for libtclcad.
 *
 */
/** @} */

#include "common.h"

#include <map>

extern "C" {
#include "tcl.h"
#include "bu/malloc.h"
}

#include "tclcad.h"
    /* Private headers */
#include "./tclcad_private.h"

struct tclcad_process_channels {
    Tcl_Channel cstdin;
    Tcl_Channel cstdout;
    Tcl_Channel cstderr;
};

struct tclcad_io_data *
tclcad_create_io_data()
{
    struct tclcad_io_data *d;
    BU_GET(d, struct tclcad_io_data);
    d->state = new std::map<struct ged_subprocess *, struct tclcad_process_channels *>;
    return d;
}

TCLCAD_EXPORT void
tclcad_destroy_io_data(struct tclcad_io_data *d)
{
    if (!d)
	return;

    std::map<struct ged_subprocess *, struct tclcad_process_channels *> *s = (std::map<struct ged_subprocess *, struct tclcad_process_channels *> *)d->state;
#ifdef USE_TCL_CHAN
    /* Normally ged_close has already removed these entries.  Be defensive so
     * an exceptional command path cannot leave Tcl channels retaining freed
     * subprocess ClientData. */
    for (auto &entry : *s) {
	struct tclcad_process_channels *pchan = entry.second;
	if (pchan->cstdin)
	    Tcl_Close(d->interp, pchan->cstdin);
	if (pchan->cstdout)
	    Tcl_Close(d->interp, pchan->cstdout);
	if (pchan->cstderr)
	    Tcl_Close(d->interp, pchan->cstderr);
	BU_PUT(pchan, struct tclcad_process_channels);
    }
#endif
    delete s;
    BU_PUT(d, struct tclcad_io_data);
}

/* Wrappers for setting up/tearing down IO handler */
#ifndef USE_TCL_CHAN
void
tclcad_create_io_handler(struct ged_subprocess *p, bu_process_io_t d, ged_io_func_t callback, void *data)
{
    if (!p || !p->p || !p->gedp || !p->gedp->ged_io_data)
	return;
    int fd = bu_process_fileno(p->p, d);

    // If invalid, nothing to do
    if (fd < 0)
	return;

    struct tclcad_io_data *t_iod = (struct tclcad_io_data *)p->gedp->ged_io_data;
    Tcl_CreateFileHandler(fd, t_iod->io_mode, callback, (ClientData)data);
    switch (d) {
	case BU_PROCESS_STDIN:
	    p->stdin_active = 1;
	    break;
	case BU_PROCESS_STDOUT:
	    p->stdout_active = 1;
	    break;
	case BU_PROCESS_STDERR:
	    p->stderr_active = 1;
	    break;
    }
}


void
tclcad_delete_io_handler(struct ged_subprocess *p, bu_process_io_t d)
{
    if (!p) return;
    int *active = NULL;
    switch (d) {
	case BU_PROCESS_STDIN: active = &p->stdin_active; break;
	case BU_PROCESS_STDOUT: active = &p->stdout_active; break;
	case BU_PROCESS_STDERR: active = &p->stderr_active; break;
    }
    if (!active || !*active)
	return;

    int fd = bu_process_fileno(p->p, d);

    /* Tcl file handlers observe the descriptor but do not own it.  The
     * bu_process object closes its descriptors when the child is reaped. */
    if (fd >= 0)
	Tcl_DeleteFileHandler(fd);
    *active = 0;
}


#else
static Tcl_Channel
tclcad_process_channel(int fd, int mode)
{
    HANDLE source = (HANDLE)_get_osfhandle(fd);
    HANDLE duplicate = INVALID_HANDLE_VALUE;

    if (source == INVALID_HANDLE_VALUE ||
	!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(),
		&duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS))
	return NULL;

    Tcl_Channel channel = Tcl_MakeFileChannel((ClientData)duplicate, mode);
    if (!channel)
	CloseHandle(duplicate);
    return channel;
}


void
tclcad_create_io_handler(struct ged_subprocess *p, bu_process_io_t d, ged_io_func_t callback, void *data)
{
    if (!p || !p->p || !p->gedp || !p->gedp->ged_io_data)
	return;
    struct tclcad_io_data *t_iod = (struct tclcad_io_data *)p->gedp->ged_io_data;
    std::map<struct ged_subprocess *, struct tclcad_process_channels *> *pmap = (std::map<struct ged_subprocess *, struct tclcad_process_channels *> *)t_iod->state;
    struct tclcad_process_channels *pchan = NULL;
    if (pmap->find(p) == pmap->end()) {
	BU_GET(pchan, struct tclcad_process_channels);
	pchan->cstdin = NULL;
	pchan->cstdout = NULL;
	pchan->cstderr = NULL;
	(*pmap)[p] = pchan;
    } else {
	pchan = (*pmap)[p];
    }

    int fd = bu_process_fileno(p->p, d);

    // If fd is invalid, nothing to do.
    if (fd < 0)
	return;

    switch (d) {
	case BU_PROCESS_STDIN:
	    p->stdin_active = 1;
	    if (!pchan->cstdin) {
		pchan->cstdin = tclcad_process_channel(fd, t_iod->io_mode);
		if (!pchan->cstdin) {
		    p->stdin_active = 0;
		    break;
		}
		Tcl_CreateChannelHandler(pchan->cstdin, t_iod->io_mode, callback, (ClientData)data);
	    }
	    break;
	case BU_PROCESS_STDOUT:
	    p->stdout_active = 1;
	    if (!pchan->cstdout) {
		pchan->cstdout = tclcad_process_channel(fd, t_iod->io_mode);
		if (!pchan->cstdout) {
		    p->stdout_active = 0;
		    break;
		}
		Tcl_CreateChannelHandler(pchan->cstdout, t_iod->io_mode, callback, (ClientData)data);
	    }
	    break;
	case BU_PROCESS_STDERR:
	    p->stderr_active = 1;
	    if (!pchan->cstderr) {
		pchan->cstderr = tclcad_process_channel(fd, t_iod->io_mode);
		if (!pchan->cstderr) {
		    p->stderr_active = 0;
		    break;
		}
		Tcl_CreateChannelHandler(pchan->cstderr, t_iod->io_mode, callback, (ClientData)data);
	    }
	    break;
    }
}


void
tclcad_delete_io_handler(struct ged_subprocess *p, bu_process_io_t d)
{
    if (!p || !p->p || !p->gedp || !p->gedp->ged_io_data)
	return;
    struct tclcad_io_data *t_iod = (struct tclcad_io_data *)p->gedp->ged_io_data;
    std::map<struct ged_subprocess *, struct tclcad_process_channels *> *pmap = (std::map<struct ged_subprocess *, struct tclcad_process_channels *> *)t_iod->state;
    struct tclcad_process_channels *pchan = NULL;
    if (!pmap || pmap->find(p) == pmap->end()) {
	return;
    }
    pchan = (*pmap)[p];
    switch (d) {
	case BU_PROCESS_STDIN:
	    if (p->stdin_active && pchan->cstdin) {
		Tcl_Close(t_iod->interp, pchan->cstdin);
		pchan->cstdin = NULL;
	    }
	    p->stdin_active = 0;
	    break;
	case BU_PROCESS_STDOUT:
	    if (p->stdout_active && pchan->cstdout) {
		Tcl_Close(t_iod->interp, pchan->cstdout);
		pchan->cstdout = NULL;
	    }
	    p->stdout_active = 0;
	    break;
	case BU_PROCESS_STDERR:
	    if (p->stderr_active && pchan->cstderr) {
		Tcl_Close(t_iod->interp, pchan->cstderr);
		pchan->cstderr = NULL;
	    }
	    p->stderr_active = 0;
	    break;
    }

    if (!pchan->cstdin && !pchan->cstdout && !pchan->cstderr) {
	/* All subprocess channels are gone; release the map entry only after
	 * the switch above has finished using pchan. */
	BU_PUT(pchan, struct tclcad_process_channels);
	pmap->erase(p);
    }
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
