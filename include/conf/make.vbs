'                         M A KE . V B S
' BRL-CAD
'
' Copyright (c) 2005-2007 United States Government as represented by
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
'
' Update the version info files in include/conf/
'
' Optional args:
'   none
'
' Author -
'   Daniel Rossberg
'
' @(#)$Header$ (BRL)


Set fileSystem     = CreateObject("Scripting.FileSystemObject")
Set wshNetwork     = Wscript.CreateObject("Wscript.Network")
Set wshShell       = Wscript.CreateObject("Wscript.Shell")
Set wshEnvironment = wshShell.Environment("Process")

Set myself = fileSystem.GetFile(Wscript.ScriptFullName)
scriptPath = myself.ParentFolder


' COUNT
fileName = scriptPath + "\COUNT"
myValue  = "1"

If fileSystem.FileExists(fileName) Then
    Set fileStream = fileSystem.OpenTextFile(fileName)
    myValue        = CStr(fileStream.ReadLine + 1)
    fileStream.Close
End If

Set fileStream = fileSystem.CreateTextFile(fileName, True)
fileStream.WriteLine(myValue)
fileStream.Close


'DATE
fileName = scriptPath + "\DATE"
myValue  = Now

Set fileStream = fileSystem.CreateTextFile(fileName, True)
fileStream.WriteLine(myValue)
fileStream.Close


' HOST
fileName = scriptPath + "\HOST"
myValue  = wshNetwork.ComputerName

Set fileStream = fileSystem.CreateTextFile(fileName, True)
fileStream.WriteLine(myValue)
fileStream.Close


' PATH
fileName = scriptPath + "\PATH"
myValue  = Replace(wshEnvironment("ProgramFiles") + "\BRL-CAD", "\", "/")

Set fileStream = fileSystem.CreateTextFile(fileName, True)
fileStream.WriteLine(myValue)
fileStream.Close


' USER
fileName = scriptPath + "\USER"
myValue  = wshNetwork.UserName

Set fileStream = fileSystem.CreateTextFile(fileName, True)
fileStream.WriteLine(myValue)
fileStream.Close


' Local Variables:
' mode: vb
' tab-width: 8
' vb-indentation: 4
' vb-basic-offset: 4
' indent-tabs-mode: t
' End:
' ex: shiftwidth=4 tabstop=8
