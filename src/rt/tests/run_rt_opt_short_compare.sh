#!/usr/bin/env bash
# run_rt_opt_short_compare.sh
#
# Compare short-option behavior of the current parser implementation
# against the unmodified main-branch control version of src/rt.
#
# Usage:
#   bash run_rt_opt_short_compare.sh [BUILD_DIR]
#
# Default BUILD_DIR:
#   <repo_root>/../brlcad_build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${RT_DIR}/../.." && pwd)"
WORKSPACE="$(cd "${REPO_ROOT}/.." && pwd)"
BUILD_DIR="${1:-${WORKSPACE}/brlcad_build}"
BACKUP_DIR="/tmp/rt_opt_compare_src_rt_$$"
LOG_CUR="/tmp/rt_opt_compare_current.$$"
LOG_CTRL="/tmp/rt_opt_compare_control.$$"
RT_CONTROL_FILES=(
    do.c
    grid.c
    heatgraph.c
    opt.c
    scanline.c
    usage.cpp
    worker.c
    viewcheck.c
    ext.h
    rtuif.h
)

restore_rt_sources()
{
    if [ -d "${BACKUP_DIR}" ]; then
	for f in "${RT_CONTROL_FILES[@]}"; do
	    cp "${BACKUP_DIR}/${f}" "${RT_DIR}/${f}"
	done
	rm -rf "${BACKUP_DIR}"
	# ensure the normal source is restored and buildable
	cmake --build "${BUILD_DIR}" --target test_rt_opt -j"$(nproc)"
    fi
}
trap restore_rt_sources EXIT

if ! git -C "${WORKSPACE}" rev-parse --verify origin/main >/dev/null 2>&1; then
    if [ "$(git -C "${WORKSPACE}" rev-parse --is-shallow-repository)" = "true" ]; then
	git -C "${WORKSPACE}" fetch --unshallow origin || {
	    echo "ERROR: failed to unshallow repository while preparing origin/main control sources."
	    exit 1
	}
    fi
    git -C "${WORKSPACE}" fetch origin main:refs/remotes/origin/main || {
	echo "ERROR: failed to fetch origin/main control sources."
	exit 1
    }
fi

mkdir -p "${BACKUP_DIR}"
for f in "${RT_CONTROL_FILES[@]}"; do
    cp "${RT_DIR}/${f}" "${BACKUP_DIR}/${f}"
done

echo "==> Building and running short-only suite (current parser)..."
cmake --build "${BUILD_DIR}" --target test_rt_opt --verbose -j"$(nproc)"
"${BUILD_DIR}/src/rt/test_rt_opt" --short-only >"${LOG_CUR}" 2>&1
RC_CUR=$?
SUM_CUR="$(grep -E 'ALL [0-9]+ TESTS PASSED|[0-9]+/[0-9]+ TESTS FAILED' "${LOG_CUR}" | tail -1 || true)"

echo "==> Replacing src/rt with origin/main control sources..."
for f in "${RT_CONTROL_FILES[@]}"; do
    git -C "${WORKSPACE}" show "origin/main:brlcad/src/rt/${f}" > "${RT_DIR}/${f}" || {
	echo "ERROR: failed to read origin/main:brlcad/src/rt/${f}"
	exit 1
    }
done

echo "==> Building and running short-only suite (origin/main control parser)..."
cmake --build "${BUILD_DIR}" --target test_rt_opt --verbose -j"$(nproc)"
"${BUILD_DIR}/src/rt/test_rt_opt" --short-only >"${LOG_CTRL}" 2>&1
RC_CTRL=$?
SUM_CTRL="$(grep -E 'ALL [0-9]+ TESTS PASSED|[0-9]+/[0-9]+ TESTS FAILED' "${LOG_CTRL}" | tail -1 || true)"

echo "Current summary : ${SUM_CUR:-<none>} (rc=${RC_CUR})"
echo "Control summary : ${SUM_CTRL:-<none>} (rc=${RC_CTRL})"

if [ "${RC_CUR}" -ne "${RC_CTRL}" ]; then
    echo "ERROR: exit codes differ between current and control parser."
    echo "Current log: ${LOG_CUR}"
    echo "Control log: ${LOG_CTRL}"
    exit 1
fi

if [ "${SUM_CUR}" != "${SUM_CTRL}" ]; then
    echo "ERROR: short-only test summaries differ between current and control parser."
    echo "Current log: ${LOG_CUR}"
    echo "Control log: ${LOG_CTRL}"
    exit 1
fi

echo "PASS: short-only suite matches origin/main control behavior."
