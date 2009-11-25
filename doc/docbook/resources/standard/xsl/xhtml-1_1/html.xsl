<?xml version="1.0" encoding="ASCII"?>
<!--This file was created automatically by html2xhtml-->
<!--from the HTML stylesheets.-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml" version="1.0">

<!-- ********************************************************************
     $Id$
     ********************************************************************

     This file is part of the XSL DocBook Stylesheet distribution.
     See ../README or http://docbook.sf.net/release/xsl/current/ for
     copyright and other information.

     ******************************************************************** -->

<!-- The generate.html.title template is currently used for generating HTML -->
<!-- "title" attributes for some inline elements only, but not for any -->
<!-- block elements. It is called in eleven places in the inline.xsl -->
<!-- file. But it's called by all the inline.* templates (e.g., -->
<!-- inline.boldseq), which in turn are called by other (element) -->
<!-- templates, so it results, currently, in supporting generation of the -->
<!-- HTML "title" attribute for a total of about 92 elements. -->
<!-- You can use mode="html.title.attribute" to get a title for -->
<!-- an element specified by a param, including targets of cross references. -->
<xsl:template name="generate.html.title">
  <xsl:apply-templates select="." mode="html.title.attribute"/>
</xsl:template>

<!-- Generate a title attribute for the context node -->
<xsl:template match="*" mode="html.title.attribute">
  <xsl:variable name="is.title">
    <xsl:call-template name="gentext.template.exists">
      <xsl:with-param name="context" select="'title'"/>
      <xsl:with-param name="name" select="local-name(.)"/>
      <xsl:with-param name="lang">
        <xsl:call-template name="l10n.language"/>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="is.title-numbered">
    <xsl:call-template name="gentext.template.exists">
      <xsl:with-param name="context" select="'title-numbered'"/>
      <xsl:with-param name="name" select="local-name(.)"/>
      <xsl:with-param name="lang">
        <xsl:call-template name="l10n.language"/>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="is.title-unnumbered">
    <xsl:call-template name="gentext.template.exists">
      <xsl:with-param name="context" select="'title-unnumbered'"/>
      <xsl:with-param name="name" select="local-name(.)"/>
      <xsl:with-param name="lang">
        <xsl:call-template name="l10n.language"/>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="gentext.title">
    <xsl:if test="$is.title != 0 or                   $is.title-numbered != 0 or                   $is.title-unnumbered != 0">
      <xsl:apply-templates select="." mode="object.title.markup.textonly"/>
    </xsl:if>
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="string-length($gentext.title) != 0">
      <xsl:attribute name="title">
        <xsl:value-of select="$gentext.title"/>
      </xsl:attribute>
    </xsl:when>
    <!-- Fall back to alt if available -->
    <xsl:when test="alt">
      <xsl:attribute name="title">
        <xsl:value-of select="normalize-space(alt)"/>
      </xsl:attribute>
    </xsl:when>
  </xsl:choose>

</xsl:template>

<xsl:template name="dir">
  <xsl:param name="inherit" select="0"/>

  <xsl:variable name="dir">
    <xsl:choose>
      <xsl:when test="@dir">
        <xsl:value-of select="@dir"/>
      </xsl:when>
      <xsl:when test="$inherit != 0">
        <xsl:value-of select="ancestor::*/@dir[1]"/>
      </xsl:when>
    </xsl:choose>
  </xsl:variable>

  <xsl:if test="$dir != ''">
    <xsl:attribute name="dir">
      <xsl:value-of select="$dir"/>
    </xsl:attribute>
  </xsl:if>
</xsl:template>

<xsl:template name="anchor">
  <xsl:param name="node" select="."/>
  <xsl:param name="conditional" select="1"/>
  <xsl:variable name="id">
    <xsl:call-template name="object.id">
      <xsl:with-param name="object" select="$node"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:if test="$conditional = 0 or $node/@id or $node/@xml:id">
    <a id="{$id}"/>
  </xsl:if>
</xsl:template>

<xsl:template name="href.target.uri">
  <xsl:param name="context" select="."/>
  <xsl:param name="object" select="."/>
  <xsl:text>#</xsl:text>
  <xsl:call-template name="object.id">
    <xsl:with-param name="object" select="$object"/>
  </xsl:call-template>
</xsl:template>

<xsl:template name="href.target">
  <xsl:param name="context" select="."/>
  <xsl:param name="object" select="."/>
  <xsl:text>#</xsl:text>
  <xsl:call-template name="object.id">
    <xsl:with-param name="object" select="$object"/>
  </xsl:call-template>
</xsl:template>

<xsl:template name="href.target.with.base.dir">
  <xsl:param name="context" select="."/>
  <xsl:param name="object" select="."/>
  <xsl:if test="$manifest.in.base.dir = 0">
    <xsl:value-of select="$base.dir"/>
  </xsl:if>
  <xsl:call-template name="href.target">
    <xsl:with-param name="context" select="$context"/>
    <xsl:with-param name="object" select="$object"/>
  </xsl:call-template>
</xsl:template>

<xsl:template name="dingbat">
  <xsl:param name="dingbat">bullet</xsl:param>
  <xsl:call-template name="dingbat.characters">
    <xsl:with-param name="dingbat" select="$dingbat"/>
  </xsl:call-template>
</xsl:template>

<xsl:template name="dingbat.characters">
  <!-- now that I'm using the real serializer, all that dingbat malarky -->
  <!-- isn't necessary anymore... -->
  <xsl:param name="dingbat">bullet</xsl:param>
  <xsl:choose>
    <xsl:when test="$dingbat='bullet'">&#8226;</xsl:when>
    <xsl:when test="$dingbat='copyright'">&#169;</xsl:when>
    <xsl:when test="$dingbat='trademark'">&#8482;</xsl:when>
    <xsl:when test="$dingbat='trade'">&#8482;</xsl:when>
    <xsl:when test="$dingbat='registered'">&#174;</xsl:when>
    <xsl:when test="$dingbat='service'">(SM)</xsl:when>
    <xsl:when test="$dingbat='nbsp'">&#160;</xsl:when>
    <xsl:when test="$dingbat='ldquo'">&#8220;</xsl:when>
    <xsl:when test="$dingbat='rdquo'">&#8221;</xsl:when>
    <xsl:when test="$dingbat='lsquo'">&#8216;</xsl:when>
    <xsl:when test="$dingbat='rsquo'">&#8217;</xsl:when>
    <xsl:when test="$dingbat='em-dash'">&#8212;</xsl:when>
    <xsl:when test="$dingbat='mdash'">&#8212;</xsl:when>
    <xsl:when test="$dingbat='en-dash'">&#8211;</xsl:when>
    <xsl:when test="$dingbat='ndash'">&#8211;</xsl:when>
    <xsl:otherwise>
      <xsl:text>&#8226;</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="id.warning">
  <xsl:if test="$id.warnings != 0 and not(@id) and not(@xml:id) and parent::*">
    <xsl:variable name="title">
      <xsl:choose>
        <xsl:when test="title">
          <xsl:value-of select="title[1]"/>
        </xsl:when>
        <xsl:when test="substring(local-name(*[1]),                                   string-length(local-name(*[1])-3) = 'info')                         and *[1]/title">
          <xsl:value-of select="*[1]/title[1]"/>
        </xsl:when>
        <xsl:when test="refmeta/refentrytitle">
          <xsl:value-of select="refmeta/refentrytitle"/>
        </xsl:when>
        <xsl:when test="refnamediv/refname">
          <xsl:value-of select="refnamediv/refname[1]"/>
        </xsl:when>
      </xsl:choose>
    </xsl:variable>

    <xsl:message>
      <xsl:text>ID recommended on </xsl:text>
      <xsl:value-of select="local-name(.)"/>
      <xsl:if test="$title != ''">
        <xsl:text>: </xsl:text>
        <xsl:choose>
          <xsl:when test="string-length($title) &gt; 40">
            <xsl:value-of select="substring($title,1,40)"/>
            <xsl:text>...</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$title"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:if>
    </xsl:message>
  </xsl:if>
</xsl:template>

<xsl:template match="*" mode="class.attribute">
  <xsl:param name="class" select="local-name(.)"/>
  <!-- permit customization of class attributes -->
  <!-- Use element name by default -->
  <xsl:attribute name="class">
    <xsl:apply-templates select="." mode="class.value">
      <xsl:with-param name="class" select="$class"/>
    </xsl:apply-templates>
  </xsl:attribute>
</xsl:template>

<xsl:template match="*" mode="class.value">
  <xsl:param name="class" select="local-name(.)"/>
  <!-- permit customization of class value only -->
  <!-- Use element name by default -->
  <xsl:value-of select="$class"/>
</xsl:template>

</xsl:stylesheet>
