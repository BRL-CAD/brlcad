#!/bin/sh
rm *.blob map.txt info.txt
./git_svn_filemap.sh css.c
./git_svn_filemap.sh cssdynamic.c
./git_svn_filemap.sh cssparser.c
./git_svn_filemap.sh csssearch.c
./git_svn_filemap.sh htmldecode.c
./git_svn_filemap.sh htmldraw.c
./git_svn_filemap.sh htmlfloat.c
./git_svn_filemap.sh htmlimage.c
./git_svn_filemap.sh htmlinline.c
./git_svn_filemap.sh htmllayout.c
./git_svn_filemap.sh htmlparse.c
./git_svn_filemap.sh htmlprop.c
./git_svn_filemap.sh htmlstyle.c
./git_svn_filemap.sh htmltable.c
./git_svn_filemap.sh htmltagdb.c
./git_svn_filemap.sh htmltcl.c
./git_svn_filemap.sh htmltree.c
./git_svn_filemap.sh main.c
./git_svn_filemap.sh restrack.c
./git_svn_filemap.sh swproc.c

