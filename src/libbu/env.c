/*                        E N V . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2024 United States Government as represented by
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

#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/malloc.h"
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

    if (getenv("BU_MEM_NOCHECK")) {
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

/* editors to test */
#define EMACS_EDITOR "emacs"
#define GEDIT_EDITOR "gedit"
#define GVIM_EDITOR "gvim"
#define KATE_EDITOR "kate"
#define MICRO_EDITOR "micro"
#define NANO_EDITOR "nano"
#define NOTEPADPP_EDITOR "\"c:/Program Files/Notepad++/notepad++.exe\""
#define TEXTEDIT_EDITOR "/Applications/TextEdit.app/Contents/MacOS/TextEdit"
#define VIM_EDITOR "vim"
#define VI_EDITOR "vi"
#define WORDPAD_EDITOR "\"c:/Program Files/Windows NT/Accessories/wordpad\""

// TODO - long ago, BRL-CAD bundled jove to always guarantee basic text editing
// capabilities.  We haven't done that for a while (jove didn't end up getting
// much development momentum), but bext's availability makes that a somewhat
// less painful possibly to consider now.
//
// The specific motivation for thinking about this is the lack on Windows of a
// built-in console editor - all the solutions seem to involve requiring the
// user be able to install a 3rd party solution themselves with something like
// winget.  Apparently this can be a real nuisance for people doing remote ssh
// into to Windows servers.  Microsoft seems to be considering proving a
// default CLI editor again, similar to what the used to provide with edit.exe,
// but that doesn't seem to have materialized yet.  See discussion at
// https://github.com/microsoft/terminal/discussions/16440
//
// https://github.com/malxau/yori does already implement a modern MIT licensed
// clone of the old edit.exe editor which would have been a fallback on older
// Windows systems - even if it doesn't ultimately get included in newer
// Windows systems, we may be able to build the necessary pieces in bext to
// provide yedit.exe ourselves...  As far as I know every other environment we
// target has at least vi available by default - Windows is the outlier.
//
// Main argument against it is if Microsoft really does use the above work to
// restore a default EDIT.exe in newer Windows versions, then a bext version
// would (eventually) become moot.

const char *
bu_editor(const char **editor_opt, int no_gui, int check_for_cnt, const char **check_for_editors)
{
    int i;
    static char bu_editor[MAXPATHLEN] = {0};
    static char bu_editor_opt[MAXPATHLEN] = {0};
    static char bu_editor_tmp[MAXPATHLEN] = {0};
    const char *which_str = NULL;
    const char *e_str = NULL;
    // Arrays for internal editor checking, in priority order.
    // Note that this order may be changed arbitrarily and is
    // explicitly NOT guaranteed by the API.
    const char *gui_editor_list[] = {
	NOTEPADPP_EDITOR, WORDPAD_EDITOR, TEXTEDIT_EDITOR, GEDIT_EDITOR, KATE_EDITOR, GVIM_EDITOR, EMACS_EDITOR, NULL
    };
    const char *nongui_editor_list[] = {
	MICRO_EDITOR, NANO_EDITOR, EMACS_EDITOR, VIM_EDITOR, VI_EDITOR, NULL
    };
    const char **editor_list = (no_gui) ? nongui_editor_list : gui_editor_list;


    // EDITOR environment variable takes precedence, if set
    const char *env_editor = getenv("EDITOR");
    if (env_editor && env_editor[0] != '\0') {
	// If EDITOR is a full, valid path we are done
	if (bu_file_exists(env_editor, NULL)) {
	    bu_strlcpy(bu_editor, env_editor, MAXPATHLEN);
	    goto do_opt;
	}
	// Doesn't exist as-is - try bu_which
	which_str = bu_which(env_editor);
	if (which_str) {
	    bu_strlcpy(bu_editor, which_str, MAXPATHLEN);
	    goto do_opt;
	}
	// Neither of the above worked - just pass through $EDITOR as-is
	bu_strlcpy(bu_editor, env_editor, MAXPATHLEN);
	goto do_opt;
    }

    // The app wants us to check some things, before investigating our default
    // set - handle them first.
    if (check_for_cnt && check_for_editors) {
	for (i = 0; i < check_for_cnt; i++) {
	    // If it exists as-is, go with that.
	    if (bu_file_exists(check_for_editors[i], NULL)) {
		bu_strlcpy(bu_editor, check_for_editors[i], MAXPATHLEN);
		goto do_opt;
	    }
	    // Doesn't exist as-is - try bu_which
	    which_str = bu_which(env_editor);
	    if (which_str) {
		bu_strlcpy(bu_editor, which_str, MAXPATHLEN);
		goto do_opt;
	    }
	}
    }

    // No environment variable and no application-provided list - use
    // our internal list
    i = 0;
    e_str = editor_list[i];
    while (e_str) {
	which_str = bu_which(e_str);
	if (which_str) {
	    bu_strlcpy(bu_editor, which_str, MAXPATHLEN);
	    goto do_opt;
	}
	e_str = editor_list[i++];
    }

    // If we have nothing after all that, we're done
    if (editor_opt)
	*editor_opt = NULL;
    return NULL;

do_opt:
    // If the caller didn't supply an option pointer, just return the editor
    // string
    if (!editor_opt)
	return (const char *)bu_editor;

    // Supply any options needed (normally graphical editor needing to be
    // non-graphical due to no_gui being set, for example.)

    snprintf(bu_editor_tmp, MAXPATHLEN, bu_which("emacs"));
    if (BU_STR_EQUAL(bu_editor, bu_editor_tmp) && no_gui) {
	// Non-graphical emacs requires an option
	sprintf(bu_editor_opt, "-nw");
    }

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
