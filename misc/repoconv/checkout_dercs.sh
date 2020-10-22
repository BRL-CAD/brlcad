#!/bin/bash
find . -type f ! -path "*/misc/*" ! -path "*/src/superbuild/stepcode/*" ! -path "*/src/other/*stepcode/*" ! -path "*/src/other/step/*" ! -path "*/src/conv/step/*" ! -path "*/ap242.exp" -exec sed -i 's/$Date:[^$;"]*/$Date/' {} \;
find . -type f ! -path "*/misc/*" ! -path "*/src/superbuild/stepcode/*" ! -path "*/src/other/*stepcode/*" ! -path "*/src/other/step/*" ! -path "*/src/conv/step/*" ! -path "*/ap242.exp" -exec sed -i 's/$Header:[^$;"]*/$Header/' {} \;
find . -type f ! -path "*/misc/*" ! -path "*/src/superbuild/stepcode/*" ! -path "*/src/other/*stepcode/*" ! -path "*/src/other/step/*" ! -path "*/src/conv/step/*" ! -path "*/ap242.exp" -exec sed -i 's/$Id:[^$;"]*/$Id/' {} \;
find . -type f ! -path "*/misc/*" ! -path "*/src/superbuild/stepcode/*" ! -path "*/src/other/*stepcode/*" ! -path "*/src/other/step/*" ! -path "*/src/conv/step/*" ! -path "*/ap242.exp" -exec sed -i 's/$Log:[^$;"]*/$Log/' {} \;
find . -type f ! -path "*/misc/*" ! -path "*/src/superbuild/stepcode/*" ! -path "*/src/other/*stepcode/*" ! -path "*/src/other/step/*" ! -path "*/src/conv/step/*" ! -path "*/ap242.exp" -exec sed -i 's/$Revision:[^$;"]*/$Revision/' {} \;
find . -type f ! -path "*/misc/*" ! -path "*/src/superbuild/stepcode/*" ! -path "*/src/other/*stepcode/*" ! -path "*/src/other/step/*" ! -path "*/src/conv/step/*" ! -path "*/ap242.exp" -exec sed -i 's/$Source:[^$;"]*/$Source/' {} \;
find . -type f -path "*/re2c/*" -exec sed -i 's/$Date:[^$;"]*/$Date/' {} \;
find . -type f -path "*/re2c/*" -exec sed -i 's/$Header:[^$;"]*/$Header/' {} \;
find . -type f -path "*/re2c/*" -exec sed -i 's/$Id:[^$;"]*/$Id/' {} \;
find . -type f -path "*/re2c/*" -exec sed -i 's/$Log:[^$;"]*/$Log/' {} \;
find . -type f -path "*/re2c/*" -exec sed -i 's/$Revision:[^$;"]*/$Revision/' {} \;
find . -type f -path "*/re2c/*" -exec sed -i 's/$Source:[^$;"]*/$Source/' {} \;
find . -type f -path "*/win32-msvc8/*" -exec sed -i 's/$Date:[^$;"]*/$Date/' {} \;
find . -type f -path "*/win32-msvc8/*" -exec sed -i 's/$Header:[^$;"]*/$Header/' {} \;
find . -type f -path "*/win32-msvc8/*" -exec sed -i 's/$Id:[^$;"]*/$Id/' {} \;
find . -type f -path "*/win32-msvc8/*" -exec sed -i 's/$Log:[^$;"]*/$Log/' {} \;
find . -type f -path "*/win32-msvc8/*" -exec sed -i 's/$Revision:[^$;"]*/$Revision/' {} \;
find . -type f -path "*/win32-msvc8/*" -exec sed -i 's/$Source:[^$;"]*/$Source/' {} \;
find . -type f -path "*/win32-msvc8/*" -exec sed -i 's/$Date:[^$;"]*/$Date/' {} \;
find . -type f -path "*/win32-msvc/*" -exec sed -i 's/$Header:[^$;"]*/$Header/' {} \;
find . -type f -path "*/win32-msvc/*" -exec sed -i 's/$Id:[^$;"]*/$Id/' {} \;
find . -type f -path "*/win32-msvc/*" -exec sed -i 's/$Log:[^$;"]*/$Log/' {} \;
find . -type f -path "*/win32-msvc/*" -exec sed -i 's/$Revision:[^$;"]*/$Revision/' {} \;
find . -type f -path "*/win32-msvc/*" -exec sed -i 's/$Source:[^$;"]*/$Source/' {} \;

sed -i 's/$Id:[^$;"]*/$Id/' misc/archlinux/brlcad.install
sed -i 's/$Id:[^$;"]*/$Id/' misc/brlcad.spec.in
sed -i 's/$Locker:[^$;"]*/$Locker/' src/other/URToolkit/tools/mallocNd.c
