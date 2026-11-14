param(
    [string]$Project = 'projects/demo_project'
)

# Regenerates cooked assets from the source OBJ/PNG files.
# Cooked files are committed so the 3DS build does not depend on host tools.

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. "$PSScriptRoot\env.ps1"

cmake --build --preset desktop --target meshc
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$projectDir = Join-Path $root $Project
$cookedModels = Join-Path $projectDir 'assets\cooked\models'
$cookedTextures = Join-Path $projectDir 'assets\cooked\textures'
$cookedMaterials = Join-Path $projectDir 'assets\cooked\materials'
$cookedAudio = Join-Path $projectDir 'assets\cooked\audio'
New-Item -ItemType Directory -Force $cookedModels, $cookedTextures, $cookedMaterials, $cookedAudio | Out-Null

$meshc = Join-Path $root 'build\desktop\tools\meshc.exe'
$tex3ds = Join-Path $env:DEVKITPRO 'tools\bin\tex3ds.exe'

Get-ChildItem (Join-Path $projectDir 'assets\models') -Filter *.obj | ForEach-Object {
    $out = Join-Path $cookedModels ($_.BaseName + '.cmesh')
    & $meshc $_.FullName $out
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Get-ChildItem (Join-Path $projectDir 'assets\textures') -Filter *.png | ForEach-Object {
    $out = Join-Path $cookedTextures ($_.BaseName + '.t3x')
    & $tex3ds -o $out $_.FullName
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Copy-Item (Join-Path $projectDir 'assets\materials\*.mat.json') $cookedMaterials -Force -ErrorAction SilentlyContinue
Copy-Item (Join-Path $projectDir 'assets\manifest.json') (Join-Path $projectDir 'assets\cooked\manifest.json') -Force
Copy-Item (Join-Path $projectDir 'assets\audio\*.wav') $cookedAudio -Force -ErrorAction SilentlyContinue

Write-Host "Cooked assets written under $projectDir\assets\cooked"
