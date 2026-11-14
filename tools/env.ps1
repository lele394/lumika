# Dot-source from the repository root:  . .\tools\env.ps1

$msysRoot = 'C:\Users\leo\dev\msys64'
if (-not (Test-Path $msysRoot)) {
    throw "MSYS2 not found at $msysRoot. Set it up first (see README)."
}

$env:MSYS2_ROOT  = $msysRoot
$env:DEVKITPRO   = Join-Path $msysRoot 'opt\devkitpro'
$env:DEVKITARM   = Join-Path $env:DEVKITPRO 'devkitARM'
$env:MINGW_PREFIX = Join-Path $msysRoot 'mingw64'

# CMake reads CMAKE_PREFIX_PATH from the environment when locating glfw3 etc.
$env:CMAKE_PREFIX_PATH = $env:MINGW_PREFIX

$env:PATH = (@(
    (Join-Path $env:DEVKITPRO 'tools\bin'),
    (Join-Path $env:DEVKITARM 'bin'),
    (Join-Path $env:MINGW_PREFIX 'bin')
) -join ';') + ';' + $env:PATH

Write-Host "DEVKITPRO   = $env:DEVKITPRO"
Write-Host "DEVKITARM   = $env:DEVKITARM"
Write-Host "MINGW_PREFIX= $env:MINGW_PREFIX"
