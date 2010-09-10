; brlcad.nsi

RequestExecutionLevel user

;--------------------------------
;BRL-CAD Version Variables

!include VERSION.txt

;--------------------------------
;Include Modern UI

!include "MUI.nsh"
!include "FileFunc.nsh"
!include "WordFunc.nsh"

!insertmacro GetFileName
!insertmacro WordFind3X

Function .onInit
  ; For the moment this must be global (nsis requires it)
  Var /GLOBAL PROG_FILES

  ReadEnvStr $PROG_FILES "PROGRAMFILES"
  StrCpy $INSTDIR "$PROG_FILES\BRL-CAD\${VERSION}"
FunctionEnd

; Tack on BRL-CAD if it's not already there
Function .onVerifyInstDir
  ${GetFileName} $INSTDIR $R0
  StrCmp "BRL-CAD" $R0 found notFound

  notFound:
    ${WordFind3X} $INSTDIR "\" "BRL-CAD" " "  "+1" $R0
    StrCmp "BRL-CAD" $R0 found stillNotFound

  stillNotFound:
    StrCpy $INSTDIR "$INSTDIR\BRL-CAD"

  found:

FunctionEnd

;--------------------------------
;Configuration

  ; The name of the installer
  Name "BRL-CAD"

  ; The file to write
  OutFile "BRL-CAD_${VERSION}${INSTALLERSUFFIX}.exe"

  ; The default installation directory
  InstallDir $PROGRAMFILES\BRL-CAD\${VERSION}

  ; Registry key to check for directory (so if you install again, it will 
  ; overwrite the old one automatically)
;  InstallDirRegKey HKLM "Software\BRL-CAD ${VERSION}" "Install_Dir"

  ; Make it look pretty in XP
  XPStyle on

;--------------------------------
;Variables

  Var MUI_TEMP
  Var STARTMENU_FOLDER
  Var BRLCAD_DATA_DIR

;--------------------------------
;Interface Settings

  ;Icons
  !define MUI_ICON "brlcad.ico"
  !define MUI_UNICON "uninstall.ico"

  ;Bitmaps
  !define MUI_WELCOMEFINISHPAGE_BITMAP "side.bmp"
  !define MUI_UNWELCOMEFINISHPAGE_BITMAP "side.bmp"

  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_BITMAP "header.bmp"
  !define MUI_COMPONENTSPAGE_CHECKBITMAP "${NSISDIR}\Contrib\Graphics\Checks\simple-round2.bmp"

  !define MUI_COMPONENTSPAGE_SMALLDESC

  ;Show a warning before aborting install
  !define MUI_ABORTWARNING

;--------------------------------

; Pages

  ;Welcome page configuration
  !define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of BRL-CAD ${VERSION}.\r\n\r\nBRL-CAD is a powerful cross-platform open source solid modeling system.\r\n\r\nSelect Next to continue."

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "..\..\COPYING"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY

  ;Start Menu Folder Page Configuration
;  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM" 
;  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\BRL-CAD\${VERSION}" 
;  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"

  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER


  !insertmacro MUI_PAGE_INSTFILES

  ;Finished page configuration
  !define MUI_FINISHPAGE_NOAUTOCLOSE
  !define MUI_FINISHPAGE_SHOWREADME "..\..\README"
  !define MUI_FINISHPAGE_SHOWREADME_TEXT "View Readme"
  !define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

  !define MUI_FINISHPAGE_LINK "BRL-CAD Website"
  !define MUI_FINISHPAGE_LINK_LOCATION "http://brlcad.org/"

  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

  !define MUI_UNFINISHPAGE_NOAUTOCLOSE
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

; The stuff to install
Section "BRL-CAD (required)" BRL-CAD

  SectionIn RO

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  File /r "..\..\brlcadInstall${PLATFORM}\archer.ico"
  File /r "..\..\brlcadInstall${PLATFORM}\brlcad.ico"

  SetOutPath $INSTDIR\bin
  File /r "..\..\brlcadInstall${PLATFORM}\bin\*"

  SetOutPath $INSTDIR\include
  File /r "..\..\brlcadInstall${PLATFORM}\include\*"

  SetOutPath $INSTDIR\lib
  File /r "..\..\brlcadInstall${PLATFORM}\lib\*"

  SetOutPath $INSTDIR\share
  File /r "..\..\brlcadInstall${PLATFORM}\share\*"

  ; Write the installation path into the registry
;  WriteRegStr HKLM "SOFTWARE\BRL-CAD ${VERSION}" "Install_Dir" "$INSTDIR"

  ; Write the uninstall keys for Windows
;  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD ${VERSION}" "DisplayName" "BRL-CAD ${VERSION}"
;  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD ${VERSION}" "DisplayVersion" "${VERSION}"
;  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD ${VERSION}" "VersionMajor" "${MAJOR}"
;  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD ${VERSION}" "VersionMinor" "${MINOR}"
;  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD ${VERSION}" "InstallLocation" '"$INSTDIR"'
;  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD ${VERSION}" "URLInfoAbout" "http://brlcad.org"
;  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD ${VERSION}" "URLUpdateInfo" "http://brlcad.org"
;  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD ${VERSION}" "HelpLink" "http://irc.brlcad.org"
;  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD ${VERSION}" "UninstallString" '"$INSTDIR\uninstall.exe"'

  ;Create uninstaller
  WriteUninstaller "uninstall.exe"


  StrCpy $BRLCAD_DATA_DIR "$INSTDIR\share\brlcad\${VERSION}"

  ; Create desktop icons
  SetOutPath $INSTDIR
  CreateShortCut "$DESKTOP\Archer${INSTALLERSUFFIX}.lnk" "$INSTDIR\bin\archer.bat" "" "$INSTDIR\archer.ico" 0
  CreateShortCut "$DESKTOP\MGED${INSTALLERSUFFIX}.lnk" "$INSTDIR\bin\mged.bat" "" "$INSTDIR\brlcad.ico" 0
  CreateShortCut "$DESKTOP\RtWizard${INSTALLERSUFFIX}.lnk" "$INSTDIR\bin\rtwizard.bat" "" "$INSTDIR\brlcad.ico" 0

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    ;Main start menu shortcuts
    SetOutPath $INSTDIR
    CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER${INSTALLERSUFFIX}"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER${INSTALLERSUFFIX}\Archer.lnk" "$INSTDIR\bin\archer.bat" "" "$INSTDIR\archer.ico" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER${INSTALLERSUFFIX}\MGED.lnk" "$INSTDIR\bin\mged.bat" "" "$INSTDIR\brlcad.ico" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER${INSTALLERSUFFIX}\RtWizard.lnk" "$INSTDIR\bin\rtwizard.bat" "" "$INSTDIR\brlcad.ico" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER${INSTALLERSUFFIX}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  !insertmacro MUI_STARTMENU_WRITE_END

SectionEnd

Section "Documentation (required)" Documentation
  ; SectionIn RO means temporarily required
  ;SectionIn RO
  ;SetOutPath $INSTDIR\doc
  ;File /r ..\..\brlcadInstall${PLATFORM}\doc\*
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    ;Main start menu shortcuts
    SetOutPath $INSTDIR
    CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER${INSTALLERSUFFIX}\Manuals"
    ;CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER${INSTALLERSUFFIX}\Manuals\Archer.lnk" "$BRLCAD_DATA_DIR\html\manuals\archer\Archer_Documentation.chm" "" "$INSTDIR\archer.ico" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER${INSTALLERSUFFIX}\Manuals\BRL-CAD.lnk" "$BRLCAD_DATA_DIR\html\manuals\index.html" "" "$INSTDIR\brlcad.ico" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER${INSTALLERSUFFIX}\Manuals\MGED.lnk" "$BRLCAD_DATA_DIR\html\manuals\mged\index.html" "" "$INSTDIR\brlcad.ico" 0
  !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

;Section "Samples" Samples
;  SetOutPath $INSTDIR\Samples
;  File ..\..\brlcadInstall${PLATFORM}\Samples\*
;SectionEnd

;Section "Developement headers" Developer
;  SetOutPath $INSTDIR\include
;  File ..\..\brlcadInstall${PLATFORM}\include\*
;SectionEnd

;--------------------------------
;Descriptions


  ;Language strings
  LangString DESC_BRL-CAD ${LANG_ENGLISH} "Installs the main application and the associated data files."
  LangString DESC_Documentation ${LANG_ENGLISH} "Installs documentation for BRL-CAD."
  ;LangString DESC_Samples ${LANG_ENGLISH} "Installs optional geometry samples."
  ;LangString DESC_Developer ${LANG_ENGLISH} "Installs programming headers for developers."


  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${BRL-CAD} $(DESC_BRL-CAD)
    !insertmacro MUI_DESCRIPTION_TEXT ${Documentation} $(DESC_Documentation)
    ;!insertmacro MUI_DESCRIPTION_TEXT ${Samples} $(DESC_Samples)
    ;!insertmacro MUI_DESCRIPTION_TEXT ${Developer} $(DESC_Developer)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END


;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
;  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD ${VERSION}"
;  DeleteRegKey HKLM "SOFTWARE\BRL-CAD ${VERSION}"

  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\$MUI_TEMP${INSTALLERSUFFIX}\Manuals\*"
  Delete "$SMPROGRAMS\$MUI_TEMP${INSTALLERSUFFIX}\*"
  Delete "$DESKTOP\Archer${INSTALLERSUFFIX}.lnk"
  Delete "$DESKTOP\MGED${INSTALLERSUFFIX}.lnk"
  Delete "$DESKTOP\RtWizard${INSTALLERSUFFIX}.lnk"


  ; Remove miscellaneous files
  Delete "$INSTDIR\archer.ico"
  Delete "$INSTDIR\brlcad.ico"
  Delete "$INSTDIR\uninstall.exe"


  ; Remove directories used
  RMDir /r "$INSTDIR\bin"
  RMDir /r "$INSTDIR\include"
  RMDir /r "$INSTDIR\lib"
  RMDir /r "$INSTDIR\share"
  RMDir "$INSTDIR"
  RMDir "$SMPROGRAMS\$MUI_TEMP${INSTALLERSUFFIX}\Manuals"
  RMDir "$SMPROGRAMS\$MUI_TEMP${INSTALLERSUFFIX}"

SectionEnd
