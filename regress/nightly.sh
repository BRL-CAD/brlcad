#!/bin/sh

case $1 in
master)
    if [ ! -x brlcad/regress/master.sh ] ; then
	cvs -q -d /c/CVS export -D today -N brlcad/regress
    fi
    cvs update brlcad
    cd brlcad/regress
    ./master.sh -d /c/regress -r /c/CVS
    ;;
status)
    if [ ! -x brlcad/regress/master.sh ] ; then
	cvs -q -d /c/CVS export -D today -N brlcad/regress
    fi
    cvs update brlcad
    cd brlcad/regress
    ./status.sh -d /c/regress -a lamas@arl.army.mil
    ;;
*)
    /bin/echo 'must specify "master" or "status" argument'
    ;;
esac