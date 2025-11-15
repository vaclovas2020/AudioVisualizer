;--------------------------------
; General Installer Settings
;--------------------------------
Name "AudioVisualizer"
OutFile "AudioVisualizerSetup.exe"
InstallDir "$PROGRAMFILES64\AudioVisualizer"
RequestExecutionLevel admin

;--------------------------------
; Pages
;--------------------------------
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------
; Init Function
;--------------------------------
Function .onInit
    SetRegView 64
    ReadRegStr $INSTDIR HKLM "Software\AudioVisualizer" "Install_Dir"
FunctionEnd

;--------------------------------
; Installer Section
;--------------------------------
Section "Install"

    SetRegView 64
    SetOutPath "$INSTDIR"

    ; Copy application files
    File "x64\Release\AudioVisualizer.exe"

    ; Create Desktop shortcut
    CreateShortcut "$DESKTOP\AudioVisualizer.lnk" "$INSTDIR\AudioVisualizer.exe"

    ; Create Start Menu folder and shortcut
    CreateDirectory "$SMPROGRAMS\AudioVisualizer"
    CreateShortcut "$SMPROGRAMS\AudioVisualizer\AudioVisualizer.lnk" "$INSTDIR\AudioVisualizer.exe"

    ; Write uninstall info
    WriteRegStr HKLM "Software\AudioVisualizer" "Install_Dir" "$INSTDIR"
    WriteUninstaller "$INSTDIR\uninstall.exe"

SectionEnd

;--------------------------------
; Uninstaller Section
;--------------------------------
Section "Uninstall"

    SetRegView 64

    ; Remove installed files
    Delete "$INSTDIR\AudioVisualizer.exe"
    Delete "$DESKTOP\AudioVisualizer.lnk"

    ; Remove Start Menu shortcut and folder
    Delete "$SMPROGRAMS\AudioVisualizer\AudioVisualizer.lnk"
    RMDir "$SMPROGRAMS\AudioVisualizer"

    ; Remove uninstaller
    Delete "$INSTDIR\uninstall.exe"

    ; Remove installation directory
    RMDir /r "$INSTDIR"

    ; Remove registry entries
    DeleteRegKey HKLM "Software\AudioVisualizer"

SectionEnd
