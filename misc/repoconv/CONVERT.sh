#!/bin/bash

rm -rf brlcad_cvs_git brlcad_full.dump brlcad_full_dercs.dump repo_dercs
rm -rf brlcad_git_checkout brlcad_svn_checkout
rm -f current_rev.txt git.log nsha1.txt rev_gsha1s.txt svn_msgs.txt branches.txt

REPODIR="$PWD/brlcad_repo"

if [ ! -e "cvs-fast-export" ]; then
        curl -o cvs-fast-export.tar.gz https://gitlab.com/esr/cvs-fast-export/-/archive/1.48/cvs-fast-export-1.48.tar.gz
	tar -xvf cvs-fast-export.tar.gz
	mv cvs-fast-export-1.48 cvs-fast-export
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
find . | ../../cvs-fast-export/cvs-fast-export -A ../../cvs_authormap_svnfexport.txt > ../../brlcad_cvs_git.fi
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
#rm -rf brlcad_cvs
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
./svnfexport ./brlcad_full_dercs.dump account-map_svnfexport.txt $REPODERCSDIR

# Create an svn revision to author map
svn log file://$REPODIR | grep "|" | grep "^r[0-9][0-9 ]" | grep -v \(no\ author\) | awk -F "|" '{print $1 $2}' | sed -e 's/r//' | sed -e 's/ $//' | sed -e 's/  / /' > rev_map

# MANUAL: Generate mapping files with the cvs_info.sh script.  Need
# two maps - one from the archival repo's msg+time key to the data
# we need, and the other a map from that same key to the SHA1 commits
# of the new repository.  The "key" is a SHA1 hash of just the commit
# message, with the Unix time appended to the string produced.  It is
# not guaranteed to be universally unique as a key, but it should be
# for anything we care about (unless we've got two commits with the
# same message and same timestamp in the history, and even then that
# would be a practical problem only if those commits had different
# CVS branches or authors.)
rm -rf cvs_info && mkdir cvs_info && cp cvs_info.sh cvs_info/ && cd cvs_info
./cvs_info.sh
mv key_authormap .. && mv key_branchmap ..
cd ..

# With the basic maps generated from a basic (no authormap) cvs-fast-export
# conversion of the CVS repository, generate the map for our target repo
# (the output of the svnfexport process.  This will produce the msgtime_sha1_map
# file used later in the process
cd cvs_git && ../domap.sh && cd ..

# MANUAL: Run verify on the CVS conversion and stage any differences found for
# incorporation - not sure if we're going to do this yet...  Here's how to kick
# off the process with just a CVS check.
#mkdir verify && cd verify
#g++ -O3 -o verify ../verify.cpp
#cp -r ../brlcad_cvs .
#cp -r ../cvs_git .
#./verify --keymap ../msgtime_sha1_map --branchmap ../key_branchmap --cvs-repo /home/user/verify/brlcad_cvs cvs_git
# mkdir ../trees && cp *.fi ../trees/
#cd ..
# If we need to do this, will also need the children map from git:
# cd cvs_git && git rev-list --children --all > ../children && cd ..

# Create a fast export file of the conversion.  IMPORTANT - need
# original ids if we're going to process the git notes down into
# the commit messages.
cd cvs_git && git checkout master && git fast-export --show-original-ids --all > ../brlcad_raw.fi && cd ..

# Build the repowork processing tool
cd ../repowork && mkdir build && cd build && cmake .. && make -j5 && cd ../../repoconv

# With the preliminaries complete, we use the repowork tool to finalize the conversion:

../repowork/build/repowork -t \
	-e email_fixups.txt \
       	-n -r cvs_git \
       	-s rev_map \
        --keymap msgtime_sha1_map --cvs-auth-map key_authormap --cvs-branch-map key_branchmap \
	~/brlcad_raw.fi brlcad_final.fi

# If we do rebuild CVS commits, the command becomes:
#../repowork/build/repowork -t \
#       -e email_fixups.txt \
#      	-n -r cvs_git \
#      	-s rev_map \
#       --cvs-rebuild-ids cvs_problem_sha1.txt --children children \
#       --keymap msgtime_sha1_map --cvs-auth-map key_authormap --cvs-branch-map key_branchmap \
#       ~/brlcad_raw.fi brlcad_final.fi

mkdir brlcad_final.git && cd brlcad_final.git && git init
cat ../brlcad_final.fi | git fast-import

# Rename master to newer default branch naming convention "main"
git branch -m master main

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
# git push --follow-tags

