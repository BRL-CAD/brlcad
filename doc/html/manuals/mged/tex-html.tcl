#!/bin/sh
#
# restart with wish \
exec tclsh "$0" "$@"

set tex_fd [open [lindex $argv 0] r]

set tmp1 [read $tex_fd]
close $tex_fd

set xlate {
{ "(\\\\chapter\{)(\[^\{]*)(\})"	{<H1>\2</H1>} }
{ "(\\\\section\{)(\[^\}]*)(\})"	{<H2>\2</H2>} }
{ "(\\\\subsection\{)(\[^\}]*)(\})"	{<H3>\2</H3>} }
{ "(\\\\cite\{)(\[^\}]*)(\})"		{<a href="#\2">\2</a>} }
{ "(\\\\ref\{)(\[^\}]*)(\})"		{<a href="#\2">\2</a>} }
{ "(\{\\\\bf )(\[^\}]*)(\})"		{<B>\2</B>} }
{ "(\{\\\\em )(\[^\}]*)(\})"		{<I>\2</I>} }
{ "(\\\\item)"				{<LI>} }
{ "(\\\\begin\{figure\})"		{<CENTER><TABLE border=1>} }
{ "(\\\\caption\{)(\[^\}]*)(\\\\label\{)(\[^\}]*)(\})(\[^\}]*)(\})"
					{<A NAME="\4"><CAPTION>\2</CAPTION></A>} }
{ "(\\\\end\{figure\})"			{</TABLE></CENTER>} }
{ "(\\\\begin\{tabular\}\{\[^\}]*\})"	{} }
{ "(\\\\end\{tabular\})"		{} }
{ "(\\\\begin\{itemize\})"		{<UL>} }
{ "(\\\\end\{itemize\})"		{</UL>} }
{ "(\\\\begin\{enumerate\})"		{<OL>} }
{ "(\\\\end\{enumerate\})"		{</OL>} }
{ "(\\\\begin\{verbatim\})"		{<PRE>} }
{ "(\\\\end\{verbatim\})"		{</PRE>} }
{ "(\\\\begin\{center\})"		{<CENTER>} }
{ "(\\\\end\{center\})"			{</CENTER>} }
{ "(\\\\mfig )(\[^,]*)(,)(\[\t -~]*)"	{<CENTER><IMG SRC="\2.gif" ALT="\2"><B>\4</B></CENTER>} }
{ "(\n)([%]+)(\[\t -~]*)"			{\1<!-- \3 -->} }
{ "(\n)(\n)"			{\1<p>\2} }
}

set odd 1
foreach pat $xlate {

	if { $odd } {
		regsub -all [lindex $pat 0] $tmp1 [lindex $pat 1] tmp2
		set odd 0
	} else {
		regsub -all [lindex $pat 0] $tmp2 [lindex $pat 1] tmp1
		set odd 1
	}
}


puts "<HTML><HEAD><TITLE>MGED User's Manual</TITLE></HEAD><BODY>"
puts "<H1> Mged User's Manual</H1>"

if { $odd } {
	puts "$tmp1\n</BODY></HTML>"
} else {
	puts "$tmp2\n</BODY></HTML>"
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
