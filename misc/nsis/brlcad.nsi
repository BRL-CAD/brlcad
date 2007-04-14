; brlcad.nsi

;--------------------------------
;BRL-CAD Version Variables

  !ifndef VERSION
    !define VERSION '7.8.0'
  !endif

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"

;--------------------------------
;Configuration

  ; The name of the installer
  Name "BRL-CAD"

  ; The file to write
  OutFile "brlcad-${VERSION}.exe"

  ; The default installation directory
  InstallDir $PROGRAMFILES\BRL-CAD

  ; Registry key to check for directory (so if you install again, it will 
  ; overwrite the old one automatically)
  InstallDirRegKey HKLM "Software\BRL-CAD" "Install_Dir"

  ; Make it look pretty in XP
  XPStyle on

;--------------------------------
;Variables

  Var MUI_TEMP
  Var STARTMENU_FOLDER

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
  !define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of BRL-CAD ${VERSION}.\r\n\r\nBRL-CAD is a free open source multiplatform CAD software developed by the U.S. Army Research Laboratory (ARL).\r\n\r\nClick Next to continue."

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "copying.txt"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY

  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\BRL-CAD${VERSION}" 
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"

  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER


  !insertmacro MUI_PAGE_INSTFILES

  ;Finished page configuration
  !define MUI_FINISHPAGE_NOAUTOCLOSE
  !define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\doc\html\manuals\index.html"
  !define MUI_FINISHPAGE_SHOWREADME_TEXT "View Readme"
  !define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

  !define MUI_FINISHPAGE_LINK "BRL-CAD Home Page"
  !define MUI_FINISHPAGE_LINK_LOCATION "http://www.brlcad.org/"

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

  ; Put file there
  File "..\*"

  SetOutPath $INSTDIR\bin
  File ..\bin\*

  SetOutPath $INSTDIR\lib
  File /r ..\lib\*

  SetOutPath $INSTDIR\plugins
  File /r ..\plugins\*

  SetOutPath $INSTDIR\tclscripts
  File /r ..\tclscripts\*

  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\BRL-CAD "Install_Dir" "$INSTDIR"

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD" "DisplayName" "BRL-CAD ${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD" "UninstallString" '"$INSTDIR\uninstall.exe"'
  ;Create uninstaller
  WriteUninstaller "uninstall.exe"


  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    ;Main start menu shortcuts
    SetOutPath $INSTDIR
    CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Archer.lnk" "$INSTDIR\bin\archer.bat" "" "$INSTDIR\archer.ico" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\MGED.lnk" "$INSTDIR\bin\mged.bat" "" "$INSTDIR\brlcad.ico" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  
  !insertmacro MUI_STARTMENU_WRITE_END

SectionEnd

Section "Documentation (required)" Documentation
  ; SectionIn RO means temporarily required
  SectionIn RO
  SetOutPath $INSTDIR\doc
  File /r ..\doc\*
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    ;Main start menu shortcuts
    SetOutPath $INSTDIR
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Manual.lnk" "$INSTDIR\doc\html\manuals\index.html" "" "" 0
  !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

Section "Samples" Samples
  SetOutPath $INSTDIR\Samples
  File ..\Samples\*
SectionEnd

Section "Developement headers" Developer
  SetOutPath $INSTDIR\include
  File ..\include\*
SectionEnd

;--------------------------------
;Descriptions


  ;Language strings
  LangString DESC_BRL-CAD ${LANG_ENGLISH} "Installs the main application and the associated data files."
  LangString DESC_Documentation ${LANG_ENGLISH} "Installs documentation for BRL-CAD."
  LangString DESC_Samples ${LANG_ENGLISH} "Installs optional learning samples."
  LangString DESC_Developer ${LANG_ENGLISH} "Installs programming headers for developers."


  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${BRL-CAD} $(DESC_BRL-CAD)
    !insertmacro MUI_DESCRIPTION_TEXT ${Documentation} $(DESC_Documentation)
    !insertmacro MUI_DESCRIPTION_TEXT ${Samples} $(DESC_Samples)
    !insertmacro MUI_DESCRIPTION_TEXT ${Developer} $(DESC_Developer)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END


;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BRL-CAD"
  DeleteRegKey HKLM SOFTWARE\BRL-CAD

  ; Remove files and uninstaller
  Delete $INSTDIR\bin\*
  RMDir /r "$INSTDIR\doc"
  Delete $INSTDIR\include\*.h
  RMDir /r "$INSTDIR\lib"
  RMDir /r "$INSTDIR\plugins"
  Delete $INSTDIR\Samples\*.*
  RMDir /r "$INSTDIR\tclscripts"
  Delete $INSTDIR\*

  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\$MUI_TEMP\*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\$MUI_TEMP"
  RMDir "$INSTDIR\bin"
  RMDir "$INSTDIR"

  ;Delete empty start menu parent diretories
  StrCpy $MUI_TEMP "$SMPROGRAMS\$MUI_TEMP"
 
  startMenuDeleteLoop:
    RMDir $MUI_TEMP
    GetFullPathName $MUI_TEMP "$MUI_TEMP\.."
    
    IfErrors startMenuDeleteLoopDone
  
    StrCmp $MUI_TEMP $SMPROGRAMS startMenuDeleteLoopDone startMenuDeleteLoop
  startMenuDeleteLoopDone:

SectionEnd
