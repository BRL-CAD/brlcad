<?xml version='1.0' encoding='us-ascii'?>

<xsl:stylesheet
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:fo="http://www.w3.org/1999/XSL/Format"
  xmlns:xi="http://www.w3.org/2001/XInclude"
  xmlns:d="http://docbook.org/ns/docbook"
  exclude-result-prefixes="d"
  version="1.0"
>

  <!-- version control data -->
  <xsl:param name="brlcad.vcs">Subversion</xsl:param>
  <xsl:param name="brlcad.revision">r4321</xsl:param>
  <xsl:param name="brlcad.pdf.gendate">2011-09-06</xsl:param>


  <!-- set default page citation style for xref from "[12]" to "12" -->
  <!-- from Bob Stayton's book -->
  <xsl:param name="local.l10n.xml" select="document('')" />
  <l:i18n xmlns:l="http://docbook.sourceforge.net/xmlns/l10n/1.0">
    <l:l10n language="en">
    <l:context name="xref">
      <l:template name="page.citation" text=" [page %p]"/>
    </l:context>
    </l:l10n>
  </l:i18n>


</xsl:stylesheet>
