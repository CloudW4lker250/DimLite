# Requires the Visual Studio C++ desktop tools and a Windows 10/11 SDK.
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio C++ Build Tools are required.' }
$installation = & $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'No installed MSVC x64 compiler was found.' }
$toolVersion = (Get-Content -LiteralPath (Join-Path $installation 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
$msvcDir = Join-Path $installation "VC\Tools\MSVC\$toolVersion"
$sdkDir = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdkVersion = Get-ChildItem -LiteralPath (Join-Path $sdkDir 'Lib') -Directory |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'um\x64\user32.lib') } |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1 -ExpandProperty Name
if (!$sdkVersion) { throw 'No Windows SDK was found.' }
$savedPath = $env:PATH
$savedInclude = $env:INCLUDE
$savedLib = $env:LIB
$buildDir = Join-Path $env:TEMP ('DimLite-build-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $buildDir | Out-Null
try {
    $env:PATH = "$msvcDir\bin\Hostx64\x64;$sdkDir\bin\$sdkVersion\x64;" + $env:PATH
    $env:INCLUDE = "$msvcDir\include;$sdkDir\Include\$sdkVersion\ucrt;$sdkDir\Include\$sdkVersion\shared;$sdkDir\Include\$sdkVersion\um"
    $env:LIB = "$msvcDir\lib\x64;$sdkDir\Lib\$sdkVersion\ucrt\x64;$sdkDir\Lib\$sdkVersion\um\x64"
    Push-Location -LiteralPath $PSScriptRoot
    try {
        & rc.exe /nologo /c65001 "/fo$buildDir\DimLite.res" DimLite.rc
        if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
        $destination = Join-Path (Split-Path -Parent $PSScriptRoot) 'DimLite.exe'
        & cl.exe /nologo /O1 /MT /GL /Gy /Gw /W4 /WX /EHsc /std:c++17 /utf-8 /guard:cf /D_WIN32_WINNT=0x0A00 /DUNICODE /D_UNICODE "/Fo$buildDir\DimLite.obj" "/Fe$destination" DimLite.cpp "$buildDir\DimLite.res" user32.lib gdi32.lib shell32.lib comctl32.lib advapi32.lib ole32.lib dwmapi.lib gdiplus.lib Magnification.lib winhttp.lib bcrypt.lib delayimp.lib /link /LTCG /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /DELAYLOAD:Magnification.dll /DELAYLOAD:gdiplus.dll /SUBSYSTEM:WINDOWS,10.00 /MANIFEST:EMBED /MANIFESTINPUT:DimLite.manifest
        if ($LASTEXITCODE -ne 0) { throw 'C++ compilation failed.' }
        Write-Output "Built: $destination"
    } finally { Pop-Location }
} finally {
    $env:PATH = $savedPath
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
    # Retain compiler intermediates in the unique temporary build folder.
}
