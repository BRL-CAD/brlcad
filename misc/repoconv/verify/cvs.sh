#!/bin/bash
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

