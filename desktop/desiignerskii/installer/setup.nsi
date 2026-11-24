Target amd64-unicode
Unicode true
ManifestDPIAware true
ManifestSupportedOS all
RequestExecutionLevel admin
Name "Desiignerskii VPN"
OutFile "${OUTPUT}"
SetCompressor zlib
BrandingText "Desiignerskii VPN"
VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1049 "ProductName" "Desiignerskii VPN"
VIAddVersionKey /LANG=1049 "CompanyName" "Desiignerskii"
VIAddVersionKey /LANG=1049 "FileDescription" "Установка Desiignerskii VPN"
VIAddVersionKey /LANG=1049 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1049 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1049 "LegalCopyright" "Desiignerskii и авторы компонентов"
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!define CLIENT "Desiignerskii VPN"
!define WVKEY "SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
!define MUI_ICON "icon.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "wizard.bmp"
!define MUI_WELCOMEPAGE_TITLE "Установка Desiignerskii VPN"
!define MUI_WELCOMEPAGE_TEXT "Версия ${VERSION} для Windows x64.$\r$\n$\r$\nМастер установит клиент и добавит ярлыки в меню «Пуск» и на рабочий стол. Если потребуется Microsoft Edge WebView2, мастер загрузит его через интернет."
!define MUI_FINISHPAGE_TITLE "Desiignerskii VPN установлен"
!define MUI_FINISHPAGE_TEXT "Открой Desiignerskii VPN через меню «Пуск» или ярлык на рабочем столе."
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "Russian"

Function .onInit
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_ICONSTOP|MB_OK "Для установки требуется Windows 10 или 11 x64." /SD IDOK
    SetErrorLevel 1633
    Abort
  ${EndIf}
FunctionEnd

Section
  check_client:
  System::Call 'kernel32::OpenMutexW(i 0x100000, i 0, w "Local\DesiignerskiiVPN.Desktop.01") p.r0'
  ${If} $0 P<> 0
    System::Call 'kernel32::CloseHandle(p r0)'
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Выйди из Desiignerskii VPN через значок в трее и нажми «Повторить»." /SD IDCANCEL IDRETRY check_client
    SetErrorLevel 1618
    Abort
  ${EndIf}

  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  SetRegView 32
  ReadRegStr $0 HKLM "${WVKEY}" "pv"
  ${If} $0 == ""
  ${OrIf} $0 == "0.0.0.0"
    ReadRegStr $0 HKCU "${WVKEY}" "pv"
  ${EndIf}
  ${If} $0 == ""
  ${OrIf} $0 == "0.0.0.0"
    DetailPrint "Установка Microsoft Edge WebView2…"
    File "/oname=webview.exe" "webview.exe"
    ClearErrors
    ExecWait '"$PLUGINSDIR\webview.exe" /silent /install' $0
    ${If} ${Errors}
    ${OrIf} $0 != 0
      MessageBox MB_OK|MB_ICONSTOP "Не удалось установить Microsoft Edge WebView2. Проверь подключение к интернету и запусти установщик снова." /SD IDOK
      SetErrorLevel 1603
      Abort
    ${EndIf}
  ${EndIf}
  SetRegView 64

  DetailPrint "Установка Desiignerskii VPN…"
  File "client.msi"
  System::Call 'msi::MsiQueryProductStateW(w "${PRODUCT_CODE}") i.r0'
  StrCpy $1 ""
  ${If} $0 == 5
    StrCpy $1 "REINSTALL=ALL REINSTALLMODE=vomus"
  ${EndIf}
  ClearErrors
  ExecWait '"$SYSDIR\msiexec.exe" /i "$PLUGINSDIR\client.msi" $1 /qn /norestart' $0
  ${If} ${Errors}
    StrCpy $0 1603
  ${EndIf}
  ${If} $0 == 3010
    SetRebootFlag true
    SetErrorLevel 3010
  ${ElseIf} $0 != 0
    MessageBox MB_OK|MB_ICONSTOP "Windows Installer не завершил установку. Код $0." /SD IDOK
    SetErrorLevel $0
    Abort
  ${EndIf}
SectionEnd
