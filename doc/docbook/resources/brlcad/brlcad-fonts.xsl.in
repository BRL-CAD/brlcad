<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:fo="http://www.w3.org/1999/XSL/Format"
  xmlns:d="http://docbook.org/ns/docbook"
  exclude-result-prefixes="d"
  version="1.0"
>

  <!-- this is based on the fonts style sheet of courtesy of Dean Nelson
       <deannelson@aol.com>, 2010-06-28, from the docbook-apps mailing list

       Dean's original fonts:

  <xsl:param name="body.font.family" select="'DejaVuSans'"/>
  <xsl:param name="title.font.family" select="'DejaVuSans'"/>
  <xsl:param name="monospace.font.family" select="'DejaVuSansMono'"/>
  <xsl:param name="body.font.master" select="'10.1'"/>
  <xsl:param name="symbol.font.family" select="'DejaVuSansMono'"/>

-->

  <!-- ============= Fonts ==================== -->

  <xsl:param name="body.font.family">STIXGeneral</xsl:param>
  <xsl:param name="symbol.font.family">STIXGeneral</xsl:param>

  <xsl:param name="title.font.family">DejaVuLGCSans</xsl:param>
  <xsl:param name="monospace.font.family">DejaVuLGCSansMono</xsl:param>

  <xsl:param name="body.font.master">11.0</xsl:param>


    <!-- ============== Symbol font use ================== -->
    <!-- use non ascii letters -->
    <xsl:template match="d:symbol[@role = 'symbolfont']">
        <fo:inline font-family="Symbol">
            <xsl:apply-templates/>
            <!--     <xsl:call-template name="inline.charseq"/> -->
        </fo:inline>
    </xsl:template>
    
    <!-- ============== Bold AND Italic fonts  ================== -->
    <xsl:template match="d:emphasis[@role='bold-italic']|d:emphasis[@role='italic-bold']">
        <fo:inline font-weight="bold" font-style="italic">
            <xsl:apply-templates/>
        </fo:inline>
    </xsl:template>


    <!-- ============== Underline fonts  ================== -->
    <xsl:template match="d:emphasis[@role='underline']">
        <fo:inline text-decoration="underline">
            <xsl:apply-templates/>
        </fo:inline>
    </xsl:template>   
    <xsl:template match="d:emphasis[@role='underline-bold']|d:emphasis[@role='bold-underline']">
        <fo:inline font-weight="bold" text-decoration="underline">
            <xsl:apply-templates/>
        </fo:inline>
    </xsl:template>
    
    <!-- ============== Strike Through fonts  ================== -->
    <xsl:template match="d:emphasis[@role='strikethrough']">
        <fo:inline text-decoration="line-through">
            <xsl:apply-templates/>
        </fo:inline>
    </xsl:template>
 
    <!-- Change the font size of a para (from Dean Nelson) -->
    <xsl:template match="d:para[@font-size]">
       <fo:block font-size="{@font-size}">
          <xsl:apply-templates/>
       </fo:block>
    </xsl:template>

    <!-- Change the font size of emphasis -->
    <xsl:template match="d:emphasis[@role='smallfont']">
        <fo:inline font-size="8pt" font-style="normal">
            <xsl:apply-templates/>
        </fo:inline>
    </xsl:template>

</xsl:stylesheet>
