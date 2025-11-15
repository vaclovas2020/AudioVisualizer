;--------------------------------
; General Installer Settings
;--------------------------------

Name "AudioVisualizer"
OutFile "AudioVisualizerSetup.exe"
InstallDir "$PROGRAMFILES64\AudioVisualizer"   ; <- 64-bit Program Files
RequestExecutionLevel admin

; Store install dir in registry so the uninstaller knows
SetRegView 64  ; Use 64-bit registry view
InstallDirRegKey HKLM "Software\AudioVisualizer" "Install_Dir"

;--------------------------------
; Pages
;--------------------------------

Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------
; Installer Section
;--------------------------------

Section "Install"

    ; Set output path
    SetOutPath "$INSTDIR"

    ; Copy application files
    File "x64\Release\AudioVisualizer.exe"

    ; Create Shortcut on Desktop
    CreateShortcut "$DESKTOP\AudioVisualizer.lnk" "$INSTDIR\AudioVisualizer.exe"

    ; Write uninstall info
    WriteRegStr HKLM "Software\AudioVisualizer" "Install_Dir" "$INSTDIR"
    WriteUninstaller "$INSTDIR\uninstall.exe"

SectionEnd

;--------------------------------
; Uninstaller Section
;--------------------------------

Section "Uninstall"

    ; Remove installed files
    Delete "$INSTDIR\AudioVisualizer.exe"
    Delete "$DESKTOP\AudioVisualizer.lnk"
    Delete "$INSTDIR\uninstall.exe"

    ; Remove installation directory
    RMDir "$INSTDIR"

    ; Remove registry entries
    SetRegView 64
    DeleteRegKey HKLM "Software\AudioVisualizer"

SectionEnd
