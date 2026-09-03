/*                        E N V . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
#include "bresource.h"

#ifdef HAVE_SYS_SYSINFO_H
#  include <sys/sysinfo.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
#  include <sys/sysctl.h>
#endif
#if defined(__FreeBSD__) && defined(HAVE_SYS_SYSCTL_H)
/* sys/user.h uses sig_t without including the signal header that defines it.
 * The alias also handles unity builds where another source file has already
 * included that header with its BSD-only declarations hidden. */
typedef void (*bu_freebsd_sig_t)(int);
#  define sig_t bu_freebsd_sig_t
#  include <sys/user.h>
#  undef sig_t
#endif
#ifdef HAVE_MACH_HOST_INFO_H
#  include <mach/host_info.h>
#endif
#ifdef HAVE_MACH_MACH_HOST_H
#  include <mach/mach_host.h>
#endif
#ifdef HAVE_WINDOWS_H
#  include <psapi.h>
#endif

#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/str.h"

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


static int
mem_size_from_uint64(uint64_t bytes, size_t *memsz)
{
    if (!memsz)
	return -1;
#if SIZE_MAX < UINT64_MAX
    *memsz = bytes > (uint64_t)SIZE_MAX ? SIZE_MAX : (size_t)bytes;
#else
    *memsz = (size_t)bytes;
#endif
    return 0;
}


static int
mem_page_size(size_t *memsz)
{
    if (!memsz)
	return -1;

#if defined(HAVE_WINDOWS_H)
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    if (!system_info.dwPageSize)
	return -1;
    *memsz = (size_t)system_info.dwPageSize;
#elif defined(_SC_PAGESIZE)
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
	return -1;
    *memsz = (size_t)page_size;
#else
    *memsz = (size_t)BU_PAGE_SIZE;
#endif
    return 0;
}


static int
mem_pages_to_bytes(uint64_t pages, size_t *memsz)
{
    size_t page_size = 0;
    if (mem_page_size(&page_size) != 0 ||
	    (page_size && pages > UINT64_MAX / (uint64_t)page_size))
	return -1;
    return mem_size_from_uint64(pages * (uint64_t)page_size, memsz);
}


static ssize_t
mem_result(size_t bytes, size_t *sz)
{
    const ssize_t result = (ssize_t)bytes;
    if (result < 0 || (size_t)result != bytes)
	return (ssize_t)-1;
    if (sz)
	*sz = bytes;
    return result;
}

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

    if (type == BU_MEM_PAGE_SIZE)
	return mem_page_size(memsz);

    long int sysmemory = 0;
    if (type == BU_MEM_AVAIL) {
	sysmemory = (long int)sysconf(_SC_AVPHYS_PAGES);
    } else {
	sysmemory = (long int)sysconf(_SC_PHYS_PAGES);
    }
    if (sysmemory < 0) {
	return -1;
    }

    return mem_pages_to_bytes((uint64_t)sysmemory, memsz);

#endif
    return 1;
}


static int
mem_proc_meminfo(int type, size_t *memsz)
{
    if (!memsz || type < 0)
	return -1;

#if defined(__linux__)
    static const uint64_t bytes_per_kibibyte = 1024u;
    const char *field = NULL;
    if (type == BU_MEM_ALL)
	field = "MemTotal:";
    if (type == BU_MEM_AVAIL)
	field = "MemAvailable:";
    if (!field)
	return 1;

    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo)
	return 1;

    char line[256] = {0};
    const size_t field_length = strlen(field);
    unsigned long long kibibytes = 0;
    int found = 0;
    while (bu_fgets(line, sizeof(line), meminfo)) {
	if (bu_strncmp(line, field, field_length) != 0)
	    continue;
	char unit[8] = {0};
	if (sscanf(line + field_length, "%llu %7s", &kibibytes, unit) == 2 &&
		BU_STR_EQUAL(unit, "kB"))
	    found = 1;
	break;
    }
    fclose(meminfo);
    if (!found || kibibytes > UINT64_MAX / bytes_per_kibibyte)
	return 1;
    return mem_size_from_uint64(
	(uint64_t)kibibytes * bytes_per_kibibyte, memsz);
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

    if (type == BU_MEM_PAGE_SIZE)
	return mem_page_size(memsz);

    uint64_t sysmemory = 0;
    if (type == BU_MEM_AVAIL) {
	sysmemory = (uint64_t)s.freeram;
    } else {
	sysmemory = (uint64_t)s.totalram;
    }

    const uint64_t memory_unit = s.mem_unit ? (uint64_t)s.mem_unit : 1u;
    if (sysmemory > UINT64_MAX / memory_unit)
	return -1;
    return mem_size_from_uint64(sysmemory * memory_unit, memsz);

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

#if defined(__APPLE__) && defined(HAVE_SYS_SYSCTL_H) && \
    defined(HAVE_MACH_HOST_INFO_H) && defined(HAVE_MACH_MACH_H)

    if (type == BU_MEM_PAGE_SIZE)
	return mem_page_size(memsz);

    if (type == BU_MEM_AVAIL) {
	mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
	vm_statistics_data_t vmstat;
	host_t host = mach_host_self();
	kern_return_t result = host_statistics(host, HOST_VM_INFO,
		(host_info_t)&vmstat, &count);
	(void)mach_port_deallocate(mach_task_self(), host);
	if (result != KERN_SUCCESS)
	    return -1;

	/* Inactive pages are reclaimable without paging another process's
	 * anonymous memory.  Do not include purgeable pages here because they
	 * may already be represented in another VM state. */
	uint64_t available_pages = (uint64_t)vmstat.free_count +
	    (uint64_t)vmstat.inactive_count;
	return mem_pages_to_bytes(available_pages, memsz);
    }

    uint64_t total_memory = 0;
    size_t value_size = sizeof(total_memory);
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    if (sysctl(mib, 2, &total_memory, &value_size, NULL, 0) != 0 ||
	    value_size != sizeof(total_memory))
	return -1;
    return mem_size_from_uint64(total_memory, memsz);

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

    if (type == BU_MEM_PAGE_SIZE)
	return mem_page_size(memsz);

    MEMORYSTATUSEX mavail = {0};
    mavail.dwLength = sizeof(mavail);
    if (!GlobalMemoryStatusEx(&mavail))
	return -1;
    uint64_t sysmemory = (type == BU_MEM_AVAIL) ?
	(uint64_t)mavail.ullAvailPhys : (uint64_t)mavail.ullTotalPhys;
    return mem_size_from_uint64(sysmemory, memsz);

#endif
    return 1;
}


static int
mem_sysctl(int type, size_t *memsz)
{
    if (!memsz)
	return -1;

    if (type < 0)
	return -2;

#if defined(__FreeBSD__) && defined(HAVE_SYS_SYSCTL_H) && defined(HAVE_SYSCTL)
    static const char *const page_size_name = "hw.pagesize";
    static const char *const total_memory_name = "hw.physmem";
    static const char *const free_pages_name = "vm.stats.vm.v_free_count";

    uint64_t page_size = 0;
    size_t value_size = sizeof(page_size);
    if (sysctlbyname(page_size_name, &page_size, &value_size, NULL, 0) != 0 ||
	    page_size == 0)
	return -1;
    if (type == BU_MEM_PAGE_SIZE)
	return mem_size_from_uint64(page_size, memsz);

    uint64_t memory = 0;
    value_size = sizeof(memory);
    const char *memory_name = total_memory_name;
    if (type == BU_MEM_AVAIL) {
	memory_name = free_pages_name;
	if (sysctlbyname(memory_name, &memory, &value_size, NULL, 0) != 0 ||
		memory > UINT64_MAX / page_size)
	    return -1;
	memory *= page_size;
    } else if (sysctlbyname(memory_name, &memory, &value_size, NULL, 0) != 0) {
	return -1;
    }

    return mem_size_from_uint64(memory, memsz);
#endif
    return 1;
}


static int
mem_process_usage(size_t *virtual_bytes, size_t *resident_bytes)
{
    if (!virtual_bytes && !resident_bytes)
	return -1;

#if defined(HAVE_WINDOWS_H)
    if (virtual_bytes)
	return 1;
    PROCESS_MEMORY_COUNTERS process_memory = {0};
    process_memory.cb = sizeof(process_memory);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &process_memory,
	    (DWORD)sizeof(process_memory)))
	return -1;
    return mem_size_from_uint64((uint64_t)process_memory.WorkingSetSize,
	    resident_bytes);
#elif defined(__linux__)
    FILE *statm = fopen("/proc/self/statm", "r");
    unsigned long long virtual_pages = 0;
    unsigned long long resident_pages = 0;
    const int have_pages = statm &&
	fscanf(statm, "%llu %llu", &virtual_pages, &resident_pages) == 2;
    if (statm)
	fclose(statm);
    if (!have_pages)
	return -1;
    if (virtual_bytes &&
	    mem_pages_to_bytes((uint64_t)virtual_pages, virtual_bytes) != 0)
	return -1;
    if (resident_bytes &&
	    mem_pages_to_bytes((uint64_t)resident_pages, resident_bytes) != 0)
	return -1;
    return 0;
#elif defined(__APPLE__) && defined(HAVE_MACH_MACH_H)
    struct task_basic_info task_memory;
    mach_msg_type_number_t task_count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO,
	    (task_info_t)&task_memory, &task_count) != KERN_SUCCESS)
	return -1;
    if (virtual_bytes && mem_size_from_uint64(
	    (uint64_t)task_memory.virtual_size, virtual_bytes) != 0)
	return -1;
    if (resident_bytes && mem_size_from_uint64(
	    (uint64_t)task_memory.resident_size, resident_bytes) != 0)
	return -1;
    return 0;
#elif defined(__FreeBSD__) && defined(HAVE_SYS_SYSCTL_H)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    struct kinfo_proc process_info;
    size_t process_info_size = sizeof(process_info);
    if (sysctl(mib, 4, &process_info, &process_info_size, NULL, 0) != 0 ||
	    process_info_size < sizeof(process_info))
	return -1;
    if (virtual_bytes && mem_size_from_uint64(
	    (uint64_t)process_info.ki_size, virtual_bytes) != 0)
	return -1;
    if (resident_bytes && mem_pages_to_bytes(
	    (uint64_t)process_info.ki_rssize, resident_bytes) != 0)
	return -1;
    return 0;
#elif defined(__NetBSD__) && defined(HAVE_SYS_SYSCTL_H)
    int mib[6] = {CTL_KERN, KERN_PROC2, KERN_PROC_PID, getpid(),
	sizeof(struct kinfo_proc2), 1};
    struct kinfo_proc2 process_info;
    size_t process_info_size = sizeof(process_info);
    if (sysctl(mib, 6, &process_info, &process_info_size, NULL, 0) != 0 ||
	    process_info_size < sizeof(process_info) ||
	    process_info.p_vm_msize < 0 || process_info.p_vm_rssize < 0)
	return -1;
    if (virtual_bytes && mem_pages_to_bytes(
	    (uint64_t)process_info.p_vm_msize, virtual_bytes) != 0)
	return -1;
    if (resident_bytes && mem_pages_to_bytes(
	    (uint64_t)process_info.p_vm_rssize, resident_bytes) != 0)
	return -1;
    return 0;
#elif defined(__OpenBSD__) && defined(HAVE_SYS_SYSCTL_H)
    int mib[6] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid(),
	sizeof(struct kinfo_proc), 1};
    struct kinfo_proc process_info;
    size_t process_info_size = sizeof(process_info);
    if (sysctl(mib, 6, &process_info, &process_info_size, NULL, 0) != 0 ||
	    process_info_size < sizeof(process_info))
	return -1;
    if (virtual_bytes && mem_size_from_uint64(
	    (uint64_t)process_info.p_vm_map_size, virtual_bytes) != 0)
	return -1;
    if (resident_bytes && mem_pages_to_bytes(
	    (uint64_t)process_info.p_vm_rssize, resident_bytes) != 0)
	return -1;
    return 0;
#elif defined(__DragonFly__) && defined(HAVE_SYS_SYSCTL_H) && \
    defined(HAVE_SYS_KINFO_H)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    struct kinfo_proc process_info;
    size_t process_info_size = sizeof(process_info);
    if (sysctl(mib, 4, &process_info, &process_info_size, NULL, 0) != 0 ||
	    process_info_size < sizeof(process_info))
	return -1;
    if (virtual_bytes && mem_size_from_uint64(
	    (uint64_t)process_info.kp_vm_map_size, virtual_bytes) != 0)
	return -1;
    if (resident_bytes && mem_pages_to_bytes(
	    (uint64_t)process_info.kp_vm_rssize, resident_bytes) != 0)
	return -1;
    return 0;
#endif
    return 1;
}


static int
mem_process_avail(size_t *memsz)
{
    if (!memsz)
	return -1;

#if defined(HAVE_WINDOWS_H)
    MEMORYSTATUSEX memory_status;
    memory_status.dwLength = sizeof(memory_status);
    if (!GlobalMemoryStatusEx(&memory_status))
	return -1;
    return mem_size_from_uint64(memory_status.ullAvailVirtual, memsz);
#elif defined(HAVE_SYS_RESOURCE_H) && (defined(__linux__) || \
    defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__))
    struct rlimit address_limit;
    if (getrlimit(RLIMIT_AS, &address_limit) != 0 ||
	address_limit.rlim_cur == RLIM_INFINITY)
	return 1;

    size_t address_bytes = 0;
    if (mem_process_usage(&address_bytes, NULL) != 0)
	return -1;

    size_t limit_bytes = 0;
    if (mem_size_from_uint64((uint64_t)address_limit.rlim_cur,
	    &limit_bytes) != 0)
	return -1;
    *memsz = address_bytes < limit_bytes ? limit_bytes - address_bytes : 0;
    return 0;
#endif
    return 1;
}


static int
mem_process_resident(size_t *memsz)
{
    if (!memsz)
	return -1;
    return mem_process_usage(NULL, memsz);
}


ssize_t
bu_mem(int type, size_t *sz)
{
    if (type < BU_MEM_ALL || type > BU_MEM_PROCESS_RESIDENT)
	return (ssize_t)-1;

    size_t subsz = 0;
    int ret = 0;

    if (type == BU_MEM_PROCESS_AVAIL) {
	ret = mem_process_avail(&subsz);
	if (ret != 0)
	    return (ssize_t)-1;
	return mem_result(subsz, sz);
    }
    if (type == BU_MEM_PROCESS_RESIDENT) {
	ret = mem_process_resident(&subsz);
	if (ret != 0)
	    return (ssize_t)-1;
	return mem_result(subsz, sz);
    }

    if (getenv("BU_MEM_NOCHECK")) {
	if (sz)
	    *sz = 0;
	return 0;
    }

    ret = mem_proc_meminfo(type, &subsz);
    if (ret == 0) {
	return mem_result(subsz, sz);
    }

    ret = mem_sysctl(type, &subsz);
    if (ret == 0) {
	return mem_result(subsz, sz);
    }

    ret = mem_host_info(type, &subsz);
    if (ret == 0) {
	return mem_result(subsz, sz);
    }

    ret = mem_status(type, &subsz);
    if (ret == 0) {
	return mem_result(subsz, sz);
    }

    ret = mem_sysconf(type, &subsz);
    if (ret == 0) {
	return mem_result(subsz, sz);
    }

    ret = mem_sysinfo(type, &subsz);
    if (ret == 0) {
	return mem_result(subsz, sz);
    }

    /* error if the above didn't work */
    return -1;
}

// If we've been asked for a non-graphical editor and we know we have a
// graphical one, or vice versa, flag as incompatible.  Otherwise, assume it
// will work since we have no way of knowing otherwise.
static int
editor_not_compatible(const char **elist, const char *candidate)
{
    if (!elist)
	return 0;

    int i = 0;
    const char *e_str = elist[i];
    char tstr[MAXPATHLEN];
    struct bu_vls component=BU_VLS_INIT_ZERO;
    while (e_str) {

	if (BU_STR_EQUAL(e_str, candidate))
	    return 1;

	bu_dir(tstr, MAXPATHLEN, candidate, BU_DIR_EXT, NULL);
	if (BU_STR_EQUAL(e_str, tstr))
	    return 1;

	bu_path_component(&component, candidate, BU_PATH_BASENAME_EXTLESS);
	if (BU_STR_EQUAL(e_str, bu_vls_cstr(&component))) {
	    bu_vls_free(&component);
	    return 1;
	}

	bu_path_component(&component, candidate, BU_PATH_EXTLESS);
	if (BU_STR_EQUAL(e_str, bu_vls_cstr(&component))) {
	    bu_vls_free(&component);
	    return 1;
	}

	e_str = elist[i++];
    }

    bu_vls_free(&component);

    return 0;
}

static int
editor_file_check(char *bu_editor, const char *estr, const char **elist)
{
    // First check if we have a mode issue
    if (editor_not_compatible(elist, estr))
	return 0;

    // If the input is a full, valid path go with that.
    if (bu_file_exists(estr, NULL)) {
	bu_strlcpy(bu_editor, estr, MAXPATHLEN);
	return 1;
    } else {
	const char *le = bu_dir(NULL, 0, estr, BU_DIR_EXT, NULL);
	if (bu_file_exists(le, NULL)) {
	    bu_strlcpy(bu_editor, le, MAXPATHLEN);
	    return 1;
	}
    }
    // Doesn't exist as-is - see if we have a BRL-CAD bundled copy
    const char *le = bu_dir(NULL, 0, BU_DIR_BIN, estr, NULL);
    if (bu_file_exists(le, NULL)) {
	bu_strlcpy(bu_editor, le, MAXPATHLEN);
	return 1;
    } else {
	le = bu_dir(NULL, 0, BU_DIR_BIN, estr, BU_DIR_EXT, NULL);
	if (bu_file_exists(le, NULL)) {
	    bu_strlcpy(bu_editor, le, MAXPATHLEN);
	    return 1;
	}
    }
    // Try bu_which
    const char *which_str = bu_which(estr);
    if (which_str) {
	bu_strlcpy(bu_editor, which_str, MAXPATHLEN);
	return 1;
    }

    return 0;
}

/* editors to test for */
#define EMACS_EDITOR "emacs"
#define GEDIT_EDITOR "gedit"
#define GTKEMACS_EDITOR "emacs-gtk"
#define GVIM_EDITOR "gvim"
#define KATE_EDITOR "kate"
#define MICRO_EDITOR "micro"
#define NANO_EDITOR "nano"
#define NOTEPADPP_EDITOR "C:/Program Files/Notepad++/notepad++.exe"
#define TEXTEDIT_EDITOR "/Applications/TextEdit.app/Contents/MacOS/TextEdit"
#define VIM_EDITOR "vim"
#define VI_EDITOR "vi"
#define WORDPAD_EDITOR "C:/Program Files/Windows NT/Accessories/wordpad.exe"
#define BRLEDIT_EDITOR "brledit"

const char *
bu_editor(struct bu_ptbl *editor_opts, int etype, int check_for_cnt, const char **check_for_editors)
{
    int i;
    const char *env_editor = NULL;
    static char bu_editor[MAXPATHLEN] = {0};
    const char *e_str = NULL;
    const char **ncompat_list = NULL;
    // Arrays for internal editor checking, in priority order.
    // Note that this order may be changed arbitrarily and is
    // explicitly NOT guaranteed by the API.
    const char *gui_editor_list[] = {
	NOTEPADPP_EDITOR, WORDPAD_EDITOR, TEXTEDIT_EDITOR, GEDIT_EDITOR, KATE_EDITOR, GTKEMACS_EDITOR, GVIM_EDITOR, NULL
    };
    const char *nongui_editor_list[] = {
	MICRO_EDITOR, NANO_EDITOR, EMACS_EDITOR, VIM_EDITOR, VI_EDITOR, BRLEDIT_EDITOR, NULL
    };
    if (etype == 1)
	ncompat_list = gui_editor_list;
    if (etype == 2)
	ncompat_list = nongui_editor_list;

    // Reset the editor_opts ptbl
    if (editor_opts)
	bu_ptbl_reset(editor_opts);

    // BRLCAD_EDITOR_GUI takes precedence, if set and GUI is an option
    if (!etype || etype == 2) {
	env_editor = getenv("BRLCAD_EDITOR_GUI");
	if (env_editor && env_editor[0] != '\0') {
	    if (editor_file_check(bu_editor, env_editor, ncompat_list))
		goto do_opt;
	}
    }

    // BRLCAD_EDITOR_CONSOLE takes precedence, if set and CONSOLE is an option
    if (!etype || etype == 1) {
	env_editor = getenv("BRLCAD_EDITOR_CONSOLE");
	if (env_editor && env_editor[0] != '\0') {
	    if (editor_file_check(bu_editor, env_editor, ncompat_list))
		goto do_opt;
	}
    }

    // VISUAL/EDITOR environment variables take precedence, if set
    env_editor = getenv("VISUAL");
    if (env_editor && env_editor[0] != '\0') {
	if (editor_file_check(bu_editor, env_editor, ncompat_list))
	    goto do_opt;
    }
    env_editor = getenv("EDITOR");
    if (env_editor && env_editor[0] != '\0') {
	if (editor_file_check(bu_editor, env_editor, ncompat_list))
	    goto do_opt;
    }

    // If the app wants us to check some candidates it has specified, handle
    // them first before investigating our default set.
    if (check_for_cnt && check_for_editors) {
	for (i = 0; i < check_for_cnt; i++) {
	    if (!check_for_editors[i]) {
		// If we reached a NULL entry in the list supplied by the
		// calling application, it means a) we weren't successful
		// and b) the application is requesting we not use libbu's
		// own list to continue.
		return NULL;
	    }
	    if (editor_file_check(bu_editor, check_for_editors[i], ncompat_list))
		goto do_opt;
	}
    }

    // No environment variable and no application-provided list - use
    // our internal list

    // Start with GUI editors
    if (!etype || etype == 2) {
	i = 0;
	e_str = gui_editor_list[i];
	while (e_str) {
	    if (editor_file_check(bu_editor, e_str, ncompat_list))
		goto do_opt;
	    e_str = gui_editor_list[i++];
	}
    }

    // Next up are console editors
    if (!etype || etype == 1) {
	i = 0;
	e_str = nongui_editor_list[i];
	while (e_str) {
	    if (editor_file_check(bu_editor, e_str, ncompat_list))
		goto do_opt;
	    e_str = nongui_editor_list[i++];
	}
    }

    // If we have nothing after all that, we're done
    return NULL;

do_opt:

    // If the caller didn't supply an option bu_ptbl, just return the editor
    // string
    if (!editor_opts)
	return (const char *)bu_editor;

    // If we're doing a console editor but we've got emacs, we need to add
    // the -nw option.
    struct bu_vls rootname = BU_VLS_INIT_ZERO;
    if (bu_path_component(&rootname, bu_editor, BU_PATH_BASENAME_EXTLESS)) {
	if (BU_STR_EQUAL(bu_vls_cstr(&rootname), "emacs") && etype == 1) {
	    // Non-graphical emacs requires an option
	    static const char *eopt = "-nw";
	    bu_ptbl_ins(editor_opts, (long *)eopt);
	}
    }
    bu_vls_free(&rootname);

    // Use both -multiInst and -nosession together for Notepad++ so we
    // get a stand-alone version rather than adding our editing file
    // to an existing instance - that produces unexpected results for
    // commands expecting to wait on the launched editor. See
    // https://superuser.com/questions/459705/open-two-instances-of-notepad
    if (BU_STR_EQUAL(bu_editor, NOTEPADPP_EDITOR)) {
	static const char *miopt = "-multiInst";
	static const char *nsopt = "-nosession";
	bu_ptbl_ins(editor_opts, (long *)miopt);
	bu_ptbl_ins(editor_opts, (long *)nsopt);
    }

    // Paths with spaces are Bad News - one of the BRL-CAD code paths for using
    // bu_editor outputs splits strings by spaces.  We can't do much on other
    // platforms, but on Windows (where such paths are more common to begin
    // with) we can try to use the short paths API.
#ifdef HAVE_WINDOWS_H
    char sp[MAXPATHLEN];
    DWORD r = GetShortPathNameA(bu_editor, sp, MAXPATHLEN);
    if (r != 0 && r < MAXPATHLEN) {
	// Unless short path call failed, use sp
	snprintf(bu_editor, MAXPATHLEN, "%s", sp);
    }
#endif

    return (const char *)bu_editor;
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
