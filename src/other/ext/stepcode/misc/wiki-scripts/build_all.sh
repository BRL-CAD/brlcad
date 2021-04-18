#!/bin/sh

#run from the scl root dir
#creates files for upload to the scl wiki. some are in markdown format, others are raw text

out_dir="wiki"
matrix_file="$out_dir/Schema-build-matrix.md"
result_dir="."
mk="make -j4"

#separated by ; for cmake
schemas="../data/203wseds/203wseds.exp;../data/ap203e2/ap203e2_mim_lf.exp;../data/ap210e2/ap210e2_v1_40_mim_lf.exp;../data/wip210e3/210e3_wip_v1_41_mim_lf.exp;../data/ap214e3/AP214E3_2010.exp;../data/ap227/ap227.exp;../data/ap235/AP235_TC_engineering_properties_schema_20110222.exp;../data/ap240/AP240_aim_lf.exp;../data/cd209/part409cdts_wg3n2617mim_lf.exp;../data/cd242/242_n2813_mim_lf.exp;../data/ifc2x3/IFC2X3_TC1.exp;../data/ifc2x4/IFC2X4_RC3.exp;../data/ISO15926/15926-0002-lifecycle_integration.exp"
#count warnings and errors, append to $matrix_file. creates hypertext links to stderr,stdout txt
# $1 is the name of the row, $2 is the path and first part of the filename, $3 is the schema
function count_we {
    e="$2"_stderr.txt
    o="$2"_stdout.txt
    (
        if [ -s $e ]; then
            echo "<tr><td><b>$1</b></td>"
            echo "<td><font color="red">`grep -ci error $e` errors</font></td>"
            echo "<td><font color="blue">`grep -ci warning $e` warnings</font></td>"
            if [ -s $e ]; then
                echo -n "<td><a href=schema_$3#wiki-$1_e>stderr text</a></td>"
            else
                echo "<td>   ----   </td>"
            fi
            echo "</td>"
        else #empty
            echo "<tr><td><b>$1</b></td>"
            echo "<td>0 errors</td>"
            echo "<td>0 warnings</td>"
            echo "<td>   ----   </td>"
        fi
        if [ -s $o ]; then
            echo -n "<td><a href=schema_$3#wiki-$1_o>stdout text</a></td>"
        else #empty
            echo "<td>   ----   </td>"
        fi
        echo "</td></tr>" 
    ) >>$matrix_file
}

#make fedex warnings and errors pretty
function fedex_details {
    i="$result_dir/fedex_$1_stderr.txt"
    w=`sed -ne 's|^.*:\([0-9]\+\): WARNING: \(.*\)$|<tr><td>\1</td><td>\2</td></tr>|p;' $i`
    e=`sed -ne 's|^.*:\([0-9]\+\): --ERROR: \(.*\)$|<tr><td>\1</td><td>\2</td></tr>|p;' $i`
    if [ "x$w" != "x" ]; then
        echo "<a name=\"exp2cxx_w\">" #enable jumping to this location in the file
        echo "### exp2cxx warnings"
        echo "</a><table  width=100%><tr><th>Line</th><th>Message</th>$w</table>"
        echo
    fi
    if [ "x$e" != "x" ]; then
        echo "<a name=\"exp2cxx_e\">"
        echo "### exp2cxx errors"
        echo "</a><table  width=100%><tr><th>Line</th><th>Message</th>$e</tr></table>"
        echo
    fi
    o=`grep -ve ": WARNING:" -e ": --ERROR:" -e ^$ $i`
    if [ "x$o" != "x" ]; then
        echo "### Other text from Standard Error"
        echo "<pre>"
        sed -e 's|^.*:[0-9]\+: WARNING: .*$||g;' -e 's|^.*:[0-9]\+: --ERROR: .*$||g;' -e 's|$|\n|;' -e 's/\t/    /;' $i|grep -v ^$
        echo "</pre>"
        echo
    fi
    if [ -s $result_dir/fedex_$1_stdout.txt ]; then
        echo "<a name=\"exp2cxx_o\">"
        echo "### Standard Output"
        echo "</a><pre>"
        cat $result_dir/fedex_$1_stdout.txt
        echo "</pre>"
    fi
}

function build_one_schema {
    #set $i to the schema name, all caps (to match exp2cxx output)
    i=`sed -ne '0,/^\s*SCHEMA/s/^.*SCHEMA\s\+\(.*\);.*$/\1/p;' $1|tr a-z A-Z`
    d=`echo $1|sed -e 's|^.*/\([^/]*\)\.exp$|\1|;'`

    echo "Running exp2cxx and gcc for $i..."
    make generate_cpp_$d 2>"$result_dir/fedex_"$i"_stderr.txt" >"$result_dir/fedex_"$i"_stdout.txt" && \
    $mk sdai_$d >/dev/null 2>"$result_dir/compile_libsdai_"$i"_stderr.txt" && \
    $mk p21read_sdai_$d >/dev/null 2>"$result_dir/compile_p21read_sdai_"$i"_stderr.txt"

    #todo: test p21read_sdai_$i
}

function build {
    echo "Running cmake..."
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC_LIBS=OFF -DBUILD_SCHEMAS=$schemas >/dev/null

    # build all parts of scl that are necessary for schema libs to build
    # when given multiple targets, cmake's makefiles don't always work well with 'make -j4' - some things get built twice, simultaneously
    echo "Building SCL:"
    $mk stepeditor | grep "^Linking"
    $mk exp2cxx stepdai | grep "^Linking"

    for h in `echo $schemas | sed -e 's/;/\n/g;'`
    do
        build_one_schema $h
    done
}

function make_schema_page {
    # $1 is schema name, i.e. AUTOMOTIVE_DESIGN, $2 is schema *file* name, i.e. ap214e3_mim_lf.exp
    echo "## File $2"
    echo "### Schema $1"
    fedex_details $1
    echo "<a name=\"lib_e\"></a><a name=\"lib_w\"></a>"
    echo "### stderr from compiling lib"
    if [ -s $result_dir/compile_libsdai_${1}_stderr.txt ]; then
        echo "<pre>"
        cat $result_dir/compile_libsdai_${1}_stderr.txt
        echo "</pre>"
    fi
    echo "<a name=\"p21read_e\"></a><a name=\"p21read_w\"></a>"
    echo "### stderr from compiling p21read"
    if [ -s $result_dir/compile_p21read_sdai_${1}_stderr.txt ]; then
        echo "<pre>"
        cat $result_dir/compile_p21read_sdai_${1}_stderr.txt
        echo "</pre>"
    fi
    echo "### end"
}

function gen_wiki {
    echo "## `date`" >$matrix_file
    git log --decorate=short -n1 |\
        head -n1 |\
        sed -e 's|^.*commit \([a-z0-9]\+\) .*$|### Current as of commit [\1](http://github.com/mpictor/StepClassLibrary/commit/\1)|;' >>$matrix_file
    echo -e "\n## See also [PLIB build matrix](http://github.com/mpictor/StepClassLibrary/wiki/PLIB-build-matrix)\n\n" >>$matrix_file
    echo "<table border=1>" >>$matrix_file
    echo "<tr><th>Key</th></tr><tr><td><table width=100%>" >>$matrix_file
    echo "<tr><th>Item</th><th>Description</th></tr>" >>$matrix_file
    echo "<tr><td>exp2cxx</td><td>Generate c++ from an EXPRESS schema</td></tr>" >>$matrix_file
    echo "<tr><td>lib</td><td>Compile the generated c++ into a library</td></tr>" >>$matrix_file
    echo "<tr><td>p21read</td><td>Compile a p21read app, linked against the above library</td></tr>" >>$matrix_file
    echo "</table></td></tr>" >>$matrix_file
    for h in `echo $schemas | sed -e 's/;/\n/g;'`
    do
        i=`sed -ne '0,/^\s*SCHEMA/s/^.*SCHEMA\s\+\(.*\);.*$/\1/p;' $h | tr a-z A-Z`

        #create a table for this schema
        echo "<tr><td><table width=100%><tr><th>Schema $i</th></tr><tr><td>" >>$matrix_file
        j=${h##*/} #schema file name, without dirs
        echo "$j</td></tr><tr><td><table border=1>" >>$matrix_file
        count_we "exp2cxx" $result_dir/fedex_$i $i
        count_we "lib" $result_dir/compile_libsdai_$i $i
        count_we "p21read" $result_dir/compile_p21read_sdai_$i $i
        echo "</table></td></tr>" >>$matrix_file
        echo "</table></td></tr>" >>$matrix_file
#         if [ -s $result_dir/fedex_"$i"_stderr.txt ]; then
#             fedex_details $i
#         fi
        make_schema_page $i $j | sed -e "s/\`/'/g;" >$out_dir/schema_$i.md  # the backtick confuses github's wiki
    done
    echo "</table>" >>$matrix_file
}

# if pwd is build_all, assume everything is up to date and just create wiki pages
if [ ! ${PWD##*/} == "build_all" ]; then
    mkdir -p build_all && cd build_all && rm -rf * && mkdir -p $out_dir && mkdir -p $result_dir
    build
    echo -n "Done building. "
fi

echo "Creating wiki pages..."
gen_wiki
echo "Done!"
echo "Copy everything in build_all/$out_dir to the wiki"
