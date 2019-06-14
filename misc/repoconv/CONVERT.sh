#!/bin/bash

# Need an up-to-date copy of the BRL-CAD svn repository on the filesystem

echo "Rsyncing BRL-CAD SVN repository"
REPODIR="$PWD/brlcad_repo"
mv $REPODIR code
rsync -av svn.code.sf.net::p/brlcad/code .
mv code $REPODIR

# Put info in a subdirectory to keep the top level relatively clear
rm -rf cvs_git_info
mkdir cvs_git_info

# To run the conversion (need to use cvs-fast-export rather than cvsconvert
# for the actual conversion to support the authors file):
if [ ! -e "brlcad_cvs.tar.gz" ]; then
        curl -o brlcad_cvs.tar.gz https://brlcad.org/brlcad_cvs.tar.gz
fi
tar -xf brlcad_cvs.tar.gz
cd brlcad_cvs/brlcad
rm src/librt/Attic/parse.c,v
rm pix/sphflake.pix,v
cp ../../cvs_repaired/sphflake.pix,v pix/
# RCS headers introduce unnecessary file differences, which are poison pills
# for git log --follow
echo "Scrubbing expanded RCS headers"
find . -type f -exec sed -i 's/$Date:[^$;"]*/$Date/' {} \;
find . -type f -exec sed -i 's/$Header:[^$;"]*/$Header/' {} \;
find . -type f -exec sed -i 's/$Id:[^$;"]*/$Id/' {} \;
find . -type f -exec sed -i 's/$Log:[^$;"]*/$Log/' {} \;
find . -type f -exec sed -i 's/$Revision:[^$;"]*/$Revision/' {} \;
find . -type f -exec sed -i 's/$Source:[^$;"]*/$Source/' {} \;
sed -i 's/$Author:[^$;"]*/$Author/' misc/Attic/cvs2cl.pl,v
sed -i 's/$Author:[^$;"]*/$Author/' sh/Attic/cvs2cl.pl,v
sed -i 's/$Locker:[^$;"]*/$Locker/' src/other/URToolkit/tools/mallocNd.c,v

echo "Running cvs-fast-export"
find . | cvs-fast-export -A ../../cvs_authormap > ../../brlcad_cvs_git.fi
cd ../
cvsconvert -n brlcad 2> ../cvs_git_info/cvs_all_fast_export_audit.txt
cd ../
rm -rf brlcad_cvs_git
mkdir brlcad_cvs_git
cd brlcad_cvs_git
git init
cat ../brlcad_cvs_git.fi | git fast-import
rm ../brlcad_cvs_git.fi
git checkout master

# This repository has as its newest commit a "test acl" commit that doesn't
# appear in subversion - the newest master commit matching subversion corresponds
# to r29886.  To clear this commit and label the new head with its corresponding
# subversion revision, we use a prepared fast import template and apply it (thus
# matching the note date to the commit date:

git reset --hard HEAD~1
CHEAD=$(git show-ref master | awk '{print $1}')
sed "s/CURRENTHEAD/$CHEAD/" ../29886-note.fi > 29886-note.fi
cat ./29886-note.fi | git fast-import
rm ./29886-note.fi

# Prepare a list of the SHA1 commits corresponding the the heads and tags, to allow
# for subsequent fast-imports that will reference them as parents:
git show-ref --heads --tags > ../cvs_git_info/brlcad_cvs_git_heads_sha1.txt

# List all blob SHA1 hashes, so we can detect in subsequent processing if SVN is
# referencing an object we don't have from the git conversion.
# (This line from https://stackoverflow.com/a/25954360/2037687)
git rev-list --objects --all | git cat-file --batch-check='%(objectname) %(objecttype) %(rest)' | grep '^[^ ]* blob' | cut -d" " -f1,3- > ../cvs_git_info/brlcad_cvs_git_all_blob_sha1.txt

# Comparing this to the svn checkout:
echo "Validating cvs-fast-export r29886 against SVN"
git archive --format=tar --prefix=brlcad_cvs-r29886/ HEAD | gzip > ../brlcad_cvs-r29886.tar.gz
# (assuming a local brlcad rsynced svn copy)
cd ..
tar -xf brlcad_cvs-r29886.tar.gz
svn co -q -r29886 file://$REPODIR/brlcad/trunk brlcad_svn-r29886
find ./brlcad_cvs-r29886 -name .gitignore |xargs rm
find ./brlcad_svn-r29886 -name .cvsignore |xargs rm
rm -rf brlcad_svn-r29886/.svn

# terra.dsp appears to be incorrectly checked out by SVN due to the file
# mime-type being set to a a text file - the CVS version is treated as correct
# for these purposes.
# (After fixing the mime-type in SVN trunk, the CVS and SVN files match.)
diff -qrw -I '\$Id' -I '\$Revision' -I'\$Header' -I'$Source' -I'$Date' -I'$Log' -I'$Locker' --exclude "terra.dsp" brlcad_cvs-r29886 brlcad_svn-r29886

# cleanup
rm -rf brlcad_cvs
rm -rf brlcad_cvs-r29886
rm -rf brlcad_svn-r29886
rm brlcad_cvs-r29886.tar.gz

# CVS conversion is ready, now set the stage for SVN

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

# Generate the set of SVN messages and map those which we can
# to the cvs git conversion
cp -r brlcad_cvs_git brlcad_cvs_git.bak
rm -f svn_msgs.txt git.log
g++ -O3 -o svn_msgs svn_msgs.cxx
./svn_msgs brlcad_full_dercs.dump
cd brlcad_cvs_git && git log --all --pretty=format:"GITMSG%n%H,%ct +0000,%B%nGITMSGEND%n" > ../git.log && cd ..
g++ -O3 -o svn_map_commit_revs svn_map_commit_revs.cxx
./svn_map_commit_revs
rm *-note.fi

# Begin the primary svn conversion
g++ -O3 -o svnfexport svnfexport.cxx
REPODERCSDIR="$PWD/repo_dercs"
./svnfexport ./brlcad_full_dercs.dump account-map $REPODERCSDIR
