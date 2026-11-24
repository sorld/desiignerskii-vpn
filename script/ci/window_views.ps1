param([IntPtr]$Window,[string]$DataDir)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
function Get-ViewButton([string]$Name) {
    $root = [System.Windows.Automation.AutomationElement]::FromHandle($Window)
    $condition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::NameProperty, $Name)
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        foreach ($node in $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition)) {
            if (!$node.Current.IsOffscreen -and $node.Current.IsEnabled -and $node.Current.ControlType -eq [System.Windows.Automation.ControlType]::Button) {
                return $node
            }
        }
        Start-Sleep -Milliseconds 300
    }
    throw "Visible button not found: $Name"
}
function Invoke-ViewButton([string]$Name) {
    $button = Get-ViewButton $Name
    $button.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
}
function Assert-ViewSaved([string]$Mode) {
    $prefs = Join-Path $DataDir 'config/desiignerskii-ui.ini'
    if (!(Test-Path $prefs) -or !(Select-String -LiteralPath $prefs -Pattern "^view=$Mode$" -Quiet)) {
        throw "View preference was not saved: $Mode"
    }
}
Assert-ViewSaved 'home'
$windowElement = [System.Windows.Automation.AutomationElement]::FromHandle($Window)
$homeWidth = $windowElement.Current.BoundingRectangle.Width
Invoke-ViewButton 'Настройки'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ConnectionPrefsWindows {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);
}
'@
$dialogHandle = [IntPtr]::Zero
for ($i=0; $i -lt 50 -and $dialogHandle -eq [IntPtr]::Zero; $i++) {
    $dialogHandle = [ConnectionPrefsWindows]::FindWindow($null, 'Настройки · Desiignerskii VPN')
    if ($dialogHandle -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 100 }
}
if ($dialogHandle -eq [IntPtr]::Zero) { throw 'Settings window did not open' }
$dialog = [System.Windows.Automation.AutomationElement]::FromHandle($dialogHandle)
$sectionList = $dialog.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
    [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty, 'Разделы настроек'))
if (!$sectionList) { throw 'Settings navigation missing' }
$cancel = $dialog.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
    [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty, 'Отмена'))
$cancel.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Invoke-ViewButton 'Расширенный вид'
$null = Get-ViewButton 'Основной вид'
Assert-ViewSaved 'advanced'
if ($windowElement.Current.BoundingRectangle.Width -lt $homeWidth * 1.25) { throw 'Primary window inherits advanced width' }
Invoke-ViewButton 'Основной вид'
$null = Get-ViewButton 'Расширенный вид'
Assert-ViewSaved 'home'
if ([Math]::Abs($windowElement.Current.BoundingRectangle.Width - $homeWidth) -gt 4) { throw 'Home size not restored' }
Write-Host 'PASS: default home, visible buttons in both directions, saved view, same native window.'
