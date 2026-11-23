$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $version = '1.4.2'
    $archiveName = "Throne-$version-windows64.zip"
    $expected = '9bd8d0f660c55b2c195baa69ed7d67c6857b6a929ea576d64d1cf12ef4b55459'
    $build = Join-Path $PSScriptRoot 'build'
    $out = Join-Path $build 'Desiignerskii-VPN-0.5.1-windows-x64'
    $engine = Join-Path $out 'engine'
    $cache = Join-Path $build $archiveName
    New-Item -ItemType Directory -Force $engine | Out-Null
    if (!(Test-Path $cache)) {
        Invoke-WebRequest -Uri "https://github.com/throneproj/Throne/releases/download/$version/$archiveName" -OutFile $cache
    }
    if ((Get-FileHash $cache -Algorithm SHA256).Hash -ne $expected) {
        throw "Unexpected SHA-256 for $cache"
    }
    Expand-Archive $cache -DestinationPath (Join-Path $build 'upstream') -Force
    foreach ($name in 'Throne.exe', 'ThroneCore.exe', 'libcronet.dll') {
        Copy-Item (Join-Path $build "upstream/Throne/$name") $engine -Force
    }
    $env:GOOS = 'windows'
    $env:GOARCH = 'amd64'
    $env:CGO_ENABLED = '0'
    go build -trimpath -ldflags '-s -w -H=windowsgui' -o (Join-Path $out 'Desiignerskii VPN.exe') .
    if ($LASTEXITCODE -ne 0) { throw 'Go build failed' }
    Copy-Item ../../LICENSE $out -Force
    Copy-Item README.ru.md $out -Force
    Copy-Item ui/Inter-LICENSE.txt $out -Force
    Write-Output $out
} finally {
    Pop-Location
}
