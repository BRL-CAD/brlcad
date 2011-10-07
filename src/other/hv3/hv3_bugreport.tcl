
namespace eval ::hv3::bugreport {

  proc init {hv3 uri} {
    $hv3 reset 0
    $hv3 html parse {

<HTML>
  <HEAD>
   <STYLE>
     body {
       max-width: 750;
       margin: auto;
       background: #DDDDDD;
     }
     .block {
       border: 2px solid purple;
       background: white;
       padding: 5px;
       margin: 10px 5px;
     }
     h1      { text-align: center; }
     a[href]       { color: purple ; font-style: italic ; font-weight: bold }
     a[href]:hover { background: purple ; color: white }

   </STYLE>
  </HEAD> <BODY>
    <DIV class=block>
      <H1>Report a bug in Hv3</H1>
      <CENTER><A href=#guidelines>View bug report guidelines</A></CENTER>
    </DIV>

    <DIV class=block>
      <H2>Quick Bug Report</H2>
      <DIV id="formcontainer">
        <FORM 
          name=quickreport 
          action="http://tkhtml.tcl.tk/cvstrac/tktnew" 
          method=POST
        >
          <TABLE>
            <TR><TD>Caption:  <TD><INPUT name=t size=60>
            <TR><TD>Uri:      <TD><INPUT id=uritext size=60>
            <TR><TD>E-mail <B>(optional)</B>: <TD><INPUT size=60 name=c>
            <TR><TD>Description:<TD><TEXTAREA name=d cols=70 rows=8></TEXTAREA>
          </TABLE>
          <CENTER>
            <SPAN id=submit_button></SPAN>
            <SPAN id=preview_button></SPAN>
            <INPUT type=hidden id="trick_control">
          </CENTER>
            <INPUT type=hidden name=f value="">
            <INPUT type=hidden name=s value="">
            <INPUT type=hidden name=w value="danielk1977">
            <INPUT type=hidden name=r value="1">
            <INPUT type=hidden name=p value="1">
            <INPUT type=hidden name=y value="Hv3">
            <INPUT type=hidden id=version name=v value="">
        </FORM>
      </DIV>
    </DIV>

    <DIV class=block>
      <A name=guidelines></A>
      <H2>Bug Report Guidelines</H2>
      <P>
        Please report the bugs you observe in Hv3. If a web-page is rendered
        incorrectly or some javascript features doesn't function correctly,
        that's a bug. If an irritating error dialog pops up, that's a bug.
        If Hv3 crashes, that's definitely a bug.
      </P>
      <P>
        At this stage in the project, obtaining bug reports is very important.
	Don't worry about submitting duplicates or accidentally reporting a
	non-bug. Such reports take mere seconds to close and cause nobody any
	inconveniance. Be as brief as you like - typing something like "the
	page is busted because the menu is rendered wrong" is often all that 
        is required.
      </P>
      <P>
	If you include a contact e-mail address, nobody but the Hv3 developers
	will see it. And we'll (probably) send you an e-mail when the bug is
        fixed. Or if the bug is complex we might mail you for clarification.
	Whether or not you include an e-mail address, your help in bringing Hv3
        bugs to our attention will be greatly appreciated.
      </P>
      <P>
        <CENTER><B><I>If in doubt, report it!</I></B></CENTER>
      </P>
    </DIV>
    <DIV class=block>
      <H2>Details</H2>
      <P>
        The Hv3/Tkhtml3 project uses cvstrac to manage source code,
        bug-reports and user contributed wiki pages. Access the cvstrac
        system here:
      </P>
      <P>
        <A href="http://tkhtml.tcl.tk/cvstrac/">
          http://tkhtml.tcl.tk/cvstrac/
        </A>
      </P>
  
      <P>
        There are two ways to submit a bug into the system. The first is to
        add the bug directly into cvstrac via the web-interface by visiting 
        the following link:
      </P>
      <P>
        <A href="http://tkhtml.tcl.tk/cvstrac/tktnew">
          http://tkhtml.tcl.tk/cvstrac/tktnew
        </A>
      </P>
      <P>
        The second method is to submit the "Quick Bug Report" form above.
      <P>
    </DIV>
  </BODY>
</HTML>

    }

    set uri_field [[$hv3 html search #uritext] replace]
    $uri_field dom_value $uri
    set caption_field [[$hv3 html search {[name="t"]}] replace]
    $caption_field dom_focus

    set node [$hv3 html search #submit_button]
    set button [::hv3::button [$hv3 html].document.submit \
        -command [list ::hv3::bugreport::submitform submit $hv3]     \
        -text "Submit Report!"
    ]
    $node replace $button -deletecmd [list destroy $button]

    set node [$hv3 html search #preview_button]
    set button [::hv3::button [$hv3 html].document.preview \
        -command [list ::hv3::bugreport::submitform preview $hv3]     \
        -text "Online Preview..."
    ]
    $node replace $button -deletecmd [list destroy $button]
  }

  proc submitform {status hv3} {
    set caption_field [[$hv3 html search {[name="t"]}] replace]
    set contact_field [[$hv3 html search {[name="c"]}] replace]

    set v [$caption_field value]
    if {[string length $v]>60 || [string length $v]==0} {
      tk_dialog .error "Caption too long" \
          {The Caption field should be between 1 and 60 characters in length} \
          {} 0 OK
      return
    }

    set c [$contact_field value]
    if {$c eq ""} {
      $contact_field dom_value "Anonymous Hv3 User"
    }

    set uri [[[$hv3 html search #uritext] replace] value]
    if {$uri ne ""} {
      set widget [[[$hv3 html search {[name="d"]}] replace] get_text_widget]
      $widget insert 0.0 "   URI: $uri\n\n"
    }

    set trick [$hv3 html search #trick_control]
    $trick attribute name $status
    $trick attribute value 1

    set formnode [::hv3::control_to_form $trick]
    [$formnode replace] submit ""
  }
}

