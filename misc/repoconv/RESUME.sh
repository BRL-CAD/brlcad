#!/bin/bash

REPODIR="$PWD/brlcad_repo"

echo "Rsyncing BRL-CAD SVN repository"
mv $REPODIR code
rsync -av svn.code.sf.net::p/brlcad/code .
mv code $REPODIR

# Make a dump file
rm -f brlcad_full.dump
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

# Begin the primary svn conversion
g++ -O3 -o svnfexport svnfexport.cxx

echo "Start main conversion"
REPODERCSDIR="$PWD/repo_dercs"
./svnfexport ./brlcad_full_dercs.dump account-map $REPODERCSDIR

echo "Do a file git gc --aggressive"
git gc --aggressive

echo "Make the final tar.gz file (NOTE!!! we can't use git bundle for this, it drops all the notes with the svn rev info)"
mkdir brlcad-git
mv .git brlcad-git
tar -czf ../brlcad-git.tar.gz brlcad-git
mv brlcad-git .git

echo "Be aware that by default a checkout of the repo won't get the notes - it requires an extra step, see https://stackoverflow.com/questions/37941650/fetch-git-notes-when-cloning"
