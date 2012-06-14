#!/bin/sh

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

export LDIR || (echo "'LDIR' not defined."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.

. "$LDIR/regress/library.sh"

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    echo "Unable to find mged, aborting"
    exit 1
fi

CV="`ensearch cv`"
if test ! -f "$CV" ; then
    echo "Unable to find cv, aborting"
    exit 1
fi

A2P="`ensearch asc2pix`"
if test ! -f "$A2P" ; then
    echo "Unable to find asc2pix, aborting"
    exit 1
fi

P2B="`ensearch pix-bw`"
if test ! -f "$P2B" ; then
    echo "Unable to find pix-bw, aborting"
    exit 1
fi

RT="`ensearch rt`"
if test ! -f "$RT" ; then
    echo "Unable to find rt, aborting"
    exit 1
fi

# don't know how to do all this elegantly
export MGED
export CV
export A2P
export P2B
export RT
