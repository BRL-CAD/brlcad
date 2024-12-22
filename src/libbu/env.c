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
#define NOTEPADPP_EDITOR "C:/Program Files/Notepad++/notepad++"
#define TEXTEDIT_EDITOR "/Applications/TextEdit.app/Contents/MacOS/TextEdit"
#define VIM_EDITOR "vim"
#define VI_EDITOR "vi"
#define WORDPAD_EDITOR "C:/Program Files/Windows NT/Accessories/wordpad"
#define YEDIT_EDITOR "yedit"

const char *
bu_editor(const char **editor_opt, int etype, int check_for_cnt, const char **check_for_editors)
{
    int i;
    static char bu_editor[MAXPATHLEN] = {0};
    static char bu_editor_opt[MAXPATHLEN] = {0};
    static char bu_editor_tmp[MAXPATHLEN] = {0};
    const char *e_str = NULL;
    const char **ncompat_list = NULL;
    // Arrays for internal editor checking, in priority order.
    // Note that this order may be changed arbitrarily and is
    // explicitly NOT guaranteed by the API.
    const char *gui_editor_list[] = {
	NOTEPADPP_EDITOR, WORDPAD_EDITOR, TEXTEDIT_EDITOR, GEDIT_EDITOR, KATE_EDITOR, GTKEMACS_EDITOR, GVIM_EDITOR, NULL
    };
    const char *nongui_editor_list[] = {
	MICRO_EDITOR, NANO_EDITOR, EMACS_EDITOR, VIM_EDITOR, VI_EDITOR, YEDIT_EDITOR, NULL
    };
    if (etype == 1)
	ncompat_list = gui_editor_list;
    if (etype == 2)
	ncompat_list = nongui_editor_list;

    // VISUAL/EDITOR environment variables takes precedence, if set
    const char *env_editor = getenv("VISUAL");
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
		if (editor_opt)
		    *editor_opt = NULL;
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

    snprintf(bu_editor_tmp, MAXPATHLEN, "%s", bu_which("emacs"));
    if (BU_STR_EQUAL(bu_editor, bu_editor_tmp) && etype == 1) {
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
