#!/bin/zsh

DBDIR=share/db

G="`find $DBDIR -name \*.g -print`"

# Define the cleanup function to handle the signal
cleanup() {
  echo "Script interrupted. Exiting..."
  exit 1
}

# Set the trap to catch SIGINT (Ctrl-C)
trap cleanup INT

# zsh annoyingly blathers by default during globbing
setopt nonomatch


export i=0

echo "$G" | while read gfile ; do

    trap cleanup INT
    
    glower="`echo $gfile | tr '[:upper:]' '[:lower:]'`"
    if test "x$glower" = "x" ; then
        echo "ERROR: did not expect tr to fail on file $gfile"
    fi
    gbase="`basename $glower`"

    found=""
    for obj in `bin/mged -c $gfile tops -n 2>&1` ; do
        objlower="`echo $obj | tr '[:upper:]' '[:lower:]'`"
        if test "x$objlower" = "x" ; then
            echo "ERROR: did not expect tr to fail on obj $obj"
            break;
        fi

        if test "x$objlower" = "x$gbase" ; then
            # echo YES $gfile:$obj
            found="$obj"
            break;
        fi
        if test "x$objlower" = "x`echo $gbase | sed 's/\.g//g'`" ; then
            # echo YES $gfile:$obj
            found="$obj"
            break;
        fi
        if test "x$objlower" = "xall.g" ; then
            # echo YES $gfile:$obj
            found="$obj"
            break;
        fi
        if test "x$objlower" = "xall" ; then
            # echo YES $gfile:$obj
            found="$obj"
            break;
        fi
        if test "x`echo $objlower | grep -i scene`" != "x" ; then
            # echo YES $gfile:$obj
            found="$obj"
            break;
        fi
        if test "x`echo $objlower | grep -i document`" != "x" ; then
            # echo YES $gfile:$obj
            found="$obj"
            break;
        fi
        if test "x`basename $glower | grep $objlower`" != "x" ; then
            # echo YES $gfile:$obj
            found="$obj"
            break;
        fi
        objsans="`echo $objlower | sed 's/\.*$//g'`"
        if test "x$objsans" != "x" ; then
            if test "x`basename $glower | grep $objsans`" != "x" ; then
                # echo YES $gfile:$obj
                found="$obj"
                break;
            fi
        fi
    done
    if test "x$found" = "x" ; then
        echo Skipping $gfile ...
        continue
    fi

    # make sure we don't have a scene object
    objs=""
    scene=0
    for subobj in `bin/mged -c $gfile lt $found 2>&1 | tr '}' '\n' | awk '{print $2}'` ; do
        mater="`bin/mged -c $gfile attr get $subobj shader 2>&1`"
        if test "x$mater" = "xcloud" ; then
            echo "FOUND cloud on $subobj in $found"
            scene=1
        else
            objs="$objs $subobj"
        fi
    done
    if test "x$scene" = "x1" ; then
        found="$objs"
    fi
     
    echo "Rendering #$i $gfile:$found ..."
    rm -f tile.$i.$gbase.$obj.pix tile.$i.$gbase.$obj.rt.log
    bin/rt -l3 -s128 -o tile.$i.$gbase.$obj.pix $gfile `echo $found` 2>tile.$i.$gbase.$obj.rt.log

    i="`expr $i \+ 1`"
done

out=renderall

rm -f $out.pix
bin/pixtile -s128 -W1200 -N800 tile.*.pix | bin/pix-fb -w 1200 -n 800 -F$out.pix

rm -f $out.png
cat $out.pix | bin/pix-png -w 1200 -n800 -o $out.png

#rm -f tile.*
