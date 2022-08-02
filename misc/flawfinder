#!/usr/bin/env python

"""flawfinder: Find potential security flaws ("hits") in source code.
 Usage:
   flawfinder [options] [source_code_file]+

 See the man page for a description of the options."""

# The default output is as follows:
# filename:line_number [risk_level] (type) function_name: message
#   where "risk_level" goes from 0 to 5. 0=no risk, 5=maximum risk.
# The final output is sorted by risk level, most risky first.
# Optionally ":column_number" can be added after the line number.
#
# Currently this program can only analyze C/C++ code.
#
# Copyright (C) 2001-2019 David A. Wheeler.
# This is released under the
# GNU General Public License (GPL) version 2 or later (GPL-2.0+):
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# This code is written to run on both Python 2.7 and Python 3.
# The Python developers did a *terrible* job when they transitioned
# to Python version 3, as I have documented elsewhere.
# Thankfully, more recent versions of Python 3, and the most recent version of
# Python 2, make it possible (though ugly) to write code that runs on both.
# That *finally* makes it possible to semi-gracefully transition.

from __future__ import division
from __future__ import print_function
import functools
import sys
import re
import string
import getopt
import pickle  # To support load/save/diff of hitlist
import os
import glob
import operator  # To support filename expansion on Windows
import time
import csv  # To support generating CSV format
import hashlib
import json

version = "2.0.18"

# Program Options - these are the default values.
# TODO: Switch to boolean types where appropriate.
# We didn't use boolean types originally because this program
# predates Python's PEP 285, which added boolean types to Python 2.3.
# Even after Python 2.3 was released, we wanted to run on older versions.
# That's irrelevant today, but since "it works" there hasn't been a big
# rush to change it.
show_context = 0
minimum_level = 1
show_immediately = 0
show_inputs = 0  # Only show inputs?
falsepositive = 0  # Work to remove false positives?
allowlink = 0  # Allow symbolic links?
skipdotdir = 1  # If 1, don't recurse into dirs beginning with "."
# Note: This doesn't affect the command line.
num_links_skipped = 0  # Number of links skipped.
num_dotdirs_skipped = 0  # Number of dotdirs skipped.
show_columns = 0
never_ignore = 0  # If true, NEVER ignore problems, even if directed.
list_rules = 0  # If true, list the rules (helpful for debugging)
patch_file = ""  # File containing (unified) diff output.
loadhitlist = None
savehitlist = None
diffhitlist_filename = None
quiet = 0
showheading = 1  # --dataonly turns this off
output_format = 0  # 0 = normal, 1 = html.
single_line = 0  # 1 = singleline (can 't be 0 if html)
csv_output = 0  # 1 = Generate CSV
csv_writer = None
sarif_output = 0  # 1 = Generate SARIF report
omit_time = 0  # 1 = omit time-to-run (needed for testing)
required_regex = None  # If non-None, regex that must be met to report
required_regex_compiled = None

ERROR_ON_DISABLED_VALUE = 999
error_level = ERROR_ON_DISABLED_VALUE  # Level where we're return error code
error_level_exceeded = False

displayed_header = 0  # Have we displayed the header yet?
num_ignored_hits = 0  # Number of ignored hits (used if never_ignore==0)


def error(message):
    sys.stderr.write("Error: %s\n" % message)


# Support routines: find a pattern.
# To simplify the calling convention, several global variables are used
# and these support routines are defined, in an attempt to make the
# actual calls simpler and clearer.
#

filename = ""  # Source filename.
linenumber = 0  # Linenumber from original file.
ignoreline = -1  # Line number to ignore.
sumlines = 0  # Number of lines (total) examined.
sloc = 0  # Physical SLOC
starttime = time.time()  # Used to determine analyzed lines/second.


# Send warning message.  This is written this way to work on
# Python version 2.5 through Python 3.
def print_warning(message):
    sys.stderr.write("Warning: ")
    sys.stderr.write(message)
    sys.stderr.write("\n")
    sys.stderr.flush()

def to_json(o):
    return json.dumps(o, default=lambda o: o.__dict__, sort_keys=False, indent=2)


# The following implements the SarifLogger.
# We intentionally merge all of flawfinder's functionality into 1 file
# so it's trivial to copy & use elsewhere.

class SarifLogger(object):
    _hitlist = None
    TOOL_NAME = "Flawfinder"
    TOOL_URL = "https://dwheeler.com/flawfinder/"
    TOOL_VERSION = version
    URI_BASE_ID = "SRCROOT"
    SARIF_SCHEMA = "https://schemastore.azurewebsites.net/schemas/json/sarif-2.1.0-rtm.5.json"
    SARIF_SCHEMA_VERSION = "2.1.0"
    CWE_TAXONOMY_NAME = "CWE"
    CWE_TAXONOMY_URI = "https://raw.githubusercontent.com/sarif-standard/taxonomies/main/CWE_v4.4.sarif"
    CWE_TAXONOMY_GUID = "FFC64C90-42B6-44CE-8BEB-F6B7DAE649E5"

    def __init__ (self, hits):
        self._hitlist = hits

    def output_sarif(self):
        tool = {
            "driver": {
                "name": self.TOOL_NAME,
                "version": self.TOOL_VERSION,
                "informationUri": self.TOOL_URL,
                "rules": self._extract_rules(self._hitlist),
                "supportedTaxonomies": [{
                    "name": self.CWE_TAXONOMY_NAME,
                    "guid": self.CWE_TAXONOMY_GUID,
                }],
            }
        }

        runs = [{
            "tool": tool,
            "columnKind": "utf16CodeUnits",
            "results": self._extract_results(self._hitlist),
            "externalPropertyFileReferences": {
                "taxonomies": [{
                    "location": {
                        "uri": self.CWE_TAXONOMY_URI,
                    },
                    "guid": self.CWE_TAXONOMY_GUID,
                }],
            },
        }]

        report = {
            "$schema": self.SARIF_SCHEMA,
            "version": self.SARIF_SCHEMA_VERSION,
            "runs": runs,
        }

        jsonstr = to_json(report)
        return jsonstr

    def _extract_rules(self, hitlist):
        rules = {}
        for hit in hitlist:
            if not hit.ruleid in rules:
                rules[hit.ruleid] = self._to_sarif_rule(hit)
        return list(rules.values())

    def _extract_results(self, hitlist):
        results = []
        for hit in hitlist:
            results.append(self._to_sarif_result(hit))
        return results

    def _to_sarif_rule(self, hit):
        return {
            "id": hit.ruleid,
            "name": "{0}/{1}".format(hit.category, hit.name),
            "shortDescription": {
                "text": self._append_period(hit.warning),
            },
            "defaultConfiguration": {
                "level": self._to_sarif_level(hit.defaultlevel),
            },
            "helpUri": hit.helpuri(),
            "relationships": self._extract_relationships(hit.cwes()),
        }

    def _to_sarif_result(self, hit):
        return {
            "ruleId": hit.ruleid,
            "level": self._to_sarif_level(hit.level),
            "message": {
                "text": self._append_period("{0}/{1}:{2}".format(hit.category, hit.name, hit.warning)),
            },
            "locations": [{
                "physicalLocation": {
                    "artifactLocation": {
                        "uri": self._to_uri_path(hit.filename),
                        "uriBaseId": self.URI_BASE_ID,
                    },
                    "region": {
                        "startLine": hit.line,
                        "startColumn": hit.column,
                        "endColumn": len(hit.context_text) + 1,
                        "snippet": {
                            "text": hit.context_text,
                        }
                    }
                }
            }],
            "fingerprints": {
                "contextHash/v1": hit.fingerprint()
            },
            "rank": self._to_sarif_rank(hit.level),
        }

    def _extract_relationships(self, cwestring):
        # example cwe string "CWE-119!/ CWE-120", "CWE-829, CWE-20"
        relationships = []
        for cwe in re.split(',|/',cwestring):
            cwestr = cwe.strip()
            if cwestr:
                relationship = {
                    "target": {
                        "id": cwestr.replace("!", ""),
                        "toolComponent": {
                            "name": self.CWE_TAXONOMY_NAME,
                            "guid": self.CWE_TAXONOMY_GUID,
                        },
                    },
                    "kinds": [
                        "relevant" if cwestr[-1] != '!' else "incomparable"
                    ],
                }
                relationships.append(relationship)
        return relationships

    @staticmethod
    def _to_sarif_level(level):
        # level 4 & 5
        if level >= 4:
            return "error"
        # level 3
        if level == 3:
            return "warning"
        # level 0 1 2
        return "note"

    @staticmethod
    def _to_sarif_rank(level):
        #SARIF rank  FF Level    SARIF level Default Viewer Action
        #0.0         0           note        Does not display by default
        #0.2         1           note        Does not display by default
        #0.4         2           note        Does not display by default
        #0.6         3           warning     Displays by default, does not break build / other processes
        #0.8         4           error       Displays by default, breaks build/ other processes
        #1.0         5           error       Displays by default, breaks build/ other processes
        return level * 0.2

    @staticmethod
    def _to_uri_path(path):
        return path.replace("\\", "/")

    @staticmethod
    def _append_period(text):
        return text if text[-1] == '.' else text + "."


# The following code accepts unified diff format from both subversion (svn)
# and GNU diff, which aren't well-documented.  It gets filenames from
# "Index:" if exists, else from the "+++ FILENAME ..." entry.
# Note that this is different than some tools (which will use "+++" in
# preference to "Index:"), but subversion's nonstandard format is easier
# to handle this way.
# Since they aren't well-documented, here's some info on the diff formats:
# GNU diff format:
#    --- OLDFILENAME OLDTIMESTAMP
#    +++ NEWFILENAME NEWTIMESTAMP
#    @@ -OLDSTART,OLDLENGTH +NEWSTART,NEWLENGTH @@
#    ... Changes where preceeding "+" is add, "-" is remove, " " is unchanged.
#
#    ",OLDLENGTH" and ",NEWLENGTH" are optional  (they default to 1).
#    GNU unified diff format doesn't normally output "Index:"; you use
#    the "+++/---" to find them (presuming the diff user hasn't used --label
#    to mess it up).
#
# Subversion format:
#    Index: FILENAME
#    --- OLDFILENAME (comment)
#    +++ NEWFILENAME (comment)
#    @@ -OLDSTART,OLDLENGTH +NEWSTART,NEWLENGTH @@
#
#    In subversion, the "Index:" always occurs, and note that paren'ed
#    comments are in the oldfilename/newfilename, NOT timestamps like
#    everyone else.
#
# Git format:
#    diff --git a/junk.c b/junk.c
#    index 03d668d..5b005a1 100644
#    --- a/junk.c
#    +++ b/junk.c
#    @@ -6,4 +6,5 @@ main() {
#
# Single Unix Spec version 3 (http://www.unix.org/single_unix_specification/)
# does not specify unified format at all; it only defines the older
# (obsolete) context diff format.  That format DOES use "Index:", but
# only when the filename isn't specified otherwise.
# We're only supporting unified format directly; if you have an older diff
# format, use "patch" to apply it, and then use "diff -u" to create a
# unified format.
#

diff_index_filename = re.compile(r'^Index:\s+(?P<filename>.*)')
diff_git_filename = re.compile(r'^diff --git a/.* b/(?P<filename>.*)$')
diff_newfile = re.compile(r'^\+\+\+\s(?P<filename>.*)$')
diff_hunk = re.compile(r'^@@ -\d+(,\d+)?\s+\+(?P<linenumber>\d+)[, ].*@@')
diff_line_added = re.compile(r'^\+[^+].*')
diff_line_del = re.compile(r'^-[^-].*')
# The "+++" newfile entries have the filename, followed by a timestamp
# or " (comment)" postpended.
# Timestamps can be of these forms:
#   2005-04-24 14:21:39.000000000 -0400
#   Mon Mar 10 15:13:12 1997
# Also, "newfile" can have " (comment)" postpended.  Find and eliminate this.
# Note that the expression below is Y10K (and Y100K) ready. :-).
diff_findjunk = re.compile(
    r'^(?P<filename>.*)('
    r'(\s\d\d\d\d+-\d\d-\d\d\s+\d\d:\d[0-9:.]+Z?(\s+[\-\+0-9A-Z]+)?)|'
    r'(\s[A-Za-z][a-z]+\s[A-za-z][a-z]+\s\d+\s\d+:\d[0-9:.]+Z?'
    r'(\s[\-\+0-9]*)?\s\d\d\d\d+)|'
    r'(\s\(.*\)))\s*$'
)


def is_svn_diff(sLine):
    if sLine.find('Index:') != -1:
        return True
    return False


def is_gnu_diff(sLine):
    if sLine.startswith('--- '):
        return True
    return False


def is_git_diff(sLine):
    if sLine.startswith('diff --git a'):
        return True
    return False


def svn_diff_get_filename(sLine):
    return diff_index_filename.match(sLine)


def gnu_diff_get_filename(sLine):
    newfile_match = diff_newfile.match(sLine)
    if newfile_match:
        patched_filename = newfile_match.group('filename').strip()
        # Clean up filename - remove trailing timestamp and/or (comment).
        return diff_findjunk.match(patched_filename)
    return None


def git_diff_get_filename(sLine):
    return diff_git_filename.match(sLine)


# For each file found in the file input_patch_file, keep the
# line numbers of the new file (after patch is applied) which are added.
# We keep this information in a hash table for a quick access later.
#
def load_patch_info(input_patch_file):
    patch = {}
    line_counter = 0
    initial_number = 0
    try:
        hPatch = open(input_patch_file, 'r')
    except BaseException:
        print("Error: failed to open", h(input_patch_file))
        sys.exit(10)

    patched_filename = ""  # Name of new file patched by current hunk.

    sLine = hPatch.readline()
    # Heuristic to determine if it's a svn diff, git diff, or a GNU diff.
    if is_svn_diff(sLine):
        fn_get_filename = svn_diff_get_filename
    elif is_git_diff(sLine):
        fn_get_filename = git_diff_get_filename
    elif is_gnu_diff(sLine):
        fn_get_filename = gnu_diff_get_filename
    else:
        print("Error: Unrecognized patch format")
        sys.exit(11)

    while True:  # Loop-and-half construct.  Read a line, end loop when no more

        # This is really a sequence of if ... elsif ... elsif..., but
        # because Python forbids '=' in conditions, we do it this way.
        filename_match = fn_get_filename(sLine)
        if filename_match:
            patched_filename = filename_match.group('filename').strip()
            if patched_filename in patch:
                error("filename occurs more than once in the patch: %s" %
                      patched_filename)
                sys.exit(12)
            else:
                patch[patched_filename] = {}
        else:
            hunk_match = diff_hunk.match(sLine)
            if hunk_match:
                if patched_filename == "":
                    error(
                        "wrong type of patch file : "
                        "we have a line number without having seen a filename"
                    )
                    sys.exit(13)
                initial_number = hunk_match.group('linenumber')
                line_counter = 0
            else:
                line_added_match = diff_line_added.match(sLine)
                if line_added_match:
                    line_added = line_counter + int(initial_number)
                    patch[patched_filename][line_added] = True
                    # Let's also warn about the lines above and below this one,
                    # so that errors that "leak" into adjacent lines are caught.
                    # Besides, if you're creating a patch, you had to at
                    # least look at adjacent lines,
                    # so you're in a position to fix them.
                    patch[patched_filename][line_added - 1] = True
                    patch[patched_filename][line_added + 1] = True
                    line_counter += 1
                else:
                    line_del_match = diff_line_del.match(sLine)
                    if line_del_match is None:
                        line_counter += 1

        sLine = hPatch.readline()
        if sLine == '':
            break  # Done reading.

    return patch


def htmlize(s):
    # Take s, and return legal (UTF-8) HTML.
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def h(s):
    # htmlize s if we're generating html, otherwise just return s.
    return htmlize(s) if output_format else s


def print_multi_line(text):
    # Print text as multiple indented lines.
    width = 78
    prefix = " "
    starting_position = len(prefix) + 1
    #
    print(prefix, end='')
    position = starting_position
    #
    for w in text.split():
        if len(w) + position >= width:
            print()
            print(prefix, end='')
            position = starting_position
        print(' ', end='')
        print(w, end='')
        position += len(w) + 1


# This matches references to CWE identifiers, so we can HTMLize them.
# We don't refer to CWEs with one digit, so we'll only match on 2+ digits.
link_cwe_pattern = re.compile(r'(CWE-([1-9][0-9]+))([,()!/])')

# This matches the CWE data, including multiple entries.
find_cwe_pattern = re.compile(r'\(CWE-[^)]*\)')


class Hit(object):
    """
    Each instance of Hit is a warning of some kind in a source code file.
    See the rulesets, which define the conditions for triggering a hit.
    Hit is initialized with a tuple containing the following:
      hook: function to call when function name found.
      level: (default) warning level, 0-5. 0=no problem, 5=very risky.
      warning: warning (text saying what's the problem)
      suggestion: suggestion (text suggesting what to do instead)
      category: One of "buffer" (buffer overflow), "race" (race condition),
                "tmpfile" (temporary file creation), "format" (format string).
                Use "" if you don't have a better category.
      url: URL fragment reference.
      other:  A dictionary with other settings.

    Other settings usually set:

      name: function name
      parameter: the function parameters (0th parameter null)
      input: set to 1 if the function inputs from external sources.
      start: start position (index) of the function name (in text)
      end:  end position of the function name (in text)
      filename: name of file
      line: line number in file
      column: column in line in file
      context_text: text surrounding hit
    """

    # Set default values:
    source_position = 2  # By default, the second parameter is the source.
    format_position = 1  # By default, the first parameter is the format.
    input = 0  # By default, this doesn't read input.
    note = ""  # No additional notes.
    filename = ""  # Empty string is filename.
    extract_lookahead = 0  # Normally don't extract lookahead.

    def __init__(self, data):
        hook, level, warning, suggestion, category, url, other, ruleid = data
        self.hook, self.level, self.defaultlevel = hook, level, level
        self.warning, self.suggestion = warning, suggestion
        self.category, self.url = category, url
        self.ruleid = ruleid
        # These will be set later, but I set them here so that
        # analysis tools like PyChecker will know about them.
        self.column = 0
        self.line = 0
        self.name = ""
        self.context_text = ""
        for key in other:
            setattr(self, key, other[key])

    def __getitem__(self, X):  # Define this so this works: "%(line)" % hit
        return getattr(self, X)

    def __eq__(self, other):
        return (self.filename == other.filename
                and self.line == other.line
                and self.column == other.column
                and self.level == other.level
                and self.name == other.name)

    def __ne__(self, other):
        return not self == other

    # Return CWEs
    def cwes(self):
        result = find_cwe_pattern.search(self.warning)
        return result.group()[1:-1] if result else ''

    def fingerprint(self):
        """Return fingerprint of stripped context."""
        m = hashlib.sha256()
        m.update(self.context_text.strip().encode('utf-8'))
        return m.hexdigest()

    # Help uri for each defined rule. e.g. "https://dwheeler.com/flawfinder#FF1002"
    # return first CWE link for now
    def helpuri(self):
        cwe = re.split(',|!', self.cwes())[0] + ")"
        return link_cwe_pattern.sub(
                r'https://cwe.mitre.org/data/definitions/\2.html',
                cwe)

    # Show as CSV format
    def show_csv(self):
        csv_writer.writerow([
            self.filename, self.line, self.column, self.defaultlevel, self.level, self.category,
            self.name, self.warning + ".", self.suggestion + "." if self.suggestion else "", self.note,
            self.cwes(), self.context_text, self.fingerprint(),
            version, self.ruleid, self.helpuri()
        ])

    def show(self):
        if csv_output:
            self.show_csv()
            return
        if sarif_output:
            return
        if output_format:
            print("<li>", end='')
        sys.stdout.write(h(self.filename))

        if show_columns:
            print(":%(line)s:%(column)s:" % self, end='')
        else:
            print(":%(line)s:" % self, end='')

        if output_format:
            print(" <b>", end='')
        # Extra space before risk level in text, makes it easier to find:
        print("  [%(level)s]" % self, end=' ')
        if output_format:
            print("</b> ", end='')
        print("(%(category)s)" % self, end=' ')
        if output_format:
            print("<i> ", end='')
        print(h("%(name)s:" % self), end='')
        main_text = h("%(warning)s. " % self)
        if output_format:  # Create HTML link to CWE definitions
            main_text = link_cwe_pattern.sub(
                r'<a href="https://cwe.mitre.org/data/definitions/\2.html">\1</a>\3',
                main_text)
        if single_line:
            print(main_text, end='')
            if self.suggestion:
                print(" " + h(self.suggestion) + ".", end='')
            print(' ' + h(self.note), end='')
        else:
            if self.suggestion:
                main_text += h(self.suggestion) + ". "
            main_text += h(self.note)
            print()
            print_multi_line(main_text)
        if output_format:
            print(" </i>", end='')
        print()
        if show_context:
            if output_format:
                print("<pre>")
            print(h(self.context_text))
            if output_format:
                print("</pre>")


# The "hitlist" is the list of all hits (warnings) found so far.
# Use add_warning to add to it.

hitlist = []


def add_warning(hit):
    global hitlist, num_ignored_hits
    if show_inputs and not hit.input:
        return
    if required_regex and (required_regex_compiled.search(hit.warning) is
                           None):
        return
    if linenumber == ignoreline:
        num_ignored_hits += 1
    else:
        hitlist.append(hit)
        if show_immediately:
            hit.show()


def internal_warn(message):
    print(h(message))


# C Language Specific


def extract_c_parameters(text, pos=0):
    "Return a list of the given C function's parameters, starting at text[pos]"
    # '(a,b)' produces ['', 'a', 'b']
    i = pos
    # Skip whitespace and find the "("; if there isn't one, return []:
    while i < len(text):
        if text[i] == '(':
            break
        elif text[i] in string.whitespace:
            i += 1
        else:
            return []
    else:  # Never found a reasonable ending.
        return []
    i += 1
    parameters = [""]  # Insert 0th entry, so 1st parameter is parameter[1].
    currentstart = i
    parenlevel = 1
    curlylevel = 0
    instring = 0  # 1=in double-quote, 2=in single-quote
    incomment = 0
    while i < len(text):
        c = text[i]
        if instring:
            if c == '"' and instring == 1:
                instring = 0
            elif c == "'" and instring == 2:
                instring = 0
                # if \, skip next character too.  The C/C++ rules for
                # \ are actually more complex, supporting \ooo octal and
                # \xhh hexadecimal (which can be shortened),
                # but we don't need to
                # parse that deeply, we just need to know we'll stay
                # in string mode:
            elif c == '\\':
                i += 1
        elif incomment:
            if c == '*' and text[i:i + 2] == '*/':
                incomment = 0
                i += 1
        else:
            if c == '"':
                instring = 1
            elif c == "'":
                instring = 2
            elif c == '/' and text[i:i + 2] == '/*':
                incomment = 1
                i += 1
            elif c == '/' and text[i:i + 2] == '//':
                while i < len(text) and text[i] != "\n":
                    i += 1
            elif c == '\\' and text[i:i + 2] == '\\"':
                i += 1  # Handle exposed '\"'
            elif c == '(':
                parenlevel += 1
            elif c == ',' and (parenlevel == 1):
                parameters.append(
                    p_trailingbackslashes.sub('', text[currentstart:i]).strip())
                currentstart = i + 1
            elif c == ')':
                parenlevel -= 1
                if parenlevel <= 0:
                    parameters.append(
                        p_trailingbackslashes.sub(
                            '', text[currentstart:i]).strip())
                    # Re-enable these for debugging:
                    # print " EXTRACT_C_PARAMETERS: ", text[pos:pos+80]
                    # print " RESULTS: ", parameters
                    return parameters
            elif c == '{':
                curlylevel += 1
            elif c == '}':
                curlylevel -= 1
            elif c == ';' and curlylevel < 1:
                internal_warn(
                    "Parsing failed to find end of parameter list; "
                    "semicolon terminated it in %s" % text[pos:pos + 200])
                return parameters
        i += 1
    internal_warn("Parsing failed to find end of parameter list in %s" %
                  text[pos:pos + 200])
    return []  # Treat unterminated list as an empty list

# These patterns match gettext() and _() for internationalization.
# This is compiled here, to avoid constant recomputation.
# FIXME: assumes simple function call if it ends with ")",
# so will get confused by patterns like  gettext("hi") + function("bye")
# In practice, this doesn't seem to be a problem; gettext() is usually
# wrapped around the entire parameter.
# The ?s makes it posible to match multi-line strings.
gettext_pattern = re.compile(r'(?s)^\s*' 'gettext' r'\s*\((.*)\)\s*$')
undersc_pattern = re.compile(r'(?s)^\s*' '_(T(EXT)?)?' r'\s*\((.*)\)\s*$')


def strip_i18n(text):
    """Strip any internationalization function calls surrounding 'text'.

    In particular, strip away calls to gettext() and _().
    """
    match = gettext_pattern.search(text)
    if match:
        return match.group(1).strip()
    match = undersc_pattern.search(text)
    if match:
        return match.group(3).strip()
    return text


p_trailingbackslashes = re.compile(r'(\s|\\(\n|\r))*$')

p_c_singleton_string = re.compile(r'^\s*L?"([^\\]|\\[^0-6]|\\[0-6]+)?"\s*$')


def c_singleton_string(text):
    "Returns true if text is a C string with 0 or 1 character."
    return 1 if p_c_singleton_string.search(text) else 0


# This string defines a C constant.
p_c_constant_string = re.compile(r'^\s*L?"([^\\]|\\[^0-6]|\\[0-6]+)*"$')


def c_constant_string(text):
    "Returns true if text is a constant C string."
    return 1 if p_c_constant_string.search(text) else 0

# Precompile patterns for speed.

p_memcpy_sizeof = re.compile(r'sizeof\s*\(\s*([^)\s]*)\s*\)')
p_memcpy_param_amp = re.compile(r'&?\s*(.*)')

def c_memcpy(hit):
    if len(hit.parameters) < 4: # 3 parameters
        add_warning(hit)
        return

    m1 = re.search(p_memcpy_param_amp, hit.parameters[1])
    m3 = re.search(p_memcpy_sizeof, hit.parameters[3])
    if not m1 or not m3 or m1.group(1) != m3.group(1):
        add_warning(hit)


def c_buffer(hit):
    source_position = hit.source_position
    if source_position <= len(hit.parameters) - 1:
        source = hit.parameters[source_position]
        if c_singleton_string(source):
            hit.level = 1
            hit.note = "Risk is low because the source is a constant character."
        elif c_constant_string(strip_i18n(source)):
            hit.level = max(hit.level - 2, 1)
            hit.note = "Risk is low because the source is a constant string."
    add_warning(hit)


p_dangerous_strncat = re.compile(r'^\s*sizeof\s*(\(\s*)?[A-Za-z_$0-9]+'
                                 r'\s*(\)\s*)?(-\s*1\s*)?$')
# This is a heuristic: constants in C are usually given in all
# upper case letters.  Yes, this need not be true, but it's true often
# enough that it's worth using as a heuristic.
# We check because strncat better not be passed a constant as the length!
p_looks_like_constant = re.compile(r'^\s*[A-Z][A-Z_$0-9]+\s*(-\s*1\s*)?$')


def c_strncat(hit):
    if len(hit.parameters) > 3:
        # A common mistake is to think that when calling strncat(dest,src,len),
        # that "len" means the ENTIRE length of the destination.
        # This isn't true,
        # it must be the length of the characters TO BE ADDED at most.
        # Which is one reason that strlcat is better than strncat.
        # We'll detect a common case of this error; if the length parameter
        # is of the form "sizeof(dest)", we have this error.
        # Actually, sizeof(dest) is okay if the dest's first character
        # is always \0,
        # but in that case the programmer should use strncpy, NOT strncat.
        # The following heuristic will certainly miss some dangerous cases, but
        # it at least catches the most obvious situation.
        # This particular heuristic is overzealous; it detects ANY sizeof,
        # instead of only the sizeof(dest) (where dest is given in
        # hit.parameters[1]).
        # However, there aren't many other likely candidates for sizeof; some
        # people use it to capture just the length of the source, but this is
        # just as dangerous, since then it absolutely does NOT take care of
        # the destination maximum length in general.
        # It also detects if a constant is given as a length, if the
        # constant follows common C naming rules.
        length_text = hit.parameters[3]
        if p_dangerous_strncat.search(
                length_text) or p_looks_like_constant.search(length_text):
            hit.level = 5
            hit.note = (
                "Risk is high; the length parameter appears to be a constant, "
                "instead of computing the number of characters left.")
            add_warning(hit)
            return
    c_buffer(hit)


def c_printf(hit):
    format_position = hit.format_position
    if format_position <= len(hit.parameters) - 1:
        # Assume that translators are trusted to not insert "evil" formats:
        source = strip_i18n(hit.parameters[format_position])
        if c_constant_string(source):
            # Parameter is constant, so there's no risk of
            # format string problems.
            # At one time we warned that very old systems sometimes incorrectly
            # allow buffer overflows on snprintf/vsnprintf, but those systems
            # are now very old, and snprintf is an important potential tool for
            # countering buffer overflows.
            # We'll pass it on, just in case it's needed, but at level 0 risk.
            hit.level = 0
            hit.note = "Constant format string, so not considered risky."
    add_warning(hit)


p_dangerous_sprintf_format = re.compile(r'%-?([0-9]+|\*)?s')


# sprintf has both buffer and format vulnerabilities.
def c_sprintf(hit):
    source_position = hit.source_position
    if hit.parameters is None:
        # Serious parameter problem, e.g., none, or a string constant that
        # never finishes.
        hit.warning = "format string parameter problem"
        hit.suggestion = "Check if required parameters present and quotes close."
        hit.level = 4
        hit.category = "format"
        hit.url = ""
    elif source_position <= len(hit.parameters) - 1:
        source = hit.parameters[source_position]
        if c_singleton_string(source):
            hit.level = 1
            hit.note = "Risk is low because the source is a constant character."
        else:
            source = strip_i18n(source)
            if c_constant_string(source):
                if not p_dangerous_sprintf_format.search(source):
                    hit.level = max(hit.level - 2, 1)
                    hit.note = "Risk is low because the source has a constant maximum length."
                # otherwise, warn of potential buffer overflow (the default)
            else:
                # Ho ho - a nonconstant format string - we have a different
                # problem.
                hit.warning = "Potential format string problem (CWE-134)"
                hit.suggestion = "Make format string constant"
                hit.level = 4
                hit.category = "format"
                hit.url = ""
    add_warning(hit)


p_dangerous_scanf_format = re.compile(r'%s')
p_low_risk_scanf_format = re.compile(r'%[0-9]+s')


def c_scanf(hit):
    format_position = hit.format_position
    if format_position <= len(hit.parameters) - 1:
        # Assume that translators are trusted to not insert "evil" formats;
        # it's not clear that translators will be messing with INPUT formats,
        # but it's possible so we'll account for it.
        source = strip_i18n(hit.parameters[format_position])
        if c_constant_string(source):
            if p_dangerous_scanf_format.search(source):
                pass  # Accept default.
            elif p_low_risk_scanf_format.search(source):
                # This is often okay, but sometimes extremely serious.
                hit.level = 1
                hit.warning = ("It's unclear if the %s limit in the "
                               "format string is small enough (CWE-120)")
                hit.suggestion = ("Check that the limit is sufficiently "
                                  "small, or use a different input function")
            else:
                # No risky scanf request.
                # We'll pass it on, just in case it's needed, but at level 0
                # risk.
                hit.level = 0
                hit.note = "No risky scanf format detected."
        else:
            # Format isn't a constant.
            hit.note = ("If the scanf format is influenceable "
                        "by an attacker, it's exploitable.")
    add_warning(hit)


p_dangerous_multi_byte = re.compile(r'^\s*sizeof\s*(\(\s*)?[A-Za-z_$0-9]+'
                                    r'\s*(\)\s*)?(-\s*1\s*)?$')
p_safe_multi_byte = re.compile(
    r'^\s*sizeof\s*(\(\s*)?[A-Za-z_$0-9]+\s*(\)\s*)?'
    r'/\s*sizeof\s*\(\s*?[A-Za-z_$0-9]+\s*\[\s*0\s*\]\)\s*(-\s*1\s*)?$')


def c_multi_byte_to_wide_char(hit):
    # Unfortunately, this doesn't detect bad calls when it's a #define or
    # constant set by a sizeof(), but trying to do so would create
    # FAR too many false positives.
    if len(hit.parameters) - 1 >= 6:
        num_chars_to_copy = hit.parameters[6]
        if p_dangerous_multi_byte.search(num_chars_to_copy):
            hit.level = 5
            hit.note = (
                "Risk is high, it appears that the size is given as bytes, but the "
                "function requires size as characters.")
        elif p_safe_multi_byte.search(num_chars_to_copy):
            # This isn't really risk-free, since it might not be the destination,
            # or the destination might be a character array (if it's a char pointer,
            # the pattern is actually quite dangerous, but programmers
            # are unlikely to make that error).
            hit.level = 1
            hit.note = "Risk is very low, the length appears to be in characters not bytes."
    add_warning(hit)


p_null_text = re.compile(r'^ *(NULL|0|0x0) *$')


def c_hit_if_null(hit):
    null_position = hit.check_for_null
    if null_position <= len(hit.parameters) - 1:
        null_text = hit.parameters[null_position]
        if p_null_text.search(null_text):
            add_warning(hit)
        else:
            return
    add_warning(hit)  # If insufficient # of parameters.


p_static_array = re.compile(r'^[A-Za-z_]+\s+[A-Za-z0-9_$,\s\*()]+\[[^]]')


def c_static_array(hit):
    # This is cheating, but it does the job for most real code.
    # In some cases it will match something that it shouldn't.
    # We don't match ALL arrays, just those of certain types (e.g., char).
    # In theory, any array can overflow, but in practice it seems that
    # certain types are far more prone to problems, so we just report those.
    if p_static_array.search(hit.lookahead):
        add_warning(hit)  # Found a static array, warn about it.


def cpp_unsafe_stl(hit):
    # Use one of the overloaded classes from the STL in C++14 and higher
    # instead of the <C++14 versions of theses functions that did not
    # if the second iterator could overflow
    if len(hit.parameters) <= 4:
        add_warning(hit)

safe_load_library_flags = [
    # Load only from the folder where the .exe file is located
    'LOAD_LIBRARY_SEARCH_APPLICATION_DIR',
    # Combination of application, System32 and user directories
    'LOAD_LIBRARY_SEARCH_DEFAULT_DIRS',
    # This flag requires an absolute path to the DLL to be passed
    'LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR',
    # Load only from System32
    'LOAD_LIBRARY_SEARCH_SYSTEM32',
    # Load only from directories specified with AddDllDirectory
    # or SetDllDirectory
    'LOAD_LIBRARY_SEARCH_USER_DIRS',
    # Loading from the current directory will only proceed if
    # the current directory is part of the safe load list
    'LOAD_LIBRARY_SAFE_CURRENT_DIRS'
]

def load_library_ex(hit):
    # If parameter 3 has one of the flags below, it's safe.
    if (len(hit.parameters) >= 4 and
            any(flag in hit.parameters[3] for flag in safe_load_library_flags)):
        return
    normal(hit)

def normal(hit):
    add_warning(hit)

# Ignore "system" if it's "system::" (that is, a C++ namespace such as
# boost::system::...), because that produces too many false positives.
# We ignore spaces before "::"
def found_system(hit):
    follow_text = hit.lookahead[len(hit.name):].lstrip()
    if not follow_text.startswith('::'):
        normal(hit)

# "c_ruleset": the rules for identifying "hits" in C (potential warnings).
# It's a dictionary, where the key is the function name causing the hit,
# and the value is a tuple with the following format:
#  (hook, level, warning, suggestion, category, {other})
# See the definition for class "Hit".
# The key can have multiple values separated with "|".

# For more information on Microsoft banned functions, see:
# http://msdn.microsoft.com/en-us/library/bb288454.aspx

c_ruleset = {
    "strcpy":
    (c_buffer, 4,
     "Does not check for buffer overflows when copying to destination [MS-banned] (CWE-120)",
     "Consider using snprintf, strcpy_s, or strlcpy (warning: strncpy easily misused)",
     "buffer", "", {}, "FF1001"),

    "strcpyA|strcpyW|StrCpy|StrCpyA|lstrcpyA|lstrcpyW|_tccpy|_mbccpy|_ftcscpy|_mbsncpy|StrCpyN|StrCpyNA|StrCpyNW|StrNCpy|strcpynA|StrNCpyA|StrNCpyW|lstrcpynA|lstrcpynW":
    # We need more info on these functions; I got their names from the
    # Microsoft "banned" list.  For now, just use "normal" to process them
    # instead of "c_buffer".
    (normal, 4,
     "Does not check for buffer overflows when copying to destination [MS-banned] (CWE-120)",
     "Consider using snprintf, strcpy_s, or strlcpy (warning: strncpy easily misused)",
     "buffer", "", {}, "FF1002"),

    "lstrcpy|wcscpy|_tcscpy|_mbscpy":
    (c_buffer, 4,
     "Does not check for buffer overflows when copying to destination [MS-banned] (CWE-120)",
     "Consider using a function version that stops copying at the end of the buffer",
     "buffer", "", {}, "FF1003"),

    "memcpy|CopyMemory|bcopy":
    (c_memcpy, 2,  # I've found this to have a lower risk in practice.
     "Does not check for buffer overflows when copying to destination (CWE-120)",
     "Make sure destination can always hold the source data",
     "buffer", "", {}, "FF1004"),

    "strcat":
    (c_buffer, 4,
     "Does not check for buffer overflows when concatenating to destination [MS-banned] (CWE-120)",
     "Consider using strcat_s, strncat, strlcat, or snprintf (warning: strncat is easily misused)",
     "buffer", "", {}, "FF1005"),

    "lstrcat|wcscat|_tcscat|_mbscat":
    (c_buffer, 4,
     "Does not check for buffer overflows when concatenating to destination [MS-banned] (CWE-120)",
     "",
     "buffer", "", {}, "FF1006"),

    # TODO: Do more analysis.  Added because they're in MS banned list.
    "StrCat|StrCatA|StrcatW|lstrcatA|lstrcatW|strCatBuff|StrCatBuffA|StrCatBuffW|StrCatChainW|_tccat|_mbccat|_ftcscat|StrCatN|StrCatNA|StrCatNW|StrNCat|StrNCatA|StrNCatW|lstrncat|lstrcatnA|lstrcatnW":
    (normal, 4,
     "Does not check for buffer overflows when concatenating to destination [MS-banned] (CWE-120)",
     "",
     "buffer", "", {}, "FF1007"),

    "strncpy":
    (c_buffer,
     1,  # Low risk level, because this is often used correctly when FIXING security
     # problems, and raising it to a higher risk level would cause many false
     # positives.
     "Easily used incorrectly; doesn't always \\0-terminate or "
     "check for invalid pointers [MS-banned] (CWE-120)",
     "",
     "buffer", "", {}, "FF1008"),

    "lstrcpyn|wcsncpy|_tcsncpy|_mbsnbcpy":
    (c_buffer,
     1,  # Low risk level, because this is often used correctly when FIXING security
     # problems, and raising it to a higher risk levle would cause many false
     # positives.
     "Easily used incorrectly; doesn't always \\0-terminate or "
     "check for invalid pointers [MS-banned] (CWE-120)",
     "",
     "buffer", "", {}, "FF1009"),

    "strncat":
    (c_strncat,
     1,  # Low risk level, because this is often used correctly when
     # FIXING security problems, and raising it to a
     # higher risk level would cause many false positives.
     "Easily used incorrectly (e.g., incorrectly computing the correct maximum size to add) [MS-banned] (CWE-120)",
     "Consider strcat_s, strlcat, snprintf, or automatically resizing strings",
     "buffer", "", {}, "FF1010"),

    "lstrcatn|wcsncat|_tcsncat|_mbsnbcat":
    (c_strncat,
     1,  # Low risk level, because this is often used correctly when FIXING security
     # problems, and raising it to a higher risk level would cause many false
     # positives.
     "Easily used incorrectly (e.g., incorrectly computing the correct maximum size to add) [MS-banned] (CWE-120)",
     "Consider strcat_s, strlcat, or automatically resizing strings",
     "buffer", "", {}, "FF1011"),

    "strccpy|strcadd":
    (normal, 1,
     "Subject to buffer overflow if buffer is not as big as claimed (CWE-120)",
     "Ensure that destination buffer is sufficiently large",
     "buffer", "", {}, "FF1012"),

    "char|TCHAR|wchar_t":  # This isn't really a function call, but it works.
    (c_static_array, 2,
     "Statically-sized arrays can be improperly restricted, "
     "leading to potential overflows or other issues (CWE-119!/CWE-120)",
     "Perform bounds checking, use functions that limit length, "
     "or ensure that the size is larger than the maximum possible length",
     "buffer", "", {'extract_lookahead': 1}, "FF1013"),

    "gets|_getts":
    (normal, 5, "Does not check for buffer overflows (CWE-120, CWE-20)",
     "Use fgets() instead", "buffer", "", {'input': 1}, "FF1014"),

    # The "sprintf" hook will raise "format" issues instead if appropriate:
    "sprintf|vsprintf|swprintf|vswprintf|_stprintf|_vstprintf":
    (c_sprintf, 4,
     "Does not check for buffer overflows (CWE-120)",
     "Use sprintf_s, snprintf, or vsnprintf",
     "buffer", "", {}, "FF1015"),

    "printf|vprintf|vwprintf|vfwprintf|_vtprintf|wprintf":
    (c_printf, 4,
     "If format strings can be influenced by an attacker, they can be exploited (CWE-134)",
     "Use a constant for the format specification",
     "format", "", {}, "FF1016"),

    "fprintf|vfprintf|_ftprintf|_vftprintf|fwprintf|fvwprintf":
    (c_printf, 4,
     "If format strings can be influenced by an attacker, they can be exploited (CWE-134)",
     "Use a constant for the format specification",
     "format", "", {'format_position': 2}, "FF1017"),

    # The "syslog" hook will raise "format" issues.
    "syslog":
    (c_printf, 4,
     "If syslog's format strings can be influenced by an attacker, "
     "they can be exploited (CWE-134)",
     "Use a constant format string for syslog",
     "format", "", {'format_position': 2}, "FF1018"),

    "snprintf|vsnprintf|_snprintf|_sntprintf|_vsntprintf":
    (c_printf, 4,
     "If format strings can be influenced by an attacker, they can be "
     "exploited, and note that sprintf variations do not always \\0-terminate (CWE-134)",
     "Use a constant for the format specification",
     "format", "", {'format_position': 3}, "FF1019"),

    "scanf|vscanf|wscanf|_tscanf|vwscanf":
    (c_scanf, 4,
     "The scanf() family's %s operation, without a limit specification, "
     "permits buffer overflows (CWE-120, CWE-20)",
     "Specify a limit to %s, or use a different input function",
     "buffer", "", {'input': 1}, "FF1020"),

    "fscanf|sscanf|vsscanf|vfscanf|_ftscanf|fwscanf|vfwscanf|vswscanf":
    (c_scanf, 4,
     "The scanf() family's %s operation, without a limit specification, "
     "permits buffer overflows (CWE-120, CWE-20)",
     "Specify a limit to %s, or use a different input function",
     "buffer", "", {'input': 1, 'format_position': 2}, "FF1021"),

    "strlen|wcslen|_tcslen|_mbslen":
    (normal,
     # Often this isn't really a risk, and even when it is, at worst it
     # often causes a program crash (and nothing worse).
     1,
     "Does not handle strings that are not \\0-terminated; "
     "if given one it may perform an over-read (it could cause a crash "
     "if unprotected) (CWE-126)",
     "",
     "buffer", "", {}, "FF1022"),

    "MultiByteToWideChar":  # Windows
    (c_multi_byte_to_wide_char,
     2,  # Only the default - this will be changed in many cases.
     "Requires maximum length in CHARACTERS, not bytes (CWE-120)",
     "",
     "buffer", "", {}, "FF1023"),

    "streadd|strecpy":
    (normal, 4,
     "This function does not protect against buffer overflows (CWE-120)",
     "Ensure the destination has 4 times the size of the source, to leave room for expansion",
     "buffer", "dangers-c", {}, "FF1024"),

    "strtrns":
    (normal, 3,
     "This function does not protect against buffer overflows (CWE-120)",
     "Ensure that destination is at least as long as the source",
     "buffer", "dangers-c", {}, "FF1025"),

    "realpath":
    (normal, 3,
     "This function does not protect against buffer overflows, "
     "and some implementations can overflow internally (CWE-120/CWE-785!)",
     "Ensure that the destination buffer is at least of size MAXPATHLEN, and"
     "to protect against implementation problems, the input argument "
     "should also be checked to ensure it is no larger than MAXPATHLEN",
     "buffer", "dangers-c", {}, "FF1026"),

    "getopt|getopt_long":
    (normal, 3,
     "Some older implementations do not protect against internal buffer overflows (CWE-120, CWE-20)",
     "Check implementation on installation, or limit the size of all string inputs",
     "buffer", "dangers-c", {'input': 1}, "FF1027"),

    "getwd":
    (normal, 3,
     "This does not protect against buffer overflows "
     "by itself, so use with caution (CWE-120, CWE-20)",
     "Use getcwd instead",
     "buffer", "dangers-c", {'input': 1}, "FF1028"),

    # fread not included here; in practice I think it's rare to mistake it.
    "getchar|fgetc|getc|read|_gettc":
    (normal, 1,
     "Check buffer boundaries if used in a loop including recursive loops (CWE-120, CWE-20)",
     "",
     "buffer", "dangers-c", {'input': 1}, "FF1029"),

    "access":        # ???: TODO: analyze TOCTOU more carefully.
    (normal, 4,
     "This usually indicates a security flaw.  If an "
     "attacker can change anything along the path between the "
     "call to access() and the file's actual use (e.g., by moving "
     "files), the attacker can exploit the race condition (CWE-362/CWE-367!)",
     "Set up the correct permissions (e.g., using setuid()) and "
     "try to open the file directly",
     "race",
     "avoid-race#atomic-filesystem", {}, "FF1030"),

    "chown":
    (normal, 5,
     "This accepts filename arguments; if an attacker "
     "can move those files, a race condition results. (CWE-362)",
     "Use fchown( ) instead",
     "race", "", {}, "FF1031"),

    "chgrp":
    (normal, 5,
     "This accepts filename arguments; if an attacker "
     "can move those files, a race condition results. (CWE-362)",
     "Use fchgrp( ) instead",
     "race", "", {}, "FF1032"),

    "chmod":
    (normal, 5,
     "This accepts filename arguments; if an attacker "
     "can move those files, a race condition results. (CWE-362)",
     "Use fchmod( ) instead",
     "race", "", {}, "FF1033"),

    "vfork":
    (normal, 2,
     "On some old systems, vfork() permits race conditions, and it's "
     "very difficult to use correctly (CWE-362)",
     "Use fork() instead",
     "race", "", {}, "FF1034"),

    "readlink":
    (normal, 5,
     "This accepts filename arguments; if an attacker "
     "can move those files or change the link content, "
     "a race condition results.  "
     "Also, it does not terminate with ASCII NUL. (CWE-362, CWE-20)",
     # This is often just a bad idea, and it's hard to suggest a
     # simple alternative:
     "Reconsider approach",
     "race", "", {'input': 1}, "FF1035"),

    "tmpfile":
    (normal, 2,
     "Function tmpfile() has a security flaw on some systems (e.g., older System V systems) (CWE-377)",
     "",
     "tmpfile", "", {}, "FF1036"),

    "tmpnam|tempnam":
    (normal, 3,
     "Temporary file race condition (CWE-377)",
     "",
     "tmpfile", "avoid-race", {}, "FF1037"),

    # TODO: Detect GNOME approach to mktemp and ignore it.
    "mktemp":
    (normal, 4,
     "Temporary file race condition (CWE-377)",
     "",
     "tmpfile", "avoid-race", {}, "FF1038"),

    "mkstemp":
    (normal, 2,
     "Potential for temporary file vulnerability in some circumstances.  Some older Unix-like systems create temp files with permission to write by all by default, so be sure to set the umask to override this. Also, some older Unix systems might fail to use O_EXCL when opening the file, so make sure that O_EXCL is used by the library (CWE-377)",
     "",
     "tmpfile", "avoid-race", {}, "FF1039"),

    "fopen|open":
    (normal, 2,
     "Check when opening files - can an attacker redirect it (via symlinks), force the opening of special file type (e.g., device files), move things around to create a race condition, control its ancestors, or change its contents? (CWE-362)",
     "",
     "misc", "", {}, "FF1040"),

    "umask":
    (normal, 1,
     "Ensure that umask is given most restrictive possible setting (e.g., 066 or 077) (CWE-732)",
     "",
     "access", "", {}, "FF1041"),

    # Windows.  TODO: Detect correct usage approaches and ignore it.
    "GetTempFileName":
    (normal, 3,
     "Temporary file race condition in certain cases "
     "(e.g., if run as SYSTEM in many versions of Windows) (CWE-377)",
     "",
     "tmpfile", "avoid-race", {}, "FF1042"),

    # TODO: Need to detect varying levels of danger.
    "execl|execlp|execle|execv|execvp|popen|WinExec|ShellExecute":
    (normal, 4,
     "This causes a new program to execute and is difficult to use safely (CWE-78)",
     "try using a library call that implements the same functionality "
     "if available",
     "shell", "", {}, "FF1043"),

    # TODO: Need to detect varying levels of danger.
    "system":
    (found_system, 4,
     "This causes a new program to execute and is difficult to use safely (CWE-78)",
     "try using a library call that implements the same functionality "
     "if available",
     "shell", "", {'extract_lookahead': 1}, "FF1044"),

    # TODO: Be more specific.  The biggest problem involves "first" param NULL,
    # second param with embedded space. Windows.
    "CreateProcessAsUser|CreateProcessWithLogon":
    (normal, 3,
     "This causes a new process to execute and is difficult to use safely (CWE-78)",
     "Especially watch out for embedded spaces",
     "shell", "", {}, "FF1045"),

    # TODO: Be more specific.  The biggest problem involves "first" param NULL,
    # second param with embedded space. Windows.
    "CreateProcess":
    (c_hit_if_null, 3,
     "This causes a new process to execute and is difficult to use safely (CWE-78)",
     "Specify the application path in the first argument, NOT as part of the second, "
     "or embedded spaces could allow an attacker to force a different program to run",
     "shell", "", {'check_for_null': 1}, "FF1046"),

    "atoi|atol|_wtoi|_wtoi64":
    (normal, 2,
     "Unless checked, the resulting number can exceed the expected range "
     "(CWE-190)",
     "If source untrusted, check both minimum and maximum, even if the"
     " input had no minus sign (large numbers can roll over into negative"
     " number; consider saving to an unsigned value if that is intended)",
     "integer", "dangers-c", {}, "FF1047"),

    # Random values.  Don't trigger on "initstate", it's too common a term.
    "drand48|erand48|jrand48|lcong48|lrand48|mrand48|nrand48|random|seed48|setstate|srand|strfry|srandom|g_rand_boolean|g_rand_int|g_rand_int_range|g_rand_double|g_rand_double_range|g_random_boolean|g_random_int|g_random_int_range|g_random_double|g_random_double_range":
    (normal, 3,
     "This function is not sufficiently random for security-related functions such as key and nonce creation (CWE-327)",
     "Use a more secure technique for acquiring random values",
     "random", "", {}, "FF1048"),

    "crypt|crypt_r":
    (normal, 4,
     "The crypt functions use a poor one-way hashing algorithm; "
     "since they only accept passwords of 8 characters or fewer "
     "and only a two-byte salt, they are excessively vulnerable to "
     "dictionary attacks given today's faster computing equipment (CWE-327)",
     "Use a different algorithm, such as SHA-256, with a larger, "
     "non-repeating salt",
     "crypto", "", {}, "FF1049"),

    # OpenSSL EVP calls to use DES.
    "EVP_des_ecb|EVP_des_cbc|EVP_des_cfb|EVP_des_ofb|EVP_desx_cbc":
    (normal, 4,
     "DES only supports a 56-bit keysize, which is too small given today's computers (CWE-327)",
     "Use a different patent-free encryption algorithm with a larger keysize, "
     "such as 3DES or AES",
     "crypto", "", {}, "FF1050"),

    # Other OpenSSL EVP calls to use small keys.
    "EVP_rc4_40|EVP_rc2_40_cbc|EVP_rc2_64_cbc":
    (normal, 4,
     "These keysizes are too small given today's computers (CWE-327)",
     "Use a different patent-free encryption algorithm with a larger keysize, "
     "such as 3DES or AES",
     "crypto", "", {}, "FF1051"),

    "chroot":
    (normal, 3,
     "chroot can be very helpful, but is hard to use correctly (CWE-250, CWE-22)",
     "Make sure the program immediately chdir(\"/\"), closes file descriptors,"
     " and drops root privileges, and that all necessary files"
     " (and no more!) are in the new root",
     "misc", "", {}, "FF1052"),

    "getenv|curl_getenv":
    (normal, 3, "Environment variables are untrustable input if they can be"
     " set by an attacker.  They can have any content and"
     " length, and the same variable can be set more than once (CWE-807, CWE-20)",
     "Check environment variables carefully before using them",
     "buffer", "", {'input': 1}, "FF1053"),

    "g_get_home_dir":
    (normal, 3, "This function is synonymous with 'getenv(\"HOME\")';"
     "it returns untrustable input if the environment can be"
     "set by an attacker.  It can have any content and length, "
     "and the same variable can be set more than once (CWE-807, CWE-20)",
     "Check environment variables carefully before using them",
     "buffer", "", {'input': 1}, "FF1054"),

    "g_get_tmp_dir":
    (normal, 3, "This function is synonymous with 'getenv(\"TMP\")';"
     "it returns untrustable input if the environment can be"
     "set by an attacker.  It can have any content and length, "
     "and the same variable can be set more than once (CWE-807, CWE-20)",
     "Check environment variables carefully before using them",
     "buffer", "", {'input': 1}, "FF1055"),


    # These are Windows-unique:

    # TODO: Should have lower risk if the program checks return value.
    "RpcImpersonateClient|ImpersonateLoggedOnUser|CoImpersonateClient|"
    "ImpersonateNamedPipeClient|ImpersonateDdeClientWindow|ImpersonateSecurityContext|"
    "SetThreadToken":
    (normal, 4, "If this call fails, the program could fail to drop heightened privileges (CWE-250)",
     "Make sure the return value is checked, and do not continue if a failure is reported",
     "access", "", {}, "FF1056"),

    "InitializeCriticalSection":
    (normal, 3, "Exceptions can be thrown in low-memory situations",
     "Use InitializeCriticalSectionAndSpinCount instead",
     "misc", "", {}, "FF1057"),

    # We have *removed* the check for EnterCriticalSection.
    # The page from the "book Writing Secure Code" describes
    # EnterCriticalSection as something that will not throw errors on XP,
    # .NET Server, and later. Windows XP EOL in April 8, 2014,
    # .Net Server EOL 14 July 2015, so users of those systems will have
    # larger security issues anyway.
    # My thanks to rgetz for reporting this. For details, see:
    # https://github.com/david-a-wheeler/flawfinder/issues/19
    # "EnterCriticalSection":
    # (normal, 3, "On some versions of Windows, exceptions can be thrown in low-memory situations",
    #  "Use InitializeCriticalSectionAndSpinCount instead",
    #  "misc", "", {}),

    "LoadLibrary":
    (normal, 3, "Ensure that the full path to the library is specified, or current directory may be used (CWE-829, CWE-20)",
     "Use LoadLibraryEx with one of the search flags, or call SetSearchPathMode to use a safe search path, or pass a full path to the library",
     "misc", "", {'input': 1}, "FF1058"),

    "LoadLibraryEx":
    (load_library_ex, 3, "Ensure that the full path to the library is specified, or current directory may be used (CWE-829, CWE-20)",
     "Use a flag like LOAD_LIBRARY_SEARCH_SYSTEM32 or LOAD_LIBRARY_SEARCH_APPLICATION_DIR to search only desired folders",
     "misc", "", {'input': 1}, "FF1059"),

    "SetSecurityDescriptorDacl":
    (c_hit_if_null, 5,
     "Never create NULL ACLs; an attacker can set it to Everyone (Deny All Access), "
     "which would even forbid administrator access (CWE-732)",
     "",
     "misc", "", {'check_for_null': 3}, "FF1060"),

    "AddAccessAllowedAce":
    (normal, 3,
     "This doesn't set the inheritance bits in the access control entry (ACE) header (CWE-732)",
     "Make sure that you set inheritance by hand if you wish it to inherit",
     "misc", "", {}, "FF1061"),

    "getlogin":
    (normal, 4,
     "It's often easy to fool getlogin.  Sometimes it does not work at all, because some program messed up the utmp file.  Often, it gives only the first 8 characters of the login name. The user currently logged in on the controlling tty of our program need not be the user who started it.  Avoid getlogin() for security-related purposes (CWE-807)",
     "Use getpwuid(geteuid()) and extract the desired information instead",
     "misc", "", {}, "FF1062"),

    "cuserid":
    (normal, 4,
     "Exactly what cuserid() does is poorly defined (e.g., some systems use the effective uid, like Linux, while others like System V use the real uid). Thus, you can't trust what it does. It's certainly not portable (The cuserid function was included in the 1988 version of POSIX, but removed from the 1990 version).  Also, if passed a non-null parameter, there's a risk of a buffer overflow if the passed-in buffer is not at least L_cuserid characters long (CWE-120)",
     "Use getpwuid(geteuid()) and extract the desired information instead",
     "misc", "", {}, "FF1063"),

    "getpw":
    (normal, 4,
     "This function is dangerous; it may overflow the provided buffer. It extracts data from a 'protected' area, but most systems have many commands to let users modify the protected area, and it's not always clear what their limits are.  Best to avoid using this function altogether (CWE-676, CWE-120)",
     "Use getpwuid() instead",
     "buffer", "", {}, "FF1064"),

    "getpass":
    (normal, 4,
     "This function is obsolete and not portable. It was in SUSv2 but removed by POSIX.2.  What it does exactly varies considerably between systems, particularly in where its prompt is displayed and where it gets its data (e.g., /dev/tty, stdin, stderr, etc.). In addition, some implementations overflow buffers. (CWE-676, CWE-120, CWE-20)",
     "Make the specific calls to do exactly what you want.  If you continue to use it, or write your own, be sure to zero the password as soon as possible to avoid leaving the cleartext password visible in the process' address space",
     "misc", "", {'input': 1}, "FF1065"),

    "gsignal|ssignal":
    (normal, 2,
     "These functions are considered obsolete on most systems, and very non-portable (Linux-based systems handle them radically different, basically if gsignal/ssignal were the same as raise/signal respectively, while System V considers them a separate set and obsolete) (CWE-676)",
     "Switch to raise/signal, or some other signalling approach",
     "obsolete", "", {}, "FF1066"),

    "memalign":
    (normal, 1,
     "On some systems (though not Linux-based systems) an attempt to free() results from memalign() may fail. This may, on a few systems, be exploitable.  Also note that memalign() may not check that the boundary parameter is correct (CWE-676)",
     "Use posix_memalign instead (defined in POSIX's 1003.1d).  Don't switch to valloc(); it is marked as obsolete in BSD 4.3, as legacy in SUSv2, and is no longer defined in SUSv3.  In some cases, malloc()'s alignment may be sufficient",
     "free", "", {}, "FF1067"),

    "ulimit":
    (normal, 1,
     "This C routine is considered obsolete (as opposed to the shell command by the same name, which is NOT obsolete) (CWE-676)",
     "Use getrlimit(2), setrlimit(2), and sysconf(3) instead",
     "obsolete", "", {}, "FF1068"),

    "usleep":
    (normal, 1,
     "This C routine is considered obsolete (as opposed to the shell command by the same name).   The interaction of this function with SIGALRM and other timer functions such as sleep(), alarm(), setitimer(), and nanosleep() is unspecified (CWE-676)",
     "Use nanosleep(2) or setitimer(2) instead",
     "obsolete", "", {}, "FF1069"),

    # Input functions, useful for -I
    "recv|recvfrom|recvmsg|fread|readv":
    (normal, 0, "Function accepts input from outside program (CWE-20)",
     "Make sure input data is filtered, especially if an attacker could manipulate it",
     "input", "", {'input': 1}, "FF1070"),

    # Unsafe STL functions that don't check the second iterator
    "equal|mismatch|is_permutation":
    (cpp_unsafe_stl,
     # Like strlen, this is mostly a risk to availability; at worst it
     # often causes a program crash.
     1,
     "Function does not check the second iterator for over-read conditions (CWE-126)",
     "This function is often discouraged by most C++ coding standards in favor of its safer alternatives provided since C++14. Consider using a form of this function that checks the second iterator before potentially overflowing it",
     "buffer", "", {}, "FF1071"),

    # TODO: detect C++'s:   cin >> charbuf, where charbuf is a char array; the problem
    #       is that flawfinder doesn't have type information, and ">>" is safe with
    #       many other types.
    # ("send" and friends aren't todo, because they send out.. not input.)
    # TODO: cwd("..") in user's space - TOCTOU vulnerability
    # TODO: There are many more rules to add, esp. for TOCTOU.
}

template_ruleset = {
    # This is a template for adding new entries (the key is impossible):
    "9": (normal, 2, "", "", "tmpfile", "", {}),
}


def find_column(text, position):
    "Find column number inside line."
    newline = text.rfind("\n", 0, position)
    if newline == -1:
        return position + 1
    return position - newline


def get_context(text, position):
    "Get surrounding text line starting from text[position]"
    linestart = text.rfind("\n", 0, position + 1) + 1
    lineend = text.find("\n", position, len(text))
    if lineend == -1:
        lineend = len(text)
    return text[linestart:lineend]


def c_valid_match(text, position):
    # Determine if this is a valid match, or a false positive.
    # If false positive controls aren't on, always declare it's a match:
    i = position
    while i < len(text):
        c = text[i]
        if c == '(':
            return 1
        elif c in string.whitespace:
            i += 1
        else:
            if falsepositive:
                return 0  # No following "(", presume invalid.
            if c in "=+-":
                # This is very unlikely to be a function use. If c is '=',
                # the name is followed by an assignment or is-equal operation.
                # Since the names of library functions are really unlikely to be
                # followed by an assignment statement or 'is-equal' test,
                # while this IS common for variable names, let's declare it invalid.
                # It's possible that this is a variable function pointer, pointing
                # to the real library function, but that's really improbable.
                # If c is "+" or "-", we have a + or - operation.
                # In theory "-" could be used for a function pointer difference
                # computation, but this is extremely improbable.
                # More likely: this is a variable in a computation, so drop it.
                return 0
            return 1
    return 0  # Never found anything other than "(" and whitespace.


def process_directive():
    "Given a directive, process it."
    global ignoreline, num_ignored_hits
    # TODO: Currently this is just a stub routine that simply removes
    # hits from the current line, if any, and sets a flag if not.
    # Thus, any directive is considered the "ignore" directive.
    # Currently that's okay because we don't have any other directives yet.
    if never_ignore:
        return
    hitfound = 0
    # Iterate backwards over hits, to be careful about the destructive iterator
    # Note: On Python 2 this is inefficient, because the "range" operator
    # creates a list.  We used to use "xrange", but that doesn't exist
    # in Python3.  So we use "range" which at least works everywhere.
    # If speed is vital on Python 2 we could replace this with xrange.
    for i in range(len(hitlist) - 1, -1, -1):
        if hitlist[i].filename == filename and hitlist[i].line == linenumber:
            del hitlist[i]  # DESTROY - this is a DESTRUCTIVE iterator.
            hitfound = 1  # Don't break, because there may be more than one.
            num_ignored_hits += 1
    if not hitfound:
        ignoreline = linenumber + 1  # Nothing found - ignore next line.


# Characters that can be in a string.
# 0x4, 4.4e4, etc.
numberset = string.hexdigits + "_x.Ee"

# Patterns for various circumstances:
p_whitespace = re.compile(r'[ \t\v\f]+')
p_include = re.compile(r'#\s*include\s+(<.*?>|".*?")')
p_digits = re.compile(r'[0-9]')
p_alphaunder = re.compile(r'[A-Za-z_]')  # Alpha chars and underline.
# A "word" in C.  Note that "$" is permitted -- it's not permitted by the
# C standard in identifiers, but gcc supports it as an extension.
p_c_word = re.compile(r'[A-Za-z_][A-Za-z_0-9$]*')
# We'll recognize ITS4 and RATS ignore directives, as well as our own,
# for compatibility's sake:
p_directive = re.compile(r'(?i)\s*(ITS4|Flawfinder|RATS):\s*([^\*]*)')

max_lookahead = 500  # Lookahead limit for c_static_array.


def process_c_file(f, patch_infos):
    global filename, linenumber, ignoreline, sumlines, num_links_skipped
    global sloc
    filename = f
    linenumber = 1
    ignoreline = -1

    cpplanguage = (f.endswith(".cpp") or f.endswith(".cxx") or f.endswith(".cc")
                   or f.endswith(".hpp"))
    incomment = 0
    instring = 0
    linebegin = 1
    codeinline = 0  # 1 when we see some code (so increment sloc at newline)

    if (patch_infos is not None) and (f not in patch_infos):
        # This file isn't in the patch list, so don't bother analyzing it.
        if not quiet:
            if output_format:
                print("Skipping unpatched file ", h(f), "<br>")
            else:
                print("Skipping unpatched file", f)
            sys.stdout.flush()
        return

    if f == "-":
        my_input = sys.stdin
    else:
        # Symlinks should never get here, but just in case...
        if (not allowlink) and os.path.islink(f):
            print("BUG! Somehow got a symlink in process_c_file!")
            num_links_skipped += 1
            return
        try:
            my_input = open(f, "r")
        except BaseException:
            print("Error: failed to open", h(f))
            sys.exit(14)

    # Read ENTIRE file into memory.  Use readlines() to convert \n if necessary.
    # This turns out to be very fast in Python, even on large files, and it
    # eliminates lots of range checking later, making the result faster.
    # We're examining source files, and today, it would be EXTREMELY bad practice
    # to create source files larger than main memory space.
    # Better to load it all in, and get the increased speed and reduced
    # development time that results.

    if not quiet:
        if output_format:
            print("Examining", h(f), "<br>")
        else:
            print("Examining", f)
        sys.stdout.flush()

    # Python3 is often configured to use only UTF-8, and presumes
    # that inputs cannot have encoding errors.
    # The real world isn't like that, so provide a prettier warning
    # in such cases - with some hints on how to solve it.
    try:
        text = "".join(my_input.readlines())
    except UnicodeDecodeError as err:
        print('Error: encoding error in', h(f))
        print(err)
        print()
        print('Python3 requires input character data to be perfectly encoded;')
        print('it also requires perfectly correct system encoding settings.')
        print('Unfortunately, your data and/or system settings are not.')
        print('Here are some options:')
        print('1. Run: PYTHONUTF8=0 python3 flawfinder')
        print('   if your system and and data are all properly set up for')
        print('   a non-UTF-8 encoding.')
        print('2. Run: PYTHONUTF8=0 LC_ALL=C.ISO-2022 python3 flawfinder')
        print('   if your data has a specific encoding such as ISO-2022')
        print('   (replace "ISO-2022" with the name of your encoding,')
        print('   and optionally replace "C" with your native language).')
        print('3. Run: PYTHONUTF8=0 LC_ALL=C.ISO-8859-1 python3 flawfinder')
        print('   if your data has an unknown or inconsistent encoding')
        print('   (ISO-8859-1 encoders normally allow anything).')
        print('4. Convert all your source code to the UTF-8 encoding.')
        print('   The system program "iconv" or Python program "cvt2utf" can')
        print('   do this (for cvt2utf, you can use "pip install cvt2utf").')
        print('5. Run: python2 flawfinder')
        print('   (That is, use Python 2 instead of Python 3).')
        print('Some of these options may not work depending on circumstance.')
        print('In the long term, we recommend using UTF-8 for source code.')
        print('For more information, see the documentation.')
        sys.exit(15)

    i = 0
    while i < len(text):
        # This is a trivial tokenizer that just tries to find "words", which
        # match [A-Za-z_][A-Za-z0-9_]*.  It skips comments & strings.
        # It also skips "#include <...>", which must be handled specially
        # because "<" and ">" aren't usually delimiters.
        # It doesn't bother to tokenize anything else, since it's not used.
        # The following is a state machine with 3 states: incomment, instring,
        # and "normal", and a separate state "linebegin" if at BOL.

        # Skip any whitespace
        m = p_whitespace.match(text, i)
        if m:
            i = m.end(0)

        if i >= len(text):
            c = "\n"  # Last line with no newline, we're done
        else:
            c = text[i]
        if linebegin:  # If at beginning of line, see if #include is there.
            linebegin = 0
            if c == "#":
                codeinline = 1  # A directive, count as code.
            m = p_include.match(text, i)
            if m:  # Found #include, skip it.  Otherwise: #include <stdio.h>
                i = m.end(0)
                continue
        if c == "\n":
            linenumber += 1
            sumlines += 1
            linebegin = 1
            if codeinline:
                sloc += 1
            codeinline = 0
            i += 1
            continue
        i += 1  # From here on, text[i] points to next character.
        if i < len(text):
            nextc = text[i]
        else:
            nextc = ''
        if incomment:
            if c == '*' and nextc == '/':
                i += 1
                incomment = 0
        elif instring:
            if c == '\\' and (nextc != "\n"):
                i += 1
            elif c == '"' and instring == 1:
                instring = 0
            elif c == "'" and instring == 2:
                instring = 0
        else:
            if c == '/' and nextc == '*':
                m = p_directive.match(text,
                                      i + 1)  # Is there a directive here?
                if m:
                    process_directive()
                i += 1
                incomment = 1
            elif c == '/' and nextc == '/':  # "//" comments - skip to EOL.
                m = p_directive.match(text,
                                      i + 1)  # Is there a directive here?
                if m:
                    process_directive()
                while i < len(text) and text[i] != "\n":
                    i += 1
            elif c == '"':
                instring = 1
                codeinline = 1
            elif c == "'":
                instring = 2
                codeinline = 1
            else:
                codeinline = 1  # It's not whitespace, comment, or string.
                m = p_c_word.match(text, i - 1)
                if m:  # Do we have a word?
                    startpos = i - 1
                    endpos = m.end(0)
                    i = endpos
                    word = text[startpos:endpos]
                    # print "Word is:", text[startpos:endpos]
                    if (word in c_ruleset) and c_valid_match(text, endpos):
                        if ((patch_infos is None)
                                or ((patch_infos is not None) and
                                    (linenumber in patch_infos[f]))):
                            # FOUND A MATCH, setup & call hook.
                            # print "HIT: #%s#\n" % word
                            # Don't use the tuple assignment form, e.g., a,b=c,d
                            # because Python (least 2.2.2) does that slower
                            # (presumably because it creates & destroys temporary tuples)
                            hit = Hit(c_ruleset[word])
                            hit.name = word
                            hit.start = startpos
                            hit.end = endpos
                            hit.line = linenumber
                            hit.column = find_column(text, startpos)
                            hit.filename = filename
                            hit.context_text = get_context(text, startpos)
                            hit.parameters = extract_c_parameters(text, endpos)
                            if hit.extract_lookahead:
                                hit.lookahead = text[startpos:
                                                     startpos + max_lookahead]
                            hit.hook(hit)
                elif p_digits.match(c):
                    while i < len(text): # Process a number.
                        # C does not have digit separator
                        if p_digits.match(text[i]) or (cpplanguage and text[i] == "'"):
                            i += 1
                        else:
                            break
                # else some other character, which we ignore.
                # End of loop through text. Wrap up.
    if codeinline:
        sloc += 1
    if incomment:
        error("File ended while in comment.")
    if instring:
        error("File ended while in string.")


def expand_ruleset(ruleset):
    # Rulesets can have compressed sets of rules
    # (multiple function names separated by "|".
    # Expand the given ruleset.
    # Note that this "for" loop modifies the ruleset while it's iterating,
    # so we *must* convert the keys into a list before iterating.
    for rule in list(ruleset.keys()):
        if "|" in rule:  # We found a rule to expand.
            for newrule in rule.split("|"):
                if newrule in ruleset:
                    print("Error: Rule %s, when expanded, overlaps %s" % (
                        rule, newrule))
                    sys.exit(15)
                ruleset[newrule] = ruleset[rule]
            del ruleset[rule]
    # To print out the set of keys in the expanded ruleset, run:
    #   print `ruleset.keys()`


def display_ruleset(ruleset):
    # First, sort the list by function name:
    sortedkeys = sorted(ruleset.keys())
    # Now, print them out:
    for key in sortedkeys:
        print(key + "\t" + str(
            ruleset[key][1]
        ) + "\t" + ruleset[key][2])  # function name, default level, default warning


def initialize_ruleset():
    expand_ruleset(c_ruleset)
    if showheading:
        print("Number of rules (primarily dangerous function names) in C/C++ ruleset:", len(
            c_ruleset))
        if output_format:
            print("<p>")
    if list_rules:
        display_ruleset(c_ruleset)
        sys.exit(0)


# Show the header, but only if it hasn't been shown yet.
def display_header():
    global displayed_header
    if csv_output:
        csv_writer.writerow([
            'File', 'Line', 'Column', 'DefaultLevel', 'Level', 'Category', 'Name', 'Warning',
            'Suggestion', 'Note', 'CWEs', 'Context', 'Fingerprint', 'ToolVersion', 'RuleId', 'HelpUri'
        ])
        return
    if sarif_output:
        return
    if not showheading:
        return
    if not displayed_header:
        if output_format:
            print(
                '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" '
                '"http://www.w3.org/TR/html4/loose.dtd">')
            print("<html>")
            print("<head>")
            print('<meta http-equiv="Content-type" content="text/html; charset=utf8">')
            print("<title>Flawfinder Results</title>")
            print('<meta name="author" content="David A. Wheeler">')
            print('<meta name="keywords" lang="en" content="flawfinder results, security scan">')
            print("</head>")
            print("<body>")
            print("<h1>Flawfinder Results</h1>")
            print("Here are the security scan results from")
            print('<a href="https://dwheeler.com/flawfinder">Flawfinder version %s</a>,' % version)
            print('(C) 2001-2019 <a href="https://dwheeler.com">David A. Wheeler</a>.')
        else:
            print("Flawfinder version %s, (C) 2001-2019 David A. Wheeler." % version)
        displayed_header = 1


c_extensions = {
    '.c': 1,
    '.h': 1,
    '.ec': 1,
    '.ecp': 1,  # Informix embedded C.
    '.pgc': 1,  # Postgres embedded C.
    '.C': 1,
    '.cpp': 1,
    '.CPP': 1,
    '.cxx': 1,
    '.cc': 1,  # C++
    '.CC': 1,
    '.c++': 1,  # C++.
    '.pcc': 1,  # Oracle C++
    '.pc': 1,  # Oracle SQL-embedded C
    '.sc': 1,  # Oracle Pro*C pre-compiler
    '.hpp': 1,
    '.H': 1,  # .h - usually C++.
}


def maybe_process_file(f, patch_infos):
    # process f, but only if (1) it's a directory (so we recurse), or
    # (2) it's source code in a language we can handle.
    # Currently, for files that means only C/C++, and we check if the filename
    # has a known C/C++ filename extension.  If it doesn't, we ignore the file.
    # We accept symlinks only if allowlink is true.
    global num_links_skipped, num_dotdirs_skipped
    if os.path.isdir(f):
        if (not allowlink) and os.path.islink(f):
            if not quiet:
                print_warning("Skipping symbolic link directory " + h(f))
            num_links_skipped += 1
            return
        base_filename = os.path.basename(f)
        if (skipdotdir and len(base_filename) > 1
                and (base_filename[0] == ".")):
            if not quiet:
                print_warning("Skipping directory with initial dot " + h(f))
            num_dotdirs_skipped += 1
            return
        for dir_entry in os.listdir(f):
            maybe_process_file(os.path.join(f, dir_entry), patch_infos)
    # Now we will FIRST check if the file appears to be a C/C++ file, and
    # THEN check if it's a regular file or symlink.  This is more complicated,
    # but I do it this way so that there won't be a lot of pointless
    # warnings about skipping files we wouldn't have used anyway.
    dotposition = f.rfind(".")
    if dotposition > 1:
        extension = f[dotposition:]
        if extension in c_extensions:
            # Its name appears to be a C/C++ source code file.
            if (not allowlink) and os.path.islink(f):
                if not quiet:
                    print_warning("Skipping symbolic link file " + h(f))
                num_links_skipped += 1
            elif not os.path.isfile(f):
                # Skip anything not a normal file.  This is so that
                # device files, etc. won't cause trouble.
                if not quiet:
                    print_warning("Skipping non-regular file " + h(f))
            else:
                # We want to know the difference only with files found in the
                # patch.
                if ((patch_infos is None)
                        or (patch_infos is not None and (f in patch_infos))):
                    process_c_file(f, patch_infos)


def process_file_args(files, patch_infos):
    # Process the list of "files", some of which may be directories,
    # which were given on the command line.
    # This is handled differently than anything not found on the command line
    # (i.e. through recursing into a directory) because flawfinder
    # ALWAYS processes normal files given on the command line.
    # This is done to give users control over what's processed;
    # if a user really, really wants to analyze a file, name it!
    # If user wants to process "this directory and down", just say ".".
    # We handle symlinks specially, handle normal files and directories,
    # and skip the rest to prevent security problems. "-" is stdin.
    global num_links_skipped
    for f in files:
        if (not allowlink) and os.path.islink(f):
            if not quiet:
                print_warning("Skipping symbolic link " + h(f))
            num_links_skipped += 1
        elif os.path.isfile(f) or f == "-":
            # If on the command line, FORCE processing of it.
            # Currently, we only process C/C++.
            # check if we only want to review a patch
            if ((patch_infos is not None and f in patch_infos)
                    or (patch_infos is None)):
                process_c_file(f, patch_infos)
        elif os.path.isdir(f):
            # At one time flawfinder used os.path.walk, but that Python
            # built-in doesn't give us enough control over symbolic links.
            # So, we'll walk the filesystem hierarchy ourselves:
            maybe_process_file(f, patch_infos)
        elif not os.path.exists(f):
            if not quiet:
                # Help humans avoid a long mysterious debugging session.
                # Sometimes people copy/paste from HTML that has a leading
                # en dash (\u2013 aka 0xE2 0x80 0x93) or
                # em dash (\u2014 aka 0xE2 0x80 0x94) instead of the
                # correct dash marker (in an attempt to make things "pretty").
                # These symbols *look* like the regular dash
                # option marker, but they are not the same characters.
                # If there's no such file, give a special warning,
                # because otherwise this can be extremely
                # difficult for humans to notice.  We'll do the check in
                # this odd way so it works on both Python 2 and Python 3.
                # (Python 3 wants \u...).
                # Note that we *only* make this report if the file doesn't
                # exist - if someone asks to process a file with this crazy
                # name, and it exists, we'll process it without complaint.
                if (h(f).startswith("\xe2\x80\x93") or
                        h(f).startswith("\xe2\x80\x94") or
                        h(f).startswith(u"\u2013") or
                        h(f).startswith(u"\u2014")):
                    print_warning(
                        "Skipping non-existent filename starting with em dash or en dash "
                        + h(f))
                else:
                    print_warning("Skipping non-existent file " + h(f))
        else:
            if not quiet:
                print_warning("Skipping non-regular file " + h(f))


def usage():
    print("""
flawfinder [--help | -h] [--version] [--listrules]
  [--allowlink] [--followdotdir] [--nolink]
           [--patch filename | -P filename]
  [--inputs | -I] [--minlevel X | -m X]
           [--falsepositive | -F] [--neverignore | -n]
  [--context | -c] [--columns | -C] [--dataonly | -D]
           [--html | -H] [--immediate | -i] [--singleline | -S]
           [--omittime] [--quiet | -Q]
  [--loadhitlist F] [--savehitlist F] [--diffhitlist F]
  [--] [source code file or source root directory]+

  The options cover various aspects of flawfinder as follows.

  Documentation:
  --help | -h Show this usage help.
  --version   Show version number.
  --listrules List the rules in the ruleset (rule database).

  Selecting Input Data:
  --allowlink Allow symbolic links.
  --followdotdir
              Follow directories whose names begin with ".".
              Normally they are ignored.
  --nolink    Skip symbolic links (ignored).
  --patch F | -P F
              Display information related to the patch F
              (patch must be already applied).

  Selecting Hits to Display:
  --inputs | -I
              Show only functions that obtain data from outside the program;
              this also sets minlevel to 0.
  -m X | --minlevel=X
              Set minimum risk level to X for inclusion in hitlist.  This
              can be from 0 (``no risk'')  to  5  (``maximum  risk'');  the
              default is 1.
  --falsepositive | -F
              Do not include hits that are likely to be false  positives.
              Currently,  this  means  that function names are ignored if
              they're not followed by "(", and that declarations of char-
              acter  arrays  aren't noted.  Thus, if you have use a vari-
              able named "access" everywhere, this will eliminate  refer-
              ences  to  this ordinary variable.  This isn't the default,
              because this  also  increases  the  likelihood  of  missing
              important  hits;  in  particular, function names in #define
              clauses and calls through function pointers will be missed.
  --neverignore | -n
              Never ignore security issues, even if they have an ``ignore''
              directive in a comment.
  --regex PATTERN | -e PATTERN
              Only report hits that match the regular expression PATTERN.

  Selecting Output Format:
  --columns | -C
              Show  the  column  number  (as well as the file name and
              line number) of each hit; this is shown after the line number
              by adding a colon and the column number in the line (the first
              character in a line is column number 1).
  --context | -c
              Show context (the line having the "hit"/potential flaw)
  --dataonly | -D
              Don't display the headers and footers of the analysis;
              use this along with --quiet to get just the results.
  --html | -H
              Display as HTML output.
  --immediate | -i
              Immediately display hits (don't just wait until the end).
  --sarif     Generate output in SARIF format.
  --singleline | -S
              Single-line output.
  --omittime  Omit time to run.
  --quiet | -Q
              Don't display status information (i.e., which files are being
              examined) while the analysis is going on.
  --error-level=LEVEL
              Return a nonzero (false) error code if there is at least one
              hit of LEVEL or higher.  If a diffhitlist is provided,
              hits noted in it are ignored.
              This option can be useful within a continuous integration script,
              especially if you mark known-okay lines as "flawfinder: ignore".
              Usually you want level to be fairly high, such as 4 or 5.
              By default, flawfinder returns 0 (true) on a successful run.

  Hitlist Management:
  --savehitlist=F
              Save all hits (the "hitlist") to F.
  --loadhitlist=F
              Load hits from F instead of analyzing source programs.
  --diffhitlist=F
              Show only hits (loaded or analyzed) not in F.


  For more information, please consult the manpage or available
  documentation.
""")


def process_options():
    global show_context, show_inputs, allowlink, skipdotdir, omit_time
    global output_format, minimum_level, show_immediately, single_line
    global csv_output, csv_writer, sarif_output
    global error_level
    global required_regex, required_regex_compiled
    global falsepositive
    global show_columns, never_ignore, quiet, showheading, list_rules
    global loadhitlist, savehitlist, diffhitlist_filename
    global patch_file
    try:
        # Note - as a side-effect, this sets sys.argv[].
        optlist, args = getopt.getopt(sys.argv[1:], "ce:m:nih?CSDQHIFP:", [
            "context", "minlevel=", "immediate", "inputs", "input", "nolink",
            "falsepositive", "falsepositives", "columns", "listrules",
            "omittime", "allowlink", "patch=", "followdotdir", "neverignore",
            "regex=", "quiet", "dataonly", "html", "singleline", "csv",
            "error-level=", "sarif",
            "loadhitlist=", "savehitlist=", "diffhitlist=", "version", "help"
        ])
        for (opt, value) in optlist:
            if opt in ("--context", "-c"):
                show_context = 1
            elif opt in ("--columns", "-C"):
                show_columns = 1
            elif opt in ("--quiet", "-Q"):
                quiet = 1
            elif opt in ("--dataonly", "-D"):
                showheading = 0
            elif opt in ("--inputs", "--input", "-I"):
                show_inputs = 1
                minimum_level = 0
            elif opt in ("--falsepositive", "falsepositives", "-F"):
                falsepositive = 1
            elif opt == "--nolink":
                allowlink = 0
            elif opt == "--omittime":
                omit_time = 1
            elif opt == "--allowlink":
                allowlink = 1
            elif opt == "--followdotdir":
                skipdotdir = 0
            elif opt == "--listrules":
                list_rules = 1
            elif opt in ("--html", "-H"):
                output_format = 1
                single_line = 0
            elif opt in ("--minlevel", "-m"):
                minimum_level = int(value)
            elif opt in ("--singleline", "-S"):
                single_line = 1
            elif opt == "--csv":
                csv_output = 1
                quiet = 1
                showheading = 0
                csv_writer = csv.writer(sys.stdout)
            elif opt == "--sarif":
                sarif_output = 1
                quiet = 1
                showheading = 0
            elif opt == "--error-level":
                error_level = int(value)
            elif opt in ("--immediate", "-i"):
                show_immediately = 1
            elif opt in ("-n", "--neverignore"):
                never_ignore = 1
            elif opt in ("-e", "--regex"):
                required_regex = value
                # This will raise an exception if it can't be compiled as a
                # regex:
                required_regex_compiled = re.compile(required_regex)
            elif opt in ("-P", "--patch"):
                # Note: This is -P, so that a future -p1 option can strip away
                # pathname prefixes (with the same option name as "patch").
                patch_file = value
                # If we consider ignore comments we may change a line which was
                # previously ignored but which will raise now a valid warning
                # without noticing it now.  So, set never_ignore.
                never_ignore = 1
            elif opt == "--loadhitlist":
                loadhitlist = value
                display_header()
                if showheading:
                    print("Loading hits from", value)
            elif opt == "--savehitlist":
                savehitlist = value
                display_header()
                if showheading:
                    print("Saving hitlist to", value)
            elif opt == "--diffhitlist":
                diffhitlist_filename = value
                display_header()
                if showheading:
                    print("Showing hits not in", value)
            elif opt == "--version":
                print(version)
                sys.exit(0)
            elif opt in ('-h', '-?', '--help'):
                # We accept "-?" but do not document it because we don't
                # want to encourage its use.
                # Unix-like systems with typical shells expand glob
                # patterns, so the question mark in "-?" should be escaped,
                # and many forget that.
                usage()
                sys.exit(0)
        # For DOS/Windows, expand filenames; for Unix, DON'T expand them
        # (the shell will expand them for us).  Some sloppy Python programs
        # always call "glob", but that's WRONG -- on Unix-like systems that
        # will expand twice.  Python doesn't have a clean way to detect
        # "has globbing occurred", so this is the best I've found:
        if os.name in ("windows", "nt", "dos"):
            sys.argv[1:] = functools.reduce(operator.add,
                                            list(map(glob.glob, args)))
        else:
            sys.argv[1:] = args
    # In Python 2 the convention is "getopt.GetoptError", but we
    # use "getopt.error" here so it's compatible with both
    # Python 1.5 and Python 2.
    except getopt.error as text:
        print("*** getopt error:", text)
        usage()
        sys.exit(16)
    if output_format == 1 and list_rules == 1:
        print('You cannot list rules in HTML format')
        sys.exit(20)


def process_files():
    """Process input (files or hitlist); return True if okay."""
    global hitlist
    if loadhitlist:
        f = open(loadhitlist, "rb")
        hitlist = pickle.load(f)
        return True
    else:
        patch_infos = None
        if patch_file != "":
            patch_infos = load_patch_info(patch_file)
        files = sys.argv[1:]
        if not files:
            print("*** No input files")
            return None
        process_file_args(files, patch_infos)
        return True


def hitlist_sort_key(hit):
    """Sort key for hitlist."""
    return (-hit.level, hit.filename, hit.line, hit.column, hit.name)


def show_final_results():
    global hitlist
    global error_level_exceeded
    count = 0
    count_per_level = {}
    count_per_level_and_up = {}
    # Name levels directly, to avoid Python "range" (a Python 2/3 difference)
    possible_levels = (0, 1, 2, 3, 4, 5)
    for i in possible_levels:  # Initialize count_per_level
        count_per_level[i] = 0
    for i in possible_levels:  # Initialize count_per_level_and_up
        count_per_level_and_up[i] = 0
    if show_immediately or not quiet:  # Separate the final results.
        print()
        if showheading:
            if output_format:
                print("<h2>Final Results</h2>")
            else:
                print("FINAL RESULTS:")
                print()
    hitlist.sort(key=hitlist_sort_key)
    # Display results.  The HTML format now uses
    # <ul> so that the format differentiates each entry.
    # I'm not using <ol>, because its numbers might be confused with
    # the risk levels or line numbers.
    if diffhitlist_filename:
        diff_file = open(diffhitlist_filename, 'rb')
        diff_hitlist = pickle.load(diff_file)
    if output_format:
        print("<ul>")
    for hit in hitlist:
        if not diffhitlist_filename or hit not in diff_hitlist:
            count_per_level[hit.level] = count_per_level[hit.level] + 1
            if hit.level >= minimum_level:
                hit.show()
                count += 1
            if hit.level >= error_level:
                error_level_exceeded = True
    if output_format:
        print("</ul>")
    if diffhitlist_filename:
        diff_file.close()
    # Done with list, show the post-hitlist summary.
    if showheading:
        if output_format:
            print("<h2>Analysis Summary</h2>")
        else:
            print()
            print("ANALYSIS SUMMARY:")
        if output_format:
            print("<p>")
        else:
            print()
        if count > 0:
            print("Hits =", count)
        else:
            print("No hits found.")
        if output_format:
            print("<br>")
        # Compute the amount of time spent, and lines analyzed/second.
        # By computing time here, we also include the time for
        # producing the list of hits, which is reasonable.
        time_analyzing = time.time() - starttime
        if required_regex:
            print("Hits limited to regular expression " + required_regex)
        print("Lines analyzed = %d" % sumlines, end='')
        if time_analyzing > 0 and not omit_time:  # Avoid divide-by-zero.
            print(" in approximately %.2f seconds (%.0f lines/second)" % (
                time_analyzing, (sumlines / time_analyzing)))
        else:
            print()
        if output_format:
            print("<br>")
        print("Physical Source Lines of Code (SLOC) = %d" % sloc)
        if output_format:
            print("<br>")
        # Output hits@each level.
        print("Hits@level =", end='')
        for i in possible_levels:
            print(" [%d] %3d" % (i, count_per_level[i]), end='')
        if output_format:
            print(" <br>")
        else:
            print()
        # Compute hits at "level x or higher"
        print("Hits@level+ =", end='')
        for i in possible_levels:
            for j in possible_levels:
                if j >= i:
                    count_per_level_and_up[
                        i] = count_per_level_and_up[i] + count_per_level[j]
        # Display hits at "level x or higher"
        for i in possible_levels:
            print(" [%d+] %3d" % (i, count_per_level_and_up[i]), end='')
        if output_format:
            print(" <br>")
        else:
            print()
        if sloc > 0:
            print("Hits/KSLOC@level+ =", end='')
            for i in possible_levels:
                print(" [%d+] %3g" % (
                    i, count_per_level_and_up[i] * 1000.0 / sloc), end='')
        if output_format:
            print(" <br>")
        else:
            print()
        #
        if num_links_skipped:
            print("Symlinks skipped =", num_links_skipped, "(--allowlink overrides but see doc for security issue)")
            if output_format:
                print("<br>")
        if num_dotdirs_skipped:
            print("Dot directories skipped =", num_dotdirs_skipped, "(--followdotdir overrides)")
            if output_format:
                print("<br>")
        if num_ignored_hits > 0:
            print("Suppressed hits =", num_ignored_hits, "(use --neverignore to show them)")
            if output_format:
                print("<br>")
        print("Minimum risk level = %d" % minimum_level)
        if output_format:
            print("<br>")
        else:
            print()
        if count > 0:
            print("Not every hit is necessarily a security vulnerability.")
            print("You can inhibit a report by adding a comment in this form:")
            print("// flawfinder: ignore")
            print("Make *sure* it's a false positive!")
            print("You can use the option --neverignore to show these.")
            if output_format:
                print("<br>")
            else:
                print()
        print("There may be other security vulnerabilities; review your code!")
        if output_format:
            print("<br>")
            print("See '<a href=\"https://dwheeler.com/secure-programs\">Secure Programming HOWTO</a>'")
            print("(<a href=\"https://dwheeler.com/secure-programs\">https://dwheeler.com/secure-programs</a>) for more information.")
        else:
            print("See 'Secure Programming HOWTO'")
            print("(https://dwheeler.com/secure-programs) for more information.")
        if output_format:
            print("</body>")
            print("</html>")


def save_if_desired():
    # We'll save entire hitlist, even if only differences displayed.
    if savehitlist:
        if not quiet:
            print("Saving hitlist to", savehitlist)
        f = open(savehitlist, "wb")
        pickle.dump(hitlist, f)
        f.close()


def flawfind():
    process_options()
    display_header()
    initialize_ruleset()
    if process_files():
        if sarif_output:
            print(SarifLogger(hitlist).output_sarif())
        else:
            show_final_results()
        save_if_desired()
    return 1 if error_level_exceeded else 0


def main():
    try:
        sys.exit(flawfind())
    except KeyboardInterrupt:
        print("*** Flawfinder interrupted")

if __name__ == '__main__':
    main()
