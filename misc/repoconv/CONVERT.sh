#!/bin/bash

rm -rf brlcad_cvs_git brlcad_full.dump brlcad_full_dercs.dump repo_dercs
rm -rf brlcad_git_checkout brlcad_svn_checkout
rm -f current_rev.txt git.log nsha1.txt rev_gsha1s.txt svn_msgs.txt branches.txt

REPODIR="$PWD/brlcad_repo"

if [ ! -e "cvs-fast-export" ]; then
        git clone https://gitlab.com/esr/cvs-fast-export.git
fi
cd cvs-fast-export && make cvs-fast-export && cd ..

# Need an up-to-date copy of the BRL-CAD svn repository on the filesystem


echo "Rsyncing BRL-CAD SVN repository"
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
rm -rf brlcad_cvs
tar -xf brlcad_cvs.tar.gz
cd brlcad_cvs/brlcad
rm src/librt/Attic/parse.c,v
rm pix/sphflake.pix,v
cp ../../cvs_repaired/sphflake.pix,v pix/
# RCS headers introduce unnecessary file differences, which are poison pills
# for git log --follow
echo "Scrubbing expanded RCS headers"
echo "Date"
find . -type f -exec sed -i 's/$Date:[^$;"]*/$Date/' {} \;
echo "Header"
find . -type f -exec sed -i 's/$Header:[^$;"]*/$Header/' {} \;
echo "Id"
find . -type f -exec sed -i 's/$Id:[^$;"]*/$Id/' {} \;
echo "Log"
find . -type f -exec sed -i 's/$Log:[^$;"]*/$Log/' {} \;
echo "Revision"
find . -type f -exec sed -i 's/$Revision:[^$;"]*/$Revision/' {} \;
echo "Source"
find . -type f -exec sed -i 's/$Source:[^$;"]*/$Source/' {} \;
sed -i 's/$Author:[^$;"]*/$Author/' misc/Attic/cvs2cl.pl,v
sed -i 's/$Author:[^$;"]*/$Author/' sh/Attic/cvs2cl.pl,v
sed -i 's/$Locker:[^$;"]*/$Locker/' src/other/URToolkit/tools/mallocNd.c,v

echo "Running cvs-fast-export $PWD"
find . | ../../cvs-fast-export/cvs-fast-export -A ../../cvs_authormap > ../../brlcad_cvs_git.fi
cd ../
../cvs-fast-export/cvsconvert -n brlcad 2> ../cvs_git_info/cvs_all_fast_export_audit.txt
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
sed "s/CURRENTHEAD/$CHEAD/" ../29886-note-template.fi > 29886-note.fi
cat ./29886-note.fi | git fast-import
rm ./29886-note.fi
cd ..

# Comparing this to the svn checkout:
echo "Validating cvs-fast-export r29886 against SVN"
cd brlcad_cvs_git && git archive --format=tar --prefix=brlcad_cvs-r29886/ HEAD | gzip > ../brlcad_cvs-r29886.tar.gz && cd ..
# (assuming a local brlcad rsynced svn copy)
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

# Position repo for svnfexport
echo "Prepare cvs_git"
rm -rf cvs_git
cp -r brlcad_cvs_git cvs_git

# Unpack merge data files
echo "Unpack supporting data"
tar -xf manual_merge_info.tar.gz
tar -xf gitignore.tar.gz

# Begin the primary svn conversion
g++ -O3 -o svnfexport svnfexport.cxx

echo "Start main conversion"
REPODERCSDIR="$PWD/repo_dercs"
./svnfexport ./brlcad_full_dercs.dump account-map $REPODERCSDIR

# Clone the repo via a mirror (gets us a copy without internal git conversion
# logs taking up space)
echo "Mirroring to brlcad_git directory"
mkdir brlcad_git && cd brlcad_git
git clone --mirror file://$PWD/cvs_git .git
git init
git remote rm origin

echo "Do a file git gc --aggressive"
git gc --aggressive

echo "Clean up the one commit from CVS that never made it to the svn history"
git reflog expire --expire-unreachable=now --all
git gc --prune=now

echo "Make the final tar.gz file (NOTE!!! we can't use git bundle for this, it drops all the notes with the svn rev info)"
mv .git brlcad-git
tar -czf ../brlcad-git.tar.gz brlcad-git
mv brlcad-git .git

echo "Be aware that by default a checkout of the repo won't get the notes - it requires an extra step, see https://stackoverflow.com/questions/37941650/fetch-git-notes-when-cloning"
