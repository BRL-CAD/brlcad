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
     --enable-docs)               options="$options -DBRLCAD_EXTRADOCS=ON";
     			          shift;;
     --enable-opengl)             options="$options -DBRLCAD_ENABLE_OPENGL=ON";
     			          shift;;
     --enable-strict)             options="$options -DBRLCAD_ENABLE_STRICT=ON";
     			          shift;;
     --enable-warnings)           options="$options -DBRLCAD_ENABLE_WARNINGS=ON";
     			          shift;;
     --enable-debug) 	          options="$options -DBRLCAD_FLAGS_DEBUG=ON";
                   	          shift;;
     --enable-regex) 	          options="$options -DBRLCAD_REGEX=Bundled";
                   	          shift;;
     --enable-zlib) 	          options="$options -DBRLCAD_ZLIB=Bundled";
                   	          shift;;
     --enable-lemon) 	          options="$options -DBRLCAD_LEMON=Bundled";
                   	          shift;;
     --enable-re2c) 	          options="$options -DBRLCAD_RE2C=Bundled";
                   	          shift;;
     --enable-perplex) 	          options="$options -DBRLCAD_PERPLEX=Bundled";
                   	          shift;;
     --enable-xsltproc)           options="$options -DBRLCAD_XSLTPROC=Bundled";
                   	          shift;;
     --enable-xmllint) 	          options="$options -DBRLCAD_XMLLINT=Bundled";
                   	          shift;;
     --enable-termlib) 	          options="$options -DBRLCAD_TERMLIB=Bundled";
                   	          shift;;
     --enable-png) 	          options="$options -DBRLCAD_PNG=Bundled";
                   	          shift;;
     --enable-utahrle) 	          options="$options -DBRLCAD_UTAHRLE=Bundled";
                   	          shift;;
     --enable-tcl) 	          options="$options -DBRLCAD_TCL=Bundled";
                   	          shift;;
     --enable-tk) 	          options="$options -DBRLCAD_ENABLE_TK=Bundled";
                   	          shift;;
     --enable-itcl) 	          options="$options -DBRLCAD_ITCL=Bundled";
                   	          shift;;
     --enable-itk) 	          options="$options -DBRLCAD_ITK=Bundled";
                   	          shift;;
     --enable-togl) 	          options="$options -DBRLCAD_TOGL=Bundled";
                   	          shift;;
     --enable-iwidgets) 	  options="$options -DBRLCAD_IWIDGETS=Bundled";
                   	          shift;;
     --enable-tkhtml) 	          options="$options -DBRLCAD_TKHTML=Bundled";
                   	          shift;;
     --enable-tkpng) 	          options="$options -DBRLCAD_TKPNG=Bundled";
                   	          shift;;
     --enable-tktable) 	          options="$options -DBRLCAD_TKTABLE=Bundled";
                   	          shift;;
     --enable-opennurbs) 	  options="$options -DBRLCAD_OPENNURBS=Bundled";
                   	          shift;;
     --enable-scl) 	          options="$options -DBRLCAD_SCL=Bundled";
                   	          shift;;
     --enable-step) 	          options="$options -DBRLCAD_SCL=Bundled";
                   	          shift;;
     --disable-all) 	          options="$options -DBRLCAD_BUNDLED_LIBS=System";
                   	          shift;;
     --disable-docs)              options="$options -DBRLCAD_EXTRADOCS=OFF";
     			          shift;;
     --disable-opengl)            options="$options -DBRLCAD_ENABLE_OPENGL=OFF";
     			          shift;;
     --disable-strict)            options="$options -DBRLCAD_ENABLE_STRICT=OFF";
     			          shift;;
     --disable-warnings)          options="$options -DBRLCAD_ENABLE_WARNINGS=OFF";
     			          shift;;
     --disable-debug) 	          options="$options -DBRLCAD_FLAGS_DEBUG=OFF";
                   	          shift;;
     --disable-regex) 	          options="$options -DBRLCAD_REGEX=System";
                   	          shift;;
     --disable-zlib) 	          options="$options -DBRLCAD_ZLIB=System";
                   	          shift;;
     --disable-lemon) 	          options="$options -DBRLCAD_LEMON=System";
                   	          shift;;
     --disable-re2c) 	          options="$options -DBRLCAD_RE2C=System";
                   	          shift;;
     --disable-perplex) 	  options="$options -DBRLCAD_PERPLEX=System";
                   	          shift;;
     --disable-xsltproc)          options="$options -DBRLCAD_XSLTPROC=System";
                   	          shift;;
     --disable-xmllint) 	  options="$options -DBRLCAD_XMLLINT=System";
                   	          shift;;
     --disable-termlib) 	  options="$options -DBRLCAD_TERMLIB=System";
                   	          shift;;
     --disable-png) 	          options="$options -DBRLCAD_PNG=System";
                   	          shift;;
     --disable-utahrle) 	  options="$options -DBRLCAD_UTAHRLE=System";
                   	          shift;;
     --disable-tcl) 	          options="$options -DBRLCAD_TCL=System";
                   	          shift;;
     --disable-tk) 	          options="$options -DBRLCAD_ENABLE_TK=System";
                   	          shift;;
     --disable-itcl) 	          options="$options -DBRLCAD_ITCL=System";
                   	          shift;;
     --disable-itk) 	          options="$options -DBRLCAD_ITK=System";
                   	          shift;;
     --disable-togl) 	          options="$options -DBRLCAD_TOGL=System";
                   	          shift;;
     --disable-iwidgets) 	  options="$options -DBRLCAD_IWIDGETS=System";
                   	          shift;;
     --disable-tkhtml) 	          options="$options -DBRLCAD_TKHTML=System";
                   	          shift;;
     --disable-tkpng) 	          options="$options -DBRLCAD_TKPNG=System";
                   	          shift;;
     --disable-tktable) 	  options="$options -DBRLCAD_TKTABLE=System";
                   	          shift;;
     --disable-opennurbs) 	  options="$options -DBRLCAD_OPENNURBS=System";
                   	          shift;;
     --disable-scl) 	          options="$options -DBRLCAD_SCL=System";
                   	          shift;;
     --disable-step) 	          options="$options -DBRLCAD_SCL=System";
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
