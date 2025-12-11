;--------------------------------
; Modern UI 2
;--------------------------------
!include "MUI2.nsh"
!include "FileFunc.nsh"

Unicode true
RequestExecutionLevel admin

;--------------------------------
; App Information
;--------------------------------
!define APP_ID          "AudioVisualizer"     ; INTERNAL, no spaces
!define APP_NAME        "Audio Visualizer"    ; DISPLAY name
!define APP_VERSION     "1.0.1"
!define COMPANY_NAME    "Vaclovas Lapinskis"
!define PUBLISHER       "Vaclovas Lapinskis"
!define APP_EXE         "AudioVisualizer.exe"

!define INSTALL_DIR     "$PROGRAMFILES64\${APP_ID}"
!define UNINSTALL_KEY   "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_ID}"

Name "${APP_NAME}"
OutFile "AudioVisualizerSetup_x64.exe"
InstallDir "${INSTALL_DIR}"
BrandingText "Copyright (c) Vaclovas Lapinskis 2025"

; Setup icons
Icon "small.ico"
UninstallIcon "small.ico"


;--------------------------------
; Modern UI Pages
;--------------------------------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"


;--------------------------------
; Init
;--------------------------------
Function .onInit
    SetRegView 64
    ReadRegStr $INSTDIR HKLM "Software\${APP_ID}" "Install_Dir"
    StrCmp $INSTDIR "" 0 +2
        StrCpy $INSTDIR "${INSTALL_DIR}"
FunctionEnd


;--------------------------------
; Install
;--------------------------------
Section "Install"

    SetRegView 64
    SetOutPath "$INSTDIR"

    ; Files
    File "x64\Release\${APP_EXE}"

    ;--------------------------------
    ; Desktop shortcut
    ;--------------------------------
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" \
        "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0

    ;--------------------------------
    ; Start Menu shortcut
    ;--------------------------------
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"

    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" \
        "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0


    ;--------------------------------
    ; Internal registry
    ;--------------------------------
    WriteRegStr HKLM "Software\${APP_ID}" "Install_Dir" "$INSTDIR"
    WriteRegStr HKLM "Software\${APP_ID}" "Version" "${APP_VERSION}"

    ; Uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"


    ;--------------------------------
    ; Windows Apps & Features (Add/Remove Programs)
    ;--------------------------------
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName"     "${APP_NAME}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion"  "${APP_VERSION}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher"       "${PUBLISHER}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon"     "$INSTDIR\${APP_EXE}"
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1

    ; Estimated Size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "EstimatedSize" $0

SectionEnd



;--------------------------------
; Uninstall
;--------------------------------
Section "Uninstall"

    SetRegView 64

    ; Shortcuts
    Delete "$DESKTOP\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    RMDir "$SMPROGRAMS\${APP_NAME}"

    ; Files
    Delete "$INSTDIR\${APP_EXE}"
    Delete "$INSTDIR\uninstall.exe"

    ; Directory
    RMDir /r "$INSTDIR"

    ; Registry
    DeleteRegKey HKLM "Software\${APP_ID}"
    DeleteRegKey HKLM "${UNINSTALL_KEY}"

SectionEnd
