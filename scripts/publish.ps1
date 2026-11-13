param(
    [string]$RepositoryUrl = 'https://github.com/CloudW4lker250/DimLite.git',
    [string]$Tag = 'v1.0.0'
)

$ErrorActionPreference = 'Stop'

if ((git status --porcelain) -ne '') {
    throw 'Working tree is not clean. Commit or remove local changes before publishing.'
}

$branch = (git branch --show-current).Trim()
if ($branch -ne 'main') { throw "Expected branch main, found '$branch'." }

$origin = git remote get-url origin 2>$null
if ($LASTEXITCODE -ne 0) {
    git remote add origin $RepositoryUrl
} elseif ($origin.Trim() -ne $RepositoryUrl) {
    throw "Origin already points to '$($origin.Trim())'. Pass the intended repository URL manually after reviewing it."
}

$existingTag = git tag --list $Tag
if (!$existingTag) { git tag -a $Tag -m "DimLite $Tag" }

git push --set-upstream origin main
git push origin $Tag

Write-Host "Published branch main and tag $Tag to $RepositoryUrl"
Write-Host "Release ZIP: $((Resolve-Path '.\release\DimLite-1.0.0-win-x64.zip').Path)"
Write-Host "Checksum:    $((Resolve-Path '.\release\DimLite-1.0.0-win-x64.sha256').Path)"


