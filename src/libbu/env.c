/*                        E N V . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "bio.h"

#ifdef HAVE_SYS_SYSINFO_H
#  include <sys/sysinfo.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
#  include <sys/sysctl.h>
#endif
#ifdef HAVE_MACH_HOST_INFO_H
#  include <mach/host_info.h>
#endif
#ifdef HAVE_MACH_MACH_HOST_H
#  include <mach/mach_host.h>
#endif

#include "bu/env.h"
#include "bu/malloc.h"

/* strict c89 doesn't declare setenv() */
#ifndef HAVE_DECL_SETENV
extern int setenv(const char *, const char *, int);
#endif

/* Modern environments will technically allow much longer strings, but for
 * BRL-CAD's environment variable use cases if we're getting longer than this
 * something is probably wrong.  Revisit if this proves too short in practice,
 * but definitely want to see a valid real-world need before going bigger.
 * (https://stackoverflow.com/q/1078031/2037687) */
#define BU_ENV_MAXLEN 2047

int
bu_setenv(const char *name, const char *value, int overwrite)
{
    /* Sanity check setenv inputs */
    if (!name || !value)
	return EINVAL;
    if (strlen(name) > BU_ENV_MAXLEN)
	return ENOMEM;
    if (strlen(value) > BU_ENV_MAXLEN)
	return ENOMEM;

#ifdef HAVE_SETENV
    return setenv(name, value, overwrite);
#else
    int errcode = 0;

    if (!overwrite) {
	size_t envsize = 0;

#  ifdef HAVE_GETENV_S
	errcode = getenv_s(&envsize, NULL, 0, name);
#  else
	if (getenv(name) == NULL)
	    errcode = EINVAL;
#  endif
	if (errcode || envsize)
	    return errcode;
    }

    /* set/overwrite value */
#  ifdef HAVE__PUTENV_S
    return _putenv_s(name, value);
#  else
    {
	size_t maxlen = strlen(name)+strlen(value)+2;
	char *keyval = (char *)bu_malloc(maxlen, "setenv key=value copy/leak");
	snprintf(keyval, maxlen, "%s=%s", name, value);

	/* NOTE: we intentionally cannot free our key=value memory
	 * here due to legacy putenv() behavior.  the pointer becomes
	 * part of the environment.
	 */
	return putenv(keyval); }
#  endif

#endif
}


static int
mem_sysconf(int type, size_t *memsz)
{
    if (!memsz)
	return -1;

    if (type < 0)
	return -2;

#ifdef HAVE_SYSCONF_AVPHYS

    long int pagesize = (long int)sysconf(_SC_PAGESIZE);
    if (type == BU_MEM_PAGE_SIZE) {
	(*memsz) = (size_t)pagesize;
	return 0;
    }

    long int sysmemory = 0;
    if (type == BU_MEM_AVAIL) {
	sysmemory = (long int)sysconf(_SC_AVPHYS_PAGES);
    } else {
	sysmemory = (long int)sysconf(_SC_PHYS_PAGES);
    }
    if (sysmemory < 0) {
	return -1;
    }

    (*memsz) = pagesize * sysmemory;
    return 0;

#endif
    return 1;
}


static int
mem_sysinfo(int type, size_t *memsz)
{
    if (!memsz)
	return -1;

    if (type < 0)
	return -2;

#if defined(HAVE_SYS_SYSINFO_H)

    struct sysinfo s;
    if (sysinfo(&s)) {
	return -3;
    }

    // sysinfo doesn't provide this
    if (type == BU_MEM_PAGE_SIZE) {
	(*memsz) = (size_t)BU_PAGE_SIZE;
	return 0;
    }

    long int sysmemory = 0;
    if (type == BU_MEM_AVAIL) {
	sysmemory = (long int)s.freeram;
    } else {
	sysmemory = (long int)s.totalram;
    }
    if (sysmemory < 0) {
	return -1;
    }

    (*memsz) = s.mem_unit * sysmemory;
    return 0;

#endif
    return 1;
}


static int
mem_host_info(int type, size_t *memsz)
{
    if (!memsz)
	return -1;

    if (type < 0)
	return -2;

#if defined(HAVE_SYS_SYSTCL_H) && defined(HAVE_MACH_HOST_INFO_H)

    long int pagesize = 0;
    size_t osize = sizeof(pagesize);
    int sargs[2] = {CTL_HW, HW_PAGESIZE};
    if (sysctl(sargs, 2, &pagesize, &osize, NULL, 0) < 0) {
	return -1;
    }
    if (type == BU_MEM_PAGE_SIZE) {
	(*memsz) = (size_t)pagesize;
	return 0;
    }

    long int sysmemory = 0;
    if (type == BU_MEM_AVAIL) {
	// See info at https://stackoverflow.com/a/6095158/2037687
	mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
	vm_statistics_data_t vmstat;
	if (host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vmstat, &count) != KERN_SUCCESS) {
	    return -1;
	}
	sysmemory = (long int) (vmstat.free_count * pagesize / (1024*1024));
    } else {
	sargs[1] = HW_MEMSIZE;
	sysctl(sargs, 2, &sysmemory, &osize, NULL, 0);
    }
    if (sysmemory < 0) {
	return -1;
    }

    (*memsz) = pagesize * sysmemory;
    return 0;

#endif
    return 1;
}


static int
mem_status(int type, size_t *memsz)
{
    if (!memsz)
	return -1;

    if (type < 0)
	return -2;

#if defined(HAVE_WINDOWS_H)

    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    size_t pagesize = (size_t)sysinfo.dwPageSize;
    if (type == BU_MEM_PAGE_SIZE) {
	(*memsz) = pagesize;
	return 0;
    }

    size_t sysmemory = 0;
    MEMORYSTATUSEX mavail;
    mavail.dwLength = sizeof(mavail);
    GlobalMemoryStatusEx(&mavail);
    if (type == BU_MEM_AVAIL) {
	sysmemory = (size_t)mavail.ullAvailPhys;
    } else {
	sysmemory = (size_t)mavail.ullTotalPhys;
    }
    (*memsz) = sysmemory;
    return 0;

#endif
    return 1;
}


ssize_t
bu_mem(int type, size_t *sz)
{
    if (type < 0)
	return (ssize_t)-1;

    size_t subsz = 0;
    unsigned long long ret = 0;

    if (getenv("BU_AVAILABLE_MEM_NOCHECK")) {
	if (sz)
	    *sz = 0;
	return 0;
    }

    ret = mem_host_info(type, &subsz);
    if (ret == 0) {
	if (sz)
	    *sz = subsz;
	return subsz;
    }

    ret = mem_status(type, &subsz);
    if (ret == 0) {
	if (sz)
	    *sz = subsz;
	return subsz;
    }

    ret = mem_sysconf(type, &subsz);
    if (ret == 0) {
	if (sz)
	    *sz = subsz;
	return subsz;
    }

    ret = mem_sysinfo(type, &subsz);
    if (ret == 0) {
	if (sz)
	    *sz = subsz;
	return subsz;
    }

    /* error if the above didn't work */
    return -1;
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
