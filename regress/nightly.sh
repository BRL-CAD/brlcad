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
    TASK=master
    echo "RUNNING MASTER"
    ;;
status)
    TASK=status
    echo "CHECKING STATUS"
    ;;
*)
    /bin/echo 'must specify "master" or "status" argument'
    exit -1
    ;;
esac

cvs -q -d /c/CVS export -D today -N brlcad/regress
cd brlcad/regress
cp nightly.sh ../..

if [ "x$TASK" = "xmaster" ] ; then
    ./master.sh -d /c/regress -r /c/CVS
    echo "DONE RUNNING MASTER"
elif [ "x#TASK" = "xstatus" ] ; then
    ./status.sh -d /c/regress -a morrison@arl.army.mil
    echo "DONE CHECKING STATUS"
else
    echo "INTERNAL ERROR -- not master or status"
    exit -1
fi

exit 0
