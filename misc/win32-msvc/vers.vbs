'                         V E R S . V B S
' BRL-CAD
'
' Copyright (C) 2005-2007 United States Government as represented by
' the U.S. Army Research Laboratory.
'
' Redistribution and use in source and binary forms, with or without
' modification, are permitted provided that the following conditions
' are met:
'
' 1. Redistributions of source code must retain the above copyright
' notice, this list of conditions and the following disclaimer.
'
' 2. Redistributions in binary form must reproduce the above
' copyright notice, this list of conditions and the following
' disclaimer in the documentation and/or other materials provided
' with the distribution.
'
' 3. The name of the author may not be used to endorse or promote
' products derived from this software without specific prior written
' permission.
'
' THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
' OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
' WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
' ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
' DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
' DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
' GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
' INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
' WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
' NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
' SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
'
'''
'
' Update the "version" file for creation of a new "vers.c" from it.
' May be run in any subdirectory of the source tree.  Output goes to
' stdout now so you'll likely need to run:
'
'   vers.bat variable_name this is a title > vers.c
'
' Optional args:
'   variable name to put version string in (default="version")
'   title
'
' Author -
'   Daniel Rossberg
'
' Source -
'   IABG mbH
'   Einsteinstr. 20, 85521 Ottobrunn, Germany
'
' @(#)$Header$ (BRL)


Set commandLineArguments = WScript.Arguments
Set fileSystem           = CreateObject("Scripting.FileSystemObject")
Set wshNetwork           = Wscript.CreateObject("Wscript.Network")

versionFileName          = "version"
configurationFileName    = "..\..\configure.ac"

' VARIABLE
If CommandLineArguments.Count > 0 Then
    myVariable = CommandLineArguments.Item(0)
Else
    myVariable = "version"
End If

' RELEASE
If (fileSystem.FileExists(configurationFileName)) Then
    Set configurationFileStream = fileSystem.OpenTextFile(configurationFileName)

    Do Until configurationFileStream.AtEndOfStream
        cfsLine = configurationFileStream.Readline

        if InStr(cfsLine, "MAJOR_VERSION=") > 0 Then
            myRelease = Mid(cfsLine, 15)
        ElseIf InStr(cfsLine, "MINOR_VERSION=") > 0 Then
            myRelease = myRelease + "." + Mid(cfsLine, 15)
        ElseIf InStr(cfsLine, "PATCH_VERSION=") > 0 Then
            myRelease = myRelease + "." + Mid(cfsLine, 15)
            Exit Do
        End If
    Loop

    configurationFileStream.Close
Else
    myRelease = "??.??.??"
End If

' TITLE
If commandLineArguments.Count > 1 Then
    myTitle = commandLineArguments.Item(1)

    For i = 2 to commandLineArguments.Count - 1
        myTitle = myTitle + " " + commandLineArguments.Item(i)
    Next
Else
    myTitle = "Graphics Editor (MGED)"
End If

' DATE
myDate = CStr(Date)

' VERSION
If fileSystem.FileExists(versionFileName) Then
    Set versionFileStream = fileSystem.OpenTextFile(versionFileName)
    myVersion             = CStr(versionFileStream.ReadLine + 1)
    versionFileStream.Close
Else
    myVersion   = "0"
End If

Set versionFileStream = fileSystem.CreateTextFile(versionFileName, True)
versionFileStream.WriteLine(myVersion)
versionFileStream.Close

' USER
myUser = wshNetwork.UserName

' COMPUTER
myComputer = wshNetwork.ComputerName

' DIRECTORY
Set versionFile = fileSystem.GetFile(versionFileName)
myDirectory     = Replace(versionFile.ParentFolder, "\", "/")

WScript.Echo("char " + myVariable + "[] = ""@(#) BRL-CAD Release " + myRelease + "   " + myTitle + "\n""")
WScript.Echo("    """ + myDate + ", Compilation " + myVersion + "\n""")
WScript.Echo("    """ + myUser + "@" + myComputer + ":" + myDirectory + "\n"";")


' Local Variables:
' mode: vb
' tab-width: 8
' vb-indentation: 4
' vb-basic-offset: 4
' indent-tabs-mode: t
' End:
' ex: shiftwidth=4 tabstop=8
