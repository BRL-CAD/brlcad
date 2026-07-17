/*                     B A C K T R A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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

#include "common.h"

/* system headers */
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_TIMES_H
#  include <sys/times.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_PROCESS_H
#  include <process.h>
#endif
#if defined(__has_include)
#  if __has_include(<execinfo.h>)
#    include <execinfo.h>
#    define BU_HAVE_EXECINFO_BACKTRACE 1
#  endif
#endif
#if !defined(BU_HAVE_EXECINFO_BACKTRACE) && (defined(__APPLE__) || defined(__linux__) || defined(__GLIBC__) || defined(__FreeBSD__))
#  include <execinfo.h>
#  define BU_HAVE_EXECINFO_BACKTRACE 1
#endif
#include "bsocket.h"
#include "bio.h"

/* common headers */
#include "bu/app.h"
#include "bu/debug.h"
#include "bu/interrupt.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "bu/str.h"
#include "bu/time.h"

/* strict c99 doesn't declare kill() (but POSIX does) */
#if !defined(HAVE_DECL_KILL) && !defined(kill)
extern int kill(pid_t, int);
#endif

#if !defined(HAVE_DECL_WAIT) && !defined(wait) && !defined(_WINSOCKAPI_)
extern pid_t wait(int *);
#endif

/* fileno() may be a macro (e.g., Windows) or may not even be declared
 * when compiling strict, but declare it as needed
 */
#ifndef HAVE_DECL_FILENO
extern int fileno(FILE*);
#endif


/* local buffer so we don't have to worry as much about stack stomping */
static char buffer[BU_PAGE_SIZE] = {0};

static pid_t process = (pid_t)0;
static pid_t pid = (pid_t)0;
static pid_t pid2 = (pid_t)0;
static volatile sig_atomic_t backtrace_done = 0;

/* avoid stack variables for backtrace() */
static int input[2] = {0, 0};
static int output[2] = {0, 0};
static fd_set fdset;
static fd_set readset;
static struct timeval tv;
static int result;
static int position;
static int processing_bt;
static char c = 0;
static int warned;

/* no stack variables in bu_backtrace() */
static char path_gdb[MAXPATHLEN] = {0};
static char path_lldb[MAXPATHLEN] = {0};
static char debugger_args[3][MAXPATHLEN] = { {0}, {0}, {0} };
static const char *locate_debugger = NULL;
static int have_gdb = 0, have_lldb = 0;


#ifdef HAVE_WINDOWS_H
/* windows.h is pulled in above via bio.h; dbghelp.h needs it and supplies the
 * SYMBOL_INFO / IMAGEHLP_* definitions (dbghelp itself is loaded at run time,
 * never linked - see windows_backtrace). */
#  include <dbghelp.h>

/* DbgHelp entry points we resolve at run time (see windows_backtrace). */
typedef DWORD (WINAPI *pfn_SymSetOptions)(DWORD);
typedef DWORD (WINAPI *pfn_SymGetOptions)(VOID);
typedef BOOL  (WINAPI *pfn_SymInitialize)(HANDLE, PCSTR, BOOL);
typedef BOOL  (WINAPI *pfn_SymCleanup)(HANDLE);
typedef BOOL  (WINAPI *pfn_SymFromAddr)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef BOOL  (WINAPI *pfn_SymGetLineFromAddr64)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
typedef BOOL  (WINAPI *pfn_SymGetModuleInfo64)(HANDLE, DWORD64, PIMAGEHLP_MODULE64);

/* Windows backtrace.
 *
 * Two independent steps:
 *
 *   1. Capturing the frames needs no symbols and no DbgHelp at all - on x64
 *      the unwind tables live in each PE module, so CaptureStackBackTrace()
 *      always yields correct return addresses even in a fully stripped build.
 *
 *   2. Turning an address into name+file:line is a best-effort *upgrade* that
 *      needs DbgHelp plus symbol data (a PDB, or at least the module's export
 *      table).  When no symbols are available we still emit "module+offset",
 *      which a developer can resolve later against archived PDBs.
 */
static int
windows_backtrace(FILE *fp)
{
    HANDLE proc;
    void *frames[128];
    USHORT nframes, i;
    /* The union guarantees SYMBOL_INFO's alignment for the trailing name
     * storage (a bare char[] cast to SYMBOL_INFO* would not). */
    union {
	SYMBOL_INFO si;
	char buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    } symbuf;
    SYMBOL_INFO *sym = &symbuf.si;
    HMODULE hdbg = NULL;
    pfn_SymSetOptions p_SymSetOptions = NULL;
    pfn_SymGetOptions p_SymGetOptions = NULL;
    pfn_SymInitialize p_SymInitialize = NULL;
    pfn_SymCleanup p_SymCleanup = NULL;
    pfn_SymFromAddr p_SymFromAddr = NULL;
    pfn_SymGetLineFromAddr64 p_SymGetLineFromAddr64 = NULL;
    pfn_SymGetModuleInfo64 p_SymGetModuleInfo64 = NULL;
    int have_syms = 0;

    if (!fp)
	fp = stdout;

    proc = GetCurrentProcess();

    nframes = CaptureStackBackTrace(0, 128, frames, NULL);
    if (!nframes)
	return 0;

    /*
     * DbgHelp is loaded ON DEMAND (only when a backtrace is actually
     * requested, i.e. essentially only when crashing) and STRICTLY from the
     * Windows system directory.  dbghelp.dll / symsrv.dll are classic
     * DLL-hijack targets and the default search order checks the application
     * directory first; loading with LOAD_LIBRARY_SEARCH_SYSTEM32 avoids that.
     * We deliberately do NOT implicitly link dbghelp, so a normal run never
     * loads or depends on it.
     *
     * NOTE: the DbgHelp symbol engine is single-threaded; concurrent Sym*
     * calls are not safe.  This is intended as a crash-time diagnostic, so we
     * deliberately do not serialize with a lock - taking one here could
     * deadlock a crashing thread that already holds it.  Callers using it as a
     * general (non-crash) facility from multiple threads must serialize
     * externally. */
    hdbg = LoadLibraryExW(L"dbghelp.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hdbg) {
	p_SymSetOptions = (pfn_SymSetOptions)(void(*)(void))GetProcAddress(hdbg, "SymSetOptions");
	p_SymGetOptions = (pfn_SymGetOptions)(void(*)(void))GetProcAddress(hdbg, "SymGetOptions");
	p_SymInitialize = (pfn_SymInitialize)(void(*)(void))GetProcAddress(hdbg, "SymInitialize");
	p_SymCleanup = (pfn_SymCleanup)(void(*)(void))GetProcAddress(hdbg, "SymCleanup");
	p_SymFromAddr = (pfn_SymFromAddr)(void(*)(void))GetProcAddress(hdbg, "SymFromAddr");
	p_SymGetLineFromAddr64 = (pfn_SymGetLineFromAddr64)(void(*)(void))GetProcAddress(hdbg, "SymGetLineFromAddr64");
	p_SymGetModuleInfo64 = (pfn_SymGetModuleInfo64)(void(*)(void))GetProcAddress(hdbg, "SymGetModuleInfo64");
	if (p_SymInitialize && p_SymFromAddr) {
	    if (p_SymSetOptions && p_SymGetOptions)
		p_SymSetOptions(p_SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS
				| SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
	    /* TRUE => invade process, auto-loading available module symbols. */
	    if (p_SymInitialize(proc, NULL, TRUE))
		have_syms = 1;
	}
    }

    memset(&symbuf, 0, sizeof(symbuf));
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = MAX_SYM_NAME;

    for (i = 0; i < nframes; i++) {
	DWORD64 addr = (DWORD64)(uintptr_t)frames[i];
	int printed = 0;

	if (have_syms) {
	    DWORD64 disp = 0;
	    DWORD ldisp = 0;
	    IMAGEHLP_MODULE64 mod;
	    IMAGEHLP_LINE64 line;
	    const char *modname = "";

	    memset(&mod, 0, sizeof(mod));
	    mod.SizeOfStruct = sizeof(mod);
	    if (p_SymGetModuleInfo64 && p_SymGetModuleInfo64(proc, addr, &mod))
		modname = mod.ModuleName;

	    memset(&line, 0, sizeof(line));
	    line.SizeOfStruct = sizeof(line);

	    if (p_SymFromAddr(proc, addr, &disp, sym)) {
		if (p_SymGetLineFromAddr64 && p_SymGetLineFromAddr64(proc, addr, &ldisp, &line))
		    fprintf(fp, "%2u  %s!%s+0x%llx  (%s:%lu)\n", (unsigned)i, modname,
			    sym->Name, (unsigned long long)disp, line.FileName,
			    (unsigned long)line.LineNumber);
		else
		    fprintf(fp, "%2u  %s!%s+0x%llx\n", (unsigned)i, modname, sym->Name,
			    (unsigned long long)disp);
		printed = 1;
	    }
	}

	if (!printed) {
	    /* No symbols: emit "module+offset" using only the loader. */
	    HMODULE hmod = NULL;
	    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				   (LPCSTR)frames[i], &hmod) && hmod) {
		char path[MAX_PATH];
		const char *base = "";
		if (GetModuleFileNameA(hmod, path, sizeof(path))) {
		    char *s = strrchr(path, '\\');
		    base = s ? s + 1 : path;
		}
		fprintf(fp, "%2u  %s+0x%llx\n", (unsigned)i, base,
			(unsigned long long)(addr - (DWORD64)(uintptr_t)hmod));
	    } else {
		fprintf(fp, "%2u  0x%llx\n", (unsigned)i, (unsigned long long)addr);
	    }
	}
    }

    if (have_syms && p_SymCleanup)
	p_SymCleanup(proc);
    if (hdbg)
	FreeLibrary(hdbg);

    fflush(fp);
    return 1;
}
#endif /* HAVE_WINDOWS_H */

static int
execinfo_backtrace(FILE *fp)
{
#ifdef BU_HAVE_EXECINFO_BACKTRACE
    void *frames[64] = {0};
    int frame_cnt = 0;
    int fd = -1;

    if (!fp) {
	fp = stdout;
    }

    frame_cnt = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
    if (frame_cnt <= 1) {
	return 0;
    }

    fd = fileno(fp);
    if (fd >= 0) {
	backtrace_symbols_fd(frames + 1, frame_cnt - 1, fd);
	fflush(fp);
	return 1;
    }

    {
	char **symbols = backtrace_symbols(frames + 1, frame_cnt - 1);
	if (!symbols) {
	    return 0;
	}
	for (int i = 0; i < frame_cnt - 1; i++) {
	    fprintf(fp, "%s\n", symbols[i]);
	}
	free(symbols);
    }
    fflush(fp);
    return 1;
#else
    (void)fp;
    return 0;
#endif
}


/* SIGINT+SIGCHLD handler for backtrace() */
static void
backtrace_interrupt(int UNUSED(signum))
{
    backtrace_done = 1;
}


/* actual guts to bu_backtrace() used to invoke debugger and parse out
 * backtrace from the output.
 */
static void
debugger_backtrace(int processid, char args[][MAXPATHLEN], int fd)
{
    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("[BACKTRACE] Invoking debugger: %s %s %s\n\n", args[0], args[1], args[2]);
    }

    if (UNLIKELY((pipe(input) == -1) || (pipe(output) == -1))) {
	perror("unable to open pipe");
	fflush(stderr);
	/* can't call bu_bomb()/bu_exit(), recursive */
	return;
    }

    /* capture our sub process PID pre-fork */
    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("[BACKTRACE] debugger_backtrace() parent is %d\n", bu_pid());
    }

    pid2 = fork();
    if (pid2 == 0) {
	int ret1, ret2, ret3;

	close(0);
	ret1 = dup(input[0]); /* set the stdin to the in pipe */
	if (ret1 == -1)
	    perror("dup");

	close(1);
	ret2 = dup(output[1]); /* set the stdout to the out pipe */
	if (ret2 == -1)
	    perror("dup");

	close(2);
	ret3 = dup(output[1]); /* set the stderr to the out pipe */
	if (ret3 == -1)
	    perror("dup");

	/* invoke debugger */
	execl(args[0], /* binary to run */
	      args[0], /* name to report */
	      (args[1][0] != '\0') ? args[1] : NULL, /* unused */
	      (args[2][0] != '\0') ? args[2] : NULL, /* unused */
	      NULL);
	perror("exec failed");
	fflush(stderr);

	/* can't call bu_bomb()/bu_exit(), recursive */
	exit(1);
    } else if (pid2 == (pid_t) -1) {
	perror("unable to fork for debugger execl");
	fflush(stderr);
	/* can't call bu_bomb()/bu_exit(), recursive */
	exit(1);
    }

    /* getting a CHLD means our child is done. getting an INT means
     * our parent is done.  either way, that's our cue to stop.
     */
#ifdef SIGCHLD
    signal(SIGCHLD, backtrace_interrupt);
#endif
#ifdef SIGINT
    signal(SIGINT, backtrace_interrupt);
#endif

    FD_ZERO(&fdset);
    FD_SET(output[0], &fdset);

    /* give debugger process time to start up */
    bu_snooze(BU_SEC2USEC(1));

    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("[BACKTRACE] debugger_backtrace() sending debugger commands\n");
    }

    {
	char attach_msg[64] = {0};
	snprintf(attach_msg, sizeof(attach_msg), "attach %d\n", processid);

	if (have_gdb) {
	    if (write(input[1], "set confirm off\n", 16) != 16) {
		perror("write [set confirm off] failed");
	    } else if (write(input[1], "set backtrace past-main on\n", 27) != 27) {
		perror("write [set backtrace past-main on] failed");
	    } else if (write(input[1], attach_msg, strlen(attach_msg)) != (ssize_t)strlen(attach_msg)) {
		perror("write [attach {pid}] failed");
	    } else if (write(input[1], "bt full\n", 8) != 8) {
		perror("write [bt full] failed");
	    } else if (write(input[1], "thread apply all bt full\n", 25) != 25) {
		perror("write [thread apply all bt full] failed");
	    }
	} else if (have_lldb) {
	    if (write(input[1], "settings set prompt \"# \"\n", 25) != 25) {
		perror("write [settings set prompt \"\"]");
	    } else if (write(input[1], "settings set auto-confirm 1\n", 28) != 28) {
		perror("write [settings set auto-confirm 1]");
	    } else if (write(input[1], "settings set stop-disassembly-display never\n", 44) != 44) {
		perror("write [settings set stop-disassembly-display never]");
	    } else if (write(input[1], attach_msg, strlen(attach_msg)) != (ssize_t)strlen(attach_msg)) {
		perror("write [attach {pid}] failed");
	    } else if (write(input[1], "bt all\n", 7) != 7) {
		perror("write [bt all] failed");
	    } else if (write(input[1], "thread backtrace all\n", 21) != 21) {
		perror("write [thread backtrace all] failed");
	    }
	}

	/* quit unless we want to allow more to attach too */
	if (!UNLIKELY(bu_debug & BU_DEBUG_ATTACH)) {
	    if (write(input[1], "quit\n", 5) != 5) {
		perror("write [quit] failed");
	    }
	}
    }

    /* Output (for gdb) will contain everything up to the "Detaching
     * from process" statement to the quit command.
     */

    position = 0;
    processing_bt = 0;
    memset(buffer, 0, BU_PAGE_SIZE);

    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("[BACKTRACE] Reading debugger output\n");
    }

    /* get/print the trace */
    warned = 0;
    while (1) {
	readset = fdset;

	tv.tv_sec = 0;
	tv.tv_usec = 42;

	result = select(FD_SETSIZE, &readset, NULL, NULL, &tv);

/*	bu_log("[BACKTRACE] select result=%d fdisset=%d\n", result, FD_ISSET(output[0], &readset)); */

	if (result == -1) {
	    break;
	}

	if ((result > 0) && (FD_ISSET(output[0], &readset))) {
	    int ret = read(output[0], &c, 1);
	    if (ret) {
		switch (c) {
		    case '#':
		    case '*':
			/* once we find a # or * on the beginning of a
			 * line, begin keeping track of the output.
			 * the first #0 backtrace frame (i.e. that for
			 * the bu_backtrace() call) is not included in
			 * the output intentionally (because of the
			 * gdb prompt).  ignore most everything else.
			 */
			if (position == 0) {
			    processing_bt = 1;
			}
			break;
		    case '\n':
			/* once we get to the end of a line, decide
			 * whether to ignore or write to file.
			 */
			if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
			    bu_log("[BACKTRACE] Output: %s\n", buffer);
			}
			if (position+1 < BU_PAGE_SIZE) {
			    buffer[position++] = c;
			    buffer[position] = '\0';
			} else {
			    position++;
			}
			if (bu_strncmp(buffer, "No locals", 9) == 0) {
			    /* skip it */
			} else if (bu_strncmp(buffer, "No symbol table", 15) == 0) {
			    /* skip it */
			} else if (bu_strncmp(buffer, "Target 0:", 9) == 0) {
			    /* skip it */
			} else if (bu_strncmp(buffer, "# settings set", 14) == 0) {
			    /* skip it */
			} else if (bu_strncmp(buffer, "Detaching", 9) == 0) {
			    /* done processing backtrace output */
			    processing_bt = 0;
			} else if (processing_bt == 1) {
			    if ((size_t)write(fd, buffer, strlen(buffer)) != strlen(buffer)) {
				perror("error writing stack to file");
				break;
			    }
			    if (position > BU_PAGE_SIZE) {
				if (write(fd, " [TRIMMED]\n", 11) != 11) {
				    perror("error writing trim message to file");
				    break;
				}
			    }
			}
			position = 0;
			continue;
		    default:
			break;
		}
		if (position+1 < BU_PAGE_SIZE) {
		    buffer[position++] = c;
		    buffer[position] = '\0';
		} else {
		    if (UNLIKELY(!warned && (bu_debug & BU_DEBUG_ATTACH))) {
			bu_log("[BACKTRACE] Warning: output overflow\n");
			warned = 1;
		    }
		    position++;
		}
	    }

	    /* !!! bu_log("[BACKTRACE] read ret=%d\n", ret); */

	} else if (backtrace_done) {
	    break;
	}
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("[BACKTRACE] Done reading debugger output\n");
    }

    fflush(stdout);
    fflush(stderr);

    close(input[0]);
    close(input[1]);
    close(output[0]);
    close(output[1]);

    if (UNLIKELY(bu_debug & BU_DEBUG_ATTACH)) {
	bu_log("\nBacktrace complete.\nAttach debugger or interrupt to continue...\n");
	bu_snooze(BU_SEC2USEC(30));
    } else {

#  ifdef HAVE_KILL

#    ifdef SIGKILL
	/* kill the debugger forcibly */
	if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("[BACKTRACE] debugger_backtrace() sending KILL to debugger %d\n", pid2);
	}
	kill(pid2, SIGKILL);
	bu_snooze(BU_SEC2USEC(1));
#    endif

#    ifdef SIGCHLD
	/* preemptively send a SIGCHLD to parent */
	if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	    bu_log("[BACKTRACE] debugger_backtrace() sending CHLD to parent %d\n", getppid());
	}
	kill(getppid(), SIGCHLD);
#    endif
#    ifdef SIGINT
	/* for good measure */
	if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	    bu_log("[BACKTRACE] debugger_backtrace() sending INT to parent %d\n", getppid());
	}
	kill(getppid(), SIGINT);
#    endif

#  endif
	bu_snooze(BU_SEC2USEC(2));
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("[BACKTRACE] debugger_backtrace() waiting for debugger %d to exit\n", pid2);
    }
#ifndef _WINSOCKAPI_
    wait(NULL);
#endif

    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("[BACKTRACE] debugger_backtrace() complete\n");
    }

    exit(0);
}


int
bu_backtrace_app(FILE *fp, const char *argv0)
{
    if (!fp) {
	fp = stdout;
    }
    fflush(fp); /* sanity */

#ifdef HAVE_WINDOWS_H
    if (windows_backtrace(fp)) {
	return 1;
    }
#endif

    if (execinfo_backtrace(fp)) {
	return 1;
    }

    /* check if GNU debugger (gdb) exists */
    if ((locate_debugger = bu_which("gdb"))) {
	bu_strlcpy(path_gdb, locate_debugger, MAXPATHLEN);
	if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	    bu_log("[BACKTRACE] Found gdb in PATH: %s\n", locate_debugger);
	}
	have_gdb = 1;
    }

    /* check if LLVM debugger (lldb) exists */
    if ((locate_debugger = bu_which("lldb"))) {
	bu_strlcpy(path_lldb, locate_debugger, MAXPATHLEN);
	if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	    bu_log("[BACKTRACE] Found lldb in PATH: %s\n", locate_debugger);
	}
	have_lldb = 1;
    }
    locate_debugger = NULL; /* made a copy */

#ifdef __APPLE__
    /* gdb is defunct on Mac since the switch to clang */
    if (have_lldb && have_gdb)
	have_gdb = 0;
#endif

    if (have_gdb) {
	const char *gdb_path = NULL;
	bu_strlcpy(debugger_args[0], path_gdb, MAXPATHLEN);
	/* MUST give gdb path to binary, otherwise attach bug causes
	 * process kill on some platforms (e.g., FreeBSD9+AMD64)
	 */
	if (argv0) {
	    gdb_path = argv0;
	} else {
	    gdb_path = bu_dir(NULL, 0, BU_DIR_BIN, bu_getprogname(), BU_DIR_EXT, NULL);
	}
	bu_strlcpy(debugger_args[1], gdb_path, MAXPATHLEN);
    } else if (have_lldb) {
	bu_strlcpy(debugger_args[0], path_lldb, MAXPATHLEN);
    }

    /* if we don't have a debugger, don't proceed */
    if (debugger_args[0][0] == '\0') {
	if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	    bu_log("[BACKTRACE] A debugger was NOT found, no backtrace available\n");
	}
	return 0;
    }

#ifdef SIGCHLD
    signal(SIGCHLD, backtrace_interrupt);
#endif
#ifdef SIGINT
    signal(SIGINT, backtrace_interrupt);
#endif

    /* capture our main process PID pre-fork */
    process = bu_pid();
    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("[BACKTRACE] bu_backtrace() parent is %d\n", process);
    }

    /* fork so that trace symbols stop _here_ instead of in some libc
     * routine (e.g., in wait(2)).
     */
    pid = fork();
    if (pid == 0) {
	/* child */
	debugger_backtrace(process, debugger_args, fileno(fp));
	exit(0);
    } else if (pid == (pid_t) -1) {
	/* failure */
	perror("unable to fork for backtrace");
	return 0;
    }

    /* parent */
    fflush(fp);

    /* Could probably do something better than this to avoid hanging
     * indefinitely. Keeps the trace clean, though, and allows for a
     * debugger to be attached interactively if needed.
     */
    backtrace_done = 0;
    {
	int cnt = 0;
	int64_t timer = bu_gettime();
	while (!backtrace_done && ((bu_gettime() - timer) / 1000000.0 < 30.0 /* seconds */)) {
	    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
		bu_log("[BACKTRACE] bu_backtrace() waiting 1 second (of %d)\n", cnt);
	    }
	    bu_snooze(BU_SEC2USEC(1));
	    cnt++;
	}
#ifdef HAVE_KILL
	if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	    bu_log("[BACKTRACE] bu_backtrace() sending INT to child %d\n", pid);
	}
	kill(pid, SIGINT);
	bu_snooze(BU_SEC2USEC(1));
#endif

	if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	    bu_log("[BACKTRACE] bu_backtrace() waiting for child %d to exit\n", pid);
	}

#ifndef _WINSOCKAPI_
	wait(NULL);
#endif
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("[BACKTRACE] Done.\n");
    }

#ifdef SIGINT
    signal(SIGINT, SIG_DFL);
#endif
#ifdef SIGCHLD
    signal(SIGCHLD, SIG_DFL);
#endif

    fflush(fp);

    return 1;
}

int
bu_backtrace(FILE *fp)
{
    return bu_backtrace_app(fp, NULL);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
