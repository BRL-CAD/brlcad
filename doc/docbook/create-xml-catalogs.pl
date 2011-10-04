#!/usr/bin/env perl

# this generates both a catalog file and the special catalog file used
# for fop

use strict;
use warnings;

use BRLCAD_DOC (
		'$genxmlcat',
		'$genfopxmlcat',
	       );
use DBPATH;

# this absolute path may need to be adjusted if the package is moved
my $dbhome = $DBPATH::DBHOME;

# Note: no going back to pre-namespace days
my $dbdir  = 'resources/other/standard/xsl';
my $dbdir2 = 'resources/other/standard/svg';
my $brldir = 'resources/brlcad';

my $dbht   = "$dbhome/$dbdir/xhtml-1_1/docbook.xsl";
my $dbfo   = "$dbhome/$dbdir/fo/docbook.xsl";
my $dbman  = "$dbhome/$dbdir/manpages/docbook.xsl";

# the two auto-generated catalog files:
my $xmlcat = "${brldir}/${genxmlcat}";
my $fopcat = "${brldir}/${genfopxmlcat}";

# instead of one-to-one mappings, may use rewrite entries--see p. 51 of
# Bob Stayton's book
my $original  = 'http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd';
my $svgdtd    = "$dbhome/$dbdir2/svg10.dtd";

my $original2 = 'http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd';
my $svgdtd2   = "$dbhome/$dbdir2/svg11-tiny-flat.dtd";

# create xml catalog file
open my $fp, '>', $xmlcat
  or die "$xmlcat: $!";

# Note that the following dtd is removed to eliminate problems with
# internet connectivity (see explanation in Bob Stayton's book at the
# top of page 49):

#<!DOCTYPE catalog
#   PUBLIC '-//OASIS/DTD Entity Resolution XML Catalog V1.0//EN'
#   'http://www.oasis-open.org/committees/entity/release/1.0/catalog.dtd'>

=pod

  <system
      systemId='$original'
      uri='file://$svgdtd'
  />

=cut

BRLCAD_DOC::print_autogen_header($fp, $0, 'xmlhdr');
print $fp <<"FILE1";
<catalog xmlns='urn:oasis:names:tc:entity:xmlns:xml:catalog'>

  <!-- use local versions of some DTDs -->
  <system
      systemId='$original2'
      uri='file://$svgdtd2'
  />

  <rewriteSystem
      systemIdStartString='http://www.w3.org/Graphics/SVG/1.1/DTD/'
      rewritePrefix='file://$dbhome/$dbdir2/'
  />

  <rewriteURI
      uriStartString='http://www.w3.org/Graphics/SVG/1.1/DTD/'
      rewritePrefix='file://$dbhome/$dbdir2/'
  />

  <rewriteURI
      uriStartString='/xsl/'
      rewritePrefix='file://$dbhome/$dbdir/'
  />

  <rewriteURI
      uriStartString='/brlcad/'
      rewritePrefix='file://$dbhome/$brldir/'
  />

  <rewriteURI
      uriStartString='/doc/'
      rewritePrefix='file://$dbhome/'
  />


</catalog>
FILE1

close $fp;

# the special file for the fop catalog manager
open $fp, '>', $fopcat
  or die "$fopcat: $!";
print $fp <<"FILE2";
catalogs=catalog.xml;./$genxmlcat
relative-catalogs=true
static-catalog=yes
catalog-class-name=org.apache.xml.resolver.Resolver
verbosity=4
FILE2

close $fp;
