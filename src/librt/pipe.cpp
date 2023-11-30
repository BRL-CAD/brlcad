/*                      P I P E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file librt/pipe.cpp
 *
 * Code for sending and receiving low level DB structures over
 * STDIN/STDOUT pipes.
 *
 */
/** @} */



#include "common.h"

#include <cerrno>
#include <thread>
#include <fcntl.h>
#include <stdlib.h>
#include "bio.h"
#include "bu/time.h"
#include "raytrace.h"

#define SCANLEN 4096

int
rt_send_external(FILE *pipe, struct bu_external *ext)
{
    if (!pipe || !ext)
	return BRLCAD_ERROR;

    // First, send the size of the ext buffer
    if (fwrite(&ext->ext_nbytes, sizeof(size_t), 1, pipe) != 1) {
	strerror(errno);
	return BRLCAD_ERROR;
    }

    size_t offset = 0;
    // write the actual data to the pipe
    while (offset < ext->ext_nbytes) {
	size_t ret = fwrite(&ext->ext_buf[offset], 1, SCANLEN, pipe);
	if (ret != SCANLEN) {
	    strerror(errno);
	    return BRLCAD_ERROR;
	}
	offset += ret;
	if (ferror(pipe)) {
	    bu_log("rt_send_external: fwrite error");
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}

// This is challenging as we need to both read from an external process
// via a pipe and allow a timeout to kick in if that external process is
// in an infinite loop state.  Needs non-blocking I/O.
int
rt_read_external(struct bu_external **ext, int pipe, int timeout_ms, int coffset)
{
    if (!ext || pipe < 0)
	return 0;

    // TODO - figure out how to do this portably - maybe some sort of libbu wrapper?
    // See https://stackoverflow.com/q/870167 for a starting point
    int flags = fcntl(pipe, F_GETFL);
    fcntl(pipe, F_SETFL, flags | O_NONBLOCK);

    // To allow for incremental non-blocking reads, we have to handle a
    // previously partially populated bu_external struct.  If this is our
    // first call, set up the struct
    if (!*ext) {
	BU_GET(*ext, struct bu_external);
	BU_EXTERNAL_INIT(*ext);
	(*ext)->ext_buf = NULL;  // This being non-NULL is our termination criteria
    }

    int64_t start = bu_gettime();
    int64_t elapsed = 0;

    // First, read the size of the ext buffer we're expecting
    // TODO - harden this to spot if the process completes without sending us the right info...
    size_t ext_nbytes = 0;
    while (read(pipe, &ext_nbytes, sizeof(size_t)) != 1) {
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	elapsed = bu_gettime() - start;
	if (timeout_ms && elapsed > timeout_ms)
	    return 0;
    }

    if (!ext_nbytes) {
	BU_PUT(*ext, struct bu_external);
	*ext = NULL;
	return 0;
    }
    (*ext)->ext_nbytes = ext_nbytes;

    char *rbuf[SCANLEN];
    size_t offset = coffset;
    size_t bufsize = 4*SCANLEN;
    char *ext_data = (char *)bu_calloc(bufsize, sizeof(char), "initial ext buffer");
    // read from pipe
    while (offset < ext_nbytes) {
	ssize_t ret = read(pipe, rbuf, SCANLEN);
	if (ret > 0) {
	    memcpy((void *)(&ext_data[offset]), rbuf, (size_t)ret);
	    offset += ret;
	    if ((offset + SCANLEN) > bufsize) {
		bufsize = bufsize * 4;
		ext_data = (char *)bu_realloc(ext_data, bufsize, "bu_external buf");
	    }
	}
	if (ret == 0)
	    break;

	// read is non-blocking, so avoid hot spinning waiting for
	// client program to produce more I/O
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    bu_free(rbuf, "rbuf");

    (*ext)->ext_buf = (uint8_t *)ext_data;
    return 0;
}



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
