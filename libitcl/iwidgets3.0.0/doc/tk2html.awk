#!/bin/nawk


$0 ~ /'[\/\\]" */ || $1 == "'"    { next }    # eat [nt]roff comments

# defining macros - eat them 
/^\.de.*/ {
        getline
        while ( $0 !~ "^\.\.$" )
        {
            getline
        }
        getline
    }

$1 == ".VS" || $1 == ".VE" || $1 == ".AS"  { next }


# handle first .SH as special case - .SH NAME
/^.SH *NAME */ {
        getline
        while ( $0 ~ /\.[a-zA-Z].*/ )   # eat dot-cmd following title 
        {
            getline
        }
        print "<TITLE>" $0 "</TITLE>"
        print "<H1>" $0 "</H1>\n"
        next

#-e 's/^.SH *NAME */{N;s#.*\n\(.*\)#<H1>\1</H1>#;}' \
    }


# Convert .IP Paragraphs upto next .cmd to hanging indents
#       using <UL></UL> pairs without intervening <LI>

/^\.IP */ {
        if ( inIP > 0 )
        {
            print "</UL>"
        }
        inIP = 1 
        print "<UL>"
        match($0, /".*"/ )
        if ( RSTART > 0 )
        {
            arg = substr( $0, RSTART+1, RLENGTH-2)
            
            print arg " <BR>"
        }
        else if ( length( $2 ) > 0 )
        {
            print $2 " <BR>"
        }
        next
    }

$0 ~ /^\.[a-zA-Z]*/ && inIP > 0 {
        inIP = 0
        print "</UL>"
    }

# Convert 
# .TP
# Line1
# line 2 - n
# .Any              
#
# to
# <DL>
# <DT> Line1
# <DD> lines 2 - n 
# <DT>

/^\.TP */ {
        if ( inTP > 0 )
        {
            print "</DL>"
        }
        inTP = 1 
        print "<DL>"
        next
    }

inTP == 1 && $1 !~ /\.[a-zA-Z]*/ {
        print "<DT> " $0
        inTP = 2
        next
    }

inTP == 2 && $1 !~ /\.[a-zA-Z]*/{
        print "</I></B>"    # Belt and suspenders
        print "<DD> " $0
        inTP = 3
        next
    }

$0 ~ /^\.[a-zA-Z]*/ && inTP > 0 {
        inTP = 0
        print "</DL>"
    }



$1 == ".AP" {
        $1=""
        print "<DL >"
        print "<DT> " $2 "\t\t" $3 "\t\t("$4")"
        inTP = 2
        next
    }

# make a blank line 
$1 == ".sp" {
        print "<BR>"
        next #        print "<BR>"
    }


$1 == ".ta"  { next }


# try and make links ( tk ) 
#       "See the .*  manual entry"

/"options"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"options\"", "<A HREF=\"http://www.sco.com/Technology/tcl/man/tk_man/options.n.html\"> \"options\" </A>")
    }

/"entry"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"entry\"", "<A HREF=\"http://www.sco.com/Technology/tcl/man/tk_man/entry.n.html\"> \"entry\" </A>")
    }

/"button"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"button\"", "<A HREF=\"http://www.sco.com/Technology/tcl/man/tk_man/button.n.html\"> \"button\" </A>")
    }

/"scrollbar"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"scrollbar\"", "<A HREF=\"http://www.sco.com/Technology/tcl/man/tk_man/scrollbar.n.html\"> \"scrollbar\" </A>")
    }

/"listbox"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"listbox\"", "<A HREF=\"http://www.sco.com/Technology/tcl/man/tk_man/listbox.n.html\"> \"listbox\" </A>")
    }

/"canvas"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"canvas\"", "<A HREF=\"http://www.sco.com/Technology/tcl/man/tk_man/canvas.n.html\"> \"canvas\" </A>")
    }

/"text"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"text\"", "<A HREF=\"http://www.sco.com/Technology/tcl/man/tk_man/text.n.html\"> \"text\" </A>")
    }

/"license.terms"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"license\".terms", "<A HREF=\"legal.html\"> license.\"terms\" </A>")
    }

/"buttonbox"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"buttonbox\"", "<A HREF=\"buttonbox.n.html\"> \"buttonbox\" </A>")
    }

/"combobox"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"combobox\"", "<A HREF=\"combobox.n.html\"> \"combobox\" </A>")
    }

/"dialog"/  {
    if ( $0 ~ /^See the .*/ )
      sub("\"dialog\"", "<A HREF=\"dialog.n.html\"> \"dialog\" </A>")
    }

/"dialogshell"/  {
    if ( $0 ~ /^See the .*/ )
      sub("\"dialogshell\"", "<A HREF=\"dialogshell.n.html\"> \"dialogshell\" </A>")
    }

/"entryfield"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"entryfield\"", "<A HREF=\"entryfield.n.html\"> \"entryfield\" </A>")
    }

/"fileselectionbox"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"fileselectionbox\"", "<A HREF=\"fileselectionbox.n.html\"> \"fileselectionbox\" </A>")
    }

/"fileselectiondialog"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"fileselectiondialog\"", "<A HREF=\"fileselectiondialog.n.html\"> \"fileselectiondialog\" </A>")
    }

/"labeledwidget"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"labeledwidget\"", "<A HREF=\"labeledwidget.n.html\"> \"labeledwidget\" </A>")
    }

/"messagedialog"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"messagedialog\"", "<A HREF=\"messagedialog.n.html\"> \"messagedialog\" </A>")
    }

/"optionmenu"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"optionmenu\"", "<A HREF=\"optionmenu.n.html\"> \"optionmenu\" </A>")
    }

/"panedwindow"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"panedwindow\"", "<A HREF=\"panedwindow.n.html\"> \"panedwindow\" </A>")
    }

/"promptdialog"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"promptdialog\"", "<A HREF=\"promptdialog.n.html\"> \"promptdialog\" </A>")
    }

/"pushbutton"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"pushbutton\"", "<A HREF=\"pushbutton.n.html\"> \"pushbutton\" </A>")
    }

/"scrolledcanvas"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"scrolledcanvas\"", "<A HREF=\"scrolledcanvas.n.html\"> \"scrolledcanvas\" </A>")
    }

/"scrolledframe"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"scrolledframe\"", "<A HREF=\"scrolledframe.n.html\"> \"scrolledframe\" </A>")
    }

/"scrolledlistbox"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"scrolledlistbox\"", "<A HREF=\"scrolledlistbox.n.html\"> \"scrolledlistbox\" </A>")
    }

/"scrolledtext"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"scrolledtext\"", "<A HREF=\"scrolledtext.n.html\"> \"scrolledtext\" </A>")
    }

/"selectionbox"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"selectionbox\"", "<A HREF=\"selectionbox.n.html\"> \"selectionbox\" </A>")
    }

/"selectiondialog"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"selectiondialog\"", "<A HREF=\"selectiondialog.n.html\"> \"selectiondialog\" </A>")
    }

/"spindate"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"spindate\"", "<A HREF=\"spindate.n.html\"> \"spindate\" </A>")
    }

/"spinint"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"spinint\"", "<A HREF=\"spinint.n.html\"> \"spinint\" </A>")
    }

/"spinner"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"spinner\"", "<A HREF=\"spinner.n.html\"> \"spinner\" </A>")
    }

/"spintime"/  {
    if ( $0 ~ /^See the .*/ )
        sub("\"spintime\"", "<A HREF=\"spintime.n.html\"> \"spintime\" </A>")
    }

/^Name: */ {
        print $1 "                   " $2
        next
    }

/^Class: */ {
        print $1 "                  " $2
        next
    }

/^Bret A. Schuhmacher*/ {
        print "<A HREF=\"mailto:bas@wn.com\">" $0 "</A>"
        next
    }

/^John S. Sigler*/ {
        print "<A HREF=\"mailto:jsigler@spd.dsccc.com\">" $0 "</A>"
        next
    }

/^Mark L. Ulferts*/ {
        print "<A HREF=\"mailto:mulferts@spd.dsccc.com\">" $0 "</A>"
        next
    }

/^Alfredo Jahn*/ {
        print "<A HREF=\"mailto:ajahn@spd.dsccc.com\">" $0 "</A>"
        next
    }

/^Sue Yockey*/ {
        print "<A HREF=\"mailto:syockey@spd.dsccc.com\">" $0 "</A>"
        next
    }

# just pass everything else on

    { print $0 }    


