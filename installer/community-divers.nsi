Unicode true
RequestExecutionLevel user
!include "MUI2.nsh"
!include "x64.nsh"
Name "${CD_NAME}"
OutFile "${CD_OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\CommunityDivers"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "InstallLocation"
SetCompressor /SOLID zlib
BrandingText "Community Divers"
!define MUI_ICON "${CD_PAYLOAD}\assets\brand\launcher.ico"
!define MUI_UNICON "${CD_PAYLOAD}\assets\brand\launcher.ico"
!define MUI_WELCOMEPAGE_TEXT "Install the Community Divers launcher for your Windows account.$\r$\n$\r$\nNo game files are included. Your profiles, mods and campaign data stay separate from the launcher."
!define MUI_FINISHPAGE_TEXT "The launcher is installed. You can remove it from Windows' installed-apps list. Uninstalling keeps profiles, mods, campaign data and game copies."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!define MUI_UNCONFIRMPAGE_TEXT_TOP "Remove the Community Divers launcher? Profiles, mods, campaign data, game copies and custom files will be kept."
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Var LockHandle
Function .onInit
  SetShellVarContext current
  SetRegView 64
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "Community Divers requires 64-bit Windows." /SD IDOK
    Abort
  ${EndIf}
FunctionEnd
Function un.onInit
  SetShellVarContext current
  SetRegView 64
FunctionEnd
Section "Community Divers"
  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  File /oname=SetupHelper.exe "${CD_PAYLOAD}\CommunityDiversUpdate.exe"
  nsExec::ExecToStack '"$PLUGINSDIR\SetupHelper.exe" --setup-check "$INSTDIR"'
  Pop $0
  Pop $1
  StrCmp $0 0 +4
    MessageBox MB_OK|MB_ICONSTOP "Cannot install here. Choose an empty folder or an existing Community Divers installation, and close the launcher.$\r$\n$1" /SD IDOK
    SetErrorLevel 1
    Abort
  CreateDirectory "$INSTDIR"
  ClearErrors
  FileOpen $LockHandle "$INSTDIR\.community-divers-launcher.lock" w
  IfErrors install_locked
  nsExec::ExecToStack '"$PLUGINSDIR\SetupHelper.exe" --setup-begin "$INSTDIR" "${CD_REGKEY}"'
  Pop $0
  Pop $1
  StrCmp $0 0 +4
    MessageBox MB_OK|MB_ICONSTOP "Setup could not record this installation.$\r$\n$1" /SD IDOK
    SetErrorLevel 1
    Goto install_end
  !include "${CD_FILELIST}"
  IfErrors install_failed
  nsExec::ExecToStack '"$PLUGINSDIR\SetupHelper.exe" --setup-register "$INSTDIR" "${CD_REGKEY}"'
  Pop $0
  Pop $1
  StrCmp $0 0 +4
    MessageBox MB_OK|MB_ICONSTOP "Setup could not verify the installed files. Run this installer again.$\r$\n$1" /SD IDOK
    SetErrorLevel 1
    Goto install_end
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortcut "$SMPROGRAMS\${CD_NAME}.lnk" "$INSTDIR\CommunityDivers.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "DisplayName" "${CD_NAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "DisplayVersion" "${CD_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "Publisher" "Community Divers contributors"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "DisplayIcon" "$INSTDIR\CommunityDivers.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "URLInfoAbout" "https://github.com/Nnamllit1/community-divers"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "NoRepair" 1
  Goto install_end
install_locked:
  MessageBox MB_OK|MB_ICONSTOP "Close Community Divers before installing." /SD IDOK
  SetErrorLevel 1
  Abort
install_failed:
  MessageBox MB_OK|MB_ICONSTOP "Some launcher files could not be installed. Close the launcher and run setup again." /SD IDOK
  SetErrorLevel 1
install_end:
  FileClose $LockHandle
SectionEnd
Section "Uninstall"
  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  File /oname=UninstallHelper.exe "${CD_PAYLOAD}\CommunityDiversUpdate.exe"
  nsExec::ExecToStack '"$PLUGINSDIR\UninstallHelper.exe" --uninstall-managed "$INSTDIR"'
  Pop $0
  Pop $1
  StrCmp $0 0 +4
    MessageBox MB_OK|MB_ICONSTOP "Uninstall could not finish. Close the launcher, or recover a pending update, then retry.$\r$\n$1" /SD IDOK
    SetErrorLevel 1
    Abort
  ReadRegStr $2 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}" "InstallLocation"
  StrCmp $2 $INSTDIR 0 +3
    Delete "$SMPROGRAMS\${CD_NAME}.lnk"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${CD_REGKEY}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
