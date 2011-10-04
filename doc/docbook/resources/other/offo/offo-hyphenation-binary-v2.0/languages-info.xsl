<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema" exclude-result-prefixes="xs" version="1.0">

  <xsl:output method="html"/>

  <xsl:template match="/languages-info">
    <table class="grid" width="100%" cellpadding="3" cellspacing="2" border="1">
      <tr>
        <th>name</th>
        <th>TeX code</th>
        <th>FOP code</th>
        <th>info</th>
      </tr>
      <xsl:apply-templates/>
    </table>
  </xsl:template>

  <xsl:template match="language">
    <tr>
      <td>
        <xsl:apply-templates select="name"/>
      </td>
      <td>
        <xsl:apply-templates select="tex-code"/>
      </td>
      <td>
        <xsl:apply-templates select="code"/>
      </td>
      <td>
        <xsl:apply-templates select="message"/>
      </td>
    </tr>
  </xsl:template>

  <xsl:template match="/">
    <html>
      <xsl:call-template name="make-head"/>
      <body class="composite" onload="focus()">
        <xsl:call-template name="make-banner"/>
        <xsl:call-template name="make-toptabs"/>
        <xsl:call-template name="make-breadcrumbs"/>
        <xsl:call-template name="make-main"/>
        <xsl:call-template name="make-footer"/>
      </body>
    </html>
  </xsl:template>

  <xsl:template name="make-head">
    <head>
      <META http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
      <meta content="Apache Forrest" name="Generator"/>
      <meta name="Forrest-version" content="0.6"/>
      <meta name="Forrest-skin-name" content="tigris"/>
      <style type="text/css">
        /*  */
        @import "skin/tigris.css";
        @import "skin/quirks.css";
        @import "skin/inst.css";
        /*   */</style>
      <link href="skin/forrest.css" type="text/css" rel="stylesheet"/>
      <script type="text/javascript" src="skin/tigris.js"/>
      <script src="skin/menu.js" language="javascript" type="text/javascript"/>
      <title>Information about included languages</title>
      <meta content="text/css" http-equiv="Content-style-type"/>
    </head>
  </xsl:template>

  <xsl:template name="make-banner">
    <div id="banner">
      <table width="100%" cellpadding="8" cellspacing="0" border="0">
        <tr>
          <td align="left"/>
          <td align="center">
            <div>
              <a href="http://offo.sourceforge.net/">
                <img class="logoImage" alt="OFFO" src="images/project.png"/>
              </a>
            </div>
          </td>
          <td valign="top" align="right"/>
        </tr>
      </table>
    </div>
  </xsl:template>

  <xsl:template name="make-toptabs">
    <div id="toptabs" class="tabs">
      <table border="0" cellspacing="0" cellpadding="4">
        <tr>
          <th>
            <a class="base-selected" href="index.html">Hyphenation</a>
          </th>
        </tr>
      </table>
    </div>
  </xsl:template>

  <xsl:template name="make-breadcrumbs">
    <table width="100%" border="0" cellpadding="0" cellspacing="0" id="breadcrumbs">
      <tr>
        <td/>
        <td>
          <div align="right"> 
            <script type="text/javascript" language="JavaScript"><!--
              document.write("Published: " + document.lastModified);
              //--></script>
          </div>
        </td>
      </tr>
    </table>
  </xsl:template>

  <xsl:template name="make-main">
    <table id="main" width="100%" cellpadding="4" cellspacing="0" border="0">
      <tr valign="top">
        <xsl:call-template name="make-leftcol"/>
        <xsl:call-template name="make-content"/>
      </tr>
    </table>
  </xsl:template>

  <xsl:template name="make-footer">
    <div id="footer">
      <table cellpadding="4" cellspacing="0" border="0">
        <tr>
          <td>
            <a href="http://sourceforge.net/">
              <img class="logoImage" alt="" src="http://sourceforge.net/sflogo.php?group_id=116740"
              />
            </a>
          </td>
          <td><a href="http://opensource.org/licenses"> Copyright © 2004–2010 OFFO</a> All rights reserved.<br/>
            <script type="text/javascript" language="JavaScript"><!--
              document.write(" - " + "Last Published: " + document.lastModified);
              //--></script>
            <div id="feedback"> Send feedback about the website to <a id="feedbackto"
                href="mailto:spepping@sourceforge.net?subject=Feedback about OFFO%C2%A0offo-hyphenation/languages-info.html"
                >the OFFO project maintainers</a>
            </div>
          </td>
          <td nowrap="nowrap" align="right"/>
        </tr>
      </table>
    </div>
  </xsl:template>

  <xsl:template name="make-leftcol">
    <td width="20%" id="leftcol">
      <div id="navcolumn">
        <div class="toolgroup" id="projecttools">
          <div class="label">
            <strong>Content</strong>
          </div>
          <div class="menu">
            <div class="body">
              <div>
                <a href="index.html">Index</a>
              </div>

              <div>
                <a href="languages-info.xml">Languages info</a>
              </div>

              <div>
                <a href="installation.html">Install</a>
              </div>

              <div class="label">
                <strong>Web links</strong>
              </div>

              <div>
                <a href="http://offo.sourceforge.net/">OFFO home page</a>
              </div>

              <div>
                <a href="http://sourceforge.net/projects/offo">Download</a>
              </div>

              <div>
                <a href="http://xmlgraphics.apache.org/fop/">FOP</a>
              </div>

            </div>
          </div>
        </div>
        <div class="toolgroup" id="admfun">
          <div class="label">
            <strong>Credits</strong>
          </div>
          <div class="body">
            <table>
              <tr>
                <td/>
                <td class="logos" height="5" colspan="4">
                  <a href="http://xmlgraphics.apache.org/fop/team.html#cl">Design: Web Maestro Clay
                    Leeds</a>
                </td>
              </tr>
            </table>
          </div>
        </div>
      </div>
      <div class="strut"> </div>
    </td>
  </xsl:template>

  <xsl:template name="make-content">
    <td>
      <div class="content">
        <div align="right" id="topmodule">
          <table width="100%" cellpadding="3" cellspacing="0" border="0">
            <tr>
              <td class="tasknav"> </td>
              <td class="tasknav" id="issueid">
                <div align="right">
                  <div id="skinconf-txtlink"/>
                </div>
              </td>
            </tr>
          </table>
        </div>
        <div id="bodycol">
          <div id="apphead">
            <h2>
              <em>Information about included languages</em>
            </h2>
          </div>
          <div class="app" id="projecthome">
            <div class="h4">

              <xsl:apply-templates/>

            </div>
          </div>
        </div>
      </div>
    </td>
  </xsl:template>

</xsl:stylesheet>
