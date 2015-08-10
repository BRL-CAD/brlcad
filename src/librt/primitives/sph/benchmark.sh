#!/bin/sh

for PROGRAM in rt "$PWD/opencl"; do
    LOG_FILE="data/$(basename "$PROGRAM").log"
    echo "$LOG_FILE"
    rm -f "$LOG_FILE"
    for I in {1..10}; do
        env "$PROGRAM" -F/dev/null share/db/sphflake.g depth0.r depth1.r depth2.r depth3.r 2>&1 | grep RTFM >>"$LOG_FILE"
    done
done
