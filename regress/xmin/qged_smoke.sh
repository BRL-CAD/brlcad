#!/bin/sh

set -eu

: "${GUI_ARTIFACT_DIR:?GUI_ARTIFACT_DIR is required}"

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 qged [argument ...]" >&2
    exit 2
fi

XDG_CONFIG_HOME="$GUI_ARTIFACT_DIR/config"
export XDG_CONFIG_HOME

exec "$@"

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
