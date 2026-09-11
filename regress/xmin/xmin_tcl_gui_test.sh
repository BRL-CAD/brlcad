#!/bin/sh

set -eu

: "${XMIN_TEST_SHELL:?XMIN_TEST_SHELL is required}"
. "$XMIN_TEST_SHELL"

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 test-name executable [argument ...]" >&2
    exit 2
fi

test_name=$1
shift
test_program=$1

xmin_require_executable "$test_program"

XMIN_TEST_DIR=$(mktemp -d "/tmp/brlcad-xmin-${test_name}.XXXXXX")
export XMIN_TEST_DIR

xmin_tcl_test_cleanup()
{
    if [ "${XMIN_KEEP_ARTIFACTS:-0}" -eq 0 ]; then
	"${CMAKE_COMMAND:-cmake}" -E remove_directory "$XMIN_TEST_DIR"
    fi
}
trap 'exit 1' INT TERM

if ! "$@"; then
    echo "Xmin GUI test artifacts: $XMIN_TEST_DIR" >&2
    exit 1
fi

if [ "${XMIN_KEEP_ARTIFACTS:-0}" -eq 1 ]; then
    echo "Xmin GUI test artifacts: $XMIN_TEST_DIR"
else
    xmin_tcl_test_cleanup
fi
