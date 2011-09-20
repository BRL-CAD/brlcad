#!/usr/bin/env perl

# this generates both a catalog file and the special catalog file used
# for fop

use strict;
use warnings;

use BRLCAD_DOC;

my $xmlcat = $BRLCAD_DOC::genxmlcat;
my $fopcat = $BRLCAD_DOC::genfopxmlcat;

# this needs to be adjusted if the package is moved
my $home   = '.';

# Note: no going back to pre-namespace days
my $dbdir = 'docbook-xsl-ns-1.76.1';

my $dbht    = "$vhome/$dbdir/html/chunk.xsl";
my $dbht2   = "$vhome/$dbdir/html/docbook.xsl";

my $xdbht    = "$vhome/$dbdir/xhtml/chunk.xsl";
my $xdbht2   = "$vhome/$dbdir/xhtml/docbook.xsl";

my $dbfo    = "$vhome/$dbdir/fo/docbook.xsl";

# instead of one-to-one mappings, may use rewrite entries--see p. 51 of
# Bob Stayton's book
my $original  = 'http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd';
my $svgdtd    = "$vhome/doc/docbook_userguide/misc-files/svg10.dtd";

my $original2 = 'http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd';
my $svgdtd2   = "$vhome/doc/docbook_userguide/misc-files/svg11-tiny-flat.dtd";

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

DOC::print_autogen_header($fp, $0, 'xmlhdr');
print $fp <<"FILE1";
<catalog xmlns='urn:oasis:names:tc:entity:xmlns:xml:catalog'>

  <!-- use local versions of some DTDs -->
  <system
      systemId='$original2'
      uri='file://$svgdtd2'
  />

  <rewriteSystem
      systemIdStartString='http://www.w3.org/Graphics/SVG/1.1/DTD/'
      rewritePrefix='file://$vhome/doc/docbook_userguide/misc-files/'
  />
  <rewriteURI
      uriStartString='http://www.w3.org/Graphics/SVG/1.1/DTD/'
      rewritePrefix='file://$vhome/doc/docbook_userguide/misc-files/'
  />

  <uri
      name='html/chunk.xsl'
      uri='file://$dbht'
  />
  <uri
      name='html/docbook.xsl'
      uri='file://$dbht2'
  />

  <uri
      name='xhtml/chunk.xsl'
      uri='file://$xdbht'
  />
  <uri
      name='xhtml/docbook.xsl'
      uri='file://$xdbht2'
  />


  <uri
      name='fo/docbook.xsl'
      uri='file://$dbfo'
  />


</catalog>
FILE1

close $fp;

# the special file for the fop catalog manager
open $fp, '>', $fopcat
  or die "$fopcat: $!";
print $fp <<"FILE2";
catalogs=catalog.xml;./$xmlcat
relative-catalogs=false
static-catalog=yes
catalog-class-name=org.apache.xml.resolver.Resolver
verbosity=4
FILE2

close $fp;
