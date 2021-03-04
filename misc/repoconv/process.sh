#!/bin/bash
rm -rf git_repo test.fi
rm -rf add replace splices
mkdir add && cp ../../repoconv/repowork_new_commmits/add/* add/
mkdir replace && cp ../../repoconv/repowork_new_commmits/replace/* replace/
mkdir splices && cp ../../repoconv/repowork_new_commmits/splices/* splices/
cp ../../repoconv/cvs_svn_branches.txt .
cp ../../repoconv/svn_rev_updates.txt .
cp ../../repoconv/branch_corrections.txt .
cat ../../repoconv/remove_commits.txt | awk -F"#" '{print $1}' | awk '{gsub(/[ ]+$/,""); print $0}' |awk NF > remove_commits.txt
cat ../../repoconv/tag_commits.txt | grep \^commit|awk '{print $2}' > tag_sha1s
./repowork --svn-revs svn_rev_updates.txt --svn-branches cvs_svn_branches.txt --svn-branches-to-tags tag_sha1s --correct-branches branch_corrections.txt --remove-commits remove_commits.txt --splice-commits --replace-commits --add-commits conv12.fi test.fi
mkdir git_repo && cd git_repo && git init
cat ../test.fi |git fast-import
git checkout main
git gc --aggressive
git reflog expire --expire-unreachable=now --all
git gc --prune=now
cd ..

