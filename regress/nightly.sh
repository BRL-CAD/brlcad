#!/bin/sh
#
###
#
# This script intended for internal use.  It verifies that there is a current
# checkout of the regression suite available in the current working directory
# and then will either invoke the master script or the status script.
#
###
#
# crontab entries for running the master scripts
#
#   0 5 * * * ~/nightly.sh master
#   0 8 * * * ~/nightly.sh status
#
# crontab entries for running the client scripts with nfs or local disk access
#
#   30 5 * * * ~/brlcad/regress/client_wait.sh -d /c/regress
#
###############################################################################

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