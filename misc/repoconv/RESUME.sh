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
./svnfexport ./brlcad_full_dercs.dump account-map_svnfexport.txt $REPODERCSDIR

# Clear previous "final" conversion
rm -rf brlcad_final.git brlcad_final.tar.gz

# Create an updated svn revision to author map
svn log file://$REPODIR | grep "|" | grep "^r[0-9][0-9 ]" | grep -v \(no\ author\) | awk -F "|" '{print $1 $2}' | sed -e 's/r//' | sed -e 's/ $//' | sed -e 's/  / /' > rev_map

# Create a fast export file of the conversion.  IMPORTANT - need
# original ids if we're going to process the git notes down into
# the commit messages.
cd cvs_git && git fast-export --show-original-ids --all > ../brlcad_raw.fi && cd ..
repowork -t -w -e email_fixups.txt -n -r cvs_git -s rev_map ~/brlcad_raw.fi brlcad_final.fi

mkdir brlcad_final.git && cd brlcad_final.git && git init
cat ../brlcad_final.fi | git fast-import

# Compress the fast-import - by default, it is unoptimized
git gc --aggressive
git reflog expire --expire-unreachable=now --all
git gc --prune=now
cd ..

# Package up the conversion
tar -czf brlcad_final.tar.gz brlcad_final.git

# If uploading to github, it will look something like the following once the
# repository has been created through the github website.  Note, in particular,
# that UNLIKE the github instructions we push everything, not just master:
#
# cd brlcad_final.git
# git remote add origin git@github.com:BRL-CAD/BRL-CAD.git
# git push --all -u origin

