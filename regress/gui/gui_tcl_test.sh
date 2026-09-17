#!/bin/sh

set -eu

: "${GUI_TEST_SHELL:?GUI_TEST_SHELL is required}"
. "$GUI_TEST_SHELL"

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 test-name executable [argument ...]" >&2
    exit 2
fi

test_name=$1
shift
test_program=$1

gui_require_executable "$test_program"

GUI_TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/brlcad-gui-${test_name}.XXXXXX")
export GUI_TEST_DIR

gui_tcl_test_cleanup()
{
    if [ "${GUI_KEEP_ARTIFACTS:-0}" -eq 0 ]; then
	"${CMAKE_COMMAND:-cmake}" -E remove_directory "$GUI_TEST_DIR"
    fi
}
trap 'exit 1' INT TERM

if ! "$@"; then
    echo "GUI test artifacts: $GUI_TEST_DIR" >&2
    exit 1
fi

if [ -s "$GUI_TEST_DIR/result" ]; then
    test_result=$(sed -n '1p' "$GUI_TEST_DIR/result")
    case "$test_result" in
	PASS:*) ;;
	*)
	    echo "$test_result" >&2
	    echo "GUI test artifacts: $GUI_TEST_DIR" >&2
	    exit 1
	    ;;
    esac
fi

if [ "${GUI_KEEP_ARTIFACTS:-0}" -eq 1 ]; then
    echo "GUI test artifacts: $GUI_TEST_DIR"
else
    gui_tcl_test_cleanup
fi

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
