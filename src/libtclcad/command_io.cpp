/*                  C O M M A N D _ I O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2000-2020 United States Government as represented by
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
#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bu/malloc.h"
#include "tclcad.h"

    /* Private headers */
#include "./tclcad_private.h"

}

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
    std::map<struct ged_subprocess *, struct tclcad_process_channels *> *s = (std::map<struct ged_subprocess *, struct tclcad_process_channels *> *)d->state;
    delete s;
    BU_PUT(d, struct tclcad_io_data);
}

/* Wrappers for setting up/tearing down IO handler */
#ifndef _WIN32
void
tclcad_create_io_handler(struct ged_subprocess *p, bu_process_io_t d, ged_io_func_t callback, void *data)
{
    if (!p || !p->p || !p->gedp || !p->gedp->ged_io_data)
	return;
    int *fdp = (int *)bu_process_fd(p->p, d);
    if (fdp) {
	struct tclcad_io_data *t_iod = (struct tclcad_io_data *)p->gedp->ged_io_data;
	Tcl_CreateFileHandler(*fdp, t_iod->io_mode, callback, (ClientData)data);
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
}


void
tclcad_delete_io_handler(struct ged_subprocess *p, bu_process_io_t d)
{
    if (!p) return;
    int *fdp = (int *)bu_process_fd(p->p, d);
    if (fdp) {
	Tcl_DeleteFileHandler(*fdp);
	close(*fdp);
    }
    switch (d) {
	case BU_PROCESS_STDIN:
	    p->stdin_active = 0;
	    break;
	case BU_PROCESS_STDOUT:
	    p->stdout_active = 0;
	    break;
	case BU_PROCESS_STDERR:
	    p->stderr_active = 0;
	    break;
    }
}


#else
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

    HANDLE *fdp = (HANDLE *)bu_process_fd(p->p, d);
    if (fdp) {
	switch (d) {
	    case BU_PROCESS_STDIN:
		p->stdin_active = 1;
		if (!pchan->cstdin) {
		    pchan->cstdin = Tcl_MakeFileChannel(*fdp, t_iod->io_mode);
		    Tcl_CreateChannelHandler(pchan->cstdin, t_iod->io_mode, callback, (ClientData)data);
		}
		break;
	    case BU_PROCESS_STDOUT:
		p->stdout_active = 1;
		if (!pchan->cstdout) {
		    pchan->cstdout = Tcl_MakeFileChannel(*fdp, t_iod->io_mode);
		    Tcl_CreateChannelHandler(pchan->cstdout, t_iod->io_mode, callback, (ClientData)data);
		}
		break;
	    case BU_PROCESS_STDERR:
		p->stderr_active = 1;
		if (!pchan->cstderr) {
		    pchan->cstderr = Tcl_MakeFileChannel(*fdp, t_iod->io_mode);
		    Tcl_CreateChannelHandler(pchan->cstderr, t_iod->io_mode, callback, (ClientData)data);
		}
		break;
	}
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
    if (!pchan->cstdin && !pchan->cstdout && !pchan->cstderr) {
	// All subprocess channels destroyed; we're done with the I/O from this subprocess, clean up
	BU_PUT(pchan, struct tclcad_process_channels);
	pmap->erase(p);
    }

    switch (d) {
	case BU_PROCESS_STDIN:
	    if (p->stdin_active && pchan->cstdin) {
		Tcl_DeleteChannelHandler(pchan->cstdin, NULL, (ClientData)NULL);
		Tcl_Close(t_iod->interp, pchan->cstdin);
		pchan->cstdin = NULL;
	    }
	    p->stdin_active = 0;
	    break;
	case BU_PROCESS_STDOUT:
	    if (p->stdout_active && pchan->cstdout) {
		Tcl_DeleteChannelHandler(pchan->cstdout, NULL, (ClientData)NULL);
		Tcl_Close(t_iod->interp, pchan->cstdout);
		pchan->cstdout = NULL;
	    }
	    p->stdout_active = 0;
	    break;
	case BU_PROCESS_STDERR:
	    if (p->stderr_active && pchan->cstderr) {
		Tcl_DeleteChannelHandler(pchan->cstderr, NULL, (ClientData)NULL);
		Tcl_Close(t_iod->interp, pchan->cstderr);
		pchan->cstderr = NULL;
	    }
	    p->stderr_active = 0;
	    break;
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

