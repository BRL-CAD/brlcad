#!/usr/bin/env python

#you probably want to run the ctest script that calls this instead:
# ctest -S ctest_matrix.cmake

#must be ran from scl/build_matrix

from xml.etree import ElementTree as ET
import os
from datetime import date
import subprocess
import codecs
import io


#ctest xml file layout
#<Site ...=...>
# <testing>
#  <StartDateTime>..</>
#  <StartTestTime>..</>
#  <TestList>..</>
#  <Test status=..>..</>
#  <EndDateTime>Dec 28 17:49 EST</EndDateTime>
#  <EndTestTime>1325112579</EndTestTime>
#  <ElapsedMinutes>1.9</ElapsedMinutes>
# </Testing>
#</Site>


#summary (aka 's') is a table near the top of the document
#body (aka 'b') contains the details for all schemas

def main():
    xml_file = find_xml()
    wikipath, matrix = find_wiki()
    
    #codecs.open is deprecated but io.open doesn't seem to work, and open() complains of unicode
    out = codecs.open(matrix,encoding='utf-8',mode='w')
    out.write( header() )
    out.write( read_tests(xml_file) )
    out.close()
    git_push(wikipath, matrix)


def find_xml():
    #find xml file
    i = 0
    for dirname in os.listdir("Testing"):
        if str(date.today().year) in dirname:
            i += 1
            if i > 1:
                print "Too many directories, exiting"
                exit(1)
            xml = os.path.join("Testing", dirname, "Test.xml")
    return xml

def find_wiki():
    #find wiki and matrix file, issue 'git pull'
    wikipath = os.path.abspath("../../wiki-scl")
    if not os.path.isdir(os.path.join(wikipath,".git")):
        print "Can't find wiki or not a git repo"
        exit(1)
    p = subprocess.call(["git", "pull", "origin"], cwd=wikipath)
    if not p == 0:
        print "'git pull' exited with error"
        exit(1)
    matrix = os.path.join(wikipath, "Schema-build-matrix.md")
    if not os.path.isfile(matrix):
        print "Matrix file doesn't exist or isn't a file"
        exit(1)
    return wikipath,matrix

def git_push(path,f):
    p = subprocess.call(["git", "add", f], cwd=path)
    if not p == 0:
        print "'git add' exited with error"
        exit(1)
    msg = date.today().__str__() + " - schema matrix updated by update-matrix.py"
    p = subprocess.call(["git", "commit", "-m", msg ], cwd=path)
    if not p == 0:
        print "'git commit' exited with error"
        exit(1)
    p = subprocess.call(["git", "push", "origin"], cwd=path)
    if not p == 0:
        print "'git push' exited with error"
        exit(1)


def header():
    h = "## Created " + date.today().__str__() + "\n" + "### Current as of commit "
    l = subprocess.check_output(["git", "log", """--pretty=format:%H Commit Summary: %s<br>Author: %aN<br>Date: %aD""", "-n1"])
    h += "[" + l[:8] + "](http://github.com/mpictor/StepClassLibrary/commit/" + l[:l.find(" ")]
    h += ") --\n<font color=grey>" + l[l.find(" ")+1:] + "</font>\n\n----\n"
    h += "### Summary\n<table width=100%><tr><th>Schema</th><th>Generate</th><th>Build</th></tr>"
    return h

def read_tests(xml):
    # read all <Test>s in xml, create mixed html/markdown
    try:
        tree = ET.parse(xml)
    except Exception, inst:
        print "Unexpected error opening %s: %s" % (xml, inst)
        return

    root = tree.getroot()
    testing = root.find("Testing")


    tests = testing.findall("Test")
    summary = ""
    body = ""
    for test in tests:
        s,b = schema_info(test,tests)
        summary += s
        body += b
    summary += "</table>\n\n"
    return summary + body



def schema_info(test,tests):
    # this returns html & markdown formatted summary and body strings
    # for the generate and build tests for a single schema
    s=""
    b=""
    name = test.find("Name").text
    if name.startswith("generate_cpp_"):
        #print this one. if it passes, find and print build.
        ap = name[len("generate_cpp_"):]
        s += "<tr><td><a href=#" + ap + ">" + ap.title() + "</a></td><td>"
        s += test_status(test) + "</td><td>"
        b += "----\n<a name=\"wiki-" + ap + "\"></a>\n"
        b += "### Schema " + ap.title()
        b += "<table width=100%>"
        b += test_table("generation",test)
        if test.get("Status") == "passed":
            for build in tests:
                if build.find("Name").text == "build_cpp_sdai_" + ap:
                    s += test_status(build) + "</td></tr>\n"
                    b += test_table("compilation",build)
                    break
        else:
            s += "----</td></tr>\n"
        b += "</table>\n"
    return s,b

def test_table(ttype, test):
    # populate the table for one test
    # returns: html & markdown formatted text to be added to 'body'
    b = "<tr><td>Code " + ttype
    output = test.find("Results").find("Measurement").find("Value").text
    w = output.count("WARNING")
    w += output.count("warning")
    lines = output.split("\n")
    if "The rest of the test output was removed since it exceeds the threshold of" in lines[-2]:
        trunc1 = "at least "
        trunc2 = "(ctest truncated output)"
    else:
        trunc1 = ""
        trunc2 = ""
    if test.get("Status") == "passed":
        #print summary in b
        b += " succeeded with " + trunc1 + w.__str__() + " warnings " + trunc2
        if w == 0:  #nothing to print in the table, so skip it
            b += "</td></tr>\n"
            return b
    else:
        #print warnings and errors in b
        e = output.count("ERROR")
        e += output.count("error")
        b += " failed with %s%d warnings and %d errors %s" %(trunc1, w, e, trunc2)
    b += "<br>\n<table border=1 width=100%>\n"
    b += "<tr><th>Line</th><th>Text</th></tr>\n"

    # ERRORs
    # 242_n2813_mim_lf.exp:2278: --ERROR: Expected a type...
    # gcc errors look like ???
    l=0
    for line in lines:
        if ": --ERROR:" in line:
            l += 1
            c1 = line.find(":")
            c2 = line.find(":",c1+1)
            b += "<tr><td>" + line[c1+1:c2] + "</td><td>" + line[c2+4:] + "</td></tr>\n"
        elif ": error:" in line:
            l += 1
            c1 = line.find(":")
            c2 = line.find(":",c1+1)
            c3 = line.find(":",c2+1) #skip the character number
            b += "<tr><td>" + line[c1+1:c2] + "</td><td>" + line[c3+2:] + "</td></tr>\n"
        if l > 20:
            b += "<tr><td>-</td><td><font color=red>-- maximum number of errors printed --</font></td></tr>\n"
            break
    # WARNINGs
    # ap239_arm_lf.exp:2731: WARNING: Implicit downcast...
    # WARNING: in SELECT TYPE date_or_date... (multi-line warning)
    # compstructs.cc:28:23: warning: unused
    l=0
    for line in lines:
        if ": WARNING" in line:
            l += 1
            c1 = line.find(":")
            c2 = line.find(":",c1+1)
            b += "<tr><td>" + line[c1+1:c2] + "</td><td>" + line[c2+2:] + "</td></tr>\n"
        elif "WARNING" in line:
            b += "<tr><td>????</td><td>" + line + "</td></tr>\n"
        elif ": warning:" in line:
            l += 1
            c1 = line.find(":")
            c2 = line.find(":",c1+1)
            c3 = line.find(":",c2+1) #skip the character number
            b += "<tr><td>" + line[c1+1:c2] + "</td><td>" + line[c3+2:] + "</td></tr>\n"
        if l > 20:
            b += "<tr><td>-</td><td><font color=red>-- maximum number of warnings printed --</font></td></tr>\n"
            break
    b += "</table></td></tr>\n"
    return b

def test_status(test):
    t = ""
    for m in test.find("Results").findall("NamedMeasurement"):
        if m.get("name") == "Execution Time":
            t = " in " + m.find("Value").text + "s"
            break
    if test.get("Status") == "passed":
        s = "<font color=green>PASS</font>"
    elif test.get("Status") == "failed":
        s = "<font color=red>FAIL</font>"
    else:
        s = "<font color=cyan>" + test.get("Status") + "</font>"
    return s + t

if __name__ == "__main__":
    # Someone is launching this directly
    main()
