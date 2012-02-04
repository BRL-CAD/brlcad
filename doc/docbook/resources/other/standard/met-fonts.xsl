<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
    xmlns:fo="http://www.w3.org/1999/XSL/Format" >


  <!-- ============= Fonts ==================== -->
  <!--   <xsl:param name="body.font.master" select="'12'"/> -->

  <xsl:param name="body.font.family" select="'DejaVuSans'"/>
  <xsl:param name="title.font.family" select="'DejaVuSans'"/>
  <xsl:param name="monospace.font.family" select="'DejaVuSansMono'"/>
  <xsl:param name="body.font.master" select="'10.1'"/>
  
  <xsl:param name="symbol.font.family" select="'DejaVuSansMono'"/> 
  
<!--    <xsl:param name="symbol.font.family" select="'Arialuni'"/> -->



    <!-- ============== Symbol font use ================== -->
    <!-- use non ascii letters -->
    <xsl:template match="symbol[@role = 'symbolfont']">
        <fo:inline font-family="Symbol">
            <xsl:apply-templates/>
            <!--     <xsl:call-template name="inline.charseq"/> -->
        </fo:inline>
    </xsl:template>
    
    <!-- ============== Bold AND Italic fonts  ================== -->
    <xsl:template match="emphasis[@role='bold-italic']|emphasis[@role='italic-bold']">
        <fo:inline font-weight="bold" font-style="italic">
            <xsl:apply-templates/>
        </fo:inline>
    </xsl:template>
    
    <!-- ============== Underline fonts  ================== -->
    <xsl:template match="emphasis[@role='underline']">
        <fo:inline text-decoration="underline">
            <xsl:apply-templates/>
        </fo:inline>
    </xsl:template>   
    <xsl:template match="emphasis[@role='underline-bold']|emphasis[@role='bold-underline']">
        <fo:inline font-weight="bold" text-decoration="underline">
            <xsl:apply-templates/>
        </fo:inline>
    </xsl:template>
    
    <!-- ============== Strike Through fonts  ================== -->
    <xsl:template match="emphasis[@role='strikethrough']">
        <fo:inline text-decoration="line-through">
            <xsl:apply-templates/>
        </fo:inline>
    </xsl:template>
 
	 <!-- Change the font size of the emphasis --> 
	<xsl:template match="emphasis[@font-size]">
	           <fo:inline font-size="{@font-size}">
	               <xsl:apply-templates/>
	          </fo:inline>
	</xsl:template>
 
 
 
</xsl:stylesheet>
