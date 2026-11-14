param(
    [string]$Rom = (Join-Path $PSScriptRoot '..\build\3ds\projects\demo_project\tetris3d.3dsx')
)

$ErrorActionPreference = 'Stop'

$azaharCandidates = @(
    'C:\Users\leo\dev\azahar\azahar-windows-msvc-2126.2\azahar.exe',
    'C:\Users\leo\dev\azahar\azahar.exe',
    "$env:LOCALAPPDATA\Programs\Azahar\azahar.exe"
)

$azahar = $azaharCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $azahar) {
    throw "Azahar not found. Download the portable Windows build into C:\Users\leo\dev\azahar or edit this script."
}

if (-not (Test-Path $Rom)) {
    throw "ROM not found: $Rom (build the 3ds preset first: .\tools\build.ps1 -Target 3ds)"
}

$romPath = (Resolve-Path $Rom).Path
Write-Host "Launching $romPath in Azahar..."
Start-Process -FilePath $azahar -ArgumentList @($romPath)
