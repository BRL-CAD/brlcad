/*                     B E N C H M A R K . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2021 United States Government as represented by
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
/** @file benchmark.c
 *
 * This is the BRL-CAD Benchmark.
 *
 * The benchmark suite will test the performance of a system by
 * iteratively rendering several well-known datasets into 512x512
 * images where performance metrics are documented and fairly well
 * understood.  The local machine's performance is compared to the
 * base system (called VGR) and a numeric "VGR" multiplier of
 * performance is computed.  This number is a simplified metric from
 * which one may qualitatively compare cpu and cache performance,
 * versions of BRL-CAD, and different compiler characteristics.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
/* TODO - fall back on https://github.com/sandsmark/winuname for Windows? */
#ifdef HAVE_SYS_UTSNAME_H
#  include <sys/utsname.h>
#endif

#include "bio.h"

#include "bu.h"


/* TODO: !!! probably will need variants of these */
int bu_exec(const char *program, void *data, struct bu_vls *out, struct bu_vls *err) {if (!program || !out || !err || !data) return 1; return 0;}
int bu_grep(const char *pattern, const char *input, int(*callback)(const char *line, void *data), void *data) {if (!pattern || !input || !data) return 1; if (!callback) bu_log("TODO: need default callback!!!\n"); return 0;}
int bu_file_glob(const char *pattern, char **paths) {if (!pattern || !paths) return 1; return 0;}
char *format_elapsed(double elapsed) {if (elapsed > 0) return "0 1 2"; return "-0 -1 -2";}
int perf(const struct bu_vls *vp, const char *items, const char *args[]) {if (!vp || !items || !args) return -1; return 0;}
double calculate_vgr(const struct bu_vls *summary) {if (!summary) return -0.0; return 0.0;}
extern int bench(const char *cmd);


/**
 * this routine writes the provided argument(s) to a log file as well
 * as echoing them to stdout if we're not in quiet mode.
 */
static void
record(const char *fmt, ...)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    va_list ap;

    const char *LOGFILE = getenv("LOGFILE");
    const char *QUIET = getenv("QUIET");

    if (!fmt) {
	return;
    }

    va_start(ap, fmt);
    bu_vls_vprintf(&str, fmt, ap);
    va_end(ap);

    if (!BU_STR_EQUAL(LOGFILE, "")) {
	FILE *fp = fopen(LOGFILE, "a");
	fprintf(fp, "%s", bu_vls_addr(&str));
	fclose(fp);
    }
    if (bu_str_false(QUIET)) {
	bu_log("%s", bu_vls_addr(&str));
    }

    bu_vls_free(&str);
}


static void
display(const char *fmt, ...)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    va_list ap;

    if (!fmt) {
	return;
    }

    va_start(ap, fmt);
    bu_vls_vprintf(&str, fmt, ap);
    va_end(ap);
    bu_log("%s", bu_vls_cstr(&str));

    bu_vls_free(&str);
}


static void
sink(const char *fmt, ...)
{
    if (!fmt) {
	return;
    }
}


/************************
 * search for resources *
 ************************/
typedef enum {
    executable=0x1,
    file=0x2,
    directory=0x4,
    script=0x8
} look_for_type_t;

#define TYPE2STR(_t) \
    ((_t) == executable) ? "executable" : \
    ((_t) == file) ? "file" : \
    ((_t) == directory) ? "directory" : \
    ((_t) == script) ? "script" : \
    "unknown"

static void
look_for(void (*verbose)(const char *, ...), look_for_type_t type, const char *label, const char *var, const char*paths[])
{
    char *val;

    /* utility function to search for a certain filesystem object in a
     * list of paths.
     */

    if (!var || !paths) {
	return;
    }
    if (!verbose) {
	verbose = sink;
    }

    verbose("Looking for %s\n", label);

    /* get the value of the variable */
    val = getenv(var);

    if (val && (strlen(val) > 0)) {
	if (label) {
	    verbose("...using %s from %s variable setting\n", val, var);
	}
    } else {
	while (paths && *paths) {
	    int failed=0;

	    switch (type) {
		default:
		case directory:
		    /* should work without read bit */
		    if (!bu_file_directory(*paths)) {
			failed=1;
		    }
		    /* fall through */
		case executable:
		case script:
		    if (!bu_file_executable(*paths)) {
			failed=1;
		    }
		    break;
		case file:
		    if (!bu_file_exists(*paths, NULL)) {
			failed=1;
		    }
		    break;
	    }

	    if (!failed) {
		verbose("...found %s %s\n", TYPE2STR(type), *paths);
		bu_setenv(var, *paths, 1);
		break;
	    }

	    paths++;
	}
    }
}


static void
set_if_unset(void (*echo)(const char *, ...), void (*verbose)(const char *, ...), const char *var, const char *val)
{
    const char *setval = getenv(var);

    if (!var || !val) {
	return;
    }
    if (!echo) {
	echo = sink;
    }
    if (!verbose) {
	verbose = sink;
    }

    if (!setval || (strlen(setval) == 0)) {
	verbose("%s=\"%s\"\n", var, val);
	bu_setenv(var, val, 1);
    }

    setval = getenv(var);
    echo("Using [%s] for %s\n", setval, var);
}


/* callback to bu_grep() */
static int
stashline(const char *line, void *data)
{
    struct bu_vls *vls = (struct bu_vls *)data;

    BU_CK_VLS(vls);

    bu_vls_printf(vls, "%s\n", line);
    return 0;
}


int
main(int ac, char *av[])
{
    int arg;
    struct bu_vls rtargs = BU_VLS_INIT_ZERO;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    const char *RT = NULL;
    const char *DB = NULL;
    const char *PIX = NULL;
    const char *LOG = NULL;
    const char *CMP = NULL;
    const char *ELP = NULL;

    char root[MAXPATHLEN] = {0};
    char db[MAXPATHLEN] = {0};
    char pix[MAXPATHLEN] = {0};
    char thisp[MAXPATHLEN] = {0};
    char *argv[MAXPATHLEN] = {NULL};
    struct bu_vls vp = BU_VLS_INIT_ZERO;

    long MAXTIME = 0;
    long TIMEFRAME = 0;

    int64_t start = 0;
    int ret = 0;

    void (*verbose_echo)(const char *, ...) = sink;
    void (*echo)(const char *, ...) = record;

    bu_setprogname(av[0]);

    /* process the argument list for commands */
    for (arg=1; arg<ac; arg++) {
	if (BU_STR_EQUAL(av[arg], "clean")) {
	    bu_setenv("CLEAN", "1", 1);
	    continue;
	}
	if (BU_STR_EQUAL(av[arg], "clobber")) {
	    bu_setenv("CLEAN", "1", 1);
	    bu_setenv("CLOBBER", "1", 1);
	    continue;
	}
	if (BU_STR_EQUAL(av[arg], "h")) {
	    bu_setenv("HELP", "1", 1);
	    continue;
	}
	if (BU_STR_EQUAL(av[arg], "help")) {
	    bu_setenv("HELP", "1", 1);
	    continue;
	}
	if (bu_strncasecmp(av[arg], "instruct", 8)) {
	    bu_setenv("INSTRUCTIONS", "1", 1);
	    continue;
	}
	if (BU_STR_EQUAL(av[arg], "start")) {
	    continue;
	}
	if (BU_STR_EQUAL(av[arg], "quiet")) {
	    bu_setenv("QUIET", "1", 1);
	    continue;
	}
	if (BU_STR_EQUAL(av[arg], "verbose")) {
	    bu_setenv("VERBOSE", "1", 1);
	    continue;
	}
	if (BU_STR_EQUAL(av[arg], "run")) {
	    bu_setenv("RUN", "1", 1);
	    continue;
	}
	if (strchr(av[arg], '=')) {
	    /* TODO: !!! make sure this still works */
	    char *var = bu_strdup(av[arg]);
	    char *val = strtok(var, "=");
	    if (var && val) {
		bu_setenv(var, val, 1);
	    }
	    bu_free(var, "strdup av[arg]");
	    continue;
	}
	bu_log("WARNING: Passing unknown option [%s] to RT\n", av[arg]);
	bu_vls_printf(&rtargs, " %s", av[arg]);
    }


    /***
     * handle help before main processing
     ***/
    if (bu_str_true(getenv("HELP"))) {
	bu_log("Usage: %s [command(s)] [OPTION=value] [RT_OPTIONS]\n", av[0]);
	bu_log("\n");
	bu_log("Available commands:\n");
	bu_log("  clean\n");
	bu_log("  clobber\n");
	bu_log("  help          (this is what you are reading right now)\n");
	bu_log("  instructions\n");
	bu_log("  quiet\n");
	bu_log("  verbose\n");
	bu_log("  run           (this is probably what you want)\n");
	bu_log("\n");
	bu_log("Available options:\n");
	bu_log("  RT=/path/to/rt_binary (e.g., rt)\n");
	bu_log("  DB=/path/to/reference/geometry (e.g. ../db)\n");
	bu_log("  PIX=/path/to/reference/images (e.g., ../pix)\n");
	bu_log("  LOG=/path/to/reference/logs (e.g., ../pix)\n");
	bu_log("  CMP=/path/to/pixcmp_tool (e.g., pixcmp)\n");
	bu_log("  ELP=/path/to/time_tool (e.g., elapsed.sh)\n");
	bu_log("  TIMEFRAME=#seconds (default 32)\n");
	bu_log("  MAXTIME=#seconds (default 300)\n");
	bu_log("  DEVIATION=%%deviation (default 3)\n");
	bu_log("  AVERAGE=#frames (default 3)\n");
	bu_log("\n");
	bu_log("Available RT options:\n");
	bu_log("  -P# (e.g., -P1 to force single CPU)\n");
	bu_log("  See the 'rt' manpage for additional options\n");
	bu_log("\n");
	bu_log("The BRL-CAD Benchmark tests the overall performance of a system\n");
	bu_log("compared to a stable well-known baseline.  The Benchmark calculates a\n");
	bu_log("figure-of-merit for this system that may be directly compared to other\n");
	bu_log("Benchmark runs on other system configurations.\n");
	bu_log("\n");
	bu_log("BRL-CAD is a powerful cross-platform open source solid modeling system.\n");
	bu_log("For more information about BRL-CAD, see http://brlcad.org\n");
	bu_log("\n");
	bu_log("Run '%s instructions' or see the manpage for additional information.\n", av[0]);
	bu_exit(1, NULL);
    }


    /***
     * handle instructions before main processing
     ***/
    if (bu_str_true(getenv("INSTRUCTIONS"))) {
	bu_log("\n");
	bu_log("This program runs the BRL-CAD Benchmark.  The benchmark suite will\n");
	bu_log("test the performance of a system by iteratively rendering several\n");
	bu_log("well-known datasets into 512x512 images where performance metrics are\n");
	bu_log("documented and fairly well understood.  The local machine's\n");
	bu_log("performance is compared to the base system (called VGR) and a numeric\n");
	bu_log("\"VGR\" mulitplier of performance is computed.  This number is a\n");
	bu_log("simplified metric from which one may qualitatively compare cpu and\n");
	bu_log("cache performance, versions of BRL-CAD, and different compiler\n");
	bu_log("characteristics.\n");
	bu_log("\n");
	bu_log("The suite is intended to be run from a source distribution of BRL-CAD\n");
	bu_log("after the package has been compiled either directly or via a make\n");
	bu_log("build system target.  There are, however, several environment\n");
	bu_log("variables that will modify how the BRL-CAD benchmark behaves so that\n");
	bu_log("it may be run in a stand-alone environment:\n");
	bu_log("\n");
	bu_log("  RT - the rt binary (e.g. ../src/rt/rt or /usr/brlcad/bin/rt)\n");
	bu_log("  DB - the directory containing the reference geometry (e.g. ../db)\n");
	bu_log("  PIX - the directory containing the reference images (e.g. ../pix)\n");
	bu_log("  LOG - the directory containing the reference logs (e.g. ../pix)\n");
	bu_log("  CMP - the name of a pixcmp tool (e.g. ./pixcmp or cmp)\n");
	bu_log("  ELP - the name of an elapsed time tool (e.g. ../sh/elapsed.sh)\n");
	bu_log("  TIMEFRAME - the minimum number of seconds each trace needs to take\n");
	bu_log("  MAXTIME - the maximum number of seconds to spend on any test\n");
	bu_log("  DEVIATION - the minimum sufficient %% deviation from the average\n");
	bu_log("  AVERAGE - how many frames to average together\n");
	bu_log("  VERBOSE - turn on extra debug output for testing/development\n");
	bu_log("  QUIET - turn off all printing output (writes results to summary)\n");
	bu_log("  INSTRUCTIONS - display detailed instructions\n");
	bu_log("  RUN - start a benchmark analysis\n");
	bu_log("\n");
	bu_log("The TIMEFRAME, MAXTIME, DEVIATION, and AVERAGE options control how the\n");
	bu_log("benchmark will proceed including how long it should take.  Each\n");
	bu_log("individual benchmark run will consume at least a minimum TIMEFRAME of\n");
	bu_log("wallclock time so that the results can stabilize.  After consuming at\n");
	bu_log("least the minimum TIMEFRAME, additional frames may be computed until\n");
	bu_log("the standard deviation from the last AVERAGE count of frames is below\n");
	bu_log("the specified DEVIATION.  When a test is run and it completes in less\n");
	bu_log("than TIMEFRAME, the raytrace is restarted using double the number of\n");
	bu_log("rays from the previous run.  If the machine is fast enough, the\n");
	bu_log("benchmark may accelerate the number or rays being fired.  These\n");
	bu_log("additional rays are hypersampled but without any jitter, so it's\n");
	bu_log("effectively performing a multiplier amount of work over the initial\n");
	bu_log("frame.\n");
	bu_log("\n");
	bu_log("Plese send your BRL-CAD Benchmark results to the developers along with\n");
	bu_log("detailed system information to <devs@brlcad.org>.  Include at least:\n");
	bu_log("\n");
	bu_log("  0) Compiler name and version (e.g. gcc --version)\n");
	bu_log("  1) All generated log files (i.e. *.log* after benchmark completes)\n");
	bu_log("  2) Anything else you think might be relevant to performance\n");
	bu_log("\n");
	bu_log("The manual page has even more information and specific usage examples.\n");
	bu_log("Run 'brlman benchmark'.\n");
	bu_log("\n");
	bu_exit(0, NULL);
    }


    /***
     * handle clean/clobber before main processing
     ***/
    if (bu_str_true(getenv("CLEAN"))) {
	size_t i;
	size_t cnt;
	size_t num;
	char cwd[MAXPATHLEN] = {0};
	const char *tests[] = {"moss", "world", "star", "bldg391", "m35", "sphflake", NULL};
	char **paths = NULL;
	int printed = 0;

	bu_log("\n");
	if (bu_str_true(getenv("CLOBBER"))) {
	    bu_log("About to wipe out all benchmark images and log files in %s\n", bu_getcwd(cwd, MAXPATHLEN));
	    bu_log("Send SIGINT (type ^C) within 5 seconds to abort\n");
	    sleep(5);
	} else {
	    bu_log("Deleting most benchmark images and log files in %s\n", bu_getcwd(cwd, MAXPATHLEN));
	    bu_log("Running '%s clobber' will remove run logs.\n", av[0]);
	}
	bu_log("\n");

	for (i=0; tests[i]; i++) {
	    struct bu_vls pattern_str = BU_VLS_INIT_ZERO;
	    char *pattern[5] = {NULL, NULL, NULL, NULL, NULL};
	    size_t j;

	    /* rm -f $i.log $i.pix $i-[0-9]*.log $i-[0-9]*.pix */
	    bu_vls_printf(&pattern_str, "%s.log %s.pix %s-[0-9]*.log %s-[0-9]*.pix", tests[i], tests[i], tests[i], tests[i]);
	    bu_log("rm -f %s\n", bu_vls_cstr(&pattern_str));

	    j = bu_argv_from_string(pattern, 4, bu_vls_addr(&pattern_str));
	    while (j > 0) {
		num = bu_file_glob(pattern[j-1], paths);
		if (!num || !paths || !paths[0]) {
		    j--;
		    continue;
		}
		cnt = num;
		while (num > 0) {
		    bu_file_delete(paths[num-1]);
		    num--;
		}
		bu_argv_free(cnt, paths);
		paths = NULL;
		j--;
	    }
	    bu_vls_free(&pattern_str);
	}

	if (bu_str_true(getenv("CLOBBER"))) {
	    /* NEVER automatically delete the summary file, but go
	     * ahead with the rest.
	     */

	    num = bu_file_glob("benchmark-[0-9]*-run.log", paths);
	    cnt = num;
	    while (num > 0) {
		bu_log("rm -f %s\n", paths[num-1]);
		bu_file_delete(paths[num-1]);
		num--;
	    }
	    bu_argv_free(cnt, paths);
	    paths = NULL;
	}

	if (bu_file_exists("summary", NULL)) {
	    if (!printed) {
		bu_log("\nThe following files must be removed manually:\n");
		printed=1;
	    }
	    bu_log("summary ");
	}
	num = bu_file_glob("benchmark-[0-9]*-run.log", paths);
	cnt = num;
	while (num > 0) {
	    if (!printed) {
		bu_log("\nThe following files must be removed manually:\n");
		printed=1;
	    }
	    bu_log("%s ", paths[num-1]);
	    num--;
	}
	bu_argv_free(cnt, paths);
	paths = NULL;

	if (printed) {
	    bu_log("\n");
	}

	if (bu_str_true(getenv("CLOBBER"))) {
	    bu_log("\nBenchmark clobber complete.\n");
	} else {
	    bu_log("\nBenchmark clean complete.\n");
	}
	bu_exit(0, NULL);
    }


    /***
     * B E G I N
     ***/

    if (bu_str_false(getenv("RUN"))) {
	bu_exit(1, "Type '%s help' for usage.\n", av[0]);
    }

    /* where to write results */
    {
	int fd;
	const char *logfile;
	bu_vls_printf(&str, "run-%d-benchmark.log", bu_process_id());
	logfile = bu_vls_addr(&str);
	bu_setenv("LOGFILE", logfile, 1);
	fd = open(logfile, O_WRONLY|O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0 || !bu_file_writable(logfile)) {
	    if (!BU_STR_EQUAL(logfile, "/dev/null")) {
		bu_log("ERROR: Unable to log to %s\n", logfile);
	    }
	    /* FIXME: not valid logfile on windows, use 'nul' */
	    bu_setenv("LOGFILE", "/dev/null", 1);
	}
    }

    if (bu_str_true(getenv("QUIET"))) {
	if (bu_str_true(getenv("VERBOSE"))) {
	    bu_log("Verbose output quelled by quiet option.  Further output disabled.\n");
	}
    } else {
	if (bu_str_true(getenv("VERBOSE"))) {
	    verbose_echo=display;
	    bu_log("Verbose output enabled\n");
	}
    }

    {
	struct bu_vls v = BU_VLS_INIT_ZERO;
	char rfc2822[1024] = {0};
#ifdef HAVE_SYS_UTSNAME_H
	struct utsname n;
	time_t t = time(NULL);
	strftime(rfc2822, sizeof(rfc2822), "%a, %d %b %Y %H:%M:%S %Z", localtime(&t));

	if (uname(&n) == 0) {
	    bu_vls_printf(&v, "%s %s %s %s %s", n.sysname, n.nodename, n.release, n.version, n.machine);
	} else {
	    bu_vls_strcat(&v, "unknown");
	}
#endif

	echo("B R L - C A D   B E N C H M A R K\n");
	echo("=================================\n");
	echo("Running %s on %s\n", av[0], rfc2822);
	echo("Logging output to %s\n", getenv("LOGFILE"));
	echo("%s\n\n", bu_vls_addr(&v));
    }


    bu_dir(root, MAXPATHLEN, BU_DIR_BIN, NULL);
    bu_dir(db, MAXPATHLEN, BU_DIR_DATA, "db", NULL);
    bu_dir(pix, MAXPATHLEN, BU_DIR_DATA, "pix" , NULL);
    bu_dir(thisp, MAXPATHLEN, BU_DIR_CURR, NULL);

    bu_vls_printf(&vp,
		  "%s/rt "
		  "%s/rt "
		  "./rt",
		  root, thisp);
    bu_argv_from_string(argv, 32, bu_vls_addr(&vp));
    look_for(verbose_echo, executable, "the BRL-CAD raytracer", "RT", (const char **)argv);
    bu_vls_trunc(&vp, 0);

    bu_vls_printf(&vp,
		  "%s/moss.g "
		  "%s/db/moss.g "
		  "%s/../db/moss.g "
		  "./db/moss.g "
		  "./moss.g",
		  db, thisp, thisp);
    bu_argv_from_string(argv, 32, bu_vls_addr(&vp));
    look_for(verbose_echo, file, "a benchmark geometry directory", "DB", (const char **)argv);
    bu_vls_trunc(&vp, 0);
    if (getenv("DB") && !bu_file_directory(getenv("DB"))) {
	bu_setenv("DB", bu_path_dirname(getenv("DB")), 1);
    }

    bu_vls_printf(&vp,
		  "%s/moss.pix "
		  "%s/pix/moss.pix "
		  "%s/../pix/moss.pix "
		  "%s/../../pix/moss.pix "
		  "%s/../../../pix/moss.pix "
		  "./pix/moss.pix "
		  "./moss.pix",
		  pix, thisp, thisp, thisp, thisp);
    bu_argv_from_string(argv, 32, bu_vls_addr(&vp));
    look_for(verbose_echo, file, "a benchmark reference image directory", "PIX", (const char **)argv);
    bu_vls_trunc(&vp, 0);
    if (getenv("PIX") && !bu_file_directory(getenv("PIX"))) {
	bu_setenv("PIX", bu_path_dirname(getenv("PIX")), 1);
    }

    bu_vls_printf(&vp,
		  "%s "
		  "%s "
		  "%s/pix "
		      "%s/../pix "
		  "%s/../../pix "
		  "%s/../../../pix "
		  "./pix",
		  getenv("PIX"), pix, thisp, thisp, thisp, thisp);
    bu_argv_from_string(argv, 32, bu_vls_addr(&vp));
    look_for(verbose_echo, directory, "a benchmark reference log directory", "LOG", (const char **)argv);
    bu_vls_trunc(&vp, 0);

    bu_vls_printf(&vp,
		  "%s/pixcmp "
		  "%s/pixcmp "
		  "%s/bench/pixcmp "
		  "%s/../bench/pixcmp "
		  "./pixcmp",
		  root, thisp, thisp, thisp);
    bu_argv_from_string(argv, 32, bu_vls_addr(&vp));
    look_for(verbose_echo, executable, "a pixel comparison utility", "CMP", (const char **)argv);
    bu_vls_trunc(&vp, 0);

    /* FIXME: elapsed is no more */
    bu_vls_printf(&vp,
		  "%s/elapsed.sh "
		  "%s/elapsed.sh "
		  "%s/sh/elapsed.sh "
		  "%s/../sh/elapsed.sh "
		  "%s/../../sh/elapsed.sh "
		  "%s/../../../sh/elapsed.sh "
		  "./elapsed.sh",
		  root, thisp, thisp, thisp, thisp, thisp);
    bu_argv_from_string(argv, 32, bu_vls_addr(&vp));
    look_for(verbose_echo, script, "a time elapsed utility", "ELP", (const char **)argv);
    bu_vls_trunc(&vp, 0);

    /*********************
     * output parameters *
     *********************/

    /* sanity check, output all the final settings together */
    RT = getenv("RT");
    if (RT && strlen(RT) > 0) {
	echo("Using [%s] for RT\n", RT);
    } else {
	bu_exit(1, "ERROR: Could not find the BRL-CAD raytracer\n");
    }

    DB = getenv("DB");
    if (DB && strlen(DB) > 0) {
	echo("Using [%s] for DB\n", DB);
    } else {
	bu_exit(1, "ERROR: Could not find the BRL-CAD database directory\n");
    }

    PIX = getenv("PIX");
    if (PIX && strlen(PIX) > 0) {
	echo("Using [%s] for PIX\n", PIX);
    } else {
	bu_exit(1, "ERROR: Could not find the BRL-CAD reference images\n");
    }

    LOG = getenv("LOG");
    if (LOG && strlen(LOG) > 0) {
	echo("Using [%s] for LOG\n", LOG);
    } else {
	bu_exit(1, "ERROR: Could not find the BRL-CAD reference logs\n");
    }

    CMP = getenv("CMP");
    if (CMP && strlen(CMP) > 0) {
	echo("Using [%s] for CMP\n", CMP);
    } else {
	bu_exit(1, "ERROR: Could not find the BRL-CAD pixel comparison utility\n");
    }

    ELP = getenv("ELP");
    if (ELP && strlen(ELP) > 0) {
	echo("Using [%s] for ELP\n", ELP);
    } else {
	bu_exit(1, "ERROR: Could not find the BRL-CAD time elapsed script\n");
    }

    /* determine the minimum time requirement in seconds for a single test run */
    set_if_unset(echo, verbose_echo, "TIMEFRAME", "32");

    /* approximate maximum time in seconds that a given test is allowed to take */
    set_if_unset(echo, verbose_echo, "MAXTIME", "300");
    {
	MAXTIME = strtol(getenv("MAXTIME"), NULL, 0);
	TIMEFRAME = strtol(getenv("TIMEFRAME"), NULL, 0);
	if (MAXTIME < TIMEFRAME) {
	    bu_exit(1, "ERROR: MAXTIME must be greater or equal to TIMEFRAME\n");
	}
    }

    /* maximum deviation percentage */
    set_if_unset(echo, verbose_echo, "DEVIATION", "3");

    /* maximum number of iterations to average */
    set_if_unset(echo, verbose_echo, "AVERAGE", "3");

    /* end of settings, separate the output */
    echo("\n");


    /**************************
     * output run-time status *
     **************************/

    /* determine raytracer version */
    echo("RT reports the following version information:\n");
    {
	struct bu_vls rtout = BU_VLS_INIT_ZERO;
	struct bu_vls grep1 = BU_VLS_INIT_ZERO;
	struct bu_vls grep2 = BU_VLS_INIT_ZERO;

	RT = getenv("RT");

	bu_exec(RT, NULL, &rtout, &rtout);
	bu_grep("BRL-CAD", bu_vls_cstr(&rtout), stashline, (void *)&grep1);
	bu_grep("Release", bu_vls_cstr(&grep1), stashline, (void *)&grep2);
	if (bu_vls_strlen(&grep2) > 0) {
	    echo("%s\n", bu_vls_addr(&grep2));
	} else {
	    echo("Unknown\n\n");
	}

	bu_vls_free(&grep2);
	bu_vls_free(&grep1);
	bu_vls_free(&rtout);
    }

    /* let the user know about how long this might take */
    {
	struct bu_vls elapsed = BU_VLS_INIT_ZERO;
	double mintime = 6.0 * TIMEFRAME;
	double maxtime = 6.0 * MAXTIME;
	double estimate = 3.0 * mintime;

	if (mintime < 1.0)
	    mintime = 0; /* zero is okay */
	bu_vls_sprintf(&elapsed, "%s", format_elapsed(mintime));
	echo("Minimum run time is %s\n", bu_vls_cstr(&elapsed));

	if (maxtime < 1.0)
	    maxtime = 1.0; /* zero would be misleading */
	bu_vls_sprintf(&elapsed, "%s", format_elapsed(maxtime));
	echo("Maximum run time is %s\n", bu_vls_cstr(&elapsed));

	if (estimate > maxtime)
	    estimate = maxtime;

	echo("Estimated time is %s\n", format_elapsed(estimate));
    }

    /************************
     * Run the actual tests *
     ************************/

    start = bu_gettime();
    echo("Running the BRL-CAD Benchmark tests... please wait ...\n\n");

    ret += bench("moss");
    ret += bench("world");
    ret += bench("star");
    ret += bench("bldg391");
    ret += bench("m35");
    ret += bench("sphflake");

    echo("\n... Done.\n");
    echo("Total testing time elapsed: %.2lfs\n", format_elapsed(bu_gettime() - start));

    if (ret != 0) {
	echo("\n");
	echo("THE BENCHMARK ANALYSIS DID NOT COMPLETE SUCCESSFULLY.\n");
	echo("\n");
	echo("A benchmark failure means this is not a viable install of BRL-CAD.  This may be\n");
	echo("a new bug or (more likely) is a compilation configuration error.  Ensure your\n");
	echo("compiler has strict aliasing disabled, compilation is optimized, and you have\n");
	echo("installed BRL-CAD (some platforms require this).  If you still get a failure,\n");
	echo("please report your configuration information to benchmark@brlcad.org\n");
	echo("\n");
	echo("Output was saved to %s from %s\n", LOG, bu_dir(NULL, 0, BU_DIR_CURR, NULL));
	echo("Run '%s clean' to remove generated pix files.\n", av[0]);
	echo("Benchmark testing failed.\n");
	return 2;
    }

    /******************************
     * compute and output results *
     ******************************/
    {
	struct bu_vls performance = BU_VLS_INIT_ZERO;
	ret = perf(&performance, /*...!!!...*/"moss world star bldg391 m35 sphflake", (const char **)argv);

	if (ret == 0) {
	    /* !!! write performance to summary */
	}

	echo("\n");
	echo("The following files have been generated and/or modified:\n");
	echo("  *.log ..... final log files for each individual raytrace test\n");
	echo("  *.pix ..... final pix image files for each individual raytrace test\n");
	echo("  *.log.* ... log files for previous frames and raytrace tests\n");
	echo("  *.pix.* ... pix image files for previous frames and raytrace tests\n");
	echo("  summary ... performance results summary, 2 lines per run\n");
	echo("\n");
	echo("Run '%s clean' to remove generated pix files.\n", av[0]);
	echo("\n");

	echo("Summary:\n");
	echo("%s\n", bu_vls_cstr(&performance));

	{
	    double vgr = calculate_vgr(&performance);

	    if (vgr > 0.0) {
		echo("\n");
		echo("#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#\n");
		echo("Benchmark results indicate an approximate VGR performance metric of %ld\n", (long int)vgr);
		echo("Logarithmic VGR metric is %.2lf  (natural logarithm is %.2lf)\n", log10(vgr), log(vgr));
		echo("#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#\n");
		echo("\n");
		echo("These numbers seem to indicate that this machine is approximately %ld times", (long int)vgr);
		echo("faster than the reference machine being used for comparison, a VAX 11/780\n");
		echo("running 4.3 BSD named VGR.  These results are in fact approximately %.2lf", log10(vgr));
		echo("orders of magnitude faster than the reference.\n");
		echo("\n");

		echo("Here are some other approximated VGR results for perspective:\n");
		echo("    120 on a 200MHz R5000 running IRIX 6.5\n");
		echo("    250 on a 500 MHz Pentium III running RedHat 7.1\n");
		echo("    550 on a dual 450 MHz UltraSPARC II running SunOS 5.8\n");
		echo("   1000 on a dual 500 MHz G4 PowerPC running Mac OS X 10.2\n");
		echo("   1500 on a dual 1.66 GHz Athlon MP 2000+ running RedHat 7.3\n");
		echo("  52000 on a 4x4 CPU 2.93 GHz Xeon running RHEL Server 5.4\n");
		echo("  65000 on a 512 CPU 400 MHz R12000 Running IRIX 6.5\n");
		echo("\n");
	    }
	}
    }

    {
	/* TODO: !!! determine if this build is optimized or not */
	/* TODO: !!! determine if this build has run-time debugging */
	/* TODO: !!! determine if this build has compile-time debugging */
    }

    /* if this was a valid benchmark run, encourage submission of results. */
    echo("You are encouraged to submit your benchmark results and system\n");
    echo("configuration information to benchmark@brlcad.org\n");
    echo("                             ~~~~~~~~~~~~~~~~~~~~\n");

    /* include information about the operating system and hardware in the log */
    echo("Including additional kernel and hardware information in the log.\n\n");

    /* TODO: !!! print out system information via sysctl/prtdiag/prtconf/hinv/etc */
    bu_vls_free(&rtargs);

    bu_vls_printf(&vp,
		  "%s/../share/brlcad/*.*.*/doc/benchmark.tr "
		  "%s/share/brlcad/*.*.*/doc/benchmark.tr "
		  "%s/share/brlcad/doc/benchmark.tr "
		  "%s/share/doc/benchmark.tr "
		  "%s/doc/benchmark.tr "
		  "%s/../doc/benchmark.tr "
		  "./benchmark.tr",
		  thisp, thisp, thisp, thisp, thisp, thisp);
    bu_argv_from_string(argv, 32, bu_vls_addr(&vp));

    look_for(NULL, file, "BENCHMARK_TR", "benchmark", (const char **)argv);
    bu_vls_trunc(&vp, 0);

    /* TODO: look for benchmark document here? */

    echo("Output was saved to %s from %s\n", LOG, bu_dir(NULL, 0, BU_DIR_CURR, NULL));
    echo("Benchmark testing complete.\n");

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
