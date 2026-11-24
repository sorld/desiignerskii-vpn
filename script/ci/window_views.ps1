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
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class ConnectionPrefsWindows {
    private delegate bool WindowCallback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] private static extern bool EnumWindows(WindowCallback callback, IntPtr data);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr window);
    public static IntPtr[] VisibleFor(uint processId) {
        var result = new List<IntPtr>();
        EnumWindows((window, data) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner == processId && IsWindowVisible(window)) result.Add(window);
            return true;
        }, IntPtr.Zero);
        return result.ToArray();
    }
}
'@
$processId = $windowElement.Current.ProcessId
$sectionName = [System.Windows.Automation.PropertyCondition]::new(
    [System.Windows.Automation.AutomationElement]::NameProperty, 'Разделы настроек')
Invoke-ViewButton 'Настройки клиента'
$dialog = $null
for ($i=0; $i -lt 40 -and !$dialog; $i++) {
    $titles = @()
    foreach ($handle in [ConnectionPrefsWindows]::VisibleFor($processId)) {
        $candidate = [System.Windows.Automation.AutomationElement]::FromHandle($handle)
        $titles += $candidate.Current.Name
        $sections = $candidate.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$sectionName)
        if ($sections -and !$sections.Current.IsOffscreen) {
            $dialog = $candidate
            break
        }
    }
    if (!$dialog) { Start-Sleep -Milliseconds 250 }
}
if (!$dialog) { throw "Settings navigation missing. Visible client windows: $($titles -join ' | ')" }
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
