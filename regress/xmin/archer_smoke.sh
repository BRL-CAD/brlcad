#!/bin/sh

set -eu

: "${XMIN_ARTIFACT_DIR:?XMIN_ARTIFACT_DIR is required}"

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 archer [argument ...]" >&2
    exit 2
fi

ARCHER_PREFS_FILE="$XMIN_ARTIFACT_DIR/.archerrc"
export ARCHER_PREFS_FILE

exec "$@"
