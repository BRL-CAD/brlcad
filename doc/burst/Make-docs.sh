#!/bin/sh

#OPTS='-V'
OPTS='-b'

groff $OPTS -mm -Tps burst.mm   > burst.ps
ps2pdf burst.ps

#groff $OPTS -mm

#groff -mm -Thtml burst.mm > burst.html
