/*              R E G R E S S _ R E M R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file regress/remrt/regress_remrt.cpp
 *
 * Regression test for remrt (network distributed ray-tracing).
 *
 * Two sub-tests are executed, both using the IPC local-worker transport
 * (libpkg ipc socketpair).  A ".remrtrc" file containing "host localhost
 * always local <db_dir>" is written before each invocation so that remrt
 * auto-spawns one rtsrv worker on this machine via add_host_local() — no
 * SSH, no TCP port allocation, and no manual rtsrv invocation are needed.
 *
 *   Sub-test 1 — IPC 512×512 benchmark render:
 *     Starts remrt in -M mode feeding the exact m35 benchmark view script.
 *     The assembled 512×512 .pix output is compared against bench/ref/m35.pix
 *     using a pixel-exact comparison to verify rendering correctness against
 *     the 1991 BRL-CAD benchmark reference image.
 *
 *   Sub-test 2 — IPC 64×64 quick render:
 *     Starts remrt with the default az/el view for a quick 64×64 render.
 *     Verifies that the output file is produced with the expected byte count.
 *
 * Both sub-tests start remrt with the benchmark flags (-B -H 0 -J 0) for
 * deterministic rendering identical to the standard BRL-CAD benchmark.
 *
 * Usage:
 *   regress_remrt --remrt PATH --m35g PATH --refpix PATH
 *
 * Exits 0 on full success, non-zero if either sub-test fails.
 */

#include "common.h"

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "bio.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bu/vls.h"


/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

/* Maximum seconds to wait for remrt to print its port and session token. */
static const int PORT_TOKEN_WAIT_SEC = 30;

/* Maximum seconds to wait for remrt to complete each sub-test. */
static const int REMRT_WAIT_SEC = 500;

/* The m35 benchmark view script (identical to bench/run.sh "bench m35" invocation). */
static const char M35_SCRIPT[] =
    "viewsize 6.787387985e+03;\n"
    "eye_pt 3.974533127e+03 1.503320754e+03 2.874633221e+03;\n"
    "viewrot"
    " -5.527838919e-01 8.332423558e-01 1.171090926e-02 0.000000000e+00"
    " -4.815587087e-01 -3.308784486e-01 8.115544728e-01 0.000000000e+00"
    "  6.800964482e-01 4.429747496e-01 5.841593895e-01 0.000000000e+00"
    "  0.000000000e+00 0.000000000e+00 0.000000000e+00 1.000000000e+00;\n"
    "start 0;\n"
    "end;\n";

/* --------------------------------------------------------------------------
 * State shared between the stderr reader thread and the main thread.
 * -------------------------------------------------------------------------- */

struct RemrtDetected {
    std::mutex mtx;
    std::condition_variable cv;
    int port = 0;
    std::string token;
    bool ready = false;   /* port AND token found */

    void reset() {
	port = 0;
	token.clear();
	ready = false;
    }
};

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/* Trim trailing whitespace (newline, CR, space, tab) from a string in-place. */
static void
rtrim(char *buf)
{
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' ||
		     buf[n-1] == ' '  || buf[n-1] == '\t'))
	buf[--n] = '\0';
}


/*
 * Thread function: drain a process's stderr pipe, parse lines to extract
 * the TCP port and session token printed by remrt, and signal the condition
 * variable once both have been found.
 *
 * Continues reading until EOF so the pipe never fills up.
 */
static void
read_remrt_stderr(struct bu_process *proc, RemrtDetected *det, std::string *log_out)
{
    /* Use raw fd + dup so that fclose() does not close the original fd
     * and cause a double-close in bu_process_wait_n().                 */
    int raw_fd = bu_process_fileno(proc, BU_PROCESS_STDERR);
    if (raw_fd < 0)
	return;

    int dup_fd = dup(raw_fd);
    if (dup_fd < 0)
	return;

    FILE *ferr = fdopen(dup_fd, "rb");
    if (!ferr) {
	close(dup_fd);
	return;
    }

    char buf[4096];
    while (bu_fgets(buf, sizeof(buf), ferr)) {
	if (log_out)
	    log_out->append(buf);

	/* "Listening at TCP port NNNNN" */
	const char *p = strstr(buf, "Listening at TCP port ");
	if (p) {
	    int port_val = atoi(p + strlen("Listening at TCP port "));
	    if (port_val > 0) {
		std::lock_guard<std::mutex> lk(det->mtx);
		det->port = port_val;
	    }
	}

	/* Fallback: "Assigned LIBPKG permport NNNNN" */
	if (!det->port) {
	    p = strstr(buf, "Assigned LIBPKG permport ");
	    if (p) {
		int port_val = atoi(p + strlen("Assigned LIBPKG permport "));
		if (port_val > 0) {
		    std::lock_guard<std::mutex> lk(det->mtx);
		    det->port = port_val;
		}
	    }
	}

	/* "Session token: XXXXXXXXXX..." */
	p = strstr(buf, "Session token: ");
	if (p) {
	    char tok[512];
	    bu_strlcpy(tok, p + strlen("Session token: "), sizeof(tok));
	    rtrim(tok);
	    if (tok[0] != '\0') {
		std::lock_guard<std::mutex> lk(det->mtx);
		det->token = tok;
	    }
	}

	/* Signal once both are available. */
	{
	    std::lock_guard<std::mutex> lk(det->mtx);
	    if (det->port && !det->token.empty() && !det->ready) {
		det->ready = true;
		det->cv.notify_all();
	    }
	}
    }

    fclose(ferr);   /* closes dup_fd only, not the original raw_fd */
}


/*
 * Thread function: drain stdout of a process (prevents pipe buffer full-block
 * when the child writes a lot of output but the parent does not read it).
 */
static void
drain_stdout(struct bu_process *proc)
{
    /* Use raw fd (not fdopen) to avoid double-close with bu_process_wait_n. */
    int fd = bu_process_fileno(proc, BU_PROCESS_STDOUT);
    if (fd < 0)
	return;
    char buf[4096];
    while (read(fd, buf, sizeof(buf)) > 0) {}
}


/*
 * Return the directory portion of a file path (everything up to and including
 * the last path separator, minus the trailing separator).  Result is written
 * into buf.  Returns buf.
 */
static char *
path_dirname(const char *path, char *buf, size_t bufsz)
{
    const char *last_sep = NULL;
    for (const char *p = path; *p; p++) {
	if (*p == '/' || *p == '\\')
	    last_sep = p;
    }
    if (!last_sep) {
	bu_strlcpy(buf, ".", bufsz);
    } else {
	size_t n = (size_t)(last_sep - path);
	if (n == 0) n = 1;   /* preserve leading "/" */
	if (n >= bufsz) n = bufsz - 1;
	memcpy(buf, path, n);
	buf[n] = '\0';
    }
    return buf;
}


/* --------------------------------------------------------------------------
 * Sub-test infrastructure
 * -------------------------------------------------------------------------- */

struct TestOptions {
    std::string remrt_exe;
    std::string m35g_path;
    std::string refpix_path;
};


/*
 * Write a ".remrtrc" that registers "localhost" as an HT_LOCAL host.
 *
 * remrt will auto-spawn an rtsrv worker on this machine via libpkg ipc
 * (socketpair) when it calls start_servers().  No SSH or manual rtsrv
 * launch is needed.
 *
 * Returns 0 on success, -1 on failure.
 */
static int
write_remrtrc_local(const char *db_dir)
{
    char cwd[MAXPATHLEN] = {0};
    bu_dir(cwd, sizeof(cwd), BU_DIR_CURR, NULL);

    struct bu_vls rcpath = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&rcpath, "%s/.remrtrc", cwd);

    FILE *fp = fopen(bu_vls_cstr(&rcpath), "w");
    if (!fp) {
	fprintf(stderr, "regress_remrt: cannot write %s: %s\n",
		bu_vls_cstr(&rcpath), strerror(errno));
	bu_vls_free(&rcpath);
	return -1;
    }
    /* "local" tells remrt to spawn rtsrv via pkg IPC (env var PKG_ADDR)
     * rather than SSH, and to send MSG_CD <db_dir> so rtsrv finds the
     * geometry.
     *
     * bu_argv_from_string() treats backslash as an escape character, so
     * Windows-style paths (e.g. D:\foo\bar) must use forward slashes here.
     * Windows accepts forward slashes in all path contexts (CreateFile,
     * chdir, fopen, etc.), so this is safe on all platforms.              */
    struct bu_vls db_dir_fwd = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&db_dir_fwd, "%s", db_dir);
    /* Replace every backslash with a forward slash */
    char *p = bu_vls_addr(&db_dir_fwd);
    for (; *p; p++) {
	if (*p == '\\') *p = '/';
    }
    fprintf(fp, "host localhost always local %s\n", bu_vls_cstr(&db_dir_fwd));
    fclose(fp);
    bu_vls_free(&rcpath);
    bu_vls_free(&db_dir_fwd);
    return 0;
}


/*
 * Remove the ".remrtrc" file written by write_remrtrc_local().
 */
static void
cleanup_remrtrc(void)
{
    char cwd[MAXPATHLEN] = {0};
    bu_dir(cwd, sizeof(cwd), BU_DIR_CURR, NULL);

    struct bu_vls rcpath = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&rcpath, "%s/.remrtrc", cwd);
    bu_file_delete(bu_vls_cstr(&rcpath));
    bu_vls_free(&rcpath);
}


/*
 * Run one IPC-mode remrt sub-test.
 *
 * A ".remrtrc" with "host localhost always local <db_dir>" is written
 * before launching remrt so that remrt auto-spawns one rtsrv worker on
 * this machine via add_host_local() (pkg IPC socketpair + PKG_ADDR
 * env var).  No TCP port allocation and no manual rtsrv invocation are
 * needed.
 *
 * Parameters
 *   opts        – paths to executables and geometry/reference files
 *   use_script  – if true: pass -M and pipe M35_SCRIPT to remrt's stdin
 *   image_size  – pixel width/height of the render (e.g. 512 or 64)
 *   out_pix     – base path for the output .pix file (remrt appends ".0")
 *   label       – short label printed in diagnostic messages
 *   remrt_log   – if non-NULL, receives the complete remrt stderr log
 *
 * Returns 0 on success, non-zero on failure.
 */
static int
run_ipc_subtest(const TestOptions &opts,
		bool use_script,
		int image_size,
		const std::string &out_pix,
		const char *label,
		std::string *remrt_log = NULL)
{
    fprintf(stderr, "\n=== remrt regression: %s ===\n", label);

    char db_dir[MAXPATHLEN];
    path_dirname(opts.m35g_path.c_str(), db_dir, sizeof(db_dir));

    if (write_remrtrc_local(db_dir) != 0) {
	fprintf(stderr, "regress_remrt [%s]: failed to set up .remrtrc\n", label);
	return 1;
    }

    bu_file_delete(out_pix.c_str());
    bu_file_delete((out_pix + ".0").c_str());

    /* Build remrt argv.  remrt auto-spawns its own rtsrv worker via the
     * "always local" .remrtrc entry; no worker args needed here.          */
    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%d", image_size);

    std::vector<const char *> remrt_argv;
    remrt_argv.push_back(opts.remrt_exe.c_str());
    remrt_argv.push_back("-B");
    if (use_script)
	remrt_argv.push_back("-M");
    remrt_argv.push_back("-s");  remrt_argv.push_back(size_str);
    remrt_argv.push_back("-H");  remrt_argv.push_back("0");
    remrt_argv.push_back("-J");  remrt_argv.push_back("0");
    remrt_argv.push_back("-o");  remrt_argv.push_back(out_pix.c_str());
    remrt_argv.push_back(opts.m35g_path.c_str());
    remrt_argv.push_back("all.g");
    remrt_argv.push_back(NULL);

    struct bu_process *remrt_proc = NULL;
    bu_process_create(&remrt_proc, remrt_argv.data(), BU_PROCESS_DEFAULT);
    if (!remrt_proc) {
	fprintf(stderr, "regress_remrt [%s]: failed to start remrt\n", label);
	cleanup_remrtrc();
	return 1;
    }

    /* Drain remrt I/O in background threads to avoid pipe-buffer deadlocks.
     * Also collect stderr for failure diagnostics and wait for the startup
     * confirmation (port + token printed by remrt) before proceeding.     */
    RemrtDetected det;
    std::string remrt_stderr_log;
    std::thread stdout_drain_thr(drain_stdout, remrt_proc);
    std::thread stderr_read_thr(read_remrt_stderr, remrt_proc,
				&det, &remrt_stderr_log);

    {
	std::unique_lock<std::mutex> lk(det.mtx);
	bool ok = det.cv.wait_for(lk,
				  std::chrono::seconds(PORT_TOKEN_WAIT_SEC),
				  [&det] { return det.ready; });
	if (!ok)
	    fprintf(stderr,
		    "regress_remrt [%s]: timeout waiting for remrt startup\n",
		    label);
    }

    /* If using -M, pipe the view script to remrt's stdin now that remrt
     * has started.  This unblocks eat_script() so do_work() can begin.   */
    if (use_script) {
	FILE *fin = bu_process_file_open(remrt_proc, BU_PROCESS_STDIN);
	if (fin) {
	    fputs(M35_SCRIPT, fin);
	    bu_process_file_close(remrt_proc, BU_PROCESS_STDIN);
	} else {
	    fprintf(stderr, "regress_remrt [%s]: cannot open remrt stdin\n",
		    label);
	}
    }

    /* Wait for remrt to complete the render and exit. */
    int remrt_status = bu_process_wait_n(&remrt_proc,
					 REMRT_WAIT_SEC * 1000000 /* us */);
    stdout_drain_thr.join();
    stderr_read_thr.join();
    if (remrt_log)
	*remrt_log = remrt_stderr_log;

    cleanup_remrtrc();

    if (remrt_status != 0) {
	fprintf(stderr, "regress_remrt [%s]: remrt exited with status %d\n"
		"  stderr:\n%s\n", label, remrt_status, remrt_stderr_log.c_str());
	bu_file_delete(out_pix.c_str());
	bu_file_delete((out_pix + ".0").c_str());
	return 1;
    }

    std::string actual_pix = out_pix + ".0";
    if (!bu_file_exists(actual_pix.c_str(), NULL)) {
	fprintf(stderr, "regress_remrt [%s]: output file not created: %s\n"
		"  stderr:\n%s\n", label, actual_pix.c_str(),
		remrt_stderr_log.c_str());
	return 1;
    }

    long expected_size = (long)image_size * image_size * 3;
    {
	FILE *fp = fopen(actual_pix.c_str(), "rb");
	if (!fp) {
	    fprintf(stderr, "regress_remrt [%s]: cannot open output %s\n",
		    label, actual_pix.c_str());
	    return 1;
	}
	fseek(fp, 0, SEEK_END);
	long actual = ftell(fp);
	fclose(fp);
	if (actual != expected_size) {
	    fprintf(stderr,
		    "regress_remrt [%s]: output size mismatch: "
		    "expected %ld bytes, got %ld bytes\n"
		    "  stderr:\n%s\n",
		    label, expected_size, actual, remrt_stderr_log.c_str());
	    bu_file_delete(out_pix.c_str());
	    bu_file_delete(actual_pix.c_str());
	    return 1;
	}
    }

    fprintf(stderr, "regress_remrt [%s]: output file OK (%ld bytes)\n",
	    label, expected_size);
    return 0;
}

/*
 * Compare two raw .pix (RGB) files byte by byte.  Returns:
 *   0   – files are pixel-exact (identical)
 *   >0  – one or more bytes differ
 *
 * A detailed summary is printed to stderr so the caller can see how many
 * pixels are affected and by how much.
 */
static int
compare_pix_tolerant(const std::string &file_a,
		     const std::string &file_b)
{
    FILE *fa = fopen(file_a.c_str(), "rb");
    FILE *fb = fopen(file_b.c_str(), "rb");
    if (!fa || !fb) {
	if (fa) fclose(fa);
	if (fb) fclose(fb);
	fprintf(stderr, "regress_remrt: cannot open files for comparison\n");
	return 1;
    }

    long bad_pixels  = 0;   /* pixels where any channel differs by >1 */
    long warn_pixels = 0;   /* pixels where any channel differs by 1 */
    long pixel_no    = 0;
    int width        = 512; /* assumed from context */

    struct diff_detail { long pix; int ra,ga,ba,rb,gb,bb; };
    std::vector<diff_detail> diffs;

    int ra, ga, ba, rb, gb, bb;
    while (true) {
	ra = fgetc(fa); ga = fgetc(fa); ba = fgetc(fa);
	rb = fgetc(fb); gb = fgetc(fb); bb = fgetc(fb);
	if (ra == EOF || rb == EOF)
	    break;
	++pixel_no;

	int dr = abs(ra - rb);
	int dg = abs(ga - gb);
	int db = abs(ba - bb);
	int maxd = dr > dg ? dr : dg;
	maxd = maxd > db ? maxd : db;

	if (maxd > 1)
	    ++bad_pixels;
	else if (maxd == 1) {
	    ++warn_pixels;
	    if ((int)diffs.size() < 30)
		diffs.push_back({pixel_no-1, ra,ga,ba,rb,gb,bb});
	}
    }

    fclose(fa);
    fclose(fb);

    if (bad_pixels > 0) {
	fprintf(stderr,
		"  %ld pixel(s) differ by >1 channel value (assembly error)\n",
		bad_pixels);
    }
    if (warn_pixels > 0) {
	fprintf(stderr,
		"  %ld pixel(s) differ by exactly 1 channel value"
		" (floating-point rounding difference)\n",
		warn_pixels);
	fprintf(stderr, "  First differing pixels (pix_no x y  a_rgb  b_rgb  delta):\n");
	for (auto &d : diffs) {
	    int x = (int)(d.pix % width);
	    int y = (int)(d.pix / width);
	    fprintf(stderr, "    pix=%ld (%d,%d)  a=(%d,%d,%d) b=(%d,%d,%d)  d=(%d,%d,%d)\n",
		    d.pix, x, y,
		    d.ra, d.ga, d.ba,
		    d.rb, d.gb, d.bb,
		    d.ra-d.rb, d.ga-d.gb, d.ba-d.bb);
	}
    }
    if (bad_pixels == 0 && warn_pixels == 0) {
	fprintf(stderr, "  pixel-exact match\n");
    }

    /* Any pixel difference is a failure — the fix to ph_matrix() (calling
     * grid_setup + view_2init with the correct view parameters after MSG_MATRIX
     * arrives) eliminates the ±1 blue-channel rounding difference that was
     * previously observed for ~19 shadow-edge pixels.  The reference image was
     * generated by standalone rt with identical flags so pixel-exact parity is
     * achievable. */
    return (bad_pixels > 0 || warn_pixels > 0) ? 1 : 0;
}


/* --------------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------------- */

static void
usage(const char *argv0)
{
    fprintf(stderr,
	    "Usage: %s --remrt PATH --m35g PATH --refpix PATH\n",
	    argv0);
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    TestOptions opts;

    for (int i = 1; i < argc - 1; i++) {
	if      (BU_STR_EQUAL(argv[i], "--remrt"))   opts.remrt_exe   = argv[++i];
	else if (BU_STR_EQUAL(argv[i], "--m35g"))    opts.m35g_path   = argv[++i];
	else if (BU_STR_EQUAL(argv[i], "--refpix"))  opts.refpix_path = argv[++i];
	/* Silently accept deprecated flags for backward compat with old
	 * cmake invocations that still pass --rtsrv.                       */
	else if (BU_STR_EQUAL(argv[i], "--rtsrv") ||
		 BU_STR_EQUAL(argv[i], "--pixcmp"))  ++i;
    }

    if (opts.remrt_exe.empty() ||
	opts.m35g_path.empty() ||
	opts.refpix_path.empty()) {
	usage(argv[0]);
	return 1;
    }

    if (!bu_file_exists(opts.remrt_exe.c_str(), NULL)) {
	fprintf(stderr, "regress_remrt: remrt not found: %s\n",
		opts.remrt_exe.c_str());
	return 1;
    }
    if (!bu_file_exists(opts.m35g_path.c_str(), NULL)) {
	fprintf(stderr, "regress_remrt: m35.g not found: %s\n",
		opts.m35g_path.c_str());
	return 1;
    }
    if (!bu_file_exists(opts.refpix_path.c_str(), NULL)) {
	fprintf(stderr, "regress_remrt: reference pix not found: %s\n",
		opts.refpix_path.c_str());
	return 1;
    }

    char cwd[MAXPATHLEN] = {0};
    bu_dir(cwd, sizeof(cwd), BU_DIR_CURR, NULL);

    int failures = 0;

    /* ------------------------------------------------------------------ */
    /* Sub-test 1: 512×512 IPC benchmark render, pixel comparison.         */
    /* Uses the exact m35 benchmark view and compares against the          */
    /* bench/ref/m35.pix reference image.                                  */
    /* ------------------------------------------------------------------ */
    std::string out_bench = std::string(cwd) + "/m35_remrt_ipc_bench.pix";
    std::string subtest1_log;
    int rc = run_ipc_subtest(opts,
			     /* use_script */ true,
			     /* image_size */ 512,
			     out_bench,
			     "IPC 512x512 benchmark",
			     &subtest1_log);
    if (rc != 0) {
	fprintf(stderr, "FAIL: sub-test 1 (IPC benchmark render)\n");
	failures++;
    } else {
	std::string actual_bench = out_bench + ".0";
	fprintf(stderr, "Comparing %s with %s ...\n",
		actual_bench.c_str(), opts.refpix_path.c_str());
	int cmp = compare_pix_tolerant(actual_bench, opts.refpix_path);
	bu_file_delete(actual_bench.c_str());
	if (cmp != 0) {
	    fprintf(stderr,
		    "FAIL: sub-test 1 pixel comparison:"
		    " output differs from %s\n",
		    opts.refpix_path.c_str());
	    if (!subtest1_log.empty())
		fprintf(stderr, "remrt stderr log:\n%s\n",
			subtest1_log.c_str());
	    failures++;
	} else {
	    fprintf(stderr,
		    "PASS: sub-test 1 (IPC benchmark render"
		    " matches reference)\n");
	}
    }
    bu_file_delete(out_bench.c_str());

    /* ------------------------------------------------------------------ */
    /* Sub-test 2: 64×64 IPC quick render, output size check only.        */
    /* ------------------------------------------------------------------ */
    std::string out_quick = std::string(cwd) + "/m35_remrt_ipc_quick.pix";
    rc = run_ipc_subtest(opts,
			 /* use_script */ false,
			 /* image_size */ 64,
			 out_quick,
			 "IPC 64x64 quick");
    if (rc != 0) {
	fprintf(stderr, "FAIL: sub-test 2 (IPC quick render)\n");
	failures++;
    } else {
	bu_file_delete((out_quick + ".0").c_str());
	fprintf(stderr, "PASS: sub-test 2 (IPC quick render)\n");
    }
    bu_file_delete(out_quick.c_str());

    if (failures == 0)
	fprintf(stderr, "\nAll remrt regression sub-tests PASSED.\n");
    else
	fprintf(stderr, "\n%d remrt regression sub-test(s) FAILED.\n", failures);

    return (failures == 0) ? 0 : 1;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
