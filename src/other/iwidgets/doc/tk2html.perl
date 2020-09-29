#!/usr/local/bin/perl
# Lightly modified man2html to make html equivs of tk/tcl man pages
# probably a dead end soln since works on output after troff processing


# Set the man path array to the paths to search...
@manpath = ('/usr/share/man','/usr/gnu/man','/usr/local/man');
#@manpath = ('/s/usr/hops/src/ftp/tcl/tk3.4/docs');

# There has to be a blank line after this...
#print "Content-type:text/html\n\n";

if (!$ARGV[0]) {
  print "<isindex>\n";
  chop($os = `uname`);
  chop($ver = `uname -r`);
  print "
<title> $os $ver Manual Pages </title>
<h1> $os $ver Manual Pages </h1>

Enter the name of the man page, optionally surrounded
by parenthesis with the number.  For example:
<p>
<ul>
<li> stat to find one or more man pages for ls
<li> stat(2) for the system call stat
</ul>

This converter is still in development.  I intend to
improve the handling of multiple matches, and add
a interface to apropos (or man -k (or whatis...))
<p>
<a href=\"/users/bcutter/intro.html\">Brooks Cutter</a>
";
  exit(0);
}

$_ = $ARGV[0];
$manpages[0] = $_;
if ((/^-$/)) {
  $manpages[0] = $_;
} elsif ((m!^/!)) {
  $manpages[0] = $_;
#} elsif (($name, $sect) = /(\S+)\((\d.*)\)/) {
#  @manpages = &findman($name, $sect, @manpath);
#} elsif (($name, $sect) = /(\S+)<(\d.*)>/) {
#  @manpages = &findman($name, $sect, @manpath);
#} elsif (($name, $sect) = /(\S+)\[(\d.*)\]/) {
#  @manpages = &findman($name, $sect, @manpath);
#} else {
#  @manpages = &findman($_, '', @manpath);
}

if (!scalar @manpages) {
  print "Sorry, I was unable to find a match for <b>$_</b>\n";
  exit(0);
} elsif (scalar @manpages > 1) {
  &which_manpage(@manpages);
} else {
  if (!-e $manpages[0]) {
    die "man2html: Error, Can't locate file '$manpages[0]'\n";
  }
  chop($type=`file -L $manpages[0]`);
  if ($type =~ /roff/i) {
    $manpages[0] = "nroff -man $manpages[0]|col -b|";
  } elsif ($type =~ /text/i) {
#    #$manpages[0] = $manpages[0]; 
#    ; # NOP (No Operation)
    $manpages[0] = "nroff -man $manpages[0]| col -b|";
  } else {
    print "
<title>Man2HTML: An Error has occurred</title>
<h1>Man2HTML: An Error has occurred</h1>

man2html found the following match for your query:</hr>
$manpages[0]
<p>
When  'file -L $manpages[0]' was run 
(which should follow symbolic links)
it returned the following value '$type'
<p>

";
  if ($type =~ /link/i) {
  print "
This problem appears to be that there is a symbolic link 
for a man page that is pointing to a file that doesn't exist.
<p>
";
  }
  print "
Please report this problem to someone who can do something about it.
<i>(Assuming you aren't that person...)</i>
If you don't know who that is, try emailing 'root' or 'postmaster'.
<p>
There was only one match for your query - and it can't currently 
be accessed.
";
  exit(0);
    #die "Unknown type '$type' for manpage '$manpages[0]'";
  }
  &print_manpage($manpages[0]);
}

exit(0);

sub findman {
# Take a argument like 'ls' or 'vi(1)' or 'tip(1c)' and return
# a list of one or more manpages.
# Arguments 2- are the directories to search in
  local($lookfor) = shift(@_);
  local($section) = shift(@_);
  local($file, @files, @return, $return);
  local(%men,%man);
  die "lookfor($lookfor) is null\n" unless($lookfor);
  for (@_) {
    # I'm... too lazy... for... opendir()... too lazy for readdir()...
    # too lazy for closedir() ... I'm too lazy!
    if (!$section) {
      @files = `/bin/ls $_/*/$lookfor.* 2> /dev/null`;
    } else {
      # if the section is like '1b' then just search *1b
      # otherwise if '1' search *1* (to catch all sub-sections)
      # Reason for wildcards: ($_/*$section*/$lookfor.*)
      # (given $section = '2')
      # 1st: So it catches cat2 and man2
      # 2nd: So it catches man2 and man2v 
      # (This should make it compatiable with HP/UX's man2.Z - not tested)
      # 3rd: So it catches stat.2 and stat.2v
      #
      if (length($section) == 1) {
        @files = `/bin/ls $_/*$section*/$lookfor.* 2> /dev/null`;
      } else {
        local($section_num) = substr($section, 0, 1); # Just the number...
        @files = `/bin/ls $_/*$section_num*/$lookfor.* $_/*$section/$lookfor.* 2> /dev/null`;
      }
    }
    next if (!scalar @files);
    # This part checks the files that were found...
    for $file (@files) {
      chop($file);
      local(@dirs) = split(/\//,$file);
      local($fn) = pop(@dirs);
      local($catman) = pop(@dirs);
      local($dir) = join('/',@dirs);
      local($key) = "$dir/$fn";
      next if ($man{$key}); # forces unique
      if (!$men{$key}) {
        $men{$key} = $catman;
        $man{$key} = $file;
      } else {
        # pre-formatted man pages always take precedence unless zero bytes...
        next if (($men{$key} =~ /^cat/i) && (!(-z $man{$key})));
        $men{$key} = $catman;
        $man{$key} = $file;
      }
    }
  }
  return(values %man);
}


sub which_manpage {
# Print a list of manpages...
  print "
There were multiple matches for the argument '$ARGV[0]'.
Below are the fully qualified pathnames of the matches, please
click on the appropriate one.

<ul>
";
  for (@_) {
    print "<li><a href=\"/htbin/man2html?$_\">$_</a>\n";
  }
  print "</ul>\n";
  return;
}

sub print_manpage {
  local($page) = @_;
  local($label, $before, $after, $begtag, $endtag, $blanks, $begtag2, $endtag2);
  local($pre);
  local($standard_indent) = 0;

  if ($page eq '-') {
    open(MAN, '-');
  } elsif (index($page,'|') == length($page)) {
    # A Pipe
    local($eval) = 
'open(MAN, "'.$page.'") || die "Can'."'t open pipe to '$page' for reading: ".'$!";';
    eval($eval);
    die "Eval error line $. : '$eval' returned '$@' : $!\n";
  } else {
    open(MAN, $page) || die "Can't open '$page' for reading: $!";
  }
  while (<MAN>) {
    s/\|\|*[   ]*$//;      # Delete trailing change bars 

    if (/^\s*$/) { 
      $blanks++;
      #if ($pre) { print "</pre>\n"; $pre = 0; }
      if (($. != 1) && ($blanks == 1)) {
        if (($pre) || ($section_pre)) {
          print "\n";
        } else {
          print "<p>\n";
        }
      }
      next;
    }
    #next if (!/^[A-Z]{2,}\(.*\).*/);
    if (//) { s/.//g; }
    # Escape & < and >
    s/&/\&amp;/g;
    s/</\&lt;/g;
    s/>/\&gt;/g;
    #
    if (/^(\w+.*)\s*$/) {
      $label = $1;
      $next_action = '';
      if (/^[A-Z ]{2,}\s*$/) {
        if (($pre) || ($section_pre)) { print "</pre>\n"; }
        $pre = $section_pre = $section_fmt = 0;
        if (!$standard_indent) { $next_action = 'check_indent'; }
      }
      if ($label eq 'NAME') {
        $begtag = '<title>';
        $endtag = '</title>';
        $begtag2 = '<h1>';
        $endtag2 = '</h1>';
        $next_action = 'check_indent';
        next;
      }
      if ($label eq 'SYNOPSIS') {
        $section_fmt = 1;
      }
      if ($label eq 'SEE ALSO') {
        $next_action = 'create_links';
      }
      if (($label =~ /OPTIONS$/) || ($label eq 'FILES')) {
        $section_pre = 1;
       print "</pre>\n";
#        print "</pre OPTION>\n";
      } elsif (/^[A-Z ]+\s*$/) {
        print "</pre>\n" if (($pre) || ($section_pre));
        $section_pre = 0;
      }
print "..$label..\n";
      if (/^[-A-Z ]+\s*$/) {
        print "<h2>$label</h2>\n";
        $blanks = 0;
        print "<pre>\n" if ($section_pre);
        next;
      }
      next;
    }
    if ($section_fmt) { print; $blanks = 0; next; }
    if ($next_action eq 'create_links') {
      # Parse see also looking for man page links.  Make it
      # call this program.  use '+' notation for spaces
      local($page);
      local($first) = 1;
      for $page (split(/,/)) {
        $page =~ tr/\x00-\x20//d; # Delete all control chars, spaces
        if ($page =~ /.+\(\d.*\).*$/) {
          $url_page = $page;
          $url_page =~ tr/()/[]/;
          print "," if (!$first);
          $first = 0;
          print "<a href=\"/tk2html?$url_page\">$page</a>\n";
        } else {
          print "," if (!$first);
          $first = 0;
          print "$page";
        }
      }
      next;
    }
    # This is to detect preformatted blocks.  I look at the first
    # line after header 'DESCRIPTION' and count the leading white
    # space as the "standard indent".  If I encounter a line with
    # a indent greater than the value of standard_indent then
    # surround it with <pre> and </pre>
    if ($next_action eq 'check_indent') {
      if (/^(\s+)\S+.*/) {
        $standard_indent = length($1);
        $next_action = '';
      }
    }
    #
    $before = length($_);
    $saved = $_;
    s/^[   ][   ]*//; # Delete leading whitespace
    $after = length($_);
    s/[   ][   ]*$//; # Delete trailing whitespace

    if ($begtag) {
      chop;
      print "$begtag$_$endtag\n";
      print "$begtag2$_$endtag2\n" if ($begtag2);
      $blanks = 0;
      $begtag2 = $endtag2 = $begtag = $endtag = '';
      next;
    }
    if ((!$section_fmt) && (!$section_pre) && ($standard_indent)) {
      if (($blanks == 1) && (!$pre) && ($after + $standard_indent) < $before) {
        $pre = 1;
        print "<pre>\n";
      } elsif (($pre) && ($after + $standard_indent) >= $before) {
        $pre = 0;
        print "</pre>\n";
      }
    }
    if (($section_pre) || ($pre)) {
      print "$saved";
      $blanks = 0;
      next;
    }
    # Handle word cont-
    # inuations
    if ($prefix) {
      print $prefix;
      $prefix = '';
    }
    if (/^(.+)\s+(\w+)\-\s*$/) {
      $prefix = $2;
      print "$1\n";
      $blanks = 0;
      next;
    }
    print;
    $blanks = 0;
  }
  close(MAN);
}

# EOF
