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


static unsigned long long
_bu_mem_sysconf(int type, int *ec)
{
    if (type < 0) {
	(*ec) = -2;
	return 0;
    }
#ifdef HAVE_SYSCONF_AVPHYS

    long int pagesize = (long int)sysconf(_SC_PAGESIZE);
    if (type == BU_MEM_PAGE_SIZE) {
	(*ec) = 1;
	return pagesize;
    }

    long int sysmemory = 0;
    if (type == BU_MEM_AVAIL) {
	sysmemory = (long int)sysconf(_SC_AVPHYS_PAGES);
    } else {
	sysmemory = (long int)sysconf(_SC_PHYS_PAGES);
    }
    if (sysmemory < 0) {
	(*ec) = -1;
	return 0;
    }

    unsigned long long avail_bytes = pagesize * sysmemory;
    (*ec) = 1;
    return avail_bytes;

#endif
    return 0;
}

static unsigned long long
_bu_mem_sysinfo(int type, int *ec)
{
    if (type < 0) {
	(*ec) = -2;
	return 0;
    }
#if defined(HAVE_SYS_SYSINFO_H)

    struct sysinfo s;
    if (sysinfo(&s)) {
	(*ec) = -1;
	return 0;
    }

    // sysinfo doesn't provide this
    if (type == BU_MEM_PAGE_SIZE) {
	(*ec) = 1;
	return (unsigned long long)BU_PAGE_SIZE;
    }

    long int sysmemory = 0;
    if (type == BU_MEM_AVAIL) {
	sysmemory = (long int)s.freeram;
    } else {
	sysmemory = (long int)s.totalram;
    }
    if (sysmemory < 0) {
	(*ec) = -1;
	return 0;
    }

    unsigned long long avail_bytes = s.mem_unit * sysmemory;
    (*ec) = 1;
    return avail_bytes;

#endif
    return 0;
}

static unsigned long long
_bu_mem_host_info(int type, int *ec)
{
    if (type < 0) {
	(*ec) = -2;
	return 0;
    }
#if defined(HAVE_SYS_SYSTCL_H) && defined(HAVE_MACH_HOST_INFO_H)
    long int pagesize = 0;
    size_t osize = sizeof(pagesize);
    int sargs[2] = {CTL_HW, HW_PAGESIZE};
    if (sysctl(sargs, 2, &pagesize, &osize, NULL, 0) < 0) {
	(*ec) = -1;
	return 0;
    }
    if (type == BU_MEM_PAGE_SIZE) {
	(*ec) = 1;
	return (unsigned long long)pagesize;
    }

    long int sysmemory = 0;
    if (type == BU_MEM_AVAIL) {
	// See info at https://stackoverflow.com/a/6095158/2037687
	mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
	vm_statistics_data_t vmstat;
	if (host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vmstat, &count) != KERN_SUCCESS)
	{
	    (*ec) = -1;
	    return 0;
	}
	sysmemory = (long int) (vmstat.free_count * pagesize / (1024*1024));
    } else {
	sargs[1] = HW_MEMSIZE;
	sysctl(sargs, 2, &sysmemory, &osize, NULL, 0);
    }
    if (sysmemory < 0) {
	(*ec) = -1;
	return 0;
    }

    unsigned long long avail_bytes = pagesize * sysmemory;
    (*ec) = 1;
    return avail_bytes;

#endif
    return 0;
}

static unsigned long long
_bu_mem_status(int type, int *ec)
{
    if (type < 0) {
	(*ec) = -2;
	return 0;
    }
#if defined(HAVE_WINDOWS_H)

    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    unsigned long long pagesize = (long int)sysinfo.dwPageSize;
    if (type == BU_MEM_PAGE_SIZE) {
	(*ec) = 1;
	return pagesize;
    }

    unsigned long long sysmemory = 0;
    MEMORYSTATUSEX mavail;
    mavail.dwLength = sizeof(mavail);
    GlobalMemoryStatusEx(&mavail);
    if (type == BU_MEM_AVAIL) {
	sysmemory = mavail.ullAvailPhys;
    } else {
	sysmemory = mavail.ullTotalPhys;
    }
    (*ec) = 1;
    return sysmemory;

#endif
    return 0;
}

unsigned long long
bu_mem(int type, int *ec)
{
    unsigned long long ret = 0;
    int success = 0;
    if (!ec)
	return 0;

    if (getenv("BU_AVAILABLE_MEM_NOCHECK")) {
	(*ec) = 0;
	return ret;
    }

    ret = _bu_mem_host_info(type, &success);
    if (success == 1) {
	(*ec) = 0;
	return ret;
    }

    ret = _bu_mem_status(type, &success);
    if (success == 1) {
	(*ec) = 0;
	return ret;
    }

    ret = _bu_mem_sysconf(type, &success);
    if (success == 1) {
	(*ec) = 0;
	return ret;
    }

    ret = _bu_mem_sysinfo(type, &success);
    if (success == 1) {
	(*ec) = 0;
	return ret;
    }

    /* Don't know how to figure this out if the above haven't worked. */
    (*ec) = -3;
    return ret;
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
