<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:fo="http://www.w3.org/1999/XSL/Format"
                xmlns:xi="http://www.w3.org/2001/XInclude"
                xmlns:fox="http://xmlgraphics.apache.org/fop/extensions"
                version="1.0"
>

<!-- use prog 'create-book-covers.pl' -->

<xsl:include href="brlcad-gendata.xsl"/>
<!-- original
<xsl:include href="brlcad-colors-autogen.xsl"/>
-->
<?brlcad insert-color-file-name ?>

<!-- ==================================================================== -->
<xsl:template name="front.cover">

  <xsl:variable name='logosize'>40pt</xsl:variable>
  <xsl:variable name='brlcadsize'>40pt</xsl:variable><!-- started with 40pt -->

  <fo:page-sequence master-reference="front-cover">

    <!--
      force-page-count="no-force">
    -->

    <fo:flow flow-name="xsl-region-body">

      <?brlcad insert-draft-overlay ?>

      <!-- BRL-CAD LOGO ====================================================== -->
      <!-- this is the BRL-CAD Logo; point size is for the 'BRL-CAD', the rest
      of the container has inherited sizes scaled as a proportion of that size
      so as to meet the company logo rules -->
      <!-- final position on the page should have the top at 0.35in -->

      <!-- &#xAE => R ; C => &#xA9; -->
      <!-- reg mark looks like 0.4 size of normal 'n' -->

<!--
      <fo:block-container font-size="{$logosize}"
         line-height='50.0%'
         text-align="right"
         font-family='Bembo'
         absolute-position='fixed'
         right='0.50in'
         top='0.40in'
         >
        <fo:block color='red'>
          BRL-CAD<fo:inline font-size='29.8%'
             baseline-shift='1.0%'>&#xAE;</fo:inline>
        </fo:block>
      </fo:block-container>
-->

      <!-- TOP RULE ================================ -->
      <fo:block-container  top="1in" absolute-position="absolute">
        <fo:block text-align='center'>
           <fo:leader leader-length="8.5in"
             leader-pattern="rule"
             alignment-baseline="middle"
             rule-thickness="2pt" color="{$brlcad.cover.color}"/>
        </fo:block>
      </fo:block-container>

      <?brlcad insert-brlcad-logo-group ?>

      <!-- DOCUMENT TITLE ================================================== -->
      <?brlcad insert-title ?>

      <!-- BOTTOM  RULE ================================ -->
      <fo:block-container top="10in" absolute-position="absolute">
        <fo:block text-align='center'>
           <fo:leader leader-length="8.5in"
             leader-pattern="rule"
             alignment-baseline="middle"
             rule-thickness="2pt" color="{$brlcad.cover.color}"/>
        </fo:block>
      </fo:block-container>

      <!-- BOTTOM DISCLAIMER ================================ -->
      <fo:block-container absolute-position="absolute" top="10.25in" left="0.5in"
          right="0.5in" bottom="1in" text-align="center" font-family="serif">
        <fo:block>Approved for public release; distribution is unlimited.</fo:block>
      </fo:block-container>


    </fo:flow>
  </fo:page-sequence>
</xsl:template>



<!-- ==================================================================== -->
<xsl:template name="back.cover">

  <fo:page-sequence master-reference="back-cover"
    initial-page-number="auto-even"
    >

    <fo:flow flow-name="xsl-region-body">

      <!-- TOP RULE ================================ -->
      <fo:block-container  top="1in" absolute-position="absolute">
        <fo:block text-align='center'>
           <fo:leader leader-length="8.5in"
             leader-pattern="rule"
             alignment-baseline="middle"
             rule-thickness="2pt" color="{$brlcad.cover.color}"/>
        </fo:block>
      </fo:block-container>

      <!-- BOTTOM  RULE ================================ -->
      <fo:block-container top="10in" absolute-position="absolute">
        <fo:block text-align='center'>
           <fo:leader leader-length="8.5in"
             leader-pattern="rule"
             alignment-baseline="middle"
             rule-thickness="2pt" color="{$brlcad.cover.color}"/>
        </fo:block>
      </fo:block-container>

    </fo:flow>
  </fo:page-sequence>
</xsl:template>

<!-- ==================================================================== -->
  <!-- page sequence is defined as follows: -->
  <xsl:template name="user.pagemasters">

    <fo:simple-page-master master-name="front-cover" page-width="{$page.width}"
      page-height="{$page.height}" margin-top="0pt" margin-bottom="0pt"
      margin-left="0pt" margin-right="0pt">

      <fo:region-body margin="0in"/>

      <!--
      <fo:region-before extent="1in" background-color='lightblue'/>
      <fo:region-after extent="1in" background-color='lightblue'/>
      <fo:region-start extent="1in" background-color='lightgreen'/>
      <fo:region-end extent="1in" background-color='lightgreen'/>
      -->

    </fo:simple-page-master>

    <fo:simple-page-master master-name="back-cover" page-width="{$page.width}"
      page-height="{$page.height}" margin-top="0pt" margin-bottom="0pt"
      margin-left="0pt" margin-right="0pt">

      <fo:region-body margin="0in"/>

    </fo:simple-page-master>

  </xsl:template>



</xsl:stylesheet>
