$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$candidates = @(
    (Get-Command ISCC.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue),
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 7\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 7\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe')
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }
$iscc = $candidates | Select-Object -First 1
if (!$iscc) { throw 'Inno Setup 7 was not found. Install it from https://jrsoftware.org/isdl.php.' }
$exe = Join-Path $repo 'DimLite.exe'
if (!(Test-Path -LiteralPath $exe)) { & (Join-Path $repo 'source\build.ps1') }
& $iscc (Join-Path $PSScriptRoot 'DimLite.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE." }
$setup = Join-Path $repo 'release\DimLite-1.0.0-Setup.exe'
if (!(Test-Path -LiteralPath $setup)) { throw 'Setup executable was not created.' }
$hash = (Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash
Set-Content -LiteralPath (Join-Path $repo 'release\DimLite-1.0.0-Setup.sha256') -Value ($hash + '  DimLite-1.0.0-Setup.exe') -Encoding ascii
Write-Output "Created: $setup"
Write-Output "SHA-256: $hash"

