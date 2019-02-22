#!/bin/bash
#
# This "README" is runnable, if the first argument is a full path to an rsynced
# copy of the BRL-CAD svn repository on the filesystem (e.g.
# /home/user/brlcad-repo)

# To run the conversion (need to use cvs-fast-export rather than cvsconvert
# for the actual conversion to support the authors file):
if [ ! -e "brlcad_cvs.tar.gz" ]; then
	curl -o brlcad_cvs.tar.gz https://brlcad.org/brlcad_cvs.tar.gz
fi
tar -xf brlcad_cvs.tar.gz
cd brlcad_cvs/brlcad
rm src/librt/Attic/parse.c,v
rm pix/sphflake.pix,v
cp ../../repaired/sphflake.pix,v pix/
# RCS headers introduce unnecessary file differences, which are poison pills
# for git log --follow
find . -type f -exec sed -i 's/$Date:[^$;"]*/$Date/' {} \;
find . -type f -exec sed -i 's/$Header:[^$;"]*/$Header/' {} \;
find . -type f -exec sed -i 's/$Id:[^$;"]*/$Id/' {} \;
find . -type f -exec sed -i 's/$Log:[^$;"]*/$Log/' {} \;
find . -type f -exec sed -i 's/$Revision:[^$;"]*/$Revision/' {} \;
find . -type f -exec sed -i 's/$Source:[^$;"]*/$Source/' {} \;
sed -i 's/$Author:[^$;"]*/$Author/' misc/Attic/cvs2cl.pl,v
sed -i 's/$Author:[^$;"]*/$Author/' sh/Attic/cvs2cl.pl,v
sed -i 's/$Locker:[^$;"]*/$Locker/' src/other/URToolkit/tools/mallocNd.c,v

# Note - it is possible (with a mode to cvs-fast-export source code) to completely
# skip the .cvsignore -> .gitignore conversion.  This might be something we want
# to do if we're not going to try and preserve svn's ignore properties...

#--- cvs-fast-export-1.45/export.c       2018-07-12 06:15:39.000000000 -0400
#+++ cvs-fast-export-1.45-new/export.c   2019-02-03 12:18:14.166917182 -0500
#@@ -139,6 +139,10 @@
#     export_stats.snapsize += len;
# 
#     if (strcmp(node->commit->master->name, ".cvsignore") == 0) {
#+       return;
#+    }
#+
#+    if (strcmp(node->commit->master->name, ".cvsignore") == 0) {
#        extralen = sizeof(CVS_IGNORES) - 1;
#     }
# 
#@@ -451,7 +455,7 @@
#     if (report)
#        printf("mark :%d\n", (int)mark);
#     if (report) {
#-       static bool need_ignores = true;
#+       static bool need_ignores = false;
#        const char *ts;
#        ct = display_date(commit, mark, opts->force_dates);
#        ts = utc_offset_timestamp(&ct, timezone);
#
#
# find . -name \*cvsignore\* |xargs rm
# cat CVSROOT/history | grep -v cvsignore > history_rewrite
# mv history_rewrite CVSROOT/history

find . | cvs-fast-export -A ../../authormap > ../../brlcad_cvs_git.fi
cd ../
rm -f ../cvs_all_fast_export_audit.txt
cvsconvert -n brlcad 2> ../cvs_all_fast_export_audit.txt
cd ../
mkdir brlcad_cvs_git-$(date +"%Y%m%d")
cd brlcad_cvs_git-$(date +"%Y%m%d")
git init
cat ../brlcad_cvs_git.fi | git fast-import
rm ../brlcad_cvs_git.fi
git checkout master

# This repository has as its newest commit a "test acl" commit that doesn't
# appear in subversion - the newest master commit matching subversion corresponds
# to r29886.  To clear this commit and label the new head with its corresponding
# subversion revision, do:

git reset --hard HEAD~1
git notes add -m "svn:revision:29886"

# Prepare a list of the SHA1 commits corresponding the the heads and tags, to allow
# for subsequent fast-imports that will reference them as parents:
git show-ref --heads --tags > ../brlcad_cvs_git_heads_sha1.txt

# List all blob SHA1 hashes, so we can detect in subsequent processing if SVN is
# referencing an object we don't have from the git conversion.
# (This line from https://stackoverflow.com/a/25954360/2037687)
git rev-list --objects --all | git cat-file --batch-check='%(objectname) %(objecttype) %(rest)' | grep '^[^ ]* blob' | cut -d" " -f1,3- > ../brlcad_cvs_git_all_blob_sha1.txt

# Comparing this to the svn checkout:
git archive --format=tar --prefix=brlcad_cvs-r29886/ HEAD | gzip > ../brlcad_cvs-r29886.tar.gz
# (assuming a local brlcad rsynced svn copy)
cd ..
tar -xf brlcad_cvs-r29886.tar.gz
svn co -q -r29886 file://$1/brlcad/trunk brlcad_svn-r29886
find ./brlcad_cvs-r29886 -name .gitignore |xargs rm
find ./brlcad_svn-r29886 -name .cvsignore |xargs rm
rm -rf brlcad_svn-r29886/.svn

# terra.dsp appears to be incorrectly checked out by SVN due to the file
# mime-type being set to a a text file - the CVS version is treated as correct
# for these purposes and hence the reported diff of terra.dsp is expected.
# (After fixing the mime-type in SVN trunk, the CVS and SVN files match.)
diff -qrw -I '\$Id' -I '\$Revision' -I'\$Header' -I'$Source' -I'$Date' -I'$Log' -I'$Locker' brlcad_cvs-r29886 brlcad_svn-r29886


# cleanup
rm -rf brlcad_cvs
rm -rf brlcad_cvs-r29886
rm -rf brlcad_svn-r29886
rm brlcad_cvs-r29886.tar.gz

# Other useful commands:

# Overview with all branches:

# gitk --branches="*" --show-notes

# Nifty trick for getting commit counts per committer (https://stackoverflow.com/a/9839491)
# git shortlog -s -n --all

# Good way to get a quick overview of branch/tag structure:
# tig --all --simplify-by-decoration


