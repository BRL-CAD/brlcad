#!/usr/bin/tclsh

proc header {} {
  puts {
    <html>
    <head>
    </head>
  }
}

set REQNOS [list]
proc req {reqno text {com {}}} {
  variable r
  if {[info exists r($reqno)]} error
  if {[lsearch $::REQNOS $reqno]!=-1} error
  lappend ::REQNOS $reqno
  set r($reqno) [list [string trim $text] [string trim $com]]
}

proc finish {} {
  variable r
  appendbody {<table border="0" cellpadding="10">}
  foreach reqno [lsort -integer [array names r]] {
    appendbody "<tr><td valign=\"top\">"
    appendbody [format REQ%05d $reqno]
    appendbody "<td valign=\"top\">"
    appendbody "<p>[lindex $r($reqno) 0]</p>"
    set comment [lindex $r($reqno) 1]
    if {$comment!=""} {
      appendbody "<blockquote><i>$comment</i></blockquote>"
    }
    appendbody "</td></tr>"
  }
  appendbody "</table>"
  unset r
}

set BODY {}
proc appendbody {line} {
  append ::BODY $line
  append ::BODY \n
}

proc TODO {} {
  appendbody {
    <p><b><i>TODO</p></i></b>
  }
}

set SECTION_NUMBER [list 0 0 0 0 0]
proc section {level title} {
  variable r
  if {[llength [array names r]]>0} finish

  lset ::SECTION_NUMBER $level [expr [lindex $::SECTION_NUMBER $level]+1]
  for {set i [expr $level+1]} {$i<[llength $::SECTION_NUMBER]} {incr i} {
    if {[lindex $::SECTION_NUMBER $i]>0} {
      puts "</ul>"
    }
    lset ::SECTION_NUMBER $i 0
  }
  if {[lindex $::SECTION_NUMBER $level]==1} {
    puts "<ul>"
  }

  set sn [join [lrange $::SECTION_NUMBER 0 $level] .]

  set lt "h[expr $level+2]"
  appendbody "<a name=\"s$sn\"><$lt>$sn. $title</$lt>"
  puts "$sn <a href=\"#s$sn\">$title</a><br>"
}

header
puts {
  <body bgcolor="white">
  <h1 align="center">Tk Html Widget Revitalization Project Requirements<br>DRAFT</h1><div id=contents>
}

section 0 {Overview} 
appendbody {
  <p>This document contains requirements for the revitalization of the 
    Tk html software. The goal of this project is to produce a second 
    generation of the Tk html widget that provides a superior alternative 
    to existing html rendering frameworks such as Gecko [1] or KHTML [2]. 
    The widget should be smaller, faster and more easily extensible than
    other frameworks.
  </p>
  <p>The deliverables for the project are:<br>
    <ul>
      <li>The Tk 'html' widget, a Tk widget written in C that renders 
          HTML code.</li>
      <li>Documentation describing the Tcl interface to the html widget.
      <li>A demo web browser application that uses the html widget, 
          written in TCL.</li>
    </ul>
  </p>
  <p>
  It is envisaged that the revitalized html widget be eventually integrated 
  into the Tcl core.
  </p>
}

req 10 {
  All code, user documentation and test artifacts produced by the project
  shall be governed by a license compatible with that of the TCL core.
}

req 20 {
  Signed copyright releases from all contributors shall be held by Hwaci.
}

section 0 {Code Compliance}
 
req 1010 {
  All C-code in the package shall compile using gcc version 3.3 Linux-ELF, 
  mingw or OS-X targets. The package shall run on all Linux, Windows or 
  OS-X runtime platforms supported by Tk.
} {
  It will also run just as well on any Tk platform with any compiler capable
  of building Tcl and Tk, but only the above are required.
}

req 1020 {
  The package shall be compatible with Tcl/Tk 8.4 and later.
} 

req 1030 {
  The source tree organization and build system used by the package shall be 
  TEA (Tcl Extension Architecture) compliant.
}

req 1040 {
  The TCL and C code in the package shall comply with the specifications in 
  the Tcl Style Guide [3] and Tcl/Tk Engineering Manual [4], respectively.
}

req 1050 {
  The package shall use modern Tcl interfaces, such as Tcl_Obj.
}

section 0 {Functionality}

appendbody {
  <p>This section defines the functionality required from the html widget.
  For the purposes of formulating requirements, the functionality of the
  widget is divided into three categories, as follows:
    <ul>
      <li> The parsing of documents, and supported queries and operations on
           the parsed document structure (see section 3.1). </li>
      <li> The rendering of documents. (section 3.2). </li>
      <li> Other functions of the widget that contains the rendered
           document, such as panning, selection manipulation and event
           handling (see section 3.3). </li>
    </ul>
  </p>
}

section 1 {Document Processing}

section 2 {Parsing}

req 2010 {
  The html widget shall parse web documents. The widget shall support 
  documents conforming to the standards:
    <ul>
      <li>HTML 4.01,
      <li>XHTML 1.1,
      <li>CSS 2.1.
    </ul>
} 

req 2020 {
  The widget shall be tolerant of errors in HTML documents in a 
  similar way to existing html engines.
} {
  In this context, "existing html rendering engines" refers to Gecko
  [1] and Internet Explorer [5]. Where Gecko and Internet
  Explorer produce substantially different results, the html widget
  will emulate the more elegant.
}

req 2030 {
  Incremental parsing of documents shall be supported. It shall be
  possible to render and query partially parsed documents.
} {
  This means a document can be passed to the widget in parts, perhaps while
  waiting for the remainder of it to be retrieved over a network.
}

section 2 {Document API}

appendbody {
  <p> This section contains requirements for an API to query and manipulate
  a parsed document structure. Although there are likely to be other 
  users, it is anticipated that this API will be primarily used to implement 
  DOM compatible interfaces (i.e. for javascript).
  </p>
}

req 2040 {
  The html widget shall expose an API for querying and modifying the
  parsed document as a tree structure. Each node of the tree structure
  shall be either an XML tag, or a string. The attributes of each tag 
  shall be available as part of the tag node.
} {
  For example, the HTML fragment:
  <pre>
    &lt;p class="normal"&gt;The quick &lt;b&gt;brown&lt;/b&gt; 
    fox &lt;i&gt;jumped &lt;b&gt;over&lt;/b&gt; the&lt;/i&gt; ...&lt;/p&gt;
  </pre>

  will be exposed as the tree structure:</p>
  <img src="tree.gif" alt="Tree structure"></img>
}

req 2050 {
  It shall be possible to obtain a reference to the root node of a document
  tree.
}

req 2060 {
  The html widget shall support querying for a list of document nodes by
  any combination of the following criteria:
  <ul>
    <li>Tag type,
    <li>Whether or not a certain attribute is defined,
    <li>Whether or not a certain attribute defined and set to a certain value.
  </ul>
}

req 2070 {
  Given a reference to a document tree node, it shall be possible to obtain
  a reference to the parent node or to any child nodes (if present).
}

req 2080 {
  Given a node reference, it shall be possible to query for the node tag 
  and attributes (if the node is an XML tag), or text (if the node is a 
  string).
}

req 2090 {
  It shall be possible to create and insert new nodes into any point in 
  the document.
}

req 2100 {
  The html widget shall support modification of the tag, attributes or 
  string value of existing nodes.
}

req 2110 {
  It shall be possible to delete nodes from the document.
}

req 2120 {
  If a document currently rendered to a window is modified, then the 
  display shall automatically update next time the process enters the
  Tk event loop.
} 

req 2130 {
  During parsing, the widget shall support invoking a user supplied scripts
  when nodes matching specified criteria are added to the document. Criteria
  are as defined for requirement 2060. The script shall be passed a 
  reference that may be used to query, modify or delete the new node.
}

section 2 {Stylesheets}

appendbody {
  <p>This section contains requirements for an API to query and manipulate
  the various style sheets contained within a document. Also requirements
  describing the way the widget may co-operate with the application to 
  support linked style sheets.
  </p>
}

req 2140 {
  The html widget shall support CSS and CSS2 stylesheets only.
} {
  If support for a future stylesheet format is required, this may be
  implemented by transforming the new stylesheet format to one of the
  supported types.
}

req 2150 {
  It shall be possible to supply the html widget with a script that is 
  invoked whenever an unrecognized stylesheet format is encountered. The
  script shall have the power to modify the content and content type of
  the stylesheet.
} {
  For example to support a future standard CSS3, the application may 
  supply a script to transform CSS3 specifications into CSS2 format.
}

req 2160 {
  It shall be possible to retrieve an ordered list of stylesheets 
  specified in the &lt;head&gt; section of a document.
}

req 2170 {
  The html widget shall supply an interface to add and remove stylesheets 
  to and from a document.
} {
  This is the same as adding and removing nodes from a document 
  &lt;head&gt; section.
}

req 2180 {
  It shall be possible to supply the html widget with a script to be
  invoked whenever a linked stylesheet is required. The script shall
  have the option of returning the required stylesheet synchronously, 
  asynchronously or not at all.
} {
  In this context, "linked stylesheets" refers to an external stylesheet
  refered to by a &lt;link&gt; tag or an @import directive within another
  stylesheet.
}

section 2 {Printing}

req 2190 {
  The html widget shall support rendering to postscript.
}

section 1 {Document Rendering}

req 3010 {
  The html widget shall render parsed web documents in a Tk window, 
  producing results consistent with existing html rendering engines. 
  The widget shall support documents conforming to:
    <ul>
      <li>HTML 4.01,
      <li>XHTML 1.1,
      <li>CSS 2.1.
    </ul>
} {
  In this context, "existing html rendering engines" refers to Gecko
  [1] and Internet Explorer [5]. Where Gecko and Internet
  Explorer produce substantially different results, the html widget
  will emulate the more elegant.
}

req 3020 {
  The html widget shall provide an interface to re-render the current
  document. When a document is re-rendered, existing images, fonts,
  applets and form elements shall be destroyed and re-requested from the
  various callbacks.
}

section 2 {Hyperlinks}

req 3030 {
  An application shall be able to register a callback script that is
  invoked when a user clicks on a hyperlink in an html document. All
  attributes of the hyperlink markup shall be available to the callback 
  script.
} {
  The 'href' field value available to the callback script may be an internal, 
  relative or absolute URI. Dealing with this is in the application domain.
}

req 3040 {
  It shall be possible to configure the colors that visited and unvisited
  hyperlinks are rendered in.
} 

req 3050 {
  It shall be possible to configure whether or not hyperlinks are rendered
  underlined.
} 

req 3060 {
  An application shall have the option of supplying a script to be
  executed by the html widget to determine if a hyperlink should be
  rendered in the visited or unvisited color. The script shall
  have access to the href field of the hyperlink.
} {
  Presumably, an implementation would query a database of previously 
  visited URLs to determine if the link should be colored as visited 
  or unvisited.
}

section 2 {Tables and Lines}

req 3070 {
  It shall be possible to configure the html widget to render horizontal 
  lines and table borders with solid lines or 3-D grooves or ridges.
}

section 2 {Fonts}

req 3080 {
  It shall be possible to provide a callback script to the html widget
  to resolve fonts. If such a script is provided, it shall be invoked
  whenever a new font is required, specifying the font-family and size as
  an integer between 1 and 7. The script shall translate this into a 
  Tk font name. If no such script is provided, a default implementation 
  shall be used.
}

section 2 {Images, Forms and Applets}

appendbody {
  <p>This section contains requirements describing the way the html widget
  will divide the job of handling some complex html tags between itself
  and the application. Specifically, requirements for the "img", "applet", 
  and "form" tags are specified here.
  </p>
  <p>In general, the html widget itself is responsible only for rendering 
  text and laying out document elements. To render more complex document
  tags, such as images or form controls, the widget invokes callback
  scripts supplied by the application. The callback scripts create
  Tk primitives (i.e. a window or image) based on the attributes of the 
  parsed tags. These primitives are passed back to the html widget
  to be included in the document layout.
  </p>
  <p>This approach allows the application to select the various toolkits
  used to implement complex functionality such as images or form controls.
  </p>
}

req 3090 {
  It shall be possible to handle the &lt;img&gt; tag by supplying
  the html widget with a handler function that interprets an img tag's 
  attributes and creates a Tk primitive to display. The html widget shall be
  responsible only for mapping the primitive into the rendered document.
} {
  In the above requirement, "function" can be taken to mean "Tcl script".
  The purpose of this is to allow the application to select the image
  toolkit to use.
}

req 3100 {
  If the &lt;img&gt; tag handler fails to create a Tk primitive to 
  display, the html widget shall display the "alt" text, if any, from 
  the img tag.
}

req 3110 {
  A simple default handler for the &lt;img&gt; tag shall be used if
  the application does not provide one.
}

req 3120 {
  It shall be possible to handle the &lt;applet&gt; tag by supplying
  the html widget with a script that interprets an applet tag's attributes 
  and creates a Tk window containing the applet. The html widget shall 
  be responsible only for mapping the applet window into the rendered 
  document.
} 

req 3130 {
  It shall be possible to handle the &lt;form&gt; tag by supplying
  the html widget with a script that interprets the attributes of 
  form, control and label elements of a form definition and creates 
  a Tk primitive for each. The html widget shall be responsible only 
  for mapping the Tk primitives into the rendered document.
} {
  Implementation of all logic to submit forms is implemented by the
  application.
}

section 2 {Frames}

req 3140 {
  It shall be possible to handle framed documents by supplying the
  html widget with a handler script that interprets the attributes of 
  a frameset structure. The html widget shall provide no support for 
  framed documents other than parsing the document and invoking the handler.
} {
  The anticipated approach is to have the handler script render each 
  frame of the document in a separate instance of the html widget.
}

req 3150 {
  If a framed document is parsed and no frame handler function has been
  supplied, the html widget shall render the document contained in the
  &lt;noframes&gt; element (if any) of the document.
} 

section 1 {Other Widget Functionality}

req 4010 {
  The html widget shall respect Tk widget conventions, including:
  <ul>
  <li> The following standard Tk options: -background, -cursor, 
       -exportselection, -foreground, -height, -highlightbackground,
       -highlightcolor, -highlightthickness,
       -takefocus, -width, -xscrollcommand, -yscrollcommand.
  <li> The configure/cget subcommands for setting and retrieving option values.
  <li> The options database used to configure widget options.
  </ul>
}

section 2 {Widget Panning}

req 4020 {
  It shall be possible to pan the widget (change the portion of the document
  visible). It shall be possible to specify the new region to display by:
  <ul>
    <li>Specifying a reference to a node within the rendered document 
        identifying the top of the required region, or
    <li>Specifying a number of pages or units to scroll up, down or across 
        from the current position, or
    <li>Specifying a fraction of the document to be left off-screen (either
        on the left or above the displayed region).
  </ul>
} { 
  If the rendered html document is too large to fit in the widget window,
  only part of it will be displayed. By default, the widget will provide no
  bindings that can be used to scroll to a different part of the document.
  Such bindings should be implemented by the application.
}

req 4030 {
  The html widget shall support smooth-scrolling.
}

req 4040 {
  It shall be possible to receive notification that the visible region
  of a html document has changed.
} { 
  This is required so that applications may keep any associated scroll-bar 
  widgets up to date when an html widget is panned or resized. This might
  use the standard Tk -xscrollcommand, -yscrollcommand interfaces.
}

section 2 {The Selection}

req 4050 {
  The html widget shall support querying for the identifier of the leaf
  node that is rendered at nominated screen coordinates. If the leaf node
  is a text string, then the index of the character in the string shall 
  also be available.
}

req 4060 {
  The html widget shall provide an interface to set the current selection.
} {
  The widget is not required to provide bindings to allow a region to be 
  selected with the pointer. Such bindings may be created by an application.
}

req 4070 {
  The html widget shall provide an interface to retrieve the current 
  selection, if any.
}

req 4080 {
  The background color used when rendering the selection shall be 
  configurable.
}

section 2 {Events}

appendbody {
  <p>This section describes requirements for binding scripts to Tk events
  received by the html widget.
  </p>
}

req 4090 {
  It shall be possible to bind Tcl scripts to Tk events received by the 
  html widget.
} {
  The html widget shall be no different to built-in widgets in this respect.
}

req 4100 {
  It shall be possible to bind Tcl scripts to Tk events received when the
  pointer is over the rendering of a specified document node. 
}

req 4110 {
  It shall be possible to bind a Tcl script to a Tk event received when
  the pointer is over a document node with a specified attribute defined.
} {
  For example a script may be bound to all nodes with attribute 
  "onmouseover" defined.
}

section 0 {Performance}

req 5010 {
  The html widget shall parse and render html code as fast or faster than 
  existing html rendering engines.
} {
  In this context, "existing html rendering engines" refers to Gecko
  [1] and KHTML [2].
}

req 5020 {
  The html widget shall initialize as fast or faster than existing html
  rendering engines.
} {
  In this context, "existing html rendering engines" refers to Gecko
  [1] and KHTML [2].
}

section 0 {Testing}

appendbody {
  <p>This section defines requirements for testing of the html widget.
  These requirements will eventually be refined into a test plan.
  </p>
}

req 6010 {
  A demo TCL web browser application that uses the html widget shall be
  packaged along with the widget. The application shall support images,
  but not applets or forms.
} {
  This application will be advanced enough to use for informal testing
  by viewing web pages.
}

req 6020 {
  The demo application shall include bindings to manipulate the selection
  with the pointer.
} {
  This can be used to informally verify both the selection interface and
  the capability to translate between screen coordinates and token 
  identifiers.
}

req 6030 {
  The demo application shall include scrollbars and key bindings to 
  informally test the panning interface.
} 

req 6040 {
  Automated tests shall be developed to verify the handling of
  frameset documents.
}

req 6050 {
  Automated tests shall be developed to test the document query and edit 
  interfaces, with the exception of those interfaces that use screen 
  coordinates. 
} {
  This requirement applies to functions required by requirements in 
  section 3.4.
}

req 6060 {
  The demo web browser application shall contain features that may be used
  to informally test the document query interfaces that use screen 
  coordinates.
} {
  One of these feature will be the selection bindings (REQ4020).
}

req 6070 {
  An automated test shall be developed to compare the initialization and
  rendering speed of the html widget against the required html rendering 
  engines.
} {
  In this context, "required html rendering engines" refers to Gecko
  [1] and KHTML [2].
}

section 0 {Documentation}

req 7010 {
  All options and commands that make up the Tcl interface to the html
  widget shall be documented. The build system will generate the same 
  documentation as both a man page and an HTML document.
} 

req 7020 {
  The Tcl demo browser application shall be written and commented 
  so that it is suitable for reading as an example.
} 

section 0 {References}

appendbody {
  <table border="0" cellpadding="10">
  <tr>
     <td valign="top">[1]
     <td valign="top">"Mozilla Layout Engine", 
<a href="http://www.mozilla.org/newlayout/">
http://www.mozilla.org/newlayout/</a>
  </tr>
  <tr>
     <td valign="top">[2]
     <td valign="top">"KHTML - KDE's HTML library", 
<a href="http://developer.kde.org/documentation/library/kdeqt/kde3arch/khtml/">
http://developer.kde.org/documentation/library/kdeqt/kde3arch/khtml/</a><br>
  </tr>
  </tr>
  <tr>
     <td valign="top">[3]
     <td valign="top">"Tcl Style Guide", Ray Johnson,
<a href="http://www.tcl.tk/doc/styleGuide.pdf">
http://www.tcl.tk/doc/styleGuide.pdf</a>
  </tr>
  <tr>
     <td valign="top">[4]
     <td valign="top">"Tcl/Tk Engineering Manual", John K. Ousterhout,
<a href="http://www.tcl.tk/doc/engManual.pdf">
http://www.tcl.tk/doc/engManual.pdf</a>
  </tr>
  <tr>
     <td valign="top">[5]
     <td valign="top">"Microsoft Internet Explorer",
<a href="http://www.microsoft.com/windows/ie/default.mspx">
http://www.microsoft.com/windows/ie/default.mspx</a>
  </tr>
  <tr>
     <td valign="top">[6]
     <td valign="top">"Cascading Style Sheets test suites",
<a href="http://www.w3.org/Style/CSS/Test/">
http://www.w3.org/Style/CSS/Test/</a>
  </tr>
  <tr>
     <td valign="top">[7]
     <td valign="top">"HTML4 Test Suite",
<a href="http://www.w3.org/MarkUp/Test/HTML401/current/">
http://www.w3.org/MarkUp/Test/HTML401/current/</a>
  </tr>
  </table>
}

appendbody {</body></html>}
for {set i -1} {$i<[lindex [lsearch -all -not $::SECTION_NUMBER 0] end]} {incr i} {
  puts "</ul>"
}
puts </div>
puts "<div id=body>"
puts $::BODY
puts </div>


