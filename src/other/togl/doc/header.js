function displayHeader(pageTitle)
{
	document.write("<center><h1>" + pageTitle + "</h1></center>");
}

function NavigationBar()
{
	document.write("<table rules='cols' cellpadding='4'>");
	document.write(" <tr>");
	document.write(" <td><a href='index.html'>Index</a></td>");
	document.write(" <td><a href='index.html#intro'>Intro</a></td>");
	document.write(" <td><a href='download.html'>Download/Install</a></td>");
	document.write(" <td><a href='using.html'>Using Togl</a></td>");
	document.write(" <td><a href='tclapi.html'>Tcl API</a></td>");
	document.write(" <td><a href='capi.html'>C API</a></td>");
	document.write(" <td><a href='faq.html'>FAQ</a></td>");
	document.write(" </tr>");
	document.write("</table>");
	document.write("<hr>");
}
