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
Invoke-ViewButton 'Расширенный вид'
$null = Get-ViewButton 'Основной вид'
Assert-ViewSaved 'advanced'
Invoke-ViewButton 'Основной вид'
$null = Get-ViewButton 'Расширенный вид'
Assert-ViewSaved 'home'
Write-Host 'PASS: default home, visible buttons in both directions, saved view, same native window.'
