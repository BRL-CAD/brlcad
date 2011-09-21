#!/usr/bin/env perl

use strict;
use warnings;

use File::Basename;
use Data::Dumper;

use BRLCAD_DOC (
		'$XML_ASCII_HEADER',
		'$XML_UTF8_HEADER',
		'$ascii',
		'$utf8',
	       );

my $top_srcdir = qx(pwd);
chomp $top_srcdir;

# global vars

# user edit this file for local validation tool paths:
use BRLCAD_DB_VALIDATION;
my $JAVA     = $BRLCAD_DB_VALIDATION::JAVA;
my $MSVJAR   = $BRLCAD_DB_VALIDATION::MSVJAR;
my $XMLLINT  = $BRLCAD_DB_VALIDATION::XMLLINT;
my $ONVDLJAR = $BRLCAD_DB_VALIDATION::ONVDLJAR;

# for DocBook (DB) validation
my $XML_TMPFILE        = '.tmp';
my $RNG_SCHEMA         = './resources/docbook-5.0/rng/docbookxi.rng';

# for xmllint DB validation
my $XMLLINT_VALID_ARGS = "--xinclude --relaxng ${RNG_SCHEMA} --noout --nonet";

# for msv DB validation
my $MSVCMD        = "$JAVA -Xss1024K -jar ${MSVJAR}";

# for oNVDL validation
my $ONVDL_XI_ARGS = '-Dorg.apache.xerces.xni.parser.XMLParserConfiguration=org.apache.xerces.parsers.XIncludeParserConfiguration';
my $NVDL_SCHEMA   = './resources/docbook-5.0/docbookxi.nvdl';
my $NVDLCMD       = "$JAVA $ONVDL_XI_ARGS -jar $ONVDLJAR";

my $javawarned = 0;
my $vfil     = 'db-file-list.txt';
my $sfil     = 'find-db-files.pl';

my $prog = basename($0);
my $usage = "Usage: $prog --go | <input file> [options...]";
if (!@ARGV) {
  print <<"HERE";
$usage

Use option '--help' or '-h' for more details.
HERE
  exit;
}


# check that the paths are defined for the desired method
my %meth
  = (
     'xmllint' => {java => 0, var => '$XMLLINT',  fil => $XMLLINT},
     'msv'     => {java => 1, var => '$MSVJAR',   fil => $MSVJAR},
     'nvdl'    => {java => 1, var => '$ONVDLJAR', fil => $ONVDLJAR},
     'java'    => {           var => '$JAVA',     fil => $JAVA},
    );

my $enc      = undef;
my $meth     = 'msv';
my $stop     = 0;
my $ifil     = 0;
my $verbose  = 0;
my $warnings = 1; # for MSV
my $nvdlopts = '';
my $debug    = 0;

foreach my $arg (@ARGV) {
  my $val = undef;
  my $idx = index $arg, '=';

  if ($idx >= 0) {
    $val = substr $arg, $idx+1;
    $arg = substr $arg, 0, $idx;
  }

  if ($arg =~ /^[-]{1,2}h/i) {
    help(); # exits from help()
  }
  elsif ($arg =~ /^[-]{1,2}g/i) {
    ; # ignore
  }
  elsif ($arg =~ /^[-]{1,2}c/i) {
    check_methods(); # exits
  }
  elsif ($arg =~ /^[-]{1,2}v/i) {
    $verbose = 1;
  }
  elsif ($arg =~ /^[-]{1,2}no/i) {
    $warnings = 0;
  }
  elsif ($arg =~ /^[-]{1,2}nv/i) {
    # format: =A,B (comma separated list of one or more oNVDL
    #   options without hyphens)
    if ($val =~ s{-}{}g) {
      print "WARNING:  One or more hyphens found in nvdl options: '$val'...correcting.\n";
    }
    my @d = split(' ', $val);
    foreach my $d (@d) {
      $nvdlopts .= ' ' if $nvdlopts;
      $nvdlopts .= '-$d';
    }
  }
  elsif ($arg =~ /^[-]{1,2}s/i) {
    $stop = 1;
  }
  elsif ($arg =~ /^[-]{1,2}enc/i) {
    $enc = $val;
  }
  elsif ($arg =~ /^[-]{1,2}met/i) {
    die "ERROR:  Undefined method.\n"
      if !defined $val;
    $meth = $val;
    die "ERROR:  Method is unknown: '$meth'.\n"
      if ($meth !~ /xml/i && $meth !~ /msv/i && $meth !~ /nvd/i);
    $meth = ($meth =~ /xml/i) ? 'xmllint'
          : ($meth =~ /msv/i) ? 'msv' : 'nvdl';
  }
  elsif (!$ifil) {
    $ifil = $arg;
  }
  else {
    die "ERROR:  Unknown argument '$arg'.\n"
  }
}

#print "args = @ARGV\n";
#die "debug exit: \$stop = $stop";

# error checks
my $errors = 0;
# check for input file
if (!$ifil) {
  if (-f $vfil) {
    $ifil = $vfil;
  }
  else {
    print "ERROR: No input file entered.\n";
    ++$errors;
  }
}
elsif (! -f $ifil) {
  print "ERROR:  Input file '$ifil' not found.\n";
  ++$errors;
}

if (defined $enc) {
  if ($enc !~ /asc/i && $enc !~ /utf/i) {
    print "ERROR:  Encoding '$enc' is unknown.\n";
    ++$errors;
  }
  else {
    $enc = ($enc =~ /asc/i) ? $ascii : $utf8;
  }
}

# check method
$errors += check_method($meth);

die "ERROR exit.\n"
  if $errors;

open my $fp, '<', $ifil
  or die "$ifil: $!\n";

my %dbfils = ();
my @dbfils_asc = ();
my @dbfils_utf = ();

while (defined(my $line = <$fp>)) {
  # eliminate any comments
  my  $idx = index $line, '#';
  if ($idx >= 0) {
    $line = substr $line, 0, $idx;
  }

  # tokenize (may have one or more files on the line)
  my @d = split(' ', $line);
  next if !defined $d[0];
  while (@d) {
    my $f = shift @d;
    if (! -f $f) {
      print "WARNING:  DB file '$f' not found...skipping.\n"
	if $verbose;
      next;
    }
    # is the encoding determinate?
    my $tenc = $ascii;
    my $lang = undef;
    $f =~ m{(?: \A | [/\\]{1}) ([a-z]{2}) [/\\]{1} }xmsi;
    if ($1) {
      $lang = lc $1;
      print "NOTE:  Lang is '$lang'.\n"
	if $verbose;
      # we have a possible language couplet
      if ($lang eq 'en') {
        $tenc = $ascii;
      }
      else {
        $tenc = $utf8;
      }
    }
    elsif (defined $enc) {
      $tenc = $enc;
    }

    $dbfils{$f} = $tenc;
    if ($tenc eq $ascii) {
      push @dbfils_asc, $f;
    }
    else {
      push @dbfils_utf, $f;
    }

    $lang = 'UNKNOWN!' if !defined $lang;
    print "DB file '$f' language code '$lang', using encoding '$tenc'.\n"
	if $verbose;

  }
}
close $fp;

#print Dumper(\%dbfils);
#die "debug exit";

my $enc_prev = '';
my ($hdr, $typ);


foreach my $f (@dbfils_asc, @dbfils_utf) {
  my $enc = $dbfils{$f};

  print "DEBUG: enc ='$enc'; enc_prev = '$enc_prev'\n"
     if $debug;
  # set encoding-specific variables
  if (!$enc_prev || ($enc ne $enc_prev)) {
    if ($enc eq $ascii) {
      $hdr  = $XML_ASCII_HEADER;
      $typ = 'ASCII';
    }
    else {
      $hdr  = $XML_UTF8_HEADER;
      $typ = 'UTF8';
    }
  }
  $enc_prev = $enc;

  print "=== $typ VALIDATION (method: $meth) ===\n";

  my $dir = dirname($f);
  my $fil = basename($f);
  my $tmpfil = "${dir}/${XML_TMPFILE}.${fil}";
  unlink $tmpfil if -f $tmpfil;
  qx(echo "$hdr" > $tmpfil);
  qx(cat $f >> $tmpfil);
  print "=== processing file '$f' (see file '$tmpfil')\n";

  my $exit_status = 0;
  if ($meth eq 'msv') {
    $exit_status = validate_msv($tmpfil);
  }
  elsif ($meth eq 'xmllint') {
    $exit_status = validate_xmllint($tmpfil);
  }
  else {
    $exit_status = validate_nvdl($tmpfil);
  }
  if ($exit_status) {
    print "=== INVALID: '$f'\n";
    if ($stop) {
      print "=== stopping after validation failure: '$f'\n";
      print "=== finished processing file '$f' (see file '$tmpfil')\n";
      exit;
    }
  }
  else {
    print "=== VALID: '$f'\n";
  }

  print "=== finished processing file '$f' (see file '$tmpfil')\n";
}

print "Normal end.\n";

#### SUBROUTINES ####
sub validate_xmllint {
  my $tmpfil = shift @_;

  my $cmd = "$XMLLINT $XMLLINT_VALID_ARGS $tmpfil";
  print "=== cmd: '$cmd'\n";

  system($cmd);

  my $exit_status = $?;
  if ($exit_status == -1) {
    print "failed to execute: $!\n";
    print "cmd = '$cmd'\n";
    return 1;
  }

  $exit_status >>= 8;

  return $exit_status;
} # validate_xmllint

sub validate_msv {
  my $tmpfil = shift @_;

  my $warn = $warnings ? '-warning' : '';
  my $cmd = "$MSVCMD $warn $RNG_SCHEMA $tmpfil";
  print "=== cmd: '$cmd'\n";

  system($cmd);

  my $exit_status = $?;
  if ($exit_status == -1) {
    print "failed to execute: $!\n";
    print "cmd = '$cmd'\n";
    return 1;
  }

  $exit_status >>= 8;

  return $exit_status;

} # validate_msv

sub validate_nvdl {
  my $tmpfil = shift @_;

  my $cmd = "$NVDLCMD $nvdlopts $NVDL_SCHEMA $tmpfil;";
  print "=== cmd: '$cmd'\n";

  system($cmd);

  my $exit_status = $?;
  if ($exit_status == -1) {
    print "failed to execute: $!\n";
    print "cmd = '$cmd'\n";
    return 1;
  }

  $exit_status >>= 8;

  return $exit_status;

} # validate_nvdl


sub help {
  print <<"HERE";
$usage

Uses one of three methods to validate a DocBook xml source file.
Input is a list of source files to validate.  Output is to stdout
and stderr which may be redirected to one or two files.

Options:

  --encoding=E    where E is one of '$ascii' or '$utf8' [default: auto]

                  If the file path contains a two-letter language
                  code bounded by path separators or a relative
                  path starting with the language code, the '/en/'
                  (or '\\en\\' or ' en/' or ' en\\') files will use
                  '$ascii' and all others will use '$utf8'.  The
                  default is '$ascii' otherwise.

  --method=M      where M is one of 'msv, 'xmllint', or 'nvdl'
                  [default: msv]

                  The 'msv' method use the Sun Multi-Schema Validator
                  (MSV) and the 'docbook-5.0/docbookxi.rng' schema.

                  The 'xmllint' method uses libXML2's xmllint plus the
                  'docbook-5.0/rng/docbookxi.rng' schema.

                  The 'nvdl' method uses the Apache Xerces xml parser
                  and the 'docbook-5.0/docbookxi.nvdl' schema.

  --stop-on-fail  stop after the first validation failure

  --verbose       show more info about what's going on

  --no-warnings   do not show validation warnings (used for 'msv' only)

  --nvdl=A[,B]    pass one or more options to the 'nvdl' method (do not
                  add the leading hyphen--it is automatically added):

                  -c
                      The schema uses RELAX NG Compact Syntax.

                  -e enc
                      Uses the encoding enc to read the schema.

                  -f
                      Checks that the document is feasibly valid. A
                      document is feasibly valid if it could be
                      transformed into a valid document by inserting
                      any number of attributes and child elements
                      anywhere in the tree. This is equivalent to
                      transforming the schema by wrapping every data,
                      list, element and attribute element in an
                      optional element and then validating against
                      the transformed schema. This option may be useful
                      while a document is still under construction. This
                      option also disables checking that for every IDREF
                      there is a corresponding ID.

                  -i
                      Disables checking of ID/IDREF/IDREFS. By default,
                      Jing enforces the constraints imposed by RELAX NG
                      DTD Compatibility with respect to ID/IDREF/IDREFS.

                  -t
                      Prints the time used by oNVDL for loading the
                      schema and for validation.

  --check         check to see what methods have requirements met

Notes:

File and directory names must not have spaces.

The list file may be generated with the shell script '$sfil'
The output file is named '$vfil'.  If the file name
'$vfil' is found it will be used automatically if no file
name is entered at the prompt (in that case at least on arg must be
entered so use '--go' if no other arg is needed).

In the list file all blank lines and all characters on a line at and
after the '#' symbol are ignored.

HERE
  exit;
} # help

sub check_method {
  my $meth = shift @_;

  my $metherrors = 0;
  my $errors     = 0;

  my $java = exists $meth{$meth}{java} ? $meth{$meth}{java} : 0;
  my $var  = $meth{$meth}{var};
  my $fil  = $meth{$meth}{fil};
  if ($java && !$javawarned) {
    if (!defined $JAVA) {
      print "ERROR:  Variable '\$JAVA' not defined.\n";
      ++$metherrors;
      ++$javawarned;
    }
    elsif (!-f $JAVA) {
      print "ERROR:  File '$JAVA' (variable '\$JAVA') not found.\n";
      ++$javawarned;
      ++$metherrors;
    }
  }

  if (!defined $fil) {
    print "ERROR:  Variable '$var' not defined.\n";
    ++$metherrors;
  }
  elsif (!-f $fil) {
    print "ERROR:  File '$fil' (variable '$var') not found.\n";
    ++$metherrors;
  }

  if ($metherrors) {
    print "ERROR:  Tool requirements for method '$meth' not met.\n";
    ++$errors;
  }

  return $errors;

} # check_method

sub check_methods {
  my $errors = 0;
  foreach my $meth (keys %meth) {
    my $err = check_method($meth);
    if (!$err) {
      print "Method '$meth' has a tool defined and in place.\n";
    }
    else {
      ++$errors;
    }
  }
  if ($errors) {
    print "One or more methods are not available.\n";
  }
  exit;
} # check_methods
