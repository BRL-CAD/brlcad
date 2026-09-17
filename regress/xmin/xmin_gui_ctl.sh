#!/bin/sh

set -eu

: "${XMIN_CTL:?XMIN_CTL is required}"

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 command [argument ...]" >&2
    exit 2
fi

command=$1
shift

case "$command" in
    capture-screen)
	exec "$XMIN_CTL" capture-root "$@"
	;;
    screen-geometry)
	: "${GUI_TEST_SCREEN:?GUI_TEST_SCREEN is required}"
	width=${GUI_TEST_SCREEN%%x*}
	height_depth=${GUI_TEST_SCREEN#*x}
	height=${height_depth%%x*}
	printf '%s %s\n' "$width" "$height"
	;;
    *)
	exec "$XMIN_CTL" "$command" "$@"
	;;
esac

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
