#!/bin/bash

# We want the script to fail hard and immediately if anything goes wrong, in
# order to avoid masking failures (e.g. build failures, which will leave
# empty scan reports but certainly don't constitute successes. See (for example)
# https://stackoverflow.com/q/821396
set -e
set -o pipefail

# CLANG_VERSION should be set by User or CI environments to avoid hard-coding
# a particular version into the script
if [ -z "$CLANG_VERSION" ]; then
    echo "Error: CLANG_VERSION environment variable is not set." >&2
    exit 1
fi

# Derive tool paths from the version
SCAN_BUILD_DIR="/usr/share/clang/scan-build-${CLANG_VERSION}"
SCAN_BUILD_BIN="${SCAN_BUILD_DIR}/bin"
SCAN_BUILD_LIBEXEC="${SCAN_BUILD_DIR}/libexec"
CLANG_ANALYZER_BIN="/usr/bin/clang-${CLANG_VERSION}"

# Add scan-build to the PATH
export PATH="${SCAN_BUILD_BIN}:${SCAN_BUILD_LIBEXEC}:${PATH}"

# Set compilers for the analyzer scripts
export CCC_CC="clang-${CLANG_VERSION}"
export CCC_CXX="clang++-${CLANG_VERSION}"

# This appears to be a workable way to enable the new Z3 static analyzer
# support, but at least as of 2017-12 it greatly slows the testing (by orders
# of magnitude).  Update 2024 - might be viable (or at least more useful even
# if it can't quickly complete) now that we're pre-building bext
#export CCC_ANALYZER_CONSTRAINTS_MODEL=z3

declare -i failure num
declare -i report_cnt num

# For this purpose, we don't need (or want) bext to be compiled with the static
# analyzer.  CI supplies a prebuilt tree; retain the local fallback so this
# script remains useful on its own.
export CC=clang
export CXX=clang++
cwdir=$(pwd)
bext_dir="${BRLCAD_EXT_DIR:-}"
if [ -z "$bext_dir" ]; then
	bext_dir="$cwdir"
	git clone https://github.com/BRL-CAD/bext
	mkdir bext-build
	cmake -S "$cwdir/bext" -B "$cwdir/bext-build" -DCMAKE_INSTALL_PREFIX="$bext_dir" -DCMAKE_BUILD_TYPE=Debug
	cmake --build bext-build --config Debug -j 1
	# Save a little space
	rm -rf bext-build
	rm -rf bext
fi

# Encapsulate the logic to do a scan build
function runtest {
    echo "$1"
    scan-build --use-analyzer="${CLANG_ANALYZER_BIN}" -o "./scan-reports-$1" make -j12 "$1"
    if [ -d "./scan-reports-$1" ] && [ -n "$(ls -A "./scan-reports-$1")" ]; then
        report_cnt=$(find "./scan-reports-$1" -name 'report-*.html' | wc -l)
        failure=$((failure + report_cnt))
    else
        rm -rf "./scan-reports-$1"
    fi
}

# Configure using the correct compiler and values.
scan-build --use-analyzer="${CLANG_ANALYZER_BIN}" -o ./scan-reports-config cmake .. \
    -DBRLCAD_EXTRADOCS=OFF \
    -DBRLCAD_ENABLE_QT=ON \
    -DBRLCAD_LTO_MODE=OFF \
    -DBRLCAD_EXT_DIR="$bext_dir" \
    -DCMAKE_C_COMPILER=ccc-analyzer \
    -DCMAKE_CXX_COMPILER=c++-analyzer

# clear out any old reports
rm -rfv ./scan-reports-*

# The following test should ideally generate empty directories (i.e. their
# report directory should not be present at the end of the test.
failure="0"

# Do primary build
runtest all

# Report results
echo "Summary: $failure failures found."

# If we have more than the expected failure count, error out
if [ "$failure" -gt "75" ]; then
	exit 1
fi

