#!/bin/sh

set -eu

: "${XMIN_ARTIFACT_DIR:?XMIN_ARTIFACT_DIR is required}"

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 qged [argument ...]" >&2
    exit 2
fi

XDG_CONFIG_HOME="$XMIN_ARTIFACT_DIR/config"
export XDG_CONFIG_HOME

exec "$@"
