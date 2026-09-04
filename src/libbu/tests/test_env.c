/*                      T E S T _ E N V . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2026 United States Government as represented by
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

#include <errno.h>
#include <string.h>

#include "bio.h"
#include "bresource.h"
#if (defined(__APPLE__) || defined(__FreeBSD__)) && defined(HAVE_SYS_SYSCTL_H)
#  include <sys/sysctl.h>
#endif

#include "bu.h"

static void
shrink_path(struct bu_vls *tp, const char *lp)
{
    if (!tp || !lp)
	return;
    bu_vls_sprintf(tp, "%s", lp);
#ifdef HAVE_WINDOWS_H
    char sp[MAXPATHLEN];
    char *llp = bu_strdup(lp);
    DWORD r = GetShortPathNameA(llp, sp, MAXPATHLEN);
    bu_log("r: %d  sp:%s llp: %s\n", r, sp, llp);
    if (r != 0 && r < MAXPATHLEN) {
	// Unless short path call failed, use sp
	bu_vls_sprintf(tp, "%s", sp);
    }
    bu_free(llp, "llp");
    bu_log("%s\n", bu_vls_cstr(tp));
#endif
}

static int
process_mem_tests(void)
{
#if defined(HAVE_SYS_RESOURCE_H) && (defined(__linux__) || \
    defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__))
    struct rlimit original;

    if (getrlimit(RLIMIT_AS, &original) != 0) {
	const int error = errno;
	bu_log("bu_mem process-limit test: getrlimit(RLIMIT_AS) failed: %s (%d)\n",
	       strerror(error), error);
	return -1;
    }

    struct rlimit limited = original;
    const rlim_t test_headroom = (rlim_t)512 * 1024 * 1024;
    rlim_t test_limit = test_headroom;
#if defined(__APPLE__) && defined(HAVE_MACH_MACH_H)
    struct task_basic_info task_memory;
    mach_msg_type_number_t task_count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO,
	    (task_info_t)&task_memory, &task_count) != KERN_SUCCESS) {
	bu_log("bu_mem process-limit test: task_info(TASK_BASIC_INFO) failed\n");
	return -2;
    }
    if ((rlim_t)task_memory.virtual_size > RLIM_INFINITY - test_headroom) {
	bu_log("bu_mem process-limit test: current virtual size is too large "
	       "to add test headroom\n");
	return -2;
    }
    /* XNU rejects an address-space limit below the process's current map. */
    test_limit = (rlim_t)task_memory.virtual_size + test_headroom;
#endif
    if (limited.rlim_max != RLIM_INFINITY &&
	    limited.rlim_max < test_limit)
	limited.rlim_cur = limited.rlim_max;
    else
	limited.rlim_cur = test_limit;

    if (setrlimit(RLIMIT_AS, &limited) != 0) {
	const int error = errno;
	bu_log("bu_mem process-limit test: setrlimit(RLIMIT_AS) failed: %s (%d) "
	       "(original cur=%llu max=%llu, requested cur=%llu max=%llu)\n",
	       strerror(error), error,
	       (unsigned long long)original.rlim_cur,
	       (unsigned long long)original.rlim_max,
	       (unsigned long long)limited.rlim_cur,
	       (unsigned long long)limited.rlim_max);
	return -3;
    }

    const ssize_t available = bu_mem(BU_MEM_PROCESS_AVAIL, NULL);
    const int restore_result = setrlimit(RLIMIT_AS, &original);
    if (restore_result != 0) {
	const int error = errno;
	bu_log("bu_mem process-limit test: restoring RLIMIT_AS failed: %s (%d) "
	       "(test cur=%llu max=%llu, original cur=%llu max=%llu)\n",
	       strerror(error), error,
	       (unsigned long long)limited.rlim_cur,
	       (unsigned long long)limited.rlim_max,
	       (unsigned long long)original.rlim_cur,
	       (unsigned long long)original.rlim_max);
	return -4;
	}
    if (available < 0 || (rlim_t)available > limited.rlim_cur) {
	bu_log("bu_mem process-limit test: BU_MEM_PROCESS_AVAIL returned %zd; "
	       "expected a non-negative value no greater than %llu "
	       "(original cur=%llu max=%llu, test cur=%llu max=%llu)\n",
	       available,
	       (unsigned long long)limited.rlim_cur,
	       (unsigned long long)original.rlim_cur,
	       (unsigned long long)original.rlim_max,
	       (unsigned long long)limited.rlim_cur,
	       (unsigned long long)limited.rlim_max);
	return -5;
    }
#else
    if (bu_mem(BU_MEM_PROCESS_AVAIL, NULL) >= 0)
	bu_log("MEM process limit is available through a native API\n");
#endif
    return 0;
}


static int
process_resident_tests(void)
{
    const ssize_t before = bu_mem(BU_MEM_PROCESS_RESIDENT, NULL);
    if (before < 0) {
	bu_log("MEM resident size is not available through a native API\n");
	return 0;
    }

    /* Exceed normal allocator slack and RSS accounting granularity without
     * imposing a substantial memory requirement on constrained test hosts. */
    const size_t test_allocation_mebibytes = 16u;
    const size_t bytes_per_mebibyte = 1024u * 1024u;
    const size_t allocation_size = test_allocation_mebibytes *
	bytes_per_mebibyte;
    volatile unsigned char *allocation = (volatile unsigned char *)
	bu_malloc(allocation_size, "resident memory test allocation");
    ssize_t page_size = bu_mem(BU_MEM_PAGE_SIZE, NULL);
    if (page_size <= 0)
	page_size = BU_PAGE_SIZE;
    for (size_t offset = 0; offset < allocation_size;
	    offset += (size_t)page_size)
	allocation[offset] = (unsigned char)(offset / (size_t)page_size);
    allocation[allocation_size - 1] = 1;

    size_t resident_output = 0;
    const ssize_t after = bu_mem(BU_MEM_PROCESS_RESIDENT,
	&resident_output);
    bu_free((void *)allocation, "resident memory test allocation");
    if (after < 0 || resident_output != (size_t)after) {
	bu_log("bu_mem resident test: query returned %zd and wrote %zu\n",
	    after, resident_output);
	return -1;
    }
    if (after <= before) {
	bu_log("bu_mem resident test: resident size did not increase after "
	    "touching %zu bytes (before=%zd, after=%zd)\n",
	    allocation_size, before, after);
	return -2;
    }

    bu_log("MEM resident size increased from %zd to %zd after touching "
	"%zu bytes\n", before, after, allocation_size);
    return 0;
}


static int
mem_test_failure(const char *query, ssize_t result, const size_t *output,
                 int return_code)
{
    if (output) {
	bu_log("bu_mem test failure (%d): %s returned %zd and wrote %zu\n",
	       return_code, query, result, *output);
    } else {
	bu_log("bu_mem test failure (%d): %s returned %zd\n",
	       return_code, query, result);
    }
    return return_code;
}


static int
platform_mem_tests(ssize_t all_mem, ssize_t avail_mem, ssize_t page_mem)
{
#if defined(__APPLE__) && defined(HAVE_SYS_SYSCTL_H)
    uint64_t native_total = 0;
    size_t value_size = sizeof(native_total);
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    if (sysctl(mib, 2, &native_total, &value_size, NULL, 0) != 0 ||
	    value_size != sizeof(native_total)) {
	bu_log("bu_mem platform test: unable to query HW_MEMSIZE\n");
	return -1;
    }
    if (native_total != (uint64_t)all_mem) {
	bu_log("bu_mem platform test: BU_MEM_ALL returned %zd; HW_MEMSIZE "
	    "reported %llu\n", all_mem, (unsigned long long)native_total);
	return -2;
    }
#elif defined(__FreeBSD__) && defined(HAVE_SYS_SYSCTL_H)
    uint64_t native_total = 0;
    size_t value_size = sizeof(native_total);
    if (sysctlbyname("hw.physmem", &native_total, &value_size, NULL, 0) != 0 ||
	    (value_size != sizeof(uint32_t) && value_size != sizeof(uint64_t))) {
	bu_log("bu_mem platform test: unable to query hw.physmem\n");
	return -1;
    }
    if (native_total != (uint64_t)all_mem) {
	bu_log("bu_mem platform test: BU_MEM_ALL returned %zd; hw.physmem "
	    "reported %llu\n", all_mem, (unsigned long long)native_total);
	return -2;
    }

    uint64_t reclaimable_pages = 0;
    const char *const page_names[] = {
	"vm.stats.vm.v_free_count",
	"vm.stats.vm.v_inactive_count",
	"vm.stats.vm.v_cache_count"
    };
    for (size_t i = 0; i < sizeof(page_names) / sizeof(page_names[0]); i++) {
	uint32_t pages = 0;
	value_size = sizeof(pages);
	if (sysctlbyname(page_names[i], &pages, &value_size, NULL, 0) != 0 ||
		value_size != sizeof(pages)) {
	    if (i == 0)
		return -3;
	    continue;
	}
	reclaimable_pages += pages;
    }
    /* VM counters may change between the bu_mem and direct sysctl queries.
     * A one-percent allowance is ample for that race while still detecting
     * the former free-pages-only implementation. */
    uint64_t native_available = reclaimable_pages * (uint64_t)page_mem;
    uint64_t allowance = native_total / 100u;
    uint64_t available_difference = ((uint64_t)avail_mem > native_available) ?
	(uint64_t)avail_mem - native_available :
	native_available - (uint64_t)avail_mem;
    if (available_difference > allowance) {
	bu_log("bu_mem platform test: BU_MEM_AVAIL returned %zd; FreeBSD "
	    "reclaimable queues reported %llu\n", avail_mem,
	    (unsigned long long)native_available);
	return -4;
    }
#else
    (void)all_mem;
    (void)avail_mem;
    (void)page_mem;
#endif
    return 0;
}


static int
editor_tests(void)
{
    const char *e = NULL;
    struct bu_ptbl eopts = BU_PTBL_INIT_ZERO;

    // Unset the EDITOR variable
    bu_setenv("EDITOR", "", 1);

    // First, check that we can find *something* - will select GUI
    // editor first, but should fall back on console editors if
    // GUI not found.
    e = bu_editor(&eopts, 0, 0, NULL);
    if (!e) {
	bu_log("Failed to identify default editor.\n");
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("Default editor: %s\n", e);

    // Some environments may not have a graphical editor, but we should
    // *always* be able to find a console editor
    e = bu_editor(&eopts, 1, 0, NULL);
    if (!e) {
	bu_log("Failed to identify default console editor.\n");
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("Default console editor: %s\n", e);

    // Exercise the code path for GUI only, even though NULL is technically OK.
    e = bu_editor(&eopts, 2, 0, NULL);
    if (e)
	bu_log("Default GUI editor: %s\n", e);

    // The only executable we can guarantee at this point is bu_test
    // itself, and its location may differ depending on which platform
    // we built on.  Find it.
    char btest_path[MAXPATHLEN] = {'\0'};
    const char *btest_exec = bu_getprogname();
    bu_strlcpy(btest_path, btest_exec, MAXPATHLEN);
    if (!bu_file_exists(btest_path, NULL))
	bu_dir(btest_path, MAXPATHLEN, btest_exec, BU_DIR_EXT, NULL);
    if (!bu_file_exists(btest_path, NULL))
	bu_dir(btest_path, MAXPATHLEN, BU_DIR_BIN, btest_exec, NULL);
    if (!bu_file_exists(btest_path, NULL))
	bu_dir(btest_path, MAXPATHLEN, BU_DIR_BIN, btest_exec, BU_DIR_EXT, NULL);
    if (!bu_file_exists(btest_path, NULL))
	bu_dir(btest_path, MAXPATHLEN, BU_DIR_BIN, "..", "src", "libbu", "tests", btest_exec, NULL);
    if (!bu_file_exists(btest_path, NULL))
	bu_dir(btest_path, MAXPATHLEN, BU_DIR_BIN, "..", "src", "libbu", "tests", btest_exec, BU_DIR_EXT, NULL);
    if (!bu_file_exists(btest_path, NULL)) {
	bu_log("Failed to locate bu_test executable\n");
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("bu_test executable: %s\n", btest_path);

    // Set the EDITOR variable and exercise the EDITOR code path
    bu_setenv("EDITOR", btest_path, 1);

    // bu_editor outputs may be processed to produce more compact paths on on
    // some platforms - we'll need to replicate that with our test paths to be
    // able to do correct comparisons.
    struct bu_vls check_path = BU_VLS_INIT_ZERO;
    shrink_path(&check_path, getenv("EDITOR"));

    e = bu_editor(&eopts, 0, 0, NULL);
    if (!e) {
	bu_log("EDITOR value %s did not produce an editor path\n", getenv("EDITOR"));
	bu_vls_free(&check_path);
	bu_ptbl_free(&eopts);
	return -1;
    }
    if (!(BU_STR_EQUAL(e, bu_vls_cstr(&check_path)))) {
	bu_log("Failed to return EDITOR value %s with bu_editor\n", bu_vls_cstr(&check_path));
	bu_vls_free(&check_path);
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("EDITOR value returned with bu_editor: %s\n", e);


    // We want to make sure the editor option setting is working, but to do so we
    // need to trigger a case we know should produce an option - most cases will not.
    // To make this work, we need a valid path on the filesystem with the "emacs"
    // name, and since there's no guarantee the user has emacs installed we make
    // a file in the same location as bu_test to use as a target.  We then use EDITOR
    // to look for that file specifically.
    struct bu_vls bp = BU_VLS_INIT_ZERO;
    if (bu_path_component(&bp , btest_path, BU_PATH_DIRNAME)) {
	char epath[MAXPATHLEN] = {'\0'};
	bu_dir(epath, MAXPATHLEN, bu_vls_cstr(&bp), "emacs", NULL);
	FILE *fp = fopen(epath, "w");
	fprintf(fp, "BRL-CAD");
	fclose(fp);
	bu_setenv("EDITOR", epath, 1);
	e = bu_editor(&eopts, 1, 0, NULL);
	bu_file_delete(epath);
	if (!e) {
	    bu_log("EDITOR value %s did not produce an editor path\n", getenv("EDITOR"));
	    bu_vls_free(&check_path);
	    bu_ptbl_free(&eopts);
	    bu_vls_free(&bp);
	    return -1;
	}
	if (!BU_PTBL_LEN(&eopts)) {
	    bu_log("Expected editor option, but no option returned\n");
	    bu_vls_free(&check_path);
	    bu_ptbl_free(&eopts);
	    bu_vls_free(&bp);
	    return -1;
	}
	const char *emacs_opt = (const char *)BU_PTBL_GET(&eopts, 0);
	if (!(BU_STR_EQUAL(emacs_opt, "-nw"))) {
	    bu_log("Failed to return EDITOR value %s with bu_editor\n", bu_vls_cstr(&check_path));
	    bu_vls_free(&check_path);
	    bu_ptbl_free(&eopts);
	    bu_vls_free(&bp);
	    return -1;
	}
	bu_log("Console mode lookup for %s returned with editor option: %s\n", e, emacs_opt);
    }
    bu_vls_free(&bp);


    // Unset the EDITOR env var and prepare a list of "editors" to provide.
    bu_setenv("EDITOR", "", 1);
    char *de = bu_strdup(bu_editor(&eopts, 0, 0, NULL));
    const char *elist[4] = {NULL};
    elist[0] = "non-existent-editor1";
    elist[1] = btest_path;
    elist[2] = de;

    // Make sure the list returns the btest_path entry
    e = bu_editor(&eopts, 0, 3, elist);
    shrink_path(&check_path, elist[1]);
    if (!BU_STR_EQUAL(e, bu_vls_cstr(&check_path))) {
	bu_log("Failed to return list entry %s\n", elist[1]);
	bu_free(de, "default editor");
	bu_vls_free(&check_path);
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("Second list entry returned: %s\n", elist[1]);

    // Make sure a non-NULL terminated list doesn't continue on to search defaults
    elist[0] = "non-existent-editor1";
    elist[1] = "non-existent-editor2";
    elist[2] = "non-existent-editor3";
    e = bu_editor(&eopts, 0, 4, elist);
    if (e) {
	bu_log("Failed to stop after checking user supplied entries\n");
	bu_free(de, "default editor");
	bu_vls_free(&check_path);
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("As requested, list check skipped libbu internal testing after all entries failed.\n");

    // Check "fall back on internal libbu search" mode
    e = bu_editor(&eopts, 0, 3, elist);
    if (!BU_STR_EQUAL(e, de)) {
	bu_log("After unsuccessful list, failed to fall back to libbu's internal list\n");
	bu_free(de, "default editor");
	bu_vls_free(&check_path);
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("As requested, fallback internal libbu check succeeded after list check failed: %s\n", e);

    bu_free(de, "default editor");
    bu_vls_free(&check_path);
    bu_ptbl_free(&eopts);
    return 0;
}

int
main(int ac, char *av[])
{
    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(av[0]);

    if (ac > 1 && BU_STR_EQUAL(av[1], "-e"))
	return editor_tests();
    if (ac > 1 && BU_STR_EQUAL(av[1], "-m"))
	return process_mem_tests();
    if (ac > 1 && BU_STR_EQUAL(av[1], "-r"))
	return process_resident_tests();

    if (bu_mem(-1, NULL) >= 0 ||
	    bu_mem(BU_MEM_PROCESS_RESIDENT + 1, NULL) >= 0)
	return mem_test_failure("invalid query", 0, NULL, -1);

    ssize_t all_mem = bu_mem(BU_MEM_ALL, NULL);
    if (all_mem <= 0)
	return mem_test_failure("BU_MEM_ALL", all_mem, NULL, -2);
    ssize_t avail_mem = bu_mem(BU_MEM_AVAIL, NULL);
    if (avail_mem < 0)
	return mem_test_failure("BU_MEM_AVAIL", avail_mem, NULL, -3);
    ssize_t page_mem = bu_mem(BU_MEM_PAGE_SIZE, NULL);
    if (page_mem <= 0)
	return mem_test_failure("BU_MEM_PAGE_SIZE", page_mem, NULL, -4);
    if (avail_mem > all_mem)
	return mem_test_failure("BU_MEM_AVAIL exceeds BU_MEM_ALL", avail_mem,
	    NULL, -5);
    if (platform_mem_tests(all_mem, avail_mem, page_mem) != 0)
	return -6;
    ssize_t process_mem = bu_mem(BU_MEM_PROCESS_AVAIL, NULL);
    ssize_t resident_mem = bu_mem(BU_MEM_PROCESS_RESIDENT, NULL);

    /* Make sure the output pointer matches the return value from that call.
     * The values reported by the operating system can change between calls. */
    size_t all_mem2 = 0;
    size_t avail_mem2 = 0;
    size_t page_mem2 = 0;
    size_t process_mem2 = 0;
    size_t resident_mem2 = 0;

    const ssize_t all_mem_ret = bu_mem(BU_MEM_ALL, &all_mem2);
    if (all_mem_ret < 0 || all_mem2 != (size_t)all_mem_ret)
	return mem_test_failure("BU_MEM_ALL with output", all_mem_ret,
	                       &all_mem2, -7);

    const ssize_t avail_mem_ret = bu_mem(BU_MEM_AVAIL, &avail_mem2);
    if (avail_mem_ret < 0 || avail_mem2 != (size_t)avail_mem_ret)
	return mem_test_failure("BU_MEM_AVAIL with output", avail_mem_ret,
	                       &avail_mem2, -8);

    const ssize_t page_mem_ret = bu_mem(BU_MEM_PAGE_SIZE, &page_mem2);
    if (page_mem_ret < 0 || page_mem2 != (size_t)page_mem_ret)
	return mem_test_failure("BU_MEM_PAGE_SIZE with output", page_mem_ret,
	                       &page_mem2, -9);

    const ssize_t process_mem_ret = bu_mem(BU_MEM_PROCESS_AVAIL,
	&process_mem2);
    if (process_mem_ret >= 0 && process_mem2 != (size_t)process_mem_ret)
	return mem_test_failure("BU_MEM_PROCESS_AVAIL with output",
	                       process_mem_ret, &process_mem2, -10);

    const ssize_t resident_mem_ret = bu_mem(BU_MEM_PROCESS_RESIDENT,
	&resident_mem2);
    if (resident_mem_ret >= 0 &&
	    resident_mem2 != (size_t)resident_mem_ret)
	return mem_test_failure("BU_MEM_PROCESS_RESIDENT with output",
	                       resident_mem_ret, &resident_mem2, -11);
    if (resident_mem >= 0 && resident_mem == 0)
	return mem_test_failure("BU_MEM_PROCESS_RESIDENT", resident_mem,
	    NULL, -12);

    char all_buf[6] = {'\0'};
    char avail_buf[6] = {'\0'};
    char p_buf[6] = {'\0'};

    bu_humanize_number(all_buf, 5, all_mem, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);
    bu_humanize_number(avail_buf, 5, avail_mem, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);
    bu_humanize_number(p_buf, 5, page_mem, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);

    bu_log("MEM report: all: %s(%zd) avail: %s(%zd) page_size: %s(%zd)\n",
	   all_buf, all_mem,
	   avail_buf, avail_mem,
	   p_buf, page_mem);
    if (process_mem >= 0)
	bu_log("MEM process address-space available: %zd\n", process_mem);
    else
	bu_log("MEM process address-space limit: unsupported or unlimited\n");
    if (resident_mem >= 0)
	bu_log("MEM process resident: %zd\n", resident_mem);
    else
	bu_log("MEM process resident: unsupported\n");

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
