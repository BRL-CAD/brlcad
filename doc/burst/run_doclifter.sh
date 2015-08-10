#!/bin/sh

MASTER=burst
#MASTER=debug

doclifter -h hints.txt -x -vvvv $MASTER.mm #2>t

STATUS=$?

echo "doclifter return value for '$MASTER' was '$STATUS'"