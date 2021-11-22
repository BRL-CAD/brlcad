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

expected_success=()
declare -A expected_failure
new_success=()
new_failure=()

# function for testing targets that are expected to be clean
function cleantest {
	echo $1
	scan-build --use-analyzer=/usr/bin/$CCC_CC -o ./scan-reports-$1 make -j12 $1
	if [ "$(ls -A ./scan-reports-$1)" ]; then
		new_failure+=("$1")
		((failures ++))
	else
		expected_success+=("$1")
	fi
}
function failingtest {
	echo "$1"
	scan-build --use-analyzer=/usr/bin/$CCC_CC -o ./scan-reports-$1 make -j12 $1
	if [ "$(ls -A ./scan-reports-$1)" ]; then
		#TODO - find some way to tell if new failures have been introduced...
		report_cnt=$(ls -l ./scan-reports-$1/*/report* | wc -l)
		expected_failure[$1]="$report_cnt"
	else
		new_success+=("$1")
	fi
}

function notest {
	echo "$1"
	make -j12 $1
}

# configure using the correct compiler and values.  We don't
# need this report, so clear it after configure is done
scan-build --use-analyzer=/usr/bin/$CCC_CC -o ./scan-reports-config cmake .. -DBRLCAD_EXTRADOCS=OFF -DCMAKE_C_COMPILER=ccc-analyzer -DCMAKE_CXX_COMPILER=c++-analyzer

# clear out any old reports
rm -rfv ./scan-reports-*

# The following test should ideally generate empty directories (i.e. their
# report directory should not be present at the end of the test.
failures="0"

# These are src/other libs that are either forked or don't have significant
# upstream activity - since we are effectively maintaining these ourselves,
# check them
cleantest osmesa
cleantest poly2tri
cleantest utahrle

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

# Libraries - order is important here.  We want to build all of
# a library's dependencies before we build the library so the
# problem reports (if any) are specific to that library

cleantest libbu
cleantest libpkg
cleantest libbn
cleantest libbv
failingtest libbg
failingtest libnmg
failingtest libbrep
cleantest librt
cleantest libwdb
cleantest libgcv
failingtest gcv_plugins
cleantest libanalyze
cleantest liboptical
cleantest libicv
cleantest libged
cleantest libdm
cleantest dm_plugins
cleantest libfft
cleantest ged_plugins
cleantest libtclcad
cleantest libtermio
cleantest librender

# Executables and anything else we haven't checked yet
failingtest all

echo "Summary:"
if [ "${#new_failure[@]}" -ne "0" ]; then
	echo "   new failures: ${new_failure[@]}"
fi
if [ "${#new_success[@]}" -ne "0" ]; then
	echo "   new successes: ${new_success[@]}"
fi
if [ "${#expected_success[@]}" -ne "0" ]; then
	echo "   successes(expected): ${expected_success[@]}"
fi
if [ "${#expected_failure[@]}" -ne "0" ]; then
	echo "   failures(expected):"
for f in "${!expected_failure[@]}"; do
	echo "                       $f(${expected_failure["$f"]} bugs)";
done
fi

# Clean up empty scan-* output dirs
find . -type d -empty -name scan-\* |xargs rmdir

if [ "$failures" -ne "0" ]; then
	exit 1
fi

