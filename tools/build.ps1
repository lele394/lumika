param(
    [ValidateSet('3ds', 'desktop')]
    [string]$Target = '3ds',
    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\env.ps1"

if ($Clean -and (Test-Path "$PSScriptRoot\..\build\$Target")) {
    Remove-Item -Recurse -Force "$PSScriptRoot\..\build\$Target"
}

cmake --preset $Target
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build --preset $Target
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Target -eq 'desktop') {
    ctest --preset desktop
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($Run) {
        & "$PSScriptRoot\..\build\desktop\projects\demo_project\tetris3d.exe"
    }
} elseif ($Run) {
    & "$PSScriptRoot\run_azahar.ps1"
}
