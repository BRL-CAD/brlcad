#!/bin/bash

# We want the script to fail hard and immediately if anything goes wrong, in
# order to avoid masking failures (e.g. build failures, which will leave
# empty scan reports but certainly don't constitute successes. See (for example)
# https://stackoverflow.com/q/821396
set -e
set -o pipefail

# Note: we set these variables to tell the ccc-analyzer script which compilers
# to use, but it is not always enough - when some of the sub-builds are
# triggered, they do not seem to inherit these settings and revert to the
# default compiler.  The only way so far to avoid this issue reliably seems to
# be to edit the ccc-analyzer script to use the compiler we want as the default
# compiler.
export CCC_CC=clang
export CCC_CXX=clang++

# This appears to be a workable way to enable the new Z3 static analyzer
# support, but at least as of 2017-12 it greatly slows the testing (by orders
# of magnitude).  Might be viable (or at least more useful even if it can't
# quickly complete) if we completely pre-build src/other
#export CCC_ANALYZER_CONSTRAINTS_MODEL=z3

declare -i failure num
declare -i report_cnt num

function runtest {
	echo "$1"
	scan-build --use-analyzer=/usr/bin/$CCC_CC -o ./scan-reports-$1 make -j12 $1
	if [ "$(ls -A ./scan-reports-$1)" ]; then
		report_cnt=$(ls -l ./scan-reports-$1/*/report* | wc -l)
		failure=$(($failure + $report_cnt))
	else
		rm -rf ./scan-reports-$1
	fi
}

# configure using the correct compiler and values.  We don't
# need this report, so clear it after configure is done
scan-build --use-analyzer=/usr/bin/$CCC_CC -o ./scan-reports-config cmake .. -DBRLCAD_EXTRADOCS=OFF -DCMAKE_C_COMPILER=ccc-analyzer -DCMAKE_CXX_COMPILER=c++-analyzer

# clear out any old reports
rm -rfv ./scan-reports-*

# The following test should ideally generate empty directories (i.e. their
# report directory should not be present at the end of the test.
failure="0"

# These are src/other libs that are either forked or don't have significant
# upstream activity - since we are effectively maintaining these ourselves,
# check them
runtest osmesa
runtest poly2tri
runtest utahrle

# we don't care about reports for src/other or misc tools
# we don't maintain ourselves - build those without recording
# the analyzer output
cd src/other
make -j12
cd ../../
cd src/other/ext
make -j12
cd ../../../
cd misc/tools
make -j12
cd ../../

# Do primary build
runtest all

# Report results
echo "Summary: $failure failures found."

# If we have more than the expected failure count, error out
if [ "$failures" -gt "20" ]; then
	exit 1
fi

