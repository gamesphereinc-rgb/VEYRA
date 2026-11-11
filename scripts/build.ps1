param([ValidateSet("Debug","Release")][string]$Configuration="Release")
$ErrorActionPreference="Stop";$Root=(Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if(!(Test-Path (Join-Path $Root "build\VEYRA.sln"))){& (Join-Path $PSScriptRoot "setup.ps1");if($LASTEXITCODE -ne 0){throw "VEYRA setup failed."}}
& cmake --build (Join-Path $Root "build") --config $Configuration --target VEYRA --parallel
if($LASTEXITCODE -ne 0){throw "VEYRA build failed. Review the compiler output."}
Write-Host "Build complete: $Root\build\out\VEYRA.exe" -ForegroundColor Green
