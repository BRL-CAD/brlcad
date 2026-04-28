#!/usr/bin/env bash
# run_rt_opt_lcov.sh
#
# Generate an lcov HTML coverage report for rt's option-parsing code
# (src/rt/opt.c).
#
# USAGE
#   bash run_rt_opt_lcov.sh [BUILD_DIR] [REPORT_DIR]
#
#   BUILD_DIR  – cmake build directory (default: brlcad_build_cov next to the
#                repo root, which this script will create and configure if
#                it does not already exist).
#   REPORT_DIR – where to write the HTML report (default: $BUILD_DIR/lcov_rt_opt)
#
# REQUIREMENTS
#   lcov >= 1.14, genhtml (part of the lcov package), gcov (GCC), cmake
#
# NOTES
#   • The build is configured with BRLCAD_ENABLE_COVERAGE=ON which adds
#     -fprofile-arcs -ftest-coverage to every compilation unit.
#   • Only src/rt/opt.c is included in the final report; all other files
#     are filtered out so the output is focused and concise.
#   • Exit code: 0 = all tests passed and report generated;
#                1 = test failure or tool not found.

set -euo pipefail

# ── locate repo root ─────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"   # brlcad/
WORKSPACE="$(cd "${REPO_ROOT}/.." && pwd)"          # quickiterate root

# ── argument defaults ─────────────────────────────────────────────────────────
BUILD_DIR="${1:-${WORKSPACE}/brlcad_build_cov}"
REPORT_DIR="${2:-${BUILD_DIR}/lcov_rt_opt}"

# ── tool checks ───────────────────────────────────────────────────────────────
for tool in cmake lcov genhtml; do
    if ! command -v "${tool}" &>/dev/null; then
        echo "ERROR: '${tool}' not found in PATH; please install it first."
        exit 1
    fi
done

# Allow the caller to override the gcov binary (default: gcov matching GCC 13).
# lcov passes --gcov-tool to gcov; if gcov-13 is available it gives the most
# accurate results when the project was compiled with GCC 13.
GCOV_TOOL="${GCOV_TOOL:-}"
if [ -z "${GCOV_TOOL}" ]; then
    if command -v gcov-13 &>/dev/null; then
        GCOV_TOOL="gcov-13"
    elif command -v gcov &>/dev/null; then
        GCOV_TOOL="gcov"
    else
        echo "ERROR: no gcov binary found; install gcov or set GCOV_TOOL."
        exit 1
    fi
fi
echo "==> Using gcov: ${GCOV_TOOL}"

# ── configure (only if the build directory is fresh) ─────────────────────────
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    echo "==> Configuring coverage build in ${BUILD_DIR} ..."
    cmake -S "${REPO_ROOT}" \
          -B "${BUILD_DIR}" \
          -DBRLCAD_EXT_DIR="${WORKSPACE}/bext_output" \
          -DBRLCAD_EXTRADOCS=OFF \
          -DBRLCAD_ENABLE_STEP=OFF \
          -DBRLCAD_ENABLE_GDAL=OFF \
          -DBRLCAD_ENABLE_QT=OFF \
          -DBRLCAD_ENABLE_COVERAGE=ON \
          -DCMAKE_BUILD_TYPE=Debug
fi

# ── build test target ─────────────────────────────────────────────────────────
echo "==> Building test_rt_opt ..."
cmake --build "${BUILD_DIR}" --target test_rt_opt -j"$(nproc)"

# ── zero counters before the test run ─────────────────────────────────────────
echo "==> Resetting coverage counters ..."
lcov --directory "${BUILD_DIR}" --zerocounters --quiet

# ── run the full test suite ───────────────────────────────────────────────────
echo "==> Running test_rt_opt ..."
TEST_BIN="${BUILD_DIR}/src/rt/tests/test_rt_opt"
if [ ! -x "${TEST_BIN}" ]; then
    # Fallback: try the bin/ directory used by some cmake generators
    TEST_BIN="${BUILD_DIR}/bin/test_rt_opt"
fi
"${TEST_BIN}"
TEST_RC=$?

# ── collect coverage data ─────────────────────────────────────────────────────
echo "==> Collecting coverage data ..."
RAW_INFO="${BUILD_DIR}/lcov_rt_opt_raw.info"
lcov --directory "${BUILD_DIR}" \
     --capture \
     --output-file "${RAW_INFO}" \
     --gcov-tool "${GCOV_TOOL}" \
     --rc lcov_branch_coverage=1 \
     --quiet

# ── filter to only src/rt/opt.c ───────────────────────────────────────────────
echo "==> Filtering to opt.c ..."
FILTERED_INFO="${BUILD_DIR}/lcov_rt_opt.info"
lcov --extract "${RAW_INFO}" \
     "*/src/rt/opt.c" \
     --output-file "${FILTERED_INFO}" \
     --gcov-tool "${GCOV_TOOL}" \
     --rc lcov_branch_coverage=1 \
     --quiet

# ── generate HTML report ──────────────────────────────────────────────────────
echo "==> Generating HTML report in ${REPORT_DIR} ..."
mkdir -p "${REPORT_DIR}"
genhtml "${FILTERED_INFO}" \
        --output-directory "${REPORT_DIR}" \
        --branch-coverage \
        --title "rt opt.c coverage" \
        --legend \
        --quiet

# ── summary ───────────────────────────────────────────────────────────────────
echo ""
echo "Coverage report written to: ${REPORT_DIR}/index.html"
lcov --summary "${FILTERED_INFO}" --rc lcov_branch_coverage=1

if [ "${TEST_RC}" -ne 0 ]; then
    echo ""
    echo "WARNING: test_rt_opt exited with ${TEST_RC} (some tests failed)."
    exit 1
fi

echo ""
echo "Done. All tests passed."
