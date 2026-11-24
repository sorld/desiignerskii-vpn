# Run from a Visual Studio x64 developer shell with CMake, Ninja, 7-Zip and Inno Setup 6.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$version = '0.6.3'
$build = Join-Path $root 'build-native'
$cache = Join-Path $build 'downloads'
$stage = Join-Path $root 'deployment/windows-amd64'
New-Item -Type Directory -Force $cache, $stage | Out-Null
function Run-Checked([string]$exe, [string[]]$argList) {
    & $exe @argList
    if ($LASTEXITCODE -ne 0) { throw "$exe exited with $LASTEXITCODE" }
}
function Get-Verified([string]$url, [string]$name, [string]$sha) {
    $file = Join-Path $cache $name
    if (!(Test-Path $file) -or (Get-FileHash $file).Hash -ne $sha) {
        Invoke-WebRequest $url -OutFile $file
    }
    if ((Get-FileHash $file).Hash -ne $sha) { throw "Checksum mismatch for $name" }
    return $file
}
$qt = Get-Verified 'https://github.com/throneproj/buildqt/releases/download/Qt_6.12.0/Qt_6.12.0_x64.7z' 'qt.7z' '2233c6f40adf952bd0d1dbd36ce5c9a0ea674a0689eebdcc8d11847e0cf4f387'
$ssl = Get-Verified 'https://github.com/throneproj/env_windows_legacy/releases/download/latest/openssl_x64.7z' 'openssl.7z' '0cc10ca8bd88a08f1f4275edf8c3f285c5988803e693d4dc8a12a85a9e544412'
$core = Get-Verified 'https://github.com/throneproj/Throne/releases/download/1.4.2/Throne-1.4.2-windows64.zip' 'upstream.zip' '9bd8d0f660c55b2c195baa69ed7d67c6857b6a929ea576d64d1cf12ef4b55459'
$ctl = Get-Verified 'https://github.com/throneproj/Throne-api/releases/download/v0.4.0/thronectl-v0.4.0-windows-amd64.zip' 'ctl.zip' '7c81e0ca3824117244d78b5703a0b8995d417140ae669d35af0ad819c01f55d9'
Run-Checked '7z' @('x','-y',"-o$cache/ctl",$ctl)
Run-Checked '7z' @('x','-y',"-o$cache",$qt)
Run-Checked '7z' @('x','-y',"-o$cache",$ssl)
Run-Checked '7z' @('x','-y',"-o$cache/upstream",$core)
Invoke-WebRequest 'https://raw.githubusercontent.com/throneproj/routeprofiles/2faa1c8cd18fa1e3d4b931ab9864c08fe629db25/srslist.h' -OutFile "$build/srslist.h"
$dashboard = Join-Path $cache 'dashboard'
if (!(Test-Path "$dashboard/.git")) { Run-Checked 'git' @('clone','--no-checkout','https://github.com/SagerNet/sing-box-dashboard.git',$dashboard) }
Run-Checked 'git' @('-C',$dashboard,'checkout','--detach','d77c2f016a7681aa1704e9f83c69139edb230c97')
Get-ChildItem $dashboard | Where-Object Name -ne '.git' | Copy-Item -Destination "$root/res/dashboard" -Recurse -Force
$env:OPENSSL_ROOT_DIR = "$cache/openssl"
Run-Checked 'cmake' @('-S',$root,'-B',$build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',"-DINPUT_VERSION=$version","-DCMAKE_PREFIX_PATH=$cache/Qt/lib/cmake")
Run-Checked 'cmake' @('--build',$build,'--target','Throne','--parallel','3')
foreach ($name in 'ThroneCore.exe','libcronet.dll','updater.exe') {
    Copy-Item "$cache/upstream/Throne/$name" $stage -Force
}
Get-ChildItem "$cache/ctl" -Recurse -Filter thronectl.exe | Select-Object -First 1 | Copy-Item -Destination $stage -Force
Copy-Item "$build/Throne.exe","$root/LICENSE" $stage -Force
$zip = Join-Path $root "deployment/Desiignerskii-VPN-$version-windows64.zip"
# The native updater expects the upstream archive layout with a Throne directory at the top.
$portable = Join-Path $build 'portable/Throne'
New-Item -Type Directory -Force $portable | Out-Null
Copy-Item "$stage/*" $portable -Force
Compress-Archive -Path $portable -DestinationPath $zip -Force
$iscc = Get-Command 'iscc.exe' -ErrorAction SilentlyContinue
if ($iscc) { $compiler = $iscc.Source } else { $compiler = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
Run-Checked $compiler @('/Qp','/DDesiignerskiiX64=1',"/DAppVersion=$version",'/DAppVersionMajor=0','/DAppVersionMinor=6','/DAppVersionPatch=3',"$root/script/windows_installer.iss")
