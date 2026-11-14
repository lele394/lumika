param(
    [ValidateSet('3ds', 'desktop')]
    [string]$Target = '3ds',
    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$msys = 'C:\Users\leo\dev\msys64'
$bash = Join-Path $msys 'usr\bin\bash.exe'

if ($Target -eq 'desktop') {
    . "$PSScriptRoot\env.ps1"

    if ($Clean) { Remove-Item -Recurse -Force "$root\build\desktop" -ErrorAction SilentlyContinue }

    cmake --preset desktop
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    cmake --build --preset desktop
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    ctest --preset desktop
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if ($Run) { & "$root\build\desktop\projects\demo_project\tetris3d.exe" }
    exit 0
}

# The devkitPro CMake toolchain requires the MSYS2-native cmake, so 3DS builds
# are driven from a login bash shell (which also sets DEVKITPRO/DEVKITARM).
$env:MSYSTEM = 'MSYS'
Remove-Item Env:CMAKE_PREFIX_PATH -ErrorAction SilentlyContinue

$posixRoot = (& $bash -lc "cygpath -u '$root'").Trim()

if ($Clean) { & $bash -lc "rm -rf '$posixRoot/build/3ds'" }

& $bash -lc "cd '$posixRoot' && cmake --preset 3ds && cmake --build --preset 3ds"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Run) { & "$PSScriptRoot\run_azahar.ps1" }
