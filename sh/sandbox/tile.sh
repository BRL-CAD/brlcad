#!/bin/zsh
#                         T I L E . S H
# BRL-CAD
#
# Copyright (c) 2025 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###

# what directory to scan
# FIXME: should take as an arg
DIR=.

# prefix name of the output pix/png files
OUT=tile

# size of individual tile renderings
SZ=64

# how many tiles per row (i.e., columns)
COLS=10


# Function to parse and set script arguments
dynamic_set_variables() {
  for arg in "$@" ; do
    # Check if the argument matches the KEY=value format
    if [[ "$arg" =~ ^([A-Za-z_][A-Za-z0-9_]*)=(.*)$ ]] ; then
      key="${match[1]}"
      value="${match[2]}"
      # Check if the key is one of the allowed variables
      case "$key" in
        DIR|OUT|SZ|COLS)
          eval "$key=\"$value\""
          ;;
        *)
          echo "ERROR: Unknown variable '$key'"
          print_usage
          return 1
          ;;
      esac
    else
      echo "ERROR: Invalid argument format: $arg"
      print_usage
      return 1
    fi
  done
}


print_usage() {
  echo "Usage: $0 [DIR=<directory>] [OUT=<output_prefix>] [SZ=<tile_size>] [COLS=<columns>]"
  echo "\nVariables:"
  echo "  DIR   - Directory to scan"
  echo "  OUT   - Prefix name of the output pix/png files"
  echo "  SZ    - Square size of individual tile renderings"
  echo "  COLS  - Number of tiles per row (i.e., columns)"
}


# Define the cleanup function to handle the signal
cleanup() {
  echo "Script interrupted. Exiting..."
  exit 1
}

# Set the trap to catch SIGINT (Ctrl-C)
trap cleanup INT

# zsh annoyingly blathers by default during globbing
setopt nonomatch

# call the function with script arguments
dynamic_set_variables "$@" || exit 1

# log our behavior
echo "Scanning for .g files in DIR=$DIR"
echo "Writing pix/png with OUT=$OUT prefix"
echo "Rendering tiles ${SZ}x${SZ} given SZ=$SZ"
echo "Tiling COLS=$COLS tiles per row"


export i=0
export tiles=""

G="`find $DIR -name \*.g -print`"
echo "Geometry files:"
echo "$G"
echo ""

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
    tileout=tile.$i.$gbase.$obj.pix
    tilelog=tile.$i.$gbase.$obj.rt.log
    rm -f $tileout $tilelog
    bin/rt -l3 -s$SZ -o $tileout $gfile `echo $found` 2>$tilelog

    tiles="$tiles $tileout"
    i="`expr $i \+ 1`"
done

echo "TILES:"
echo "$tiles"

width="$((SZ * COLS))"
rows="$((i / COLS))"
rows="$((rows + 1))"
height="$((rows * SZ))"

rm -f $OUT.pix
bin/pixtile -s$SZ -W$width -N$height `echo $tiles` | bin/pix-fb -w $width -n $height -F$OUT.pix

rm -f $OUT.png
cat $OUT.pix | bin/pix-png -w$width -n$height -o $OUT.png

#rm -f tile.*

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
