#!/bin/sh
export CCC_CC=clang
export CCC_CXX=clang++
scan-build --use-analyzer=/path/to/clang -o ~/scan-reports cmake .. -DENABLE_ALL=ON -DCMAKE_C_COMPILER=ccc-analyzer -DCMAKE_CXX_COMPILER=c++-analyzer
rm -rf ~/scan-reports
cd src/other
make -j3
cd ../../misc/tools/astyle
make -j3
cd ../../../misc/tools/xmltools
make -j3
cd ../../../doc/docbook/articles
make -j3
cd ../../../doc/docbook/books
make -j3
cd ../../../doc/docbook/lessons
make -j3
cd ../../../doc/docbook/presentations
make -j3
cd ../../../doc/docbook/specifications
make -j3
cd ../../../doc/docbook/system/man1
make -j3
cd ../../../../doc/docbook/system/man3
make -j3
cd ../../../../doc/docbook/system/mann
make -j3
cd ../../../../
scan-build --use-analyzer=/path/to/clang -o ~/scan-reports make
