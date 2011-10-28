#!/bin/sh
#
# This is a simple script to translate autotools style
# configure options into CMake variables.  The resulting
# translation is then run as a CMake configure.

srcpath=$(dirname $0)
options="$srcpath"
while [ "$1" != "" ]
do
   case $1
   in
     --enable-all) 	          options="$options -DBRLCAD_BUNDLED_LIBS=Bundled";
                   	          shift;;
     --enable-debug) 	          options="$options -DBRLCAD-ENABLE_DEBUG_FLAGS=ON";
                   	          shift;;
     --disable-docs)              options="$options -DBRLCAD_EXTRADOCS=OFF";
     			          shift;;
     --disable-opengl)            options="$options -DBRLCAD-ENABLE_OPENGL=OFF";
     			          shift;;
     --disable-strict)            options="$options -DBRLCAD-ENABLE_STRICT=OFF";
     			          shift;;
     --disable-warnings)          options="$options -DBRLCAD-ENABLE_WARNINGS=OFF";
     			          shift;;
     --disable-zlib)          	  options="$options -DBRLCAD_ZLIB=System";
     			          shift;;
     --prefix=*)   	          inputstr=$1;
     		   	          options="$options -DCMAKE_INSTALL_PREFIX=${inputstr#--prefix=}";
     		   	          shift;;
     *) 	   	          echo "Warning: unknown option $1";
     		   	          shift;;
   esac
done 
echo cmake $options
cmake $options
