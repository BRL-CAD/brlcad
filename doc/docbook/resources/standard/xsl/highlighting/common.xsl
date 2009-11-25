<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		xmlns:hl="java:net.sf.xslthl.ConnectorSaxon6"
                xmlns:exsl="http://exslt.org/common"
		exclude-result-prefixes="exsl hl"
                version='1.0'>

<!-- ********************************************************************
     $Id$
     ********************************************************************

     This file is part of the XSL DocBook Stylesheet distribution.
     See ../README or http://docbook.sf.net/release/xsl/current/ for
     and other information.

     ******************************************************************** -->

<!-- You can override this template to do more complex mapping of
     language attribute to highlighter language ID (see xslthl-config.xml) -->
<xsl:template name="language.to.xslthl">
  <xsl:param name="context"/>

  <xsl:choose>
    <xsl:when test="$context/@language != ''">
      <xsl:value-of select="$context/@language"/>
    </xsl:when>
    <xsl:when test="$highlight.default.language != ''">
      <xsl:value-of select="$highlight.default.language"/>
    </xsl:when>
  </xsl:choose>
</xsl:template>

<xsl:template name="apply-highlighting">
  <xsl:choose>
    <!-- Do we want syntax highlighting -->
    <xsl:when test="$highlight.source != 0 and function-available('hl:highlight')">
      <xsl:variable name="language">
	<xsl:call-template name="language.to.xslthl">
	  <xsl:with-param name="context" select="."/>
	</xsl:call-template>
      </xsl:variable>
      <xsl:choose>
	<xsl:when test="$language != ''">
	  <xsl:variable name="content">
	    <xsl:apply-templates/>
	  </xsl:variable>
	  <xsl:apply-templates select="hl:highlight($language, exsl:node-set($content))"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:apply-templates/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <!-- No syntax highlighting -->
    <xsl:otherwise>
      <xsl:apply-templates/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

</xsl:stylesheet>

