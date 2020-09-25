#!/bin/bash

REPODIR="$PWD/brlcad_repo"

echo "Rsyncing BRL-CAD SVN repository"
mv $REPODIR code
rsync -av svn.code.sf.net::p/brlcad/code .
mv code $REPODIR

# Make a dump file
svnadmin dump $REPODIR > brlcad_full.dump

# Strip the populated RCS tags from as much of the SVN repo
# as we can, then use the new dump file to populate a
# repo to make sure the dump file wasn't damaged
g++ -O3 -o dercs svn_de-rcs.cxx
rm -f brlcad_full_dercs.dump
./dercs brlcad_full.dump brlcad_full_dercs.dump
rm -rf repo_dercs
svnadmin create repo_dercs
svnadmin load repo_dercs < brlcad_full_dercs.dump

