#!/bin/bash

# Do initial setup for appropriate comparison repositories
./cvs.sh
./svn.sh

CVSDIR="$PWD/brlcad_cvs"
SVNDIR="$PWD/repo_dercs"

./verify --cvs-repo $CVSDIR --svn-repo $SVNDIR $1
