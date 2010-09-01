#!/bin/nawk
# cleans up any leading crap before <TITLE> line in stream from tk2html

/^<TITLE>/ { go = 1 }

/^<table>*/ {
        getline ln
        numf = split (ln, spln)

        if ( ln !~ "Name: *" )
        {
            ind = 0
            inc = 4
            print "<table cellpadding=5>"

            while ( ln !~ "^</table>" )
            {
                for (i = 1; i <= numf; i++)
                {
                    tablns[ind] = spln[i]
                    ind++
                }
                getline ln
                numf = split (ln, spln)
            }

            for (i = 0; i < inc; i++)
                {
                    print "<td valign=top>"
                    for (j = i; j < ind; j += inc)
                        print tablns[j] "<br>"
                    print "</td>"
                }
            
            print "</table>"
        }

        else
        {
            print "<pre>"
            while ( ln !~ "^</table>" )
            {
                print ln
                getline ln
            }
            print "</pre>"
        }
        
        next
    }

go == 1 { print $0 }

